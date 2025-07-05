#include "herowatcher_detector.h"
#include "herowatcher_plugin.h"

struct hero_crop_context {
	obs_source_t *target;
	gs_texrender_t *texrender;
	gs_stagesurf_t *stage;
	uint32_t crop_width;
	uint32_t crop_height;
	struct hero_watcher_data *filter;
};

static bool hero_crop_init(struct hero_crop_context *ctx, void *data)
{
	// Fill out thread struct
	ctx->filter = (struct hero_watcher_data *)data;
	ctx->target = obs_filter_get_target(ctx->filter->context);
	ctx->texrender = NULL;
	ctx->stage = NULL;
	if (!ctx->target || !ctx->filter->effect) {
		blog(LOG_ERROR, "[%s] Could not find: target source || crop effect", __func__);
		return false;
	}

	blog(LOG_DEBUG, "[%s] Found colorspace, technique, multiplier, and format", __func__);
	uint32_t source_width = obs_source_get_width(ctx->target);
	uint32_t source_height = obs_source_get_height(ctx->target);
	ctx->crop_width = (uint32_t)(ctx->filter->mul_val.x * source_width);
	ctx->crop_height = (uint32_t)(ctx->filter->mul_val.y * source_height);
	if (ctx->crop_width == 0 || ctx->crop_height == 0) {
		blog(LOG_ERROR, "[%s] Invalid crop values!", __func__);
		return false;
	}

	blog(LOG_DEBUG, "[%s] Thread context set successfully", __func__);
	return true;
}

static void hero_crop_render(struct hero_crop_context *ctx)
{
	ctx->texrender = gs_texrender_create(ctx->filter->format, GS_ZS_NONE);
	ctx->stage = gs_stagesurface_create(ctx->crop_width, ctx->crop_height, ctx->filter->format);
	bool begin_success = ctx->texrender &&
						 ctx->stage &&
						 gs_texrender_begin(ctx->texrender, ctx->crop_width, ctx->crop_height);
    
	if (!begin_success) {
		blog(LOG_DEBUG, "[%s] Failed to create texrender and stage", __func__);
		return false;
	}
	gs_ortho(0.0f, (float)ctx->crop_width, 0.0f, (float)ctx->crop_height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, ctx->crop_width, ctx->crop_height);
	gs_clear(GS_CLEAR_COLOR, NULL, 0.0f, 0);
	blog(LOG_DEBUG, "[%s] Canvas set", __func__);


	if (obs_source_process_filter_begin_with_color_space(ctx->filter->context, format, source_space,
	                                                     OBS_NO_DIRECT_RENDERING)) {
		blog(LOG_DEBUG, "[%s] Set crop effect shader params", __func__);
		gs_effect_set_vec2(ctx->filter->param_mul, &ctx->filter->mul_val);
		gs_effect_set_vec2(ctx->filter->param_add, &ctx->filter->add_val);
		gs_effect_set_float(ctx->filter->param_multiplier, multiplier);

		blog(LOG_DEBUG, "[%s] Run Shader??", __func__);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

		obs_source_process_filter_tech_end(ctx->filter->context, ctx->filter->effect,
		                                   ctx->crop_width, ctx->crop_height, technique);

		gs_blend_state_pop();
	}
	gs_texrender_end(ctx->texrender);

	gs_texture_t *tex = gs_texrender_get_texture(ctx->texrender);
	gs_stage_texture(ctx->stage, tex);
	blog(LOG_DEBUG, "[%s] Cropped image saved in texture", __func__);
	obs_leave_graphics();
}

static void hero_crop_save(struct hero_crop_context *ctx)
{
	uint8_t *savedata;
	uint32_t linesize;

	obs_enter_graphics();
	bool mapped = gs_stagesurface_map(ctx->stage, &savedata, &linesize);
	obs_leave_graphics();

	blog(LOG_DEBUG, "[%s] Starting file save...", __func__);
	if (mapped) {
		time_t now = time(NULL);
		char filename[128];
		snprintf(filename, sizeof(filename), "crop_%ld.raw", now);

		FILE *f = os_fopen(filename, "wb");
		if (f) {
			for (uint32_t y = 0; y < ctx->crop_height; ++y) {
				fwrite(savedata + y * linesize, 1, ctx->crop_width * 4, f);
			}
			fclose(f);
			blog(LOG_INFO, "[%s] Saved cropped RGBA frame to %s", __func__, filename);
		} else {
			blog(LOG_ERROR, "[%s] Failed to open file %s", __func__, filename);
		}

		obs_enter_graphics();
		gs_stagesurface_unmap(ctx->stage);
		obs_leave_graphics();
	}
}

void *hero_detection_thread(void *data)
{
	blog(LOG_DEBUG, "[%s] Starting hero detection thread!", __func__);
	struct hero_crop_context ctx = {0};
    
	if (!hero_crop_init(&ctx, data))
		goto done;

	blog(LOG_DEBUG, "[%s] Entering graphics context!", __func__);
	obs_enter_graphics();
	hero_crop_render(&ctx);
	hero_crop_save(&ctx);

cleanup:
	if (ctx.stage)
		gs_stagesurface_destroy(ctx.stage);
	if (ctx.texrender)
		gs_texrender_destroy(ctx.texrender);
	obs_leave_graphics();
done:
	blog(LOG_DEBUG, "[%s] Hero detection thread done", __func__);
	os_atomic_store_bool(&ctx.filter->hero_detection_running, false);
	return NULL;
}
