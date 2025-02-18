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

// Destroy function for graph source
void destroy_graph_source(void *data)
{
	// obs_log(LOG_INFO, "Graph source destroyed");
	struct graph_source *graph = reinterpret_cast<struct graph_source *>(data);

	if (graph) {

		// // Release the OBS source
		// if (graph->source) {
		// 	obs_source_release(graph->source);
		// 	graph->source = nullptr;
		// };

		// 		// Clear buffer (only applicable in C++)
		// #ifdef __cplusplus
		// 		graph->buffer.clear();
		// #endif

		// 		// Free memory
		// 		bfree(graph);
	}
}

static bool find_heart_rate_monitor_filter(void *param, obs_source_t *source)
{
	const char *filter_name = "Heart Rate Monitor";

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

void graph_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct graph_source *graphSource = reinterpret_cast<struct graph_source *>(data);
	if (!graphSource || !graphSource->source) {
		return; // Ensure graphSource is valid
	}

	// obs_log(LOG_INFO, "Rendering graph source...");

	// Retrieve OBS settings for the heart rate monitor source
	obs_source_t *heartRateSource = get_heart_rate_monitor_filter();
	if (!heartRateSource) {
		obs_log(LOG_INFO, "Failed to get heart rate source");
		return;
	}
	obs_data_t *hrsSettings = obs_source_get_settings(heartRateSource);
	int curHeartRate = obs_data_get_int(hrsSettings, "heart rate"); // Retrieve heart rate
	obs_data_release(hrsSettings);
	obs_source_release(heartRateSource);

	// Draw the graph using the retrieved heart rate
	draw_graph(graphSource, curHeartRate);

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

void draw_graph(struct graph_source *graph_source, int curHeartRate)
{
	// obs_log(LOG_INFO, "Checking for null graph source");
	if (!graph_source || !graph_source->source)
		return; // Null check to avoid crashes

	// Retrieve source width and height
	uint32_t width = obs_source_get_width(graph_source->source);
	uint32_t height = obs_source_get_height(graph_source->source);

	// obs_log(LOG_INFO, "Graph source dimensions: Width = %u, Height = %u", width, height);

	if (width == 0 || height == 0)
		return; // Avoid division by zero

	// Maintain a buffer size of 10
	if (curHeartRate > 0) {
		while (graph_source->buffer.size() >= 10) {
			graph_source->buffer.erase(graph_source->buffer.begin());
		}
		graph_source->buffer.push_back(curHeartRate);
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
		// Set color for the graph (Red)
		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"),
				    get_color_code(obs_data_get_int(hrsSettings, "graphLineDropdown")));

		gs_render_start(GS_LINESTRIP); // Use GS_LINESTRIP to connect the points
		// obs_log(LOG_INFO, "Drawing heart rate graph... %d values", graph_source->buffer.size());

		// Normalize and scale values to fit within the source box
		for (size_t i = 0; i < graph_source->buffer.size(); i++) {
			float x = (static_cast<float>(i) / 9.0f) * width; // Scale X across width
			float y = height -
				  (static_cast<float>(graph_source->buffer[i]) / 200.0f) * height; // Scale Y to fit

			gs_vertex2f(x, y);
		}

		gs_render_stop(GS_LINESTRIP);

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

void *create_graph_source_info(obs_data_t *settings, obs_source_t *source)
{
	void *data = bmalloc(sizeof(struct graph_source));
	struct graph_source *graph_src = new (data) graph_source();
	graph_src->source = source;
	if (!source) {
		obs_log(LOG_INFO, "current source in create graph source is null");
		return nullptr;
	}

	std::vector<int> buffer;
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
