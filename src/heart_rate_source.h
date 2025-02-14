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
	input_BGRA_data *BGRA_data;
	std::mutex BGRA_data_mutex;
#else
	struct input_BGRA_data *BGRA_data;
	void *BGRA_data_mutex; // Placeholder for C compatibility
#endif
	bool isDisabled;
};

// Function declarations
const char *getHeartRateSourceName(void *);
void *heartRateSourceCreate(obs_data_t *settings, obs_source_t *source);
void heartRateSourceDestroy(void *data);
void heartRateSourceDefaults(obs_data_t *settings);
obs_properties_t *heartRateSourceProperties(void *data);
void heartRateSourceActivate(void *data);
void heartRateSourceDeactivate(void *data);
void heartRateSourceTick(void *data, float seconds);
void heartRateSourceRender(void *data, gs_effect_t *effect);

#ifdef __cplusplus
}
#endif

#endif // HEART_RATE_SOURCE_H
