#include "xwa_2d_internal.h"

void Xwa2dFrameSet_Free(Xwa2dFrameSet* set) {
	if (!set)
		return;
	for (int i = 0; i < set->count; i++)
		free(set->frames[i].rgba);
	free(set->frames);
	memset(set, 0, sizeof *set);
}

void Xwa2dFontAtlas_Free(Xwa2dFontAtlas* font) {
	if (!font)
		return;
	free(font->rgba);
	free(font->glyphs);
	memset(font, 0, sizeof *font);
}

int Xwa2d_DecodeIndexedFrame(const uint8_t* pixels, size_t pixel_size, int compressed, int width, int height,
							 int bounds_right, int bounds_bottom, const uint8_t palette[1024],
							 Xwa2dFrame* out, char* error, size_t error_size) {
	if (!out)
		return xwa2d_fail(error, error_size, "invalid indexed frame output");
	memset(out, 0, sizeof *out);
	out->rgba = xwa2d_decode_indexed(pixels, pixel_size, compressed, width, height, bounds_right,
									 bounds_bottom, palette);
	if (!out->rgba)
		return xwa2d_fail(error, error_size, "malformed indexed frame");
	out->width = width;
	out->height = height;
	out->sprite_id = -1;
	return 1;
}

int xwa2d_append_frame(Xwa2dFrameSet* set, Xwa2dFrame* frame) {
	for (int i = 0; i < set->count; i++) {
		if (frame->sprite_id >= 0 && set->frames[i].sprite_id == frame->sprite_id) {
			free(frame->rgba);
			memset(frame, 0, sizeof *frame);
			return 1;
		}
	}
	Xwa2dFrame* frames = (Xwa2dFrame*)realloc(set->frames, (size_t)(set->count + 1) * sizeof *frames);
	if (!frames)
		return 0;
	set->frames = frames;
	set->frames[set->count++] = *frame;
	memset(frame, 0, sizeof *frame);
	return 1;
}

static void indexed_pixel(uint8_t* out, const uint8_t* palette, uint8_t index) {
	if (!index) {
		out[0] = out[1] = out[2] = out[3] = 0;
		return;
	}
	out[0] = palette[4u * index];
	out[1] = palette[4u * index + 1];
	out[2] = palette[4u * index + 2];
	out[3] = 0xff;
}

uint8_t* xwa2d_decode_indexed(const uint8_t* pixels, size_t pixel_size, int compressed, int width, int height,
							  int bounds_right, int bounds_bottom, const uint8_t palette[1024]) {
	if (!pixels || width <= 0 || height <= 0 || (size_t)width > SIZE_MAX / (size_t)height / 4u)
		return NULL;
	uint8_t* rgba = (uint8_t*)calloc((size_t)width * height, 4);
	if (!rgba)
		return NULL;
	if (!compressed) {
		if ((size_t)width * height > pixel_size)
			goto malformed;
		for (size_t i = 0; i < (size_t)width * height; i++)
			indexed_pixel(rgba + 4 * i, palette, pixels[i]);
		return rgba;
	}

	/* Each row is a 4-byte size, then tokens up to 0x80. As the game's
	 * full-width blit does, the size is skipped and the next row starts after
	 * the 0x80: some mods' files give sizes without their own 4 bytes. */
	const uint8_t* p = pixels;
	const uint8_t* end = pixels + pixel_size;
	const int region_width = bounds_right + 1;
	const int region_height = bounds_bottom + 1;
	for (int y = 0; y < region_height && y < height; y++) {
		if ((size_t)(end - p) < 4)
			goto malformed;
		p += 4;
		int x = 0;
		for (;;) {
			if (p >= end)
				goto malformed;
			uint8_t token = *p++;
			if (token == 0x80u)
				break;
			/* Nothing is drawn past the frame's bounds. */
			const int draw = x < region_width;
			if ((token & 0xc0u) == 0x40u) {
				x += token & 0x3fu;
			} else if (token & 0x80u) {
				int length = token & 0x7fu;
				if (length > end - p)
					goto malformed;
				for (int i = 0; draw && i < length && x + i < width; i++)
					indexed_pixel(rgba + ((size_t)y * width + x + i) * 4, palette, p[i]);
				p += length;
				x += length;
			} else {
				int length = token;
				if (!length || p >= end)
					goto malformed;
				uint8_t value = *p++;
				for (int i = 0; draw && i < length && x + i < width; i++)
					indexed_pixel(rgba + ((size_t)y * width + x + i) * 4, palette, value);
				x += length;
			}
		}
	}
	return rgba;

malformed:
	free(rgba);
	return NULL;
}
