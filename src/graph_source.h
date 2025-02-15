#ifndef GRAPH_SOURCE_H
#define GRAPH_SOURCE_H

#include <obs-module.h>

#ifdef __cplusplus
#include <mutex>
#else
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct graph_source {
	obs_source_t *source;
	// 	gs_vertbuffer_t *vertex_buffer;
	gs_effect_t *effect;
// 	uint32_t color;
#ifdef __cplusplus
	std::vector<int> buffer;
#endif
};

const char *get_graph_source_name(void *);
void *create_graph_source_info(obs_data_t *settings, obs_source_t *source);
void destroy_graph_source_info(void *data);
void graph_source_render(void *data, gs_effect_t *effect);
uint32_t graph_source_info_get_width(void *data);
uint32_t graph_source_info_get_height(void *data);

void draw_graph(struct graph_source *source, int curHeartRate);

#ifdef __cplusplus
}
#endif

#endif