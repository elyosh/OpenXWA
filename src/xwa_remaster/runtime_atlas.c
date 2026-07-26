#include "xwa_remaster/runtime_atlas.h"

#include "aeron/atlas_pack.h"
#include "aeron/image.h"

#include <stdlib.h>
#include <string.h>

#define RUNTIME_ATLAS_GUTTER 2
#define RUNTIME_ATLAS_MAX_DIM 4096

static int atlas_mip_count(int width, int height, int enabled) {
	int count = 1;
	if (!enabled)
		return count;
	while (width > 1 || height > 1) {
		if (width > 1)
			width /= 2;
		if (height > 1)
			height /= 2;
		count++;
	}
	return count;
}

static int atlas_upload_page(XwaRuntimeAtlasPage* page, AeronCommandBuffer* cmd, uint8_t* pixels, int width,
							 int height, int generate_mips, AeronTextureFormat format,
							 AeronColorSpace color_space, const char* debug_name) {
	const int mip_count = atlas_mip_count(width, height, generate_mips);
	AeronTextureUploadDesc* uploads;
	uint8_t** levels;
	page->texture = Aeron_CreateTexture(&(AeronTextureDesc) {
		.width = width,
		.height = height,
		.mip_count = mip_count,
		.format = format,
		.usage = AERON_TEXTURE_USAGE_SAMPLED | AERON_TEXTURE_USAGE_TRANSFER_DST,
		.debug_name = debug_name,
	});
	if (!page->texture)
		return 0;

	uploads = (AeronTextureUploadDesc*)calloc((size_t)mip_count, sizeof *uploads);
	levels = (uint8_t**)calloc((size_t)mip_count, sizeof *levels);
	if (!uploads || !levels) {
		free(uploads);
		free(levels);
		Aeron_DestroyTexture(page->texture);
		page->texture = NULL;
		return 0;
	}
	levels[0] = pixels;
	int level_width = width;
	int level_height = height;
	for (int mip = 0; mip < mip_count; mip++) {
		uploads[mip] = (AeronTextureUploadDesc) {
			.texture = page->texture,
			.mip_level = mip,
			.width = level_width,
			.height = level_height,
			.pixels = levels[mip],
			.pitch = level_width * 4,
			.pixel_format = AERON_PIXEL_FORMAT_RGBA8888,
			.color_space = color_space,
		};
		if (mip + 1 < mip_count) {
			levels[mip + 1] = Aeron_ImageDownsampleRgba8(
				levels[mip], level_width, level_height, &level_width, &level_height);
			if (!levels[mip + 1]) {
				for (int i = 1; i < mip_count; i++)
					free(levels[i]);
				free(levels);
				free(uploads);
				Aeron_DestroyTexture(page->texture);
				page->texture = NULL;
				return 0;
			}
		}
	}
	const int uploaded = Aeron_UploadTextureBatchCmd(cmd, uploads, (uint32_t)mip_count);
	for (int i = 1; i < mip_count; i++)
		free(levels[i]);
	free(levels);
	free(uploads);
	if (!uploaded) {
		Aeron_DestroyTexture(page->texture);
		page->texture = NULL;
		return 0;
	}
	page->width = width;
	page->height = height;
	return 1;
}

AeronTexture* XwaRuntimeTexture_UploadLinearRgba(AeronCommandBuffer* cmd, uint8_t* rgba, int width,
												 int height, int generate_mips, const char* debug_name) {
	if (!cmd || !rgba || width <= 0 || height <= 0)
		return NULL;
	Aeron_ImagePremultiplyRgba8(rgba, (size_t)width * height);
	XwaRuntimeAtlasPage page = { 0 };
	return atlas_upload_page(&page, cmd, rgba, width, height, generate_mips, AERON_TEXTURE_FORMAT_RGBA8_UNORM,
							 AERON_COLOR_SPACE_LINEAR_SRGB, debug_name)
			   ? page.texture
			   : NULL;
}

int XwaRuntimeAtlas_Build(XwaRuntimeAtlas* atlas, AeronCommandBuffer* cmd, const Xwa2dFrameSet* source,
						  int generate_mips, const char* debug_name) {
	if (!atlas || !cmd || !source || !source->frames || source->count <= 0)
		return 0;
	memset(atlas, 0, sizeof *atlas);
	AeronAtlasImage* images = (AeronAtlasImage*)calloc((size_t)source->count, sizeof *images);
	AeronCpuAtlas cpu = { 0 };
	if (!images)
		return 0;
	for (int i = 0; i < source->count; i++) {
		images[i] = (AeronAtlasImage) {
			.rgba = source->frames[i].rgba,
			.width = source->frames[i].width,
			.height = source->frames[i].height,
		};
	}
	const AeronAtlasBuildOptions options = {
		.gutter = RUNTIME_ATLAS_GUTTER,
		.max_dimension = RUNTIME_ATLAS_MAX_DIM,
		.max_pages = 0,
	};
	const int built = Aeron_AtlasBuildRgba8(images, source->count, &options, &cpu);
	if (!built) {
		free(images);
		return 0;
	}
	atlas->layout.frames = (AeronSpriteRect*)calloc((size_t)source->count, sizeof *atlas->layout.frames);
	atlas->layout.origin_x = (int16_t*)calloc((size_t)source->count, sizeof *atlas->layout.origin_x);
	atlas->layout.origin_y = (int16_t*)calloc((size_t)source->count, sizeof *atlas->layout.origin_y);
	atlas->layout.ids = (int32_t*)calloc((size_t)source->count, sizeof *atlas->layout.ids);
	atlas->layout.pages = (int16_t*)calloc((size_t)source->count, sizeof *atlas->layout.pages);
	atlas->layout.classic_w = (int16_t*)calloc((size_t)source->count, sizeof *atlas->layout.classic_w);
	atlas->layout.classic_h = (int16_t*)calloc((size_t)source->count, sizeof *atlas->layout.classic_h);
	atlas->pages = (XwaRuntimeAtlasPage*)calloc((size_t)cpu.page_count, sizeof *atlas->pages);
	if (!atlas->layout.frames || !atlas->layout.origin_x || !atlas->layout.origin_y ||
		!atlas->layout.ids || !atlas->layout.pages || !atlas->layout.classic_w ||
		!atlas->layout.classic_h || !atlas->pages)
		goto failed;
	atlas->layout.frame_count = source->count;
	for (int i = 0; i < source->count; i++) {
		const AeronAtlasImage* input = &images[i];
		const Xwa2dFrame* metadata = &source->frames[i];
		atlas->layout.frames[i] = (AeronSpriteRect) {
			.x = input->x,
			.y = input->y,
			.w = input->width,
			.h = input->height,
		};
		atlas->layout.ids[i] = metadata->sprite_id;
		atlas->layout.pages[i] = (int16_t)input->page;
		atlas->layout.origin_x[i] = (int16_t)metadata->anchor_x;
		atlas->layout.origin_y[i] = (int16_t)metadata->anchor_y;
		atlas->layout.classic_w[i] = (int16_t)metadata->width;
		atlas->layout.classic_h[i] = (int16_t)metadata->height;
	}
	free(images);
	images = NULL;
	atlas->layout.atlas_w = cpu.pages[0].width;
	atlas->layout.atlas_h = cpu.pages[0].height;
	for (int i = 0; i < cpu.page_count; i++) {
		Aeron_ImagePremultiplyRgba8(cpu.pages[i].rgba, (size_t)cpu.pages[i].width * cpu.pages[i].height);
		if (!atlas_upload_page(&atlas->pages[i], cmd, cpu.pages[i].rgba, cpu.pages[i].width,
							   cpu.pages[i].height, generate_mips, AERON_TEXTURE_FORMAT_RGBA8_SRGB,
							   AERON_COLOR_SPACE_SRGB, debug_name))
			goto failed;
		atlas->layout.page_count++;
	}
	Aeron_AtlasBuildFree(&cpu);
	return 1;

failed:
	free(images);
	Aeron_AtlasBuildFree(&cpu);
	XwaRuntimeAtlas_Free(atlas);
	return 0;
}

void XwaRuntimeAtlas_Free(XwaRuntimeAtlas* atlas) {
	if (!atlas)
		return;
	for (int i = 0; i < atlas->layout.page_count; i++)
		Aeron_DestroyTexture(atlas->pages[i].texture);
	free(atlas->pages);
	Aeron_SpriteAtlasFree(&atlas->layout);
	memset(atlas, 0, sizeof *atlas);
}
