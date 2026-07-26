#include "xwa_2d_internal.h"

#define CBM_DESC_SIZE 36u
#define CBM_IMAGE_HEADER_SIZE 2084u

static int bmp_decode_rle8(const uint8_t* data, size_t size, uint8_t* indices, int width, int height,
						   int top_down) {
	const uint8_t* p = data;
	const uint8_t* end = data + size;
	int x = 0;
	int y = top_down ? 0 : height - 1;
	const int direction = top_down ? 1 : -1;
	while (p < end && y >= 0 && y < height) {
		if ((size_t)(end - p) < 2)
			return 0;
		const int count = *p++;
		const int value = *p++;
		if (count) {
			if (x + count > width)
				return 0;
			memset(indices + (size_t)y * width + x, value, (size_t)count);
			x += count;
		} else if (value == 0) {
			x = 0;
			y += direction;
		} else if (value == 1) {
			return 1;
		} else if (value == 2) {
			if ((size_t)(end - p) < 2)
				return 0;
			x += *p++;
			y += direction * *p++;
			if (x > width || y < 0 || y >= height)
				return 0;
		} else {
			const size_t literal = (size_t)value;
			const size_t encoded = literal + (literal & 1u);
			if (encoded > (size_t)(end - p) || x + value > width)
				return 0;
			memcpy(indices + (size_t)y * width + x, p, literal);
			p += encoded;
			x += value;
		}
	}
	return y < 0 || y >= height;
}

int Xwa2d_DecodeCbm(const uint8_t* bytes, size_t size, Xwa2dFrameSet* out, char* error, size_t error_size) {
	if (!bytes || !out || size < CBM_DESC_SIZE)
		return xwa2d_fail(error, error_size, "invalid CBM input");
	memset(out, 0, sizeof *out);
	int frame_count = xwa2d_i32(bytes);
	if (frame_count <= 0 || frame_count > 4096)
		return xwa2d_fail(error, error_size, "invalid CBM frame count");
	size_t offset = CBM_DESC_SIZE;
	for (int i = 0; i < frame_count; i++) {
		if (size - offset < CBM_IMAGE_HEADER_SIZE)
			goto malformed;
		const uint8_t* header = bytes + offset;
		int width = xwa2d_i32(header);
		int height = xwa2d_i32(header + 4);
		int compressed = xwa2d_i32(header + 8);
		int pixel_count = xwa2d_i32(header + 12);
		int bounds_right = xwa2d_i32(header + 24);
		int bounds_bottom = xwa2d_i32(header + 28);
		offset += CBM_IMAGE_HEADER_SIZE;
		if (width <= 0 || height <= 0 || pixel_count <= 0 || (size_t)pixel_count > size - offset)
			goto malformed;
		Xwa2dFrame frame = { 0 };
		if (!Xwa2d_DecodeIndexedFrame(bytes + offset, (size_t)pixel_count, compressed, width, height,
									  bounds_right, bounds_bottom, header + 1060, &frame, error, error_size))
			goto malformed;
		frame.frame_index = i;
		frame.sprite_id = -1;
		if (!xwa2d_append_frame(out, &frame)) {
			free(frame.rgba);
			goto oom;
		}
		offset += (size_t)pixel_count;
	}
	return 1;

oom:
	Xwa2dFrameSet_Free(out);
	return xwa2d_fail(error, error_size, "CBM allocation failed");
malformed:
	Xwa2dFrameSet_Free(out);
	return xwa2d_fail(error, error_size, "malformed CBM resource");
}

int Xwa2d_DecodeBmp(const uint8_t* bytes, size_t size, Xwa2dFrameSet* out, char* error, size_t error_size) {
	if (!bytes || !out || size < 54 || bytes[0] != 'B' || bytes[1] != 'M')
		return xwa2d_fail(error, error_size, "invalid BMP input");
	memset(out, 0, sizeof *out);
	uint32_t pixel_offset = xwa2d_u32(bytes + 10);
	uint32_t dib_size = xwa2d_u32(bytes + 14);
	if (dib_size < 40 || 14u + dib_size > size)
		return xwa2d_fail(error, error_size, "unsupported BMP header");
	int width = xwa2d_i32(bytes + 18);
	int signed_height = xwa2d_i32(bytes + 22);
	uint16_t planes = xwa2d_u16(bytes + 26);
	uint16_t bpp = xwa2d_u16(bytes + 28);
	uint32_t compression = xwa2d_u32(bytes + 30);
	if (width <= 0 || signed_height == 0 || planes != 1 ||
		(compression != 0 && !(compression == 1 && bpp == 8)) || (bpp != 4 && bpp != 8))
		return xwa2d_fail(error, error_size, "unsupported BMP layout");
	int height = signed_height < 0 ? -signed_height : signed_height;
	uint32_t color_count = xwa2d_u32(bytes + 46);
	if (!color_count)
		color_count = 1u << bpp;
	if (color_count > 256 || 14u + dib_size + color_count * 4u > size || pixel_offset > size)
		return xwa2d_fail(error, error_size, "malformed BMP palette");
	uint8_t palette[1024] = { 0 };
	const uint8_t* disk_palette = bytes + 14u + dib_size;
	for (uint32_t i = 0; i < color_count; i++) {
		palette[4 * i] = disk_palette[4 * i + 2];
		palette[4 * i + 1] = disk_palette[4 * i + 1];
		palette[4 * i + 2] = disk_palette[4 * i];
	}
	uint8_t* indices = (uint8_t*)malloc((size_t)width * height);
	if (!indices)
		return xwa2d_fail(error, error_size, "BMP allocation failed");
	memset(indices, 0, (size_t)width * height);
	if (compression == 1) {
		if (!bmp_decode_rle8(bytes + pixel_offset, size - pixel_offset, indices, width, height,
							 signed_height < 0)) {
			free(indices);
			return xwa2d_fail(error, error_size, "malformed BMP RLE8 pixels");
		}
	} else {
		const size_t row_stride = ((size_t)width * bpp + 31u) / 32u * 4u;
		if (row_stride > SIZE_MAX / (size_t)height || row_stride * (size_t)height > size - pixel_offset) {
			free(indices);
			return xwa2d_fail(error, error_size, "truncated BMP pixels");
		}
		for (int y = 0; y < height; y++) {
			int source_y = signed_height > 0 ? height - 1 - y : y;
			const uint8_t* row = bytes + pixel_offset + (size_t)source_y * row_stride;
			for (int x = 0; x < width; x++)
				indices[(size_t)y * width + x] =
					bpp == 8 ? row[x] : (uint8_t)((row[x / 2] >> ((x & 1) ? 0 : 4)) & 0x0f);
		}
	}
	Xwa2dFrame frame = { 0 };
	const int decoded = Xwa2d_DecodeIndexedFrame(indices, (size_t)width * height, 0, width, height, width - 1,
												 height - 1, palette, &frame, error, error_size);
	free(indices);
	if (!decoded)
		return 0;
	if (!xwa2d_append_frame(out, &frame)) {
		free(frame.rgba);
		return xwa2d_fail(error, error_size, "BMP allocation failed");
	}
	return 1;
}
