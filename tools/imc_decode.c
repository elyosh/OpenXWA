#include "xwa/audio/imuse/imuse.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int Tool_Print(const char* fmt, ...) {
	va_list args;
	int result;

	va_start(args, fmt);
	result = vfprintf(stderr, fmt, args);
	va_end(args);
	return result;
}

static void Tool_AssertFail(const char* expr, const char* file, int line) {
	fprintf(stderr, "assertion failed: %s (%s:%d)\n", expr ? expr : "", file ? file : "", line);
}

static int Tool_RegisterAtExit(void (*fn)(void)) { return atexit(fn); }

static void* Tool_AllocMem(unsigned int size) { return malloc((size_t)size); }

static void Tool_FreeMem(void* block) { free(block); }

static void* Tool_ReallocMem(void* block, unsigned int size) { return realloc(block, (size_t)size); }

static void* Tool_OpenFile(const char* path, const char* mode) { return fopen(path, mode); }

static int Tool_CloseFile(void* stream) { return fclose((FILE*)stream); }

static unsigned int Tool_ReadFile(void* stream, void* dst, unsigned int size) {
	return (unsigned int)fread(dst, 1, size, (FILE*)stream);
}

static char* Tool_ReadLine(void* stream, char* dst, int size) { return fgets(dst, size, (FILE*)stream); }

static int Tool_WriteFile(void* stream, void* src, unsigned int size) {
	return (int)fwrite(src, 1, size, (FILE*)stream);
}

static int Tool_AtEof(void* stream) { return feof((FILE*)stream); }

static int Tool_TellFile(void* stream) { return (int)ftell((FILE*)stream); }

static int Tool_SeekFile(void* stream, int offset, int origin) {
	if (fseek((FILE*)stream, offset, origin) != 0) {
		return -1;
	}
	return (int)ftell((FILE*)stream);
}

static int Tool_GetFileSize(const char* path) {
	FILE* file;
	long size;

	file = fopen(path, "rb");
	if (!file) {
		return -1;
	}
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return -1;
	}
	size = ftell(file);
	fclose(file);
	return (int)size;
}

static int Tool_FilePrintf(void* stream, const char* fmt, ...) {
	va_list args;
	int result;

	va_start(args, fmt);
	result = vfprintf((FILE*)stream, fmt, args);
	va_end(args);
	return result;
}

static void Tool_WriteLE16(FILE* file, unsigned int value) {
	unsigned char bytes[2];

	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)((value >> 8) & 0xffu);
	fwrite(bytes, 1, sizeof(bytes), file);
}

static void Tool_WriteLE32(FILE* file, unsigned int value) {
	unsigned char bytes[4];

	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)((value >> 8) & 0xffu);
	bytes[2] = (unsigned char)((value >> 16) & 0xffu);
	bytes[3] = (unsigned char)((value >> 24) & 0xffu);
	fwrite(bytes, 1, sizeof(bytes), file);
}

static int Tool_HasMcmpMagic(const char* path) {
	FILE* file;
	char magic[4];
	int ok;

	file = fopen(path, "rb");
	if (!file) {
		return 0;
	}
	ok = fread(magic, 1, sizeof(magic), file) == sizeof(magic) && memcmp(magic, "MCMP", 4) == 0;
	fclose(file);
	return ok;
}

static int Tool_WriteWav(const char* path, const void* pcm, unsigned int dataSize, int rate, int bits,
						 int channels) {
	FILE* file;
	unsigned int blockAlign;
	unsigned int byteRate;

	file = fopen(path, "wb");
	if (!file) {
		fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return 0;
	}

	blockAlign = (unsigned int)(channels * (bits / 8));
	byteRate = (unsigned int)rate * blockAlign;

	fwrite("RIFF", 1, 4, file);
	Tool_WriteLE32(file, 36u + dataSize);
	fwrite("WAVE", 1, 4, file);
	fwrite("fmt ", 1, 4, file);
	Tool_WriteLE32(file, 16u);
	Tool_WriteLE16(file, 1u);
	Tool_WriteLE16(file, (unsigned int)channels);
	Tool_WriteLE32(file, (unsigned int)rate);
	Tool_WriteLE32(file, byteRate);
	Tool_WriteLE16(file, blockAlign);
	Tool_WriteLE16(file, (unsigned int)bits);
	fwrite("data", 1, 4, file);
	Tool_WriteLE32(file, dataSize);
	fwrite(pcm, 1, dataSize, file);

	if (fclose(file) != 0) {
		fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return 0;
	}
	return 1;
}

static int Tool_AbsSample(int sample) {
	if (sample == -32768) {
		return 32768;
	}
	return sample < 0 ? -sample : sample;
}

static int Tool_ReadS16LE(const unsigned char* p) {
	return (int16_t)((unsigned int)p[0] | ((unsigned int)p[1] << 8));
}

static int Tool_BlockForDecodedOffset(const ImMcmpStream* stream, int offset) {
	int i;

	for (i = 0; i < stream->blockCount; ++i) {
		if (offset >= stream->blockStartOffset[i] && offset < stream->blockStartOffset[i + 1]) {
			return i;
		}
	}
	return -1;
}

static void Tool_LogPathologicalPcm(const char* path, const ImMcmpStream* stream, const char* decoded,
									const char* pcm, int dataSize, int rate, int bits, int channels) {
	enum { TOOL_PATHOLOGY_LOG_LIMIT = 128 };
	const unsigned char* bytes;
	int pcmOffset;
	int frameBytes;
	int totalFrames;
	int logged;
	int totalPathological;
	int blockIndex;

	if (bits != 16 || channels < 1 || channels > 2 || dataSize <= 0 || rate <= 0) {
		return;
	}

	bytes = (const unsigned char*)pcm;
	pcmOffset = (int)(pcm - decoded);
	frameBytes = channels * 2;
	totalFrames = dataSize / frameBytes;
	logged = 0;
	totalPathological = 0;

	for (blockIndex = 0; blockIndex < stream->blockCount; ++blockIndex) {
		int blockStart;
		int blockEnd;
		int pcmStart;
		int pcmEnd;
		int firstFrame;
		int lastFrame;
		int frameCount;
		int channel;

		blockStart = stream->blockStartOffset[blockIndex];
		blockEnd = stream->blockStartOffset[blockIndex + 1];
		pcmStart = blockStart > pcmOffset ? blockStart : pcmOffset;
		pcmEnd = blockEnd < pcmOffset + dataSize ? blockEnd : pcmOffset + dataSize;
		if (pcmStart >= pcmEnd) {
			continue;
		}

		firstFrame = (pcmStart - pcmOffset + frameBytes - 1) / frameBytes;
		lastFrame = (pcmEnd - pcmOffset) / frameBytes;
		if (firstFrame < 0) {
			firstFrame = 0;
		}
		if (lastFrame > totalFrames) {
			lastFrame = totalFrames;
		}
		if (firstFrame >= lastFrame) {
			continue;
		}

		frameCount = lastFrame - firstFrame;
		for (channel = 0; channel < channels; ++channel) {
			int frame;
			int prev;
			int peak;
			int maxDelta;
			int clips;
			int zeroCrossings;
			uint64_t absSum;
			uint64_t squareSum;
			int pathological;

			prev = 0;
			peak = 0;
			maxDelta = 0;
			clips = 0;
			zeroCrossings = 0;
			absSum = 0;
			squareSum = 0;

			for (frame = firstFrame; frame < lastFrame; ++frame) {
				const unsigned char* samplePtr;
				int sample;
				int absSample;

				samplePtr = bytes + (size_t)frame * (size_t)frameBytes + (size_t)channel * 2u;
				sample = Tool_ReadS16LE(samplePtr);
				absSample = Tool_AbsSample(sample);
				if (absSample > peak) {
					peak = absSample;
				}
				if (absSample >= 32767) {
					++clips;
				}
				if (frame > firstFrame) {
					int delta;

					delta = sample - prev;
					if (delta < 0) {
						delta = -delta;
					}
					if (delta > maxDelta) {
						maxDelta = delta;
					}
					if ((sample < 0) != (prev < 0)) {
						++zeroCrossings;
					}
				}
				absSum += (uint64_t)absSample;
				squareSum += (uint64_t)((int64_t)sample * (int64_t)sample);
				prev = sample;
			}

			pathological = clips >= 16 || maxDelta >= 30000 ||
						   (peak >= 30000 && frameCount > 0 && absSum / (uint64_t)frameCount >= 8000u);
			if (pathological) {
				double timeSeconds;
				int pcmBlock;

				++totalPathological;
				if (logged >= TOOL_PATHOLOGY_LOG_LIMIT) {
					continue;
				}
				timeSeconds = (double)firstFrame / (double)rate;
				pcmBlock = Tool_BlockForDecodedOffset(stream, pcmOffset + firstFrame * frameBytes);
				fprintf(stderr,
						"%s: pathology mcmpBlock=%d pcmBlock=%d time=%.3f ch=%d frames=%d peak=%d clips=%d "
						"maxDelta=%d zeroCross=%d avgAbs=%llu meanSq=%llu decodedRange=0x%x..0x%x\n",
						path, blockIndex, pcmBlock, timeSeconds, channel, frameCount, peak, clips, maxDelta,
						zeroCrossings, (unsigned long long)(absSum / (uint64_t)frameCount),
						(unsigned long long)(squareSum / (uint64_t)frameCount), pcmStart, pcmEnd);
				++logged;
			}
		}
	}

	if (!totalPathological) {
		fprintf(stderr, "%s: pathology none\n", path);
	} else if (totalPathological > logged) {
		fprintf(stderr, "%s: pathology suppressed=%d printed=%d\n", path, totalPathological - logged, logged);
	} else {
		fprintf(stderr, "%s: pathology total=%d\n", path, totalPathological);
	}
}

int main(int argc, char** argv) {
	ImHostServices services;
	ImMcmpStream* stream;
	char* decoded;
	char* pcm;
	unsigned int readBytes;
	unsigned int totalSize;
	int format;
	int rate;
	int bits;
	int channels;
	int field6;
	int dataSize;
	int result;

	if (argc != 3) {
		fprintf(stderr, "usage: %s input.IMC output.wav\n", argv[0]);
		return 2;
	}
	if (!Tool_HasMcmpMagic(argv[1])) {
		fprintf(stderr, "%s: not an MCMP/IMC file\n", argv[1]);
		return 1;
	}

	memset(&services, 0, sizeof(services));
	services.version = 1;
	services.printStatus = Tool_Print;
	services.printMessage = Tool_Print;
	services.printWarning = Tool_Print;
	services.printError = Tool_Print;
	services.printDebug = Tool_Print;
	services.assertFail = Tool_AssertFail;
	services.registerAtExit = Tool_RegisterAtExit;
	services.allocMem = Tool_AllocMem;
	services.freeMem = Tool_FreeMem;
	services.reallocMem = Tool_ReallocMem;
	services.openFile = Tool_OpenFile;
	services.closeFile = Tool_CloseFile;
	services.readFile = Tool_ReadFile;
	services.readLine = Tool_ReadLine;
	services.writeFile = Tool_WriteFile;
	services.atEof = Tool_AtEof;
	services.tellFile = Tool_TellFile;
	services.seekFile = Tool_SeekFile;
	services.getFileSize = Tool_GetFileSize;
	services.filePrintf = Tool_FilePrintf;
	g_imHostServicesPtr = &services;

	stream = ImResFopen(argv[1], "rb");
	if (!stream) {
		fprintf(stderr, "%s: could not open through ImResFopen\n", argv[1]);
		return 1;
	}

	totalSize = (unsigned int)stream->totalSize;
	decoded = (char*)malloc((size_t)totalSize);
	if (!decoded) {
		fprintf(stderr, "out of memory allocating %d decoded bytes\n", stream->totalSize);
		ImMcmpClose(stream);
		return 1;
	}

	readBytes = ImMcmpRead(stream, decoded, totalSize);
	if (readBytes != totalSize) {
		fprintf(stderr, "%s: decoded %u/%u bytes\n", argv[1], readBytes, totalSize);
		ImMcmpClose(stream);
		free(decoded);
		return 1;
	}

	format = 0;
	rate = 0;
	bits = 0;
	channels = 0;
	field6 = 0;
	dataSize = 0;
	pcm = ImParseSoundHeader(decoded, &format, &rate, &bits, &channels, &field6, &dataSize, NULL, NULL);
	if (!pcm || dataSize <= 0 || rate <= 0 || (bits != 8 && bits != 16) || (channels != 1 && channels != 2)) {
		fprintf(stderr, "%s: decoded stream has unsupported audio header\n", argv[1]);
		ImMcmpClose(stream);
		free(decoded);
		return 1;
	}

	Tool_LogPathologicalPcm(argv[1], stream, decoded, pcm, dataSize, rate, bits, channels);
	ImMcmpClose(stream);
	result = Tool_WriteWav(argv[2], pcm, (unsigned int)dataSize, rate, bits, channels) ? 0 : 1;
	free(decoded);
	return result;
}
