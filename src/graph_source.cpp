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
// void destroy_graph_source(void *data)
// {
// 	obs_log(LOG_INFO, "Graph source destroyed");
// 	struct graph_source *graph = reinterpret_cast<struct graph_source *>(data);

// 	if (graph) {
// 		obs_enter_graphics();

// 		// Destroy vertex buffer
// 		if (graph->vertex_buffer) {
// 			gs_vertexbuffer_destroy(graph->vertex_buffer);
// 			graph->vertex_buffer = nullptr;
// 		}

// 		// Destroy effect
// 		if (graph->effect) {
// 			gs_effect_destroy(graph->effect);
// 			graph->effect = nullptr;
// 		}

// 		// Release the OBS source
// 		if (graph->source) {
// 			obs_source_release(graph->source);
// 			graph->source = nullptr;
// 		}

// 		obs_leave_graphics();

// 		// Clear buffer (only applicable in C++)
// #ifdef __cplusplus
// 		graph->buffer.clear();
// #endif

// 		// Free memory
// 		bfree(graph);
// 	}
// }

static bool find_heart_rate_monitor_filter(void *param, obs_source_t *source)
{
	const char *filter_name = "Heart Rate Monitor";

	// Try to get the filter from the current source
	obs_source_t *filter = obs_source_get_filter_by_name(source, filter_name);
	obs_log(LOG_INFO, "Source: %s", obs_source_get_name(source));

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

	obs_log(LOG_INFO, "Rendering graph source...");

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

	obs_log(LOG_INFO, "Current heart rate: %d", curHeartRate);

	// Draw the graph using the retrieved heart rate
	draw_graph(graphSource, curHeartRate);

	obs_log(LOG_INFO, "Graph rendering completed!");
}

void draw_graph(struct graph_source *graph_source, int curHeartRate)
{
	obs_log(LOG_INFO, "checking for null graph source");

	if (!graph_source)
		return; // Null check to avoid crashes
	graph_source->buffer.push_back(60);

	// Maintain a buffer size of 10
	while (graph_source->buffer.size() >= 10) {
		graph_source->buffer.erase(graph_source->buffer.begin());
	}
	graph_source->buffer.push_back(curHeartRate);

	obs_log(LOG_INFO, "Updating heart rate buffer...");

	// Start rendering
	if (!graph_source->effect) {
		obs_log(LOG_ERROR, "Failed to get solid effect for rendering");
		return;
	}

	while (gs_effect_loop(graph_source->effect, "Solid")) {
		gs_effect_set_color(gs_effect_get_param_by_name(graph_source->effect, "color"), 0xFF0000FF);

		gs_render_start(true); // Use GS_LINESTRIP to connect the points

		obs_log(LOG_INFO, "Drawing heart rate graph... %d values", graph_source->buffer.size());

		for (size_t i = 0; i < graph_source->buffer.size(); i++) {

			gs_vertex2f(static_cast<float>(i * 10), static_cast<float>(graph_source->buffer[i]));
		}

		gs_render_stop(GS_LINESTRIP);
	}
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

	graph_src->effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	if (!graph_src->effect) {
		destroy_graph_source_info(graph_src);
		graph_src = NULL;
	}
	return graph_src;
}
void destroy_graph_source_info(void *data)
{
	struct graph_source *graph_src = (struct graph_source *)data;
	obs_enter_graphics();
	gs_effect_destroy(graph_src->effect);
	obs_leave_graphics();
	graph_src->~graph_source();
	bfree(graph_src);
}
// void graph_source_info_render(void *data, gs_effect_t *effect) {
//     UNUSED_PARAMETER(effect);
// 		UNUSED_PARAMETER(data);
// 		obs_log(LOG_INFO, "start graph_source_render");
//     struct graph_source *graphSource = reinterpret_cast<struct graph_source *>(data);
//     if (!graphSource || !graphSource->source) {
//         return;  // Ensure graphSource is valid
//     }

//     // obs_log(LOG_INFO, "Rendering graph source...");
// 		obs_source_t *source = graphSource->source;

//     // // Retrieve OBS settings for the heart rate monitor source
//     // obs_source_t *heart_rate_source = obs_get_source_by_name("Video Capture Device");
//     if (!source) {
//         obs_log(LOG_ERROR, "Graph source not found");
// 		// skip_video_filter_if_safe(heart_rate_source);
//         return;
//     } else {
// 			obs_log(LOG_INFO, "found graohp source");
// 		}
// 		obs_log(LOG_INFO, "end graph_source_render");
//     // obs_data_t *settings = obs_source_get_settings(heart_rate_source);
//     // int curHeartRate = static_cast<int>(obs_data_get_int(settings, "heart rate"));  // Retrieve heart rate
//     // obs_data_release(settings);
//     // obs_source_release(heart_rate_source);

//     // obs_log(LOG_INFO, "Current heart rate: %d", curHeartRate);

//     // // Draw the graph using the retrieved heart rate
//     // draw_graph(graphSource, curHeartRate);

//     // obs_log(LOG_INFO, "Graph rendering completed!");
// }
uint32_t graph_source_info_get_width(void *data)
{
	UNUSED_PARAMETER(data);
	return 0;
}
uint32_t graph_source_info_get_height(void *data)
{
	UNUSED_PARAMETER(data);
	return 0;
}
