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
#include <algorithm>
#include <random>
#include "plugin-support.h"
#include "obs_utils.h"
#include "graph_source.h"
#include "graph_source_info.h"
#include "heart_rate_source.h"
#include <chrono>

#define LINE_THICKNESS 3.0f
#define UPDATE_FREQUENCY 15
#define PIXEL_PER_HR 3.5f

static int frameCount = 0;

// Destroy function for graph source
void destroyGraphSource(void *data)
{
	struct graph_source *graph = reinterpret_cast<struct graph_source *>(data);

	if (graph) {
		graph->isDisabled = true;
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

static bool findHeartRateMonitorFilter(void *param, obs_source_t *source)
{
	const char *filterName = MONITOR_SOURCE_NAME;

	// Try to get the filter from the current source
	obs_source_t *filter = obs_source_get_filter_by_name(source, filterName);

	if (filter) {
		// Store the filter reference in param
		*(obs_source_t **)param = filter;
		return false; // Stop further enumeration
	}

	return true; // Continue searching
}

obs_source_t *getHeartRateMonitorFilter()
{
	obs_source_t *found_filter = nullptr;

	// Enumerate all sources to find the desired filter
	obs_enum_sources(findHeartRateMonitorFilter, &found_filter);

	return found_filter; // Return the filter reference (must be released later)
}

void graphSourceRender(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct graph_source *graphSource = reinterpret_cast<struct graph_source *>(data);
	if (!graphSource || !graphSource->source || graphSource->isDisabled) {
		return; // Ensure graphSource is valid
	}

	int curHeartRate = -1;

	if (graphSource->ecg || frameCount % UPDATE_FREQUENCY == 0) {
		// Retrieve OBS settings for the heart rate monitor source
		obs_source_t *heartRateSource = getHeartRateMonitorFilter();
		if (!heartRateSource) {
			return;
		}
		obs_data_t *hrsSettings = obs_source_get_settings(heartRateSource);

		if (obs_data_get_bool(hrsSettings, "is disabled")) {
			obs_data_release(hrsSettings);
			obs_source_release(heartRateSource);
			return;
		}
		curHeartRate = obs_data_get_int(hrsSettings, "heart rate"); // Retrieve heart rate
		obs_data_release(hrsSettings);
		obs_source_release(heartRateSource);
	}
	frameCount++;

	// Draw the graph using the retrieved heart rate
	drawGraph(graphSource, curHeartRate, graphSource->ecg);
}

static void thickenLines(const std::vector<std::pair<float, float>> &points)
{
	gs_render_start(GS_LINESTRIP);
	for (size_t i = 0; i < points.size() - 1; i++) {
		float x1 = points[i].first;
		float y1 = points[i].second;
		float x2 = points[i + 1].first;
		float y2 = points[i + 1].second;
		// Compute direction of the segment
		float dx = x2 - x1;
		float dy = y2 - y1;
		float length = std::sqrt(dx * dx + dy * dy);
		if (length == 0)
			continue;
		// Compute perpendicular vector (normal)
		float nx = -dy / length;
		float ny = dx / length;
		// Offset points perpendicular to the line
		for (float offset = -LINE_THICKNESS / 2; offset <= LINE_THICKNESS / 2; offset += 0.1f) {
			gs_vertex2f(x1 + nx * offset, y1 + ny * offset);
			gs_vertex2f(x2 + nx * offset, y2 + ny * offset);
		}
	}

	gs_render_stop(GS_LINESTRIP);
}

std::vector<float> generate_ecg_waveform(int heartRate, int width)
{
	std::vector<float> waveform(width, 0.0f);

	// ECG timing parameters based on heart rate
	float cycle_length = 60.0f / heartRate * width; // ECG cycle in pixels
	// float p_wave_start = 0.15f * cycle_length;
	// float q_wave_start = 0.3f * cycle_length;
	// float r_wave_start = 0.35f * cycle_length;
	// float s_wave_start = 0.4f * cycle_length;
	// float t_wave_start = 0.6f * cycle_length;

	// Generate waveform pattern
	for (int i = 0; i < width; i++) {
		float pos = static_cast<float>(i) / cycle_length; // Normalize position in cycle

		// P wave: small upward bump
		if (pos > 0.1f && pos < 0.2f)
			waveform[i] = 0.05f * sin((pos - 0.15f) * M_PI * 10);

		// Q wave: small downward dip
		else if (pos > 0.3f && pos < 0.32f)
			waveform[i] = -0.1f;

		// R wave: large upward spike
		else if (pos > 0.35f && pos < 0.37f)
			waveform[i] = 0.6f;

		// S wave: small downward dip after R wave
		else if (pos > 0.4f && pos < 0.42f)
			waveform[i] = -0.2f;

		// T wave: broad, small positive bump
		else if (pos > 0.6f && pos < 0.8f)
			waveform[i] = 0.1f * sin((pos - 0.7f) * M_PI * 5);
	}

	return waveform;
}

// Define a function to get delta time
float getDeltaTime()
{
	static auto lastTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();

	std::chrono::duration<float> elapsed = currentTime - lastTime;
	lastTime = currentTime;

	return elapsed.count(); // Return time in seconds
}

void drawGraph(struct graph_source *graphSource, int curHeartRate, bool ecg)
{
	if (!graphSource || !graphSource->source)
		return; // Null check to avoid crashes

	// Retrieve source width and height
	uint32_t width = obs_source_get_width(graphSource->source);
	uint32_t height = obs_source_get_height(graphSource->source);
	obs_source_t *heartRateSource = getHeartRateMonitorFilter();
	if (!heartRateSource) {
		obs_log(LOG_INFO, "Failed to get heart rate source");
		return;
	}
	obs_data_t *hrsSettings = obs_source_get_settings(heartRateSource);
	int graphSize = obs_data_get_int(hrsSettings, "heart rate graph size");

	if (width == 0 || height == 0 || graphSize == 0)
		return; // Avoid division by zero

	// Maintain a buffer size of 10
	if (curHeartRate > 0) {
		while (graphSource->buffer.size() >= static_cast<size_t>(graphSize)) {
			graphSource->buffer.erase(graphSource->buffer.begin());
		}
		graphSource->buffer.push_back(curHeartRate);
	}

	obs_enter_graphics();

	// Ensure no conflicting active effects
	gs_effect_t *activeEffect = gs_get_effect();
	if (activeEffect) {
		gs_technique_t *activeTechnique = gs_effect_get_current_technique(activeEffect);
		gs_technique_end(activeTechnique);
		gs_effect_destroy(activeEffect);
	}

	// Get base effect
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	while (gs_effect_loop(effect, "Solid")) {

		int background = obs_data_get_int(hrsSettings, "graph plane dropdown");
		if (background == 1) {
			// **Draw background stripes for heart rate regions**
			struct {
				int minHr, maxHr;
				uint32_t colour;
			} heartRateZones[] = {
				{50, 90, 0xFF00FF00},   // Green (Good)
				{90, 120, 0xFFFFFF00},  // Yellow (Warning - High)
				{120, 150, 0xFFFFA500}, // Orange (Caution - High)
				{150, 180, 0xFFFF0000}  // Red (Bad - Too High)
			};

			for (size_t i = 0; i < sizeof(heartRateZones) / sizeof(heartRateZones[0]); i++) {
				float top = height -
					    (static_cast<float>(heartRateZones[i].maxHr - 50) / 260.0f) * height * 2;
				float bottom = height -
					       (static_cast<float>(heartRateZones[i].minHr - 50) / 260.0f) * height * 2;

				gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"),
						    heartRateZones[i].colour);
				// Push matrix and translate to correct Y position
				gs_matrix_push();
				gs_matrix_translate3f(0, top, 0);                // Move to the correct position
				gs_draw_sprite(nullptr, 0, width, bottom - top); // Draw stripe
				gs_matrix_pop();                                 // Restore previous transformation
			}
		} else if (background == 2) {
			// Get the colour of the graph plane from the colour picker, convert it to ARGB instead of ABGR
			uint32_t graphPlaneAbgrColour = obs_data_get_int(hrsSettings, "graph plane colour");
			uint32_t graphPlaneArgbColour =
				(graphPlaneAbgrColour & 0xFF000000) | ((graphPlaneAbgrColour & 0xFF) << 16) |
				(graphPlaneAbgrColour & 0xFF00) | ((graphPlaneAbgrColour & 0xFF0000) >> 16);

			// Set background colour to chosen colour
			gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), graphPlaneArgbColour);
			gs_draw_sprite(nullptr, 0, width, height);
		}

		if (graphSource->buffer.size() >= 3) {
			// obs_log(LOG_INFO, "1");

			std::vector<std::pair<float, float>> points;

			if (ecg) {
				// Get the colour of the graph line from the colour picker, convert it to RGB instead of BGR
				uint32_t ecgBackgroundAbgrColour =
					obs_data_get_int(hrsSettings, "ecg background colour");
				uint32_t ecgBackgroundArgbColour = (ecgBackgroundAbgrColour & 0xFF000000) |
								   ((ecgBackgroundAbgrColour & 0xFF) << 16) |
								   (ecgBackgroundAbgrColour & 0xFF00) |
								   ((ecgBackgroundAbgrColour & 0xFF0000) >> 16);

				// Set colour for the graph using the colour from the colour picker
				gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"),
						    ecgBackgroundArgbColour);
				gs_draw_sprite(nullptr, 0, width, height); // Draw stripe

				float baseHeight = height / 2;
				float beatsPerSecond = curHeartRate / 100.0f;
				float deltaTime = getDeltaTime(); // Get frame time

				float waveSpeed = (width / 2) * beatsPerSecond * deltaTime; // Movement speed

				// Generate ECG waveform for one cycle (only once, reuse for efficiency)
				static std::vector<float> ecg_wave = generate_ecg_waveform(curHeartRate, width / 2);

				// Compute phase shift (how much the wave moves per frame)
				static float waveOffset = 0.0f;
				waveOffset += waveSpeed;

				if (waveOffset >= width / 2) {
					waveOffset -= width / 2; // Wrap around within half width
				}

				// **Clear the points before drawing**
				points.clear();

				// **Draw first wave**
				for (size_t i = 0; i < width / 2; i++) {
					size_t shiftedIndex = (i + static_cast<size_t>(waveOffset)) % (width / 2);

					float x = static_cast<float>(i);
					float y = baseHeight -
						  (ecg_wave[shiftedIndex] * height * 0.4f); // Scale ECG wave height

					points.push_back({x, y});
				}

				// **Draw second wave immediately after the first one**
				for (size_t i = 0; i < width / 2; i++) {
					size_t shiftedIndex = (i + static_cast<size_t>(waveOffset)) % (width / 2);

					float x =
						static_cast<float>(i + width / 2); // Offset x-position for second wave
					float y = baseHeight -
						  (ecg_wave[shiftedIndex] * height * 0.4f); // Scale ECG wave height

					points.push_back({x, y});
				}

				// Get the colour of the graph line from the colour picker, convert it to RGB instead of BGR
				uint32_t ecgLineAbgrColour = obs_data_get_int(hrsSettings, "ecg line colour");
				uint32_t ecgLineArgbColour =
					(ecgLineAbgrColour & 0xFF000000) | ((ecgLineAbgrColour & 0xFF) << 16) |
					(ecgLineAbgrColour & 0xFF00) | ((ecgLineAbgrColour & 0xFF0000) >> 16);

				// Set colour for the graph using the colour from the colour picker
				gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), ecgLineArgbColour);
				thickenLines(points);
			} else {
				for (size_t i = 0; i < graphSource->buffer.size() - 1; i++) {
					float x1 = (static_cast<float>(i) / (graphSize - 1)) * width;

					// Calculate the y1 value in the zoomed in middle interval representation
					float y1;
					if (graphSource->buffer[i] <= 70) {
						y1 = height - static_cast<float>(graphSource->buffer[i] - 50);
					} else if (graphSource->buffer[i] <= 110) {
						y1 = height - 20 -
						     (static_cast<float>(graphSource->buffer[i] - 70) * 4);
					} else {
						y1 = height - 20 - 160 -
						     static_cast<float>(graphSource->buffer[i] - 110);
					}

					// // Increase the difference scaling (was *2, now *4 for more contrast)
					// float y1 = height -
					// 	   std::clamp(std::round((static_cast<float>(graphSource->buffer[i] -
					// 						     50)) *
					// 				 PIXEL_PER_HR),
					// 		      0.0f, static_cast<float>(height));

					points.push_back({x1, y1});
					if (i == graphSource->buffer.size() - 2) {
						points.push_back({width, y1});
					}
				}

				// Get the colour of the graph line from the colour picker, convert it to RGB instead of BGR
				uint32_t graphLineAbgrColour = obs_data_get_int(hrsSettings, "graph line colour");
				uint32_t graphLineArgbColour =
					(graphLineAbgrColour & 0xFF000000) | ((graphLineAbgrColour & 0xFF) << 16) |
					(graphLineAbgrColour & 0xFF00) | ((graphLineAbgrColour & 0xFF0000) >> 16);

				// Set colour for the graph using the colour from the colour picker
				gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), graphLineArgbColour);
				thickenLines(points);

				std::vector<std::pair<float, float>> points;
				// Draw X-Axis (Horizontal Line)
				points.push_back({0.0f, height});
				points.push_back({width, height});
				thickenLines(points);
				// Draw Y-Axis (Vertical Line)
				points.clear();
				points.push_back({0.0f, 0.0f});
				points.push_back({0.0f, height});
				thickenLines(points);

				// **Draw Y-Axis Labels (70, ..., 190)**
				// for (float i = 20; i <= height; i += 20) {
				// 	if (i == 20 || i == 100 || i >= 180) {
				// 		float y = height - i;
				// 		points.clear();
				// 		points.push_back({0.0f, y});
				// 		points.push_back({5.0f, y});
				// 		thickenLines(points);
				// 	}
				// }

				// // **Draw Y-Axis Labels (60, 80, ..., 180)**
				for (float i = 10; i <= std::min(130.0f, height / PIXEL_PER_HR); i += 20) {
					float y = height - i * PIXEL_PER_HR;
					points.clear();
					points.push_back({0.0f, y});
					points.push_back({5.0f, y});
					thickenLines(points);
				}
			}
		}
	}

	obs_data_release(hrsSettings);
	obs_source_release(heartRateSource);

	obs_leave_graphics();
}

const char *getGraphSourceName(void *)
{
	return GRAPH_SOURCE_NAME;
}

const char *getECGSourceName(void *)
{
	return ECG_SOURCE_NAME;
}

static void *createGeneralGraphSourceInfo(obs_source_t *source, bool ecg)
{
	void *data = bmalloc(sizeof(struct graph_source));
	struct graph_source *graphSrc = new (data) graph_source();
	graphSrc->source = source;
	if (!source) {
		obs_log(LOG_INFO, "current source in create graph source is null");
		if (graphSrc) {
			bfree(graphSrc);
		}
		return nullptr;
	}

	std::vector<int> buffer;
	graphSrc->buffer = buffer;

	graphSrc->isDisabled = false;
	graphSrc->ecg = ecg;

	return graphSrc;
}

void *createGraphSourceInfo(obs_data_t *settings, obs_source_t *source)
{
	return createGeneralGraphSourceInfo(source, false);
}

void *createECGSourceInfo(obs_data_t *settings, obs_source_t *source)
{
	return createGeneralGraphSourceInfo(source, true);
}

uint32_t graphSourceInfoGetWidth(void *data)
{
	UNUSED_PARAMETER(data);
	obs_source_t *hrs = getHeartRateMonitorFilter();
	if (!hrs) {
		return 0;
	}
	obs_data_t *settings = obs_source_get_settings(hrs);
	int size = obs_data_get_int(settings, "heart rate graph size");
	obs_data_release(settings);
	obs_source_release(hrs);
	if (size < 20) {
		return 260;
	} else if (size < 30) {
		return 520;
	} else {
		return 780;
	}
}
uint32_t graphSourceInfoGetHeight(void *data)
{
	UNUSED_PARAMETER(data);
	return 260;
}

uint32_t ecgSourceInfoGetWidth(void *data)
{
	UNUSED_PARAMETER(data);
	return 260;
}

void graphSourceActivate(void *data)
{
	struct graph_source *graphSource = reinterpret_cast<graph_source *>(data);
	graphSource->isDisabled = false;
}

void graphSourceDeactivate(void *data)
{
	struct graph_source *graphSource = reinterpret_cast<graph_source *>(data);
	graphSource->isDisabled = true;
}