/*
 * xwa_asset_bake — frontend font pass.
 *
 * Loads each classic frontend bitmap font through the shared XWA 2D decoder
 * and emits an HD atlas pair
 * `fonts/font<size>.{png,fnt}` (TFNT v2, via imgbake font_atlas_write)
 * under the bake root:
 *
 *   - the PNG holds white PMA glyphs on a 16-column grid, NN-upscaled
 *     by `scale` (RM_SCALE) — the auto tier, classic-look text in HD;
 *   - the .fnt carries the per-glyph atlas rects the runtime consumes
 *     (AeronFontAtlas; glyph positions come from the snapshot records,
 *     so only the rects matter at render time).
 *
 * Hand-tuning: TIE's font_tune GUI reads/writes this exact pair —
 *   font_tune --reference <root>/fonts/font15 --ttf Some.ttf
 * and saving over the same basename substitutes the TTF atlas.
 *
 * Glyph decode mirrors FrontImage_BlitGlyphRLE_16bpp exactly: per row
 * a u32 total-size prefix, then tokens — t == 0x80 row end; t & 0x80
 * literal run (len = t & 0x7F, one payload byte per texel); t & 0x40
 * transparent run (len = t & 0x3F); else value run (len = t, one
 * payload byte). Payload bytes are ANTIALIASING levels: the engine
 * mixes fg*(v+1)/32 + bg*(31-v)/32 through g_glyphGradientLut in
 * ENCODED space, so the baked alpha is srgb_to_linear((v+1)/32) —
 * the shared decoder converts those levels to linear alpha, and the PMA blend displays
 * the same edge values classic produced.
 */

#include "bake_fonts.h"

#include "font_atlas.h"
#include "xwa_2d.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/* ---- pass -------------------------------------------------------------- */

static int mkdir_p2(const char* path) {
	char tmp[1024];
	snprintf(tmp, sizeof tmp, "%s", path);
	for (char* p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
				return 0;
			}
			*p = '/';
		}
	}
	return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

/* Rasterize the substitute TTF at HD metrics matched to the classic
 * font GLOBALLY (cell height, cap height, baseline) — glyph shapes and
 * widths stay natural; per-glyph adjustments belong in font_tune. */
static int bake_ttf_font(const BakeFontsOptions* opt, int point_size, int classic_cell_h,
						 int cap_h_classic, int classic_baseline, const uint8_t* ttf,
						 size_t ttf_size, const char* basename) {
	const int scale = opt->scale > 0 ? opt->scale : 1;

	FontAtlasParams params = { 0 };
	params.first_char      = 32; /* the builder clamps 0 to 32 anyway */
	params.num_chars       = 224; /* 32..255 — engine text is cp1252 */
	params.cell_h          = classic_cell_h * scale;
	/* Anchor the TTF baseline at the classic ink baseline so glyphs
	 * drawn top-aligned at record positions sit at the same rows. */
	params.baseline        = classic_baseline > 0 ? classic_baseline : -1;
	params.cap_height      = cap_h_classic > 0 ? cap_h_classic * scale : 0;
	memset(params.compression_glyph_lsb_atlas, -1,
		   sizeof params.compression_glyph_lsb_atlas); /* -1 = natural LSBs */
	/* Engine text is Windows-1252: bytes 0x80..0x9F are typographic
	 * punctuation, not C1 controls — remap those slots to the Unicode
	 * codepoints the TTF actually carries. */
	static const int cp1252_high[32] = {
		0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
		0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
		0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178,
	};
	for (int i = 0; i < 32; i++) {
		params.codepoint_remap[0x80 + i] = cp1252_high[i];
	}

	FontAtlasResult r = { 0 };
	char            err[256];
	if (!font_atlas_build(ttf, ttf_size, &params, &r, err, sizeof err)) {
		fprintf(stderr, "bake: font %d TTF build failed: %s\n", point_size, err);
		return 0;
	}

	int ok = font_atlas_write(&r, basename, err, sizeof err) ? 1 : 0;
	if (!ok) {
		fprintf(stderr, "bake: font %d write failed: %s\n", point_size, err);
	} else {
		printf("font %-3d -> %s.{png,fnt} (TTF, %dx%d, cell %dx%d, baseline %d)\n", point_size,
			   basename, r.atlas_w, r.atlas_h, r.cell_w, r.cell_h, r.baseline);
	}
	font_atlas_free(&r);
	return ok;
}

static uint8_t* read_whole_file(const char* path, size_t* out_size) {
	FILE* f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	rewind(f);
	uint8_t* buf = sz > 0 ? (uint8_t*)malloc((size_t)sz) : NULL;
	if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return NULL;
	}
	fclose(f);
	*out_size = (size_t)sz;
	return buf;
}

static int bake_one_font(const BakeFontsOptions* opt, AeronVfs* vfs, int point_size,
						 const uint8_t* ttf, size_t ttf_size) {
	char source_path[64];
	snprintf(source_path, sizeof source_path, "TIMES%d.ABP", point_size);
	uint8_t* source_bytes = NULL;
	size_t source_size = 0;
	Xwa2dFontAtlas source = { 0 };
	char decode_error[256] = { 0 };
	if (!AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, source_path, 0, &source_bytes, &source_size) ||
		!Xwa2d_DecodeAbpFont(source_bytes, source_size, &source, decode_error, sizeof decode_error)) {
		fprintf(stderr, "bake: font %d decode failed: %s\n", point_size,
				decode_error[0] ? decode_error : "file read failed");
		free(source_bytes);
		return 0;
	}
	free(source_bytes);

	const int scale = opt->scale > 0 ? opt->scale : 1;
	const int atlas_w = source.width * scale;
	const int atlas_h = source.height * scale;
	uint8_t* rgba = (uint8_t*)calloc((size_t)atlas_w * atlas_h, 4);
	FontAtlasGlyph* glyphs = (FontAtlasGlyph*)calloc(source.glyph_count, sizeof *glyphs);
	if (!rgba || !glyphs) {
		free(rgba);
		free(glyphs);
		Xwa2dFontAtlas_Free(&source);
		return 0;
	}
	for (int y = 0; y < atlas_h; y++) {
		for (int x = 0; x < atlas_w; x++) {
			const uint8_t alpha = source.rgba[((size_t)(y / scale) * source.width + x / scale) * 4 + 3];
			uint8_t* pixel = rgba + ((size_t)y * atlas_w + x) * 4;
			pixel[0] = pixel[1] = pixel[2] = pixel[3] = alpha;
		}
	}
	for (uint16_t ch = 0; ch < source.glyph_count; ch++) {
		const Xwa2dGlyph* input = &source.glyphs[ch];
		FontAtlasGlyph* output = &glyphs[ch];
		output->atlas_x = (uint16_t)(input->x * scale);
		output->atlas_y = (uint16_t)(input->y * scale);
		output->atlas_w = (uint16_t)(input->width * scale);
		output->atlas_h = (uint16_t)(input->height * scale);
		const int advance = input->advance * scale - FONT_ATLAS_SPACE_BETWEEN_PX;
		output->advance = (uint16_t)(advance > 0 ? advance : 0);
	}

	int cap_h_classic = 0;
	int baseline = source.cell_height * scale;
	for (int ch = 'A'; ch <= 'H'; ch += 'H' - 'A') {
		const Xwa2dGlyph* glyph = &source.glyphs[ch];
		int ink_bottom = -1;
		int ink_top = glyph->height;
		for (int y = 0; y < glyph->height; y++) {
			for (int x = 0; x < glyph->width; x++) {
				const uint8_t alpha =
					source.rgba[((size_t)(glyph->y + y) * source.width + glyph->x + x) * 4 + 3];
				if (alpha > 127) {
					if (y > ink_bottom) ink_bottom = y;
					if (y < ink_top) ink_top = y;
				}
			}
		}
		if (ch == 'A' && ink_bottom >= 0)
			baseline = (ink_bottom + 1) * scale;
		if (ch == 'H' && ink_bottom >= ink_top)
			cap_h_classic = ink_bottom - ink_top + 1;
	}

	FontAtlasResult r = { 0 };
	r.atlas_w         = atlas_w;
	r.atlas_h         = atlas_h;
	r.cell_w          = source.cell_width * scale;
	r.cell_h          = source.cell_height * scale;
	r.first_char      = source.first_char;
	r.num_chars       = source.glyph_count;
	r.baseline        = baseline;
	r.rgba            = rgba;
	r.glyphs          = glyphs;

	/* With a TTF the classic NN atlas becomes the font_tune reference;
	 * without one it IS the runtime atlas (blocky classic look). */
	char dir[1024];
	if (ttf) {
		snprintf(dir, sizeof dir, "%s/fonts/reference", opt->out_root);
	} else {
		snprintf(dir, sizeof dir, "%s/fonts", opt->out_root);
	}
	mkdir_p2(dir);
	char basename[1100];
	snprintf(basename, sizeof basename, "%s/font%d", dir, point_size);
	char err[256];
	int  ok = font_atlas_write(&r, basename, err, sizeof err) ? 1 : 0;
	if (!ok) {
		fprintf(stderr, "bake: font %d write failed: %s\n", point_size, err);
	} else {
		printf("font %-3d -> %s.{png,fnt} (%dx%d, cell %dx%d, baseline %d)\n", point_size,
			   basename, atlas_w, atlas_h, r.cell_w, r.cell_h, baseline);
	}
	free(rgba);
	free(glyphs);

	if (ok && ttf) {
		char main_base[1100];
		snprintf(dir, sizeof dir, "%s/fonts", opt->out_root);
		mkdir_p2(dir);
		snprintf(main_base, sizeof main_base, "%s/font%d", dir, point_size);
		ok = bake_ttf_font(opt, point_size, source.cell_height, cap_h_classic, baseline, ttf, ttf_size,
						   main_base);
	}
	Xwa2dFontAtlas_Free(&source);
	return ok;
}

typedef struct FontDiscovery {
	int sizes[32];
	int count;
} FontDiscovery;

static int glob_collect_abp(void* user, const AeronVfsEntry* e) {
	FontDiscovery* d = (FontDiscovery*)user;
	if (e->is_directory || strncasecmp(e->name, "TIMES", 5) != 0) {
		return 1;
	}
	const int size = atoi(e->name + 5);
	if (size > 0 && size < 256 && d->count < (int)(sizeof d->sizes / sizeof d->sizes[0])) {
		d->sizes[d->count++] = size;
	}
	return 1;
}

int BakeFonts_Run(const BakeFontsOptions* opt, AeronVfs* vfs) {
	uint8_t* ttf      = NULL;
	size_t   ttf_size = 0;
	if (opt->ttf_path) {
		ttf = read_whole_file(opt->ttf_path, &ttf_size);
		if (!ttf) {
			fprintf(stderr, "bake: cannot read TTF '%s'\n", opt->ttf_path);
			return 0;
		}
	}

	FontDiscovery disc = { 0 };
	AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, ".", "TIMES*.ABP",
				  AERON_VFS_GLOB_FILES | AERON_VFS_GLOB_CASE_INSENSITIVE, glob_collect_abp,
				  &disc);
	printf("bake: %d frontend fonts discovered\n", disc.count);
	int baked = 0;
	for (int i = 0; i < disc.count; i++) {
		baked += bake_one_font(opt, vfs, disc.sizes[i], ttf, ttf_size);
	}
	free(ttf);
	return baked;
}
