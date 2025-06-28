#include <obs-module.h>

// Structs
struct hero_watcher_data {
    // OBS Plugin API Members
    obs_source_t *context;
	gs_effect_t *effect;
    int width;
	int height;

    // Actual Plugin Setting Members
    //// Crop
	int left;
	int right;
	int top;
	int bottom;

    /// Other
    int refresh_seconds;
    bool preview;

};

// Functions
static const char *hero_watcher_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("HeroWatcher");
}

static void *hero_watcher_create(obs_data_t *settings, obs_source_t *context)
{
	struct hero_watcher_data *filter = bzalloc(sizeof(*filter));
	char *effect_path = obs_module_file("crop_filter.effect");
	blog(LOG_INFO, "Trying to load effect from: %s", effect_path);
	filter->context = context;

	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	obs_leave_graphics();
	bfree(effect_path);

	if (!filter->effect) {
		bfree(filter);
		return NULL;
	}

	obs_source_t *target = obs_filter_get_target(filter->context);
	filter->width = obs_source_get_base_width(target);
	filter->height = obs_source_get_base_height(target);

	obs_source_update(context, settings);
	return filter;
}

static void hero_watcher_destroy(void *data)
{
	struct hero_watcher_data *filter = data;

	obs_enter_graphics();
	gs_effect_destroy(filter->effect);
	obs_leave_graphics();

	bfree(filter);
}

static uint32_t hero_watcher_width(void *data)
{
	struct hero_watcher_data *crop = data;
	return (uint32_t)crop->width;
}

static uint32_t hero_watcher_height(void *data)
{
	struct hero_watcher_data *crop = data;
	return (uint32_t)crop->height;
}

static void hero_watcher_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct hero_watcher_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);

	if (!target || !filter->effect){
		return;
	}

	if (obs_source_process_filter_begin(filter->context, GS_RGBA, OBS_NO_DIRECT_RENDERING)) {
		obs_source_process_filter_end(filter->context, filter->effect, filter->width, filter->height);
	}
}

struct obs_source_info hero_watcher = {
	.id = "hero_watcher",
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_VIDEO,
    .get_name = hero_watcher_get_name,
    .create = hero_watcher_create,
	.destroy = hero_watcher_destroy,
    .get_width = hero_watcher_width,
	.get_height = hero_watcher_height,
    .video_render = hero_watcher_render
};