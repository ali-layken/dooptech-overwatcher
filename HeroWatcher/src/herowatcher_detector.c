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
	blog(LOG_DEBUG, "[%s] crop_width: %u, crop_height: %u", __func__, ctx->crop_width, ctx->crop_height);
	if (ctx->crop_width == 0 || ctx->crop_height == 0) {
		blog(LOG_ERROR, "[%s] Invalid crop values!", __func__);
		return false;
	}

	blog(LOG_DEBUG, "[%s] Thread context set successfully", __func__);
	return true;
}

static void hero_crop_save(struct hero_crop_context *ctx)
{
	uint8_t *savedata;
	uint32_t linesize;
	bool mapped = gs_stagesurface_map(ctx->stage, &savedata, &linesize);

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

		gs_stagesurface_unmap(ctx->stage);
	}
}

void *hero_detection_thread(void *data)
{
	blog(LOG_DEBUG, "[%s] Starting hero detection thread!", __func__);
	struct hero_crop_context ctx = {0};
    
	if (!hero_crop_init(&ctx, data))
	{
		blog(LOG_ERROR, "[%s] Hero detection thread failed to initiate.", __func__);
		goto done;
	}
	blog(LOG_DEBUG, "[%s] Entering graphics context!", __func__);
	obs_enter_graphics();
		ctx.texrender = gs_texrender_create(ctx.filter->color_format, GS_ZS_NONE);
		ctx.stage = gs_stagesurface_create(ctx.crop_width, ctx.crop_height, ctx.filter->color_format);
		bool begin_success = ctx.texrender &&
							 ctx.stage &&
							 gs_texrender_begin(ctx.texrender, ctx.crop_width, ctx.crop_height);
			if (!begin_success) {
				blog(LOG_ERROR, "[%s] Failed to create texrender and stage", __func__);
				goto cleanup;
			
			}
			blog(LOG_DEBUG, "[%s] Created texrender and stage surface successfully", __func__);
			struct vec4 clear_color = {1.0f, 0.0f, 0.0f, 1.0f};
			gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_texrender_end(ctx.texrender);

		blog(LOG_DEBUG, "[%s] Grabbing cropped image from GPU", __func__);
		gs_texture_t *tex = gs_texrender_get_texture(ctx.texrender);
		if (tex)
		{
			gs_stage_texture(ctx.stage, tex);
			gs_flush(); 
			os_sleepto_ns(os_gettime_ns() + 20 * 1000000); 
			hero_crop_save(&ctx);
		} else {
			blog(LOG_ERROR, "[%s] Failed to get texture from texrender", __func__);
		}

cleanup:
		if (ctx.stage)
			gs_stagesurface_destroy(ctx.stage);
		if (ctx.texrender)
			gs_texrender_destroy(ctx.texrender);
	obs_leave_graphics();
	
done:
	os_atomic_store_bool(&ctx.filter->hero_detection_running, false);
	blog(LOG_DEBUG, "[%s] Hero detection thread done", __func__);
	return NULL;
}
