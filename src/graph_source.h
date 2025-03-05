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
#ifdef __cplusplus
	std::vector<int> buffer;
#endif
	bool isDisabled;
	bool ecg;
};

const char *getGraphSourceName(void *);
void *createGraphSourceInfo(obs_data_t *settings, obs_source_t *source);
void destroyGraphSource(void *data);
void graphSourceRender(void *data, gs_effect_t *effect);
uint32_t graphSourceInfoGetWidth(void *data);
uint32_t graphSourceInfoGetHeight(void *data);
void graphSourceActivate(void *data);
void graphSourceDeactivate(void *data);

void drawGraph(struct graph_source *source, int curHeartRate, bool ecg);

const char *getECGSourceName(void *);
void *createECGSourceInfo(obs_data_t *settings, obs_source_t *source);
uint32_t ecgSourceInfoGetWidth(void *data);
float getDeltaTime();

#ifdef __cplusplus
}

std::vector<float> generate_ecg_waveform(int heartRate, int width);

#endif

#endif