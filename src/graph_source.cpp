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

const char *get_graph_source_name(void *)
{
    return "user_drawn_graph";
}


void *create_graph_source(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(source);
	obs_log(LOG_INFO, "create graph source");
	// Allocate graph_source and attach it to heart_rate_source
	struct graph_source *graphrender = (struct graph_source *)bzalloc(sizeof(struct graph_source));
	if (!graphrender) {
		obs_log(LOG_ERROR, "Failed to allocate graph_source.");
		return nullptr;
	}

	// Create OBS source for the graph
	obs_data_t *graph_settings = obs_data_create();
	obs_source_t *graph_obs_source =
		obs_source_create("user_drawn_graph", GRAPH_SOURCE_NAME, graph_settings, nullptr);
	obs_data_release(graph_settings);

	if (!graph_obs_source) {
		obs_log(LOG_ERROR, "Failed to create graph source.");
		bfree(graphrender);
		graphrender = nullptr;
		return nullptr;
	}

	graphrender->source = graph_obs_source;
	graphrender->vertex_buffer = gs_vertexbuffer_create(nullptr, GS_DYNAMIC); // Create a dynamic vertex buffer
	graphrender->color = 0xFF0000FF;
	graphrender->effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	return graphrender;
}

// Destroy function for graph source
void destroy_graph_source(void *data)
{
	obs_log(LOG_INFO, "Graph source destroyed");
	struct graph_source *graph = reinterpret_cast<struct graph_source *>(data);

	if (graph) {
		obs_enter_graphics();

		// Destroy vertex buffer
		if (graph->vertex_buffer) {
			gs_vertexbuffer_destroy(graph->vertex_buffer);
			graph->vertex_buffer = nullptr;
		}

		// Destroy effect
		if (graph->effect) {
			gs_effect_destroy(graph->effect);
			graph->effect = nullptr;
		}

		// Release the OBS source
		if (graph->source) {
			obs_source_release(graph->source);
			graph->source = nullptr;
		}

		obs_leave_graphics();

		// Clear buffer (only applicable in C++)
#ifdef __cplusplus
		graph->buffer.clear();
#endif

		// Free memory
		bfree(graph);
	}
}


void graph_source_render(void *data, gs_effect_t *effect) {
    UNUSED_PARAMETER(effect);

    struct graph_source *graphSource = reinterpret_cast<struct graph_source *>(data);
    if (!graphSource || !graphSource->source) {
        return;  // Ensure graphSource is valid
    }

    obs_log(LOG_INFO, "Rendering graph source...");

    // Retrieve OBS settings for the heart rate monitor source
    obs_source_t *heart_rate_source = obs_get_source_by_name("Heart Rate Monitor");
    if (!heart_rate_source) {
        obs_log(LOG_ERROR, "Heart Rate Monitor source not found");
        return;
    }

    obs_data_t *settings = obs_source_get_settings(heart_rate_source);
    int curHeartRate = static_cast<int>(obs_data_get_int(settings, "heart rate"));  // Retrieve heart rate
    obs_data_release(settings);
    obs_source_release(heart_rate_source);

    obs_log(LOG_INFO, "Current heart rate: %d", curHeartRate);

    // Draw the graph using the retrieved heart rate
    draw_graph(graphSource, curHeartRate);

    obs_log(LOG_INFO, "Graph rendering completed!");
}


void draw_graph(struct graph_source *graphrender, int curHeartRate)
{
	obs_log(LOG_INFO, "checking for null graph source");
	
	if (!graphrender)
		return; // Null check to avoid crashes
	graphrender->buffer.push_back(60);

	// Maintain a buffer size of 10
	while (graphrender->buffer.size() >= 10) {
		graphrender->buffer.erase(graphrender->buffer.begin());
	}
	graphrender->buffer.push_back(curHeartRate);

	obs_log(LOG_INFO, "Updating heart rate buffer...");

	// Start rendering
	// gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	if (!graphrender->effect) {
		obs_log(LOG_ERROR, "Failed to get solid effect for rendering");
		return;
	}

	while (gs_effect_loop(graphrender->effect, "Solid")) {
		gs_effect_set_color(gs_effect_get_param_by_name(graphrender->effect, "color"), graphrender->color);

		gs_render_start(false); // Use GS_LINESTRIP to connect the points

		obs_log(LOG_INFO, "Drawing heart rate graph...");

		for (size_t i = 0; i < graphrender->buffer.size(); i++) {
			gs_vertex2f(static_cast<float>(i * 10), static_cast<float>(graphrender->buffer[i]));
		}

		gs_render_stop(GS_LINESTRIP);
	}
}
