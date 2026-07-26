#include "xwa_2d_internal.h"

#include <math.h>

#define ABP_HEADER_SIZE 0x60bu
#define FONT_COLUMNS 16
#define FONT_GUTTER 1
#define FLIGHT_FONT_FIRST_GLYPH 32
#define FLIGHT_FONT_GLYPH_COUNT 256
#define FLIGHT_FONT_COLUMNS 16
#define FLIGHT_FONT_ROWS 14
#define FLIGHT_FONT_GUTTER 2

typedef struct FlightFontTierDesc {
	int source_index;
	int glyph_size;
	int width_padding;
} FlightFontTierDesc;

static const FlightFontTierDesc flight_font_tiers[3] = {
	{ 1, 16, 2 },
	{ 0, 12, 1 },
	{ 2, 10, 1 },
};

static uint8_t font_alpha(uint8_t value) {
	float encoded = (float)((value & 0x1fu) + 1u) / 32.0f;
	float linear = encoded <= 0.04045f ? encoded / 12.92f : powf((encoded + 0.055f) / 1.055f, 2.4f);
	return (uint8_t)(linear * 255.0f + 0.5f);
}

static int decode_glyph(const uint8_t* data, const uint8_t* end, int width, int height, uint8_t* alpha) {
	memset(alpha, 0, (size_t)width * height);
	const uint8_t* row = data;
	for (int y = 0; y < height; y++) {
		if ((size_t)(end - row) < 4)
			return 0;
		uint32_t row_size = xwa2d_u32(row);
		if (row_size <= 4 || row_size > (uint32_t)(end - row))
			return 0;
		const uint8_t* p = row + 4;
		const uint8_t* row_end = row + row_size;
		int x = 0;
		while (p < row_end) {
			uint8_t token = *p++;
			if (token == 0x80)
				break;
			if (token & 0x80u) {
				int length = token & 0x7f;
				if (length > row_end - p)
					return 0;
				for (int i = 0; i < length && x + i < width; i++)
					alpha[(size_t)y * width + x + i] = font_alpha(p[i]);
				p += length;
				x += length;
			} else if (token & 0x40u) {
				x += token & 0x3f;
			} else {
				if (!token || p >= row_end)
					return 0;
				uint8_t value = font_alpha(*p++);
				for (int i = 0; i < token && x + i < width; i++)
					alpha[(size_t)y * width + x + i] = value;
				x += token;
			}
		}
		row = row_end;
	}
	return 1;
}

int Xwa2d_DecodeAbpFont(const uint8_t* bytes, size_t size, Xwa2dFontAtlas* out, char* error,
						size_t error_size) {
	if (!bytes || !out || size < ABP_HEADER_SIZE)
		return xwa2d_fail(error, error_size, "invalid ABP input");
	memset(out, 0, sizeof *out);
	uint32_t blob_size = xwa2d_u32(bytes);
	if (blob_size > size - ABP_HEADER_SIZE)
		return xwa2d_fail(error, error_size, "truncated ABP glyph data");
	int cell_width = 1;
	int cell_height = 1;
	for (int i = 0; i < 256; i++) {
		if (bytes[0x504 + i] > cell_width)
			cell_width = bytes[0x504 + i];
		if (bytes[0x404 + i] > cell_height)
			cell_height = bytes[0x404 + i];
	}
	int stride_width = cell_width + 2 * FONT_GUTTER;
	int stride_height = cell_height + 2 * FONT_GUTTER;
	out->width = FONT_COLUMNS * stride_width;
	out->height = 16 * stride_height;
	out->cell_width = cell_width;
	out->cell_height = cell_height;
	out->baseline = cell_height;
	out->first_char = 0;
	out->glyph_count = 256;
	out->rgba = (uint8_t*)calloc((size_t)out->width * out->height, 4);
	out->glyphs = (Xwa2dGlyph*)calloc(256, sizeof *out->glyphs);
	uint8_t* alpha = (uint8_t*)malloc((size_t)cell_width * cell_height);
	if (!out->rgba || !out->glyphs || !alpha)
		goto oom;
	const uint8_t* blob = bytes + ABP_HEADER_SIZE;
	const uint8_t* blob_end = blob + blob_size;
	uint8_t spacing = bytes[0x609];
	for (int glyph = 0; glyph < 256; glyph++) {
		int width = bytes[0x504 + glyph];
		int height = bytes[0x404 + glyph];
		uint32_t offset = xwa2d_u32(bytes + 4 + 4 * glyph);
		Xwa2dGlyph* metric = &out->glyphs[glyph];
		metric->x = (uint16_t)((glyph % 16) * stride_width + FONT_GUTTER);
		metric->y = (uint16_t)((glyph / 16) * stride_height + FONT_GUTTER);
		metric->width = (uint16_t)width;
		metric->height = (uint16_t)height;
		metric->advance = (uint16_t)(width + spacing);
		if (!width || !height)
			continue;
		if (offset >= blob_size || !decode_glyph(blob + offset, blob_end, width, height, alpha))
			goto malformed;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				uint8_t coverage = alpha[(size_t)y * width + x];
				uint8_t* pixel = out->rgba + (((size_t)metric->y + y) * out->width + metric->x + x) * 4;
				pixel[0] = pixel[1] = pixel[2] = 0xff;
				pixel[3] = coverage;
			}
		}
	}
	free(alpha);
	return 1;

oom:
	free(alpha);
	Xwa2dFontAtlas_Free(out);
	return xwa2d_fail(error, error_size, "ABP allocation failed");
malformed:
	free(alpha);
	Xwa2dFontAtlas_Free(out);
	return xwa2d_fail(error, error_size, "malformed ABP glyph data");
}

static int flight_glyph_advance(const Xwa2dFrame* source, int glyph, int size, int padding) {
	const int index = glyph - FLIGHT_FONT_FIRST_GLYPH;
	const int sx = size * (index % FLIGHT_FONT_COLUMNS);
	const int sy = size * (index / FLIGHT_FONT_COLUMNS);
	for (int x = size - 1; x >= 0; x--) {
		for (int y = 0; y < size; y++) {
			if (source->rgba[((size_t)(sy + y) * source->width + sx + x) * 4u + 3] != 0)
				return x + 1 + padding;
		}
	}
	return size / 4 + padding;
}

int Xwa2d_BuildFlightFontTier(const Xwa2dFrameSet* group, int tier, Xwa2dFontAtlas* out, char* error,
							  size_t error_size) {
	if (!group || !out || tier < 0 || tier >= 3 || group->count < 3)
		return xwa2d_fail(error, error_size, "invalid flight font tier input");
	memset(out, 0, sizeof *out);
	const FlightFontTierDesc* desc = &flight_font_tiers[tier];
	const Xwa2dFrame* source = &group->frames[desc->source_index];
	if (source->width < desc->glyph_size * FLIGHT_FONT_COLUMNS ||
		source->height < desc->glyph_size * FLIGHT_FONT_ROWS)
		return xwa2d_fail(error, error_size, "flight font source is too small");
	const int stride = desc->glyph_size + 2 * FLIGHT_FONT_GUTTER;
	out->width = FLIGHT_FONT_COLUMNS * stride;
	out->height = FLIGHT_FONT_ROWS * stride;
	out->cell_width = desc->glyph_size;
	out->cell_height = desc->glyph_size;
	out->baseline = desc->glyph_size;
	out->first_char = 0;
	out->glyph_count = FLIGHT_FONT_GLYPH_COUNT;
	out->rgba = (uint8_t*)calloc((size_t)out->width * out->height, 4u);
	out->glyphs = (Xwa2dGlyph*)calloc(FLIGHT_FONT_GLYPH_COUNT, sizeof *out->glyphs);
	if (!out->rgba || !out->glyphs) {
		Xwa2dFontAtlas_Free(out);
		return xwa2d_fail(error, error_size, "flight font allocation failed");
	}
	for (int glyph = FLIGHT_FONT_FIRST_GLYPH; glyph < FLIGHT_FONT_GLYPH_COUNT; glyph++) {
		const int index = glyph - FLIGHT_FONT_FIRST_GLYPH;
		const int sx = desc->glyph_size * (index % FLIGHT_FONT_COLUMNS);
		const int sy = desc->glyph_size * (index / FLIGHT_FONT_COLUMNS);
		Xwa2dGlyph* metric = &out->glyphs[glyph];
		metric->x = (uint16_t)((index % FLIGHT_FONT_COLUMNS) * stride + FLIGHT_FONT_GUTTER);
		metric->y = (uint16_t)((index / FLIGHT_FONT_COLUMNS) * stride + FLIGHT_FONT_GUTTER);
		metric->width = (uint16_t)desc->glyph_size;
		metric->height = (uint16_t)desc->glyph_size;
		metric->advance =
			(uint16_t)flight_glyph_advance(source, glyph, desc->glyph_size, desc->width_padding);
		for (int y = 0; y < desc->glyph_size; y++) {
			memcpy(out->rgba + ((size_t)(metric->y + y) * out->width + metric->x) * 4u,
				   source->rgba + ((size_t)(sy + y) * source->width + sx) * 4u,
				   (size_t)desc->glyph_size * 4u);
		}
	}
	return 1;
}
