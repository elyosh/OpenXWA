#include "xwa/frontend/frontend_image.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include "aeron/log.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/sprite_resource.h"
#include "xwa/audio/music.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/memory.h"
#include "xwa/util/string.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	FRONT_IMAGE_RESOURCE_DESCRIPTOR_FILE_SIZE = 36,
	FRONT_IMAGE_RESOURCE_FILE_SIZE = 2084,
	FRONT_IMAGE_PATH_SIZE = 512,
	FRONT_IMAGE_RLE_ROW_BUFFER_SIZE = 0x10004,
};

typedef struct FlicState {
	XwaFile* file;
	uint8_t headerPrefix[4];
	uint16_t magic;
	uint16_t frameCount;
	uint16_t width;
	uint16_t height;
	uint8_t headerRest[124];
	uint16_t currentFrame;
} FlicState;

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define XWA_FRONT_IMAGE_PACKED_STRUCT
#else
#define XWA_FRONT_IMAGE_PACKED_STRUCT __attribute__((packed))
#endif

typedef struct XWA_FRONT_IMAGE_PACKED_STRUCT FrontImageSpritePayload {
	uint32_t payloadSize;
	uint32_t colorTable24Offset;
	uint32_t rowDataOffset;
	uint32_t palette16Offset;
	uint32_t field22;
	uint32_t field26;
	uint32_t anchorX;
	uint32_t anchorY;
	uint32_t field32;
	uint32_t field36;
	uint32_t colorCount;
} FrontImageSpritePayload;

struct XWA_FRONT_IMAGE_PACKED_STRUCT FrontImageBmpFileHeader {
	uint16_t type;
	uint32_t fileSize;
	uint16_t reserved1;
	uint16_t reserved2;
	uint32_t pixelOffset;
};

typedef struct FrontImageBmpPaletteEntry {
	unsigned char blue;
	unsigned char green;
	unsigned char red;
	unsigned char reserved;
} FrontImageBmpPaletteEntry;

typedef struct FrontImageFlicPaletteEntry {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	unsigned char reserved;
} FrontImageFlicPaletteEntry;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef XWA_FRONT_IMAGE_PACKED_STRUCT

static ResourceEntry g_resourceTableStorage[FRONT_IMAGE_MAX_RESOURCES];
// GLOBAL: XWA 0x9F60E0
unsigned char g_rleRowBuffer[FRONT_IMAGE_RLE_ROW_BUFFER_SIZE];
// GLOBAL: XWA 0x782DE8
static uint16_t g_paletteRemapCache[256];

// GLOBAL: XWA 0x9F65E1
ResourceEntry* g_resourceTable = g_resourceTableStorage;
// GLOBAL: XWA 0x9F65E5
int g_resourceCount = 0;
// GLOBAL: XWA 0xABC954
int g_bmpSaveEnabled = 0;
// GLOBAL: XWA 0x7838A8
uint16_t* g_atlasBlendLut = NULL;
// GLOBAL: XWA 0x7838B4
int g_atlasSpriteWidth = 0;
// GLOBAL: XWA 0x7838B8
int g_atlasSpriteHeight = 0;
// GLOBAL: XWA 0x7838B0
const unsigned char* g_atlasSpriteRows = NULL;
// GLOBAL: XWA 0x7838AC
const unsigned char* g_atlasColorTable = NULL;
// GLOBAL: XWA 0x7838BC
int g_atlasRunRemaining = 0;

static int FrontImage_StrCaseEqual(const char* lhs, const char* rhs) {
	while (*lhs != '\0' && *rhs != '\0') {
		if (tolower((unsigned char)*lhs) != tolower((unsigned char)*rhs)) {
			return 0;
		}

		++lhs;
		++rhs;
	}

	return *lhs == *rhs;
}

#ifdef XWA_MODERN
static void FrontImage_NoteSnapshotResourceBinding(const char* fileName, const char* name) {
	const int index = FrontImage_FindResourceByName(name);
	if (index >= 0 && g_resourceTable[index].desc) {
		XwaSnapshot_NoteResourceBinding(fileName, name, g_resourceTable[index].desc->frameCount);
	}
}
#endif

static int FrontImage_HasExtension(const char* fileName, const char* extension) {
	size_t fileNameLength;
	size_t extensionLength;

	if (fileName == NULL || extension == NULL) {
		return 0;
	}

	fileNameLength = strlen(fileName);
	extensionLength = strlen(extension);
	if (fileNameLength < extensionLength) {
		return 0;
	}

	return FrontImage_StrCaseEqual(fileName + fileNameLength - extensionLength, extension);
}

static void FrontImage_CopyUppercasePath(char* dest, size_t destSize, const char* src) {
	size_t i;

	if (destSize == 0) {
		return;
	}

	for (i = 0; i + 1 < destSize && src[i] != '\0'; ++i) {
		dest[i] = (char)toupper((unsigned char)src[i]);
	}

	dest[i] = '\0';
}

static XwaFile* FrontImage_OpenRead(AeronVfsRoot root, const char* path) {
	char uppercase[FRONT_IMAGE_PATH_SIZE];
	XwaFile* stream;

	stream = File_Open(root, path, "rb");
	if (stream != NULL) {
		return stream;
	}

	FrontImage_CopyUppercasePath(uppercase, sizeof(uppercase), path);
	return File_Open(root, uppercase, "rb");
}

static void FrontImage_MakeCbmPath(const char* srcFile, char* outPath, size_t outPathSize) {
	size_t length;

	snprintf(outPath, outPathSize, "%s", srcFile);
	length = strlen(outPath);
	if (length >= 4) {
		outPath[length - 4] = '\0';
	}

	length = strlen(outPath);
	if (length + 5 <= outPathSize) {
		memcpy(outPath + length, ".cbm", 5);
	}
}

static void FrontImage_ReadRect(const unsigned char* bytes, FrontendRect* rect) {
	rect->left = ByteOrder_ReadI32Le(bytes);
	rect->top = ByteOrder_ReadI32Le(bytes + 4);
	rect->right = ByteOrder_ReadI32Le(bytes + 8);
	rect->bottom = ByteOrder_ReadI32Le(bytes + 12);
}

static void FrontImage_WriteRect(unsigned char* bytes, const FrontendRect* rect) {
	ByteOrder_WriteU32Le(bytes, (uint32_t)rect->left);
	ByteOrder_WriteU32Le(bytes + 4, (uint32_t)rect->top);
	ByteOrder_WriteU32Le(bytes + 8, (uint32_t)rect->right);
	ByteOrder_WriteU32Le(bytes + 12, (uint32_t)rect->bottom);
}

static inline void FrontImage_WriteSurface16(unsigned char* dest, int value) {
#ifndef XWA_MODERN
	*(uint16_t*)dest = (uint16_t)value;
#else
	ByteOrder_WriteU16Le(dest, (uint16_t)value);
#endif
}

static inline uint16_t FrontImage_ReadSurface16(const unsigned char* src) {
#ifndef XWA_MODERN
	return *(const uint16_t*)src;
#else
	return ByteOrder_ReadU16Le(src);
#endif
}

static inline void FrontImage_WriteSurface16Lut(unsigned char* dest, const int* value) {
#ifndef XWA_MODERN
	*(uint16_t*)dest = *(const uint16_t*)value;
#else
	ByteOrder_WriteU16Le(dest, (uint16_t)*value);
#endif
}

static inline int FrontImage_ReadRleRowLength(const unsigned char* row) {
#ifndef XWA_MODERN
	return *(const int*)row;
#else
	return (int)ByteOrder_ReadU32Le(row);
#endif
}

static inline const unsigned char* FrontImage_GlyphData(const intptr_t* glyph) {
	return (const unsigned char*)(uintptr_t)glyph[8];
}

static inline int FrontImage_ReadGlyphRowLength(const unsigned char* row) {
#ifndef XWA_MODERN
	return *(const int*)row;
#else
	return (int)ByteOrder_ReadU32Le(row);
#endif
}

static inline uint32_t FrontImage_ReadAtlasColor(int index) {
#ifndef XWA_MODERN
	return *(const uint32_t*)(g_atlasColorTable + 2 * (unsigned char)index);
#else
	return ByteOrder_ReadU16Le(g_atlasColorTable + 2 * (unsigned char)index);
#endif
}

static inline void FrontImage_WriteAtlasColor(unsigned char* dest, int color) {
	dest[0] = (unsigned char)color;
	dest[1] = (unsigned char)((unsigned int)color >> 8);
}

static inline const unsigned char* FrontImage_SkipAtlasRowRuns(const unsigned char* row) {
	unsigned char runCount;
	unsigned char runCountMinusOne;

	runCount = *row++;
	runCountMinusOne = (unsigned char)(runCount - 1);
	if (runCount != 0) {
		int remainingRuns;

		remainingRuns = runCountMinusOne + 1;
		do {
			int control;
			int runLength;
			int mode;

			control = *row++;
			runLength = control & 0x3f;
			mode = control & 0xc0;
			if (mode == 0) {
				row += runLength;
			} else if (mode == 0x80) {
				row += 2 * runLength;
			}
			--remainingRuns;
		} while (remainingRuns != 0);
	}

	return row;
}

static int FrontImage_FlicReadHeader(XwaFile* stream, FlicState* state) {
	unsigned char header[128];

	File_ReadCount(stream, header, sizeof(header));
	memcpy(state->headerPrefix, header, sizeof(state->headerPrefix));
	state->magic = ByteOrder_ReadU16Le(header + 4);
	state->frameCount = ByteOrder_ReadU16Le(header + 6);
	state->width = ByteOrder_ReadU16Le(header + 8);
	state->height = ByteOrder_ReadU16Le(header + 10);
	state->currentFrame = 0;
	memcpy(state->headerRest, header + 12, sizeof(state->headerRest));
	return 1;
}

// FUNCTION: XWA 0x5747A0
int FlicChunk_Color(XwaFile* stream, unsigned char* paletteRgbx) {
	int16_t packetCount;
	unsigned char value;
	int16_t currentIndex;
	int consumed;
	int packetIndex;

#ifdef XWA_MODERN
	packetCount = 0;
#endif
	File_ReadWord(stream, &packetCount);
	currentIndex = 0;
	consumed = 8;
	packetIndex = 0;
	if (packetCount > 0) {
		do {
			int16_t colorCount;

#ifdef XWA_MODERN
			value = 0;
#endif
			File_ReadByte(stream, &value);
			currentIndex += value;
			File_ReadByte(stream, &value);
			colorCount = value;
			if (colorCount == 0) {
				colorCount = 256;
			}

			if (colorCount > 0) {
				unsigned char* out = paletteRgbx + 4 * (int16_t)currentIndex;
				int i = colorCount;

				do {
					File_ReadCount(stream, out, 3);
					out += 4;
					--i;
				} while (i != 0);
			}

			consumed += colorCount + 2 * colorCount + 2;
			currentIndex += colorCount;
			++packetIndex;
		} while ((int16_t)packetIndex < packetCount);
	}

	return consumed;
}

// FUNCTION: XWA 0x5743D0
static int16_t FlicReadInitialPalette(FlicState* state, unsigned char* palette) {
	typedef struct FlicFrameChunkHeader {
		uint32_t size;
		uint16_t type;
	} FlicFrameChunkHeader;
	typedef struct FlicFrameDiskHeader {
		uint32_t size;
		uint16_t magic;
		uint16_t chunkCount;
		uint8_t reserved[8];
	} FlicFrameDiskHeader;

	int found;
	int frameIndex;
	unsigned char* paletteBytes;

	found = 0;
	frameIndex = 0;
	if (state->frameCount > 0) {
		paletteBytes = palette;
		while (1) {
			FlicFrameDiskHeader frameHeader;
			int chunkIndex;

			if ((int16_t)frameIndex >= 1) {
				break;
			}

			File_ReadCount(state->file, &frameHeader, sizeof(frameHeader));
			if (frameHeader.magic == 0xf1fa) {
				chunkIndex = 0;
				if (frameHeader.chunkCount > 0) {
					do {
						FlicFrameChunkHeader chunkHeader;
						int chunkType;
						int consumed;

						File_ReadCount(state->file, &chunkHeader, 6);
						chunkType = chunkHeader.type;
						switch (chunkType) {
							case 4:
								consumed = FlicChunk_Color(state->file, paletteBytes);
								found = 1;
								break;
							case 11:
								consumed = FlicChunk_Color(state->file, paletteBytes);
								found = 1;
								break;
							default:
								consumed = (int)chunkHeader.size;
								File_Seek(state->file, (int)chunkHeader.size - 6, SEEK_CUR);
								break;
						}

						if ((consumed & 1) != 0 && ((consumed ^ (int)frameHeader.size) & 1) != 0) {
							File_Seek(state->file, 1, SEEK_CUR);
							++consumed;
						}

						if (consumed != (int)chunkHeader.size) {
							return 0;
						}

						++chunkIndex;
					} while ((int16_t)chunkIndex < frameHeader.chunkCount);
				}

				++frameIndex;
			} else {
				File_Seek(state->file, (int)frameHeader.size - 16, SEEK_CUR);
			}

			if ((int16_t)frameIndex >= (int)state->frameCount) {
				return found ? 1 : 0;
			}
		}
	}

	return (int16_t)found;
}

// FUNCTION: XWA 0x574360
static int16_t FlicOpen(char* fileName, FlicState* state, void* palette) {
	XwaFile* stream;

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	if (stream != NULL) {
		state->file = stream;
		state->currentFrame = 0;
		FrontImage_FlicReadHeader(stream, state);
		if (state->magic == 0xaf11 || state->magic == 0xaf12) {
			FlicReadInitialPalette(state, palette);
			File_Seek(stream, 128, SEEK_SET);
			return 1;
		}
	}

	return 0;
}

static void FlicClose(FlicState* state) {
	if (state->file != NULL) {
		File_Close(state->file);
		state->file = NULL;
	}
}

// FUNCTION: XWA 0x574870
int FlicChunk_DeltaFlc(XwaFile* stream, const unsigned char* chunkHeader, ImageResource* image) {
	unsigned char* pixels = image->pixels;
	int changedLines;
	int line;
	int changedIndex;
	int consumed;
	int pair;

	(void)chunkHeader;
	File_ReadWord(stream, &changedLines);
	consumed = 8;
	line = 0;
	changedIndex = 0;

	if ((uint16_t)changedLines > 0) {
		do {
			int16_t packetCount = -1;
			int lastByte = -1;
			int offset;

			do {
				int opcode;

				File_ReadWord(stream, &opcode);
				consumed += 2;
				switch ((uint16_t)opcode >> 14) {
					case 0:
						packetCount = (int16_t)opcode;
						break;
					case 2:
						lastByte = opcode & 0xff;
						break;
					case 3:
						line -= opcode;
						break;
				}
			} while (packetCount < 0);

			offset = (int16_t)line * image->width;
			if (packetCount > 0) {
				int packets = packetCount;

				do {
					int buffer;
					int16_t count;

					File_ReadByte(stream, &buffer);
					offset += (unsigned char)buffer;
					File_ReadByte(stream, &buffer);
					count = (unsigned char)buffer;
					if (count > 0x7f) {
						count -= 0x100;
					}
					consumed += 2;
					if (count < 0) {
						int runCount;
						int i;

						File_ReadWord(stream, &pair);
						consumed += 2;
						runCount = -count;
						i = 0;
						if (runCount > 0) {
							do {
								pixels[offset++] = (unsigned char)pair;
								pixels[offset++] = (unsigned char)(pair >> 8);
								++i;
							} while ((int16_t)i < runCount);
						}
					} else if (count > 0) {
						int literalCount = count;

						consumed += 2 * literalCount;
						do {
							File_ReadWord(stream, &pair);
							pixels[offset++] = (unsigned char)pair;
							pixels[offset++] = (unsigned char)(pair >> 8);
							--literalCount;
						} while (literalCount != 0);
					}

					--packets;
				} while (packets != 0);
			}

			if ((int16_t)lastByte >= 0) {
				pixels[offset] = (unsigned char)lastByte;
			}

			++line;
			++changedIndex;
		} while ((int16_t)changedIndex < (int)(uint16_t)changedLines);
	}

	return consumed;
}

// FUNCTION: XWA 0x574A20
static int FlicChunk_DeltaFli(XwaFile* stream, const unsigned char* chunkHeader, ImageResource* image) {
	unsigned char* pixels = image->pixels;
	uint16_t firstLine;
	int16_t lineCount;
	int line;
	int savedLine;
	unsigned char buffer[4];
	int consumed;

	(void)chunkHeader;
	File_ReadWord(stream, &firstLine);
	File_ReadWord(stream, &lineCount);
	line = 0;
	consumed = 10;
	savedLine = 0;

	while ((int16_t)line < lineCount) {
		uint16_t packetCount;
		uint16_t offset;

		File_ReadByte(stream, buffer);
		packetCount = buffer[0];
		++consumed;
		offset = (uint16_t)(firstLine + line);
		offset = (uint16_t)(image->width * offset);
		if ((int16_t)packetCount > 0) {
			int packets = (int16_t)packetCount;

			do {
				File_ReadByte(stream, buffer);
				offset += buffer[0];
				File_ReadByte(stream, buffer);
				line = buffer[0];
				consumed += 2;
				if ((int16_t)line > 0x7f) {
					line -= 0x100;
				}
				if ((int16_t)line < 0) {
					int runCount = -(int16_t)line;

					File_ReadByte(stream, buffer);
					++consumed;
					memset(&pixels[offset], buffer[0], (size_t)runCount);
					offset -= line;
				} else if ((int16_t)line != 0) {
					File_ReadCount(stream, &pixels[offset], (int16_t)line);
					consumed += (int16_t)line;
					offset += line;
				}

				--packets;
			} while (packets != 0);

			line = savedLine;
		}

		++line;
		savedLine = line;
	}

	return consumed;
}

static int FrontImage_FlicChunkBlack(XwaFile* stream, const unsigned char* chunkHeader,
									 ImageResource* image) {
	uint32_t chunkSize = ByteOrder_ReadU32Le(chunkHeader);

	File_Seek(stream, (int)chunkSize - 6, SEEK_CUR);
	memset(image->pixels, 0, (size_t)(image->width * image->height));
	return (int)chunkSize;
}

// FUNCTION: XWA 0x574BC0
int FlicChunk_ByteRun(XwaFile* stream, const unsigned char* chunkHeader, ImageResource* image) {
	unsigned char* pixels = image->pixels;
	int buffer;
	int consumed = 6;
	int row;
	XwaFile* file = stream;

	(void)chunkHeader;
	row = 0;
	if (image->height > 0) {
		do {
			int offset;
			int rowStart;

			File_ReadByte(file, &buffer);
			offset = image->width * (int16_t)row;
			++consumed;
			rowStart = offset;
			if (image->width > 0) {
				do {
					int16_t count;

					File_ReadByte(file, &buffer);
					count = (int8_t)buffer;
					++consumed;

					if (count < 0) {
						File_ReadCount(file, pixels + offset, (size_t)-count);
						consumed += abs(count);
						offset += abs(count);
					} else {
						File_ReadByte(file, &buffer);
						++consumed;
						memset(&pixels[offset], (unsigned char)buffer, (size_t)count);
						offset += count;
					}
				} while (offset - rowStart < image->width);
			}
			++row;
		} while ((int16_t)row < image->height);
	}

	return consumed;
}

static int FrontImage_FlicChunkCopy(XwaFile* stream, const unsigned char* chunkHeader, ImageResource* image) {
	size_t size = (size_t)(image->width * image->height);

	(void)chunkHeader;
	File_ReadCount(stream, image->pixels, size);
	return (int)size + 6;
}

static int FlicDecodeNextFrame(FlicState* state, ImageResource* image, unsigned char* palette) {
	int producedFrame = 0;
	int paletteChanged = 0;
	int currentFrame = (int16_t)state->currentFrame;

	if (currentFrame < (int)state->frameCount) {
		while (!producedFrame) {
			unsigned char frameHeader[16];
			uint32_t frameSize;
			uint16_t frameMagic;
			uint16_t chunkCount;
			int chunkIndex = 0;

			File_ReadCount(state->file, frameHeader, sizeof(frameHeader));
			frameSize = ByteOrder_ReadU32Le(frameHeader);
			frameMagic = ByteOrder_ReadU16Le(frameHeader + 4);
			chunkCount = ByteOrder_ReadU16Le(frameHeader + 6);
			if (frameMagic == 0xf1fa) {
				if (chunkCount != 0) {
					while (1) {
						unsigned char chunkHeader[6];
						uint32_t chunkSize;
						uint16_t chunkType;
						int consumed;

						File_ReadCount(state->file, chunkHeader, sizeof(chunkHeader));
						chunkSize = ByteOrder_ReadU32Le(chunkHeader);
						chunkType = ByteOrder_ReadU16Le(chunkHeader + 4);
						switch ((int16_t)chunkType) {
							case 4:
							case 11:
								consumed = FlicChunk_Color(state->file, palette);
								producedFrame = 1;
								paletteChanged = -1;
								break;
							case 7:
								consumed = FlicChunk_DeltaFlc(state->file, chunkHeader, image);
								producedFrame = 1;
								break;
							case 12:
								consumed = FlicChunk_DeltaFli(state->file, chunkHeader, image);
								producedFrame = 1;
								break;
							case 13:
								consumed = FrontImage_FlicChunkBlack(state->file, chunkHeader, image);
								producedFrame = 1;
								break;
							case 15:
								consumed = FlicChunk_ByteRun(state->file, chunkHeader, image);
								producedFrame = 1;
								break;
							case 16:
								consumed = FrontImage_FlicChunkCopy(state->file, chunkHeader, image);
								producedFrame = 1;
								break;
							default:
								consumed = (int)chunkSize;
								File_Seek(state->file, (int)chunkSize - 6, SEEK_CUR);
								break;
						}

						if ((consumed & 1) != 0 &&
							(((unsigned char)frameSize ^ (unsigned char)consumed) & 1) != 0) {
							File_Seek(state->file, 1, SEEK_CUR);
							++consumed;
						}

						if (consumed != (int)chunkSize) {
							return 0;
						}

						++chunkIndex;
						if ((int16_t)chunkIndex >= (int)chunkCount) {
							break;
						}
					}
				}

				if (chunkCount == 0) {
					producedFrame = 1;
				}

				++currentFrame;
			} else {
				File_Seek(state->file, (int)frameSize - 16, SEEK_CUR);
			}

			if (currentFrame >= (int)state->frameCount) {
				break;
			}
		}
	}

	state->currentFrame = (uint16_t)currentFrame;
	return producedFrame | paletteChanged;
}

static void FrontImage_ReadDescriptorHeader(const unsigned char* bytes, ResourceDescriptor* desc) {
	desc->frameCount = ByteOrder_ReadI32Le(bytes);
	desc->currentFrame = ByteOrder_ReadI32Le(bytes + 4);
	desc->atlasBaseIndex = ByteOrder_ReadI32Le(bytes + 24);
	desc->atlasGroupId = ByteOrder_ReadI32Le(bytes + 28);
	desc->image = NULL;
	FrontImage_ReadRect(bytes + 8, &desc->bounds);
}

static void FrontImage_WriteDescriptorHeader(unsigned char* bytes, const ResourceDescriptor* desc) {
	memset(bytes, 0, FRONT_IMAGE_RESOURCE_DESCRIPTOR_FILE_SIZE);
	ByteOrder_WriteU32Le(bytes, (uint32_t)desc->frameCount);
	ByteOrder_WriteU32Le(bytes + 4, (uint32_t)desc->currentFrame);
	FrontImage_WriteRect(bytes + 8, &desc->bounds);
	ByteOrder_WriteU32Le(bytes + 24, (uint32_t)desc->atlasBaseIndex);
	ByteOrder_WriteU32Le(bytes + 28, (uint32_t)desc->atlasGroupId);
	ByteOrder_WriteU32Le(bytes + 32, 0);
}

static void FrontImage_ReadImageHeader(const unsigned char* bytes, ImageResource* image) {
	int i;

	image->width = ByteOrder_ReadI32Le(bytes);
	image->height = ByteOrder_ReadI32Le(bytes + 4);
	image->isCompressed = ByteOrder_ReadI32Le(bytes + 8);
	image->pixelCount = ByteOrder_ReadI32Le(bytes + 12);
	image->boundsLeft = ByteOrder_ReadI32Le(bytes + 16);
	image->boundsTop = ByteOrder_ReadI32Le(bytes + 20);
	image->boundsRight = ByteOrder_ReadI32Le(bytes + 24);
	image->boundsBottom = ByteOrder_ReadI32Le(bytes + 28);
	image->pixels = NULL;

	for (i = 0; i < 256; ++i) {
		image->colorLUT[i] = ByteOrder_ReadI32Le(bytes + 36 + 4 * i);
	}

	memcpy(image->palette, bytes + 1060, sizeof(image->palette));
}

static void FrontImage_WriteImageHeader(unsigned char* bytes, const ImageResource* image) {
	int i;

	memset(bytes, 0, FRONT_IMAGE_RESOURCE_FILE_SIZE);
	ByteOrder_WriteU32Le(bytes, (uint32_t)image->width);
	ByteOrder_WriteU32Le(bytes + 4, (uint32_t)image->height);
	ByteOrder_WriteU32Le(bytes + 8, (uint32_t)image->isCompressed);
	ByteOrder_WriteU32Le(bytes + 12, (uint32_t)image->pixelCount);
	ByteOrder_WriteU32Le(bytes + 16, (uint32_t)image->boundsLeft);
	ByteOrder_WriteU32Le(bytes + 20, (uint32_t)image->boundsTop);
	ByteOrder_WriteU32Le(bytes + 24, (uint32_t)image->boundsRight);
	ByteOrder_WriteU32Le(bytes + 28, (uint32_t)image->boundsBottom);
	ByteOrder_WriteU32Le(bytes + 32, 0);

	for (i = 0; i < 256; ++i) {
		ByteOrder_WriteU32Le(bytes + 36 + 4 * i, (uint32_t)image->colorLUT[i]);
	}

	memcpy(bytes + 1060, image->palette, sizeof(image->palette));
}

static void FrontImage_BuildColorLut16(ImageResource* image) {
	int i;

	for (i = 0; i < 256; ++i) {
		const unsigned char* color = image->palette + 4 * i;
		int red;
		int green;
		int blue;

		red = color[0];
		green = color[1];
		blue = color[2];

		if (g_pixelFormat555) {
			image->colorLUT[i] = (blue >> 3) + 32 * ((green >> 3) + 32 * (red >> 3));
		} else {
			image->colorLUT[i] = (blue >> 3) + 32 * ((green >> 2) + 64 * (red >> 3));
		}
	}
}

static void FrontImage_FreeDescriptor(ResourceDescriptor* desc) {
	int i;

	if (desc == NULL) {
		return;
	}

	if (desc->atlasBaseIndex == 0 && desc->image != NULL) {
		for (i = 0; i < desc->frameCount; ++i) {
			if (desc->image[i].pixels != NULL) {
				Mem_Free(desc->image[i].pixels);
				desc->image[i].pixels = NULL;
			}
		}

		Mem_Free(desc->image);
		desc->image = NULL;
	}

	Mem_Free(desc);
}

static int FrontImage_LoadResourceListImpl(char* fileName, int unload) {
	XwaFile* stream;
	int id;
	char buffer[256];
	char line[256];
	char name[256];

	stream = FrontImage_OpenRead(AERON_VFS_ROOT_ASSET, fileName);
	if (stream == NULL) {
		Aeron_LogError("xwa.assets", "Failed to open frontend resource list '%s'", fileName);
		return 0;
	}

	if (File_ReadLine(stream, line, sizeof(line))) {
		while (File_ReadLine(stream, line, sizeof(line))) {
			if (sscanf(line, "%255s %255s %d", buffer, name, &id) != 3) {
				continue;
			}

			if (unload) {
				FrontImage_FreeResourceByName(name);
			} else {
				if (!FrontImage_RegisterResource(buffer, name, 0, id)) {
					Aeron_LogError("xwa.assets", "Failed to register frontend resource '%s' from '%s' (id %d)",
							  name, buffer, id);
				}
				Music_Update();
			}
		}
	}

	File_Close(stream);
	return 1;
}

// FUNCTION: XWA 0x537570
int FrontImage_BSearchResource(ResourceEntry* table, int hi, const char* key) {
	ResourceEntry* searchTable;
	int baseIndex;

	searchTable = table;
	baseIndex = 0;
	while (1) {
		int mid;
		int cmp;

		if (hi < 0) {
			break;
		}

		mid = hi >> 1;
		cmp = strncmp(searchTable[mid].name, key, sizeof(searchTable[mid].name));

		if (cmp == 0) {
			return mid + baseIndex;
		}

		if (hi <= 0) {
			break;
		}

		if (cmp < 0) {
			hi -= mid + 1;
			baseIndex += mid + 1;
			searchTable += mid + 1;
		} else {
			hi = mid - 1;
		}
	}

	return -1;
}

// FUNCTION: XWA 0x537540
int FrontImage_FindResourceByName(const char* name) {
	if (name == NULL) {
		return -1;
	}

	return FrontImage_BSearchResource(g_resourceTable, g_resourceCount - 1, name);
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x537440
int FrontImage_InsertResourceSorted(ResourceEntry* entry) {
	int insertIndex = 0;
	int i;

	while (insertIndex < g_resourceCount &&
		   strncmp(entry->name, g_resourceTable[insertIndex].name, sizeof(entry->name)) >= 0) {
		++insertIndex;
	}

	for (i = g_resourceCount; i > insertIndex; --i) {
		g_resourceTable[i] = g_resourceTable[i - 1];
	}

	g_resourceTable[insertIndex] = *entry;
	return ++g_resourceCount;
}

// FUNCTION: XWA 0x5374E0
void FrontImage_RemoveResourceAt(int index) {
	int i;

	if (index < 0 || index >= g_resourceCount) {
		return;
	}

	for (i = index; i < g_resourceCount - 1; ++i) {
		g_resourceTable[i] = g_resourceTable[i + 1];
	}

	--g_resourceCount;
}

// FUNCTION: XWA 0x532080
void FrontImage_FreeResourceByName(const char* name) {
	ResourceDescriptor* desc;
	int index;
	int i;

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return;
	}
#ifdef XWA_MODERN
	XwaSnapshot_NoteResourceFree(name);
#endif

	desc = g_resourceTable[index].desc;
	if (desc != NULL) {
		if (desc->atlasBaseIndex == 0) {
			for (i = 0; i < desc->frameCount; ++i) {
				ImageResource* frame = &desc->image[i];
				if (frame->pixels != NULL) {
					Mem_Free(frame->pixels);
					frame->pixels = NULL;
				}
			}

			Mem_Free(desc->image);
			desc->image = NULL;
		}

		Mem_Free(desc);
	}

	g_resourceTable[index].desc = NULL;
	FrontImage_RemoveResourceAt(index);
}

// FUNCTION: XWA 0x532130
void FrontImage_FreeAllResources(void) {
	int i;

	if (g_resourceTable == NULL) {
		return;
	}

	for (i = FRONT_IMAGE_MAX_RESOURCES - 1; i >= 0; --i) {
		FrontImage_FreeResourceByName(g_resourceTable[i].name);
	}
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x532160
int FrontImage_ResourceExists(const char* name) {
	if (*name == '\0') {
		return 0;
	} else {
		return (FrontImage_FindResourceByName(name) != -1);
	}
}

// FUNCTION: XWA 0x532180
int FrontImage_GetResourceRect(const char* name, FrontendRect* out) {
	int index;

	if (*name == '\0') {
		FrontendDraw_RectAssign(out, 0, 0, 0, 0);
		return 0;
	}

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		FrontendDraw_RectAssign(out, 0, 0, 0, 0);
		return 0;
	}

	FrontendDraw_RectCopy(out, &g_resourceTable[index].desc->bounds);
	return 1;
}

// FUNCTION: XWA 0x5321F0
int FrontImage_GetSpriteFrame(const char* name) {
	int index;

	if (*name == '\0') {
		return 0;
	}

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}

	return g_resourceTable[index].desc->currentFrame;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x532230
int FrontImage_AdvanceSpriteFrame(const char* name, int loop) {
	ResourceDescriptor* desc;
	int index;

	if (*name == '\0') {
		return 0;
	}

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}

	desc = g_resourceTable[index].desc;
	++desc->currentFrame;
	if (desc->currentFrame >= desc->frameCount) {
		if (loop) {
			desc->currentFrame = 0;
		} else {
			desc->currentFrame = desc->frameCount - 1;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x532290
int FrontImage_RewindSpriteFrame(const char* name, int wrap) {
	ResourceDescriptor* desc;
	int index;

	if (*name == '\0') {
		return 0;
	}

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}

	desc = g_resourceTable[index].desc;
	if (desc->currentFrame <= 0) {
		if (wrap) {
			desc->currentFrame = desc->frameCount - 1;
		} else {
			desc->currentFrame = 0;
		}

		return 1;
	}

	--desc->currentFrame;
	return 1;
}

// FUNCTION: XWA 0x5322F0
int FrontImage_SetSpriteFrame(const char* name, int frame) {
	ResourceDescriptor* desc;
	int index;

	if (*name == '\0') {
		return 0;
	}

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}

	desc = g_resourceTable[index].desc;
	if (frame < 0) {
		frame = 0;
	} else if (frame >= desc->frameCount) {
		frame = desc->frameCount - 1;
	}
	desc->currentFrame = frame;

	return 1;
}

// FUNCTION: XWA 0x534D00
void FrontImage_BlitRLE8(ImageResource* image, int destX, int destY, int srcLeft, int srcTop,
						 int visibleWidth, int visibleHeight) {
	const unsigned char* row;
	const unsigned char* rowStart;
	unsigned char* dest;
	int rowLength;
	int tokenOffset;
	int destOffset;
	unsigned char started;
	unsigned char token;
	int count;
	int endOffset;
	unsigned char value;

	if (visibleWidth == image->width) {
		row = image->pixels;
		dest = g_drawSurfacePtr + g_drawSurfacePitch * destY + destX;
		while (srcTop > 0) {
			row += FrontImage_ReadRleRowLength(row);
			--srcTop;
		}

		if (visibleHeight <= 0) {
			return;
		}

		visibleWidth = visibleHeight;
		do {
			unsigned char encodedToken = 0;
			int fastDestOffset = 0;

			row += 4;

			while (encodedToken != 0x80) {
				encodedToken = *row++;

				if (encodedToken == 0x80) {
					break;
				}

				token = encodedToken;

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					memcpy(dest + fastDestOffset, row, token);
					fastDestOffset += token;
					row += token;
					continue;
				}

				if ((token & 0x40u) != 0) {
					fastDestOffset += token & 0x3f;
					continue;
				}

				value = *row++;
				memset(dest + fastDestOffset, value, token);
				fastDestOffset += token;
			}

			dest += g_drawSurfacePitch;
			--visibleWidth;
		} while (visibleWidth != 0);

		return;
	}

	row = image->pixels;
	dest = g_drawSurfacePtr + g_drawSurfacePitch * destY + destX;
	while (srcTop > 0) {
		row += FrontImage_ReadRleRowLength(row);
		--srcTop;
	}

	if (visibleHeight <= 0) {
		return;
	}

	while (visibleHeight > 0) {
		rowStart = row;
		destOffset = 0;
		tokenOffset = 4;
		rowLength = FrontImage_ReadRleRowLength(rowStart);
		started = 0;

		for (;;) {
			token = rowStart[tokenOffset];
			++tokenOffset;

			if (!started) {
				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					count = token;
					endOffset = destOffset + count;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						tokenOffset += count;
						started = 1;
						token -= count;
						destOffset = 0;
						if (token != 0) {
							token |= 0x80;
						}
					} else {
						destOffset = endOffset;
						tokenOffset += count;
					}
				} else if ((token & 0x40u) != 0) {
					token &= 0x3f;
					count = token;
					endOffset = destOffset + token;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						started = 1;
						token -= count;
						destOffset = 0;
						if (token != 0) {
							token |= 0x40;
						}
					} else {
						destOffset += count;
					}
				} else {
					count = token;
					endOffset = destOffset + token;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						started = 1;
						token -= count;
						destOffset = 0;
						if (token == 0) {
							++tokenOffset;
						}
					} else {
						++tokenOffset;
						destOffset += count;
					}
				}
			}

			if (started != 1 || token == 0) {
				continue;
			}

			if (token == 0x80) {
				break;
			}

			if ((token & 0x80u) != 0) {
				token &= 0x7f;
				endOffset = destOffset + token;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					memcpy(dest + destOffset, rowStart + tokenOffset, token);
					break;
				}

				memcpy(dest + destOffset, rowStart + tokenOffset, token);
				destOffset = endOffset;
				tokenOffset += token;
			} else if ((token & 0x40u) != 0) {
				token &= 0x3f;
				destOffset += token;
				if (destOffset >= visibleWidth) {
					break;
				}
			} else {
				value = rowStart[tokenOffset];
				++tokenOffset;
				count = token;
				endOffset = destOffset + count;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					memset(dest + destOffset, value, token);
					break;
				}

				memset(dest + destOffset, value, count);
				destOffset = endOffset;
			}
		}

		row += rowLength;
		dest += g_drawSurfacePitch;
		--visibleHeight;
	}
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x5350A0
void FrontImage_BlitRLE16(ImageResource* image, int destX, int destY, int srcLeft, int srcTop,
						  int visibleWidth, int visibleHeight) {
	const unsigned char* row;
	const unsigned char* rowStart;
	unsigned char* dest;
	int rowLength;
	int tokenOffset;
	int destOffset;
	unsigned char started;
	unsigned char token;
	unsigned char colorIndex;
	int count;
	int endOffset;
	int rowsRemaining;
	int i;
	int value;

	if (visibleWidth == image->width) {
		row = image->pixels;
		dest = g_drawSurfacePtr + g_drawSurfacePitch * destY + 2 * destX;
		while (srcTop > 0) {
			row += FrontImage_ReadRleRowLength(row);
			--srcTop;
		}

		if (visibleHeight <= 0) {
			return;
		}

		visibleWidth = visibleHeight;
		do {
			srcLeft = 0;
			row += 4;

			for (;;) {
				token = *row++;

				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					for (i = 0; i < token; ++i) {
						FrontImage_WriteSurface16Lut(dest + 2 * (srcLeft + i), &image->colorLUT[row[i]]);
					}
					srcLeft += token;
					row += token;
				} else if ((token & 0x40u) != 0) {
					token &= 0x3f;
					srcLeft += token;
				} else {
					value = image->colorLUT[*row++];
					for (i = 0; i < token; ++i) {
						FrontImage_WriteSurface16(dest + 2 * (srcLeft + i), value);
					}
					srcLeft += token;
				}
			}

			dest += 2 * (g_drawSurfacePitch >> 1);
			--visibleWidth;
		} while (visibleWidth != 0);

		return;
	}

	row = image->pixels;
	dest = g_drawSurfacePtr + g_drawSurfacePitch * destY + 2 * destX;
	while (srcTop > 0) {
		row += FrontImage_ReadRleRowLength(row);
		--srcTop;
	}

	if (visibleHeight <= 0) {
		return;
	}

	rowsRemaining = visibleHeight;
	do {
		rowStart = row;
		destOffset = 0;
		tokenOffset = 4;
		rowLength = FrontImage_ReadRleRowLength(rowStart);
		started = 0;

		for (;;) {
			token = rowStart[tokenOffset];
			++tokenOffset;

			if (!started) {
				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					count = token;
					endOffset = destOffset + count;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						tokenOffset += count;
						started = 1;
						token -= (unsigned char)count;
						destOffset = 0;
						if (token != 0) {
							token |= 0x80;
						}
					} else {
						destOffset = endOffset;
						tokenOffset += count;
					}
				} else if ((token & 0x40u) != 0) {
					token &= 0x3f;
					count = token;
					endOffset = destOffset + token;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						started = 1;
						token -= (unsigned char)count;
						destOffset = 0;
						if (token != 0) {
							token |= 0x40;
						}
					} else {
						destOffset += count;
					}
				} else {
					count = token;
					endOffset = destOffset + token;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						started = 1;
						token -= (unsigned char)count;
						destOffset = 0;
						if (token == 0) {
							++tokenOffset;
						}
					} else {
						++tokenOffset;
						destOffset += count;
					}
				}
			}

			if (started != 1 || token == 0) {
				continue;
			}

			if (token == 0x80) {
				break;
			}

			if ((token & 0x80u) != 0) {
				token &= 0x7f;
				endOffset = destOffset + token;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					i = 0;
					while (token != 0) {
						FrontImage_WriteSurface16Lut(dest + 2 * (destOffset + i),
													 &image->colorLUT[rowStart[tokenOffset + i]]);
						++i;
						--token;
					}
					break;
				}

				for (i = 0; i < token; ++i) {
					FrontImage_WriteSurface16Lut(dest + 2 * (destOffset + i),
												 &image->colorLUT[rowStart[tokenOffset + i]]);
				}
				destOffset = endOffset;
				tokenOffset += token;
			} else if ((token & 0x40u) != 0) {
				destOffset += token & 0x3f;
				if (destOffset >= visibleWidth) {
					break;
				}
			} else {
				colorIndex = rowStart[tokenOffset++];
				endOffset = destOffset + token;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					value = image->colorLUT[colorIndex];
					if (token != 0) {
						unsigned char* fillDest = dest + 2 * destOffset;

						count = token;
						do {
							FrontImage_WriteSurface16(fillDest, value);
							fillDest += 2;
							--count;
						} while (count != 0);
					}
					break;
				}

				value = image->colorLUT[colorIndex];
				for (i = 0; i < token; ++i) {
					FrontImage_WriteSurface16(dest + 2 * (destOffset + i), value);
				}
				destOffset = endOffset;
			}
		}

		row += rowLength;
		dest += 2 * (g_drawSurfacePitch >> 1);
		--rowsRemaining;
	} while (rowsRemaining != 0);
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x5356D0
void FrontImage_BlitRLE8Opaque(ImageResource* image, int destX, int destY, int srcLeft, int srcTop,
							   int visibleWidth, int visibleHeight) {
	int tokenOffset;
	const unsigned char* row;
	int count;
	int destOffset;
	unsigned char* dest;
	int rowLength;
	const unsigned char* rowStart;
	unsigned char started;
	unsigned char token;
	int endOffset;
	unsigned char value;

	if (visibleWidth == image->width) {
		row = image->pixels;
		dest = g_drawSurfacePtr + g_drawSurfacePitch * destY + destX;
		while (srcTop > 0) {
			row += FrontImage_ReadRleRowLength(row);
			--srcTop;
		}

		if (visibleHeight <= 0) {
			return;
		}

		srcLeft = visibleHeight;
		do {
			unsigned char encodedToken = 0;
			destOffset = 0;

			row += 4;

			while (encodedToken != 0x80) {
				token = *row++;
				encodedToken = token;

				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					memcpy(dest + destOffset, row, token);
					destOffset += token;
					row += token;
				} else if ((token & 0x40u) != 0) {
					token &= 0x3f;
					memset(dest + destOffset, 0, token);
					destOffset += token;
				} else {
					value = *row++;
					memset(dest + destOffset, value, token);
					destOffset += token;
				}
			}

			dest += g_drawSurfacePitch;
			--srcLeft;
		} while (srcLeft != 0);

		return;
	}

	row = image->pixels;
	dest = g_drawSurfacePtr + g_drawSurfacePitch * destY + destX;
	while (srcTop > 0) {
		row += FrontImage_ReadRleRowLength(row);
		--srcTop;
	}

	if (visibleHeight <= 0) {
		return;
	}

	while (visibleHeight > 0) {
		rowStart = row;
		destOffset = 0;
		tokenOffset = 4;
		rowLength = FrontImage_ReadRleRowLength(rowStart);
		started = 0;

		for (;;) {
			token = rowStart[tokenOffset++];

			if (!started) {
				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					count = token;
					endOffset = destOffset + count;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						tokenOffset += count;
						started = 1;
						token -= (unsigned char)count;
						destOffset = 0;
						if (token != 0) {
							token |= 0x80;
						}
					} else {
						destOffset = endOffset;
						tokenOffset += count;
					}
				} else if ((token & 0x40u) != 0) {
					token &= 0x3f;
					count = token;
					endOffset = destOffset + token;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						started = 1;
						token -= (unsigned char)count;
						destOffset = 0;
						if (token != 0) {
							token |= 0x40;
						}
					} else {
						destOffset += count;
					}
				} else {
					count = token;
					endOffset = destOffset + token;
					if (endOffset >= srcLeft) {
						count = srcLeft - destOffset;
						started = 1;
						token -= (unsigned char)count;
						destOffset = 0;
						if (token == 0) {
							++tokenOffset;
						}
					} else {
						++tokenOffset;
						destOffset += count;
					}
				}
			}

			if (started != 1 || token == 0) {
				continue;
			}

			if (token == 0x80) {
				break;
			}

			if ((token & 0x80u) != 0) {
				token &= 0x7f;
				endOffset = destOffset + token;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					memcpy(dest + destOffset, rowStart + tokenOffset, token);
					break;
				}

				memcpy(dest + destOffset, rowStart + tokenOffset, token);
				destOffset = endOffset;
				tokenOffset += token;
			} else if ((token & 0x40u) != 0) {
				token &= 0x3f;
				endOffset = destOffset + token;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					memset(dest + destOffset, 0, token);
					break;
				}

				memset(dest + destOffset, 0, token);
				destOffset = endOffset;
			} else {
				value = rowStart[tokenOffset++];
				count = token;
				endOffset = destOffset + count;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					memset(dest + destOffset, value, token);
					break;
				}

				memset(dest + destOffset, value, count);
				destOffset = endOffset;
			}
		}

		row += rowLength;
		dest += g_drawSurfacePitch;
		--visibleHeight;
	}

	return;
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x535AE0
void FrontImage_BlitRLE16Opaque(ImageResource* image, int destX, int destY, int srcLeft, int srcTop,
								int visibleWidth, int visibleHeight) {
	const unsigned char* row;
	const unsigned char* rowStart;
	uint16_t* dest;
	int rowLength;
	int tokenOffset;
	int destOffset;
	unsigned char started;
	unsigned char token;
	int count;
	int i;
	int value;
	int endOffset;

	if (visibleWidth == image->width) {
		row = image->pixels;
		dest = (uint16_t*)(g_drawSurfacePtr + g_drawSurfacePitch * destY) + destX;
		while (srcTop > 0) {
			row += *(const int*)row;
			--srcTop;
		}

		if (visibleHeight <= 0) {
			return;
		}

		srcLeft = visibleHeight;
		do {
			visibleWidth = 0;
			row += 4;

			for (;;) {
				token = *row++;

				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					for (i = 0; i < token; ++i) {
						dest[visibleWidth + i] = (uint16_t)image->colorLUT[row[i]];
					}
					visibleWidth += token;
					row += token;
				} else if ((token & 0x40u) != 0) {
					token &= 0x3f;
					value = image->colorLUT[0];
					for (i = 0; i < token; ++i) {
						dest[visibleWidth + i] = (uint16_t)value;
					}
					visibleWidth += token;
				} else {
					for (i = 0; i < token; ++i) {
						dest[visibleWidth + i] = (uint16_t)image->colorLUT[row[0]];
					}
					visibleWidth += token;
					++row;
				}
			}

			dest += g_drawSurfacePitch >> 1;
			--srcLeft;
		} while (srcLeft != 0);

		return;
	}

	row = image->pixels;
	dest = (uint16_t*)(g_drawSurfacePtr + g_drawSurfacePitch * destY) + destX;
	while (srcTop > 0) {
		row += *(const int*)row;
		--srcTop;
	}

	if (visibleHeight <= 0) {
		return;
	}

	while (visibleHeight > 0) {
		rowStart = row;
		destOffset = 0;
		tokenOffset = 4;
		rowLength = *(const int*)rowStart;
		started = 0;

		for (;;) {
			token = rowStart[tokenOffset++];

			if (!started) {
				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					count = token;
					if (destOffset + count < srcLeft) {
						destOffset += count;
						tokenOffset += count;
						continue;
					}

					tokenOffset += srcLeft - destOffset;
					token -= (unsigned char)(srcLeft - destOffset);
					started = 1;
					destOffset = 0;
					if (token != 0) {
						token |= 0x80;
					}
				} else if ((token & 0x40u) != 0) {
					token &= 0x3f;
					count = token;
					if (destOffset + count < srcLeft) {
						destOffset += count;
						continue;
					}

					token -= (unsigned char)(srcLeft - destOffset);
					started = 1;
					destOffset = 0;
					if (token != 0) {
						token |= 0x40;
					}
				} else {
					count = token;
					if (destOffset + count < srcLeft) {
						destOffset += count;
						++tokenOffset;
						continue;
					}

					token -= (unsigned char)(srcLeft - destOffset);
					started = 1;
					destOffset = 0;
					if (token == 0) {
						++tokenOffset;
					}
				}
			}

			if (started != 1 || token == 0) {
				continue;
			}

			if (token == 0x80) {
				break;
			}

			if ((token & 0x80u) != 0) {
				token &= 0x7f;
				endOffset = destOffset + token;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					for (i = 0; i < token; ++i) {
						dest[destOffset + i] = (uint16_t)image->colorLUT[rowStart[tokenOffset + i]];
					}
					break;
				}

				for (i = 0; i < token; ++i) {
					dest[destOffset + i] = (uint16_t)image->colorLUT[rowStart[tokenOffset + i]];
				}
				destOffset = endOffset;
				tokenOffset += token;
			} else if ((token & 0x40u) != 0) {
				token &= 0x3f;
				endOffset = destOffset + token;
				if (endOffset >= visibleWidth) {
					token = (unsigned char)(visibleWidth - destOffset);
					for (i = 0; i < token; ++i) {
						dest[destOffset + i] = (uint16_t)image->colorLUT[0];
					}
					break;
				}

				for (i = 0; i < token; ++i) {
					dest[destOffset + i] = (uint16_t)image->colorLUT[0];
				}
				destOffset = endOffset;
			} else {
				value = image->colorLUT[rowStart[tokenOffset++]];
				count = token;
				endOffset = destOffset + count;
				if (endOffset >= visibleWidth) {
					count = visibleWidth - destOffset;
					for (i = 0; i < count; ++i) {
						dest[destOffset + i] = (uint16_t)value;
					}
					break;
				}

				for (i = 0; i < count; ++i) {
					dest[destOffset + i] = (uint16_t)value;
				}
				destOffset = endOffset;
			}
		}

		row += rowLength;
		dest += g_drawSurfacePitch >> 1;
		--visibleHeight;
	}

	return;
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x534AD0
int FrontImage_BlitClipped(ImageResource* image, int x, int y) {
	int clipResult;
	FrontendRect srcRect;
	FrontendRect dstRect;

	if (image == NULL) {
		return 0;
	}

	srcRect.left = x;
	srcRect.right = image->width + x - 1;
	srcRect.top = y;
	srcRect.bottom = image->height + y - 1;
	FrontendDraw_RectCopy(&dstRect, &srcRect);
	clipResult = FrontendDraw_RectClipToBounds(&srcRect);

	if (srcRect.right < srcRect.left) {
		return clipResult;
	}

	if (srcRect.bottom < srcRect.top) {
		return clipResult;
	}

	{
		int srcLeft;
		int srcTop;
		int imageWidth;
		int visibleHeight;
		int visibleWidth;
		int displayBpp;

		srcLeft = srcRect.left - dstRect.left;
		srcTop = srcRect.top - dstRect.top;
		imageWidth = image->width;
		visibleHeight = image->height + srcRect.bottom - dstRect.bottom - srcTop;
		visibleWidth = imageWidth + srcRect.right - dstRect.right - srcLeft;
		displayBpp = g_displayBpp;

		if (!image->isCompressed) {
			if (displayBpp != 8) {
				if (displayBpp == 16) {
					unsigned char* src;
					unsigned char* dest;
					int rowCount;

					src = image->pixels + imageWidth * srcTop + srcLeft;
					dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (y + srcTop) + 2 * (dstRect.left + srcLeft);

					if (visibleHeight > 0) {
						rowCount = visibleHeight;
						do {
							int col;

							col = 0;
							if (visibleWidth > 0) {
								unsigned char* rowDest;

								rowDest = dest;
								do {
									if (src[col] != 0) {
										*(uint16_t*)rowDest = (uint16_t)image->colorLUT[src[col]];
									}
									++col;
									rowDest += 2;
								} while (col < visibleWidth);
							}

							src += image->width;
							dest += 2 * (g_drawSurfacePitch >> 1);
							--rowCount;
						} while (rowCount != 0);
						return clipResult;
					}
				}

				return clipResult;
			} else {
				unsigned char* src;
				unsigned char* dest;
				int rowCount;

				src = image->pixels + imageWidth * srcTop + srcLeft;
				dest = g_drawSurfacePtr + g_drawSurfacePitch * (y + srcTop) + dstRect.left + srcLeft;

				if (visibleHeight <= 0) {
					return clipResult;
				}

				rowCount = visibleHeight;
				do {
					if (visibleWidth > 0) {
						int count;
						unsigned char* srcPixel;
						int destOffset;

						count = visibleWidth;
						srcPixel = src;
						destOffset = (int)(dest - src);
						do {
							if (*srcPixel != 0) {
								srcPixel[destOffset] = *srcPixel;
							}
							++srcPixel;
							--count;
						} while (count != 0);
					}

					src += image->width;
					dest += g_drawSurfacePitch;
					--rowCount;
				} while (rowCount != 0);
				return clipResult;
			}
		} else {
			if (displayBpp != 8) {
				if (displayBpp == 16) {
					FrontImage_BlitRLE16(image, dstRect.left + srcLeft, y + srcTop, srcLeft, srcTop,
										 visibleWidth, visibleHeight);
					return clipResult;
				}
			} else {
				FrontImage_BlitRLE8(image, dstRect.left + srcLeft, y + srcTop, srcLeft, srcTop, visibleWidth,
									visibleHeight);
			}
			return clipResult;
		}
	}

	return clipResult;
}

// FUNCTION: XWA 0x5354C0
int FrontImage_BlitOpaque(ImageResource* image, int x, int y) {
	FrontendRect srcRect;
	FrontendRect dstRect;
	int srcTop;
	int srcLeft;
	int visibleWidth;
	int visibleHeight;
	int displayBpp;
	int clipResult;

	if (image == NULL) {
		return 0;
	}

	srcRect.left = x;
	srcRect.right = image->width + x - 1;
	srcRect.top = y;
	srcRect.bottom = image->height + y - 1;
	FrontendDraw_RectCopy(&dstRect, &srcRect);
	clipResult = FrontendDraw_RectClipToBounds(&srcRect);

	if (srcRect.right < srcRect.left) {
		return clipResult;
	}

	if (srcRect.bottom < srcRect.top) {
		return clipResult;
	}

	srcLeft = srcRect.left - dstRect.left;
	srcTop = srcRect.top - dstRect.top;
	visibleWidth = image->width + srcRect.right - dstRect.right - srcLeft;
	visibleHeight = image->height + srcRect.bottom - dstRect.bottom - srcTop;

	if (!image->isCompressed) {
		displayBpp = g_displayBpp;
		if (displayBpp != 8) {
			if (displayBpp == 16) {
				unsigned char* src;
				unsigned char* dest;
				int rowCount;

				src = image->pixels + image->width * srcTop + srcLeft;
				dest = g_drawSurfacePtr + g_drawSurfacePitch * (y + srcTop) + 2 * (x + srcLeft);

				if (visibleHeight > 0) {
					rowCount = visibleHeight;
					do {
						int col;

						col = 0;
						if (visibleWidth > 0) {
							unsigned char* rowDest;

							rowDest = dest;
							do {
								*(uint16_t*)rowDest = (uint16_t)image->colorLUT[src[col]];
								++col;
								rowDest += 2;
							} while (col < visibleWidth);
						}

						src += image->width;
						dest += 2 * (g_drawSurfacePitch >> 1);
						--rowCount;
					} while (rowCount != 0);
					return clipResult;
				}
			}

			return clipResult;
		} else {
			unsigned char* src;
			unsigned char* dest;
			int rowCount;

			src = image->pixels + image->width * srcTop + srcLeft;
			dest = g_drawSurfacePtr + g_drawSurfacePitch * (y + srcTop) + x + srcLeft;

			if (visibleHeight <= 0) {
				return clipResult;
			}

			rowCount = visibleHeight;
			do {
				if (visibleWidth > 0) {
					int count;
					unsigned char* destPixel;
					int srcOffset;

					destPixel = dest;
					srcOffset = (int)(src - dest);
					count = visibleWidth;
					do {
						*destPixel = destPixel[srcOffset];
						++destPixel;
						--count;
					} while (count != 0);
				}

				src += image->width;
				dest += g_drawSurfacePitch;
				--rowCount;
			} while (rowCount != 0);
			return clipResult;
		}
	} else {
		displayBpp = g_displayBpp;
		if (displayBpp != 8) {
			if (displayBpp == 16) {
				FrontImage_BlitRLE16Opaque(image, x + srcLeft, y + srcTop, srcLeft, srcTop, visibleWidth,
										   visibleHeight);
				return clipResult;
			}
		} else {
			FrontImage_BlitRLE8Opaque(image, x + srcLeft, y + srcTop, srcLeft, srcTop, visibleWidth,
									  visibleHeight);
		}
		return clipResult;
	}

	return clipResult;
}

// FUNCTION: XWA 0x534A60
int FrontImage_DrawSprite(const char* name, int x, int y) {
	int index;
	ResourceDescriptor* desc;
	int currentFrame;

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}

	desc = g_resourceTable[index].desc;
	currentFrame = desc->currentFrame;
	if (desc->atlasBaseIndex != 0) {
		FrontImage_DrawAtlasSprite(desc->atlasGroupId, (int16_t)(desc->atlasBaseIndex + currentFrame),
								   (int16_t)x, (int16_t)y);
		return 0;
	}

#ifdef XWA_MODERN
	/* Remaster snapshot observer (non-atlas path; the atlas branch
	 * above emits inside FrontImage_DrawAtlasSprite). */
	XwaSnapshot_EmitSprite(XWA_DRAW2D_SPRITE, name, currentFrame, 0, x, y, desc->image[currentFrame].width,
						   desc->image[currentFrame].height, 0, 0, 0);
#endif
	return FrontImage_BlitClipped(&desc->image[currentFrame], x, y);
}

// FUNCTION: XWA 0x55DB00
int FrontImage_DrawAtlasSprite(int16_t groupId, int16_t index, int16_t x, int16_t y) {
	Sprite* sprite = SpriteResource_ResolveSprite(groupId, (uint16_t)index);
	const FrontImageSpritePayload* payload;

	if (sprite == NULL) {
		return 0;
	}

	g_atlasSpriteWidth = sprite->width;
	g_atlasSpriteHeight = sprite->height;
	g_atlasSpriteRows = SpriteResource_GetRowData(sprite);
	payload = (const FrontImageSpritePayload*)SpriteResource_GetSpritePayload(sprite);
	g_atlasColorTable = (const unsigned char*)payload;
	g_atlasColorTable = (const unsigned char*)payload + payload->palette16Offset;
#ifdef XWA_MODERN
	/* Remaster snapshot observer. Emits the anchored blit position and
	 * the sprite dims (aeron atlas convention: origins are baked into
	 * the draw position at emit time). */
	XwaSnapshot_EmitAtlasSprite(groupId, index, x + (int16_t)payload->anchorX, y + (int16_t)payload->anchorY,
								sprite->width, sprite->height);
#endif
	FrontImage_BlitAtlasSprite((int16_t)(x + (int16_t)payload->anchorX),
							   (int16_t)(y + (int16_t)payload->anchorY));
	return 1;
}

// FUNCTION: XWA 0x55EBE0
void FrontImage_BuildAtlasBlendLut(void) {
	void* allocation;

	if (g_atlasBlendLut != NULL) {
		Mem_Free(g_atlasBlendLut);
		g_atlasBlendLut = NULL;
	}

	allocation = Mem_Alloc(0x100000u);
	g_atlasBlendLut = (uint16_t*)allocation;
	if (allocation == NULL) {
		return;
	}

	memset(allocation, 0, 0x100000u);
	if (g_pixelFormat555) {
		uint16_t* lut = g_atlasBlendLut;
		int color;

		for (color = 0; color < 0x10000; ++color) {
			int alpha;

			for (alpha = 0; alpha <= 224; alpha += 32) {
				uint32_t blended;

				blended = (((uint16_t)alpha * (color & 0x7c00)) & 0x7c0000) |
						  (((uint16_t)alpha * (color & 0x03e0)) & 0x03e000) |
						  (((uint16_t)alpha * (color & 0x001f)) & 0x001f00u);
				blended >>= 8;
				*lut++ = (uint16_t)blended;
			}
		}
	} else {
		uint16_t* lut = g_atlasBlendLut;
		int color;

		for (color = 0; color < 0x10000; ++color) {
			int alpha;

			for (alpha = 0; alpha <= 224; alpha += 32) {
				uint32_t blended;

				blended = (((uint16_t)alpha * (color & 0xf800)) & 0xf80000) |
						  (((uint16_t)alpha * (color & 0x07e0)) & 0x07e000) |
						  (((uint16_t)alpha * (color & 0x001f)) & 0x001f00u);
				blended >>= 8;
				*lut++ = (uint16_t)blended;
			}
		}
	}

	return;
}

// FUNCTION: XWA 0x55DBE0
int FrontImage_InitAtlasSprites(void) {
	FrontImage_BuildAtlasBlendLut();
	FrontImage_RegisterAtlasSprites();
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x55E170
int FrontImage_RegisterAtlasSprites(void) {
	int i;
	char buffer[20];

	SpriteResource_LoadGroup(15000);
	SpriteResource_LoadGroup(15900);
	FrontImage_RegisterAtlasSprite("leftbar5", 15000, 1500, 20);
	FrontImage_RegisterAtlasSprite("leftbar4", 15000, 1400, 20);
	FrontImage_RegisterAtlasSprite("leftbar3", 15000, 1300, 20);
	FrontImage_RegisterAtlasSprite("leftbar2", 15000, 1200, 20);
	FrontImage_RegisterAtlasSprite("leftbar1", 15000, 1100, 20);
	FrontImage_RegisterAtlasSprite("rightbar5", 15000, 2500, 20);
	FrontImage_RegisterAtlasSprite("rightbar4", 15000, 2400, 20);
	FrontImage_RegisterAtlasSprite("rightbar3", 15000, 2300, 20);
	FrontImage_RegisterAtlasSprite("rightbar2", 15000, 2200, 20);
	FrontImage_RegisterAtlasSprite("rightbar1", 15000, 2100, 20);
	FrontImage_RegisterAtlasSprite("techroom", 15000, 3000, 1);
	FrontImage_RegisterAtlasSprite("techtop", 15000, 3001, 1);
	FrontImage_RegisterAtlasSprite("settingleftup", 15000, 3200, 5);
	FrontImage_RegisterAtlasSprite("settingleftdown", 15000, 3300, 1);
	FrontImage_RegisterAtlasSprite("settingrightup", 15000, 3400, 5);
	FrontImage_RegisterAtlasSprite("settingrightdown", 15000, 3500, 1);
	FrontImage_RegisterAtlasSprite("lsettingleftu", 15000, 3600, 5);
	FrontImage_RegisterAtlasSprite("lsettingleftd", 15000, 3700, 1);
	FrontImage_RegisterAtlasSprite("lsettingrightu", 15000, 3800, 5);
	FrontImage_RegisterAtlasSprite("lsettingrightd", 15000, 3900, 1);
	FrontImage_RegisterAtlasSprite("setting1", 15000, 4090, 2);
	FrontImage_RegisterAtlasSprite("setting2", 15000, 4020, 3);
	FrontImage_RegisterAtlasSprite("setting3", 15000, 4040, 2);
	FrontImage_RegisterAtlasSprite("setting4", 15000, 4120, 1);
	FrontImage_RegisterAtlasSprite("setting5", 15000, 4150, 3);
	FrontImage_RegisterAtlasSprite("setting6", 15000, 4100, 2);
	FrontImage_RegisterAtlasSprite("setting7", 15000, 4030, 3);
	FrontImage_RegisterAtlasSprite("setting8", 15000, 4130, 1);
	FrontImage_RegisterAtlasSprite("setting9", 15000, 4070, 4);
	FrontImage_RegisterAtlasSprite("setting10", 15000, 4110, 1);
	FrontImage_RegisterAtlasSprite("setting11", 15000, 4080, 1);
	FrontImage_RegisterAtlasSprite("setting12", 15000, 4140, 2);
	FrontImage_RegisterAtlasSprite("setting13", 15000, 4050, 2);
	FrontImage_RegisterAtlasSprite("setting14", 15000, 4060, 2);
	FrontImage_RegisterAtlasSprite("loadsetting", 15000, 4170, 1);
	FrontImage_RegisterAtlasSprite("savesetting", 15000, 4180, 1);
	FrontImage_RegisterAtlasSprite("cancelsetting", 15000, 4190, 1);
	FrontImage_RegisterAtlasSprite("deletesetting", 15000, 4400, 1);
	FrontImage_RegisterAtlasSprite("deletesettingd", 15000, 4401, 1);
	FrontImage_RegisterAtlasSprite("donesetting", 15000, 4200, 1);
	FrontImage_RegisterAtlasSprite("copysetting", 15000, 4210, 1);
	FrontImage_RegisterAtlasSprite("pastesetting", 15000, 4220, 1);
	FrontImage_RegisterAtlasSprite("yessetting", 15000, 4230, 1);
	FrontImage_RegisterAtlasSprite("nosetting", 15000, 4240, 1);
	FrontImage_RegisterAtlasSprite("laps", 15000, 4510, 1);
	FrontImage_RegisterAtlasSprite("clearfg", 15000, 4520, 1);
	FrontImage_RegisterAtlasSprite("settingbar", 15000, 4003, 1);
	FrontImage_RegisterAtlasSprite("chatarm", 15000, 4500, 1);

	for (i = 0; i < 53; ++i) {
		sprintf(buffer, "medal%d", i);
		FrontImage_RegisterAtlasSprite(buffer, 15900, (uint16_t)(i + 100), 1);
	}

	for (i = 0; i < 7; ++i) {
		sprintf(buffer, "battle%d", i);
		FrontImage_RegisterAtlasSprite(buffer, 15900, (uint16_t)(i + 200), 1);
	}

	for (i = 0; i < 7; ++i) {
		sprintf(buffer, "rating%d", i);
		FrontImage_RegisterAtlasSprite(buffer, 15900, (uint16_t)(10 * (i + 30)), 4);
	}

	FrontImage_RegisterAtlasSprite("jacket", 15900, 400, 1);
	FrontImage_RegisterAtlasSprite("helmet", 15900, 700, 1);
	FrontImage_RegisterAtlasSprite("ladyblue", 15900, 900, 1);
	FrontImage_RegisterAtlasSprite("cologne", 15900, 800, 1);
	FrontImage_RegisterAtlasSprite("medalcase", 15900, 1000, 1);

	for (i = 0; i < 8; ++i) {
		sprintf(buffer, "trank%d", i);
		FrontImage_RegisterAtlasSprite(buffer, 15900, (uint16_t)(i + 500), 1);
	}

	for (i = 0; i < 6; ++i) {
		sprintf(buffer, "kalidor%d", i);
		FrontImage_RegisterAtlasSprite(buffer, 15900, (uint16_t)(i + 600), 1);
	}

	return 1;
}

// FUNCTION: XWA 0x55E800
int FrontImage_RegisterAtlasSprite(const char* name, uint16_t groupId, uint16_t baseIndex,
								   uint16_t frameCount) {
	int frameIndex;
	FrontendRect bounds;
	FrontendRect frameBounds;

	frameIndex = 0;
	bounds.left = 0x7fffffff;
	bounds.right = 0;
	bounds.top = 0x7fffffff;
	bounds.bottom = 0;

	if (frameCount != 0) {
		do {
			FrontImage_GetAtlasSpriteBounds(&frameBounds, groupId, (uint16_t)(frameIndex + baseIndex));
			if (frameBounds.left < bounds.left) {
				bounds.left = frameBounds.left;
			}
			if (frameBounds.right > bounds.right) {
				bounds.right = frameBounds.right;
			}
			if (frameBounds.top < bounds.top) {
				bounds.top = frameBounds.top;
			}
			if (frameBounds.bottom > bounds.bottom) {
				bounds.bottom = frameBounds.bottom;
			}
			++frameIndex;
		} while (frameIndex < frameCount);
	}

	return FrontImage_CreateAtlasResource(name, groupId, baseIndex, frameCount, &bounds);
}

// FUNCTION: XWA 0x574290
int FrontImage_CreateAtlasResource(const char* name, int groupId, int baseIndex, int frameCount,
								   const FrontendRect* bounds) {
	ResourceDescriptor* desc;
	ResourceEntry entry;

	if (baseIndex == 0) {
		return 0;
	}

	if (*name == '\0') {
		return 0;
	}

	if (FrontImage_FindResourceByName(name) != -1) {
		return 0;
	}

	if (g_resourceCount >= FRONT_IMAGE_MAX_RESOURCES) {
		return 0;
	}

	desc = (ResourceDescriptor*)Mem_Alloc(sizeof(*desc));
	if (desc == NULL) {
		return 0;
	}

	desc->frameCount = frameCount;
	desc->currentFrame = 0;
	desc->atlasBaseIndex = baseIndex;
	desc->atlasGroupId = groupId;
	desc->image = NULL;
	FrontendDraw_RectCopy(&desc->bounds, bounds);

	entry.desc = desc;
	strncpy(entry.name, name, sizeof(entry.name));
	FrontImage_InsertResourceSorted(&entry);
	return 1;
}

// FUNCTION: XWA 0x55DB80
int FrontImage_GetAtlasSpriteBounds(FrontendRect* out, int groupId, uint16_t index) {
	Sprite* sprite = SpriteResource_ResolveSprite(groupId, index);
	SpritePayload* payload;

	if (sprite == NULL) {
		return 0;
	}

#ifdef XWA_MODERN
	payload = (SpritePayload*)SpriteResource_GetMutableSpritePayload(sprite);
#else
	payload = (SpritePayload*)sprite->pixels;
#endif
	out->left = 0;
	out->top = 0;
	out->right = sprite->width - 1;
	out->bottom = sprite->height - 1;
	FrontendDraw_RectOffsetXY(out, (int)payload->anchorX, (int)payload->anchorY);
	return 1;
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x55E8C0
void FrontImage_BlitAtlasSprite(int16_t x, int16_t y) {
	const unsigned char* row;
	int endY;
	int visibleRows;
	int drawY;
	int drawX;
	int atlasColorDepth;
	int atlasColorOffset;

	endY = y;
	row = g_atlasSpriteRows;
	endY += g_atlasSpriteHeight;
	if (endY >= g_clipMaxY + 1) {
		endY = g_clipMaxY + 1;
	}

	visibleRows = endY - y;
	if (visibleRows < 1) {
		return;
	}

	drawX = x;
	drawY = y;
	if (y < g_clipMinY) {
		int skipRows;

		visibleRows -= g_clipMinY - y;
		if (visibleRows < 1) {
			return;
		}

		drawY = g_clipMinY;
		if (g_clipMinY != y) {
			skipRows = g_clipMinY - y;
			do {
				row = FrontImage_SkipAtlasRowRuns(row);
				--skipRows;
			} while (skipRows != 0);
		}
	}

	atlasColorDepth = g_displayBpp;
	if (atlasColorDepth != 16) {
		atlasColorDepth = 16;
	}
	atlasColorOffset = g_displayBpp;
	if (atlasColorOffset != 0) {
		atlasColorOffset = 0;
	}

	do {
		unsigned char runCount;
		unsigned char runCountMinusOne;
		int currentX;
		int runIndex;

		runCount = *row++;
		runCountMinusOne = (unsigned char)(runCount - 1);
		currentX = drawX;
		if (runCount != 0) {
			runIndex = runCountMinusOne + 1;
			do {
				unsigned char token = *row++;
				const unsigned char* runData = row;
				int runLength = token & 0x3f;
				int mode = token & 0xc0;
				int runEndX = currentX + runLength;
				int unclippedEnd = runEndX;
				const unsigned char* nextRun = row;

				if (mode != 0xc0) {
					nextRun = row + runLength;
					if (mode == 0x80) {
						nextRun = row + 2 * runLength;
					}

					if (runEndX >= g_clipMinX && currentX < g_clipMaxX + 1) {
						if (currentX < g_clipMinX) {
							runData = row + (g_clipMinX - currentX);
							if (mode == 0x80) {
								runData += g_clipMinX - currentX;
							}
							runLength = runEndX - g_clipMinX;
							currentX = g_clipMinX;
						}

						if (runEndX >= g_clipMaxX + 1) {
							runLength += g_clipMaxX - runEndX + 1;
						}

						{
							unsigned char* dest =
								g_drawSurfacePtr + 2 * currentX + g_drawSurfacePitch * drawY;

							g_atlasRunRemaining = runLength;
							if (runLength != 0) {
								if (mode == 0) {
									const unsigned char* src = runData;
									int count = runLength;

									if (atlasColorDepth == 16 || atlasColorDepth == 15) {
										do {
											int color;

											color = FrontImage_ReadAtlasColor(*src + atlasColorOffset);
											dest[0] = (unsigned char)color;
											++src;
											dest[1] = (unsigned char)((unsigned int)color >> 8);
											dest += 2;
											--count;
										} while (count != 0);
									} else if (atlasColorDepth == 8) {
										do {
											dest[0] =
												g_atlasColorTable[(unsigned char)(*src + atlasColorOffset)];
											++src;
											++dest;
											--count;
										} while (count != 0);
									} else {
										do {
											uint32_t color;

#ifndef XWA_MODERN
											color = *(const uint32_t*)(g_atlasColorTable +
																	   4 * (unsigned char)(*src +
																						   atlasColorOffset));
											*(uint32_t*)dest = color;
#else
											color = ByteOrder_ReadU32Le(
												g_atlasColorTable +
												4 * (unsigned char)(*src + atlasColorOffset));
											ByteOrder_WriteU32Le(dest, color);
#endif
											++src;
											dest += 4;
											--count;
										} while (count != 0);
									}
								} else {
									const unsigned char* src = runData;
									uint16_t* lut;

									lut = g_atlasBlendLut;
									if (atlasColorDepth == 16) {
										do {
											for (;;) {
												unsigned char alpha = *src;

												if (alpha == 0xff) {
													break;
												}

												if (alpha >= 0x20u) {
													uint16_t destColor;
													uint16_t srcColor;

													alpha >>= 5;
													srcColor = FrontImage_ReadAtlasColor(src[1]);
													destColor = FrontImage_ReadSurface16(dest);
													FrontImage_WriteSurface16(dest,
																			  lut[8 * destColor + 8 - alpha] +
																				  lut[8 * srcColor + alpha]);
													src += 2;
													dest += 2;
													if (--g_atlasRunRemaining != 0) {
														continue;
													}
													goto atlas_run_done;
												} else {
													src += 2;
													dest += 2;
													if (--g_atlasRunRemaining != 0) {
														continue;
													}
													goto atlas_run_done;
												}
											}

											FrontImage_WriteAtlasColor(dest,
																	   FrontImage_ReadAtlasColor(src[1]));
											src += 2;
											dest += 2;
											--g_atlasRunRemaining;
										} while (g_atlasRunRemaining != 0);
									} else if (atlasColorDepth == 8) {
										int count = runLength;

										do {
											int pixel = *src++;

											if (pixel >= 0x80) {
												pixel = *src + atlasColorOffset;
												dest[0] = g_atlasColorTable[pixel];
											}

											++src;
											++dest;
											--count;
										} while (count != 0);
									}
								}
							}
						}
					}
				}

			atlas_run_done:
				--runIndex;
				currentX = unclippedEnd;
				row = nextRun;
			} while (runIndex != 0);
		}

		--visibleRows;
		++drawY;
	} while (visibleRows != 0);
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x55E7E0
int FrontImage_UnloadAtlasSpriteGroups(void) {
	SpriteResource_UnloadGroup(15000);
	SpriteResource_UnloadGroup(15900);
	return 1;
}

// FUNCTION: XWA 0x55DBF0
int FrontImage_FreeAtlasResources(void) {
	if (g_atlasBlendLut != NULL) {
		Mem_Free(g_atlasBlendLut);
		g_atlasBlendLut = NULL;
	}

	FrontImage_UnloadAtlasSpriteGroups();
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x535470
int FrontImage_DrawSpriteOpaque(const char* name, int x, int y) {
	int index;
	ResourceDescriptor* desc;

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}
#ifdef XWA_MODERN
	{
		/* Remaster snapshot observer (opaque). */
		const ResourceDescriptor* snapDesc = g_resourceTable[index].desc;
		const ImageResource* snapImage = &snapDesc->image[snapDesc->currentFrame];
		XwaSnapshot_EmitSprite(XWA_DRAW2D_SPRITE_OPAQUE, name, snapDesc->currentFrame, 0, x, y,
							   snapImage->width, snapImage->height, 0,
							   (uint32_t)(uint16_t)snapImage->colorLUT[0], 0);
	}
#endif

	desc = g_resourceTable[index].desc;
	return FrontImage_BlitOpaque(&desc->image[desc->currentFrame], x, y);
}

// FUNCTION: XWA 0x532680
int FrontImage_BlitRLE16Translucent(ImageResource* image, int dstX, int dstY, int srcX, int srcY, int width,
									int height) {
	const unsigned char* row;
	const unsigned char* rowStart;
	unsigned char* dest;
	const unsigned char* tokenPtr;
	int rowLength;
	int tokenOffset;
	int destOffset;
	int started;
	unsigned char token;
	unsigned char source;
	int count;
	int i;
	int value;
	unsigned char* pixel;
	int destValue;
	int blendValue;

	if (width == image->width) {
		row = image->pixels;
		dest = g_drawSurfacePtr + g_drawSurfacePitch * dstY + 2 * dstX;
		while (srcY > 0) {
			row += *(const int*)row;
			--srcY;
		}

		if (height <= 0) {
			return height;
		}

		while (height > 0) {
			destOffset = 0;
			row += 4;

			for (;;) {
				token = *row++;

				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					count = token;
					tokenPtr = row;
					pixel = dest + 2 * destOffset;
					if (g_pixelFormat555) {
						for (i = 0; i < count; ++i) {
							value = image->colorLUT[tokenPtr[i]];
							destValue = FrontImage_ReadSurface16(pixel);
							blendValue = ((value >> 1) & 0x3def) + ((destValue >> 1) & 0x3def);
							FrontImage_WriteSurface16(pixel, blendValue);
							pixel += 2;
						}
					} else {
						for (i = 0; i < count; ++i) {
							value = image->colorLUT[tokenPtr[i]];
							destValue = FrontImage_ReadSurface16(pixel);
							blendValue = ((value >> 1) & 0x7bef) + ((destValue >> 1) & 0x7bef);
							FrontImage_WriteSurface16(pixel, blendValue);
							pixel += 2;
						}
					}
					destOffset += count;
					row += count;
				} else if ((token & 0x40u) != 0) {
					token &= 0x3f;
					destOffset += token;
				} else {
					source = *row++;
					value = image->colorLUT[source];
					count = token;
					pixel = dest + 2 * destOffset;
					if (g_pixelFormat555) {
						blendValue = ((unsigned int)value >> 1) & 0x3def;
						while (count > 0) {
							destValue = FrontImage_ReadSurface16(pixel);
							FrontImage_WriteSurface16(pixel, (blendValue + ((destValue >> 1) & 0x3def)));
							pixel += 2;
							--count;
						}
					} else {
						blendValue = ((unsigned int)value >> 1) & 0x7bef;
						while (count > 0) {
							destValue = FrontImage_ReadSurface16(pixel);
							FrontImage_WriteSurface16(pixel, (blendValue + ((destValue >> 1) & 0x7bef)));
							pixel += 2;
							--count;
						}
					}
					destOffset += token;
				}
			}

			dest += g_drawSurfacePitch;
			--height;
		}

		return 0;
	}

	row = image->pixels;
	dest = g_drawSurfacePtr + g_drawSurfacePitch * dstY + 2 * dstX;
	while (srcY > 0) {
		row += *(const int*)row;
		--srcY;
	}

	if (height <= 0) {
		return height;
	}

	while (height > 0) {
		destOffset = 0;
		tokenOffset = 4;
		rowStart = row;
		rowLength = *(const int*)rowStart;
		started = 0;

		for (;;) {
			token = rowStart[tokenOffset++];

			if (!started) {
				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					count = token & 0x7f;
					if (destOffset + count >= srcX) {
						tokenOffset += srcX - destOffset;
						count = (unsigned char)(count - (srcX - destOffset));
						started = 1;
						destOffset = 0;
						token = count != 0 ? (unsigned char)(count | 0x80) : 0;
					} else {
						destOffset += count;
						tokenOffset += count;
						continue;
					}
				} else if ((token & 0x40u) != 0) {
					count = token & 0x3f;
					if (destOffset + count < srcX) {
						destOffset += count;
						continue;
					}

					count = (unsigned char)(count - (srcX - destOffset));
					started = 1;
					destOffset = 0;
					token = count != 0 ? (unsigned char)(count | 0x40) : 0;
				} else {
					count = token;
					if (destOffset + count < srcX) {
						destOffset += count;
						++tokenOffset;
						continue;
					}

					count = (unsigned char)(count - (srcX - destOffset));
					started = 1;
					destOffset = 0;
					token = (unsigned char)count;
					if (count == 0) {
						++tokenOffset;
					}
				}
			}

			if (started != 1 || token == 0) {
				continue;
			}

			if (token == 0x80) {
				break;
			}

			if ((token & 0x80u) != 0) {
				count = token & 0x7f;
				if (destOffset + count >= width) {
					count = (unsigned char)(width - destOffset);
					tokenPtr = rowStart + tokenOffset;
					pixel = dest + 2 * destOffset;
					if (g_pixelFormat555) {
						for (i = 0; i < count; ++i) {
							value = image->colorLUT[tokenPtr[i]];
							destValue = FrontImage_ReadSurface16(pixel);
							blendValue = ((value >> 1) & 0x3def) + ((destValue >> 1) & 0x3def);
							FrontImage_WriteSurface16(pixel, blendValue);
							pixel += 2;
						}
					} else {
						for (i = 0; i < count; ++i) {
							value = image->colorLUT[tokenPtr[i]];
							destValue = FrontImage_ReadSurface16(pixel);
							blendValue = ((value >> 1) & 0x7bef) + ((destValue >> 1) & 0x7bef);
							FrontImage_WriteSurface16(pixel, blendValue);
							pixel += 2;
						}
					}
					break;
				}

				tokenPtr = rowStart + tokenOffset;
				pixel = dest + 2 * destOffset;
				if (g_pixelFormat555) {
					for (i = 0; i < count; ++i) {
						value = image->colorLUT[tokenPtr[i]];
						destValue = FrontImage_ReadSurface16(pixel);
						blendValue = ((value >> 1) & 0x3def) + ((destValue >> 1) & 0x3def);
						FrontImage_WriteSurface16(pixel, blendValue);
						pixel += 2;
					}
				} else {
					for (i = 0; i < count; ++i) {
						value = image->colorLUT[tokenPtr[i]];
						destValue = FrontImage_ReadSurface16(pixel);
						blendValue = ((value >> 1) & 0x7bef) + ((destValue >> 1) & 0x7bef);
						FrontImage_WriteSurface16(pixel, blendValue);
						pixel += 2;
					}
				}
				destOffset += count;
				tokenOffset += count;
			} else if ((token & 0x40u) != 0) {
				count = token & 0x3f;
				destOffset += count;
				if (destOffset >= width) {
					break;
				}
			} else {
				source = rowStart[tokenOffset++];
				count = token;
				if (destOffset + count >= width) {
					count = (unsigned char)(width - destOffset);
					value = image->colorLUT[source];
					pixel = dest + 2 * destOffset;
					if (g_pixelFormat555) {
						blendValue = ((unsigned int)value >> 1) & 0x3def;
						while (count > 0) {
							destValue = FrontImage_ReadSurface16(pixel);
							FrontImage_WriteSurface16(pixel, (blendValue + ((destValue >> 1) & 0x3def)));
							pixel += 2;
							--count;
						}
					} else {
						blendValue = ((unsigned int)value >> 1) & 0x7bef;
						while (count > 0) {
							destValue = FrontImage_ReadSurface16(pixel);
							FrontImage_WriteSurface16(pixel, (blendValue + ((destValue >> 1) & 0x7bef)));
							pixel += 2;
							--count;
						}
					}
					break;
				}

				value = image->colorLUT[source];
				pixel = dest + 2 * destOffset;
				if (g_pixelFormat555) {
					blendValue = ((unsigned int)value >> 1) & 0x3def;
					while (count > 0) {
						destValue = FrontImage_ReadSurface16(pixel);
						FrontImage_WriteSurface16(pixel, (blendValue + ((destValue >> 1) & 0x3def)));
						pixel += 2;
						--count;
					}
				} else {
					blendValue = ((unsigned int)value >> 1) & 0x7bef;
					while (count > 0) {
						destValue = FrontImage_ReadSurface16(pixel);
						FrontImage_WriteSurface16(pixel, (blendValue + ((destValue >> 1) & 0x7bef)));
						pixel += 2;
						--count;
					}
				}
				destOffset += token;
			}
		}

		row += rowLength;
		dest += 2 * (g_drawSurfacePitch >> 1);
		--height;
	}

	return 0;
}

// FUNCTION: XWA 0x5323B0
int FrontImage_BlitTranslucent(ImageResource* image, int x, int y) {
	FrontendRect srcRect;
	FrontendRect dstRect;
	int srcLeft;
	int srcTop;
	int visibleWidth;
	int visibleHeight;
	int clipResult;
	int row;
	int col;

	if (image == NULL) {
		return 0;
	}

	srcRect.left = x;
	srcRect.right = x + image->width - 1;
	srcRect.top = y;
	srcRect.bottom = y + image->height - 1;
	FrontendDraw_RectCopy(&dstRect, &srcRect);
	clipResult = FrontendDraw_RectClipToBounds(&srcRect);

	if (srcRect.right < srcRect.left) {
		return clipResult;
	}
	if (srcRect.bottom < srcRect.top) {
		return clipResult;
	}

	srcLeft = srcRect.left - dstRect.left;
	srcTop = srcRect.top - dstRect.top;
	visibleHeight = image->height + srcRect.bottom - dstRect.bottom - srcTop;
	visibleWidth = image->width + srcRect.right - dstRect.right - srcLeft;

	if (!image->isCompressed) {
		if (g_displayBpp != 8) {
			if (g_displayBpp == 16) {
				const char* src = (const char*)image->pixels + image->width * srcTop + srcLeft;
				unsigned short* dest =
					(unsigned short*)(g_drawSurfacePtr + g_drawSurfacePitch * (y + srcTop)) + (x + srcLeft);

				if (g_pixelFormat555) {
					for (row = 0; row < visibleHeight; ++row) {
						for (col = 0; col < visibleWidth; ++col) {
							if (src[col] != 0) {
								dest[col] = (unsigned short)(((dest[col] >> 1) & 0x3def) +
															 (((unsigned int)
																   image->colorLUT[(unsigned char)src[col]] >>
															   1) &
															  0x3def));
							}
						}
						src += image->width;
						dest += g_drawSurfacePitch >> 1;
					}
				} else {
					for (row = 0; row < visibleHeight; ++row) {
						for (col = 0; col < visibleWidth; ++col) {
							if (src[col] != 0) {
								dest[col] = (unsigned short)(((dest[col] >> 1) & 0x7bef) +
															 (((unsigned int)
																   image->colorLUT[(unsigned char)src[col]] >>
															   1) &
															  0x7bef));
							}
						}
						dest += g_drawSurfacePitch >> 1;
						src += image->width;
					}
				}
			}

			return clipResult;
		}

		{
			const char* src = (const char*)image->pixels + image->width * srcTop + srcLeft;
			unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (y + srcTop) + x + srcLeft;

			for (row = 0; row < visibleHeight; ++row) {
				for (col = 0; col < visibleWidth; ++col) {
					if (src[col] != 0) {
						dest[col] = src[col];
					}
				}
				src += image->width;
				dest += g_drawSurfacePitch;
			}
		}

		return clipResult;
	}

	if (g_displayBpp != 8) {
		if (g_displayBpp == 16) {
			FrontImage_BlitRLE16Translucent(image, x + srcLeft, y + srcTop, srcLeft, srcTop, visibleWidth,
											visibleHeight);
		}
		return clipResult;
	}

	FrontImage_BlitRLE8(image, x + srcLeft, y + srcTop, srcLeft, srcTop, visibleWidth, visibleHeight);
	return clipResult;
}

// FUNCTION: XWA 0x532350
int FrontImage_DrawSpriteTranslucent(const char* name, int x, int y) {
	int index;
	ResourceDescriptor* desc;

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}
#ifdef XWA_MODERN
	{
		/* Remaster snapshot observer (translucent). */
		const ResourceDescriptor* snapDesc = g_resourceTable[index].desc;
		XwaSnapshot_EmitSprite(XWA_DRAW2D_SPRITE_TRANSLUCENT, name, snapDesc->currentFrame, 0, x, y,
							   snapDesc->image[snapDesc->currentFrame].width,
							   snapDesc->image[snapDesc->currentFrame].height, 0, 0, 0);
	}
#endif

	desc = g_resourceTable[index].desc;
	if (desc->atlasBaseIndex != 0) {
		return 0;
	}

	return FrontImage_BlitTranslucent(&desc->image[desc->currentFrame], x, y);
}

// FUNCTION: XWA 0x5336E0
int FrontImage_BlitRectTransparent(ImageResource* image, FrontendRect* srcRect, int dstX, int dstY) {
	FrontendRect dstRect;
	FrontendRect clippedRect;
	int srcLeft;
	int srcTop;
	int visibleWidth;
	int visibleHeight;
	int clipResult;
	int row;

	if (image == NULL) {
		return 0;
	}

	FrontendDraw_RectCopy(&dstRect, srcRect);
	FrontendDraw_RectOffsetXY(&dstRect, dstX - srcRect->left, dstY - srcRect->top);
	FrontendDraw_RectCopy(&clippedRect, &dstRect);
	clipResult = FrontendDraw_RectClipToBounds(&dstRect);

	if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
		return clipResult;
	}

	srcLeft = dstRect.left - clippedRect.left;
	srcTop = dstRect.top - clippedRect.top;
	visibleWidth = dstRect.right + srcRect->right - clippedRect.right - srcLeft - srcRect->left + 1;
	visibleHeight = dstRect.bottom + srcRect->bottom - clippedRect.bottom - srcTop - srcRect->top + 1;

	if (g_displayBpp != 8) {
		if (g_displayBpp == 16) {
			const unsigned char* src =
				image->pixels + image->width * (srcRect->top + srcTop) + srcRect->left + srcLeft;
			unsigned char* dest =
				g_drawSurfacePtr + g_drawSurfacePitch * (dstY + srcTop) + 2 * (dstX + srcLeft);

			for (row = 0; row < visibleHeight; ++row) {
				int col;

				for (col = 0; col < visibleWidth; ++col) {
					if (src[col] != 0) {
						FrontImage_WriteSurface16(dest + 2 * col, image->colorLUT[src[col]]);
					}
				}

				src += image->width;
				dest += 2 * (g_drawSurfacePitch >> 1);
			}
		}
	} else {
		const unsigned char* src =
			image->pixels + image->width * (srcRect->top + srcTop) + srcRect->left + srcLeft;
		unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + srcTop) + dstX + srcLeft;

		for (row = 0; row < visibleHeight; ++row) {
			int col;

			for (col = 0; col < visibleWidth; ++col) {
				if (src[col] != 0) {
					dest[col] = src[col];
				}
			}

			src += image->width;
			dest += g_drawSurfacePitch;
		}
	}

	return clipResult;
}

// FUNCTION: XWA 0x533670
int FrontImage_DrawSpriteRectTransparent(const char* name, FrontendRect* srcRect, int dstX, int dstY) {
	int index;
	ResourceDescriptor* desc;

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}
#ifdef XWA_MODERN
	{
		/* Remaster snapshot observer. */
		int16_t snapSrc[4] = { (int16_t)srcRect->left, (int16_t)srcRect->top, (int16_t)srcRect->right,
							   (int16_t)srcRect->bottom };
		const ResourceDescriptor* snapDesc = g_resourceTable[index].desc;
		XwaSnapshot_EmitSprite(XWA_DRAW2D_SPRITE_RECT, name, snapDesc->currentFrame, snapSrc, dstX, dstY,
							   snapDesc->image[snapDesc->currentFrame].width,
							   snapDesc->image[snapDesc->currentFrame].height, 0, 0, 0);
	}
#endif

	desc = g_resourceTable[index].desc;
	if (desc->image->isCompressed != 0) {
		return 0;
	}

	if (desc->atlasBaseIndex != 0) {
		return 0;
	}

	return FrontImage_BlitRectTransparent(&desc->image[desc->currentFrame], srcRect, dstX, dstY);
}

// FUNCTION: XWA 0x533930
int FrontImage_BlitRectTinted(ImageResource* image, FrontendRect* srcRect, int dstX, int dstY,
							  unsigned int tintColor) {
	FrontendRect dstRect;
	FrontendRect clippedRect;
	int srcLeft;
	int srcTop;
	int visibleWidth;
	int visibleHeight;
	int clipResult;
	int row;

	if (image == NULL) {
		return 0;
	}

	FrontendDraw_RectCopy(&dstRect, srcRect);
	FrontendDraw_RectOffsetXY(&dstRect, dstX - srcRect->left, dstY - srcRect->top);
	FrontendDraw_RectCopy(&clippedRect, &dstRect);
	clipResult = FrontendDraw_RectClipToBounds(&dstRect);

	if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
		return clipResult;
	}

	srcLeft = dstRect.left;
	srcLeft -= clippedRect.left;
	srcTop = dstRect.top;
	srcTop -= clippedRect.top;
	visibleWidth = srcRect->right;
	visibleWidth += dstRect.right;
	visibleWidth -= srcLeft;
	visibleWidth -= clippedRect.right;
	visibleWidth -= srcRect->left;
	++visibleWidth;
	visibleHeight = srcRect->bottom;
	visibleHeight += dstRect.bottom;
	visibleHeight -= srcTop;
	visibleHeight -= clippedRect.bottom;
	visibleHeight -= srcRect->top;
	++visibleHeight;

	if (g_displayBpp != 8) {
		if (g_displayBpp == 16) {
			const unsigned char* src =
				image->pixels + image->width * (srcRect->top + srcTop) + srcRect->left + srcLeft;
			unsigned char* dest =
				g_drawSurfacePtr + g_drawSurfacePitch * (dstY + srcTop) + 2 * (dstX + srcLeft);

			for (row = 0; row < visibleHeight; ++row) {
				int col;

				for (col = 0; col < visibleWidth; ++col) {
					if (src[col] != 0) {
						unsigned int color = tintColor;
						int sourceIntensity = image->colorLUT[src[col]];
						int red;
						int green;
						int blue;
						int redPart;

						sourceIntensity &= 0x1f;
						if (g_pixelFormat555) {
							red = (int)(color >> 10);
							green = (int)((color >> 5) & 0x1f);
						} else {
							red = (int)(color >> 11);
							green = (int)((color >> 5) & 0x3f);
						}

						red *= sourceIntensity;
						red /= 31;
						green *= sourceIntensity;
						green /= 31;
						color &= 0x1f;
						blue = (int)color * sourceIntensity / 31;

						if (g_pixelFormat555) {
							redPart = 32 * red;
						} else {
							redPart = red << 6;
						}

						FrontImage_WriteSurface16(dest + 2 * col, blue + 32 * (green + redPart));
					}
				}

				src += image->width;
				dest += 2 * (g_drawSurfacePitch >> 1);
			}
		}
	} else {
		const unsigned char* src =
			image->pixels + image->width * (srcRect->top + srcTop) + srcRect->left + srcLeft;
		unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + srcTop) + dstX + srcLeft;

		for (row = 0; row < visibleHeight; ++row) {
			int col;

			for (col = 0; col < visibleWidth; ++col) {
				if (src[col] != 0) {
					dest[col] = (unsigned char)tintColor;
				}
			}

			src += image->width;
			dest += g_drawSurfacePitch;
		}
	}

	return clipResult;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5338C0
int FrontImage_DrawSpriteRectTinted(const char* name, FrontendRect* srcRect, int dstX, int dstY,
									unsigned int tintColor) {
	int index;
	ResourceDescriptor* desc;

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}
#ifdef XWA_MODERN
	{
		/* Remaster snapshot observer. */
		int16_t snapSrc[4] = { (int16_t)srcRect->left, (int16_t)srcRect->top, (int16_t)srcRect->right,
							   (int16_t)srcRect->bottom };
		const ResourceDescriptor* snapDesc = g_resourceTable[index].desc;
		XwaSnapshot_EmitSprite(XWA_DRAW2D_SPRITE_RECT_TINTED, name, snapDesc->currentFrame, snapSrc, dstX,
							   dstY, snapDesc->image[snapDesc->currentFrame].width,
							   snapDesc->image[snapDesc->currentFrame].height, tintColor, 0, 0);
	}
#endif

	desc = g_resourceTable[index].desc;
	if (desc->image->isCompressed != 0) {
		return 0;
	}

	if (desc->atlasBaseIndex != 0) {
		return 0;
	}

	return FrontImage_BlitRectTinted(&g_resourceTable[index].desc->image[desc->currentFrame], srcRect, dstX,
									 dstY, tintColor);
}

// FUNCTION: XWA 0x532D00
int FrontImage_BlitRectBlendMode(ImageResource* image, FrontendRect* srcRect, int dstX, int dstY,
								 int blendMode) {
	FrontendRect dstRect;
	FrontendRect savedRect;
	int clipX;
	int clipY;
	int visibleWidth;
	int visibleHeight;
	int sourceWidth;
	int row;

	if (image == NULL) {
		return 0;
	}

	switch (blendMode) {
		case 1:
			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, -srcRect->left, -srcRect->top);
			visibleWidth = dstRect.right - dstRect.left + 1;
			visibleHeight = dstRect.bottom - dstRect.top + 1;
			sourceWidth = visibleWidth;
			dstRect.right = dstRect.left + visibleHeight - 1;
			dstRect.bottom = dstRect.top + visibleWidth - 1;
			FrontendDraw_RectOffsetXY(&dstRect, dstX, dstY);
			FrontendDraw_RectCopy(&savedRect, &dstRect);
			blendMode = FrontendDraw_RectClipToBounds(&dstRect);

			if (dstRect.right < dstRect.left) {
				return blendMode;
			}
			if (dstRect.bottom < dstRect.top) {
				return blendMode;
			}

			clipX = dstRect.left - savedRect.left;
			clipY = dstRect.top - savedRect.top;
			visibleWidth = visibleHeight + dstRect.right - savedRect.right - clipX;
			visibleHeight = sourceWidth + dstRect.bottom - savedRect.bottom - clipY;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (srcRect->top + clipX) + srcRect->right - clipY;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);

					if (visibleHeight > 0) {
						for (row = 0; row < visibleHeight; ++row) {
							int col;

							for (col = 0; col < visibleWidth; ++col) {
								if (src[image->width * col] != 0) {
									FrontImage_WriteSurface16Lut(dest + 2 * col,
																 &image->colorLUT[src[image->width * col]]);
								}
							}

							--src;
							dest += 2 * (g_drawSurfacePitch >> 1);
						}
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (srcRect->top + clipX) + srcRect->right - clipY;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;

				if (visibleHeight > 0) {
					for (row = 0; row < visibleHeight; ++row) {
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							if (src[image->width * col] != 0) {
								dest[col] = src[image->width * col];
							}
						}

						--src;
						dest += g_drawSurfacePitch;
					}
				}
			}
			break;

		case 2:
			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, dstX - srcRect->left, dstY - srcRect->top);
			FrontendDraw_RectCopy(&savedRect, &dstRect);
			blendMode = FrontendDraw_RectClipToBounds(&dstRect);

			if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
				return blendMode;
			}

			clipX = dstRect.left - savedRect.left;
			clipY = dstRect.top - savedRect.top;
			visibleWidth = dstRect.right + srcRect->right - savedRect.right - clipX - srcRect->left + 1;
			visibleHeight = dstRect.bottom + srcRect->bottom - savedRect.bottom - clipY - srcRect->top + 1;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (srcRect->bottom - clipY) + srcRect->left + clipX;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);

					if (visibleHeight > 0) {
						for (row = 0; row < visibleHeight; ++row) {
							int col;

							for (col = 0; col < visibleWidth; ++col) {
								if (src[col] != 0) {
									FrontImage_WriteSurface16Lut(dest + 2 * col, &image->colorLUT[src[col]]);
								}
							}

							src -= image->width;
							dest += 2 * (g_drawSurfacePitch >> 1);
						}
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (srcRect->bottom - clipY) + srcRect->left + clipX;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;

				if (visibleHeight > 0) {
					for (row = 0; row < visibleHeight; ++row) {
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							if (src[col] != 0) {
								dest[col] = src[col];
							}
						}

						src -= image->width;
						dest += g_drawSurfacePitch;
					}
				}
			}
			break;

		case 3:
			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, -srcRect->left, -srcRect->top);
			visibleWidth = dstRect.right - dstRect.left + 1;
			visibleHeight = dstRect.bottom - dstRect.top + 1;
			sourceWidth = visibleWidth;
			dstRect.right = dstRect.left + visibleHeight - 1;
			dstRect.bottom = dstRect.top + visibleWidth - 1;
			FrontendDraw_RectOffsetXY(&dstRect, dstX, dstY);
			FrontendDraw_RectCopy(&savedRect, &dstRect);
			blendMode = FrontendDraw_RectClipToBounds(&dstRect);

			if (dstRect.right < dstRect.left) {
				return blendMode;
			}
			if (dstRect.bottom < dstRect.top) {
				return blendMode;
			}

			clipX = dstRect.left - savedRect.left;
			clipY = dstRect.top - savedRect.top;
			visibleWidth = visibleHeight + dstRect.right - savedRect.right - clipX;
			visibleHeight = sourceWidth + dstRect.bottom - savedRect.bottom - clipY;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (srcRect->bottom - clipX) + srcRect->left + clipY;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);

					if (visibleHeight > 0) {
						for (row = 0; row < visibleHeight; ++row) {
							int col;

							for (col = 0; col < visibleWidth; ++col) {
								if (src[-image->width * col] != 0) {
									FrontImage_WriteSurface16Lut(dest + 2 * col,
																 &image->colorLUT[src[-image->width * col]]);
								}
							}

							++src;
							dest += 2 * (g_drawSurfacePitch >> 1);
						}
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (srcRect->bottom - clipX) + srcRect->left + clipY;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;

				if (visibleHeight > 0) {
					for (row = 0; row < visibleHeight; ++row) {
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							if (src[-image->width * col] != 0) {
								dest[col] = src[-image->width * col];
							}
						}

						++src;
						dest += g_drawSurfacePitch;
					}
				}
			}
			break;

		case 4:
			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, dstX - srcRect->left, dstY - srcRect->top);
			FrontendDraw_RectCopy(&savedRect, &dstRect);
			blendMode = FrontendDraw_RectClipToBounds(&dstRect);

			if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
				return blendMode;
			}

			clipX = dstRect.left - savedRect.left;
			clipY = dstRect.top - savedRect.top;
			visibleWidth = dstRect.right + srcRect->right - savedRect.right - clipX - srcRect->left + 1;
			visibleHeight = dstRect.bottom + srcRect->bottom - savedRect.bottom - clipY - srcRect->top + 1;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (srcRect->top + clipY) + srcRect->right - clipX;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);

					if (visibleHeight > 0) {
						for (row = 0; row < visibleHeight; ++row) {
							int col;

							for (col = 0; col < visibleWidth; ++col) {
								if (src[-col] != 0) {
									FrontImage_WriteSurface16Lut(dest + 2 * col, &image->colorLUT[src[-col]]);
								}
							}

							src += image->width;
							dest += 2 * (g_drawSurfacePitch >> 1);
						}
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (srcRect->top + clipY) + srcRect->right - clipX;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;

				if (visibleHeight > 0) {
					for (row = 0; row < visibleHeight; ++row) {
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							if (src[-col] != 0) {
								dest[col] = src[-col];
							}
						}

						src += image->width;
						dest += g_drawSurfacePitch;
					}
				}
			}
			break;

		default:
			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, dstX - srcRect->left, dstY - srcRect->top);
			FrontendDraw_RectCopy(&savedRect, &dstRect);
			blendMode = FrontendDraw_RectClipToBounds(&dstRect);

			if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
				return blendMode;
			}

			clipX = dstRect.left - savedRect.left;
			clipY = dstRect.top - savedRect.top;
			visibleWidth = dstRect.right + srcRect->right - savedRect.right - clipX - srcRect->left + 1;
			visibleHeight = dstRect.bottom + srcRect->bottom - savedRect.bottom - clipY - srcRect->top + 1;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (srcRect->top + clipY) + srcRect->left + clipX;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);

					if (visibleHeight > 0) {
						for (row = 0; row < visibleHeight; ++row) {
							int col;

							for (col = 0; col < visibleWidth; ++col) {
								if (src[col] != 0) {
									FrontImage_WriteSurface16Lut(dest + 2 * col, &image->colorLUT[src[col]]);
								}
							}
							src += image->width;
							dest += 2 * (g_drawSurfacePitch >> 1);
						}
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (srcRect->top + clipY) + srcRect->left + clipX;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;

				if (visibleHeight > 0) {
					for (row = 0; row < visibleHeight; ++row) {
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							if (src[col] != 0) {
								dest[col] = src[col];
							}
						}

						src += image->width;
						dest += g_drawSurfacePitch;
					}
				}
			}
			break;
	}

	return blendMode;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x532C90
int FrontImage_DrawSpriteRectBlendMode(const char* name, FrontendRect* srcRect, int dstX, int dstY,
									   int blendMode) {
	int index;
	ResourceDescriptor* desc;

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}
#ifdef XWA_MODERN
	{
		/* Remaster snapshot observer. */
		int16_t snapSrc[4] = { (int16_t)srcRect->left, (int16_t)srcRect->top, (int16_t)srcRect->right,
							   (int16_t)srcRect->bottom };
		const ResourceDescriptor* snapDesc = g_resourceTable[index].desc;
		XwaSnapshot_EmitSprite(XWA_DRAW2D_SPRITE_RECT_BLEND, name, snapDesc->currentFrame, snapSrc, dstX,
							   dstY, snapDesc->image[snapDesc->currentFrame].width,
							   snapDesc->image[snapDesc->currentFrame].height, 0, 0, blendMode);
	}
#endif

	desc = g_resourceTable[index].desc;
	if (desc->image->isCompressed != 0) {
		return 0;
	}

	if (desc->atlasBaseIndex != 0) {
		return 0;
	}

	return FrontImage_BlitRectBlendMode(&g_resourceTable[index].desc->image[desc->currentFrame], srcRect,
										dstX, dstY, blendMode);
}

// FUNCTION: XWA 0x533C70
int FrontImage_BlitRectTintedBlendMode(ImageResource* image, FrontendRect* srcRect, int dstX, int dstY,
									   unsigned int tintColor, int blendMode) {
	int clipResult;
	int clipX;
	int clipY;
	int visibleWidth;
	int visibleHeight;
	FrontendRect dstRect;
	FrontendRect srcRectCopy;

	if (image == NULL) {
		return 0;
	}

	switch (blendMode) {
		case 1: {
			int srcWidth;
			int srcHeight;

			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, -srcRect->left, -srcRect->top);
			srcWidth = dstRect.right - dstRect.left + 1;
			srcHeight = dstRect.bottom - dstRect.top + 1;
			dstRect.right = srcHeight + dstRect.left - 1;
			dstRect.bottom = srcWidth + dstRect.top - 1;
			FrontendDraw_RectOffsetXY(&dstRect, dstX, dstY);
			FrontendDraw_RectCopy(&srcRectCopy, &dstRect);
			clipResult = FrontendDraw_RectClipToBounds(&dstRect);
			if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
				return clipResult;
			}

			clipX = dstRect.left - srcRectCopy.left;
			clipY = dstRect.top - srcRectCopy.top;
			visibleWidth = dstRect.right + srcHeight - clipX - srcRectCopy.right;
			visibleHeight = srcWidth + dstRect.bottom - clipY - srcRectCopy.bottom;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + srcRect->right + image->width * (srcRect->top + clipX) - clipY;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);
					int row;

					if (visibleHeight <= 0) {
						return clipResult;
					}

					for (row = visibleHeight; row != 0; --row) {
						unsigned char* rowDest = dest;
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							int srcValue = src[image->width * col];
							if (srcValue != 0) {
								int sourceIntensity = image->colorLUT[srcValue] & 0x1f;
								int tintRed;
								int tintGreen;
								int tintBlue;
								int red;
								int green;
								int blue;
								int redPart;

								if (g_pixelFormat555) {
									tintRed = (int)(tintColor >> 10);
									tintGreen = (int)((tintColor >> 5) & 0x1f);
								} else {
									tintRed = (int)(tintColor >> 11);
									tintGreen = (int)((tintColor >> 5) & 0x3f);
								}

								tintBlue = (int)(tintColor & 0x1f);
								red = sourceIntensity * tintRed / 31;
								green = sourceIntensity * tintGreen / 31;
								blue = sourceIntensity * tintBlue / 31;

								if (g_pixelFormat555) {
									redPart = 32 * red;
								} else {
									redPart = red << 6;
								}

								FrontImage_WriteSurface16(rowDest, blue + 32 * (green + redPart));
							}
							rowDest += 2;
						}

						--src;
						dest += 2 * (g_drawSurfacePitch >> 1);
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + srcRect->right + image->width * (srcRect->top + clipX) - clipY;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;
				int row;

				if (visibleHeight <= 0) {
					return clipResult;
				}

				for (row = visibleHeight; row != 0; --row) {
					int col;

					for (col = 0; col < visibleWidth; ++col) {
						unsigned char srcValue = src[image->width * col];
						if (srcValue != 0) {
							dest[col] = srcValue;
						}
					}

					--src;
					dest += g_drawSurfacePitch;
				}
			}

			return clipResult;
		}

		case 2: {
			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, dstX - srcRect->left, dstY - srcRect->top);
			FrontendDraw_RectCopy(&srcRectCopy, &dstRect);
			clipResult = FrontendDraw_RectClipToBounds(&dstRect);
			if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
				return clipResult;
			}

			clipY = dstRect.top - srcRectCopy.top;
			clipX = dstRect.left - srcRectCopy.left;
			visibleWidth = dstRect.right + srcRect->right - clipX - srcRectCopy.right - srcRect->left + 1;
			visibleHeight = srcRect->bottom + dstRect.bottom - clipY - srcRectCopy.bottom - srcRect->top + 1;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (srcRect->bottom - clipY) + srcRect->left + clipX;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);
					int row;

					if (visibleHeight <= 0) {
						return clipResult;
					}

					for (row = visibleHeight; row != 0; --row) {
						unsigned char* rowDest = dest;
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							int srcValue = src[col];
							if (srcValue != 0) {
								int sourceIntensity = image->colorLUT[srcValue] & 0x1f;
								int tintRed;
								int tintGreen;
								int tintBlue;
								int red;
								int green;
								int blue;
								int redPart;

								if (g_pixelFormat555) {
									tintRed = (int)(tintColor >> 10);
									tintGreen = (int)((tintColor >> 5) & 0x1f);
								} else {
									tintRed = (int)(tintColor >> 11);
									tintGreen = (int)((tintColor >> 5) & 0x3f);
								}

								tintBlue = (int)(tintColor & 0x1f);
								red = sourceIntensity * tintRed / 31;
								green = sourceIntensity * tintGreen / 31;
								blue = sourceIntensity * tintBlue / 31;

								if (g_pixelFormat555) {
									redPart = 32 * red;
								} else {
									redPart = red << 6;
								}

								FrontImage_WriteSurface16(rowDest, blue + 32 * (green + redPart));
							}
							rowDest += 2;
						}

						src -= image->width;
						dest += 2 * (g_drawSurfacePitch >> 1);
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (srcRect->bottom - clipY) + srcRect->left + clipX;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;
				int row;

				if (visibleHeight <= 0) {
					return clipResult;
				}

				for (row = visibleHeight; row != 0; --row) {
					int col;

					for (col = 0; col < visibleWidth; ++col) {
						if (src[col] != 0) {
							dest[col] = src[col];
						}
					}

					src -= image->width;
					dest += g_drawSurfacePitch;
				}
			}

			return clipResult;
		}

		case 3: {
			int srcWidth;
			int srcHeight;

			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, -srcRect->left, -srcRect->top);
			srcWidth = dstRect.right - dstRect.left + 1;
			srcHeight = dstRect.bottom - dstRect.top + 1;
			dstRect.right = srcHeight + dstRect.left - 1;
			dstRect.bottom = srcWidth + dstRect.top - 1;
			FrontendDraw_RectOffsetXY(&dstRect, dstX, dstY);
			FrontendDraw_RectCopy(&srcRectCopy, &dstRect);
			clipResult = FrontendDraw_RectClipToBounds(&dstRect);
			if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
				return clipResult;
			}

			clipX = dstRect.left - srcRectCopy.left;
			clipY = dstRect.top - srcRectCopy.top;
			visibleWidth = dstRect.right + srcHeight - clipX - srcRectCopy.right;
			visibleHeight = srcWidth + dstRect.bottom - clipY - srcRectCopy.bottom;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (srcRect->bottom - clipX) + srcRect->left + clipY;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);
					int row;

					if (visibleHeight <= 0) {
						return clipResult;
					}

					for (row = visibleHeight; row != 0; --row) {
						unsigned char* rowDest = dest;
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							int srcValue = src[-image->width * col];
							if (srcValue != 0) {
								int sourceIntensity = image->colorLUT[srcValue] & 0x1f;
								int tintRed;
								int tintGreen;
								int tintBlue;
								int red;
								int green;
								int blue;
								int redPart;

								if (g_pixelFormat555) {
									tintRed = (int)(tintColor >> 10);
									tintGreen = (int)((tintColor >> 5) & 0x1f);
								} else {
									tintRed = (int)(tintColor >> 11);
									tintGreen = (int)((tintColor >> 5) & 0x3f);
								}

								tintBlue = (int)(tintColor & 0x1f);
								red = sourceIntensity * tintRed / 31;
								green = sourceIntensity * tintGreen / 31;
								blue = sourceIntensity * tintBlue / 31;

								if (g_pixelFormat555) {
									redPart = 32 * red;
								} else {
									redPart = red << 6;
								}

								FrontImage_WriteSurface16(rowDest, blue + 32 * (green + redPart));
							}
							rowDest += 2;
						}

						++src;
						dest += 2 * (g_drawSurfacePitch >> 1);
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (srcRect->bottom - clipX) + srcRect->left + clipY;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;
				int row;

				if (visibleHeight <= 0) {
					return clipResult;
				}

				for (row = visibleHeight; row != 0; --row) {
					int col;

					for (col = 0; col < visibleWidth; ++col) {
						unsigned char srcValue = src[-image->width * col];
						if (srcValue != 0) {
							dest[col] = srcValue;
						}
					}

					++src;
					dest += g_drawSurfacePitch;
				}
			}

			return clipResult;
		}

		case 4: {
			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, dstX - srcRect->left, dstY - srcRect->top);
			FrontendDraw_RectCopy(&srcRectCopy, &dstRect);
			clipResult = FrontendDraw_RectClipToBounds(&dstRect);
			if (dstRect.right < dstRect.left) {
				return clipResult;
			}
			if (dstRect.bottom < dstRect.top) {
				return clipResult;
			}

			clipX = dstRect.left - srcRectCopy.left;
			clipY = dstRect.top - srcRectCopy.top;
			visibleWidth = dstRect.right - srcRect->left - clipX - srcRectCopy.right + srcRect->right + 1;
			visibleHeight = dstRect.bottom + srcRect->bottom - clipY - srcRectCopy.bottom - srcRect->top + 1;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (clipY + srcRect->top) + srcRect->right - clipX;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);
					int row;

					if (visibleHeight <= 0) {
						return clipResult;
					}

					for (row = visibleHeight; row != 0; --row) {
						const unsigned char* srcPixel = src;
						unsigned char* rowDest = dest;
						int col;

						for (col = visibleWidth; col != 0; --col) {
							if (*srcPixel != 0) {
								int srcValue = *srcPixel;
								int sourceIntensity = image->colorLUT[srcValue] & 0x1f;
								int tintRed;
								int tintGreen;
								int tintBlue;
								int red;
								int green;
								int blue;
								int redPart;

								if (g_pixelFormat555) {
									tintRed = (int)(tintColor >> 10);
									tintGreen = (int)((tintColor >> 5) & 0x1f);
								} else {
									tintRed = (int)(tintColor >> 11);
									tintGreen = (int)((tintColor >> 5) & 0x3f);
								}

								tintBlue = (int)(tintColor & 0x1f);
								red = sourceIntensity * tintRed / 31;
								green = sourceIntensity * tintGreen / 31;
								blue = sourceIntensity * tintBlue / 31;

								if (g_pixelFormat555) {
									redPart = 32 * red;
								} else {
									redPart = red << 6;
								}

								FrontImage_WriteSurface16(rowDest, blue + 32 * (green + redPart));
							}
							--srcPixel;
							rowDest += 2;
						}

						src += image->width;
						dest += 2 * (g_drawSurfacePitch >> 1);
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (clipY + srcRect->top) + srcRect->right - clipX;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;
				int row;

				if (visibleHeight <= 0) {
					return clipResult;
				}

				for (row = visibleHeight; row != 0; --row) {
					const unsigned char* srcPixel = src;
					int col;

					for (col = 0; col < visibleWidth; ++col) {
						if (*srcPixel != 0) {
							dest[col] = *srcPixel;
						}
						--srcPixel;
					}

					src += image->width;
					dest += g_drawSurfacePitch;
				}
			}

			return clipResult;
		}

		default: {
			FrontendDraw_RectCopy(&dstRect, srcRect);
			FrontendDraw_RectOffsetXY(&dstRect, dstX - srcRect->left, dstY - srcRect->top);
			FrontendDraw_RectCopy(&srcRectCopy, &dstRect);
			clipResult = FrontendDraw_RectClipToBounds(&dstRect);
			if (dstRect.right < dstRect.left || dstRect.bottom < dstRect.top) {
				return clipResult;
			}

			clipX = dstRect.left - srcRectCopy.left;
			clipY = dstRect.top - srcRectCopy.top;
			visibleWidth = dstRect.right + srcRect->right - clipX - srcRectCopy.right - srcRect->left + 1;
			visibleHeight = dstRect.bottom + srcRect->bottom - clipY - srcRectCopy.bottom - srcRect->top + 1;

			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* src =
						image->pixels + image->width * (clipY + srcRect->top) + srcRect->left + clipX;
					unsigned char* dest =
						g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + 2 * (dstX + clipX);
					int row;

					if (visibleHeight <= 0) {
						return clipResult;
					}

					for (row = visibleHeight; row != 0; --row) {
						unsigned char* rowDest = dest;
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							int srcValue = src[col];
							if (srcValue != 0) {
								int sourceIntensity = image->colorLUT[srcValue] & 0x1f;
								int tintRed;
								int tintGreen;
								int tintBlue;
								int red;
								int green;
								int blue;
								int redPart;

								if (g_pixelFormat555) {
									tintRed = (int)(tintColor >> 10);
									tintGreen = (int)((tintColor >> 5) & 0x1f);
								} else {
									tintRed = (int)(tintColor >> 11);
									tintGreen = (int)((tintColor >> 5) & 0x3f);
								}

								tintBlue = (int)(tintColor & 0x1f);
								red = sourceIntensity * tintRed / 31;
								green = sourceIntensity * tintGreen / 31;
								blue = sourceIntensity * tintBlue / 31;

								if (g_pixelFormat555) {
									redPart = 32 * red;
								} else {
									redPart = red << 6;
								}

								FrontImage_WriteSurface16(rowDest, blue + 32 * (green + redPart));
							}
							rowDest += 2;
						}

						src += image->width;
						dest += 2 * (g_drawSurfacePitch >> 1);
					}
				}
			} else {
				const unsigned char* src =
					image->pixels + image->width * (clipY + srcRect->top) + srcRect->left + clipX;
				unsigned char* dest = g_drawSurfacePtr + g_drawSurfacePitch * (dstY + clipY) + dstX + clipX;
				int row;

				if (visibleHeight > 0) {
					for (row = visibleHeight; row != 0; --row) {
						int col;

						for (col = 0; col < visibleWidth; ++col) {
							if (src[col] != 0) {
								dest[col] = src[col];
							}
						}

						src += image->width;
						dest += g_drawSurfacePitch;
					}
				}
			}

			return clipResult;
		}
	}
}

// FUNCTION: XWA 0x533BF0
int FrontImage_DrawSpriteRectTintedBlendMode(const char* name, FrontendRect* srcRect, int dstX, int dstY,
											 unsigned int tintColor, int blendMode) {
	int index;
	ResourceDescriptor* desc;

	index = FrontImage_FindResourceByName(name);
	if (index == -1) {
		return 0;
	}
#ifdef XWA_MODERN
	{
		/* Remaster snapshot observer. */
		int16_t snapSrc[4] = { (int16_t)srcRect->left, (int16_t)srcRect->top, (int16_t)srcRect->right,
							   (int16_t)srcRect->bottom };
		const ResourceDescriptor* snapDesc = g_resourceTable[index].desc;
		XwaSnapshot_EmitSprite(XWA_DRAW2D_SPRITE_RECT_TINTED_BLEND, name, snapDesc->currentFrame, snapSrc,
							   dstX, dstY, snapDesc->image[snapDesc->currentFrame].width,
							   snapDesc->image[snapDesc->currentFrame].height, tintColor, 0, blendMode);
	}
#endif

	desc = g_resourceTable[index].desc;
	if (desc->image->isCompressed != 0) {
		return 0;
	}

	if (desc->atlasBaseIndex != 0) {
		return 0;
	}

	return FrontImage_BlitRectTintedBlendMode(&desc->image[desc->currentFrame], srcRect, dstX, dstY,
											  tintColor, blendMode);
}

// FUNCTION: XWA 0x564C50
int FrontImage_LoadResourceList(char* fileName) { return FrontImage_LoadResourceListImpl(fileName, 0); }

// FUNCTION: XWA 0x564D10
int FrontImage_UnloadResourceList(char* fileName) { return FrontImage_LoadResourceListImpl(fileName, 1); }

// FUNCTION: XWA 0x537A30
int FrontImage_LoadCbmResource(const char* srcFile, const char* name) {
	XwaFile* stream;
	ResourceDescriptor* desc;
	ResourceEntry entry;
	unsigned char header[FRONT_IMAGE_RESOURCE_FILE_SIZE];
	char fileName[FRONT_IMAGE_PATH_SIZE];
	int i;

	FrontImage_MakeCbmPath(srcFile, fileName, sizeof(fileName));
	stream = FrontImage_OpenRead(AERON_VFS_ROOT_USER, fileName);
	if (stream == NULL) {
		stream = FrontImage_OpenRead(AERON_VFS_ROOT_ASSET, fileName);
	}

	if (stream == NULL) {
		return 0;
	}

	desc = (ResourceDescriptor*)Mem_Alloc(sizeof(*desc));
	if (desc == NULL) {
		File_Close(stream);
		return 0;
	}

	memset(desc, 0, sizeof(*desc));
	if (!File_ReadCount(stream, header, FRONT_IMAGE_RESOURCE_DESCRIPTOR_FILE_SIZE)) {
		File_Close(stream);
		Mem_Free(desc);
		return 0;
	}

	FrontImage_ReadDescriptorHeader(header, desc);
	desc->image = (ImageResource*)Mem_Alloc(sizeof(*desc->image) * (size_t)desc->frameCount);
	if (desc->image == NULL) {
		File_Close(stream);
		Mem_Free(desc);
		return 0;
	}

	memset(desc->image, 0, sizeof(*desc->image) * (size_t)desc->frameCount);
	for (i = 0; i < desc->frameCount; ++i) {
		if (!File_ReadCount(stream, header, FRONT_IMAGE_RESOURCE_FILE_SIZE)) {
			File_Close(stream);
			FrontImage_FreeDescriptor(desc);
			return 0;
		}

		FrontImage_ReadImageHeader(header, &desc->image[i]);
		desc->image[i].pixels = (unsigned char*)Mem_Alloc((size_t)desc->image[i].pixelCount);
		if (desc->image[i].pixels == NULL) {
			File_Close(stream);
			FrontImage_FreeDescriptor(desc);
			return 0;
		}

		if (!File_ReadCount(stream, desc->image[i].pixels, (size_t)desc->image[i].pixelCount)) {
			File_Close(stream);
			FrontImage_FreeDescriptor(desc);
			return 0;
		}

		if (g_displayBpp == 16) {
			FrontImage_BuildColorLut16(&desc->image[i]);
		}
	}

	File_Close(stream);

	memset(&entry, 0, sizeof(entry));
	entry.desc = desc;
	strncpy(entry.name, name, sizeof(entry.name));
	FrontImage_InsertResourceSorted(&entry);
	return 1;
}

// FUNCTION: XWA 0x531EF0
int FrontImage_RegisterResource(const char* fileName, const char* name, int flags, int id) {
	ImageResource* image;
	ResourceDescriptor* desc;
	ResourceEntry entry;

	if (*fileName == '\0') {
		return 0;
	}

	if (*name == '\0') {
		return 0;
	}

	if (FrontImage_FindResourceByName(name) != -1) {
		return 0;
	}

	if (g_resourceCount >= FRONT_IMAGE_MAX_RESOURCES) {
		return 0;
	}

	if (FrontImage_LoadCbmResource(fileName, name) == 1) {
#ifdef XWA_MODERN
		FrontImage_NoteSnapshotResourceBinding(fileName, name);
#endif
		return 1;
	}

	if (
#ifdef XWA_MODERN
		FrontImage_HasExtension(fileName, ".flc")
#else
		Xwa_CrtStricmp(fileName + strlen(fileName) - 4, ".flc") == 0
#endif
	) {
		int result = FrontImage_RegisterFlicResource((char*)fileName, (char*)name, flags, id);
#ifdef XWA_MODERN
		if (!result) {
			Aeron_LogError("xwa.assets", "Failed to load FLIC frontend resource '%s' as '%s'", fileName, name);
		}
		if (result) {
			FrontImage_NoteSnapshotResourceBinding(fileName, name);
		}
#endif
		return result;
	}

	image = (ImageResource*)Mem_Alloc(sizeof(*image));
	if (image == NULL) {
		return 0;
	}

	memset(image, 0, sizeof(*image));
	if (!FrontImage_LoadBmpFile((char*)fileName, image, flags, id)) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.assets", "Failed to load BMP frontend resource '%s' as '%s'", fileName, name);
#endif
		Mem_Free(image);
		return 0;
	}

	desc = (ResourceDescriptor*)Mem_Alloc(sizeof(*desc));
	if (desc == NULL) {
		Mem_Free(image->pixels);
		Mem_Free(image);
		return 0;
	}

	desc->image = image;
	desc->currentFrame = 0;
	desc->atlasBaseIndex = 0;
	desc->atlasGroupId = 0;
	desc->frameCount = 1;
	FrontendDraw_RectCopy(&desc->bounds, (const FrontendRect*)&image->boundsLeft);
	FrontImage_WriteCbmResourceCache(fileName, desc);

	entry.desc = desc;
	strncpy(entry.name, name, sizeof(entry.name));
	FrontImage_InsertResourceSorted(&entry);
#ifdef XWA_MODERN
	FrontImage_NoteSnapshotResourceBinding(fileName, name);
#endif
	return 1;
}

// FUNCTION: XWA 0x531D70
int FrontImage_RegisterResourceDefault(const char* fileName, const char* name) {
	ImageResource* image;
	ResourceDescriptor* desc;
	ResourceEntry entry;

	if (*fileName == '\0') {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.assets", "Failed to register default frontend resource with empty file name as '%s'",
				  name);
#endif
		return 0;
	}

	if (*name == '\0') {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.assets", "Failed to register default frontend resource '%s' with empty name",
				  fileName);
#endif
		return 0;
	}

	if (g_resourceCount >= FRONT_IMAGE_MAX_RESOURCES) {
#ifdef XWA_MODERN
		Aeron_LogWarn("xwa.assets", "No free frontend resource slots for default resource '%s' as '%s'", fileName,
				  name);
#endif
		return 0;
	}

	if (FrontImage_FindResourceByName(name) != -1) {
#ifdef XWA_MODERN
		Aeron_LogWarn("xwa.assets", "Default frontend resource name '%s' is already registered from '%s'", name,
				  fileName);
#endif
		return 0;
	}

	if (FrontImage_LoadCbmResource(fileName, name) == 1) {
#ifdef XWA_MODERN
		FrontImage_NoteSnapshotResourceBinding(fileName, name);
#endif
		return 1;
	}

	if (
#ifdef XWA_MODERN
		FrontImage_HasExtension(fileName, ".flc")
#else
		Xwa_CrtStricmp(fileName + strlen(fileName) - 4, ".flc") == 0
#endif
	) {
		int result = FrontImage_RegisterFlicResource((char*)fileName, (char*)name, 1, 1);
#ifdef XWA_MODERN
		if (!result) {
			Aeron_LogError("xwa.assets", "Failed to load default FLIC frontend resource '%s' as '%s'", fileName,
					  name);
		}
		if (result) {
			FrontImage_NoteSnapshotResourceBinding(fileName, name);
		}
#endif
		return result;
	}

	image = (ImageResource*)Mem_Alloc(sizeof(*image));
	if (image == NULL) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.assets", "Failed to allocate default frontend resource image '%s' as '%s'", fileName,
				  name);
#endif
		return 0;
	}

	memset(image, 0, sizeof(*image));
	if (!FrontImage_LoadBmpFile((char*)fileName, image, 1, 1)) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.assets", "Failed to load default BMP frontend resource '%s' as '%s'", fileName, name);
#endif
		Mem_Free(image);
		return 0;
	}

	desc = (ResourceDescriptor*)Mem_Alloc(sizeof(*desc));
	if (desc == NULL) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.assets", "Failed to allocate default frontend resource descriptor '%s' as '%s'",
				  fileName, name);
#endif
		Mem_Free(image->pixels);
		Mem_Free(image);
		return 0;
	}

	desc->image = image;
	desc->frameCount = 1;
	desc->currentFrame = 0;
	desc->atlasBaseIndex = 0;
	desc->atlasGroupId = 0;
	FrontendDraw_RectCopy(&desc->bounds, (const FrontendRect*)&image->boundsLeft);
	FrontImage_WriteCbmResourceCache(fileName, desc);

	entry.desc = desc;
	strncpy(entry.name, name, sizeof(entry.name));
	FrontImage_InsertResourceSorted(&entry);
#ifdef XWA_MODERN
	FrontImage_NoteSnapshotResourceBinding(fileName, name);
#endif
	return 1;
}

// FUNCTION: XWA 0x537D80
int FrontImage_WriteCbmResourceCache(const char* srcFile, ResourceDescriptor* desc) {
	XwaFile* stream;
	unsigned char descriptorHeader[FRONT_IMAGE_RESOURCE_DESCRIPTOR_FILE_SIZE];
	unsigned char imageHeader[FRONT_IMAGE_RESOURCE_FILE_SIZE];
	char fileName[FRONT_IMAGE_PATH_SIZE];
	int i;

	FrontImage_MakeCbmPath(srcFile, fileName, sizeof(fileName));
	stream = File_Open(AERON_VFS_ROOT_USER, fileName, "wb");
	if (stream == NULL) {
		return 0;
	}

	FrontImage_WriteDescriptorHeader(descriptorHeader, desc);
	if (!File_WriteCount(stream, descriptorHeader, FRONT_IMAGE_RESOURCE_DESCRIPTOR_FILE_SIZE)) {
		File_Close(stream);
		remove(fileName);
		return 0;
	}

	for (i = 0; i < desc->frameCount; ++i) {
		FrontImage_WriteImageHeader(imageHeader, &desc->image[i]);
		if (!File_WriteCount(stream, imageHeader, FRONT_IMAGE_RESOURCE_FILE_SIZE)) {
			File_Close(stream);
			remove(fileName);
			return 0;
		}

		if (!File_WriteCount(stream, desc->image[i].pixels, (size_t)desc->image[i].pixelCount)) {
			File_Close(stream);
			remove(fileName);
			return 0;
		}
	}

	File_Close(stream);
	return 1;
}

// FUNCTION: XWA 0x573DE0
int FrontImage_RegisterFlicResource(char* fileName, char* name, int remapPalette, int compress) {
	FlicState state;
	ResourceDescriptor* desc;
	FrontImageFlicPaletteEntry palette[256];
	int colorLut[256];
	int openResult;
	int width;
	int height;
	int frameIndex;
	int initialFrame;

	if (*name == '\0') {
		return 0;
	}

	if (FrontImage_FindResourceByName(name) != -1) {
		return 0;
	}

	if (g_resourceCount >= FRONT_IMAGE_MAX_RESOURCES) {
		return 0;
	}

	openResult = FlicOpen(fileName, &state, palette);
	initialFrame = 0;
	if (openResult == initialFrame) {
		return 0;
	}

	desc = (ResourceDescriptor*)Mem_Alloc(sizeof(*desc));
	if (desc == NULL) {
		FlicClose(&state);
		return 0;
	}

	desc->frameCount = state.frameCount;
	desc->currentFrame = initialFrame;
	desc->atlasBaseIndex = initialFrame;
	desc->atlasGroupId = initialFrame;
	width = state.width;
	height = state.height;
	desc->image = (ImageResource*)Mem_Alloc(sizeof(*desc->image) * (size_t)state.frameCount);
	if (desc->image == NULL) {
		Mem_Free(desc);
		FlicClose(&state);
		return 0;
	}

	if (g_displayBpp == 16) {
		int i;

		for (i = 0; i < 256; ++i) {
			int redPart;
			int greenPart;

			if (g_pixelFormat555) {
				redPart = palette[i].red >> 3;
				redPart <<= 5;
				greenPart = palette[i].green;
				greenPart >>= 3;
			} else {
				redPart = palette[i].red >> 3;
				redPart <<= 6;
				greenPart = palette[i].green;
				greenPart >>= 2;
			}

			redPart += greenPart;
			redPart <<= 5;
			redPart += palette[i].blue >> 3;
			colorLut[i] = redPart;
		}
	}

	frameIndex = initialFrame;
	if (desc->frameCount > 0) {
		size_t frameBytes = (size_t)(width * height);

		while (1) {
			ImageResource* image = &desc->image[frameIndex];
			int frameResult;

			memset(image, 0, sizeof(*image));
			image->pixels = (unsigned char*)Mem_Alloc(frameBytes);
			if (image->pixels == NULL) {
				desc->currentFrame = 1;
				break;
			}

			if (frameIndex != 0) {
				ImageResource* previous = &desc->image[frameIndex - 1];

				memcpy(image->pixels, previous->pixels, frameBytes);
				if (compress == 1) {
					FrontImage_CompressRLE(previous);
				}
			}

			image->width = width;
			image->height = height;
			image->pixelCount = (int)frameBytes;
			image->isCompressed = 0;
			frameResult = FlicDecodeNextFrame(&state, image, (unsigned char*)palette);
			if (frameResult == 0) {
				desc->currentFrame = 2;
				break;
			}

			FrontImage_ComputeImageBounds(image, &image->boundsLeft);
			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					if (frameResult < 0) {
						int i;

						for (i = 0; i < 256; ++i) {
							int redPart;
							int greenPart;

							if (g_pixelFormat555) {
								redPart = palette[i].red >> 3;
								redPart <<= 5;
								greenPart = palette[i].green;
								greenPart >>= 3;
							} else {
								redPart = palette[i].red >> 3;
								redPart <<= 6;
								greenPart = palette[i].green;
								greenPart >>= 2;
							}

							redPart += greenPart;
							redPart <<= 5;
							redPart += palette[i].blue >> 3;
							colorLut[i] = redPart;
						}
					}

					memcpy(image->palette, palette, sizeof(image->palette));
					memcpy(image->colorLUT, colorLut, sizeof(image->colorLUT));
				}
			} else {
				if (remapPalette == 1) {
					FrontImage_RemapPalette(image->pixels, (unsigned char*)palette, width, height);
				}
			}

			++frameIndex;
			if (frameIndex >= desc->frameCount) {
				break;
			}
		}
	}

	if (desc->currentFrame == 0) {
		FlicClose(&state);
		if (compress == 1) {
			FrontImage_CompressRLE(&desc->image[frameIndex - 1]);
		}

		FrontendDraw_RectAssign(&desc->bounds, 0x7fffffff, 0x7fffffff, 0, 0);
		if (desc->frameCount > 0) {
			int i;

			for (i = 0; i < desc->frameCount; ++i) {
				ImageResource* image = &desc->image[i];

				if (desc->bounds.left > image->boundsLeft) {
					desc->bounds.left = image->boundsLeft;
				}
				if (desc->bounds.top > image->boundsTop) {
					desc->bounds.top = image->boundsTop;
				}
				if (desc->bounds.right < image->boundsRight) {
					desc->bounds.right = image->boundsRight;
				}
				if (desc->bounds.bottom < image->boundsBottom) {
					desc->bounds.bottom = image->boundsBottom;
				}
			}
		}

		FrontImage_WriteCbmResourceCache(fileName, desc);
		{
			ResourceEntry entry;

			entry.desc = desc;
			strncpy(entry.name, name, sizeof(entry.name));
			FrontImage_InsertResourceSorted(&entry);
		}

		return 1;
	}

	if (desc->currentFrame == 1) {
		int i;

		for (i = 0; i < frameIndex; ++i) {
			Mem_Free(desc->image[i].pixels);
		}
	} else {
		int i;

		for (i = 0; i <= frameIndex; ++i) {
			Mem_Free(desc->image[i].pixels);
		}
	}

	Mem_Free(desc->image);
	Mem_Free(desc);
	FlicClose(&state);
	return state.file != NULL;
}

// FUNCTION: XWA 0x55D210
void FrontImage_ReadBmpPalette(XwaFile* stream, char* dest, int count) {
	FrontImageBmpPaletteEntry entry;
	int i;

	memset(dest, 0, 0x400);
	for (i = 0; i < count; ++i) {
		File_ReadCount(stream, &entry, sizeof(entry));
		dest[4 * i] = (char)entry.red;
		dest[4 * i + 1] = (char)entry.green;
		dest[4 * i + 2] = (char)entry.blue;
		dest[4 * i + 3] = 0;
	}
}

// FUNCTION: XWA 0x536CC0
int FrontImage_DecodeBmp4bpp(XwaFile* stream, void* dstPixels, const FrontImageBmpFileHeader* fileHeader,
							 int* infoHeader) {
	int dataSize;
	unsigned char* data;
	int* bitmapInfo;

	dataSize = (int)(fileHeader->fileSize - fileHeader->pixelOffset);
	data = (unsigned char*)Mem_Alloc(dataSize);
	if (data == NULL) {
		return 0;
	}

	bitmapInfo = infoHeader;
	if (bitmapInfo[4] == 0) {
		int srcOffset;
		int16_t row;

		File_ReadCount(stream, data, dataSize);
		srcOffset = 0;

		for (row = 0; row < bitmapInfo[2]; ++row) {
			int16_t col;
			int dstRowOffset = bitmapInfo[1] * (bitmapInfo[2] - row - 1);

			for (col = 0; col < bitmapInfo[1]; ++col) {
				if ((col & 1) != 0) {
					((unsigned char*)dstPixels)[dstRowOffset + col] = data[srcOffset] & 0x0f;
					++srcOffset;
				} else {
					((unsigned char*)dstPixels)[dstRowOffset + col] = data[srcOffset] >> 4;
				}
			}

			if ((bitmapInfo[1] & 1) != 0) {
				++srcOffset;
			}

			if ((bitmapInfo[1] & 6) != 0) {
				srcOffset += (int16_t)(4 - ((bitmapInfo[1] >> 1) & 3));
			}
		}
	}

	Mem_Free(data);
	return 1;
}

// FUNCTION: XWA 0x536DA0
int FrontImage_DecodeBmp8bpp(XwaFile* stream, void* dstPixels, void* fileHeader, int* infoHeader) {
	int padding;
	unsigned char* data;
	int compression;
	char paddingBytes[4];

	(void)fileHeader;

	padding = infoHeader[1] % 4;
	if (padding != 0) {
		padding = 4 - padding;
	}

	data = NULL;
	if (infoHeader[4] != 0) {
		data = (unsigned char*)Mem_Alloc((size_t)infoHeader[5]);
		if (data == NULL) {
			return 0;
		}
	}

	compression = infoHeader[4];
	if (compression != 0) {
		if (compression == 1) {
			int srcOffset;
			int destOffset;
			int rowBase;
			int16_t done;

			File_ReadCount(stream, data, (size_t)infoHeader[5]);
			srcOffset = 0;
			destOffset = infoHeader[2];
			--destOffset;
			destOffset *= infoHeader[1];
			rowBase = destOffset;
			done = 0;

			do {
				unsigned char count;

				count = data[srcOffset];
				++srcOffset;

				if (count == 0) {
					unsigned char command;

					command = data[srcOffset];
					++srcOffset;

					switch (command) {
						case 0:
							rowBase -= infoHeader[1];
							destOffset = rowBase;
							break;
						case 1:
							done = 1;
							break;
						case 2: {
							int rowOffset = destOffset - rowBase;
							int dx = data[srcOffset];
							int dy = data[srcOffset + 1];

							rowBase -= infoHeader[1] * dy;
							destOffset = rowBase + dx + rowOffset;
							srcOffset += 2;
							break;
						}
						default: {
							int literalCount = command;
							int i;

							for (i = 0; i < literalCount; ++i) {
								((unsigned char*)dstPixels)[destOffset++] = data[srcOffset++];
							}

							if ((command & 1) != 0) {
								++srcOffset;
							}
							break;
						}
					}
				} else {
					unsigned char value = data[srcOffset++];
					memset((unsigned char*)dstPixels + destOffset, value, count);
					destOffset += count;
				}
			} while (!done);
		}
	} else {
		int row;

		for (row = 0; row < infoHeader[2]; ++row) {
			File_ReadCount(stream, (unsigned char*)dstPixels + infoHeader[1] * (infoHeader[2] - row - 1),
						   (size_t)infoHeader[1]);
			File_ReadCount(stream, paddingBytes, (size_t)padding);
		}
	}

	if (data != NULL) {
		Mem_Free(data);
	}

	return 1;
}

// FUNCTION: XWA 0x537980
int FrontImage_ComputeImageBounds(ImageResource* image, int* outBounds) {
	int width;
	int scanY;
	int rightBound;
	int bottomBound;
	unsigned char* row;
	int rowIndex;
	int leftBound;
	int topBound;
	unsigned char* pixels;

	width = image->width;
	scanY = image->height - 1;
	rightBound = 0;
	bottomBound = 0;
	row = image->pixels + scanY * width;

	if (scanY > 0) {
		int maxRight = width - 1;

		do {
			int col;

			for (col = maxRight; col > rightBound; --col) {
				if (row[col] != 0) {
					if (scanY > bottomBound) {
						bottomBound = scanY;
					}

					rightBound = col;
					break;
				}
			}

			if (rightBound >= maxRight) {
				break;
			}

			row -= width;
			--scanY;
		} while (scanY > 0);
	}

	outBounds[3] = bottomBound++;
	rowIndex = 0;
	outBounds[2] = rightBound;
	topBound = rightBound + 1;
	pixels = image->pixels;
	leftBound = bottomBound;

	if (bottomBound > 0) {
		do {
			int col;

			for (col = 0; col < topBound; ++col) {
				if (pixels[col] != 0) {
					if (rowIndex < leftBound) {
						leftBound = rowIndex;
					}

					topBound = col;
					break;
				}
			}

			if (topBound == 0) {
				break;
			}

			pixels += image->width;
			++rowIndex;
		} while (rowIndex < bottomBound);
	}

	outBounds[1] = leftBound;
	outBounds[0] = topBound;
	return 1;
}

// FUNCTION: XWA 0x537040
int FrontImage_CompressRLE(ImageResource* image) {
	int originalSize;
	unsigned char* packedWrite;
	unsigned char* packed;
	unsigned char* rowPixels;
	int width;
	int height;
	int size;
	int rowIndex;

	originalSize = image->height * image->width;
	packed = (unsigned char*)Mem_Alloc((size_t)originalSize);
	if (packed == NULL) {
		image->isCompressed = 0;
		return 0;
	}

	width = image->boundsRight + 1;
	height = image->boundsBottom + 1;
	size = 0;
	packedWrite = packed;
	rowPixels = image->pixels;
	rowIndex = 0;

	while (rowIndex < height) {
		int encodedBytes;
		unsigned char* src;
		int consumed;
		unsigned char* lastToken;
		unsigned char* tokenWrite;
		unsigned char value;
		uint32_t rowSize;

		src = rowPixels;
		value = *src;
		encodedBytes = 0;
		consumed = 0;
		tokenWrite = g_rleRowBuffer + 4;
		lastToken = tokenWrite;
		*tokenWrite = 0;

		if (width <= 0) {
			g_rleRowBuffer[4] = 0x80;
			rowSize = 5;
#ifndef XWA_MODERN
			*(uint32_t*)g_rleRowBuffer = rowSize;
#else
			ByteOrder_WriteU32Le(g_rleRowBuffer, rowSize);
#endif
		} else {
			for (;;) {
				int runLength = 0;

				while (runLength < 63 && consumed < width && src[runLength] == value) {
					++runLength;
					++consumed;
				}

				if (value == 0) {
					*tokenWrite = (unsigned char)(runLength | 0x40);
					lastToken = tokenWrite++;
					++encodedBytes;
				} else if (runLength > 2) {
					*tokenWrite = (unsigned char)runLength;
					lastToken = tokenWrite;
					++tokenWrite;
					++encodedBytes;
					*tokenWrite = value;
					++tokenWrite;
					++encodedBytes;
				} else if ((*lastToken & 0x80u) == 0) {
					*tokenWrite = (unsigned char)(runLength | 0x80);
					lastToken = tokenWrite;
					++tokenWrite;
					++encodedBytes;
					if (runLength > 0) {
						unsigned char* literalWrite = tokenWrite;
						const unsigned char* literalRead = src;
						int remaining = runLength;

						do {
							*literalWrite++ = *literalRead++;
						} while (--remaining != 0);
					}
					tokenWrite += runLength;
					encodedBytes += runLength;
				} else {
					unsigned char literalLength = (unsigned char)(runLength + (*lastToken & 0x7f));

					if (literalLength < 0x80) {
						*lastToken = (unsigned char)(literalLength | 0x80);
						if (runLength > 0) {
							unsigned char* literalWrite = tokenWrite;
							const unsigned char* literalRead = src;
							int remaining = runLength;

							do {
								*literalWrite++ = *literalRead++;
							} while (--remaining != 0);
						}
					} else {
						*tokenWrite = (unsigned char)(runLength | 0x80);
						lastToken = tokenWrite++;
						++encodedBytes;
						if (runLength > 0) {
							unsigned char* literalWrite = tokenWrite;
							const unsigned char* literalRead = src;
							int remaining = runLength;

							do {
								*literalWrite++ = *literalRead++;
							} while (--remaining != 0);
						}
					}

					tokenWrite += runLength;
					encodedBytes += runLength;
				}

				if (consumed >= width) {
					break;
				}

				value = src[runLength];
				src += runLength;
			}

			*tokenWrite = 0x80;
#ifndef XWA_MODERN
			encodedBytes += 5;
			*(uint32_t*)g_rleRowBuffer = (uint32_t)encodedBytes;
			rowSize = (uint32_t)encodedBytes;
#else
			rowSize = (uint32_t)(encodedBytes + 5);
			ByteOrder_WriteU32Le(g_rleRowBuffer, rowSize);
#endif
		}
		if (size + (int)rowSize > originalSize) {
			Mem_Free(packed);
			size = image->height * image->width + 1;
			break;
		} else {
			memcpy(packedWrite, g_rleRowBuffer, rowSize);
			size += (int)rowSize;
			packedWrite += rowSize;
			rowPixels += image->width;
			++rowIndex;
		}
	}

	if (size > image->height * image->width) {
		return 0;
	}

	packedWrite = (unsigned char*)Mem_Realloc(packed, (size_t)size);
	if (packedWrite == NULL) {
		Mem_Free(packed);
		image->isCompressed = 0;
		return 0;
	}

	Mem_Free(image->pixels);
	image->pixels = packedWrite;
	image->width = width;
	image->isCompressed = 1;
	image->pixelCount = size;
	image->height = height;
	return 1;
}

// FUNCTION: XWA 0x535F60
int FrontImage_DrawGlyph(intptr_t* glyph, int x, int y, unsigned int color, int allowColorRemap) {
	int clipResult;
	FrontendRect src;
	FrontendRect dst;

	if (glyph == NULL) {
		return 0;
	}

	src.right = x + (int)glyph[0] - 1;
	src.top = y;
	src.left = x;
	src.bottom = y + (int)glyph[1] - 1;

	FrontendDraw_RectCopy(&dst, &src);
	clipResult = FrontendDraw_RectClipToBounds(&src);
	if (src.right < src.left) {
		return clipResult;
	}

	if (src.bottom < src.top) {
		return clipResult;
	}

	{
		int clipLeftSkip = src.left - dst.left;
		int clipTopSkip = src.top - dst.top;
		int visibleWidth = (int)glyph[0] + src.right - dst.right - clipLeftSkip;
		int visibleRows = (int)glyph[1] + src.bottom - dst.bottom - clipTopSkip;

		if (glyph[2] == 0) {
			if (g_displayBpp != 8) {
				if (g_displayBpp == 16) {
					const unsigned char* srcPixels;
					unsigned char* dest;

					if (allowColorRemap && color != 0 && g_glyphScratchTtl != 0) {
						color = FrontImage_GetFadedGlyphColor16(color);
					}

					srcPixels = FrontImage_GlyphData(glyph) + clipLeftSkip + (int)glyph[0] * clipTopSkip;
					dest = g_drawSurfacePtr + 2 * (x + clipLeftSkip) + g_drawSurfacePitch * (y + clipTopSkip);
					if (visibleRows > 0) {
						do {
							int column = 0;

							if (visibleWidth > 0) {
								do {
									if (srcPixels[column] != 0) {
										FrontImage_WriteSurface16(dest + 2 * column, (int)color);
									}
									++column;
								} while (column < visibleWidth);
							}

							srcPixels += (int)glyph[0];
							--visibleRows;
							dest += 2 * (g_drawSurfacePitch >> 1);
						} while (visibleRows != 0);
					}
				}

				return clipResult;
			}

			{
				const unsigned char* srcPixels =
					FrontImage_GlyphData(glyph) + clipLeftSkip + (int)glyph[0] * clipTopSkip;
				unsigned char* dest =
					g_drawSurfacePtr + x + clipLeftSkip + g_drawSurfacePitch * (y + clipTopSkip);

				if (visibleRows > 0) {
					y = visibleRows;
					do {
						if (visibleWidth > 0) {
							int column = visibleWidth;
							unsigned char* rowDest = dest;

							do {
								if (srcPixels[rowDest - dest] != 0) {
									*rowDest = (unsigned char)color;
								}
								++rowDest;
								--column;
							} while (column != 0);
						}

						srcPixels += (int)glyph[0];
						dest += g_drawSurfacePitch;
						--y;
					} while (y != 0);
				}
			}

			return clipResult;
		}

		if (g_displayBpp != 8) {
			if (g_displayBpp == 16) {
				if (allowColorRemap && color != 0 && g_glyphScratchTtl != 0) {
					color = FrontImage_GetFadedGlyphColor16(color);
				}

				FrontImage_BlitGlyphRLE_16bpp(glyph, x + clipLeftSkip, y + clipTopSkip, clipLeftSkip,
											  clipTopSkip, visibleWidth, visibleRows, color);
			}

			return clipResult;
		}

		FrontImage_BlitGlyphRLE_8bpp(glyph, x + clipLeftSkip, y + clipTopSkip, clipLeftSkip, clipTopSkip,
									 visibleWidth, visibleRows, (unsigned char)color);
		return clipResult;
	}
}

// FUNCTION: XWA 0x5361D0
void FrontImage_BlitGlyphRLE_8bpp(intptr_t* glyph, int destX, int destY, int clipLeftSkip, int clipTopSkip,
								  int visibleWidth, int visibleRows, unsigned char color) {
	const unsigned char* row;
	unsigned char* dest;
	unsigned char token;
	int destOffset;
	int y;

	if (visibleWidth == (int)glyph[0]) {
		dest = g_drawSurfacePtr + destX + g_drawSurfacePitch * destY;
		row = FrontImage_GlyphData(glyph);

		for (y = 0; y < clipTopSkip; ++y) {
			row += FrontImage_ReadGlyphRowLength(row);
		}

		if (visibleRows <= 0) {
			return;
		}

		y = visibleRows;
		destOffset = 0;
		row += 4;
		for (;;) {
			token = *row++;

			if (token != 0x80) {
				int count;

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					count = token;
					memset(dest + destOffset, color, count);
					destOffset += count;
					row += count;
					continue;
				}

				if ((token & 0x40u) != 0) {
					destOffset += token & 0x3f;
					continue;
				}

				count = token;
				++row;
				memset(dest + destOffset, color, count);
				destOffset += count;
				continue;
			}

			dest += g_drawSurfacePitch;
			if (--y != 0) {
				destOffset = 0;
				row += 4;
				continue;
			}

			break;
		}

		return;
	}

	{
		row = FrontImage_GlyphData(glyph);
		dest = g_drawSurfacePtr + destX + g_drawSurfacePitch * destY;

		for (y = 0; y < clipTopSkip; ++y) {
			row += FrontImage_ReadGlyphRowLength(row);
		}

		for (y = 0; y < visibleRows; ++y) {
			int rowLength;
			int tokenOffset;
			char started;

			rowLength = FrontImage_ReadGlyphRowLength(row);
			destOffset = 0;
			tokenOffset = 4;
			started = 0;

			for (;;) {
				token = *(row + tokenOffset);
				++tokenOffset;

				if (started == 0) {
					if (token == 0x80) {
						break;
					}

					if ((token & 0x80u) != 0) {
						token &= 0x7f;
						if (destOffset + token >= clipLeftSkip) {
							int skip = clipLeftSkip - destOffset;

							started = 1;
							tokenOffset += skip;
							token -= (unsigned char)skip;
							destOffset = 0;
							if (token != 0) {
								token |= 0x80;
							}
						} else {
							destOffset += token;
							tokenOffset += token;
						}
					} else if ((token & 0x40u) != 0) {
						token &= 0x3f;
						if (destOffset + token >= clipLeftSkip) {
							int skip = clipLeftSkip - destOffset;

							started = 1;
							token -= (unsigned char)skip;
							destOffset = 0;
							if (token != 0) {
								token |= 0x40;
							}
						} else {
							destOffset += token;
						}
					} else {
						if (destOffset + token >= clipLeftSkip) {
							int skip = clipLeftSkip - destOffset;

							started = 1;
							token -= (unsigned char)skip;
							destOffset = 0;
							if (token == 0) {
								++tokenOffset;
							}
						} else {
							++tokenOffset;
							destOffset += token;
						}
					}
				}

				if (started != 1 || token == 0) {
					continue;
				}

				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					token &= 0x7f;
					if (destOffset + token >= visibleWidth) {
						token = (unsigned char)(visibleWidth - destOffset);
						memset(dest + destOffset, color, token);
						break;
					}

					memset(dest + destOffset, color, token);
					destOffset += token;
					tokenOffset += token;
				} else if ((token & 0x40u) != 0) {
					destOffset += token & 0x3f;
					if (destOffset >= visibleWidth) {
						break;
					}
				} else {
					++tokenOffset;
					if (destOffset + token >= visibleWidth) {
						token = (unsigned char)(visibleWidth - destOffset);
						memset(dest + destOffset, color, token);
						break;
					}

					memset(dest + destOffset, color, token);
					destOffset += token;
				}
			}

			row += rowLength;
			dest += g_drawSurfacePitch;
		}
	}
}

// FUNCTION: XWA 0x536580
void FrontImage_BlitGlyphRLE_16bpp(intptr_t* glyph, int destX, int destY, int clipLeftSkip, int clipTopSkip,
								   int visibleWidth, int visibleRows, unsigned int color) {
	const unsigned char* row;
	unsigned char* dest;
	int y;

	if ((int)color != g_glyphGradientFgCached) {
		unsigned int fgRed;
		unsigned int fgGreen;
		unsigned int fgBlue;
		unsigned int bgRed;
		unsigned int bgGreen;
		unsigned int bgBlue;
		unsigned int bgColor;
		unsigned int redAccum;
		unsigned int greenAccum;
		unsigned int blueAccum;
		int weight;
		int* gradient;

		g_glyphGradientFgCached = (int)color;

		if (g_pixelFormat555) {
			fgRed = (color >> 10) & 0x1f;
			fgGreen = (color >> 5) & 0x1f;
		} else {
			fgRed = color >> 11;
			fgGreen = (color >> 5) & 0x3f;
		}

		bgColor = (unsigned int)g_glyphGradientBg;
		fgBlue = color & 0x1f;

		if (g_pixelFormat555) {
			bgRed = (bgColor >> 10) & 0x1f;
			bgGreen = (bgColor >> 5) & 0x1f;
		} else {
			bgRed = bgColor >> 11;
			bgGreen = (bgColor >> 5) & 0x3f;
		}

		bgBlue = bgColor & 0x1f;
		redAccum = fgRed;
		greenAccum = fgGreen;
		blueAccum = fgBlue;
		weight = 1;
		gradient = g_glyphGradientLut;

		while ((intptr_t)gradient < (intptr_t)(g_glyphGradientLut + 32)) {
			unsigned int inverse = (unsigned int)(32 - weight);
			unsigned int red;
			unsigned int green;

			if (g_pixelFormat555) {
				green = ((greenAccum + bgGreen * inverse) >> 5) & 0x1f;
				red = 32 * (((redAccum + bgRed * inverse) >> 5) & 0x1f);
			} else {
				green = ((greenAccum + bgGreen * inverse) >> 5) & 0x3f;
				red = (((redAccum + bgRed * inverse) >> 5) & 0x1f) << 6;
			}

			*gradient = (((blueAccum + bgBlue * inverse) >> 5) & 0x1f) + 32 * (red + green);
			redAccum += fgRed;
			greenAccum += fgGreen;
			blueAccum += fgBlue;
			++gradient;
			++weight;
		}
	}

	if (visibleWidth == (int)glyph[0]) {
		row = (const unsigned char*)(uintptr_t)glyph[8];
		dest = g_drawSurfacePtr + 2 * destX + g_drawSurfacePitch * destY;

		for (y = clipTopSkip; y > 0; --y) {
			row += FrontImage_ReadGlyphRowLength(row);
		}

		for (y = 0; y < visibleRows; ++y) {
			int destOffset = 0;

			row += 4;
			for (;;) {
				unsigned char token = *row++;
				int count;
				int i;

				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					count = token & 0x7f;
					for (i = 0; i < count; ++i) {
						FrontImage_WriteSurface16Lut(dest + 2 * (destOffset + i),
													 &g_glyphGradientLut[row[i] & 0x1f]);
					}
					destOffset += count;
					row += count;
				} else if ((token & 0x40u) != 0) {
					destOffset += token & 0x3f;
				} else {
					int fillCount = token;
					int value = g_glyphGradientLut[row[0] & 0x1f];
					unsigned char* fillDest = dest + 2 * destOffset;

					while (fillCount > 0) {
						FrontImage_WriteSurface16(fillDest, value);
						fillDest += 2;
						--fillCount;
					}
					destOffset += token;
					++row;
				}
			}

			dest += 2 * (g_drawSurfacePitch >> 1);
		}

		return;
	}

	row = (const unsigned char*)(uintptr_t)glyph[8];
	dest = g_drawSurfacePtr + 2 * destX + g_drawSurfacePitch * destY;

	for (y = clipTopSkip; y > 0; --y) {
		row += FrontImage_ReadGlyphRowLength(row);
	}

	for (y = 0; y < visibleRows; ++y) {
		const unsigned char* rowStart = row;
		int rowLength = FrontImage_ReadGlyphRowLength(rowStart);
		int tokenOffset = 4;
		int destOffset = 0;
		unsigned char started = 0;

		for (;;) {
			unsigned char token = rowStart[tokenOffset++];
			int count;

			if (!started) {
				if (token == 0x80) {
					break;
				}

				if ((token & 0x80u) != 0) {
					count = token & 0x7f;
					if (destOffset + count < clipLeftSkip) {
						destOffset += count;
						tokenOffset += count;
						continue;
					}

					tokenOffset += clipLeftSkip - destOffset;
					count -= clipLeftSkip - destOffset;
					started = 1;
					destOffset = 0;
					token = count != 0 ? (unsigned char)(count | 0x80) : 0;
				} else if ((token & 0x40u) != 0) {
					count = token & 0x3f;
					if (destOffset + count < clipLeftSkip) {
						destOffset += count;
						continue;
					}

					count -= clipLeftSkip - destOffset;
					started = 1;
					destOffset = 0;
					token = count != 0 ? (unsigned char)(count | 0x40) : 0;
				} else {
					count = token;
					if (destOffset + count < clipLeftSkip) {
						destOffset += count;
						++tokenOffset;
						continue;
					}

					count -= clipLeftSkip - destOffset;
					started = 1;
					destOffset = 0;
					token = (unsigned char)count;
					if (count == 0) {
						++tokenOffset;
					}
				}
			}

			if (started != 1 || token == 0) {
				continue;
			}

			if (token == 0x80) {
				break;
			}

			if ((token & 0x80u) != 0) {
				int i;

				count = token & 0x7f;
				if (destOffset + count >= visibleWidth) {
					count = visibleWidth - destOffset;
					for (i = 0; i < count; ++i) {
						FrontImage_WriteSurface16Lut(dest + 2 * (destOffset + i),
													 &g_glyphGradientLut[rowStart[tokenOffset + i] & 0x1f]);
					}
					break;
				}

				for (i = 0; i < count; ++i) {
					FrontImage_WriteSurface16Lut(dest + 2 * (destOffset + i),
												 &g_glyphGradientLut[rowStart[tokenOffset + i] & 0x1f]);
				}
				destOffset += count;
				tokenOffset += count;
			} else if ((token & 0x40u) != 0) {
				destOffset += token & 0x3f;
				if (destOffset >= visibleWidth) {
					break;
				}
			} else {
				int value = g_glyphGradientLut[rowStart[tokenOffset++] & 0x1f];

				count = token;
				if (destOffset + count >= visibleWidth) {
					int fillCount = visibleWidth - destOffset;

					unsigned char* fillDest = dest + 2 * destOffset;
					while (fillCount > 0) {
						FrontImage_WriteSurface16(fillDest, value);
						fillDest += 2;
						--fillCount;
					}
					break;
				}

				{
					int fillCount = count;

					unsigned char* fillDest = dest + 2 * destOffset;
					while (fillCount > 0) {
						FrontImage_WriteSurface16(fillDest, value);
						fillDest += 2;
						--fillCount;
					}
				}
				destOffset += count;
			}
		}

		row += rowLength;
		dest += 2 * (g_drawSurfacePitch >> 1);
	}

	return;
}

// FUNCTION: XWA 0x5372F0
int FrontImage_EncodeGlyphRow(int* rowRecord, unsigned char* srcPixels, int width) {
	unsigned char* rowStart;
	unsigned char* tokenWrite;
	int encodedBytes;
	int consumed;
	int rowSize;
	unsigned char* lastToken;
	unsigned char* src;
	unsigned char value;

	rowStart = (unsigned char*)rowRecord;
	src = srcPixels;
	tokenWrite = rowStart + 4;
	encodedBytes = 0;
	consumed = 0;
	lastToken = tokenWrite;
	value = *src;
	*tokenWrite = 0;

	if (width <= 0) {
		*tokenWrite = 0x80;
		*(int*)rowStart = 5;
		return 5;
	}

	for (;;) {
		int runLength;

		for (runLength = 0; runLength < 63; ++runLength) {
			if (consumed >= width) {
				break;
			}

			if (src[runLength] != value) {
				break;
			}

			++consumed;
		}

		if (value == 0) {
			*tokenWrite = (unsigned char)(runLength | 0x40);
			lastToken = tokenWrite++;
			++encodedBytes;
		} else if (runLength > 2) {
			*tokenWrite = (unsigned char)runLength;
			lastToken = tokenWrite;
			++tokenWrite;
			++encodedBytes;
			*tokenWrite = value;
			++tokenWrite;
			++encodedBytes;
		} else if ((*lastToken & 0x80u) == 0) {
			*tokenWrite = (unsigned char)(runLength | 0x80);
			lastToken = tokenWrite;
			++tokenWrite;
			++encodedBytes;
			if (runLength > 0) {
				int i;

				for (i = 0; i < runLength; ++i) {
					tokenWrite[i] = src[i];
				}
			}
			tokenWrite += runLength;
			encodedBytes += runLength;
		} else {
			unsigned char literalLength = (unsigned char)(runLength + (*lastToken & 0x7f));

			if (literalLength < 0x80) {
				*lastToken = (unsigned char)(literalLength | 0x80);
				if (runLength > 0) {
					int i;

					for (i = 0; i < runLength; ++i) {
						tokenWrite[i] = src[i];
					}
				}
			} else {
				*tokenWrite = (unsigned char)(runLength | 0x80);
				lastToken = tokenWrite++;
				++encodedBytes;
				if (runLength > 0) {
					int i;

					for (i = 0; i < runLength; ++i) {
						tokenWrite[i] = src[i];
					}
				}
			}

			tokenWrite += runLength;
			encodedBytes += runLength;
		}

		if (consumed >= width) {
			break;
		}

		value = src[runLength];
		src += runLength;
	}

	*tokenWrite = 0x80;
	rowSize = encodedBytes + 5;
	*(int*)rowStart = rowSize;
	return rowSize;
}

// FUNCTION: XWA 0x55D3B0
unsigned int FrontImage_GetFadedGlyphColor16(uint16_t color16) {
	unsigned int green;
	unsigned int color;
	unsigned int blue;
	unsigned int reload;
	unsigned int ttl;
	unsigned int fade;
	unsigned int scaledGreen;
	unsigned int scaledRed;
	unsigned int result;

	color = color16;
	if (g_glyphScratchBuffer[color] != 0) {
		return g_glyphScratchBuffer[color];
	}

	blue = color;
	if (g_pixelFormat555) {
		green = color;
		green >>= 5;
		blue &= 0x1f;
		green &= 0x1f;
		color >>= 10;
	} else {
		green = color;
		green >>= 5;
		blue &= 0x1f;
		green &= 0x3f;
		color >>= 11;
	}

	reload = (unsigned int)g_glyphScratchReload;
	ttl = (unsigned int)g_glyphScratchTtl;
	fade = reload - ttl;
	result = fade * blue / reload;
	color &= 0x1f;
	scaledGreen = fade * green / reload;
	scaledRed = fade * color / reload;

	if (g_pixelFormat555) {
		scaledRed &= 0x1f;
		scaledGreen &= 0x1f;
		scaledRed <<= 5;
	} else {
		scaledRed &= 0x1f;
		scaledGreen &= 0x3f;
		scaledRed <<= 6;
	}

	scaledGreen += scaledRed;
	scaledGreen <<= 5;
	result &= 0x1f;
	scaledGreen += result;
	result = scaledGreen;
	if (result == 0) {
		result = 1;
	}

	g_glyphScratchBuffer[color16] = (uint16_t)result;
	return result;
}

// FUNCTION: XWA 0x536FF0
char FrontImage_RemapPaletteIndex(unsigned char* srcRgb, int srcIndex) {
	if ((int16_t)g_paletteRemapCache[srcIndex] < 256) {
		return (char)g_paletteRemapCache[srcIndex];
	}

	if (srcIndex == 0) {
		return 0;
	}

	{
		int value = FrontendDisplay_PackRGB(srcRgb[0], srcRgb[1], srcRgb[2]);

		g_paletteRemapCache[srcIndex] = (uint16_t)value;
		return (char)value;
	}
}

// FUNCTION: XWA 0x536F90
void FrontImage_RemapPalette(unsigned char* pixels, unsigned char* srcPalette, int width, int height) {
	int i;

	for (i = 0; i < 256; ++i) {
		g_paletteRemapCache[i] = 0x100;
	}

	if (height > 0) {
		int rows = height;

		do {
			if (width > 0) {
				int x = width;

				do {
					unsigned char srcIndex = *pixels;

					*pixels = FrontImage_RemapPaletteIndex(&srcPalette[4 * srcIndex], srcIndex);
					++pixels;
					--x;
				} while (x != 0);
			}

			--rows;
		} while (rows != 0);
	}
}

// FUNCTION: XWA 0x537F40
int FrontImage_RebuildPaletteCache(void) {
	if (g_resourceTable != NULL) {
		int resourceIndex = 0;

		if (g_resourceCount > 0) {
			int displayBpp = g_displayBpp;

			do {
				ResourceDescriptor* desc = g_resourceTable[resourceIndex].desc;

				if (desc->atlasBaseIndex == 0) {
					int frameIndex = 0;

					if (desc->frameCount > 0) {
						do {
							if (displayBpp == 16) {
								int colorIndex = 0;
								unsigned char* palette = &desc->image[frameIndex].palette[2];

								do {
									int redPart;
									int greenPart;
									int color;

									if (g_pixelFormat555) {
										redPart = 32 * (*(palette - 2) >> 3);
										greenPart = *(palette - 1) >> 3;
									} else {
										redPart = *(palette - 2) >> 3 << 6;
										greenPart = *(palette - 1) >> 2;
									}

									color = (*palette >> 3) + 32 * (greenPart + redPart);
									palette += 4;
									desc->image[frameIndex].colorLUT[colorIndex] = color;
									++colorIndex;
								} while (colorIndex < 256);

								displayBpp = g_displayBpp;
							}

							++frameIndex;
						} while (frameIndex < desc->frameCount);
					}
				}

				++resourceIndex;
			} while (resourceIndex < g_resourceCount);
		}

		g_textColorCodes[0] = 0xffff;
		g_textColorCodes[1] = FrontendDisplay_PackRGB(0x60, 0x80, 0xff);
		g_textColorCodes[2] = FrontendDisplay_PackRGB(0xff, 0, 0);
		g_textColorCodes[3] = FrontendDisplay_PackRGB(0xff, 0xff, 0);
		g_textColorCodes[4] = FrontendDisplay_PackRGB(0x32, 0x32, 0xff);
		g_textColorCodes[5] = FrontendDisplay_PackRGB(0x80, 0x80, 0xff);
		g_glyphGradientFgCached = 0x7fffffff;
	}

	return 1;
}

// FUNCTION: XWA 0x5375E0
int FrontImage_SaveBmpFile(char* fileName, const void* pixels, int width, int height, int pitch, int bpp,
						   int is555, const void* palette) {
	XwaFile* stream;
	int fileSize;
	int y;

	if (!g_bmpSaveEnabled) {
		return 0;
	}

	if (bpp == 8 && palette == NULL) {
		return 0;
	}

	stream = File_Open(AERON_VFS_ROOT_USER, fileName, "wb");
	if (stream == NULL) {
		return 0;
	}

	File_Seek(stream, 54, SEEK_SET);
	fileSize = 54;

	if (bpp == 8) {
		y = height - 1;
		if (y >= 0) {
			const unsigned char* row = (const unsigned char*)pixels + pitch * y;

			for (;;) {
				int x = 0;

				if (width > 0) {
					do {
						const unsigned char* color =
							(const unsigned char*)palette + 4 * (unsigned char)row[x];

						if (!File_WriteByte(stream, color[0]) || !File_WriteByte(stream, color[1]) ||
							!File_WriteByte(stream, color[2])) {
							File_Close(stream);
							return 0;
						}

						++x;
						fileSize += 3;
					} while (x < width);
				}

				if ((x & 1) != 0) {
					if (!File_WriteByte(stream, 0) || !File_WriteByte(stream, 0) ||
						!File_WriteByte(stream, 0)) {
						File_Close(stream);
						return 0;
					}

					fileSize += 3;
				}

				row -= pitch;
				if (--y < 0) {
					break;
				}
			}
		}
	} else if (bpp == 16) {
		y = height - 1;
		if (y >= 0) {
			const unsigned char* row = (const unsigned char*)pixels + pitch * y;

			for (;;) {
				int x = 0;

				if (width > 0) {
					do {
						uint16_t value = ByteOrder_ReadU16Le(row + 2 * x);
						int green = value >> 5;
						int red;

						if (is555) {
							red = value >> 10;
							green *= 8;
						} else {
							red = value >> 11;
							green *= 4;
						}

						if (!File_WriteByte(stream, 8 * value) || !File_WriteByte(stream, green) ||
							!File_WriteByte(stream, 8 * red)) {
							File_Close(stream);
							return 0;
						}

						fileSize += 3;
						++x;
					} while (x < width);
				}

				if ((x & 1) != 0) {
					if (!File_WriteByte(stream, 0) || !File_WriteByte(stream, 0) ||
						!File_WriteByte(stream, 0)) {
						File_Close(stream);
						return 0;
					}

					fileSize += 3;
				}

				row -= pitch;
				if (--y < 0) {
					break;
				}
			}
		}
	}

	{
		unsigned char fileHeader[14];
		unsigned char infoHeader[40];
		int headerWidth = width;

		File_Seek(stream, 0, SEEK_SET);
		memset(fileHeader, 0, sizeof(fileHeader));
		memset(infoHeader, 0, sizeof(infoHeader));

		if ((width & 1) != 0) {
			headerWidth = width + 1;
		}

		ByteOrder_WriteU16Le(fileHeader, 0x4d42);
		ByteOrder_WriteU32Le(fileHeader + 2, (uint32_t)fileSize);
		ByteOrder_WriteU32Le(fileHeader + 10, 54);
		ByteOrder_WriteU32Le(infoHeader, 40);
		ByteOrder_WriteU32Le(infoHeader + 4, (uint32_t)headerWidth);
		ByteOrder_WriteU32Le(infoHeader + 8, (uint32_t)height);
		ByteOrder_WriteU16Le(infoHeader + 12, 1);
		ByteOrder_WriteU16Le(infoHeader + 14, 24);
		ByteOrder_WriteU32Le(infoHeader + 20, (uint32_t)(fileSize - 54));

		if (!File_WriteCount(stream, fileHeader, sizeof(fileHeader)) ||
			!File_WriteCount(stream, infoHeader, sizeof(infoHeader))) {
			File_Close(stream);
			return 0;
		}
	}

	File_Close(stream);
	return 1;
}

// FUNCTION: XWA 0x536A50
int FrontImage_LoadBmpFile(char* fileName, ImageResource* image, int makePalette, int upload) {
	void* pixels = NULL;
	XwaFile* stream;
	int result = 0;
	unsigned char fileHeader[14];
	unsigned char infoHeaderBytes[40];
	int infoHeader[10];
	unsigned char palette[1024];
	int width = 0;
	int height = 0;
	int planes = 0;
	int bpp = 0;
	int compression = 0;
	int padding;
	int pixelBytes;

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	memset(palette, 0, sizeof(palette));
	if (stream != NULL) {
		File_ReadCount(stream, fileHeader, sizeof(fileHeader));
		if (ByteOrder_ReadU16Le(fileHeader) != 0x4d42) {
			goto close_stream;
		}

		File_ReadCount(stream, infoHeaderBytes, sizeof(infoHeaderBytes));
		infoHeader[0] = ByteOrder_ReadI32Le(infoHeaderBytes);
		infoHeader[1] = ByteOrder_ReadI32Le(infoHeaderBytes + 4);
		infoHeader[2] = ByteOrder_ReadI32Le(infoHeaderBytes + 8);
		planes = (int16_t)ByteOrder_ReadU16Le(infoHeaderBytes + 12);
		bpp = ByteOrder_ReadU16Le(infoHeaderBytes + 14);
		compression = ByteOrder_ReadI32Le(infoHeaderBytes + 16);
		infoHeader[3] = (bpp << 16) | (uint16_t)planes;
		infoHeader[4] = compression;
		infoHeader[5] = ByteOrder_ReadI32Le(infoHeaderBytes + 20);
		infoHeader[6] = ByteOrder_ReadI32Le(infoHeaderBytes + 24);
		infoHeader[7] = ByteOrder_ReadI32Le(infoHeaderBytes + 28);
		infoHeader[8] = ByteOrder_ReadI32Le(infoHeaderBytes + 32);
		infoHeader[9] = ByteOrder_ReadI32Le(infoHeaderBytes + 36);
		width = infoHeader[1];
		height = infoHeader[2];

		padding = width % 4;
		if (padding != 0) {
			padding = 4 - padding;
		}

		pixelBytes = height * width + padding;
		pixels = Mem_Alloc((size_t)pixelBytes);
		if (pixels == NULL) {
			goto close_stream;
		}

		memset(pixels, 0, (size_t)pixelBytes);
		if (planes != 1) {
			goto close_stream;
		}

		if (bpp == 4) {
			FrontImage_ReadBmpPalette(stream, (char*)palette, 16);
			result = FrontImage_DecodeBmp4bpp(stream, pixels, fileHeader, infoHeader);
		} else if (bpp == 8) {
			FrontImage_ReadBmpPalette(stream, (char*)palette, 256);
			result = FrontImage_DecodeBmp8bpp(stream, pixels, fileHeader, infoHeader);
		}

		if (g_displayBpp == 8) {
			if (result == 1 && makePalette == 1) {
				FrontImage_RemapPalette((unsigned char*)pixels, palette, width, height);
			}
		} else if (g_displayBpp == 16) {
			memcpy(image->palette, palette, sizeof(image->palette));
			FrontImage_BuildColorLut16(image);
		}

	close_stream:
		File_Close(stream);
	}

	if (result == 1) {
		image->height = height;
		image->width = width;
		image->pixels = pixels;
		image->pixelCount = width * height;
		image->isCompressed = 0;
		FrontImage_ComputeImageBounds(image, &image->boundsLeft);
		if (upload == 1) {
			FrontImage_CompressRLE(image);
		}
		return 1;
	}

	if (pixels != NULL) {
		Mem_Free(pixels);
	}

	return 0;
}
