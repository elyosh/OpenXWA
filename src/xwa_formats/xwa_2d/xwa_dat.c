#include "xwa_2d_internal.h"

typedef struct DatEntry {
	uint16_t group;
	uint16_t sprite_count;
	uint32_t data_offset;
} DatEntry;

static int dat_directory(const uint8_t* bytes, size_t size, size_t* out_record_size,
						 uint16_t* out_entry_count, const uint8_t** out_entries,
						 char* error, size_t error_size) {
	if (!bytes || size < 10)
		return xwa2d_fail(error, error_size, "invalid DAT input");
	const size_t record_size = xwa2d_u16(bytes + 8) == 0 ? 12u : 24u;
	if (size < 10u + record_size)
		return xwa2d_fail(error, error_size, "truncated DAT directory");
	const uint16_t entry_count = xwa2d_u16(bytes + 10);
	if (entry_count > 512 || (size_t)(entry_count + 1) * record_size > size - 10u)
		return xwa2d_fail(error, error_size, "invalid DAT directory count");
	*out_record_size = record_size;
	*out_entry_count = entry_count;
	*out_entries = bytes + 10u + record_size;
	return 1;
}

int Xwa2d_DatListGroups(const uint8_t* bytes, size_t size, uint16_t* groups, int capacity,
						int* in_out_count, char* error, size_t error_size) {
	if (!groups || !in_out_count || *in_out_count < 0 || *in_out_count > capacity)
		return xwa2d_fail(error, error_size, "invalid DAT group output");
	size_t record_size;
	uint16_t entry_count;
	const uint8_t* entries;
	if (!dat_directory(bytes, size, &record_size, &entry_count, &entries, error, error_size))
		return 0;
	for (uint16_t i = 0; i < entry_count; i++) {
		const uint16_t group = xwa2d_u16(entries + (size_t)i * record_size);
		int found = 0;
		for (int j = 0; j < *in_out_count; j++) {
			if (groups[j] == group) {
				found = 1;
				break;
			}
		}
		if (!found) {
			if (*in_out_count >= capacity)
				return xwa2d_fail(error, error_size, "DAT group output is full");
			groups[(*in_out_count)++] = group;
		}
	}
	return 1;
}

static int dat_insert_frame(Xwa2dFrameSet* set, Xwa2dFrame* frame) {
	int insert = 0;
	while (insert < set->count && set->frames[insert].sprite_id < frame->sprite_id)
		insert++;
	if (insert < set->count && set->frames[insert].sprite_id == frame->sprite_id) {
		free(frame->rgba);
		return 1;
	}
	Xwa2dFrame* grown = (Xwa2dFrame*)realloc(set->frames, (size_t)(set->count + 1) * sizeof *grown);
	if (!grown)
		return 0;
	set->frames = grown;
	memmove(&set->frames[insert + 1], &set->frames[insert],
			(size_t)(set->count - insert) * sizeof *set->frames);
	set->frames[insert] = *frame;
	set->count++;
	for (int i = 0; i < set->count; i++)
		set->frames[i].frame_index = i;
	return 1;
}

int Xwa2d_DecodeDatSprite(const uint8_t* record, size_t record_size, Xwa2dFrame* out, char* error,
						  size_t error_size) {
	if (!record || !out || record_size < 18)
		return xwa2d_fail(error, error_size, "invalid DAT sprite input");
	memset(out, 0, sizeof *out);
	uint16_t type = xwa2d_u16(record);
	int width = xwa2d_u16(record + 2);
	int height = xwa2d_u16(record + 4);
	uint32_t payload_size = xwa2d_u32(record + 14);
	if (width <= 0 || height <= 0 || payload_size > record_size - 18 || payload_size < 44)
		return xwa2d_fail(error, error_size, "invalid DAT sprite header");
	const uint8_t* payload = record + 18;
	uint32_t color_offset = xwa2d_u32(payload + 4);
	uint32_t rows_offset = xwa2d_u32(payload + 8);
	uint32_t color_count = xwa2d_u32(payload + 40);
	if (!color_count || color_count > 256 || color_offset > payload_size || rows_offset >= payload_size ||
		3u * color_count > payload_size - color_offset)
		return xwa2d_fail(error, error_size, "invalid DAT sprite payload");
	const uint8_t* colors = payload + color_offset;
	const uint8_t* p = payload + rows_offset;
	const uint8_t* end = payload + payload_size;
	uint8_t* rgba = (uint8_t*)calloc((size_t)width * height, 4);
	if (!rgba)
		return xwa2d_fail(error, error_size, "DAT sprite allocation failed");

	if (type == 24 || type == 25) {
		if ((size_t)(end - p) < (size_t)width * height * 2u)
			goto malformed;
		for (size_t i = 0; i < (size_t)width * height; i++) {
			uint8_t index = p[2 * i];
			uint8_t alpha = p[2 * i + 1];
			if (index >= color_count || !alpha)
				continue;
			rgba[4 * i] = colors[3u * index];
			rgba[4 * i + 1] = colors[3u * index + 1];
			rgba[4 * i + 2] = colors[3u * index + 2];
			rgba[4 * i + 3] = alpha;
		}
	} else {
		for (int y = 0; y < height; y++) {
			if (p >= end)
				goto malformed;
			int run_count = *p++;
			int x = 0;
			while (run_count-- > 0) {
				if (p >= end)
					goto malformed;
				uint8_t token = *p++;
				int length;
				int mode;
				if (type == 7) {
					length = token & 0x7f;
					mode = token & 0x80 ? 0xc0 : 0;
				} else {
					length = token & 0x3f;
					mode = token & 0xc0;
				}
				if (mode == 0xc0) {
					x += length;
					continue;
				}
				int bytes_per_pixel = mode == 0 ? 1 : 2;
				if ((size_t)(end - p) < (size_t)length * bytes_per_pixel)
					goto malformed;
				for (int i = 0; i < length; i++) {
					uint8_t alpha_code = mode == 0 ? 0xff : p[2 * i];
					uint8_t index = p[bytes_per_pixel * i + bytes_per_pixel - 1];
					if (x + i >= width || index >= color_count || alpha_code < 0x20)
						continue;
					uint8_t* out = rgba + ((size_t)y * width + x + i) * 4;
					out[0] = colors[3u * index];
					out[1] = colors[3u * index + 1];
					out[2] = colors[3u * index + 2];
					out[3] = alpha_code == 0xff ? 0xff : (uint8_t)(((alpha_code >> 5) * 255u) / 8u);
				}
				p += (size_t)length * bytes_per_pixel;
				x += length;
			}
		}
	}
	out->rgba = rgba;
	out->width = width;
	out->height = height;
	out->sprite_id = xwa2d_u16(record + 12);
	out->anchor_x = xwa2d_i32(payload + 24);
	out->anchor_y = xwa2d_i32(payload + 28);
	return 1;

malformed:
	free(rgba);
	return xwa2d_fail(error, error_size, "malformed DAT sprite pixels");
}

int Xwa2d_DatAppendGroup(const uint8_t* bytes, size_t size, uint16_t group, Xwa2dFrameSet* in_out,
						 char* error, size_t error_size) {
	if (!in_out)
		return xwa2d_fail(error, error_size, "invalid DAT group output");
	size_t record_size;
	uint16_t entry_count;
	const uint8_t* entries;
	if (!dat_directory(bytes, size, &record_size, &entry_count, &entries, error, error_size))
		return 0;
	DatEntry match = { 0 };
	int found = 0;
	for (uint16_t i = 0; i < entry_count; i++) {
		const uint8_t* entry = entries + (size_t)i * record_size;
		if (xwa2d_u16(entry) != group)
			continue;
		match.group = group;
		match.sprite_count = xwa2d_u16(entry + 2);
		match.data_offset = xwa2d_u32(entry + record_size - 4);
		found = 1;
		break;
	}
	if (!found)
		return 1;
	size_t cursor = 10u + (size_t)(entry_count + 1) * record_size;
	if (match.data_offset > size - cursor)
		return xwa2d_fail(error, error_size, "invalid DAT group offset");
	cursor += match.data_offset;
	for (uint16_t i = 0; i < match.sprite_count; i++) {
		if (size - cursor < 18)
			return xwa2d_fail(error, error_size, "truncated DAT sprite header");
		const uint8_t* record = bytes + cursor;
		uint32_t payload_size = xwa2d_u32(record + 14);
		if (payload_size > size - cursor - 18u)
			return xwa2d_fail(error, error_size, "truncated DAT sprite");
		cursor += 18u + payload_size;
		if (xwa2d_u16(record + 10) != group)
			continue;
		Xwa2dFrame frame = { 0 };
		if (!Xwa2d_DecodeDatSprite(record, 18u + payload_size, &frame, error, error_size))
			return 0;
		if (!dat_insert_frame(in_out, &frame)) {
			free(frame.rgba);
			return xwa2d_fail(error, error_size, "DAT allocation failed");
		}
	}
	return 1;
}
