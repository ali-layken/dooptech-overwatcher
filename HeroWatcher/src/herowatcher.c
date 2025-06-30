#include <obs-module.h>

// Structs
struct hero_watcher_data {
    // OBS Plugin API Members
    obs_source_t *context;
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
	filter->context = context;

	obs_source_update(context, settings);

	obs_source_t *target = obs_filter_get_target(filter->context);
	filter->width = obs_source_get_base_width(target);
	filter->height = obs_source_get_base_height(target);

	return filter;
}

static void hero_watcher_destroy(void *data)
{
	struct hero_watcher_data *filter = data;
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
	obs_source_skip_video_filter(filter->context);
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