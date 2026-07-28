#include "xwa/audio/imuse/imuse.h"

#ifdef XWA_MODERN
#include "aeron/log.h"
#endif

#include <stdlib.h>
#include <string.h>

static const signed char g_vimaStepAdjust4[8] = { -1, 4, -1, 4, 0, 0, 0, 0 };
static const signed char g_vimaStepAdjust5[8] = { -1, -1, 2, 6, -1, -1, 2, 6 };
static const signed char g_vimaStepAdjust6[16] = { -1, -1, -1, -1, 1, 2, 4, 6, -1, -1, -1, -1, 1, 2, 4, 6 };
static const signed char g_vimaStepAdjust7[32] = { -1, -1, -1, -1, -1, -1, -1, -1, 1, 1, 1, 2, 2, 4, 5, 6,
												   -1, -1, -1, -1, -1, -1, -1, -1, 1, 1, 1, 2, 2, 4, 5, 6 };
static const signed char g_vimaStepAdjust8[64] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1,  1,  1,  1,  1,  2,
	2,  2,  2,  4,  4,  4,  5,  5,  6,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, 1,  1,  1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  5,  5,  6,  6,
};
static const signed char g_vimaStepAdjust9[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  4,  4,
	4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6,
};

// GLOBAL: XWA 0x5ABAC8
const unsigned short g_vimaStepTable[92] = {
	7,     8,     9,     10,    11,    12,   13,    14,    16,    17,    19,    21,    23,    25,
	28,    31,    34,    37,    41,    45,   50,    55,    60,    66,    73,    80,    88,    97,
	107,   118,   130,   143,   157,   173,  190,   209,   230,   253,   279,   307,   337,   371,
	408,   449,   494,   544,   598,   658,  724,   796,   876,   963,   1060,  1166,  1282,  1411,
	1552,  1707,  1878,  2066,  2272,  2499, 2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
	5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
	22385, 24623, 27086, 29794, 32767, 0,    0,     0,
};
// GLOBAL: XWA 0x5ABB80
const unsigned char g_vimaCodeBits[96] = {
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0,
};
// GLOBAL: XWA 0x605788
const signed char* g_vimaStepAdjust[8] = {
	NULL,
	NULL,
	g_vimaStepAdjust4,
	g_vimaStepAdjust5,
	g_vimaStepAdjust6,
	g_vimaStepAdjust7,
	g_vimaStepAdjust8,
	g_vimaStepAdjust9,
};
// GLOBAL: XWA 0x786088
unsigned short g_vimaDecodeTable[89][64];
// GLOBAL: XWA 0x788D08
int g_vimaTableInit;

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x59323C
unsigned int ImReadBE16(const unsigned char* p) {
	unsigned int value;

	value = 0;
	value |= *p++;
	value <<= 8;
	value |= *p;
	return value;
}

// FUNCTION: XWA 0x5931B8
int ImReadBE32(const unsigned char* p) {
	unsigned int value;

	value = 0;
	value |= *p++;
	value <<= 8;
	value |= *p++;
	value <<= 8;
	value |= *p++;
	value <<= 8;
	value |= *p;
	return (int)value;
}

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x59327E
void ImWriteBE32(unsigned char* p, int value) {
	*p++ = (unsigned char)((value >> 24) & 0xff);
	*p++ = (unsigned char)((value >> 16) & 0xff);
	*p++ = (unsigned char)((value >> 8) & 0xff);
	*p++ = (unsigned char)(value & 0xff);
}

static __inline unsigned int ImReadLE16(const char* p) {
#ifndef XWA_MODERN
	return *(const unsigned short*)p;
#else
	const unsigned char* u;

	u = (const unsigned char*)p;
	return (unsigned int)u[0] | ((unsigned int)u[1] << 8);
#endif
}

static __inline unsigned int ImReadLE32(const char* p) {
#ifndef XWA_MODERN
	return *(const unsigned int*)p;
#else
	const unsigned char* u;

	u = (const unsigned char*)p;
	return (unsigned int)u[0] | ((unsigned int)u[1] << 8) | ((unsigned int)u[2] << 16) |
		   ((unsigned int)u[3] << 24);
#endif
}

static __inline unsigned int ImReadHeaderBE16(const char* p) {
#ifndef XWA_MODERN
	return (((int)*(const unsigned short*)p & 0xff) << 8) | ((int)*(const unsigned short*)p >> 8);
#else
	return ((unsigned int)(unsigned char)p[0] << 8) | (unsigned int)(unsigned char)p[1];
#endif
}

static __inline int ImReadHeaderBE32(const char* p) {
#ifndef XWA_MODERN
	return (int)((*(const unsigned int*)p >> 24) | ((*(const unsigned int*)p >> 8) & 0xff00u) |
				 ((*(const unsigned int*)p & 0xff00u) << 8) | (*(const unsigned int*)p << 24));
#else
	return (int)(((unsigned int)(unsigned char)p[0] << 24) | ((unsigned int)(unsigned char)p[1] << 16) |
				 ((unsigned int)(unsigned char)p[2] << 8) | (unsigned int)(unsigned char)p[3]);
#endif
}

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define IM_SOUND_PACKED
#else
#define IM_SOUND_PACKED __attribute__((packed))
#endif

typedef struct IM_SOUND_PACKED ImIndySoundHeader {
	char tag[6];
	int sampleRate;
	int bitsPerSample;
	int channels;
	int dataSize;
	int syncSize;
} ImIndySoundHeader;

typedef struct IM_SOUND_PACKED ImWaveFormatChunk {
	int size;
	short formatTag;
	short channels;
	int sampleRate;
	int averageBytesPerSecond;
	short blockAlign;
	short bitsPerSample;
} ImWaveFormatChunk;

typedef struct IM_SOUND_PACKED ImLittleEndianChunk {
	char tag[4];
	int size;
} ImLittleEndianChunk;

typedef struct IM_SOUND_PACKED ImMcmpHeader {
	char tag[4];
	unsigned short blockCount;
} ImMcmpHeader;

typedef struct IM_SOUND_PACKED ImBigEndianValue {
	unsigned int value;
} ImBigEndianValue;

typedef struct IM_SOUND_PACKED ImBigEndianChunk {
	char tag[4];
	unsigned int size;
} ImBigEndianChunk;

typedef struct IM_SOUND_PACKED ImImusHeader {
	char tag[4];
	unsigned int fileSize;
	char mapTag[4];
	unsigned int formatLength;
	char formatTag[4];
} ImImusHeader;

typedef struct IM_SOUND_PACKED ImImusFormat {
	unsigned int bitsPerSample;
	unsigned int sampleRate;
	unsigned int channels;
} ImImusFormat;

typedef struct IM_SOUND_PACKED ImAiffCommonChunk {
	unsigned int size;
	unsigned char channelsHigh;
	unsigned char channels;
	unsigned int sampleFrames;
	unsigned char bitsPerSampleHigh;
	unsigned char bitsPerSample;
	unsigned char sampleRateExponentHigh;
	unsigned char sampleRateExponent;
} ImAiffCommonChunk;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef IM_SOUND_PACKED

static void ImWriteLE32(char* p, unsigned int value) {
	p[0] = (char)(value & 0xff);
	p[1] = (char)((value >> 8) & 0xff);
	p[2] = (char)((value >> 16) & 0xff);
	p[3] = (char)((value >> 24) & 0xff);
}

static void ImWriteLE16(char* p, unsigned int value) {
	p[0] = (char)(value & 0xff);
	p[1] = (char)((value >> 8) & 0xff);
}

#ifndef XWA_MODERN
#pragma function(memcmp)
#endif
// FUNCTION: XWA 0x587700
char* ImParseSoundHeader(char* src, int* pFormat, int* pSampleRate, int* pBitsPerSample, int* pChannels,
						 int* pField6, int* pDataSize, void** pSyncData, int* pSyncSize) {
	int insideMcmp;

	insideMcmp = 0;
	if (pFormat) {
		*pFormat = 0;
	}
	if (!src) {
		return NULL;
	}
	if (pSyncData) {
		*pSyncData = NULL;
	}
	if (pSyncSize) {
		*pSyncSize = 0;
	}

	if (memcmp(src, "MCMP", 4u) == 0) {
		int mcmpSize;

#ifndef XWA_MODERN
		mcmpSize = (((int)((const ImMcmpHeader*)src)->blockCount & 0xff) << 8) |
				   ((int)((const ImMcmpHeader*)src)->blockCount >> 8);
#else
		mcmpSize = (int)ImReadHeaderBE16(src + 4);
#endif
		src += 9 * mcmpSize + 6;
#ifndef XWA_MODERN
		mcmpSize = (((int)((const unsigned short*)src)[0] & 0xff) << 8) |
				   ((int)((const unsigned short*)src)[0] >> 8);
#else
		mcmpSize = (int)ImReadHeaderBE16(src);
#endif
		src += mcmpSize + 2;
		insideMcmp = 1;
	}

	if (memcmp(src, "INDYWV", 6u) == 0) {
		int syncSize;

		syncSize = 0;
		if (pFormat) {
			*pFormat = 6;
		}
		if (pSampleRate) {
#ifndef XWA_MODERN
			*pSampleRate = ((const ImIndySoundHeader*)src)->sampleRate;
#else
			*pSampleRate = (int)ImReadLE32(src + 6);
#endif
		}
		if (pBitsPerSample) {
#ifndef XWA_MODERN
			*pBitsPerSample = ((const ImIndySoundHeader*)src)->bitsPerSample;
#else
			*pBitsPerSample = (int)ImReadLE32(src + 10);
#endif
		}
		if (pChannels) {
#ifndef XWA_MODERN
			*pChannels = ((const ImIndySoundHeader*)src)->channels;
#else
			*pChannels = (int)ImReadLE32(src + 14);
#endif
		}
		if (pField6) {
#ifndef XWA_MODERN
			*pField6 = ((const ImIndySoundHeader*)src)->dataSize;
#else
			*pField6 = (int)ImReadLE32(src + 18);
#endif
		}
		if (pDataSize) {
#ifndef XWA_MODERN
			*pDataSize = ((const ImIndySoundHeader*)src)->dataSize;
#else
			*pDataSize = (int)ImReadLE32(src + 18);
#endif
		}
		if (pSyncData && pSyncSize) {
#ifndef XWA_MODERN
			syncSize = ((const ImIndySoundHeader*)src)->syncSize;
#else
			syncSize = (int)ImReadLE32(src + 22);
#endif
			*pSyncSize = syncSize;
			if (*pSyncSize) {
				*pSyncData = src + 26;
			} else {
				*pSyncData = NULL;
			}
		}
		return src + syncSize + 26;
	} else if (memcmp(src, "RIFF", 4u) == 0) {
		char* riffEnd;

#ifndef XWA_MODERN
		riffEnd = src + ((const ImLittleEndianChunk*)src)->size + 8;
#else
		riffEnd = src + ImReadLE32(src + 4) + 8;
#endif
		if (memcmp(src + 8, "WAVEfmt ", 8u) != 0) {
			return NULL;
		}
#ifndef XWA_MODERN
		if (((const ImWaveFormatChunk*)(src + 16))->formatTag != 1) {
#else
		if (ImReadLE16(src + 20) != 1) {
#endif
			return NULL;
		}
		{
			int chunkSize;

			if (pFormat) {
				if (insideMcmp) {
					*pFormat = 4;
				} else {
					*pFormat = 3;
				}
			}
			src += 16;
#ifndef XWA_MODERN
			chunkSize = ((const ImWaveFormatChunk*)src)->size;
#else
			chunkSize = (int)ImReadLE32(src);
#endif
			if (pSampleRate) {
#ifndef XWA_MODERN
				*pSampleRate = ((const ImWaveFormatChunk*)src)->sampleRate;
#else
				*pSampleRate = (int)ImReadLE32(src + 8);
#endif
			}
			if (pBitsPerSample) {
#ifndef XWA_MODERN
				*pBitsPerSample = ((const ImWaveFormatChunk*)src)->bitsPerSample;
#else
				*pBitsPerSample = (short)ImReadLE16(src + 18);
#endif
			}
			if (pChannels) {
#ifndef XWA_MODERN
				*pChannels = ((const ImWaveFormatChunk*)src)->channels;
#else
				*pChannels = (short)ImReadLE16(src + 6);
#endif
			}
			if (pField6) {
				*pField6 = 1;
			}

			do {
				src += chunkSize + 8;
#ifndef XWA_MODERN
				chunkSize = ((const ImLittleEndianChunk*)(src - 4))->size;
#else
				chunkSize = (int)ImReadLE32(src);
#endif
				if (memcmp(src - 4, "sync", 4u) == 0 && pSyncData && pSyncSize) {
					*pSyncData = src + 4;
					*pSyncSize = chunkSize;
				}
			} while (src < riffEnd && memcmp(src - 4, "data", 4u) != 0);

			if (src < riffEnd) {
				if (pDataSize) {
					*pDataSize = chunkSize;
				}
				return src + 4;
			}
			return NULL;
		}
	} else if (memcmp(src, "iMUS", 4u) == 0) {
		int formatLength;

#ifndef XWA_MODERN
		formatLength = (int)((((const ImImusHeader*)src)->formatLength >> 24) |
							 ((((const ImImusHeader*)src)->formatLength >> 8) & 0xff00u) |
							 ((((const ImImusHeader*)src)->formatLength & 0xff00u) << 8) |
							 (((const ImImusHeader*)src)->formatLength << 24));
#else
		formatLength = ImReadHeaderBE32(src + 12);
#endif
		if (memcmp(src + 16, "FRMT", 4u) != 0) {
			return NULL;
		}

		if (pFormat) {
			if (insideMcmp) {
				*pFormat = 2;
			} else {
				*pFormat = 1;
			}
		}
		src += 32;
		if (pBitsPerSample) {
#ifndef XWA_MODERN
			*pBitsPerSample = (int)((((const ImImusFormat*)src)->bitsPerSample >> 24) |
									((((const ImImusFormat*)src)->bitsPerSample >> 8) & 0xff00u) |
									((((const ImImusFormat*)src)->bitsPerSample & 0xff00u) << 8) |
									(((const ImImusFormat*)src)->bitsPerSample << 24));
#else
			*pBitsPerSample = ImReadHeaderBE32(src);
#endif
		}
		if (pSampleRate) {
#ifndef XWA_MODERN
			*pSampleRate = (int)((((const ImImusFormat*)src)->sampleRate >> 24) |
								 ((((const ImImusFormat*)src)->sampleRate >> 8) & 0xff00u) |
								 ((((const ImImusFormat*)src)->sampleRate & 0xff00u) << 8) |
								 (((const ImImusFormat*)src)->sampleRate << 24));
#else
			*pSampleRate = ImReadHeaderBE32(src + 4);
#endif
		}
		if (pChannels) {
#ifndef XWA_MODERN
			*pChannels = (int)((((const ImImusFormat*)src)->channels >> 24) |
							   ((((const ImImusFormat*)src)->channels >> 8) & 0xff00u) |
							   ((((const ImImusFormat*)src)->channels & 0xff00u) << 8) |
							   (((const ImImusFormat*)src)->channels << 24));
#else
			*pChannels = ImReadHeaderBE32(src + 8);
#endif
		}
		if (pField6) {
			*pField6 = 1;
		}
		if (pDataSize) {
#ifndef XWA_MODERN
			*pDataSize =
				(int)(((const ImBigEndianValue*)(src + formatLength - 12))->value >> 24 |
					  ((((const ImBigEndianValue*)(src + formatLength - 12))->value >> 8) & 0xff00u) |
					  ((((const ImBigEndianValue*)(src + formatLength - 12))->value & 0xff00u) << 8) |
					  (((const ImBigEndianValue*)(src + formatLength - 12))->value << 24));
#else
			*pDataSize = ImReadHeaderBE32(src + formatLength - 12);
#endif
		}
		return src + formatLength - 8;
	} else if (insideMcmp) {
		return NULL;
	} else if (memcmp(src, "FORM", 4u) == 0) {
		char* formEnd;

#ifndef XWA_MODERN
		formEnd = src +
				  (int)((((const ImBigEndianChunk*)src)->size >> 24) |
						((((const ImBigEndianChunk*)src)->size >> 8) & 0xff00u) |
						((((const ImBigEndianChunk*)src)->size & 0xff00u) << 8) |
						(((const ImBigEndianChunk*)src)->size << 24)) +
				  8;
#else
		formEnd = src + ImReadHeaderBE32(src + 4) + 8;
#endif
		if (memcmp(src + 8, "AIFFCOMM", 8u) != 0) {
			return NULL;
		}
		{
			int aiffChunkSize;

			if (pFormat) {
				*pFormat = 5;
			}
			src += 16;
#ifndef XWA_MODERN
			aiffChunkSize = (int)((((const ImAiffCommonChunk*)src)->size >> 24) |
								  ((((const ImAiffCommonChunk*)src)->size >> 8) & 0xff00u) |
								  ((((const ImAiffCommonChunk*)src)->size & 0xff00u) << 8) |
								  (((const ImAiffCommonChunk*)src)->size << 24));
#else
			aiffChunkSize = ImReadHeaderBE32(src);
#endif
			if (pSampleRate) {
				int sampleRate;

				if (((const ImAiffCommonChunk*)src)->sampleRateExponent == 14) {
					sampleRate = 44100;
				} else {
					sampleRate = ((const ImAiffCommonChunk*)src)->sampleRateExponent != 12 ? 22050 : 11025;
				}
				*pSampleRate = sampleRate;
			}
			if (pBitsPerSample) {
				*pBitsPerSample = ((const ImAiffCommonChunk*)src)->bitsPerSample;
			}
			if (pChannels) {
				*pChannels = ((const ImAiffCommonChunk*)src)->channels;
			}
			if (pField6) {
				*pField6 = 0;
			}

			do {
				src += aiffChunkSize + 8;
#ifndef XWA_MODERN
				aiffChunkSize = (int)((((const ImBigEndianValue*)src)->value >> 24) |
									  ((((const ImBigEndianValue*)src)->value >> 8) & 0xff00u) |
									  ((((const ImBigEndianValue*)src)->value & 0xff00u) << 8) |
									  (((const ImBigEndianValue*)src)->value << 24));
#else
				aiffChunkSize = ImReadHeaderBE32(src);
#endif
			} while (src < formEnd && memcmp(src - 4, "SSND", 4u) != 0);

			if (src < formEnd) {
				if (pDataSize) {
					*pDataSize = aiffChunkSize - 8;
				}
				return src + 12;
			}
			return NULL;
		}
	}

	return NULL;
}
#ifndef XWA_MODERN
#pragma intrinsic(memcmp)
#endif

// FUNCTION: XWA 0x588EBA
unsigned int ImCalcConvertedSize(int outFormat, unsigned int rate, unsigned int bits, unsigned int channels,
								 char* src) {
	int bitsPerSample;
	int halfOutput;
	{
		int compressedInput;
		int headerSize;
		{
			int format;
			unsigned int dataSize;
			{
				int sourceChannels;
				{
					int sampleRate;

					compressedInput = 0;
					halfOutput = 0;
#ifdef XWA_MODERN
					format = 0;
					sampleRate = 0;
					bitsPerSample = 0;
					sourceChannels = 0;
					dataSize = 0;
#endif

					ImParseSoundHeader(src, &format, &sampleRate, &bitsPerSample, &sourceChannels, NULL,
									   (int*)&dataSize, NULL, NULL);
					if (!sourceChannels) {
						sourceChannels = 1;
					}
					if ((unsigned int)sampleRate < 11025u) {
						sampleRate = 11025;
					}
					if ((unsigned int)bitsPerSample < 8u) {
						bitsPerSample = 8;
					}

					switch (format) {
						default:
							return 0;
						case 1:
						case 3:
						case 5:
							break;
						case 2:
						case 4:
							compressedInput = 1;
							break;
					}

					if (!rate) {
						rate = (unsigned int)sampleRate;
					}
					if (!bits) {
						bits = (unsigned int)bitsPerSample;
					}
					if (!channels) {
						channels = (unsigned int)sourceChannels;
					}

					switch (outFormat) {
						default:
							return 0;
						case 1:
							headerSize = 80;
							break;
						case 3:
							headerSize = 44;
							break;
						case 5:
							headerSize = 54;
							break;
						case 2:
							headerSize = 80;
							halfOutput = 1;
							break;
						case 4:
							headerSize = 44;
							halfOutput = 1;
							break;
					}

					if ((unsigned int)sampleRate != rate) {
						dataSize *= rate >> 10;
						dataSize /= (unsigned int)sampleRate >> 10;
					}
					if ((unsigned int)bitsPerSample != bits) {
						dataSize *= bits >> 3;
						dataSize /= (unsigned int)bitsPerSample >> 3;
					}
					if ((unsigned int)sourceChannels != channels) {
						dataSize *= channels;
						dataSize /= (unsigned int)sourceChannels;
					}
					if (halfOutput) {
						dataSize >>= 1;
					}
					return (unsigned int)headerSize + dataSize;
				}
			}
		}
	}
}

// FUNCTION: XWA 0x58896C
int ImEncodeSoundToMcmp(char* dst, char* src) {
	unsigned int headerBytes;
	unsigned int encodedSize;
	char* out;
	char* encodedDst;
	int format;
	int bitsPerSample;
	int channels;
	int field6;
	int dataSize;
	int16_t* samples;
	ImVimaState state;

	out = dst;
	samples = (int16_t*)ImParseSoundHeader(src, &format, NULL, &bitsPerSample, &channels, &field6, &dataSize,
										   NULL, NULL);
	if (!samples || bitsPerSample != 16) {
		return 0;
	}

	headerBytes = (unsigned int)((char*)samples - src);
	memcpy(out, "MCMP", 4u);
	out += 4;
	out[0] = 0;
	out[1] = 2;
	out += 2;

	*out++ = 0;
	ImWriteBE32((unsigned char*)out, (int)headerBytes);
	out += 4;
	ImWriteBE32((unsigned char*)out, (int)headerBytes);
	out += 4;

	*out++ = 1;
	ImWriteBE32((unsigned char*)out, dataSize);
	out[8] = 0;
	out[9] = 10;
	out += 10;

	memcpy(out, "NONE", 5u);
	out += 5;
	memcpy(out, "VIMA", 5u);
	out += 5;

	memcpy(out, src, (size_t)headerBytes);
	encodedDst = out + headerBytes;
	ImVimaResetState(&state);
	encodedSize = ImVimaEncode(&state, (unsigned char*)encodedDst, samples, (unsigned int)dataSize,
							   (unsigned int)channels);
	ImWriteBE32((unsigned char*)dst + 20, (int)encodedSize);
	return (int)(encodedDst + encodedSize - dst);
}

#ifndef XWA_MODERN
#pragma function(abs)
#pragma function(strcpy)
#endif
// FUNCTION: XWA 0x5891E5
int ImBuildSyncChunk(char* dst, char* pcmData, unsigned int sampleRate, unsigned char codeBaseA,
					 unsigned char codeBaseB, int windowDivisor, int bits, int channels, int alignFlag,
					 unsigned int pcmSize) {
	unsigned int historyIndex;
	int syncTime;
	int totalAbs;
	int totalCrossings;
	int maxPeak;
	unsigned int expectedCrossings;
	char* out;
	unsigned int i;
	unsigned int j;
	unsigned int windowSize;
	unsigned int offset;
	int maskA;
	int maskB;
	int lastCodeA;
	int lastCodeB;
	unsigned int timeStep;
	int history[30];

	out = dst;
	historyIndex = 0;
	syncTime = 0;
	totalAbs = 0;
	totalCrossings = 0;
	maxPeak = 0;
	expectedCrossings = 0;
	lastCodeA = -1;
	lastCodeB = -1;
	maskA = 0;
	maskB = 0;

	if (!pcmSize) {
#ifndef XWA_MODERN
		printf("File is corrupt %c\n", 7, 7, 7, 7, 7, 7, 7, 7, 7, 7);
#else
		Aeron_LogError("xwa.audio", "IMC file is corrupt");
#endif
		return 0;
	}
	if (!pcmData || bits != 16 || channels != 1) {
		return 0;
	}
	if (alignFlag) {
		++pcmData;
	}

	strcpy(dst, "SYNC");
	out = dst + 8;

	--codeBaseA;
	--codeBaseB;
	for (i = 0; i < 7; ++i) {
		maskA *= 2;
		maskB *= 2;
		if (codeBaseA) {
			maskA |= 1;
		}
		if (codeBaseB) {
			maskB |= 1;
		}
		codeBaseA >>= 1;
		codeBaseB >>= 1;
	}

	timeStep = 0x3e8000u / sampleRate;
	for (j = 2; j < pcmSize; j += 2) {
		int16_t prevSample;
		int16_t sample;

		prevSample = (int16_t)((unsigned int)(unsigned char)pcmData[j - 2] << 8);
		sample = (int16_t)((unsigned int)(unsigned char)pcmData[j] << 8);
		if (sample >= 0 && prevSample < 0) {
			++totalCrossings;
		}
		sample = (int16_t)abs(sample);
		if ((unsigned int)sample > (unsigned int)maxPeak) {
			maxPeak = sample;
		}
		totalAbs += sample;
	}
	totalAbs = (int)((unsigned int)totalAbs / (pcmSize >> 1));

	for (i = 0; i < 30; ++i) {
		history[i] = maxPeak;
	}

	windowSize = (unsigned int)(2 * windowDivisor) / sampleRate;
	expectedCrossings = (unsigned int)totalCrossings * windowSize / pcmSize;
	for (offset = 0; offset + windowSize <= pcmSize; offset += windowSize, pcmData += windowSize) {
		unsigned int windowPeak;
		int windowCrossings;
		int historyAverage;
		unsigned int hist;
		int ampScore;
		int crossScore;
		int clippedCrossScore;
		unsigned int clippedAmpScore;
		int codeHi;
		int codeLo;

		windowPeak = 0;
		windowCrossings = 0;
		totalAbs = 0;

		for (i = 2; i < windowSize; i += 2) {
			int16_t prevSample;
			int16_t sample;

			prevSample = (int16_t)((unsigned int)(unsigned char)pcmData[i - 2] << 8);
			sample = (int16_t)((unsigned int)(unsigned char)pcmData[i] << 8);
			if (sample >= 0 && prevSample < 0) {
				++windowCrossings;
			}
			sample = (int16_t)abs(sample);
			if ((unsigned int)sample > windowPeak) {
				windowPeak = (unsigned int)sample;
			}
			totalAbs += sample;
		}

		totalAbs = 0;
		for (hist = historyIndex; hist < historyIndex + 30; ++hist) {
			totalAbs += history[hist % 30];
		}
		historyAverage = (int)((unsigned int)totalAbs / 30u);
		if (!historyAverage) {
			++historyAverage;
		}
		if ((unsigned int)historyAverage < ((unsigned int)maxPeak >> 2)) {
			historyAverage = (int)((unsigned int)maxPeak >> 2);
		}

		history[historyIndex] = (int)windowPeak;
		historyIndex = (historyIndex + 1) % 30u;

		ampScore = (int)(60u * windowPeak / (unsigned int)historyAverage);
		crossScore = (int)(50u * (unsigned int)windowCrossings / (expectedCrossings + 1u));
		if (ampScore < 25) {
			crossScore = (ampScore * crossScore + (25 - ampScore) * 37) / 25;
		}

		if (crossScore < 100) {
			clippedCrossScore = crossScore;
		} else {
			clippedCrossScore = 100;
		}
		crossScore = clippedCrossScore;
		codeHi = 127 * crossScore / 100;

		if (ampScore < 100) {
			clippedAmpScore = (unsigned int)ampScore;
		} else {
			clippedAmpScore = 100;
		}
		ampScore = (int)clippedAmpScore;
		codeLo = 127 * ampScore / 100;
		codeHi &= maskA;
		codeLo &= maskB;

		if (codeHi != lastCodeA || codeLo != lastCodeB) {
			unsigned int syncValue;

			syncValue = ((unsigned int)syncTime & 0xffff0000u) | (((unsigned int)codeHi & 0x7fu) << 8) |
						((unsigned int)codeLo & 0x7fu);
			memcpy(out, &syncValue, 4u);
			out += 4;
			lastCodeA = codeHi;
			lastCodeB = codeLo;
		}

		syncTime += (int)timeStep;
	}

	{
		unsigned int syncValue;
		int entryCount;

		syncValue = (unsigned int)syncTime & 0xffff0000u;
		memcpy(out, &syncValue, 4u);
		out += 4;
		entryCount = (int)((out - dst) >> 2) - 2;
		out = dst + 4;
		memcpy(out, &entryCount, 4u);
		return 4 * entryCount + 8;
	}
}

// GLOBAL: XWA 0x5ABBE0
static const unsigned char g_imMapTemplate0[10] = { 0x40, 0x0c, 0xac, 0x44, 0, 0, 0, 0, 0, 0 };
// GLOBAL: XWA 0x5ABBF0
static const unsigned char g_imMapTemplate1[10] = { 0x40, 0x0d, 0xac, 0x44, 0, 0, 0, 0, 0, 0 };
// GLOBAL: XWA 0x5ABC00
static const unsigned char g_imMapTemplate2[10] = { 0x40, 0x0e, 0xac, 0x44, 0, 0, 0, 0, 0, 0 };

// FUNCTION: XWA 0x587D9D
int ImBuildMap(void* dst, int formatType, unsigned int sampleRate, unsigned int bitsPerSample,
			   unsigned int channels, unsigned int dataSize, void* syncChunk) {
	char* out;
	unsigned int syncSize;
	unsigned int frameCount;

	out = (char*)dst;
	switch (formatType) {
		case 1:
			memcpy(out, "iMUS", 4u);
			ImWriteBE32((unsigned char*)out + 4, (int)(dataSize + 80));
			memcpy(out + 8, "MAP ", 4u);
			ImWriteBE32((unsigned char*)out + 12, 56);
			memcpy(out + 16, "FRMT", 4u);
			ImWriteBE32((unsigned char*)out + 20, 20);
			ImWriteBE32((unsigned char*)out + 24, 80);
			ImWriteBE32((unsigned char*)out + 28, 0);
			ImWriteBE32((unsigned char*)out + 32, (int)bitsPerSample);
			ImWriteBE32((unsigned char*)out + 36, (int)sampleRate);
			ImWriteBE32((unsigned char*)out + 40, (int)channels);
			memcpy(out + 44, "REGN", 4u);
			ImWriteBE32((unsigned char*)out + 48, 8);
			ImWriteBE32((unsigned char*)out + 52, 80);
			ImWriteBE32((unsigned char*)out + 56, (int)dataSize);
			memcpy(out + 60, "STOP", 4u);
			ImWriteBE32((unsigned char*)out + 64, 4);
			ImWriteBE32((unsigned char*)out + 68, (int)(dataSize + 80));
			memcpy(out + 72, "DATA", 4u);
			ImWriteBE32((unsigned char*)out + 76, (int)dataSize);
			out += 80;
			break;

		case 3:
			syncSize = 0;
			if (syncChunk) {
				syncSize = 4u * ((const unsigned int*)syncChunk)[1] + 16u;
			}

			memcpy(out, "RIFF", 4u);
			ImWriteLE32(out + 4, dataSize + syncSize + 36u);
			memcpy(out + 8, "WAVEfmt ", 8u);
			ImWriteLE32(out + 16, 16u);
			ImWriteLE16(out + 20, 1u);
			ImWriteLE16(out + 22, channels);
			ImWriteLE32(out + 24, sampleRate);
			ImWriteLE32(out + 28, channels * (bitsPerSample >> 3) * sampleRate);
			ImWriteLE16(out + 32, channels * (bitsPerSample >> 3));
			ImWriteLE16(out + 34, channels);
			out += 36;

			if (syncSize) {
				memcpy(out, "sync", 4u);
				ImWriteLE32(out + 4, syncSize - 8u);
				memcpy(out + 8, syncChunk, (size_t)(syncSize - 8u));
				out = (char*)dst + syncSize + 36u;
			}

			memcpy(out, "data", 4u);
			out += 4;
			ImWriteLE32(out, dataSize);
			out += 4;
			break;

		case 5:
			memcpy(out, "FORM", 4u);
			ImWriteBE32((unsigned char*)out + 4, (int)(dataSize + 54));
			memcpy(out + 8, "AIFFCOMM", 8u);
			ImWriteBE32((unsigned char*)out + 16, 18);
			out[20] = (char)((channels >> 8) & 0xff);
			out[21] = (char)(channels & 0xff);
			frameCount = dataSize / (channels * (bitsPerSample >> 3));
			ImWriteBE32((unsigned char*)out + 22, (int)frameCount);
			out[26] = (char)((bitsPerSample >> 8) & 0xff);
			out[27] = (char)(bitsPerSample & 0xff);
			if (sampleRate == 11025) {
				memcpy(out + 28, g_imMapTemplate0, 10u);
			} else if (sampleRate == 44100) {
				memcpy(out + 28, g_imMapTemplate2, 10u);
			} else {
				memcpy(out + 28, g_imMapTemplate1, 10u);
			}
			memcpy(out + 38, "SSND", 4u);
			ImWriteBE32((unsigned char*)out + 42, (int)(dataSize + 8));
			ImWriteBE32((unsigned char*)out + 46, 0);
			ImWriteBE32((unsigned char*)out + 50, 0);
			out += 54;
			break;

		default:
			break;
	}

	return (int)(out - (char*)dst);
}

// FUNCTION: XWA 0x588CFF
void ImVimaResetState(ImVimaState* state) {
	state->stepIndex[1] = 0;
	state->stepIndex[0] = 0;
	state->predictor[1] = 0;
	state->predictor[0] = 0;
}

static int ImVimaEncodeSample(const int16_t* sample, int nativeEndian) {
	uint16_t raw;

	raw = (uint16_t)*sample;
	if (nativeEndian) {
		return (int)(int16_t)raw;
	}
	return (int)((raw >> 8) | ((raw & 0xff) << 8));
}

// FUNCTION: XWA 0x589886
unsigned int ImVimaEncodeBlock(ImVimaState* state, unsigned char* dst, int16_t* samples,
							   unsigned int sampleCount, unsigned int channelCount, int nativeEndian,
							   int continueState) {
	int bitCount;
	unsigned char* out;
	uint16_t bitBuffer;
	unsigned int channel;
	unsigned int i;
	const int16_t* samplePtr;
	int stepIndex;
	int predictor;
	int diff;
	unsigned char code;
	unsigned char sign;
	unsigned char codeMask;
	int step;
	unsigned char stepBits;
	int bitsLeft;
	int mask;
	int delta;
	int sample;
	int adjust;
	unsigned char freeBits;

	bitCount = 0;
	out = dst;
	bitBuffer = 0;

	if (!continueState) {
		state->stepIndex[1] = 0;
		state->stepIndex[0] = 0;
		state->predictor[1] = 0;
		state->predictor[0] = 0;
	}

	for (channel = 0; channel < channelCount; ++channel) {
		samplePtr = samples + channel;
		stepIndex = state->stepIndex[channel];
		predictor = state->predictor[channel];

		for (i = 0; i < sampleCount; ++i) {
			diff = 0;
			code = 0;
			sign = 0;
			step = g_vimaStepTable[stepIndex];
			sample = ImVimaEncodeSample(samplePtr, nativeEndian);
			delta = sample - predictor;
			stepBits = g_vimaCodeBits[stepIndex];
			bitsLeft = stepBits - 1;
			mask = 1 << bitsLeft;
			codeMask = (unsigned char)(mask - 1);

			if (delta < 0) {
				sign = (unsigned char)mask;
				delta = -delta;
			}

			for (;;) {
				mask >>= 1;
				if (!bitsLeft--) {
					break;
				}
				if (delta >= step) {
					code = (unsigned char)(code | mask);
					delta -= step;
					diff += step;
				}
				step >>= 1;
			}

			if (code) {
				diff += step;
			}

			freeBits = (unsigned char)(8 - (bitCount & 7));
			bitBuffer = (uint16_t)((bitBuffer << stepBits) | sign | code);
			bitCount += stepBits;
			if (stepBits >= freeBits) {
				*out++ = (unsigned char)(bitBuffer >> (stepBits - freeBits));
			}

			if ((unsigned char)code == codeMask) {
				predictor = sample;
				bitBuffer = (uint16_t)((bitBuffer << 8) | (((unsigned int)predictor >> 8) & 0xff));
				*out++ = (unsigned char)(bitBuffer >> (bitCount & 7));
				bitBuffer = (uint16_t)((bitBuffer << 8) | (predictor & 0xff));
				*out++ = (unsigned char)(bitBuffer >> (bitCount & 7));
			} else {
				adjust = sign ? -diff : diff;
				predictor += adjust;
				if (predictor < -32768) {
					predictor = -32768;
				} else if (predictor > 32767) {
					predictor = 32767;
				}
			}

			stepIndex += g_vimaStepAdjust[stepBits][code];
			if (stepIndex < 0) {
				stepIndex = 0;
			}
			if (stepIndex > 88) {
				stepIndex = 88;
			}
			samplePtr += channelCount;
		}

		state->stepIndex[channel] = (char)stepIndex;
		state->predictor[channel] = (int16_t)predictor;
	}

	if ((bitCount & 7) != 0) {
		bitBuffer = (uint16_t)(bitBuffer << (8 - (bitCount & 7)));
		*out++ = (unsigned char)(bitBuffer & 0xff);
	}
	return (unsigned int)(out - dst);
}

static int ImVimaAbsBitCount(int sample) {
	int bits;

	if (sample < 0) {
		sample = -sample;
	}
	bits = 1;
	while (sample) {
		sample >>= 1;
		++bits;
	}
	return bits;
}

static int ImVimaClampStatsSample(int sample) {
	if (sample < -32767) {
		return -32767;
	}
	if (sample > 32767) {
		return 32767;
	}
	return sample;
}

static unsigned char* ImVimaWriteStatsSample(unsigned char* out, int quantized) {
	if (quantized <= 127 && quantized >= -127) {
		*out++ = (unsigned char)quantized;
	} else {
		*out++ = 0x80;
		*out++ = (unsigned char)(quantized >> 8);
		*out++ = (unsigned char)quantized;
	}
	return out;
}

// FUNCTION: XWA 0x589F93
int ImVimaEncodeStats(unsigned char* dst, int16_t* samples, int byteCount, FILE* statsFile) {
	int leftHist[17];
	int rightHist[17];
	int sampleWords;
	int i;
	int leftShift;
	int rightShift;
	int budget;
	int leftSaved;
	int rightSaved;
	int useLeft;
	int leftRound;
	int rightRound;
	unsigned char* out;
	int16_t* src;
	int sample;
	int adjusted;
	int quantized;
	int decoded;
	int right;
	int totalBytes;

	out = dst;
	sampleWords = byteCount >> 1;
	for (i = 0; i < 17; ++i) {
		rightHist[i] = 0;
		leftHist[i] = 0;
	}

	src = samples;
	for (i = 0; i < sampleWords; i += 2) {
		++leftHist[ImVimaAbsBitCount(*src++)];
		++rightHist[ImVimaAbsBitCount(*src++)];
	}

	rightShift = 8;
	for (leftShift = 8; leftShift > 0 && !leftHist[leftShift + 8]; --leftShift) {
	}
	while (rightShift > 0 && !rightHist[rightShift + 8]) {
		--rightShift;
	}

	budget = 192;
	rightSaved = 0;
	leftSaved = 0;
	while (budget) {
		if (leftShift > 0 && rightShift > 0) {
			useLeft = leftHist[leftShift + 8] < rightHist[rightShift + 8];
		} else if (leftShift > 0) {
			useLeft = 1;
		} else if (rightShift > 0) {
			useLeft = 0;
		} else {
			break;
		}

		if (useLeft) {
			if (leftHist[leftShift + 8] > budget) {
				break;
			}
			budget -= leftHist[leftShift + 8];
			--leftShift;
			++leftSaved;
		} else {
			if (rightHist[rightShift + 8] > budget) {
				break;
			}
			budget -= rightHist[rightShift + 8];
			--rightShift;
			++rightSaved;
		}
	}

	leftRound = leftShift ? (1 << (leftShift - 1)) : 0;
	rightRound = rightShift ? (1 << (rightShift - 1)) : 0;

	if (statsFile) {
		fprintf(statsFile, "L = %4d:", leftShift);
		for (i = 0; i < 16; ++i) {
			fprintf(statsFile, "%4d,", leftHist[i]);
		}
		fprintf(statsFile, "%4d\n", leftHist[16]);
		fprintf(statsFile, "R = %4d:", rightShift);
		for (i = 0; i < 16; ++i) {
			fprintf(statsFile, "%4d,", rightHist[i]);
		}
		fprintf(statsFile, "%4d\n", rightHist[16]);
		fprintf(statsFile, "%d escape slots used, %dL/%dR bits saved\n\n", 192 - budget, leftSaved,
				rightSaved);
	}

	*out = 0;
	*out++ = 0;
	*++out = (unsigned char)(rightShift + 16 * leftShift);
	++out;

	src = samples;
	for (i = 0; i < sampleWords; i += 2) {
		sample = *src++;
		adjusted = sample < 0 ? sample - leftRound : sample + leftRound;
		adjusted = ImVimaClampStatsSample(adjusted);
		quantized = adjusted >> leftShift;
		decoded = quantized << leftShift;
		out = ImVimaWriteStatsSample(out, quantized);
		(void)(sample - decoded);

		right = *src++;
		adjusted = right < 0 ? right - rightRound : right + rightRound;
		adjusted = ImVimaClampStatsSample(adjusted);
		quantized = adjusted >> rightShift;
		decoded = quantized << rightShift;
		out = ImVimaWriteStatsSample(out, quantized);
		(void)(right - decoded);
	}

	totalBytes = (int)(out - dst);
	dst[0] = (unsigned char)((totalBytes - 2) >> 8);
	dst[1] = (unsigned char)(totalBytes - 2);
	return totalBytes;
}

static void ImWriteBE16(unsigned char* p, int value) {
	p[0] = (unsigned char)((value >> 8) & 0xff);
	p[1] = (unsigned char)(value & 0xff);
}

// FUNCTION: XWA 0x588771
unsigned int ImVimaEncode(ImVimaState* state, unsigned char* dst, int16_t* samples, unsigned int byteCount,
						  unsigned int channelCount) {
	unsigned int chunkSize;
	unsigned char* out;
	unsigned int encodeChannels;

	out = dst;
	if (channelCount <= 2) {
		*out++ = 0xe4;
		*out++ = 0x11;
		*out++ = 0x11;
		*out++ = 0x64;
		*out++ = 0x22;
		*out++ = 0x22;
		*out++ = 'W';
		*out++ = 'V';
		*out++ = 'S';
		*out++ = 'M';

		while (byteCount) {
			chunkSize = 4096;
			if (byteCount <= 4096) {
				chunkSize = byteCount;
				byteCount = 0;
			} else {
				byteCount -= 4096;
			}
			out += ImVimaEncodeStats(out, samples, (int)chunkSize, NULL);
			samples += 2048;
		}
		return (unsigned int)(out + 10 - dst);
	}

	encodeChannels = channelCount - 2;
	if (encodeChannels > 1) {
		*out++ = (unsigned char)~state->stepIndex[0];
	} else {
		*out++ = (unsigned char)state->stepIndex[0];
	}
	ImWriteBE16(out, state->predictor[0]);
	out += 2;

	if (encodeChannels > 1) {
		*out++ = (unsigned char)state->stepIndex[1];
		ImWriteBE16(out, state->predictor[1]);
		out += 2;
	}

	return ImVimaEncodeBlock(state, out, samples, byteCount / (2 * encodeChannels), encodeChannels, 1, 1) +
		   3 * encodeChannels;
}

// FUNCTION: XWA 0x58A622
unsigned int ImDecodeWvsm(int16_t* dst, unsigned char* src, int byteCount) {
	int sampleCount;
	unsigned char* srcCur;
	int header;
	int leftShift;
	int rightShift;
	int i;
	int left;
	int right;
	int16_t* dstCur;

	srcCur = src;
	dstCur = dst;
	byteCount &= 0xfffffffe;
	sampleCount = byteCount >> 1;
	left = *srcCur;
	++srcCur;
	left <<= 8;
	left |= *srcCur;
	++srcCur;
	header = *srcCur;
	++srcCur;
	leftShift = header >> 4;
	rightShift = header & 0xf;

	for (i = 0; i < sampleCount; i += 2) {
		left = *srcCur;
		++srcCur;
		if (left == 0x80) {
			left = *srcCur << 8;
			++srcCur;
			*dstCur = (int16_t)(left + *srcCur);
			++dstCur;
			++srcCur;
		} else {
			*dstCur = (int16_t)((signed char)left << leftShift);
			++dstCur;
		}

		right = *srcCur;
		++srcCur;
		if (right == 0x80) {
			right = *srcCur << 8;
			++srcCur;
			*dstCur = (int16_t)(right + *srcCur);
			++dstCur;
			++srcCur;
		} else {
			*dstCur = (int16_t)((signed char)right << rightShift);
			++dstCur;
		}
	}

	return (unsigned int)(srcCur - src);
}

// FUNCTION: XWA 0x589C7C
void ImVimaDecodeAdpcm(ImVimaState* state, int16_t* dst, unsigned char* src, int sampleCount,
					   unsigned int channelCount, int resetFlag) {
	unsigned short step;
	unsigned char* srcCur;
	unsigned int code;
	unsigned int channel;
	unsigned int i;
	unsigned int j;
	int sample;
	int bitCount;
	int value;
	unsigned short bitBuffer;
	int stepIndex;

	srcCur = src;
	bitCount = 0;
	if (!g_vimaTableInit) {
		for (i = 0; i < 64; ++i) {
			for (j = 0; j < 89; ++j) {
				step = g_vimaStepTable[j];
				code = 32;
				channel = 0;
				while (code) {
					if ((code & i) != 0) {
						channel += step;
					}
					code >>= 1;
					step >>= 1;
				}
				g_vimaDecodeTable[j][i] = (unsigned short)channel;
			}
		}
		g_vimaTableInit = 1;
	}

	if (!resetFlag) {
		state->stepIndex[1] = 0;
		state->stepIndex[0] = 0;
		state->predictor[1] = 0;
		state->predictor[0] = 0;
	}

	bitBuffer = *srcCur++;
	bitBuffer <<= 8;
	bitBuffer |= *srcCur++;
	for (channel = 0; channel < channelCount; ++channel) {
		int samplesLeft;
		int16_t* dstCur;

		samplesLeft = sampleCount;
		stepIndex = state->stepIndex[channel];
		sample = state->predictor[channel];
		dstCur = dst + channel;
		channelCount <<= 1;

		while (samplesLeft--) {
			unsigned char stepBits;
			int maskBit;
			unsigned char codeMask;

			stepBits = g_vimaCodeBits[stepIndex];
			maskBit = 1 << (stepBits - 1);
			codeMask = (unsigned char)(maskBit - 1);
			bitCount += stepBits;
			code = ((unsigned int)bitBuffer >> (16 - bitCount)) & (maskBit | codeMask);
			if (bitCount > 7) {
				bitCount -= 8;
#ifndef XWA_MODERN
				bitBuffer <<= 8;
				*(unsigned char*)&bitBuffer = *srcCur++;
#else
				bitBuffer = (unsigned short)((bitBuffer << 8) | *srcCur);
				++srcCur;
#endif
			}

			if ((code & maskBit) != 0) {
				code ^= maskBit;
			} else {
				maskBit = 0;
			}

			if (code == codeMask) {
				sample = (int16_t)(bitBuffer << bitCount);
#ifndef XWA_MODERN
				bitBuffer <<= 8;
				*(unsigned char*)&bitBuffer = *srcCur++;
#else
				bitBuffer = (unsigned short)((bitBuffer << 8) | *srcCur++);
#endif
#ifndef XWA_MODERN
				*(unsigned char*)&sample = (unsigned char)(bitBuffer >> (8 - bitCount));
#else
				sample = (sample & ~0xff) | ((bitBuffer >> (8 - bitCount)) & 0xff);
#endif
#ifndef XWA_MODERN
				bitBuffer <<= 8;
				*(unsigned char*)&bitBuffer = *srcCur++;
#else
				bitBuffer = (unsigned short)((bitBuffer << 8) | *srcCur++);
#endif
			} else {
				value = g_vimaDecodeTable[stepIndex][code << (7 - stepBits)];
				if ((unsigned short)code) {
					value += (short)g_vimaStepTable[stepIndex] >> (stepBits - 1);
				}
				if ((unsigned short)maskBit) {
					sample -= value;
					if (sample <= -32768) {
						sample = -32768;
					}
				} else {
					sample += value;
					if (sample >= 32767) {
						sample = 32767;
					}
				}
			}

			*dstCur = (int16_t)sample;
			dstCur = (int16_t*)((char*)dstCur + channelCount);
			{
				int nextStepIndex;

				nextStepIndex = stepIndex + g_vimaStepAdjust[stepBits][code];
				if (nextStepIndex <= 0) {
					nextStepIndex = 0;
				} else if (nextStepIndex >= 88) {
					nextStepIndex = 88;
				}
				stepIndex = nextStepIndex;
			}
		}

		channelCount >>= 1;
		state->stepIndex[channel] = (char)stepIndex;
		state->predictor[channel] = (int16_t)sample;
	}
}

#ifndef XWA_MODERN
#pragma function(memcmp)
#endif
// FUNCTION: XWA 0x588D23
void ImVimaDecodeBlock(ImVimaState* state, int16_t* dst, char* src, unsigned int sampleCount) {
	unsigned int channels;

	channels = 1;
	state->stepIndex[0] = *src;
	++src;
	if (state->stepIndex[0] < 0) {
		state->stepIndex[0] = (char)~state->stepIndex[0];
		++channels;
	}

	state->predictor[0] =
		(int16_t)((((unsigned int)*(unsigned short*)src & 0xffu) << 8) | (*(unsigned short*)src >> 8));
	src += 2;
	if (channels > 1) {
		state->stepIndex[1] = *src;
		++src;
		state->predictor[1] =
			(int16_t)((((unsigned int)*(unsigned short*)src & 0xffu) << 8) | (*(unsigned short*)src >> 8));
		src += 2;
	}

	if (channels == 2 && state->predictor[0] == 0x1111 && state->stepIndex[1] == 100 &&
		state->predictor[1] == 0x2222 && memcmp(src, "WVSM", 4u) == 0) {
		src += 4;
		while (sampleCount) {
			unsigned int chunkBytes;

			chunkBytes = 4096;
			if (sampleCount > 4096) {
				sampleCount -= 4096;
			} else {
				chunkBytes = sampleCount;
				sampleCount = 0;
			}
			src += ImDecodeWvsm(dst, (unsigned char*)src, (int)chunkBytes);
			dst += 2048;
		}
		return;
	}

	ImVimaDecodeAdpcm(state, dst, (unsigned char*)src, (int)(sampleCount / (2 * channels)), channels, 1);
}
#ifndef XWA_MODERN
#pragma intrinsic(memcmp)
#endif
