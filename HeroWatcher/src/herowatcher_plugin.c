#include "herowatcher_plugin.h"
#include "herowatcher_detector.h"


static const char *hero_watcher_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("HeroWatcher");
}

static void *hero_watcher_create(obs_data_t *settings, obs_source_t *context)
{
	// Setup Filter
	struct hero_watcher_data *filter = bzalloc(sizeof(*filter));
	filter->context = context;

	// Load Crop Effect Shader
	char *effect_path = obs_module_file("crop_filter.effect");
	blog(LOG_DEBUG, "[%s] Loading crop effect from: %s", __func__, effect_path);
	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	obs_leave_graphics();
	bfree(effect_path);
	if (!filter->effect) {
		blog(LOG_ERROR, "[%s] Crop effect not loaded properly.", __func__);
		bfree(filter);
		return NULL;
	}

	// Link Filter to Shader
	filter->param_mul = gs_effect_get_param_by_name(filter->effect, "mul_val");
	filter->param_add = gs_effect_get_param_by_name(filter->effect, "add_val");
	filter->param_multiplier = gs_effect_get_param_by_name(filter->effect, "multiplier");

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
	struct hero_watcher_data *filter = data;
	if (!filter->preview)
		return obs_source_get_base_width(obs_filter_get_target(filter->context));
	return (uint32_t)filter->width;
}

static uint32_t hero_watcher_height(void *data)
{
	struct hero_watcher_data *filter = data;
	if (!filter->preview)
		return obs_source_get_base_height(obs_filter_get_target(filter->context));
	return (uint32_t)filter->height;
}

const char *get_tech_name_and_multiplier(enum gs_color_space current_space, enum gs_color_space source_space,
						float *multiplier)
{
	const char *tech_name = "Draw";
	*multiplier = 1.f;

	switch (source_space) {
	case GS_CS_SRGB:
	case GS_CS_SRGB_16F:
		if (current_space == GS_CS_709_SCRGB) {
			tech_name = "DrawMultiply";
			*multiplier = obs_get_video_sdr_white_level() / 80.0f;
		}
		break;
	case GS_CS_709_EXTENDED:
		switch (current_space) {
		case GS_CS_SRGB:
		case GS_CS_SRGB_16F:
			tech_name = "DrawTonemap";
			break;
		case GS_CS_709_SCRGB:
			tech_name = "DrawMultiply";
			*multiplier = obs_get_video_sdr_white_level() / 80.0f;
			break;
		case GS_CS_709_EXTENDED:
			break;
		}
		break;
	case GS_CS_709_SCRGB:
		switch (current_space) {
		case GS_CS_SRGB:
		case GS_CS_SRGB_16F:
			tech_name = "DrawMultiplyTonemap";
			*multiplier = 80.0f / obs_get_video_sdr_white_level();
			break;
		case GS_CS_709_EXTENDED:
			tech_name = "DrawMultiply";
			*multiplier = 80.0f / obs_get_video_sdr_white_level();
			break;
		case GS_CS_709_SCRGB:
			break;
		}
	}

	return tech_name;
}

static void hero_watcher_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct hero_watcher_data *filter = data;

	const enum gs_color_space source_space = obs_source_get_color_space(
	obs_filter_get_target(filter->context), OBS_COUNTOF(preferred_spaces), preferred_spaces);
	float multiplier;
	const char *technique = get_tech_name_and_multiplier(gs_get_color_space(), source_space, &multiplier);
	filter->format = gs_get_format_from_space(source_space);

	if (!filter->preview || !filter->effect) {
		obs_source_skip_video_filter(filter->context);
		return;
	}
	

	if (obs_source_process_filter_begin_with_color_space(filter->context, format, source_space,
							     OBS_NO_DIRECT_RENDERING)) {
		gs_effect_set_vec2(filter->param_mul, &filter->mul_val);
		gs_effect_set_vec2(filter->param_add, &filter->add_val);
		gs_effect_set_float(filter->param_multiplier, multiplier);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

		obs_source_process_filter_tech_end(filter->context, filter->effect, filter->width, filter->height,
						   technique);

		gs_blend_state_pop();
	}
}

static void hero_watcher_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "preview_weapon", false);
	obs_data_set_default_bool(settings, "tagging_enabled", false);
	obs_data_set_default_int(settings, "refresh_seconds", 30);
}

static bool preview_weapon_enabled(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);

    bool preview_weapon = obs_data_get_bool(settings, "preview_weapon");

    obs_property_set_visible(obs_properties_get(props, "crop_group"), preview_weapon);

    return true;
}

static obs_properties_t *hero_watcher_properties(void *data)
{
	UNUSED_PARAMETER(data);

	// Create GUI
	obs_properties_t *props = obs_properties_create();

	// Crop Settings
	obs_property_t *p2 = obs_properties_add_bool(props, "preview_weapon", obs_module_text("PreviewCrop"));
	obs_property_set_modified_callback(p2, preview_weapon_enabled);

	obs_properties_t *crop_group_props = obs_properties_create();
	obs_properties_add_group(props, "crop_group", obs_module_text("CropGroup"), OBS_GROUP_NORMAL, crop_group_props);
	obs_properties_add_int(crop_group_props, "left", obs_module_text("CropLeft"), -8192, 8192, 1);
	obs_properties_add_int(crop_group_props, "top", obs_module_text("CropTop"), -8192, 8192, 1);
	obs_properties_add_int(crop_group_props, "right", obs_module_text("CropRight"), -8192, 8192, 1);
	obs_properties_add_int(crop_group_props, "bottom", obs_module_text("CropBottom"), -8192, 8192, 1);

	// Tagging Settings
	obs_properties_add_int(props, "refresh_seconds", obs_module_text("RefreshTimer"), 10, 300, 1);
	obs_properties_add_bool(props, "tagging_enabled", obs_module_text("TaggingEnable"));

	return props;
}

static void hero_watcher_update(void *data, obs_data_t *settings)
{
	struct hero_watcher_data *filter = data;

	// Update Crop Settings
	filter->preview = obs_data_get_bool(settings, "preview_weapon");
	filter->left    = (int)obs_data_get_int(settings, "left");
	filter->right   = (int)obs_data_get_int(settings, "right");
	filter->top     = (int)obs_data_get_int(settings, "top");
	filter->bottom  = (int)obs_data_get_int(settings, "bottom");

	// Update Tagging Settings
	filter->refresh_seconds  = (int)obs_data_get_int(settings, "refresh_seconds");
	filter->tagging = obs_data_get_bool(settings, "tagging_enabled");
	filter->remaining_time = (float)filter->refresh_seconds;

}

static void calc_crop_dimensions(struct hero_watcher_data *filter, struct vec2 *mul_val, struct vec2 *add_val)
{
	obs_source_t *target = obs_filter_get_target(filter->context);
	uint32_t width;
	uint32_t height;

	if (!target) {
		return;
	} else {
		width = obs_source_get_base_width(target);
		height = obs_source_get_base_height(target);
	}

	filter->width = (int)width - filter->left - filter->right;
	filter->height = (int)height - filter->top - filter->bottom;

	if (filter->width < 1)
		filter->width = 1;
	if (filter->height < 1)
		filter->height = 1;

	if (width) {
		mul_val->x = (float)filter->width / (float)width;
		add_val->x = (float)filter->left / (float)width;
	}

	if (height) {
		mul_val->y = (float)filter->height / (float)height;
		add_val->y = (float)filter->top / (float)height;
	}
}

static void init_hero_detection(struct hero_watcher_data *filter)
{
    if (os_atomic_load_bool(&filter->hero_detection_running))
        return;

    os_atomic_store_bool(&filter->hero_detection_running, true);

    int ret = pthread_create(&filter->hero_thread, NULL, hero_detection_thread, filter);
    if (ret != 0) {
        blog(LOG_ERROR, "[%s] Failed to create hero detection thread: %d", __func__, ret);
        os_atomic_store_bool(&filter->hero_detection_running, false);
    }
}

static void hero_watcher_tick(void *data, float seconds)
{
	struct hero_watcher_data *filter = data;
	vec2_zero(&filter->mul_val);
	vec2_zero(&filter->add_val);
	calc_crop_dimensions(filter, &filter->mul_val, &filter->add_val);
	if(filter->tagging){
		filter->remaining_time -= seconds;
		if(filter->remaining_time < 0){
				blog(LOG_DEBUG, "[%s] Timer Resetting!", __func__);
				filter->remaining_time = (float)filter->refresh_seconds;
				init_hero_detection(filter);
		}
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
	.get_defaults = hero_watcher_defaults,
	.get_properties = hero_watcher_properties,
	.update = hero_watcher_update,
	.video_tick = hero_watcher_tick,
    .video_render = hero_watcher_render
};