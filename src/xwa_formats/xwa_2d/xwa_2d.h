#ifndef XWA_2D_FORMAT_H
#define XWA_2D_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Xwa2dFrame {
	uint8_t* rgba;
	int width;
	int height;
	int frame_index;
	int sprite_id;
	int anchor_x;
	int anchor_y;
} Xwa2dFrame;

typedef struct Xwa2dFrameSet {
	Xwa2dFrame* frames;
	int count;
} Xwa2dFrameSet;

typedef struct Xwa2dGlyph {
	uint16_t x, y, width, height, advance;
} Xwa2dGlyph;

typedef struct Xwa2dFontAtlas {
	uint8_t* rgba;
	Xwa2dGlyph* glyphs;
	int width, height;
	int cell_width, cell_height;
	int baseline;
	uint16_t first_char, glyph_count;
} Xwa2dFontAtlas;

void Xwa2dFrameSet_Free(Xwa2dFrameSet* set);
void Xwa2dFontAtlas_Free(Xwa2dFontAtlas* font);

int Xwa2d_DecodeCbm(const uint8_t* bytes, size_t size, Xwa2dFrameSet* out, char* error, size_t error_size);
int Xwa2d_DecodeBmp(const uint8_t* bytes, size_t size, Xwa2dFrameSet* out, char* error, size_t error_size);
int Xwa2d_DecodeFlc(const uint8_t* bytes, size_t size, Xwa2dFrameSet* out, char* error, size_t error_size);

/* Append every record for `group` from one DAT file. Calling this for each
 * RESDATA.TXT entry merges split groups and ignores duplicate sprite IDs. */
int Xwa2d_DatAppendGroup(const uint8_t* bytes, size_t size, uint16_t group, Xwa2dFrameSet* in_out,
						 char* error, size_t error_size);
/* Appends unique DAT directory group IDs to `groups`; `in_out_count` is both
 * the initial count and resulting count. */
int Xwa2d_DatListGroups(const uint8_t* bytes, size_t size, uint16_t* groups, int capacity,
						int* in_out_count, char* error, size_t error_size);

int Xwa2d_DecodeAbpFont(const uint8_t* bytes, size_t size, Xwa2dFontAtlas* out, char* error,
						size_t error_size);
int Xwa2d_BuildFlightFontTier(const Xwa2dFrameSet* group, int tier, Xwa2dFontAtlas* out, char* error,
							  size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
