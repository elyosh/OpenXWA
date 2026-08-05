#include "bake_flight_fonts.h"

#include "aeron/image.h"
#include "bake_source.h"
#include "ktx2_writer.h"
#include "xwa_2d.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FNT_MAGIC 0x544E4654u
#define FNT_VERSION 2

static int mkdir_p(const char* path) {
	char tmp[1024];
	snprintf(tmp, sizeof tmp, "%s", path);
	for (char* p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
				return 0;
			*p = '/';
		}
	}
	return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static void wr_u16(FILE* f, uint16_t value) {
	uint8_t b[2] = { (uint8_t)value, (uint8_t)(value >> 8) };
	fwrite(b, 1, sizeof b, f);
}

static void wr_u32(FILE* f, uint32_t value) {
	uint8_t b[4] = { (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24) };
	fwrite(b, 1, sizeof b, f);
}

static int scaled_u16_fits(int value, int scale) {
	return value >= 0 && scale > 0 && value <= UINT16_MAX / scale;
}

static int write_fnt(const char* path, const Xwa2dFontAtlas* font, int scale) {
	if (!font || !scaled_u16_fits(font->width, scale) ||
		!scaled_u16_fits(font->height, scale) ||
		!scaled_u16_fits(font->cell_width, scale) ||
		!scaled_u16_fits(font->cell_height, scale) ||
		!scaled_u16_fits(font->baseline, scale))
		return 0;
	for (uint16_t glyph = 0; glyph < font->glyph_count; glyph++) {
		const Xwa2dGlyph* metric = &font->glyphs[glyph];
		if (!scaled_u16_fits(metric->x, scale) ||
			!scaled_u16_fits(metric->y, scale) ||
			!scaled_u16_fits(metric->width, scale) ||
			!scaled_u16_fits(metric->height, scale) ||
			!scaled_u16_fits(metric->advance, scale))
			return 0;
	}
	FILE* f = fopen(path, "wb");
	if (!f)
		return 0;
	wr_u32(f, FNT_MAGIC);
	wr_u16(f, FNT_VERSION);
	wr_u16(f, font->first_char);
	wr_u16(f, font->glyph_count);
	wr_u16(f, (uint16_t)(font->width * scale));
	wr_u16(f, (uint16_t)(font->height * scale));
	wr_u16(f, (uint16_t)(font->cell_width * scale));
	wr_u16(f, (uint16_t)(font->cell_height * scale));
	wr_u16(f, (uint16_t)(font->baseline * scale));
	wr_u32(f, 0);
	for (uint16_t glyph = 0; glyph < font->glyph_count; glyph++) {
		const Xwa2dGlyph* metric = &font->glyphs[glyph];
		wr_u16(f, (uint16_t)(metric->x * scale));
		wr_u16(f, (uint16_t)(metric->y * scale));
		wr_u16(f, (uint16_t)(metric->width * scale));
		wr_u16(f, (uint16_t)(metric->height * scale));
		wr_u16(f, (uint16_t)(metric->advance * scale));
	}
	const int ok = !ferror(f) && fclose(f) == 0;
	return ok;
}

static int bake_tier(const BakeFlightFontsOptions* opt, int tier, const Xwa2dFontAtlas* font) {
	const int scale = opt->scale > 0 ? opt->scale : 1;
	int hd_width = 0, hd_height = 0;
	uint8_t* hd =
		Aeron_ImageUpscaleNearestRgba8(font->rgba, font->width, font->height, scale, &hd_width, &hd_height);
	if (!hd)
		return 0;
	char base[1024], path[1060];
	snprintf(base, sizeof base, "%s/flight/fonts/font_tier_%d", opt->out_root, tier);
	snprintf(path, sizeof path, "%s.ktx2", base);
	int ok = write_ktx2_bc7_with_generated_mips(path, hd_width, hd_height, hd, opt->bc7_quality, KTX2_TF_SRGB,
												opt->zstd != 0);
	free(hd);
	if (ok) {
		snprintf(path, sizeof path, "%s.fnt", base);
		ok = write_fnt(path, font, scale);
	}
	if (ok)
		printf("flight font tier %d -> %s.{ktx2,fnt}\n", tier, base);
	return ok;
}

int BakeFlightFonts_Run(const BakeFlightFontsOptions* opt, AeronVfs* vfs) {
	if (!opt || !opt->out_root)
		return 0;
	char dir[1024];
	snprintf(dir, sizeof dir, "%s/flight/fonts", opt->out_root);
	if (!mkdir_p(dir))
		return 0;
	BakeDatCatalog catalog;
	Xwa2dFrameSet source_group = { 0 };
	char error[256] = { 0 };
	if (!BakeSource_InitDatCatalog(vfs, &catalog, error, sizeof error) ||
		!BakeSource_LoadDatGroup(&catalog, 16000, &source_group, error, sizeof error) ||
		source_group.count < 3) {
		fprintf(stderr, "bake: flight font source failed: %s\n", error);
		BakeSource_FreeDatCatalog(&catalog);
		Xwa2dFrameSet_Free(&source_group);
		return 0;
	}
	BakeSource_FreeDatCatalog(&catalog);
	int baked = 0;
	for (int tier = 0; tier < 3; tier++) {
		Xwa2dFontAtlas font = { 0 };
		error[0] = '\0';
		if (!Xwa2d_BuildFlightFontTier(&source_group, tier, &font, error, sizeof error)) {
			fprintf(stderr, "bake: flight font tier %d failed: %s\n", tier, error);
			continue;
		}
		baked += bake_tier(opt, tier, &font);
		Xwa2dFontAtlas_Free(&font);
	}
	Xwa2dFrameSet_Free(&source_group);
	return baked;
}
