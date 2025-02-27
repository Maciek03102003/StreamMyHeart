#include <obs-module.h>
#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <obs-data.h>
#include "plugin-support.h"
#include "obs_utils.h"
#include "heart_rate_source.h"

void removeSource(const char *source_name)
{
	obs_source_t *source = obs_get_source_by_name(source_name);
	if (source) {
		obs_source_t *scene_as_source = obs_frontend_get_current_scene();
		if (scene_as_source) {
			obs_scene_t *scene = obs_scene_from_source(scene_as_source);
			if (scene) {
				obs_sceneitem_t *scene_item = getSceneItemFromSource(scene, source);
				if (scene_item) {
					obs_sceneitem_remove(scene_item);  // Remove from scene
					obs_sceneitem_release(scene_item); // Release scene item reference
				}
			}
			obs_source_release(scene_as_source);
		}
		obs_source_release(source); // Release the source reference
	}
	// obs_log(LOG_INFO, "[remove_source] done removing %s source", source_name);
}

void skipVideoFilterIfSafe(obs_source_t *source)
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

obs_sceneitem_t *getSceneItemFromSource(obs_scene_t *scene, obs_source_t *source)
{
	obs_sceneitem_t *found_item = NULL;
	void *params[2] = {source, &found_item}; // Pass source and output pointer

	// Enumerate scene items and find the one matching the source
	obs_scene_enum_items(scene, find_scene_item_callback, params);

	return found_item;
}