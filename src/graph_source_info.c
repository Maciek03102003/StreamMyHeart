#include <obs-module.h>
#include "graph_source.h"

struct obs_source_info graph_source_info = {
	.id = "heart_rate_graph",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = get_graph_source_name,
	.create = create_graph_source_info,
	.destroy = destroy_graph_source,
	.activate = graphSourceActivate,
	.deactivate = graphSourceDeactivate,
	.video_render = graph_source_render,
	.get_width = graph_source_info_get_width,
	.get_height = graph_source_info_get_height,
};