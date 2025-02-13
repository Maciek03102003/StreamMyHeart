#ifndef HEART_RATE_SOURCE_H
#define HEART_RATE_SOURCE_H

#include <obs-module.h>

#ifdef __cplusplus
#include <mutex>
#include <vector>
#else
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TEXT_SOURCE_NAME "Heart Rate Display"
#define IMAGE_SOURCE_NAME "Heart Rate Icon"
#define GRAPH_SOURCE_NAME "Heart Rate Graph"

struct graph_source {
	obs_source_t *source;
	// 	gs_vertbuffer_t *vertex_buffer;
	gs_effect_t *effect;
// 	uint32_t color;
#ifdef __cplusplus
	std::vector<int> buffer;
#endif
};

struct input_BGRA_data {
	uint8_t *data;
	uint32_t width;
	uint32_t height;
	uint32_t linesize;
};

struct heart_rate_source {
	obs_source_t *source;
	obs_source_t *graph_source;
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;
	gs_effect_t *testing;
	gs_effect_t *graph_effect;
#ifdef __cplusplus
	input_BGRA_data *BGRA_data;
	std::mutex BGRA_data_mutex;
	std::vector<int> buffer;
#else
	struct input_BGRA_data *BGRA_data;
	void *BGRA_data_mutex; // Placeholder for C compatibility
#endif
	bool isDisabled;
};

// Function declarations
const char *get_heart_rate_source_name(void *);
void *heart_rate_source_create(obs_data_t *settings, obs_source_t *source);
void heart_rate_source_destroy(void *data);
void heart_rate_source_defaults(obs_data_t *settings);
obs_properties_t *heart_rate_source_properties(void *data);
void heart_rate_source_activate(void *data);
void heart_rate_source_deactivate(void *data);
void heart_rate_source_tick(void *data, float seconds);
void heart_rate_source_render(void *data, gs_effect_t *effect);
// void create_graph_source(obs_scene_t *scene);
void create_graph_source(obs_scene_t *scene, obs_source_t *parent_source);
void skip_video_filter_if_safe(obs_source_t *source);

const char *get_graph_source_name(void *);
void *create_graph_source_info(obs_data_t *settings, obs_source_t *source);
void destroy_graph_source_info(void *data);
void graph_source_render(void *data, gs_effect_t *effect);
uint32_t graph_source_info_get_width(void *data);
uint32_t graph_source_info_get_height(void *data);

void draw_graph(struct graph_source *source, int curHeartRate);

obs_sceneitem_t *get_scene_item_from_source(obs_scene_t *scene, obs_source_t *source);
// void add_graph_source_to_scene(obs_source_t *graph_obs_source, obs_scene_t *scene);

#ifdef __cplusplus
}
#endif

#endif // HEART_RATE_SOURCE_H
