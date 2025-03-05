/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "heart_rate_source_info.h"
#include "plugin-support.h"
#include "graph_source_info.h"

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-GB")

extern struct obs_source_info heartRateSourceInfo;
extern struct obs_source_info graphSourceInfo;
extern struct obs_source_info ecgSourceInfo;

bool obs_module_load(void)
{
	obs_register_source(&heartRateSourceInfo);
	obs_register_source(&graphSourceInfo);
	obs_register_source(&ecgSourceInfo);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
