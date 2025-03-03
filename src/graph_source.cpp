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

#define LINE_THICKNESS 3.0f

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
	int curHeartRate = obs_data_get_int(hrsSettings, "heart rate"); // Retrieve heart rate
	obs_data_release(hrsSettings);
	obs_source_release(heartRateSource);

	// Draw the graph using the retrieved heart rate
	drawGraph(graphSource, curHeartRate, graphSource->ecg);
}

static float getRandomFloat(float min, float max) {
  static std::random_device rd;  // Seed
  static std::mt19937 gen(rd()); // Mersenne Twister RNG
  std::uniform_real_distribution<float> dist(min, max);
  return dist(gen);
}

static 

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

		// Get the colour of the graph line from the colour picker, convert it to RGB instead of BGR
		uint32_t graphLineAbgrColour = obs_data_get_int(hrsSettings, "graph line colour");
		uint32_t graphLineArgbColour = (graphLineAbgrColour & 0xFF000000) |
					       ((graphLineAbgrColour & 0xFF) << 16) | (graphLineAbgrColour & 0xFF00) |
					       ((graphLineAbgrColour & 0xFF0000) >> 16);

		// Set colour for the graph using the colour from the colour picker
		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), graphLineArgbColour);
		if (graphSource->buffer.size() >= 3) {
      gs_render_start(GS_LINESTRIP);
      if (ecg) {
        float totalHr = -150;
        for (size_t i = 0; i < 3; i++) {
          totalHr += (float)graphSource->buffer[i];
        }
        float hr_start = 0;
        float hr_end = 0;
        for (size_t i = 0; i < 3; i++) {
          hr_start = hr_end;
          hr_end += (float)(graphSource->buffer[i] - 50) / totalHr * width;
          float hr_width = hr_end - hr_start;

          float normalizedHR =
            (float)(graphSource->buffer[i] - 50) / (float)(180 - 50);
          float peakHeight = height * 0.2 + (normalizedHR * height * 0.3);
          float numPeaks = (float)(normalizedHR * 10.0 + 1.0);

          for (float j = 0; j < std::floor(numPeaks); j++) {
            float x_start =
              hr_start +
              (static_cast<float>(j) / (float)numPeaks) * hr_width;
            float x_end = hr_start +
                    (static_cast<float>(j + 1) / (float)numPeaks) *
                      hr_width;
            float seg_width = (x_end - x_start) / 6;

            // Horizontal start
            gs_vertex2f(x_start, height / 2);
            // Slope start
            gs_vertex2f(x_start + seg_width * 2, height / 2);
            // Positive peak
            gs_vertex2f(x_start + seg_width * 3, height / 2 - peakHeight + getRandomFloat(0, 0.5 * peakHeight));
            // Negative peak
            gs_vertex2f(x_end - seg_width, height / 2 + peakHeight - getRandomFloat(0, 0.5 * peakHeight));
            // Slope end
            gs_vertex2f(x_end, height / 2);
          }
        }
      } else {
        for (size_t i = 0; i < graphSource->buffer.size() - 1; i++) {
          float x1 = (static_cast<float>(i) / (graphSize - 1)) * width;
          float y1 =
            height - (static_cast<float>(graphSource->buffer[i] - 50)) * 2;
          float x2 = (static_cast<float>(i + 1) / (graphSize - 1)) * width;
          float y2 = height -
              (static_cast<float>(graphSource->buffer[i + 1] - 50)) * 2;

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
      }

      gs_render_stop(GS_LINESTRIP);
		}

		// **Draw X-Axis (Horizontal Line)**
		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFF000000);
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