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
#include "heart_rate_source.h"
#include "heart_rate_source_info.h"

void add_graph_source_to_scene(obs_source_t *graph_obs_source, obs_scene_t *scene)
{
	// Add the graph to the OBS scene
	obs_scene_add(scene, graph_obs_source);

	// Set transform properties
	obs_transform_info transform_info;
	transform_info.pos.x = 300.0f;
	transform_info.pos.y = 600.0f;
	transform_info.bounds.x = 400.0f;
	transform_info.bounds.y = 200.0f;
	transform_info.bounds_type = OBS_BOUNDS_SCALE_INNER;
	transform_info.bounds_alignment = OBS_ALIGN_CENTER;
	transform_info.alignment = OBS_ALIGN_CENTER;
	transform_info.scale.x = 30.0f;
	transform_info.scale.y = 30.0f;
	transform_info.rot = 0.0f;

	obs_sceneitem_t *scene_item = get_scene_item_from_source(scene, graph_obs_source);
	if (scene_item != NULL) {
		obs_sceneitem_set_info2(scene_item, &transform_info);
		obs_sceneitem_release(scene_item);
	}

	obs_source_release(graph_obs_source);
}

void create_graph_source(obs_scene_t *scene, obs_source_t *parent_source)
{
	obs_log(LOG_INFO, "create graph source");

	// Create OBS source for the graph
	obs_data_t *graph_settings = obs_data_create();
	obs_data_set_int(graph_settings, "heart rate", 1);
	obs_source_t *graph_obs_source =
		obs_source_create("heart_rate_graph", GRAPH_SOURCE_NAME, graph_settings, nullptr);
	obs_data_release(graph_settings);
	if (!graph_obs_source) {
		obs_log(LOG_INFO, "graph_obs_source is null");
	}

	// obs_log(LOG_INFO, "parent source name: %s", obs_source_get_name(parent_source));
	// obs_source_filter_add(parent_source, graph_obs_source);

	obs_scene_add(scene, graph_obs_source);
	obs_transform_info transform_info;
	transform_info.pos.x = 100.0f;
	transform_info.pos.y = 300.0f;
	transform_info.bounds.x = 600.0f;
	transform_info.bounds.y = 400.0f;
	transform_info.bounds_type = OBS_BOUNDS_SCALE_INNER;
	transform_info.bounds_alignment = OBS_ALIGN_CENTER;
	transform_info.alignment = OBS_ALIGN_CENTER;
	transform_info.scale.x = 2.0f;
	transform_info.scale.y = 2.0f;
	transform_info.rot = 0.0f;
	obs_sceneitem_t *source_sceneitem = get_scene_item_from_source(scene, graph_obs_source);

	if (source_sceneitem != NULL) {
		obs_log(LOG_INFO, "Entering transform info adding part");

		obs_sceneitem_set_info2(source_sceneitem, &transform_info);
		obs_sceneitem_release(source_sceneitem);
	}
	obs_source_release(graph_obs_source);
}

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

void graph_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct graph_source *graphSource = reinterpret_cast<struct graph_source *>(data);
	if (!graphSource || !graphSource->source) {
		return; // Ensure graphSource is valid
	}

	obs_log(LOG_INFO, "Rendering graph source...");

	// Retrieve OBS settings for the heart rate monitor source
	// obs_source_t *heart_rate_source = obs_get_source_by_name("Heart Rate Monitor");
	// if (!heart_rate_source) {
	//     obs_log(LOG_ERROR, "Heart Rate Monitor source not found");
	//     return;
	// }

	obs_data_t *settings = obs_source_get_settings(graphSource->source);
	int curHeartRate = static_cast<int>(obs_data_get_int(settings, "heart rate")); // Retrieve heart rate
	obs_data_release(settings);
	obs_source_release(graphSource->source);

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
	// gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	if (!graph_source->effect) {
		obs_log(LOG_ERROR, "Failed to get solid effect for rendering");
		return;
	}

	while (gs_effect_loop(graph_source->effect, "Solid")) {
		gs_effect_set_color(gs_effect_get_param_by_name(graph_source->effect, "color"), 0xFF0000FF);

		gs_render_start(true); // Use GS_LINESTRIP to connect the points

		obs_log(LOG_INFO, "Drawing heart rate graph...");

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
	obs_data_set_int(settings, "heart rate", 1);
	std::vector<int> buffer;
	graph_src->buffer = buffer;
	obs_log(LOG_INFO, "create graph source info triggered");
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	graph_src->effect = effect;
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
