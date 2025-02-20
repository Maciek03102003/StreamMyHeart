#include <obs-module.h>
#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <obs-data.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <vector>
#include <sstream>
#include "plugin-support.h"
#include "obs_utils.h"
#include "graph_source.h"
#include "graph_source_info.h"
#include "heart_rate_source.h"

#define LINE_THICKNESS 3.0f
#define BUFFER_SIZE 100    // Store more data for stable min/max
#define PLOT_SIZE 50       // Only plot the last 50 points
#define SMOOTHING_WINDOW 5 // Moving average window

// Destroy function for graph source
void destroy_graph_source(void *data)
{
	// obs_log(LOG_INFO, "Graph source destroyed");
	struct graph_source *graph = reinterpret_cast<struct graph_source *>(data);

	if (graph) {
		// Release the OBS source
		if (graph->source) {
			obs_source_release(graph->source);
			graph->source = nullptr;
		};

// Clear buffer (only applicable in C++)
#ifdef __cplusplus
		graph->buffer.clear();
#endif

		// Free memory
		bfree(graph);
	}
}

static bool find_heart_rate_monitor_filter(void *param, obs_source_t *source)
{
	const char *filter_name = MONITOR_SOURCE_NAME;

	// Try to get the filter from the current source
	obs_source_t *filter = obs_source_get_filter_by_name(source, filter_name);
	// obs_log(LOG_INFO, "Source: %s", obs_source_get_name(source));

	if (filter) {
		// Store the filter reference in param
		*(obs_source_t **)param = filter;
		return false; // Stop further enumeration
	}

	return true; // Continue searching
}

obs_source_t *get_heart_rate_monitor_filter()
{
	obs_source_t *found_filter = nullptr;

	// Enumerate all sources to find the desired filter
	obs_enum_sources(find_heart_rate_monitor_filter, &found_filter);

	return found_filter; // Return the filter reference (must be released later)
}

static std::vector<double> getSmoothedY(std::vector<double> &graph_buffer, uint32_t height)
{
	// Extract the most recent PLOT_SIZE values for rendering
	std::vector<double> plotBuffer;
	size_t startIdx = (graph_buffer.size() > PLOT_SIZE) ? graph_buffer.size() - PLOT_SIZE : 0;
	plotBuffer.insert(plotBuffer.end(), graph_buffer.begin() + startIdx, graph_buffer.end());

	// Apply Moving Average Smoothing
	std::vector<double> smoothedBuffer;
	for (size_t i = 0; i < plotBuffer.size(); i++) {
		double sum = 0, count = 0;
		for (size_t j = std::max(0, (int)i - SMOOTHING_WINDOW + 1); j <= i; j++) {
			sum += plotBuffer[j];
			count++;
		}
		smoothedBuffer.push_back(sum / count);
	}

	// Compute min/max from the entire BUFFER_SIZE history (not just what’s plotted)
	double minGreen;
	double maxGreen;

	if (!graph_buffer.empty()) {
		minGreen = *std::min_element(graph_buffer.begin(), graph_buffer.end());
		maxGreen = *std::max_element(graph_buffer.begin(), graph_buffer.end());
	} else {
		obs_log(LOG_INFO, "Buffer is empty! Setting default min/max values.");
		minGreen = 0.0; // Default value
		maxGreen = 1.0; // Default value
	}

	// Prevent zero range issue
	if (minGreen == maxGreen) {
		minGreen -= 1;
		maxGreen += 1;
	}

	std::vector<double> smoothedY(smoothedBuffer.size());
	for (size_t i = 0; i < smoothedBuffer.size(); i++) {
		smoothedY[i] = height - (smoothedBuffer[i] - minGreen) / (maxGreen - minGreen) * height;
	}
	return smoothedY;
}

void graph_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct graph_source *graphSource = reinterpret_cast<struct graph_source *>(data);
	if (!graphSource || !graphSource->source) {
		return; // Ensure graphSource is valid
	}

	obs_log(LOG_INFO, "Rendering graph/signal source...");

	// Retrieve OBS settings for the heart rate monitor source
	obs_source_t *heartRateSource = get_heart_rate_monitor_filter();
	if (!heartRateSource) {
		obs_log(LOG_INFO, "Failed to get heart rate source");
		return;
	}
	obs_data_t *hrsSettings = obs_source_get_settings(heartRateSource);
	const char *sourceName = obs_source_get_name(graphSource->source);
	obs_log(LOG_INFO, "Source Name: %s", sourceName);
	if (strcmp(sourceName, GRAPH_SOURCE_NAME) == 0) {
		double curHeartRate = obs_data_get_double(hrsSettings, "heart rate"); // Retrieve heart rate
		obs_data_release(hrsSettings);
		obs_source_release(heartRateSource);
		draw_graph(graphSource, curHeartRate);
	} else if (strcmp(sourceName, SIGNAL_SOURCE_NAME) == 0) {
		double avgGreen = obs_data_get_double(hrsSettings, "average green");
		obs_log(LOG_INFO, ("Average Green: " + std::to_string(avgGreen)).c_str());
		obs_data_release(hrsSettings);
		obs_source_release(heartRateSource);
		draw_graph(graphSource, avgGreen);
		obs_log(LOG_INFO, std::to_string(avgGreen).c_str());
	} else {
		obs_data_release(hrsSettings);
		obs_source_release(heartRateSource);
	}

	// obs_log(LOG_INFO, "Graph rendering completed!");
}

uint32_t get_color_code(int color_option)
{
	switch (color_option) {
	case 0:                    // White
		return 0xFFFFFFFF; // RGBA: White
	case 1:                    // Red
		return 0xFFFF0000; // RGBA: Red
	case 2:                    // Yellow
		return 0xFFFFFF00; // RGBA: Yellow
	case 3:                    // Green
		return 0xFF00FF00; // RGBA: Green
	case 4:                    // Blue
		return 0xFF0000FF; // RGBA: Blue
	default:
		return 0xFFFFFFFF; // Default: White
	}
}

void draw_graph(struct graph_source *graph_source, double data)
{

	if (!graph_source || !graph_source->source) {
		obs_log(LOG_INFO, "graph source is null");
		return; // Null check to avoid crashes
	}
	// Retrieve source width and height
	uint32_t width = obs_source_get_width(graph_source->source);
	uint32_t height = obs_source_get_height(graph_source->source);

	// obs_log(LOG_INFO, "Graph source dimensions: Width = %u, Height = %u", width, height);

	if (width == 0 || height == 0)
		return; // Avoid division by zero

	const char *sourceName = obs_source_get_name(graph_source->source);
	// Maintain a buffer size of 10
	if (data > 0) {
		size_t bufferSize;
		if (strcmp(sourceName, GRAPH_SOURCE_NAME) == 0) {
			bufferSize = 10;
		} else if (strcmp(sourceName, SIGNAL_SOURCE_NAME) == 0) {
			bufferSize = BUFFER_SIZE;
		} else {
			bufferSize = 0;
		}
		while (graph_source->buffer.size() >= bufferSize) {
			graph_source->buffer.erase(graph_source->buffer.begin());
		}
		graph_source->buffer.push_back(data);
	}

	std::vector<double> smoothedY;
	if (strcmp(sourceName, SIGNAL_SOURCE_NAME) == 0) {
		smoothedY = getSmoothedY(graph_source->buffer, height);
	}

	obs_enter_graphics();

	// Ensure no conflicting active effects
	gs_effect_t *active_effect = gs_get_effect();
	if (active_effect) {
		gs_technique_t *active_technique = gs_effect_get_current_technique(active_effect);
		gs_technique_end(active_technique);
		gs_effect_destroy(active_effect);
	}

	obs_source_t *heartRateSource = get_heart_rate_monitor_filter();
	if (!heartRateSource) {
		obs_log(LOG_INFO, "Failed to get heart rate source");
		return;
	}
	obs_data_t *hrsSettings = obs_source_get_settings(heartRateSource);

	// Get base effect
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	while (gs_effect_loop(effect, "Solid")) {
		// Set background colour
		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFFFFFFFF);
		gs_draw_sprite(nullptr, 0, width, height);

		// Set color for the graph (Default: Red)
		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"),
				    get_color_code(obs_data_get_int(hrsSettings, "graphLineDropdown")));

		obs_log(LOG_INFO, "Drawing heart rate graph... %d values", graph_source->buffer.size());

		// **Simulate thicker lines by drawing multiple parallel lines**
		for (float offset = -LINE_THICKNESS / 2; offset <= LINE_THICKNESS / 2; offset += 1.0f) {
			if (strcmp(sourceName, GRAPH_SOURCE_NAME) == 0) {
				gs_render_start(GS_LINESTRIP);
				for (size_t i = 0; i < graph_source->buffer.size(); i++) {
					float x = (static_cast<float>(i) / 9.0f) * width;
					float y = height -
						  (static_cast<float>(graph_source->buffer[i]) / 200.0f) * height;
					gs_vertex2f(x, y + offset); // Shift the line slightly to create thickness
				}
				gs_render_stop(GS_LINESTRIP);
			} else if (strcmp(sourceName, SIGNAL_SOURCE_NAME) == 0) {
				gs_render_start(GS_LINESTRIP);
				for (size_t i = 0; i < smoothedY.size(); i++) {
					float x = (static_cast<float>(i) / PLOT_SIZE) * width;
					float y = smoothedY[i];
					obs_log(LOG_INFO, ("Normalized y: " + std::to_string(y)).c_str());
					gs_vertex2f(x, y + offset); // Shift the line slightly to create thickness
				}
				gs_render_stop(GS_LINESTRIP);
			}
		}

		// **Draw X-Axis (Horizontal Line)**
		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"),
				    get_color_code(obs_data_get_int(hrsSettings, "graphPlaneDropdown")));
		gs_render_start(GS_LINES);

		gs_vertex2f(0.0f, height); // Midpoint of the height
		gs_vertex2f(width, height);

		gs_render_stop(GS_LINES);

		// **Draw Y-Axis (Vertical Line)**
		gs_render_start(GS_LINES);

		gs_vertex2f(0.0f, 0.0f); // Small offset from the left
		gs_vertex2f(0.0f, height);

		gs_render_stop(GS_LINES);

		// **Draw Y-Axis Labels (20, 40, ..., 200)**
		for (int i = 20; i <= 200; i += 20) {
			float y = height - (static_cast<float>(i) / 200.0f) * height; // Scale Y position

			// Tick mark for the label
			gs_render_start(GS_LINES);
			gs_vertex2f(-1.0f, y); // Left end of tick
			gs_vertex2f(1.0f, y);  // Right end of tick
			gs_render_stop(GS_LINES);
		}
	}

	obs_data_release(hrsSettings);
	obs_source_release(heartRateSource);

	obs_leave_graphics();
}

const char *get_graph_source_name(void *)
{
	return GRAPH_SOURCE_NAME;
}

const char *get_signal_source_name(void *)
{
	return SIGNAL_SOURCE_NAME;
}

void *create_graph_source_info(obs_data_t *settings, obs_source_t *source)
{
	void *data = bmalloc(sizeof(struct graph_source));
	struct graph_source *graph_src = new (data) graph_source();
	graph_src->source = source;
	if (!source) {
		obs_log(LOG_INFO, "current source in create graph source is null");
		return nullptr;
	}

	std::vector<double> buffer;
	graph_src->buffer = buffer;

	return graph_src;
}
uint32_t graph_source_info_get_width(void *data)
{
	UNUSED_PARAMETER(data);
	return 250;
}
uint32_t graph_source_info_get_height(void *data)
{
	UNUSED_PARAMETER(data);
	return 250;
}