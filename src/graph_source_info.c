#include <obs-module.h>
#include "graph_source.h"

struct obs_source_info graphSourceInfo = {
	.id = "heart_rate_graph",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = getGraphSourceName,
	.create = createGraphSourceInfo,
	.destroy = destroyGraphSource,
	.activate = graphSourceActivate,
	.deactivate = graphSourceDeactivate,
	.video_render = graphSourceRender,
	.get_width = graphSourceInfoGetWidth,
	.get_height = graphSourceInfoGetHeight,
};