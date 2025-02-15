#include <obs-module.h>
#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <obs-data.h>
#include "plugin-support.h"
#include "obs_utils.h"
#include "heart_rate_source.h"

void remove_source(const char *source_name)
{
	obs_source_t *source = obs_get_source_by_name(source_name);
	if (source) {
		obs_log(LOG_INFO, "[remove_source] found text source");
		obs_source_t *scene_as_source = obs_frontend_get_current_scene();
		if (scene_as_source) {
			obs_log(LOG_INFO, "[remove_source] found scene source");
			obs_scene_t *scene = obs_scene_from_source(scene_as_source);
			if (scene) {
				obs_log(LOG_INFO, "[remove_source] found scene");
				obs_sceneitem_t *scene_item = get_scene_item_from_source(scene, source);
				if (scene_item) {
					obs_log(LOG_INFO, "[remove_source] found scene item");
					obs_sceneitem_remove(scene_item);  // Remove from scene
					obs_sceneitem_release(scene_item); // Release scene item reference
				}
			}
			obs_source_release(scene_as_source);
		}
		obs_source_release(source); // Release the source reference
	}
	obs_log(LOG_INFO, "[remove_source] done removing text source");
}

void skip_video_filter_if_safe(obs_source_t *source)
{
	if (!source) {
		return;
	}

	obs_source_t *parent = obs_filter_get_parent(source);
	if (parent) {
		obs_source_skip_video_filter(source);
	}
}

// Callback function to find the matching scene item
static bool find_scene_item_callback(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);
	obs_source_t *target_source = (obs_source_t *)((void **)param)[0];      // First element is target source
	obs_sceneitem_t **found_item = (obs_sceneitem_t **)((void **)param)[1]; // Second element is result pointer

	obs_source_t *item_source = obs_sceneitem_get_source(item);

	if (item_source == target_source) {
		// Add a reference to the scene item to ensure it doesn't get released
		obs_sceneitem_addref(item);
		*found_item = item; // Store the found scene item
		return false;       // Stop enumeration since we found the item
	}

	return true; // Continue enumeration
}

obs_sceneitem_t *get_scene_item_from_source(obs_scene_t *scene, obs_source_t *source)
{
	obs_sceneitem_t *found_item = NULL;
	void *params[2] = {source, &found_item}; // Pass source and output pointer

	// Enumerate scene items and find the one matching the source
	obs_scene_enum_items(scene, find_scene_item_callback, params);

	return found_item;
}