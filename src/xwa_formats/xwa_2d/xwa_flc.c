#include "xwa_2d_internal.h"

typedef struct FlcCursor {
	const uint8_t* p;
	const uint8_t* end;
} FlcCursor;

static int flc_take(FlcCursor* c, size_t size, const uint8_t** out) {
	if (size > (size_t)(c->end - c->p))
		return 0;
	*out = c->p;
	c->p += size;
	return 1;
}

static int flc_palette(const uint8_t* data, size_t size, uint8_t palette[1024]) {
	FlcCursor c = { data, data + size };
	const uint8_t* p;
	if (!flc_take(&c, 2, &p))
		return 0;
	int packets = xwa2d_u16(p);
	int index = 0;
	for (int packet = 0; packet < packets; packet++) {
		if (!flc_take(&c, 2, &p))
			return 0;
		index += p[0];
		int count = p[1] ? p[1] : 256;
		if (index < 0 || index + count > 256 || !flc_take(&c, (size_t)count * 3u, &p))
			return 0;
		for (int i = 0; i < count; i++) {
			palette[4 * (index + i)] = p[3 * i];
			palette[4 * (index + i) + 1] = p[3 * i + 1];
			palette[4 * (index + i) + 2] = p[3 * i + 2];
		}
		index += count;
	}
	return 1;
}

static int flc_brun(const uint8_t* data, size_t size, uint8_t* pixels, int width, int height) {
	FlcCursor c = { data, data + size };
	for (int y = 0; y < height; y++) {
		const uint8_t* p;
		if (!flc_take(&c, 1, &p))
			return 0;
		int x = 0;
		while (x < width) {
			if (!flc_take(&c, 1, &p))
				return 0;
			int count = (int8_t)p[0];
			if (count >= 0) {
				if (!flc_take(&c, 1, &p) || x + count > width)
					return 0;
				memset(pixels + (size_t)y * width + x, p[0], (size_t)count);
				x += count;
			} else {
				int literal = -count;
				if (x + literal > width || !flc_take(&c, (size_t)literal, &p))
					return 0;
				memcpy(pixels + (size_t)y * width + x, p, (size_t)literal);
				x += literal;
			}
		}
	}
	return 1;
}

static int flc_delta_fli(const uint8_t* data, size_t size, uint8_t* pixels, int width, int height) {
	FlcCursor c = { data, data + size };
	const uint8_t* p;
	if (!flc_take(&c, 4, &p))
		return 0;
	int first_line = xwa2d_u16(p);
	int line_count = xwa2d_i16(p + 2);
	if (first_line < 0 || line_count < 0 || first_line + line_count > height)
		return 0;
	for (int line = 0; line < line_count; line++) {
		if (!flc_take(&c, 1, &p))
			return 0;
		int packets = p[0];
		int x = 0;
		for (int packet = 0; packet < packets; packet++) {
			if (!flc_take(&c, 2, &p))
				return 0;
			x += p[0];
			int count = (int8_t)p[1];
			if (x > width)
				return 0;
			if (count < 0) {
				int run = -count;
				if (!flc_take(&c, 1, &p) || x + run > width)
					return 0;
				memset(pixels + (size_t)(first_line + line) * width + x, p[0], (size_t)run);
				x += run;
			} else {
				if (x + count > width || !flc_take(&c, (size_t)count, &p))
					return 0;
				memcpy(pixels + (size_t)(first_line + line) * width + x, p, (size_t)count);
				x += count;
			}
		}
	}
	return 1;
}

static int flc_delta_flc(const uint8_t* data, size_t size, uint8_t* pixels, int width, int height) {
	FlcCursor c = { data, data + size };
	const uint8_t* p;
	if (!flc_take(&c, 2, &p))
		return 0;
	int changed_lines = xwa2d_u16(p);
	int y = 0;
	for (int changed = 0; changed < changed_lines; changed++) {
		int packet_count = -1;
		int last_byte = -1;
		while (packet_count < 0) {
			if (!flc_take(&c, 2, &p))
				return 0;
			uint16_t opcode = xwa2d_u16(p);
			switch (opcode >> 14) {
				case 0:
					packet_count = (int16_t)opcode;
					break;
				case 2:
					last_byte = opcode & 0xff;
					break;
				case 3:
					y -= (int16_t)opcode;
					break;
				default:
					break;
			}
		}
		if (y < 0 || y >= height)
			return 0;
		int x = 0;
		for (int packet = 0; packet < packet_count; packet++) {
			if (!flc_take(&c, 2, &p))
				return 0;
			x += p[0];
			int count = (int8_t)p[1];
			if (count < 0) {
				int run = -count;
				if (!flc_take(&c, 2, &p) || x + 2 * run > width)
					return 0;
				for (int i = 0; i < run; i++) {
					pixels[(size_t)y * width + x++] = p[0];
					pixels[(size_t)y * width + x++] = p[1];
				}
			} else {
				int literal = 2 * count;
				if (x + literal > width || !flc_take(&c, (size_t)literal, &p))
					return 0;
				memcpy(pixels + (size_t)y * width + x, p, (size_t)literal);
				x += literal;
			}
		}
		if (last_byte >= 0)
			pixels[(size_t)y * width + width - 1] = (uint8_t)last_byte;
		y++;
	}
	return 1;
}

int Xwa2d_DecodeFlc(const uint8_t* bytes, size_t size, Xwa2dFrameSet* out, char* error, size_t error_size) {
	if (!bytes || !out || size < 128)
		return xwa2d_fail(error, error_size, "invalid FLC input");
	memset(out, 0, sizeof *out);
	uint16_t magic = xwa2d_u16(bytes + 4);
	int frame_count = xwa2d_u16(bytes + 6);
	int width = xwa2d_u16(bytes + 8);
	int height = xwa2d_u16(bytes + 10);
	if ((magic != 0xaf11 && magic != 0xaf12) || frame_count <= 0 || width <= 0 || height <= 0)
		return xwa2d_fail(error, error_size, "unsupported FLC header");
	uint8_t palette[1024] = { 0 };
	uint8_t* indices = (uint8_t*)calloc((size_t)width * height, 1);
	if (!indices)
		return xwa2d_fail(error, error_size, "FLC allocation failed");
	size_t cursor = 128;
	for (int frame_index = 0; frame_index < frame_count;) {
		if (size - cursor < 16)
			goto malformed;
		uint32_t frame_size = xwa2d_u32(bytes + cursor);
		uint16_t frame_magic = xwa2d_u16(bytes + cursor + 4);
		uint16_t chunks = xwa2d_u16(bytes + cursor + 6);
		if (frame_size < 16 || frame_size > size - cursor)
			goto malformed;
		if (frame_magic != 0xf1fa) {
			cursor += frame_size;
			continue;
		}
		size_t chunk_cursor = cursor + 16;
		for (uint16_t chunk = 0; chunk < chunks; chunk++) {
			if (cursor + frame_size - chunk_cursor < 6)
				goto malformed;
			uint32_t chunk_size = xwa2d_u32(bytes + chunk_cursor);
			uint16_t type = xwa2d_u16(bytes + chunk_cursor + 4);
			if (chunk_size < 6 || chunk_size > cursor + frame_size - chunk_cursor)
				goto malformed;
			const uint8_t* data = bytes + chunk_cursor + 6;
			size_t data_size = chunk_size - 6;
			int ok = 1;
			switch (type) {
				case 4:
				case 11:
					ok = flc_palette(data, data_size, palette);
					break;
				case 7:
					ok = flc_delta_flc(data, data_size, indices, width, height);
					break;
				case 12:
					ok = flc_delta_fli(data, data_size, indices, width, height);
					break;
				case 13:
					memset(indices, 0, (size_t)width * height);
					break;
				case 15:
					ok = flc_brun(data, data_size, indices, width, height);
					break;
				case 16:
					if (data_size < (size_t)width * height)
						ok = 0;
					else
						memcpy(indices, data, (size_t)width * height);
					break;
				default:
					break;
			}
			if (!ok)
				goto malformed;
			chunk_cursor += chunk_size;
			if ((chunk_size & 1u) != 0 && ((chunk_size ^ frame_size) & 1u) != 0)
				chunk_cursor++;
			if (chunk_cursor > cursor + frame_size)
				goto malformed;
		}
		Xwa2dFrame frame = { 0 };
		frame.rgba = xwa2d_decode_indexed(indices, (size_t)width * height, 0, width, height, width - 1,
										  height - 1, palette);
		if (!frame.rgba)
			goto malformed;
		frame.width = width;
		frame.height = height;
		frame.frame_index = frame_index;
		frame.sprite_id = -1;
		if (!xwa2d_append_frame(out, &frame)) {
			free(frame.rgba);
			goto oom;
		}
		frame_index++;
		cursor += frame_size;
	}
	free(indices);
	return 1;

oom:
	free(indices);
	Xwa2dFrameSet_Free(out);
	return xwa2d_fail(error, error_size, "FLC allocation failed");
malformed:
	free(indices);
	Xwa2dFrameSet_Free(out);
	return xwa2d_fail(error, error_size, "malformed FLC animation");
}
