#ifndef HEART_RATE_SOURCE_H
#define HEART_RATE_SOURCE_H

#include <obs-module.h>

#ifdef __cplusplus
#include <mutex>
#else
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TEXT_SOURCE_NAME "Heart Rate Display"

struct input_BGRA_data {
	uint8_t *data;
	uint32_t width;
	uint32_t height;
	uint32_t linesize;
};

struct heart_rate_source {
	obs_source_t *source;
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;
	gs_effect_t *testing;
#ifdef __cplusplus
	// **Frame Buffers**
	std::unique_ptr<input_BGRA_data> BGRA_data;           // Stores the current frame from OBS
	std::unique_ptr<input_BGRA_data> currentFrameData;    // Stores a stable frame copy
	std::unique_ptr<input_BGRA_data> processingFrameData; // Stores a copy for processing

	// **Mutexes for Thread Safety**
	std::mutex BGRA_data_mutex;                           // Mutex to sync BGRA data access
	std::mutex processingMutex;    // Mutex to sync face detection & heart rate processing

	// **Face Detection Data**
	std::vector<struct vec4> face_coordinates; // Stores detected face regions
#else
	void *BGRA_data; // Placeholder for C compatibility
	void *currentFrameData;
	void *processingFrameData;

	void *BGRA_data_mutex; // Placeholder for C compatibility
	void *face_coordinates;
	
	void *processingMutex;
#endif
	double latest_heart_rate; // Stores the most recent heart rate
	bool newDataAvailable; // Flag to indicate new processed data is ready
	bool isDisabled;
};

// Function declarations
const char *get_heart_rate_source_name(void *);
void *heart_rate_source_create(obs_data_t *settings, obs_source_t *source);
void heart_rate_source_destroy(void *data);
obs_properties_t *heart_rate_source_properties(void *data);
void heart_rate_source_activate(void *data);
void heart_rate_source_deactivate(void *data);
void heart_rate_source_tick(void *data, float seconds);
void heart_rate_source_render(void *data, gs_effect_t *effect);

#ifdef __cplusplus
}
#endif

#endif // HEART_RATE_SOURCE_H
