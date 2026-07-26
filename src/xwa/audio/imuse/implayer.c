#include "xwa/audio/imuse/imuse.h"

#include "aeron/log.h"
#include "xwa_runtime/compat/middleware_crt.h"

#include <string.h>

// GLOBAL: XWA 0x789210
int g_imRenderRate;
// GLOBAL: XWA 0x789214
ImTrack* g_imActivePlayers;
// GLOBAL: XWA 0x789218
unsigned int g_imRenderFrameLen;
// GLOBAL: XWA 0x789224
int g_imRenderFramesPending;
// GLOBAL: XWA 0x789228
void* g_imRenderWritePtr[4];

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x58BF99
int ImApplyVolumes(void) {
	ImTrack* track;

	track = g_imActivePlayers;
	ImIncBusyCount();
	while (track) {
		track->effectiveVolume =
			((track->volume + 1) * ImGetGroupGain((unsigned int)track->volumeGroup)) >> 7;
		track = track->next;
	}
	ImDecBusyCount();
	return 0;
}

// FUNCTION: XWA 0x58679D
int ImGetNextSound(int prevSoundId) { return (int)ImNextSoundId((unsigned int)prevSoundId); }

// FUNCTION: XWA 0x58C277
unsigned int ImNextSoundId(unsigned int prevSoundId) {
	ImTrack* track;
	unsigned int soundId;

	soundId = 0;
	track = g_imActivePlayers;
	ImIncBusyCount();
	while (track) {
		if ((unsigned int)track->soundId > prevSoundId &&
			(!soundId || (unsigned int)track->soundId < soundId)) {
			soundId = (unsigned int)track->soundId;
		}
		track = track->next;
	}
	ImDecBusyCount();
	return soundId;
}

// FUNCTION: XWA 0x58C0D4
int ImStartSoundCore(int soundId, int priority, int streamSize) {
	ImTrack* track;

	if (priority > 127) {
		priority = 127;
	} else if (priority < 0) {
		priority = 0;
	}

	track = ImAllocTrack(priority);
	if (!track) {
		return -6;
	}

	ImIncBusyCount();
	track->soundId = soundId;
	track->field_10 = 0;
	track->volumeGroup = 0;
	track->priority = priority;
	track->volume = 127;
	track->effectiveVolume = ImGetGroupGain(0);
	track->pan = 64;
	track->detune = 0;
	track->transpose = 0;
	track->pitch = 256;
	track->field_34 = 0;
	track->hookId = 0;

	if (ImDispatchStartStream(track, streamSize)) {
		ImLog("ERR:dispatch couldn't start sound...");
		track->soundId = 0;
		ImDecBusyCount();
		return -1;
	}

	ImListAdd2((ImListNode**)&g_imActivePlayers, (ImListNode*)track);
	ImDecBusyCount();
	return 0;
}

// FUNCTION: XWA 0x58C41B
int ImTrSetParam(int soundId, ImSoundParam param, int value) {
	ImTrack* track;

	track = g_imActivePlayers;
	ImIncBusyCount();
	while (track) {
		if (track->soundId == soundId) {
			switch (param) {
				case P_VGROUP:
					if ((unsigned int)value >= 0x10u) {
						return -5;
					}
					track->volumeGroup = value;
					track->effectiveVolume = ((track->volume + 1) * ImGetGroupGain((unsigned int)value)) >> 7;
					ImDecBusyCount();
					return 0;
				case P_PRIORITY:
					if ((unsigned int)value > 0x7fu) {
						return -5;
					}
					track->priority = value;
					ImDecBusyCount();
					return 0;
				case P_VOLUME:
					if ((unsigned int)value > 0x7fu) {
						return -5;
					}
					track->volume = value;
					track->effectiveVolume =
						((value + 1) * ImGetGroupGain((unsigned int)track->volumeGroup)) >> 7;
					ImDecBusyCount();
					return 0;
				case P_PAN:
					if ((unsigned int)value > 0x7fu) {
						return -5;
					}
					track->pan = value;
					ImDecBusyCount();
					return 0;
				case P_DETUNE:
					if (value < -9216 || value > 9216) {
						return -5;
					}
					track->detune = value;
					track->pitch = value + (track->transpose << 8);
					ImDecBusyCount();
					return 0;
				case P_PITCH:
					if (value < 0 || value > 0xfff) {
						ImDecBusyCount();
						return -5;
					}
					track->pitch = value;
					ImDecBusyCount();
					return 0;
				case P_PARAM_A00:
					track->field_34 = value;
					ImDecBusyCount();
					return 0;
				default:
					ImLog("ERR: TrSetParam() couldn't set param %lu...", param);
					ImDecBusyCount();
					return -5;
			}
		}
		track = track->next;
	}

	ImDecBusyCount();
	return -4;
}

// FUNCTION: XWA 0x58C657
int ImTrGetParam(int soundId, ImSoundParam param) {
	ImTrack* track;
	int count;

	count = 0;
	track = g_imActivePlayers;
	while (track) {
		if (track->soundId == soundId) {
			switch (param) {
				case P_NONE:
				case P_STATUS:
					return -1;
				case P_COUNT:
					++count;
					break;
				case P_STREAM_POS: {
					ImDispatch* dispatch;
					int denominator;

					dispatch = track->dispatch;
					if (!dispatch->fmtA || !dispatch->fmtB || !dispatch->fmtC) {
						return 0;
					}
					denominator = dispatch->fmtB * (dispatch->fmtA >> 3) * dispatch->fmtC / 200;
					return (int)((uint32_t)(5 * dispatch->currentOffset) / (uint32_t)denominator);
				}
				case P_PARAM_300:
					return track->field_10;
				case P_VGROUP:
					return track->volumeGroup;
				case P_PRIORITY:
					return track->priority;
				case P_VOLUME:
					return track->volume;
				case P_PAN:
					return track->pan;
				case P_DETUNE:
					return track->detune;
				case P_PITCH:
					return track->transpose;
				case P_PARAM_A00:
					return track->field_34;
				case P_IS_STREAMING:
					return track->dispatch->stream != NULL;
				case P_STREAM_SIZE:
					return track->dispatch->streamSize;
				default:
					return -5;
			}
		}
		track = track->next;
	}

	return param == P_COUNT ? count : -4;
}

// FUNCTION: XWA 0x586858
int ImGetHook(int soundId) { return ImGetHookCore(soundId); }

// FUNCTION: XWA 0x58C8F7
int ImGetHookCore(int soundId) {
	ImTrack* track;

	track = g_imActivePlayers;
	while (track) {
		if (track->soundId == soundId) {
			return track->hookId;
		}
		track = track->next;
	}
	return -4;
}

// FUNCTION: XWA 0x586843
int ImSetHook(int soundId, int hookId) { return ImSetHookCore(soundId, (unsigned int)hookId); }

// FUNCTION: XWA 0x58C8A9
int ImSetHookCore(int soundId, unsigned int hookId) {
	ImTrack* track;

	track = g_imActivePlayers;
	if (hookId > 0x80) {
		return -5;
	}

	while (track) {
		if (track->soundId == soundId) {
			track->hookId = (int)hookId;
			return 0;
		}
		track = track->next;
	}
	return -4;
}

// FUNCTION: XWA 0x5901AA
int ImGetFadeOutGain(ImDispatch* dispatch) {
	int gain;

	gain = ((127 - (dispatch->crossfadeQ16Pos >> 16) + 1) * dispatch->track->effectiveVolume) >> 7;
	if (!dispatch->crossfadeQ16Step) {
		unsigned int fadeBytes;

		if ((unsigned int)dispatch->fadeBytesAvailable > 1u) {
			fadeBytes = (unsigned int)dispatch->fadeBytesAvailable;
		} else {
			fadeBytes = 2;
		}
		dispatch->crossfadeQ16Step = 0 - (0x7f0000 / fadeBytes);
	}
	return gain;
}

// FUNCTION: XWA 0x590220
int ImGetFadeInGain(ImDispatch* dispatch, int delta) {
	int gain;

	gain = (dispatch->track->effectiveVolume * ((dispatch->crossfadeQ16Pos >> 16) + 1)) >> 7;
	dispatch->crossfadeQ16Pos += delta * dispatch->crossfadeQ16Step;
	if (dispatch->crossfadeQ16Pos < 0) {
		dispatch->crossfadeQ16Pos = 0;
	}
	if (dispatch->crossfadeQ16Pos > 0x7f0000) {
		dispatch->crossfadeQ16Pos = 0x7f0000;
	}
	return gain;
}

// FUNCTION: XWA 0x58E20C
int ImDispatchStartStream(ImTrack* track, int streamSize) {
	ImDispatch* dispatch;

	dispatch = track->dispatch;
	dispatch->jumpBuffer = NULL;
	dispatch->map[0] = 0;
	dispatch->regionEndOffset = 0;
	dispatch->currentOffset = 0;
	dispatch->jumpInProgress = 0;

	if (streamSize) {
		dispatch->stream = ImStreamOpen((unsigned int)track->soundId, streamSize, 0x4000u);
		if (dispatch->stream) {
			dispatch->streamSize = streamSize;
			dispatch->pendingJumpZones = NULL;
			dispatch->streamStarved = 0;
		} else {
			ImLog("ERR: unable to alloc stream...");
			return -1;
		}
	} else {
		dispatch->stream = NULL;
	}

	{
		int result;

		result = ImNavigateMap(dispatch);
		if (result && result != -3) {
			ImLog("ERR: problem starting sound in dispatch...");
			ImDpFreeStream(dispatch->track);
			return -1;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x58E30D
int ImDpFreeStream(ImTrack* track) {
	ImDispatch* dispatch;

	dispatch = track->dispatch;
	if (dispatch->stream) {
		ImFreeStream(dispatch->stream);
		while (dispatch->pendingJumpZones) {
			dispatch->pendingJumpZones->inUse = 0;
			ImListRemove2((ImListNode**)&dispatch->pendingJumpZones, (ImListNode*)dispatch->pendingJumpZones);
		}
	}

	if (dispatch->jumpBuffer) {
		ImFreeFadeBuf(dispatch->jumpBuffer);
	}
	return 0;
}

// FUNCTION: XWA 0x58E396
int ImDpSwitchStream(int curSoundId, int newSoundId, unsigned int fadeMs, uint64_t switchFlags) {
	unsigned int chunkSize;
	unsigned int streamFill;
	void* streamChunk;
	int result;
	int frameBytes;
	int i;
	uint32_t switchFlagsHigh;
	uint32_t switchFlagsLow;
	ImDispatch* dispatch;

	switchFlagsLow = (uint32_t)switchFlags;
	switchFlagsHigh = (uint32_t)(switchFlags >> 32);
	dispatch = g_imDispatchPool;
	ImIncBusyCount();
	if (fadeMs > 2000u) {
		fadeMs = 2000u;
	}

	for (i = 0;
		 i < g_imMaxTracks[0] && (!curSoundId || curSoundId != dispatch->track->soundId || !dispatch->stream);
		 ++i) {
		++dispatch;
	}

	if (i == g_imMaxTracks[0]) {
		ImLog("ERR: DpSwitchStream() couldn't find sound...");
		ImDecBusyCount();
		return -1;
	}

	if (dispatch->pendingJumpZones) {
		if (!dispatch->fmtA) {
			ImLog("ERR: DpSwitchStream() found streamZoneList but null wordSize...");
			ImDecBusyCount();
			return -1;
		}

		if (dispatch->jumpBuffer) {
			ImFreeFadeBuf(dispatch->jumpBuffer);
		}

		g_imDpSwitchBufSize = (int)((((uint32_t)dispatch->fmtB * fadeMs / 1000u) * (uint32_t)dispatch->fmtA *
									 (uint32_t)dispatch->fmtC) >>
									3);
		if ((unsigned int)g_imDpSwitchBufSize >= (unsigned int)dispatch->pendingJumpZones->byteCount) {
			g_imDpSwitchBufSize = dispatch->pendingJumpZones->byteCount;
		}

		frameBytes = dispatch->fmtC * (dispatch->fmtA >> 3);
		g_imDpSwitchBufSize -= (frameBytes - 1) & g_imDpSwitchBufSize;
		dispatch->jumpInProgress = 1;
		dispatch->jumpBuffer = ImAllocFadeBuf((unsigned int*)&g_imDpSwitchBufSize);
		if (dispatch->jumpBuffer) {
			dispatch->fadeReadOffset = 0;
			dispatch->fadeBytesAvailable = 0;
			dispatch->fadeBitsPerSample = dispatch->fmtA;
			dispatch->fadeSampleRate = dispatch->fmtB;
			dispatch->fadeChannels = dispatch->fmtC;
			dispatch->jumpFadeHoldMask = (int)(switchFlagsHigh | switchFlagsLow);
			dispatch->jumpFadeHoldFrames = 0;
			dispatch->crossfadeQ16Pos = 0x7f0000;
			dispatch->crossfadeQ16Step = 0;

			while ((unsigned int)dispatch->fadeBytesAvailable < (unsigned int)g_imDpSwitchBufSize) {
				if ((unsigned int)(g_imDpSwitchBufSize - dispatch->fadeBytesAvailable) >= 0x4000u) {
					chunkSize = 0x4000u;
				} else {
					chunkSize = (unsigned int)(g_imDpSwitchBufSize - dispatch->fadeBytesAvailable);
				}

				streamChunk = ImStreamGet(dispatch->stream, chunkSize);
				if ((uintptr_t)streamChunk < 0x10000u) {
					memset((char*)dispatch->jumpBuffer + dispatch->fadeBytesAvailable, 0,
						   (size_t)(g_imDpSwitchBufSize - dispatch->fadeBytesAvailable));
					dispatch->fadeBytesAvailable = g_imDpSwitchBufSize;
					break;
				}

				memcpy((char*)dispatch->jumpBuffer + dispatch->fadeBytesAvailable, streamChunk, chunkSize);
				dispatch->fadeBytesAvailable += (int)chunkSize;
			}
		} else {
			ImLog("WARNING:DpSwitchStream() couldn't alloc fade buf...");
		}
	}

	dispatch->jumpInProgress = 0;
	ImCancelFade(dispatch->track->soundId, -1);
	ImClearTriggerCore(dispatch->track->soundId, (char*)-1, -1);
	dispatch->track->soundId = newSoundId;
	streamFill = (unsigned int)ImGetStreamFill(dispatch->stream);
	ImStreamConsume(dispatch->stream, streamFill);

	if (switchFlagsHigh && dispatch->pendingJumpZones) {
		ImStreamSeek(dispatch->stream, newSoundId, dispatch->currentOffset);
		while (dispatch->pendingJumpZones->next) {
			dispatch->pendingJumpZones->next->inUse = 0;
			ImListRemove2((ImListNode**)&dispatch->pendingJumpZones->next,
						  (ImListNode*)dispatch->pendingJumpZones->next);
		}
		dispatch->pendingJumpZones->byteCount = 0;
		ImDecBusyCount();
		return 0;
	}

	ImStreamSeek(dispatch->stream, newSoundId, 0);
	while (dispatch->pendingJumpZones) {
		dispatch->pendingJumpZones->inUse = 0;
		ImListRemove2((ImListNode**)&dispatch->pendingJumpZones, (ImListNode*)dispatch->pendingJumpZones);
	}
	dispatch->map[0] = 0;
	dispatch->regionEndOffset = 0;
	dispatch->currentOffset = 0;
	result = ImNavigateMap(dispatch);
	if (!result || result == -3) {
		ImDecBusyCount();
		return 0;
	}

	ImLog("ERR: problem switching stream in dispatch...");
	ImFreeTrack(dispatch->track);
	ImDecBusyCount();
	return -1;
}

// FUNCTION: XWA 0x58E85A
void ImRenderPlayer(ImTrack* track, unsigned int frames, unsigned int outRate) {
	int fadeOutGain;
	unsigned int holdFramesToSkip;
	int16_t* sampleData;
	unsigned int fadeBytesToRender;
	unsigned int outputFrameOffset;
	unsigned int bufferedFadeFrames;
	int fadeInGain;
	int navResult;
	ImDispatch* dispatch;
	unsigned int sourceFramesToRender;
	unsigned int outputFramesToRender;
	unsigned int regionFramesRemaining;
	unsigned int streamBytesToRender;
	unsigned int requestedSourceFrames;

	dispatch = track->dispatch;
	if (dispatch->stream && dispatch->pendingJumpZones) {
		ImPredictStream(dispatch);
	}

	if (dispatch->jumpBuffer && !dispatch->jumpInProgress) {
		bufferedFadeFrames = ((unsigned int)dispatch->fadeBytesAvailable << 3) /
							 (unsigned int)(dispatch->fadeChannels * dispatch->fadeBitsPerSample);
		requestedSourceFrames =
			(unsigned int)(((track->pitch * dispatch->fadeSampleRate) >> 8) * frames) / outRate;
		if (bufferedFadeFrames < requestedSourceFrames) {
			sourceFramesToRender = bufferedFadeFrames;
			outputFramesToRender =
				outRate * bufferedFadeFrames / (unsigned int)((track->pitch * dispatch->fadeSampleRate) >> 8);
		} else {
			sourceFramesToRender = requestedSourceFrames;
			outputFramesToRender = frames;
		}

		if (sourceFramesToRender) {
			fadeBytesToRender =
				(unsigned int)(dispatch->fadeChannels * dispatch->fadeBitsPerSample * sourceFramesToRender) >>
				3;
			fadeInGain = ImGetFadeInGain(dispatch, (int)fadeBytesToRender);
			ImMixSource((int16_t*)((char*)dispatch->jumpBuffer + dispatch->fadeReadOffset),
						(int)sourceFramesToRender, dispatch->fadeBitsPerSample, dispatch->fadeChannels,
						dispatch->atRegionStart, outputFramesToRender, 0, fadeInGain, track->pan);
			dispatch->fadeReadOffset += (int)fadeBytesToRender;
			dispatch->fadeBytesAvailable -= (int)fadeBytesToRender;
			if (!dispatch->fadeBytesAvailable) {
				ImFreeFadeBuf(dispatch->jumpBuffer);
				dispatch->jumpBuffer = NULL;
			}
		} else {
			ImLog("WARNING:fade ends with incomplete frame (or odd 12-bit mono frame)...");
			ImFreeFadeBuf(dispatch->jumpBuffer);
			dispatch->jumpBuffer = NULL;
		}
	}

	outputFrameOffset = 0;
	while (g_imMusicJumpsEnabled) {
		if (!dispatch->regionEndOffset) {
			g_imNavEventReady = 0;
			navResult = ImNavigateMap(dispatch);
			if (navResult) {
				if (navResult == -1) {
					ImFreeTrack(track);
				}
				if (dispatch->jumpBuffer && dispatch->jumpFadeHoldMask) {
					dispatch->jumpFadeHoldFrames += (int)frames;
				}
				return;
			}

			if (g_imNavEventReady) {
				bufferedFadeFrames = ((unsigned int)dispatch->fadeBytesAvailable << 3) /
									 (unsigned int)(dispatch->fadeChannels * dispatch->fadeBitsPerSample);
				requestedSourceFrames =
					(unsigned int)(((track->pitch * dispatch->fadeSampleRate) >> 8) * frames) / outRate;
				if (bufferedFadeFrames < requestedSourceFrames) {
					sourceFramesToRender = bufferedFadeFrames;
					outputFramesToRender = outRate * bufferedFadeFrames /
										   (unsigned int)((track->pitch * dispatch->fadeSampleRate) >> 8);
				} else {
					sourceFramesToRender = requestedSourceFrames;
					outputFramesToRender = frames;
				}
				if (!sourceFramesToRender) {
					ImLog("WARNING:fade ends with incomplete frame (or odd 12-bit mono frame)...");
				}
				fadeBytesToRender = (unsigned int)(dispatch->fadeChannels * dispatch->fadeBitsPerSample *
												   sourceFramesToRender) >>
									3;
				fadeInGain = ImGetFadeInGain(dispatch, (int)fadeBytesToRender);
				ImMixSource((int16_t*)((char*)dispatch->jumpBuffer + dispatch->fadeReadOffset),
							(int)sourceFramesToRender, dispatch->fadeBitsPerSample, dispatch->fadeChannels,
							dispatch->atRegionStart, outputFramesToRender, (int)outputFrameOffset, fadeInGain,
							track->pan);
				dispatch->fadeReadOffset += (int)fadeBytesToRender;
				dispatch->fadeBytesAvailable -= (int)fadeBytesToRender;
				if (!dispatch->fadeBytesAvailable) {
					ImFreeFadeBuf(dispatch->jumpBuffer);
					dispatch->jumpBuffer = NULL;
				}
			}
		}

		if (!frames) {
			return;
		}

		regionFramesRemaining =
			((unsigned int)dispatch->regionEndOffset << 3) / (unsigned int)(dispatch->fmtC * dispatch->fmtA);
		requestedSourceFrames = (unsigned int)(((track->pitch * dispatch->fmtB) >> 8) * frames) / outRate;
		if (regionFramesRemaining < requestedSourceFrames) {
			sourceFramesToRender = regionFramesRemaining;
			outputFramesToRender =
				outRate * regionFramesRemaining / (unsigned int)((track->pitch * dispatch->fmtB) >> 8);
		} else {
			sourceFramesToRender = requestedSourceFrames;
			outputFramesToRender = frames;
		}

		if (!sourceFramesToRender) {
			ImFreeTrack(track);
			return;
		}

		streamBytesToRender = (unsigned int)(dispatch->fmtC * dispatch->fmtA * sourceFramesToRender) >> 3;
		if (dispatch->stream) {
			sampleData = (int16_t*)ImStreamGet(dispatch->stream, streamBytesToRender);
			if (!sampleData) {
				dispatch->streamStarved = 1;
				if (dispatch->jumpBuffer && dispatch->jumpFadeHoldMask) {
					dispatch->jumpFadeHoldFrames += (int)frames;
				}
				ImGetStreamStatus(dispatch->stream, &g_imStatStreamSize, &g_imStatStreamRefillThreshold,
								  &g_imStatStreamFill, &g_imStatStreamEof);
				return;
			}
			if (dispatch->pendingJumpZones) {
				dispatch->pendingJumpZones->streamOffset += (int)streamBytesToRender;
				dispatch->pendingJumpZones->byteCount -= (int)streamBytesToRender;
			}
			dispatch->streamStarved = 0;
		} else {
			sampleData = (int16_t*)ImResAddr((unsigned int)track->soundId);
			if (!sampleData) {
				ImLog("ERR: dispatch got NULL file addr... %d", track->soundId);
				return;
			}
			sampleData = (int16_t*)((char*)sampleData + dispatch->currentOffset);
		}

		if (dispatch->jumpBuffer && dispatch->jumpFadeHoldMask && dispatch->jumpFadeHoldFrames) {
			if ((unsigned int)dispatch->jumpFadeHoldFrames >= outputFramesToRender) {
				holdFramesToSkip = outputFramesToRender;
			} else {
				holdFramesToSkip = (unsigned int)dispatch->jumpFadeHoldFrames;
			}
			dispatch->jumpFadeHoldFrames -= (int)holdFramesToSkip;
			outputFramesToRender -= holdFramesToSkip;
			sourceFramesToRender =
				(unsigned int)(((track->pitch * dispatch->fmtB) >> 8) * outputFramesToRender) / outRate;
			sampleData =
				(int16_t*)((char*)sampleData + streamBytesToRender -
						   ((unsigned int)(dispatch->fmtC * dispatch->fmtA * sourceFramesToRender) >> 3));
		}

		if (dispatch->jumpBuffer) {
			fadeOutGain = ImGetFadeOutGain(dispatch);
		} else {
			fadeOutGain = track->effectiveVolume;
		}
		ImMixSource(sampleData, (int)sourceFramesToRender, dispatch->fmtA, dispatch->fmtC,
					dispatch->atRegionStart, outputFramesToRender, (int)outputFrameOffset, fadeOutGain,
					track->pan);
		outputFrameOffset += outputFramesToRender;
		frames -= outputFramesToRender;
		dispatch->currentOffset += (int)streamBytesToRender;
		dispatch->regionEndOffset -= (int)streamBytesToRender;
	}
}

// FUNCTION: XWA 0x58BFEC
void ImRenderFrame(void) {
	ImTrack* track;
	ImTrack* nextTrack;

	if (g_imEnginePaused) {
		++g_imEnginePaused;
		if (g_imEnginePaused >= 3) {
			g_imEnginePaused = 3;
		} else {
			return;
		}
	}

	do {
		ImServiceStreamJumps();
		ImServiceDSBuffer(g_imRenderWritePtr, &g_imRenderFrameLen, &g_imRenderRate);
		g_imRenderFramesPending = (int)g_imRenderFrameLen;
		if (g_imRenderFrameLen) {
			ImClearMixBuffer();
			if (!g_imEnginePaused) {
				track = g_imActivePlayers;
				while (track) {
					nextTrack = track->next;
					ImRenderPlayer(track, g_imRenderFrameLen, (unsigned int)g_imRenderRate);
					track = nextTrack;
				}
			}
			ImDownmixOutput((int16_t*)g_imRenderWritePtr[0], (int)g_imRenderFrameLen);
			ImServiceDSBuffer(g_imRenderWritePtr, &g_imRenderFrameLen, NULL);
		}
	} while (g_imRenderFramesPending);
}

// FUNCTION: XWA 0x58C352
int ImFeedSoundCore(int soundId, void* src, int len, int feedFlag) {
	ImTrack* track;

	track = g_imActivePlayers;
	ImIncBusyCount();
	while (track) {
		if (track->soundId && track->soundId == soundId && track->dispatch->stream) {
			ImDecBusyCount();
			return ImFeedStream(track->dispatch->stream, (char*)src, len, feedFlag);
		}
		track = track->next;
	}

	ImDecBusyCount();
	return -1;
}

// FUNCTION: XWA 0x586AF7
int ImProcessStreams(void) { return ImProcessStreamSwitches(); }

// FUNCTION: XWA 0x586B22
int ImFeedSound(int soundId, void* src, int len, int feedFlag) {
	return ImFeedSoundCore(soundId, src, len, feedFlag);
}

// FUNCTION: XWA 0x58C2D3
int ImQueryStreamCore(int soundId, int* outBufSize, int* outRefillThreshold, int* outFill, int* outEof) {
	ImTrack* track;

	track = g_imActivePlayers;
	ImIncBusyCount();
	while (track) {
		if (track->soundId && track->soundId == soundId && track->dispatch->stream) {
			ImGetStreamStatus(track->dispatch->stream, outBufSize, outRefillThreshold, outFill, outEof);
			ImDecBusyCount();
			return 0;
		}
		track = track->next;
	}

	ImDecBusyCount();
	return -1;
}

// FUNCTION: XWA 0x586B01
int ImQueryStream(int soundId, int* outBufSize, int* outRefillThreshold, int* outFill, int* outEof) {
	return ImQueryStreamCore(soundId, outBufSize, outRefillThreshold, outFill, outEof);
}

// FUNCTION: XWA 0x58C3CB
void ImFreeTrack(ImTrack* track) {
	ImListRemove2((ImListNode**)&g_imActivePlayers, (ImListNode*)track);
	ImDpFreeStream(track);
	ImCancelFade(track->soundId, -1);
	ImClearTriggerCore(track->soundId, (char*)-1, -1);
	track->soundId = 0;
}

// FUNCTION: XWA 0x58C930
ImTrack* ImAllocTrack(int priority) {
	int lowestPriority;
	{
		int i;
		ImTrack* track;

		track = g_imTrackPool;
		for (i = 0; i < g_imMaxTracks[0]; ++i) {
			if (!track->soundId) {
				return track;
			}
			++track;
		}

		ImLog("ERR: no spare tracks...");
		{
			ImTrack* stealTrack;

			lowestPriority = 127;
			stealTrack = NULL;
			track = g_imActivePlayers;
			while (track) {
				if (track->priority <= lowestPriority) {
					lowestPriority = track->priority;
					stealTrack = track;
				}
				track = track->next;
			}

			if (stealTrack && priority >= lowestPriority) {
				ImFreeTrack(stealTrack);
				return stealTrack;
			}
		}
		return NULL;
	}
}

// FUNCTION: XWA 0x58C240
int ImStopAllSoundsCore(void) {
	ImTrack* nextTrack;
	ImTrack* track;

	track = g_imActivePlayers;
	while (track) {
		nextTrack = track->next;
		ImFreeTrack(track);
		track = nextTrack;
	}
	return 0;
}

// FUNCTION: XWA 0x58C1E5
int ImStopSoundCore(int soundId) {
	struct {
		int result;
		ImTrack* next;
	} search;

	search.result = -1;
	{
		ImTrack* track;

		track = g_imActivePlayers;
		ImIncBusyCount();
		while (track) {
			search.next = track->next;
			if (track->soundId == soundId) {
				ImFreeTrack(track);
				search.result = 0;
			}
			track = search.next;
		}
	}
	ImDecBusyCount();
	return search.result;
}

// FUNCTION: XWA 0x58676F
int ImStopSound(int soundId) { return ImStopSoundCore(soundId); }

// FUNCTION: XWA 0x586780
int ImStopAllSounds(void) {
	int result;

	ImIncBusyCount();
	result = ImStopAllSoundsCore();
	ImDecBusyCount();
	return result;
}

// FUNCTION: XWA 0x58BE46
int ImSaveTracks(void* buf, unsigned int bufSize) {
	int savedSize;
	ImTrack* tracks;

	tracks = g_imTrackPool;
	ImIncBusyCount();
	savedSize = ImSaveDispatch(buf, bufSize);
	if (savedSize < 0) {
		ImDecBusyCount();
		return savedSize;
	}
	if (bufSize - (unsigned int)savedSize < sizeof(g_imTrackPool)) {
		ImDecBusyCount();
		return -5;
	}
	memcpy_0((char*)buf + savedSize, g_imTrackPool, sizeof(g_imTrackPool));
	savedSize += (int)sizeof(g_imTrackPool);
	ImDecBusyCount();
	return savedSize;
}

// FUNCTION: XWA 0x58BEC6
int ImRestoreTracks(char* src) {
	int restoredSize;
	ImTrack* track;

	track = g_imTrackPool;
	ImIncBusyCount();
	g_imActivePlayers = NULL;
	restoredSize = ImRestoreDispatch(src);
	memcpy_0(g_imTrackPool, src + restoredSize, sizeof(g_imTrackPool));
	restoredSize += (int)sizeof(g_imTrackPool);

	{
		int trackIdx;

		for (trackIdx = 0; trackIdx < g_imMaxTracks[0]; ++trackIdx, ++track) {
			track->prev = NULL;
			track->next = NULL;
			track->dispatch = ImGetDispatch(trackIdx);
			track->dispatch->track = track;
			if (track->soundId) {
				ImListAdd2((ImListNode**)&g_imActivePlayers, (ImListNode*)track);
			}
		}
	}

	ImRestoreDispatchStreams();
	ImDecBusyCount();
	return restoredSize;
}

// FUNCTION: XWA 0x5930A0
int ImListAdd2(ImListNode** head, ImListNode* node) {
	if (!node || node->prev || node->next) {
		ImLog("ERR: list arg err when adding...");
		return -5;
	}

	node->next = *head;
	if (*head) {
		(*head)->prev = node;
	}
	node->prev = NULL;
	*head = node;
	return 0;
}

// FUNCTION: XWA 0x593100
int ImListRemove2(ImListNode** head, ImListNode* node) {
	ImListNode* cur = *head;

	if (!node || !*head) {
		ImLog("ERR: list arg err when removing...");
		return -5;
	}

	while (cur) {
		if (cur == node) {
			break;
		}
		cur = cur->next;
	}
	if (!cur) {
		ImLog("ERR: item not on list...");
		return -3;
	}

	if (node->next) {
		node->next->prev = node->prev;
	}
	if (node->prev) {
		node->prev->next = node->next;
	} else {
		*head = node->next;
	}
	node->next = NULL;
	node->prev = NULL;
	return 0;
}
