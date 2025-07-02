#ifndef HEROWATCHER_PLUGIN_H
#define HEROWATCHER_PLUGIN_H

#include <obs-module.h>
#include <util/threading.h>

struct hero_watcher_data {
    // OBS Plugin API Members
    obs_source_t *context;
	gs_effect_t *effect;
    int width;
	int height;

	//// OBS Shader Parameters
	gs_eparam_t *param_mul;
	gs_eparam_t *param_add;
	gs_eparam_t *param_multiplier;
	struct vec2 mul_val;
	struct vec2 add_val;

    // Actual Plugin Setting Members
    //// Crop
	int left;
	int right;
	int top;
	int bottom;
	bool preview;


    //// Detection
    int refresh_seconds;
	float remaining_time;
	bool tagging;
	bool hero_detection_running;
    pthread_t hero_thread;

};

void *hero_detection_thread(void *data);

#endif 