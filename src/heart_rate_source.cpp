#include "algorithm/face_detection/opencv_dlib_68_landmarks_face_tracker.h"
#include "algorithm/face_detection/opencv_haarcascade.h"
#include "algorithm/HeartRateAlgorithm.h"

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

MovingAvg movingAvg;

const char *get_heart_rate_source_name(void *)
{
	return "Heart Rate Monitor";
}

static void skip_video_filter_if_safe(obs_source_t *source)
{
	if (!source) {
		return;
	}

	obs_source_t *parent = obs_filter_get_parent(source);
	if (parent) {
		// obs_log(LOG_INFO, "Source is skipped");
		obs_source_skip_video_filter(source);
	} else {
		obs_log(LOG_INFO, "No valid parent, skipping filter safely");
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

static obs_sceneitem_t *get_scene_item_from_source(obs_scene_t *scene, obs_source_t *source)
{
	obs_sceneitem_t *found_item = NULL;
	void *params[2] = {source, &found_item}; // Pass source and output pointer

	// Enumerate scene items and find the one matching the source
	obs_scene_enum_items(scene, find_scene_item_callback, params);

	return found_item;
}

static void create_text_source(obs_scene_t *scene)
{
	obs_source_t *source = obs_source_create("text_ft2_source_v2", TEXT_SOURCE_NAME, nullptr, nullptr);

	if (source) {
		// add source to the current scene
		obs_scene_add(scene, source);
		// set source settings
		obs_data_t *source_settings = obs_source_get_settings(source);
		obs_data_set_bool(source_settings, "word_wrap", true);
		obs_data_set_bool(source_settings, "extents", true);
		obs_data_set_bool(source_settings, "outline", true);
		obs_data_set_int(source_settings, "outline_color", 4278190080);
		obs_data_set_int(source_settings, "outline_size", 7);
		obs_data_set_int(source_settings, "extents_cx", 1500);
		obs_data_set_int(source_settings, "extents_cy", 230);
		obs_data_t *font_data = obs_data_create();
		obs_data_set_string(font_data, "face", "Arial");
		obs_data_set_string(font_data, "style", "Regular");
		obs_data_set_int(font_data, "size", 72);
		obs_data_set_int(font_data, "flags", 0);
		obs_data_set_obj(source_settings, "font", font_data);
		obs_data_release(font_data);

		std::string heartRateText = "Heart Rate: 0 BPM";
		obs_data_set_string(source_settings, "text", heartRateText.c_str());
		obs_source_update(source, source_settings);
		obs_data_release(source_settings);

		// set transform settings
		obs_transform_info transform_info;
		transform_info.pos.x = 260.0;
		transform_info.pos.y = 1040.0 - 50.0;
		transform_info.bounds.x = 500.0;
		transform_info.bounds.y = 145.0;
		transform_info.bounds_type = obs_bounds_type::OBS_BOUNDS_SCALE_INNER;
		transform_info.bounds_alignment = OBS_ALIGN_CENTER;
		transform_info.alignment = OBS_ALIGN_CENTER;
		transform_info.scale.x = 1.0;
		transform_info.scale.y = 1.0;
		transform_info.rot = 0.0;
		obs_sceneitem_t *source_sceneitem = get_scene_item_from_source(scene, source);
		if (source_sceneitem != NULL) {
			obs_sceneitem_set_info2(source_sceneitem, &transform_info);
			obs_sceneitem_release(source_sceneitem);
		}

		obs_source_release(source);
	}
}

static void add_graph_source_to_scene(obs_source_t *graph_obs_source, obs_scene_t *scene) {
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
static struct graph_source *create_graph_source()
{
	obs_log(LOG_INFO, "create graph source");
    // Allocate graph_source and attach it to heart_rate_source
    struct graph_source *graphrender = (struct graph_source *)bzalloc(sizeof(struct graph_source));
    if (!graphrender) {
        obs_log(LOG_ERROR, "Failed to allocate graph_source.");
        return nullptr;
    }

    // Create OBS source for the graph
    obs_data_t *graph_settings = obs_data_create();
    obs_source_t *graph_obs_source = obs_source_create("user_drawn_graph", "Heart Rate Graph", graph_settings, nullptr);
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

    // // Add the graph to the OBS scene
    // obs_scene_add(scene, graph_obs_source);

    // // Set transform properties
    // obs_transform_info transform_info;
    // transform_info.pos.x = 300.0f;
    // transform_info.pos.y = 600.0f;
    // transform_info.bounds.x = 400.0f;
    // transform_info.bounds.y = 200.0f;
    // transform_info.bounds_type = OBS_BOUNDS_SCALE_INNER;
    // transform_info.bounds_alignment = OBS_ALIGN_CENTER;
    // transform_info.alignment = OBS_ALIGN_CENTER;
    // transform_info.scale.x = 30.0f;
    // transform_info.scale.y = 30.0f;
    // transform_info.rot = 0.0f;

    // obs_sceneitem_t *scene_item = get_scene_item_from_source(scene, graph_obs_source);
    // if (scene_item != NULL) {
    //     obs_sceneitem_set_info2(scene_item, &transform_info);
    //     obs_sceneitem_release(scene_item);
    // }

    // obs_source_release(graph_obs_source);
}


static void create_image_source(obs_scene_t *scene)
{
	// Create the heart rate image source (assuming a file path)
	obs_data_t *image_settings = obs_data_create();
	obs_data_set_string(image_settings, "file", obs_module_file("heart_rate.gif"));
	obs_source_t *image_source = obs_source_create("image_source", IMAGE_SOURCE_NAME, image_settings, nullptr);
	obs_data_release(image_settings);

	// Add image to the group
	obs_scene_add(scene, image_source);

	// set transform settings
	obs_transform_info transform_info;
	transform_info.pos.x = 260.0;
	transform_info.pos.y = 700.0;
	transform_info.bounds.x = 300.0;
	transform_info.bounds.y = 400.0;
	transform_info.bounds_type = obs_bounds_type::OBS_BOUNDS_SCALE_INNER;
	transform_info.bounds_alignment = OBS_ALIGN_CENTER;
	transform_info.alignment = OBS_ALIGN_CENTER;
	transform_info.scale.x = 0.1;
	transform_info.scale.y = 0.1;
	transform_info.rot = 0.0;
	obs_sceneitem_t *source_sceneitem = get_scene_item_from_source(scene, image_source);
	if (source_sceneitem != NULL) {
		obs_sceneitem_set_info2(source_sceneitem, &transform_info);
		obs_sceneitem_release(source_sceneitem);
	}

	obs_source_release(image_source);
}

static void create_obs_heart_display_source_if_needed()
{
	obs_source_t *scene_as_source = obs_frontend_get_current_scene();
	obs_scene_t *scene = obs_scene_from_source(scene_as_source);

	// check if a source called TEXT_SOURCE_NAME exists
	obs_source_t *text_source = obs_get_source_by_name(TEXT_SOURCE_NAME);
	if (text_source) { // source already exists, release it
		obs_source_release(text_source);
		obs_source_release(scene_as_source);
		return;
	} else {
		create_text_source(scene);
	}

	obs_source_t *image_source = obs_get_source_by_name(IMAGE_SOURCE_NAME);
	if (image_source) {
		obs_source_release(image_source);
		obs_source_release(scene_as_source);
		return;
	} else {
		create_image_source(scene);
	}
	obs_source_t *graph_source = obs_get_source_by_name(GRAPH_SOURCE_NAME);
	add_graph_source_to_scene(graph_source, scene);
	// if(graph_source) {
	// 	obs_source_release(graph_source);
	// 	obs_source_release(scene_as_source);
	// } else {
	// 	create_graph_source(scene);
	// }

	obs_source_release(scene_as_source);
}

// Create function
void *heart_rate_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);

	void *data = bmalloc(sizeof(struct heart_rate_source));
	struct heart_rate_source *hrs = new (data) heart_rate_source();

	hrs->source = source;

	char *effect_file;
	obs_enter_graphics();
	effect_file = obs_module_file("test.effect");

	hrs->testing = gs_effect_create_from_file(effect_file, NULL);

	bfree(effect_file);
	if (!hrs->testing) {
		heart_rate_source_destroy(hrs);
		hrs = NULL;
	}
	obs_leave_graphics();

	hrs->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
	hrs->graphrender = create_graph_source();
	create_obs_heart_display_source_if_needed();

	return hrs;
}

static void remove_source(const char *source_name)
{
	obs_source_t *source = obs_get_source_by_name(source_name);
	if (source) {
		obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
		if (scene) {
			obs_sceneitem_t *scene_item = get_scene_item_from_source(scene, source);
			if (scene_item) {
				obs_sceneitem_remove(scene_item);  // Remove from scene
				obs_sceneitem_release(scene_item); // Release scene item reference
			}
		}
		obs_source_release(source); // Release the source reference
	}
}

// Destroy function
void heart_rate_source_destroy(void *data)
{
	obs_log(LOG_INFO, "Heart rate monitor destroyed");
	struct heart_rate_source *hrs = reinterpret_cast<struct heart_rate_source *>(data);

	if (hrs) {
		hrs->isDisabled = true;

		remove_source(TEXT_SOURCE_NAME);
		remove_source(IMAGE_SOURCE_NAME);

		obs_enter_graphics();
		gs_texrender_destroy(hrs->texrender);
		if (hrs->stagesurface) {
			gs_stagesurface_destroy(hrs->stagesurface);
		}
		gs_effect_destroy(hrs->testing);
		obs_leave_graphics();
		hrs->~heart_rate_source();
		bfree(hrs);
	}
}

void heart_rate_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "face detection algorithm", 1);
	obs_data_set_default_bool(settings, "enable face tracking", true);
	obs_data_set_default_int(settings, "frame update interval", 60);
	obs_data_set_default_int(settings, "ppg algorithm", 0);
}

static bool update_properties(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	bool is_dlib_selected = obs_data_get_int(settings, "face detection algorithm") == 1;
	bool is_tracker_enabled = obs_data_get_bool(settings, "enable face tracking");

	obs_property_t *enable_tracker = obs_properties_get(props, "enable face tracking");
	obs_property_t *face_tracking_explain = obs_properties_get(props, "face tracking explain");
	obs_property_t *frame_update_interval = obs_properties_get(props, "frame update interval");
	obs_property_t *frame_update_interval_explain = obs_properties_get(props, "frame update interval explain");

	obs_property_set_visible(enable_tracker, is_dlib_selected);
	obs_property_set_visible(face_tracking_explain, is_dlib_selected);
	obs_property_set_visible(frame_update_interval, is_dlib_selected && is_tracker_enabled);
	obs_property_set_visible(frame_update_interval_explain, is_dlib_selected && is_tracker_enabled);

	return true; // Forces the UI to refresh
}

obs_properties_t *heart_rate_source_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();

	// Allow user to disable face detection boxes drawing
	obs_properties_add_bool(props, "face detection debug boxes",
				obs_module_text("See face detection/tracking result"));

	// Set the face detection algorithm
	obs_property_t *dropdown = obs_properties_add_list(props, "face detection algorithm",
							   obs_module_text("Face Detection Algorithm:"),
							   OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(dropdown, "OpenCV Haarcascade Face Detection", 0);
	obs_property_list_add_int(dropdown, "Dlib Face Detection", 1);

	// Set if enable face tracking
	obs_property_t *enable_tracker =
		obs_properties_add_bool(props, "enable face tracking", obs_module_text("Enable Face Tracker"));
	obs_properties_add_text(props, "face tracking explain",
				obs_module_text("Enabling face tracking speeds up video processing."), OBS_TEXT_INFO);

	obs_properties_add_int(props, "frame update interval", obs_module_text("Frame Update Interval"), 1, 120, 1);
	obs_properties_add_text(props, "frame update interval explain",
				obs_module_text("Set how often the face detector updates the face position in frames."),
				OBS_TEXT_INFO);

	// Add dropdown for selecting PPG algorithm
	obs_property_t *ppg_dropdown = obs_properties_add_list(
		props, "ppg algorithm", obs_module_text("PPG Algorithm:"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(ppg_dropdown, "Green Channel", 0);
	obs_property_list_add_int(ppg_dropdown, "PCA", 1);
	obs_property_list_add_int(ppg_dropdown, "Chrom", 2);

	obs_data_t *settings = obs_source_get_settings((obs_source_t *)data);
	obs_property_set_modified_callback(dropdown, update_properties);
	obs_property_set_modified_callback(enable_tracker, update_properties);
	obs_property_set_modified_callback(ppg_dropdown, update_properties);
	update_properties(props, dropdown, settings); // Apply default visibility
	obs_data_release(settings);

	return props;
}

void heart_rate_source_activate(void *data)
{
	obs_log(LOG_INFO, "Heart rate monitor activated");
	struct heart_rate_source *hrs = reinterpret_cast<heart_rate_source *>(data);
	hrs->isDisabled = false;
}

void heart_rate_source_deactivate(void *data)
{
	obs_log(LOG_INFO, "Heart rate monitor deactivated");
	struct heart_rate_source *hrs = reinterpret_cast<heart_rate_source *>(data);
	hrs->isDisabled = true;
}

// Tick function
void heart_rate_source_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct heart_rate_source *hrs = reinterpret_cast<struct heart_rate_source *>(data);

	if (hrs->isDisabled) {
		return;
	}

	if (!obs_source_enabled(hrs->source)) {
		return;
	}
}

static bool getBGRAFromStageSurface(struct heart_rate_source *hrs)
{
	uint32_t width;
	uint32_t height;

	// Check if the source is enabled
	if (!obs_source_enabled(hrs->source)) {
		return false;
	}

	// Retrieve the target source of the filter
	obs_source_t *target;
	if (hrs->source) {
		target = obs_filter_get_target(hrs->source);
	} else {
		return false;
	}
	if (!target) {
		return false;
	}

	// Retrieve the base dimensions of the target source
	width = obs_source_get_base_width(target);
	height = obs_source_get_base_height(target);
	if (width == 0 || height == 0) {
		return false;
	}

	// Resets the texture renderer and begins rendering with the specified width and height
	gs_texrender_reset(hrs->texrender);
	if (!hrs->texrender) {
		return false;
	}
	if (!gs_texrender_begin(hrs->texrender, width, height)) {
		return false;
	}

	// Clear up and set up rendering
	// - Clears the rendering surface with a zeroed background
	// - Sets up the orthographic projection matrix
	// - Pushes the blend state and sets the blend function
	// - Renders the video of the target source
	// - Pops the blend state and ends the texture rendering

	// Declare a vec4 structure to hold the background colour
	struct vec4 background; // two component vector struct (x, y, z, w)

	// Set the background colour to zero (black)
	vec4_zero(&background); // zeroes a vector

	// Clear the rendering surface from the previous frame processing, with the specified background colour. The GS_CLEAR_COLOR flag indicates that the colour buffer should be cleared. This sets the colour to &background colour
	gs_clear(GS_CLEAR_COLOR, &background, 0.0f,
		 0); // Clears color/depth/stencil buffers

	// Sets up an orthographic projection matrix. This matrix defines a 2D rendering space where objects are rendered without perspective distortion
	// Parameters:
	// - 0.0f: The left coordinate of the projection
	// static_cast<float>(width): The rigCan youht coordinate of the projection, set to the width of the target source
	// 0.0f: The bottom coordinate of the projection.
	// static_cast<float>(height): The top coordinate of the projection, set to the height of the target source
	// -100.0f: The near clipping plane. The near clipping plane is the closest plane to the camera. Objects closer to the camera than this plane are clipped (not rendered). It helps to avoid rendering artifacts and improves depth precision by discarding objects that are too close to the camera
	// 100.0f: The far clipping plane. The far clipping plane is the farthest plane from the camera. Objects farther from the camera than this plane are clipped (not rendered).It helps to limit the rendering distance and manage depth buffer precision by discarding objects that are too far away
	gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);

	// This function saves the current blend state onto a stack. The blend state includes settings that control how colors from different sources are combined during rendering. By pushing the current blend state, you can make temporary changes to the blend settings and later restore the original settings by popping the blend state from the stack
	gs_blend_state_push();

	// This function sets the blend function for rendering. The blend function determines how the source (the new pixels being drawn) and the destination (the existing pixels in the framebuffer) colors are combined
	// Parameters:
	// - GS_BLEND_ONE: The source color is used as-is. This means the source color is multiplied by 1
	// - GS_BLEND_ZERO: The destination color is ignored. This means the destination color is multiplied by 0
	// Effect: The combination of GS_BLEND_ONE and GS_BLEND_ZERO results in the source color completely replacing the destination color. This is equivalent to disabling blending, where the new pixels overwrite the existing pixels
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	// This function renders the video frame of the target source onto the current rendering surface
	obs_source_video_render(target);

	// This function restores the previous blend state from the stack. The blend state that was saved by gs_blend_state_push is now restored. By popping the blend state, you undo the temporary changes made to the blend settings, ensuring that subsequent rendering operations use the original blend settings
	gs_blend_state_pop();

	// This function ends the texture rendering process. It finalizes the rendering operations and makes the rendered texture available for further processing. This function completes the rendering process, ensuring that the rendered texture is properly finalised and can be used for subsequent operations, such as extracting pixel data or further processing
	gs_texrender_end(hrs->texrender);

	// Retrieve the old existing stage surface
	if (hrs->stagesurface) {
		uint32_t stagesurf_width = gs_stagesurface_get_width(hrs->stagesurface);
		uint32_t stagesurf_height = gs_stagesurface_get_height(hrs->stagesurface);
		// If it still matches the new width and height, reuse it
		if (stagesurf_width != width || stagesurf_height != height) {
			// Destroy the old stage surface
			gs_stagesurface_destroy(hrs->stagesurface);
			hrs->stagesurface = nullptr;
		}
	}

	// Create a new stage surface if necessary
	if (!hrs->stagesurface) {
		hrs->stagesurface = gs_stagesurface_create(width, height, GS_BGRA);
	}

	// Use gs_stage_texture to stage the texture from the texture renderer (hrs->texrender) to the stage surface (hrs->stagesurface). This operation transfers the rendered texture to the stage surface for further processing
	gs_stage_texture(hrs->stagesurface, gs_texrender_get_texture(hrs->texrender));

	// Use gs_stagesurface_map to map the stage surface and retrieve the video data and line size. The video_data pointer will point to the BGRA data, and linesize will indicate the number of bytes per line
	uint8_t *video_data; // A pointer to the memory location where the BGRA data will be accessible
	uint32_t linesize;   // The number of bytes per line (or row) of the image data
	// The gs_stagesurface_map function creates a mapping between the GPU memory and the CPU memory. This allows the CPU to access the pixel data directly from the GPU memory
	if (!gs_stagesurface_map(hrs->stagesurface, &video_data, &linesize)) {
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(hrs->BGRA_data_mutex);
		struct input_BGRA_data *BGRA_data = (struct input_BGRA_data *)bzalloc(sizeof(struct input_BGRA_data));
		BGRA_data->width = width;
		BGRA_data->height = height;
		BGRA_data->linesize = linesize;
		BGRA_data->data = video_data;
		if (hrs->BGRA_data) {
			bfree(hrs->BGRA_data->data);
			bfree(hrs->BGRA_data);
		}
		hrs->BGRA_data = BGRA_data;
	}

	// Use gs_stagesurface_unmap to unmap the stage surface, releasing the mapped memory.
	gs_stagesurface_unmap(hrs->stagesurface);
	return true;
}

static gs_texture_t *draw_rectangle(struct heart_rate_source *hrs, uint32_t width, uint32_t height,
				    std::vector<struct vec4> &face_coordinates)
{
	uint64_t rect_before = os_gettime_ns();
	gs_texture_t *blurredTexture = gs_texture_create(width, height, GS_BGRA, 1, nullptr, 0);
	gs_copy_texture(blurredTexture, gs_texrender_get_texture(hrs->texrender));

	gs_texrender_reset(hrs->texrender);
	if (!gs_texrender_begin(hrs->texrender, width, height)) {
		obs_log(LOG_INFO, "Could not open background blur texrender!");
		return blurredTexture;
	}

	gs_effect_set_texture(gs_effect_get_param_by_name(hrs->testing, "image"), blurredTexture);

	std::vector<std::string> params = {"face", "eye_1", "eye_2", "mouth", "detected"};

	for (size_t i = 0; i < std::min(params.size(), face_coordinates.size()); i++) {
		gs_effect_set_vec4(gs_effect_get_param_by_name(hrs->testing, params[static_cast<int>(i)].c_str()),
				   &face_coordinates[i]);
	}

	struct vec4 background;
	vec4_zero(&background);
	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
	gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	gs_blend_state_pop();
	gs_texrender_end(hrs->texrender);
	gs_copy_texture(blurredTexture, gs_texrender_get_texture(hrs->texrender));
	uint64_t rect_after = os_gettime_ns();
	// obs_log(LOG_INFO, "Rect time: %lu ns", rect_after - rect_before);
	return blurredTexture;
}

static void draw_graph(struct graph_source *graphrender, int curHeartRate) {
	obs_log(LOG_INFO, "checking for null graph source");
    if (!graphrender) return;  // Null check to avoid crashes
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

	while(gs_effect_loop(graphrender->effect, "Solid")) {
		gs_effect_set_color(gs_effect_get_param_by_name(graphrender->effect, "color"),
			graphrender->color);

		gs_render_start(GS_LINESTRIP); // Use GS_LINESTRIP to connect the points

		obs_log(LOG_INFO, "Drawing heart rate graph...");
		
		for (size_t i = 0; i < graphrender->buffer.size(); i++) {
				gs_vertex2f(static_cast<float>(i * 10), static_cast<float>(graphrender->buffer[i]));
		}

		gs_render_stop(GS_LINESTRIP);
	}

    // if (!gs_effect_loop(effect, "Solid")) {
    //     obs_log(LOG_ERROR, "gs_effect_loop failed");
    //     return;
    // }

    // gs_render_start(GS_LINESTRIP); // Use GS_LINESTRIP to connect the points

    // obs_log(LOG_INFO, "Drawing heart rate graph...");
    
    // for (size_t i = 0; i < heartRateBuffer.size(); i++) {
    //     gs_vertex2f(static_cast<float>(i * 10), static_cast<float>(heartRateBuffer[i]));
    // }

    // gs_render_stop(GS_LINESTRIP);
}


// Render function
void heart_rate_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct heart_rate_source *hrs = reinterpret_cast<struct heart_rate_source *>(data);

	if (!hrs->source) {
		return;
	}

	if (hrs->isDisabled) {
		skip_video_filter_if_safe(hrs->source);
		return;
	}

	if (!getBGRAFromStageSurface(hrs)) {
		skip_video_filter_if_safe(hrs->source);
		return;
	}

	if (!hrs->testing) {
		obs_log(LOG_INFO, "Effect not loaded");
		// Effect failed to load, skip rendering
		skip_video_filter_if_safe(hrs->source);
		return;
	}

	obs_data_t *settings = obs_source_get_settings(hrs->source);
	std::vector<struct vec4> face_coordinates;
	int64_t selected_algorithm = obs_data_get_int(settings, "face detection algorithm");
	// obs_log(LOG_INFO, "Which algo: %d", selected_algorithm);
	bool enable_debug_boxes = obs_data_get_bool(settings, "face detection debug boxes");
	std::vector<double_t> avg;
	if (selected_algorithm == 0) {
		// Haarcascade face detection with OpenCV
		avg = detectFacesAndCreateMask(hrs->BGRA_data, face_coordinates, enable_debug_boxes);
	} else if (selected_algorithm == 1) {
		// Dlib face detection with 68 landmarks, with/without tracking
		bool enable_tracker = obs_data_get_bool(settings, "enable face tracking");
		int64_t frame_update_interval = obs_data_get_int(settings, "frame update interval");
		avg = detectFaceAOI(hrs->BGRA_data, face_coordinates, frame_update_interval, enable_tracker,
				    enable_debug_boxes);
	}
	obs_data_release(settings);

	// Get the selected PPG algorithm
	int64_t selected_ppg_algorithm = obs_data_get_int(obs_source_get_settings(hrs->source), "ppg algorithm");

	double heart_rate = movingAvg.calculateHeartRate(avg, 0, selected_ppg_algorithm, 0);
	std::string result = "Heart Rate: " + std::to_string((int)heart_rate);

	if (heart_rate != 0.0) {
		obs_source_t *source = obs_get_source_by_name(TEXT_SOURCE_NAME);
		if (source) {
			obs_log(LOG_INFO, "Updating text source");
			obs_data_t *source_settings = obs_source_get_settings(source);
			obs_data_set_string(source_settings, "text", result.c_str());
			obs_source_update(source, source_settings);
			obs_data_release(source_settings);
			obs_source_release(source);
		}
	}
	obs_log(LOG_INFO, "Heart rate: %lf", heart_rate);

	
	draw_graph(hrs->graphrender, heart_rate);
	obs_log(LOG_INFO, "After draw graph!");

	// if (enable_debug_boxes) {
	// 	gs_texture_t *testingTexture =
	// 		draw_rectangle(hrs, hrs->BGRA_data->width, hrs->BGRA_data->height, face_coordinates);

	// 	if (!obs_source_process_filter_begin(hrs->source, GS_BGRA, OBS_ALLOW_DIRECT_RENDERING)) {
	// 		skip_video_filter_if_safe(hrs->source);
	// 		gs_texture_destroy(testingTexture);
	// 		return;
	// 	}
	// 	gs_effect_set_texture(gs_effect_get_param_by_name(hrs->testing, "image"), testingTexture);

	// 	gs_blend_state_push();
	// 	gs_reset_blend_state();

	// 	if (hrs->source) {
	// 		obs_source_process_filter_tech_end(hrs->source, hrs->testing, hrs->BGRA_data->width,
	// 						   hrs->BGRA_data->height, "Draw");
	// 	}

	// 	gs_blend_state_pop();

	// 	gs_texture_destroy(testingTexture);

	// } else {
	// 	skip_video_filter_if_safe(hrs->source);
	// }
	// obs_log(LOG_INFO, "FINISH RENDERING");
}
