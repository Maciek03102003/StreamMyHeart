#include "heart_rate_source.h"

struct obs_source_info heartRateSourceInfo = {
	.id = "heart_rate_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = getHeartRateSourceName,
	.create = heartRateSourceCreate,
	.destroy = heartRateSourceDestroy,
	.activate = heartRateSourceActivate,
	.deactivate = heartRateSourceDeactivate,
	.get_defaults = heartRateSourceDefaults,
	.get_properties = heartRateSourceProperties,
	.video_tick = heartRateSourceTick,
	.video_render = heartRateSourceRender,
};
