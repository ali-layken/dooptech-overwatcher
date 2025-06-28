#include <obs-module.h>
#include <graphics/vec2.h>
#include <util/platform.h>

struct hero_tracker_data {
	obs_source_t *context;

	gs_effect_t *effect;
	gs_eparam_t *param_mul;
	gs_eparam_t *param_add;
	gs_eparam_t *param_multiplier;

	int left;
	int right;
	int top;
	int bottom;
	int abs_cx;
	int abs_cy;
	int width;
	int height;
	bool absolute;

	struct vec2 mul_val;
	struct vec2 add_val;
};

static const char *hero_tracker_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("HeroTracker");
}

static void *hero_tracker_create(obs_data_t *settings, obs_source_t *context)
{
	struct hero_tracker_data *filter = bzalloc(sizeof(*filter));
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

	filter->param_mul = gs_effect_get_param_by_name(filter->effect, "mul_val");
	filter->param_add = gs_effect_get_param_by_name(filter->effect, "add_val");
	filter->param_multiplier = gs_effect_get_param_by_name(filter->effect, "multiplier");

	obs_source_update(context, settings);
	return filter;
}

static void hero_tracker_destroy(void *data)
{
	struct hero_tracker_data *filter = data;

	obs_enter_graphics();
	gs_effect_destroy(filter->effect);
	obs_leave_graphics();

	bfree(filter);
}

static void hero_tracker_update(void *data, obs_data_t *settings)
{
	struct hero_tracker_data *filter = data;

	filter->left = (int)obs_data_get_int(settings, "crop_left");
	filter->top = (int)obs_data_get_int(settings, "crop_top");
	filter->right = (int)obs_data_get_int(settings, "crop_right");
	filter->bottom = (int)obs_data_get_int(settings, "crop_bottom");
}

static bool preview_weapon_clicked(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
    bool preview_weapon = obs_data_get_bool(settings, "preview_weapon");

    obs_property_set_visible(obs_properties_get(props, "crop_group"), preview_weapon);

    UNUSED_PARAMETER(p);
    return true;
}

static obs_properties_t *hero_tracker_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	// Preview Weapon Crop Checkbox
	obs_property_t *p= obs_properties_add_bool(props, "preview_weapon", obs_module_text("Preview.Weapon"));
    obs_property_set_modified_callback(p, preview_weapon_clicked);
	
	obs_properties_t *crop_group_props = obs_properties_create();
	obs_properties_add_group(props, "crop_group", obs_module_text("Crop.Group"), OBS_GROUP_NORMAL, crop_group_props);

	// Weapon crop region settings
	obs_properties_add_int(crop_group_props, "crop_left", obs_module_text("Crop.Left"), -8192, 8192, 1);
	obs_properties_add_int(crop_group_props, "crop_right", obs_module_text("Crop.Right"), -8192, 8192, 1);
	obs_properties_add_int(crop_group_props, "crop_top", obs_module_text("Crop.Top"), -8192, 8192, 1);
	obs_properties_add_int(crop_group_props, "crop_bottom", obs_module_text("Crop.Bottom"), -8192, 8192, 1);

	UNUSED_PARAMETER(data);
	return props;
}

static void hero_tracker_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "preview_binarization", false);
	obs_data_set_default_int(settings, "crop_left", 0);
	obs_data_set_default_int(settings, "crop_right", 0);
	obs_data_set_default_int(settings, "crop_top", 0);
	obs_data_set_default_int(settings, "crop_bottom", 0);
}

static void calc_crop_dimensions(struct hero_tracker_data *filter, struct vec2 *mul_val, struct vec2 *add_val)
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

	bool preview_weapon = obs_data_get_bool(obs_source_get_settings(filter->context), "preview_weapon");

	if (preview_weapon) {
		filter->width = (int)width - filter->left - filter->right;
		filter->height = (int)height - filter->top - filter->bottom;
	} else {
        filter->width = (int)width;
        filter->height = (int)height;
    }

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

static void hero_tracker_tick(void *data, float seconds)
{
	struct hero_tracker_data *filter = data;

	vec2_zero(&filter->mul_val);
	vec2_zero(&filter->add_val);
	calc_crop_dimensions(filter, &filter->mul_val, &filter->add_val);

	UNUSED_PARAMETER(seconds);
}

static const char *get_tech_name_and_multiplier(enum gs_color_space current_space, enum gs_color_space source_space,
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

static void hero_tracker_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct hero_tracker_data *filter = data;

	const enum gs_color_space preferred_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};

	const enum gs_color_space source_space = obs_source_get_color_space(
		obs_filter_get_target(filter->context), OBS_COUNTOF(preferred_spaces), preferred_spaces);
	float multiplier;
	const char *technique = get_tech_name_and_multiplier(gs_get_color_space(), source_space, &multiplier);
	const enum gs_color_format format = gs_get_format_from_space(source_space);
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

static uint32_t hero_tracker_width(void *data)
{
	struct hero_tracker_data *crop = data;
	return (uint32_t)crop->width;
}

static uint32_t hero_tracker_height(void *data)
{
	struct hero_tracker_data *crop = data;
	return (uint32_t)crop->height;
}

static enum gs_color_space hero_tracker_get_color_space(void *data, size_t count,
							const enum gs_color_space *preferred_spaces)
{
	const enum gs_color_space potential_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};

	struct hero_tracker_data *const filter = data;
	const enum gs_color_space source_space = obs_source_get_color_space(
		obs_filter_get_target(filter->context), OBS_COUNTOF(potential_spaces), potential_spaces);

	enum gs_color_space space = source_space;
	for (size_t i = 0; i < count; ++i) {
		space = preferred_spaces[i];
		if (space == source_space)
			break;
	}

	return space;
}

struct obs_source_info hero_tracker = {
	.id = "hero_tracker2",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = hero_tracker_get_name,
	.create = hero_tracker_create,
	.destroy = hero_tracker_destroy,
	.update = hero_tracker_update,
	.get_properties = hero_tracker_properties,
	.get_defaults = hero_tracker_defaults,
	.video_tick = hero_tracker_tick,
	.video_render = hero_tracker_render,
	.get_width = hero_tracker_width,
	.get_height = hero_tracker_height,
	.video_get_color_space = hero_tracker_get_color_space,
};