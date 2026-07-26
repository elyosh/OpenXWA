#include "xwa/audio/imuse/imuse.h"
#include "xwa_runtime/compat/middleware_crt.h"

#include <string.h>

#ifndef XWA_MODERN
#pragma function(memcpy)
#endif

// GLOBAL: XWA 0x5ABC0C
int g_imMusicJumpsEnabled = 1;
// GLOBAL: XWA 0x78A184
int g_imStatStreamRefillThreshold;
// GLOBAL: XWA 0x78A188
int g_imNavEventReady;
// GLOBAL: XWA 0x7AABC8
int g_imStatStreamFill;
// GLOBAL: XWA 0x7AABCC
int g_imStatStreamEof;
// GLOBAL: XWA 0x7AABD0
int g_imNavBufSize;
// GLOBAL: XWA 0x7AABD8
ImCriticalSection g_imNavCritSec;
// GLOBAL: XWA 0x7AABF8
int g_imStatStreamSize;
// GLOBAL: XWA 0x7AABFC
int g_imNavCsInitialized;

#define IM_TAG_FRMT 0x46524d54
#define IM_TAG_JUMP 0x4a554d50
#define IM_TAG_REGN 0x5245474e
#define IM_TAG_STOP 0x53544f50
#define IM_TAG_SYNC 0x53594e43
#define IM_TAG_TEXT 0x54455854

#if defined(XWA_MODERN)
// SYNC chunks may not be naturally aligned in modern host buffers.
static inline uint32_t ImReadNative32(const char* values, int index) {
	uint32_t value;

	memcpy(&value, values + (size_t)index * sizeof(value), sizeof(value));
	return value;
}
#endif

#ifndef XWA_MODERN
#define IM_NAV_STDCALL __stdcall
#else
#define IM_NAV_STDCALL
#endif

#ifndef XWA_MODERN
typedef void(IM_NAV_STDCALL* ImNavCriticalSectionFn)(ImCriticalSection* critSec);
// GLOBAL: XWA 0x5A9128
ImNavCriticalSectionFn InitializeCriticalSection;
// GLOBAL: XWA 0x5A9124
ImNavCriticalSectionFn EnterCriticalSection;
// GLOBAL: XWA 0x5A9120
ImNavCriticalSectionFn LeaveCriticalSection;
#define ImNavInitializeCriticalSection InitializeCriticalSection
#define ImNavEnterCriticalSection EnterCriticalSection
#define ImNavLeaveCriticalSection LeaveCriticalSection
#else
static void IM_NAV_STDCALL ImNavInitializeCriticalSection(ImCriticalSection* critSec) {
	memset(critSec, 0, sizeof(*critSec));
}

static void IM_NAV_STDCALL ImNavEnterCriticalSection(ImCriticalSection* critSec) { (void)critSec; }

static void IM_NAV_STDCALL ImNavLeaveCriticalSection(ImCriticalSection* critSec) { (void)critSec; }
#endif

// FLAGS: /O2 /Og- /Oi-
// FUNCTION: XWA 0x5932E4
int ImMatchJumpHook(int* trackHook, int hookId) {
	if (hookId) {
		if (hookId == *trackHook) {
			*trackHook = 0;
			return 0;
		}
		return -1;
	}

	if (*trackHook == 128) {
		*trackHook = 0;
		return -1;
	}
	return 0;
}

// FUNCTION: XWA 0x58907B
int ImSyncLookup(char* syncChunk, unsigned int position, unsigned char* outCodeHi, unsigned char* outCodeLo) {
	int failed;
	int index;

	{
		int low;
		int mid;

		{
			char codeHi = 0;
			char codeLo = 0;

			failed = 0;

			{
				int count;

				{
					int high;
#if defined(XWA_MODERN)
					char* cursor;
#else
					uint32_t* cursor;
#endif

#if defined(XWA_MODERN)
					cursor = syncChunk;
#else
					cursor = (uint32_t*)syncChunk;
#endif

					if (memcmp(syncChunk, "SYNC", 4) != 0) {
						failed = 1;
					} else {
#if defined(XWA_MODERN)
						cursor += sizeof(uint32_t);
						count = (int)ImReadNative32(cursor, 0);
						cursor += sizeof(uint32_t);
#else
						cursor += 1;
						count = *(int*)cursor;
						cursor += 1;
#endif

#if !defined(XWA_MODERN)
						if (position < 0) {
							// Retain the original unsigned guard for legacy code generation.
						} else {
#endif
							if (((unsigned int)position & 0xfff00000u) != 0) {
								failed = 1;
							} else {
								position = (int)((unsigned int)position << 12);
								position = (int)((unsigned int)position & 0xffff0000u);
								low = 0;
								high = count - 1;
								index = 0;

#if defined(XWA_MODERN)
								if ((unsigned int)position > ImReadNative32(cursor, high)) {
#else
							if ((unsigned int)position > cursor[high]) {
#endif
									index = high;
									position = 0;
								}

								while (position != 0) {
									uint32_t probe;

									mid = (high - low) / 2 + low;
									if (low + 1 >= high) {
										index = mid;
										break;
									}

#if defined(XWA_MODERN)
									probe = ImReadNative32(cursor, mid);
#else
								probe = cursor[mid];
#endif
									if ((unsigned int)position == (probe & 0xffff0000u)) {
										index = mid;
										break;
									}
									if ((unsigned int)position > probe) {
										low = mid;
									} else {
										high = mid;
									}
								}

								{
#if defined(XWA_MODERN)
									uint32_t selected = ImReadNative32(cursor, index);
#else
								uint32_t selected = cursor[index];
#endif

									codeHi = (unsigned char)((selected >> 8) & 0x7f);
									codeLo = (unsigned char)(selected & 0x7f);
								}
							}
#if !defined(XWA_MODERN)
						}
#endif
					}
				}
			}

			if (outCodeHi) {
				*outCodeHi = codeHi;
			}
			if (outCodeLo) {
				*outCodeLo = codeLo;
			}
		}
	}
	return failed;
}

// FUNCTION: XWA 0x58DCD2
unsigned char* ImFindChunk(unsigned char* blob, const char* tag, int occurrence) {
	unsigned int totalSize;
	unsigned int offset;
	int mismatch;
	int i;
	unsigned char* p;

	if (!blob) {
		return NULL;
	}

	p = blob + 4;
	totalSize = (unsigned int)ImReadBE32(p);
	p += 4;
	for (offset = 0; offset < totalSize;) {
		mismatch = 0;
		for (i = 0; i < 4; ++i) {
			if ((char)p[offset++] != tag[i]) {
				mismatch = 1;
			}
		}
		if (!mismatch && !occurrence--) {
			return p + offset - 4;
		}
		offset += (unsigned int)ImReadBE32(p + offset) + 4;
	}
	return NULL;
}

// FUNCTION: XWA 0x58F4D4
int ImGetMap(ImDispatch* dispatch) {
	char mapHeader[80];
	int dataSize;
	int mapSize;
	int* mapPtr;
	char* buf;

	if (*(int*)dispatch->map == 0x4d415020) {
		if (!dispatch->stream) {
			return 0;
		}
		if (dispatch->pendingJumpZones) {
			return 0;
		}
	}

	if (dispatch->currentOffset) {
		ImLog("ERR: GetMap found offset but no map...");
		return -1;
	}

	if (!dispatch->stream) {
		buf = (char*)ImMapSoundAddr((unsigned int)dispatch->track->soundId);
		if (!buf) {
			ImLog("ERR: GetMap() couldn't get sound address...");
			return -1;
		}

		if (ImReadBE32((unsigned char*)buf) != 0x694d5553 ||
			ImReadBE32((unsigned char*)buf + 8) != 0x4d415020) {
			ImLog("ERR1: unrecognized file format in stream buf... %x %x %x %x", buf[0], buf[1], buf[2],
				  buf[3]);
			return -1;
		}

		mapSize = ImReadBE32((unsigned char*)buf + 12);
		dispatch->currentOffset = mapSize + 24;
		buf += 8;
		if (ImConvertMap(buf, dispatch->map)) {
			ImLog("ERR: ConvertMap() failed...");
			return -1;
		}

		mapPtr = dispatch->map + 8;
		if (*(int*)mapPtr != 0x46524d54) {
			ImLog("ERR: expected 'FRMT' at start of map...");
			return -1;
		}
		if (dispatch->currentOffset != ((int*)mapPtr)[2]) {
			ImLog("ERR: GetMap() expected data to follow map...");
			return -1;
		}

		ImLog("End of NON-Stream section.\n");
		return 0;
	}

	dataSize = 0;
	buf = (char*)ImStreamPeek(dispatch->stream, 0, 0x40u);
	if (!buf) {
		return -3;
	}

	if (!memcmp(buf, "RIFF", 4u) && !memcmp(buf + 8, "WAVEfmt ", 8u)) {
		int channels;
		char* dataPtr;
		int sampleRate;

#ifndef XWA_MODERN
		dataPtr = ((char* (*)(char*, int*, int*, int*, int*, int*, int*))ImParseSoundHeader)(
			buf, NULL, &sampleRate, NULL, &channels, NULL, &dataSize);
#else
		dataPtr = ImParseSoundHeader(buf, NULL, &sampleRate, NULL, &channels, NULL, &dataSize, NULL, NULL);
#endif
		dispatch->currentOffset = (int)(dataPtr - buf);
		buf = (char*)ImStreamGet(dispatch->stream, (unsigned int)dispatch->currentOffset);
		if (!buf) {
			ImLog("ERR1: stream read failed after view succeeded in GetMap()...");
			return -1;
		}
#ifndef XWA_MODERN
		((int (*)(void*, int, unsigned int, unsigned int, unsigned int, unsigned int))ImBuildMap)(
			mapHeader, 3, (unsigned int)sampleRate, 16u, (unsigned int)channels, (unsigned int)dataSize);
#else
		ImBuildMap(mapHeader, 3, (unsigned int)sampleRate, 16u, (unsigned int)channels,
				   (unsigned int)dataSize, NULL);
#endif
		mapHeader[31] = 1;
		buf = mapHeader;
	} else {
		if (memcmp(buf, "iMUS", 4u) || memcmp(buf + 8, "MAP ", 4u)) {
			ImLog("ERR2: unrecognized file format in stream buf... %x %x %x %x", buf[0], buf[1], buf[2],
				  buf[3]);
			return -1;
		}

		mapSize = ImReadBE32((unsigned char*)buf + 12);
		buf = (char*)ImStreamPeek(dispatch->stream, 0, (unsigned int)(mapSize + 24));
		if (!buf) {
			return -3;
		}
		buf = (char*)ImStreamGet(dispatch->stream, (unsigned int)(mapSize + 24));
		if (!buf) {
			ImLog("ERR: stream read failed after view succeeded in GetMap()...");
			return -1;
		}
		dispatch->currentOffset = mapSize + 24;
	}

	buf += 8;
	if (ImConvertMap(buf, dispatch->map)) {
		ImLog("ERR: ConvertMap() failed...");
		return -1;
	}

	if (dataSize) {
		int* mapOffset;

		mapOffset = (int*)(dispatch->map + 16);
		*mapOffset = dispatch->currentOffset;
		mapOffset = (int*)(dispatch->map + 44);
		*mapOffset = dispatch->currentOffset;
		mapOffset = (int*)(dispatch->map + 60);
		*mapOffset = dispatch->currentOffset + dataSize;
	}

	mapPtr = (int*)(dispatch->map + 8);
	if (*mapPtr != 0x46524d54) {
		ImLog("ERR: expected 'FRMT' at start of map...");
		return -1;
	}
	if (dispatch->currentOffset != mapPtr[2]) {
		ImLog("ERR: GetMap() expected data to follow map...");
		return -1;
	}
	if (dispatch->pendingJumpZones) {
		ImLog("ERR: GetMap() expected null streamZoneList...");
		return -1;
	}

	dispatch->pendingJumpZones = ImAllocStreamZone();
	if (!dispatch->pendingJumpZones) {
		ImLog("ERR: GetMap() couldn't alloc zone...");
		return -1;
	}

	dispatch->pendingJumpZones->streamOffset = dispatch->currentOffset;
	dispatch->pendingJumpZones->byteCount = ImGetStreamFill(dispatch->stream);
	dispatch->pendingJumpZones->isPrefetch = 0;
	ImLog("End of Stream section.\n");
	return 0;
}

// FUNCTION: XWA 0x58F9BC
int ImConvertMap(char* src, char* dst) {
	int mapSize;
	int tag;
	unsigned int chunkSize;
	unsigned int i;
	unsigned char* p;
	unsigned char* end;

	if (ImReadBE32((unsigned char*)src) != 0x4d415020) {
		ImLog("ERR: ConvertMap() got bogus map...");
		return -1;
	}

	mapSize = ImReadBE32((unsigned char*)src + 4);
	if ((unsigned int)(mapSize + 8) > 0x2000u) {
		ImLog("ERR: MAP TOO BIG (%lu)!!!...", mapSize + 8);
		return -1;
	}

	memcpy(dst, src, (size_t)(mapSize + 8));
	p = (unsigned char*)dst;
	*(int*)p = ImReadBE32(p);
	p += 4;
	*(int*)p = ImReadBE32(p);
	p += 4;

	end = (unsigned char*)dst + mapSize + 8;
	while (p < end) {
		*(int*)p = ImReadBE32(p);
		tag = *(int*)p;
		p += 4;
		*(int*)p = ImReadBE32(p);
		chunkSize = *(unsigned int*)p;
		p += 4;

		if (tag == 0x54455854) {
			*(int*)p = ImReadBE32(p);
			p += 4;
			while (*p++) {
			}
		} else {
			for (i = 0; i < (chunkSize >> 2); ++i) {
				*(int*)p = ImReadBE32(p);
				p += 4;
			}
		}
	}

	if (p != end) {
		ImLog("ERR: ConvertMap() converted wrong number of bytes...");
		return -1;
	}
	return 0;
}

// FUNCTION: XWA 0x58FDEF
void ImPrepareToJump(ImDispatch* dispatch, ImStreamZone* zone, ImMapEvent* jumpEvent, int force) {
	ImStreamZone* prefetchZone;
	ImStreamZone* cur;

	prefetchZone = NULL;
	if (zone->streamOffset + zone->byteCount == jumpEvent->sourceOffset && zone->next) {
		if (zone->next->isPrefetch) {
			if (zone->next->streamOffset == jumpEvent->sourceOffset && zone->next->next &&
				zone->next->next->streamOffset == jumpEvent->destOffset) {
				return;
			}
		} else if (zone->next->streamOffset == jumpEvent->destOffset) {
			return;
		}
	}

	g_imJumpZoneSize = ((((unsigned int)jumpEvent->fadeMs * (unsigned int)dispatch->fmtB) / 1000u) *
						(unsigned int)dispatch->fmtA * (unsigned int)dispatch->fmtC) >>
					   3;
	if (!force && (unsigned int)zone->byteCount <
					  (unsigned int)(jumpEvent->sourceOffset - zone->streamOffset + g_imJumpZoneSize)) {
		return;
	}

	{
		int clampedSize;
		ImStreamZone* jumpZone;
		int count;

		if ((unsigned int)g_imJumpZoneSize <
			(unsigned int)(zone->byteCount - (jumpEvent->sourceOffset - zone->streamOffset))) {
			clampedSize = g_imJumpZoneSize;
		} else {
			clampedSize = zone->byteCount - (jumpEvent->sourceOffset - zone->streamOffset);
		}
		g_imJumpZoneSize = clampedSize;
		g_imJumpZoneSize =
			g_imJumpZoneSize - (g_imJumpZoneSize & ((dispatch->fmtA >> 3) * dispatch->fmtC - 1));

		if ((unsigned int)jumpEvent->sourceOffset < 2000u &&
			(unsigned int)jumpEvent->destOffset > (unsigned int)jumpEvent->sourceOffset) {
			g_imJumpZoneSize = 0;
		}
		if (dispatch->fadeBytesAvailable) {
			g_imJumpZoneSize = 0;
		}

		if (g_imJumpZoneSize) {
			prefetchZone = ImAllocStreamZone();
			if (!prefetchZone) {
				ImLog("ERR: PrepareToJump() couldn't alloc zone...");
				return;
			}
		}

		jumpZone = ImAllocStreamZone();
		if (!jumpZone) {
			ImLog("ERR: PrepareToJump() couldn't alloc zone...");
			return;
		}

		zone->byteCount = jumpEvent->sourceOffset - zone->streamOffset;
		count = zone->byteCount + g_imJumpZoneSize;
		cur = dispatch->pendingJumpZones;
		while (cur != zone) {
			count += cur->byteCount;
			cur = cur->next;
		}
		ImStreamSetAvail(dispatch->stream, (unsigned int)count);

		while (zone->next) {
			zone->next->inUse = 0;
			ImListRemove2((ImListNode**)&zone->next, (ImListNode*)zone->next);
		}

		ImStreamSeek(dispatch->stream, dispatch->track->soundId, jumpEvent->destOffset);

		if (g_imJumpZoneSize) {
			zone->next = prefetchZone;
			prefetchZone->prev = zone;
			prefetchZone->next = NULL;
			prefetchZone->streamOffset = jumpEvent->sourceOffset;
			prefetchZone->byteCount = g_imJumpZoneSize;
			prefetchZone->isPrefetch = 1;
			zone = prefetchZone;
		}

		zone->next = jumpZone;
		jumpZone->prev = zone;
		jumpZone->next = NULL;
		jumpZone->streamOffset = jumpEvent->destOffset;
		jumpZone->byteCount = 0;
		jumpZone->isPrefetch = 0;
	}
}

// FUNCTION: XWA 0x58FC39
void ImPredictStream(ImDispatch* dispatch) {
	ImStreamZone* tailZone;
	int byteCount;
	ImMapEvent* jumpEvent;

	tailZone = NULL;
	if (!dispatch->stream || !dispatch->pendingJumpZones) {
		ImLog("ERR: null streamID or zoneList in PredictStream()...");
		return;
	}

	{
		ImStreamZone* zone;

		zone = dispatch->pendingJumpZones;
		byteCount = 0;
		while (zone) {
			byteCount += zone->byteCount;
			tailZone = zone;
			zone = zone->next;
		}

		tailZone->byteCount += ImGetStreamFill(dispatch->stream) - byteCount;
		zone = dispatch->pendingJumpZones;
		g_imPredictTrackHook = dispatch->track->hookId;
		while (zone) {
			if (!zone->isPrefetch) {
				jumpEvent = ImFindJump((ImMapChunk*)dispatch->map, zone, &g_imPredictTrackHook);
				if (jumpEvent) {
					ImPrepareToJump(dispatch, zone, jumpEvent, 0);
				} else {
					ImCommitJump(dispatch, zone);
				}
			}
			zone = zone->next;
		}
	}
}

// FUNCTION: XWA 0x58EF56
void ImServiceStreamJumps(void) {
	ImDispatch* dispatch;

	dispatch = g_imDispatchPool;
	ImIncBusyCount();

	{
		int i;

		for (i = 0; i < g_imMaxTracks[0]; ++i, ++dispatch) {
			if (dispatch->track->soundId) {
				if (dispatch->stream) {
					if (dispatch->pendingJumpZones) {
						ImPredictStream(dispatch);
					}
				}
			}
		}
	}

	ImDecBusyCount();
}

// FUNCTION: XWA 0x58EFCB
int ImNavigateMap(ImDispatch* dispatch) {
	int result;
	int mapResult;
	ImMapEvent* event;

	result = -1;
	if (!g_imNavCsInitialized) {
		ImNavInitializeCriticalSection(&g_imNavCritSec);
		g_imNavCsInitialized = 1;
	}

	ImNavEnterCriticalSection(&g_imNavCritSec);
	mapResult = ImGetMap(dispatch);
	if (mapResult) {
		result = mapResult;
	} else if (dispatch->regionEndOffset || !dispatch->pendingJumpZones ||
			   (dispatch->stream && dispatch->currentOffset != dispatch->pendingJumpZones->streamOffset)) {
		ImLog("ERR: navigation error in dispatch...");
	} else {
		int soundId;
		unsigned int chunkSize;

		event = NULL;
		while (g_imMusicJumpsEnabled) {
			event = ImGetNextMapEvent((ImMapChunk*)dispatch->map, dispatch->currentOffset, event);
			if (!event) {
				ImLog("ERR: no more map events at offset %lx...", dispatch->currentOffset);
				goto done;
			}

			switch (event->tag) {
				case IM_TAG_FRMT:
					dispatch->atRegionStart = event->destOffset == 0;
					dispatch->fmtA = event->hookId;
					dispatch->fmtB = event->fadeMs;
					dispatch->fmtC = *(int*)((char*)event + 24);
					goto nextEvent;

				case IM_TAG_REGN:
					if (dispatch->currentOffset != event->sourceOffset) {
						ImLog("ERR: region offset %lu != currentOffset %lu...", event->sourceOffset,
							  dispatch->currentOffset);
					}
					dispatch->regionEndOffset = event->destOffset;
					result = 0;
					goto done;

				case IM_TAG_TEXT:
					soundId = dispatch->track->soundId;
					ImTgProcessMarker(dispatch->track->soundId, (char*)event + 12);
					if (soundId != dispatch->track->soundId) {
						result = -3;
						goto done;
					}
					if (dispatch->regionEndOffset) {
						result = 0;
						goto done;
					}
					goto nextEvent;

				case IM_TAG_JUMP:
					if (!ImMatchJumpHook(&dispatch->track->hookId, event->hookId)) {
						dispatch->currentOffset = event->destOffset;
						if (dispatch->stream) {
							if (dispatch->pendingJumpZones->byteCount || !dispatch->pendingJumpZones->next) {
								ImLog("ERR: failed to prepare for jump...");
								if (!dispatch->pendingJumpZones) {
									ImLog("iMUSE CRASH! StreamID = %d\n", dispatch->stream);
								}
								ImPrepareToJump(dispatch, dispatch->pendingJumpZones, event, 1);
							}

							dispatch->pendingJumpZones->inUse = 0;
							ImListRemove2((ImListNode**)&dispatch->pendingJumpZones,
										  (ImListNode*)dispatch->pendingJumpZones);

							if (dispatch->pendingJumpZones->isPrefetch) {
								if (dispatch->jumpBuffer) {
									ImFreeFadeBuf(dispatch->jumpBuffer);
								}

								g_imNavBufSize = dispatch->pendingJumpZones->byteCount;
								dispatch->jumpInProgress = 1;
								dispatch->jumpBuffer = ImAllocFadeBuf((unsigned int*)&g_imNavBufSize);
								if (dispatch->jumpBuffer) {
									dispatch->fadeReadOffset = 0;
									dispatch->fadeBytesAvailable = 0;
									dispatch->fadeBitsPerSample = dispatch->fmtA;
									dispatch->fadeSampleRate = dispatch->fmtB;
									dispatch->fadeChannels = dispatch->fmtC;
									dispatch->jumpFadeHoldMask = 0;
									dispatch->jumpFadeHoldFrames = 0;
									dispatch->crossfadeQ16Pos = 0x7f0000;
									dispatch->crossfadeQ16Step = 0;

									while ((unsigned int)dispatch->fadeBytesAvailable <
										   (unsigned int)g_imNavBufSize) {
										chunkSize = (unsigned int)(g_imNavBufSize -
																   dispatch->fadeBytesAvailable) < 0x4000u
														? (unsigned int)(g_imNavBufSize -
																		 dispatch->fadeBytesAvailable)
														: 0x4000u;
										memcpy_0((char*)dispatch->jumpBuffer + dispatch->fadeBytesAvailable,
												 ImStreamGet(dispatch->stream, chunkSize), chunkSize);
										dispatch->fadeBytesAvailable += (int)chunkSize;
									}
									g_imNavEventReady = 1;
								}

								dispatch->jumpInProgress = 0;
								dispatch->pendingJumpZones->inUse = 0;
								ImListRemove2((ImListNode**)&dispatch->pendingJumpZones,
											  (ImListNode*)dispatch->pendingJumpZones);
							}
						}
						event = NULL;
					}
					goto nextEvent;

				case IM_TAG_STOP:
					goto done;

				case IM_TAG_SYNC:
					goto nextEvent;

				default:
					ImLog("ERR: Unrecognized map event at offset %lx...", dispatch->currentOffset);
					goto done;
			}
		nextEvent:
			continue;
		}
	}

done:
	ImNavLeaveCriticalSection(&g_imNavCritSec);
	return result;
}

// FUNCTION: XWA 0x58FB6D
ImMapEvent* ImGetNextMapEvent(ImMapChunk* map, int offset, ImMapEvent* prevEvent) {
	ImMapEvent* event;

	{
		int mapSize = map->size;

		if (prevEvent) {
			event = prevEvent;
			event = (ImMapEvent*)((char*)event + prevEvent->size + 8);
#ifdef XWA_MODERN
			if ((uintptr_t)event >= (uintptr_t)map + mapSize + sizeof(*map)) {
#else
			if (event >= (ImMapEvent*)((char*)map + mapSize + sizeof(*map))) {
#endif
				ImLog("ERR: GetNextMapEvent() map overrun...");
				return NULL;
			}
			if (event->sourceOffset != offset) {
				ImLog("ERR: GetNextMapEvent() no more events at offset %lu...", offset);
				return NULL;
			}
			return event;
		}

		event = (ImMapEvent*)((char*)map + sizeof(*map));
		while (g_imMusicJumpsEnabled) {
			if (event->sourceOffset == offset) {
				return event;
			}
			event = (ImMapEvent*)((char*)event + event->size + 8);
#ifdef XWA_MODERN
			if ((uintptr_t)event >= (uintptr_t)map + mapSize + sizeof(*map)) {
#else
			if (event >= (ImMapEvent*)((char*)map + mapSize + sizeof(*map))) {
#endif
				ImLog("ERR: GetNextMapEvent() couldn't find event at offset %lu...", offset);
				return NULL;
			}
		}
#ifdef XWA_MODERN
		return (ImMapEvent*)map;
#endif
	}
}

// FUNCTION: XWA 0x58FD4A
ImMapEvent* ImFindJump(ImMapChunk* region, ImStreamZone* zone, int* trackHook) {
	ImMapEvent* event;
	unsigned int sourceOffset;

	event = (ImMapEvent*)((char*)region + sizeof(*region));
	{
		int regionSize;
		unsigned int maxOffset;

		regionSize = region->size;
		sourceOffset = (unsigned int)zone->streamOffset;
		maxOffset = (unsigned int)zone->streamOffset + (unsigned int)zone->byteCount;

		while (g_imMusicJumpsEnabled) {
			ImMapEvent* currentEvent = event;
#ifdef XWA_MODERN
			if ((uintptr_t)currentEvent >= (uintptr_t)region + regionSize + sizeof(*region)) {
#else
			if (currentEvent >= (ImMapEvent*)((char*)region + regionSize + sizeof(*region))) {
#endif
				return NULL;
			}
			if (currentEvent->tag == 0x4a554d50 && (unsigned int)currentEvent->sourceOffset > sourceOffset &&
				(unsigned int)currentEvent->sourceOffset <= maxOffset &&
				!ImMatchJumpHook(trackHook, currentEvent->hookId)) {
				return currentEvent;
			}
			event = (ImMapEvent*)((char*)event + event->size + 8);
		}
	}
#ifdef XWA_MODERN
	return event;
#endif
}
