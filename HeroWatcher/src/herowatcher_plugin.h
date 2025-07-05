#ifndef HEROWATCHER_PLUGIN_H
#define HEROWATCHER_PLUGIN_H

#include <obs-module.h>
#include <util/threading.h>
#include <graphics/color.h> 
#include <obs.h>

const enum gs_color_space preferred_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
};

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

	//gs_color_space source_space;
	//float multiplier;
	//char *technique;
	gs_color_format format;
};

const char *get_tech_name_and_multiplier(enum gs_color_space current_space, enum gs_color_space source_space,
	float *multiplier);

#endif 