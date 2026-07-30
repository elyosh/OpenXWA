#include "xwa/flight/flight_sync.h"

#include "xwa/audio/sound.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight_net.h"
#include "xwa/flight/net_session.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/yard.h"
#include "xwa/math/fixed.h"
#include "xwa/render/renderer.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/time.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// FLAGS: /O2

// FUNCTION: XWA 0x4F81B0
void FlightSync_QueuePredictedRemoteInputFrames(int predictedFrameDelta) {
	int playerIdx;

	if (g_asyncFlag) {
		return;
	}

	for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
		int count;
		InputFrame* lastFrame;
		InputFrame* predictedFrame;
		FlightInputFrameRecord input;

		if (!g_players[playerIdx].connectedFlag || playerIdx == g_localPlayer) {
			continue;
		}

		count = g_inputFrameCount[playerIdx];
		if (count == 0) {
			continue;
		}

		lastFrame = &g_inputHistory[playerIdx][count - 1];
		memset(&input, 0, sizeof(input));
		input.axisX = lastFrame->input.axisX;
		input.axisY = lastFrame->input.axisY;
		input.axisR = lastFrame->input.axisR;

		predictedFrame =
			FlightSync_InsertInputFrame(playerIdx, lastFrame->timestamp + predictedFrameDelta, &input);
		if (predictedFrame != NULL) {
			predictedFrame->applied = 0;
			predictedFrame->valid = 2;
		}
	}
}

// FUNCTION: XWA 0x4F8260
__inline void FlightSync_DiscardPredictedInputFrames(int playerIdx) {
	int frameIdx;
	InputFrame* frame;
	InputFrame* frameBase;

	if (g_asyncFlag) {
		return;
	}
	if (!g_players[playerIdx].connectedFlag) {
		return;
	}
	if (playerIdx == g_localPlayer) {
		return;
	}

	frame = g_inputHistory[playerIdx];
	frameBase = frame;
	frameIdx = 0;
	while (frameIdx < g_inputFrameCount[playerIdx]) {
		if (frame->applied != 0 || frame->valid != 2) {
			++frame;
			++frameIdx;
			continue;
		}

		if (g_inputFrameCount[playerIdx] != 0 && frame >= frameBase) {
			int copyIdx;

			--g_inputFrameCount[playerIdx];
			copyIdx = 0;
			while (copyIdx < g_inputFrameCount[playerIdx]) {
				if (frameBase >= frame) {
					*frameBase = frameBase[1];
				}
				++copyIdx;
				++frameBase;
			}
			frameBase = g_inputHistory[playerIdx];
		}
	}
}

// FUNCTION: XWA 0x4F8350
InputFrame* FlightSync_InsertInputFrame(int playerIdx, int timestamp, const FlightInputFrameRecord* input) {
	int idx;
	int count;
	InputFrame* frame;
	InputFrame* arrayEnd;
	int existingTime;

	idx = 0;
	count = g_inputFrameCount[playerIdx];
	frame = g_inputHistory[playerIdx];
	arrayEnd = &frame[count];

	while (idx < count && frame->timestamp < timestamp) {
		++idx;
		++frame;
	}

	existingTime = frame->timestamp;
	if (existingTime > timestamp || idx == count) {
		if (count == XWA_INPUT_HISTORY_FRAME_COUNT) {
			return NULL;
		}
		g_inputFrameCount[playerIdx] = count + 1;
		if (arrayEnd > frame) {
			InputFrame* shiftDestination;

			shiftDestination = arrayEnd + 1;
			do {
				*--shiftDestination = *--arrayEnd;
			} while (arrayEnd > frame);
		}
	} else if (timestamp == existingTime) {
		if (!frame->valid) {
			return NULL;
		}
		if (frame->applied == 1) {
			return NULL;
		}
	}

	frame->timestamp = timestamp;
	frame->valid = 1;
	frame->applied = 0;
	frame->input = *input;
	return frame;
}

// FUNCTION: XWA 0x4F8410
InputFrame* FlightSync_FindLastNonzeroInputFrame(int playerIdx) {
	int remaining;
	InputFrame* frame;
	InputFrame* result;

	remaining = g_inputFrameCount[playerIdx];
	frame = g_inputHistory[playerIdx];
	result = NULL;
	while (remaining > 0) {
		if (frame->applied != 0) {
			result = frame;
		}
		++frame;
		--remaining;
	}

	return result;
}

// FUNCTION: XWA 0x4F8450
void FlightSync_ResetRemotePlayerRenderSmoothing(void) {
	int i;

	for (i = 0; i < 8; ++i) {
		g_remotePlayerRenderSamples[i].valid = 0;
		g_remotePlayerSavedRenderPoses[i].valid = 0;
	}
}

// FUNCTION: XWA 0x4F8470
void FlightSync_CaptureRemotePlayerRenderSamples(void) {
	int playerIdx;

	if (!g_remotePlayerRenderSmoothingEnabled) {
		return;
	}

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		PlayerData* player;
		RemotePlayerRenderSample* sample;
		int previousValid;
		ObjectRecord* obj;
		MobileObject* mobj;

		player = &g_players[playerIdx];
		sample = &g_remotePlayerRenderSamples[playerIdx];
		previousValid = sample->valid;
		sample->valid = 0;

		if (!player->connectedFlag || playerIdx == g_localPlayer || player->objectIndex == 0xffff) {
			continue;
		}

		obj = &g_objectTable[player->objectIndex];
		if (obj->objectType == OBJ_None) {
			continue;
		}

		mobj = obj->mobj;
		if (mobj == NULL) {
			continue;
		}

		if (!previousValid) {
			sample->roll = (int16_t)obj->roll;
			sample->pitch = (int16_t)obj->pitch;
			sample->yaw = (int16_t)obj->yaw;
		}

		sample->valid = 1;
		sample->objectSignature = obj->objectSignature;
		sample->worldX = obj->world_x;
		sample->worldY = obj->world_y;
		sample->worldZ = obj->world_z;
		sample->rollDelta = (int)obj->roll - (uint16_t)sample->roll;
		sample->pitchDelta = (int)obj->pitch - (uint16_t)sample->pitch;
		sample->yawDelta = (int)obj->yaw - (uint16_t)sample->yaw;
		sample->roll = (int16_t)obj->roll;
		sample->pitch = (int16_t)obj->pitch;
		sample->yaw = (int16_t)obj->yaw;

		if (mobj->moveVectorDirty) {
			FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
		}

		sample->moveX = mobj->moveX;
		sample->moveY = mobj->moveY;
		sample->moveZ = mobj->moveZ;
		sample->speedMagnitude = mobj->speed;
		sample->simStateTimestamp = mobj->simStateTimestamp;

		if (g_remotePlayerSavedRenderPoses[playerIdx].valid) {
			RemotePlayerRenderSample* saved;

			saved = &g_remotePlayerSavedRenderPoses[playerIdx];
			obj->roll = (uint16_t)saved->roll;
			obj->pitch = (uint16_t)saved->pitch;
			obj->yaw = (uint16_t)saved->yaw;
			obj->world_x = saved->worldX;
			obj->world_y = saved->worldY;
			obj->world_z = saved->worldZ;
		}
	}
}

// FUNCTION: XWA 0x4F8670
void FlightSync_ApplyRemotePlayerRenderSmoothing(void) {
	int playerIdx;
	PlayerData* player;
	int playerSlot;

	if (!g_remotePlayerRenderSmoothingEnabled) {
		return;
	}

	playerSlot = 0;
	player = g_players;
	playerIdx = 0;
	for (; (intptr_t)player < (intptr_t)&g_players[XWA_PLAYER_COUNT]; ++player, ++playerIdx, ++playerSlot) {
		ObjectRecord* obj;
		int sampleWorldX;
		int sampleWorldY;
		int sampleWorldZ;
		int elapsedTicks;
		int extrapolatedTicks;
		int deltaX;
		int deltaY;
		int deltaZ;
		int roughDistance;
		int blendFrac;
		int maxAngleStep;

		g_remotePlayerSavedRenderPoses[playerIdx].valid = 0;
		if (!player->connectedFlag || player->objectIndex == 0xffff || player->hasCheckpointFlag) {
			continue;
		}

		obj = &g_objectTable[player->objectIndex];
		if (obj->objectType == OBJ_None) {
			continue;
		}

		if (obj->mobj == NULL) {
			continue;
		}

		if (!g_remotePlayerRenderSamples[playerIdx].valid || playerSlot == g_localPlayer ||
			player->boundObjectSignature != g_remotePlayerRenderSamples[playerIdx].objectSignature) {
			continue;
		}

		sampleWorldX = g_remotePlayerRenderSamples[playerIdx].worldX;
		g_remotePlayerSavedRenderPoses[playerIdx].roll = (int16_t)obj->roll;
		g_remotePlayerSavedRenderPoses[playerIdx].pitch = (int16_t)obj->pitch;
		g_remotePlayerSavedRenderPoses[playerIdx].yaw = (int16_t)obj->yaw;
		g_remotePlayerSavedRenderPoses[playerIdx].worldX = obj->world_x;
		g_remotePlayerSavedRenderPoses[playerIdx].worldY = obj->world_y;
		sampleWorldZ = obj->world_z;
		sampleWorldY = g_remotePlayerRenderSamples[playerIdx].worldY;
		g_remotePlayerSavedRenderPoses[playerIdx].worldZ = sampleWorldZ;
		sampleWorldZ = g_remotePlayerRenderSamples[playerIdx].worldZ;
		g_remotePlayerSavedRenderPoses[playerIdx].valid = 1;

		elapsedTicks =
			obj->mobj->simStateTimestamp - g_remotePlayerRenderSamples[playerIdx].simStateTimestamp;
		if (elapsedTicks < 0) {
			continue;
		}

		extrapolatedTicks = 0;
		if (elapsedTicks > 0 && g_remotePlayerRenderSamples[playerIdx].speedMagnitude != 0) {
			int scaledSpeed;

			scaledSpeed = (4660 * (int)g_remotePlayerRenderSamples[playerIdx].speedMagnitude + 128) >> 8;
			extrapolatedTicks = (int)(int32_t)((uint32_t)elapsedTicks * (uint32_t)scaledSpeed) / 236;
			sampleWorldX += Xwa_Q15MulReuseFirstSlot((int)g_remotePlayerRenderSamples[playerIdx].moveX,
													 extrapolatedTicks);
			sampleWorldY += Xwa_Q15MulReuseFirstSlot((int)g_remotePlayerRenderSamples[playerIdx].moveY,
													 extrapolatedTicks);
			sampleWorldZ += Xwa_Q15MulReuseFirstSlot((int)g_remotePlayerRenderSamples[playerIdx].moveZ,
													 extrapolatedTicks);
		}

		deltaX = obj->world_x - sampleWorldX;
		deltaY = obj->world_y - sampleWorldY;
		deltaZ = obj->world_z - sampleWorldZ;
		roughDistance = collide_roughdistance3d(deltaX, deltaY, deltaZ);
		if (roughDistance <= 32 * extrapolatedTicks && roughDistance != 0) {
			blendFrac = (roughDistance << 14) / (32 * extrapolatedTicks);
		} else {
			blendFrac = 0x4000;
		}

		deltaX = Xwa_Q15MulReuseFirstSlot(blendFrac, deltaX);
		deltaY = Xwa_Q15MulReuseFirstSlot(blendFrac, deltaY);
		deltaZ = Xwa_Q15MulReuseFirstSlot(blendFrac, deltaZ);
		obj->world_x = sampleWorldX + deltaX;
		obj->world_y = sampleWorldY + deltaY;
		obj->world_z = sampleWorldZ + deltaZ;

		{
			int angleDiff;
			int signedAngleDiff;
			int angleCandidate;
			int angleCurrentDiff;
			int angleLimit;
			int16_t sampleAngle;

			sampleAngle = g_remotePlayerRenderSamples[playerIdx].roll;
			angleDiff = (int)(int16_t)(obj->roll - (uint16_t)sampleAngle);
			signedAngleDiff = angleDiff;
			if (g_remotePlayerRenderSamples[playerIdx].rollDelta > 0) {
				if (angleDiff < 0) {
					angleDiff = 0;
					obj->roll = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			} else if (g_remotePlayerRenderSamples[playerIdx].rollDelta < 0) {
				if (angleDiff > 0) {
					angleDiff = 0;
					obj->roll = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			}
			if (angleDiff < 0) {
				angleDiff = -angleDiff;
			}
			maxAngleStep = (6144 * elapsedTicks) / 236;
			if (angleDiff > maxAngleStep) {
				angleCandidate = (int)g_remotePlayerRenderSamples[playerIdx].roll + signedAngleDiff;
				angleLimit = maxAngleStep;
				angleLimit <<= 3;
				angleCurrentDiff = (int)(int16_t)obj->roll - angleCandidate;
				if (angleCurrentDiff < 0) {
					angleCurrentDiff = -angleCurrentDiff;
				}
				if (angleCurrentDiff < angleLimit) {
					obj->roll = (Q16Angle)angleCandidate;
				}
			}
		}

		{
			int angleDiff;
			int signedAngleDiff;
			int angleCandidate;
			int angleCurrentDiff;
			int angleLimit;
			int sampleDelta;
			int16_t sampleAngle;

			sampleAngle = g_remotePlayerRenderSamples[playerIdx].pitch;
			sampleDelta = g_remotePlayerRenderSamples[playerIdx].pitchDelta;
			angleDiff = (int)(int16_t)(obj->pitch - (uint16_t)sampleAngle);
			signedAngleDiff = angleDiff;
			if (sampleDelta > 0) {
				if (angleDiff < 0) {
					angleDiff = 0;
					obj->pitch = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			} else if (sampleDelta < 0) {
				if (angleDiff > 0) {
					angleDiff = 0;
					obj->pitch = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			}
			if (angleDiff < 0) {
				angleDiff = -angleDiff;
			}
			if (angleDiff > maxAngleStep) {
				angleCandidate = (int)g_remotePlayerRenderSamples[playerIdx].pitch + signedAngleDiff;
				angleLimit = maxAngleStep;
				angleLimit <<= 3;
				angleCurrentDiff = (int)(int16_t)obj->pitch - angleCandidate;
				if (angleCurrentDiff < 0) {
					angleCurrentDiff = -angleCurrentDiff;
				}
				if (angleCurrentDiff < angleLimit) {
					obj->pitch = (Q16Angle)angleCandidate;
				}
			}
		}

		{
			int angleDiff;
			int signedAngleDiff;
			int angleCandidate;
			int angleCurrentDiff;
			int angleLimit;
			int sampleDelta;
			int16_t sampleAngle;

			sampleAngle = g_remotePlayerRenderSamples[playerIdx].yaw;
			sampleDelta = g_remotePlayerRenderSamples[playerIdx].yawDelta;
			angleDiff = (int)(int16_t)(obj->yaw - (uint16_t)sampleAngle);
			signedAngleDiff = angleDiff;
			if (sampleDelta > 0) {
				if (angleDiff < 0) {
					angleDiff = 0;
					obj->yaw = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			} else if (sampleDelta < 0) {
				if (angleDiff > 0) {
					angleDiff = 0;
					obj->yaw = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			}
			if (angleDiff < 0) {
				angleDiff = -angleDiff;
			}
			if (angleDiff > maxAngleStep) {
				angleCandidate = (int)g_remotePlayerRenderSamples[playerIdx].yaw + signedAngleDiff;
				angleLimit = maxAngleStep;
				angleLimit <<= 3;
				angleCurrentDiff = (int)(int16_t)obj->yaw - angleCandidate;
				if (angleCurrentDiff < 0) {
					angleCurrentDiff = -angleCurrentDiff;
				}
				if (angleCurrentDiff < angleLimit) {
					obj->yaw = (Q16Angle)angleCandidate;
				}
			}
		}
	}
}

static __inline uint16_t FlightSync_ReadPacketWord(const uint8_t* packet) {
	uint16_t value;

	memcpy(&value, packet, sizeof(value));
	return value;
}

static __inline uint32_t FlightSync_PeekPacketDword(const uint8_t* packet) {
	uint32_t value;

	memcpy(&value, packet, sizeof(value));
	return value;
}

// FUNCTION: XWA 0x4F8A40
void FlightSync_ApplyWorldMessagePacket(uint8_t* packet) {
	uint32_t rawPacketTick;
	int packetTick;
	int checksumRequested;
	FlightInputFrameRecord input;

	if (NetSession_GetLocalPlayerId() == 0 && g_flightNetBufferWorldMessagesUntilChecksum == 1) {
		uint8_t* cursor;
		int packetLen;
		int connectedSections;

		cursor = packet + 8;
		packetLen = 9;
		connectedSections = *cursor++;
		while (connectedSections > 0) {
			int frameCount;

			frameCount = *cursor++;
			++packetLen;
			while (frameCount > 0) {
				int code;

				code = *cursor++;
				++packetLen;
				if ((code & 0x7f) == 0x7f) {
					cursor += 4;
					packetLen += 4;
				}
				if ((code & 0x80) != 0) {
					++cursor;
					++packetLen;
				}
				cursor += 3;
				packetLen += 3;
				--frameCount;
			}
			--connectedSections;
		}

		if (g_worldMessageBufferBytesFree < packetLen) {
			int oldHandle;
			int oldCapacity;
			int growBytes;

			oldHandle = g_worldMessageBufferHandle;
			oldCapacity = g_worldMessageBufferCapacity;
			growBytes = packetLen * 100;
			g_worldMessageBufferBytesFree += growBytes;
			g_worldMessageBufferCapacity += growBytes;

			g_worldMessageBufferHandle =
				Memory_AllocHandle("WORLDMESSAGEBUF", (size_t)g_worldMessageBufferCapacity);
			if (g_worldMessageBufferHandle == 0) {
				FeDiskIo_FatalError(0);
			}
			g_worldMessageBuffer = (uint8_t*)Memory_LockHandle(g_worldMessageBufferHandle);

			if (oldHandle != 0) {
				uint8_t* oldBuffer;

				oldBuffer = (uint8_t*)Memory_LockHandle(oldHandle);
				memcpy(g_worldMessageBuffer, oldBuffer, (size_t)oldCapacity);
				Memory_UnlockHandle(oldHandle);
				Memory_FreeHandle("WORLDMESSAGEBUF", oldHandle);
			}
		}

		memcpy(&g_worldMessageBuffer[g_worldMessageBufferCapacity - g_worldMessageBufferBytesFree], packet,
			   (size_t)packetLen);
		g_worldMessageBufferBytesFree -= packetLen;
		++g_worldMessageBufferedCount;
	}

	rawPacketTick = FlightSync_PeekPacketDword(packet + 4);
	packet += 8;
	packetTick = (int)rawPacketTick;
	checksumRequested = (int)(rawPacketTick & 0x80000000u);
	packetTick &= 0x7fffffff;
	if (packetTick <= g_serverTickTime) {
		return;
	}

	if (packetTick - g_serverTickTime != dtMs) {
		NetReliable_ResetRecvQueueState();
	}

	if (!g_asyncFlag) {
		int playerIdx;

		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			FlightSync_DiscardPredictedInputFrames(playerIdx);
		}
	}

	Flight_RestoreWorldState();
	g_gameTime = g_serverTickTime;

	if (g_flightNetDirtyAllObjectTransformsAfterRestore != 0) {
		uint32_t slot;

		for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
			if (g_objectTable[slot].objectType != OBJ_None && g_objectTable[slot].mobj != NULL) {
				g_objectTable[slot].mobj->moveVectorDirty = 1;
				g_objectTable[slot].mobj->orientMatrixDirty = 1;
			}
		}
		g_flightNetDirtyAllObjectTransformsAfterRestore = 0;
	}

	{
		uint8_t* cursor;
		int remainingSections;
		int playerIdx;

		cursor = packet;
		remainingSections = *cursor++;
		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			if (!g_players[playerIdx].connectedFlag) {
				continue;
			}
			if (remainingSections == 0) {
				break;
			}

			--remainingSections;
			{
				int frameCount;

				frameCount = *cursor++;
				while (frameCount > 0) {
					int code;
					int deltaCode;
					int timestamp;
					InputFrame* inserted;

					code = *cursor++;
					deltaCode = code & 0x7f;
					if (deltaCode == 0x7f) {
						timestamp = (int)FlightSync_PeekPacketDword(cursor);
						cursor += 4;
					} else if (deltaCode == 0x7e) {
						timestamp = packetTick - (int)FlightSync_ReadPacketWord(cursor);
						cursor += 2;
					} else if (deltaCode == 0x7d) {
						timestamp = packetTick - 0x7d;
						timestamp -= (int)*cursor;
						++cursor;
					} else {
						timestamp = packetTick - (int)deltaCode;
					}

#ifdef XWA_MODERN
					memset(&input, 0, sizeof(input));
#endif
					if ((code & 0x80) != 0) {
						input.key = *cursor++;
					} else {
						input.key = 0;
					}

					input.axisX = (int8_t)(cursor[0] & 0xfe);
					input.axisY = (int8_t)(cursor[1] & 0xfe);
					input.axisR = (int8_t)(cursor[2] & 0xfe);
					input.keyMods = (uint8_t)(cursor[1] & 1);
					input.keyMods <<= 1;
					input.keyMods |= (uint8_t)(cursor[0] & 1);
					input.key += (uint16_t)((cursor[2] & 1) << 8);
					cursor += 3;

					inserted = FlightSync_InsertInputFrame(playerIdx, timestamp, &input);
					if (inserted != NULL) {
						inserted->valid = 0;
						inserted->applied = 0;
					}
					--frameCount;
				}
			}
		}
	}

	g_flightSimSideEffectsSuppressed = 0;
	Flight_StepSimToTime(packetTick);
	if (g_provingGroundsModeActive) {
		Yard_UpdateChallengeTick(dtMs);
	}

	{
		int playerIdx;

		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag && playerIdx != g_localPlayer) {
				FlightView_UpdatePlayerCamera(playerIdx);
			}
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);

	g_gameTime = packetTick;
	g_serverTickTime = packetTick;
#ifndef XWA_MODERN
	Sound_FlushQueuedEffects();
#endif
	Flight_SaveWorldState();

	if (checksumRequested) {
		Flight_ChecksumWorldState(0, 0);
		g_flightNetWorldChecksumEpoch = (uint32_t)g_serverTickTime;
		if (NetSession_GetLocalPlayerId() != 0) {
			FlightNet_BroadcastWorldChecksum((const int*)g_worldChecksum, (const int*)peerChecksum, 16);
		}
		FlightNet_SendWorldChecksumToLocalPlayer((const int*)g_worldChecksum, (const int*)peerChecksum, 16);

		g_flightNetBufferWorldMessagesUntilChecksum = 1;
		memcpy(g_worldStateDupBuffer, g_worldStateBuffer, (size_t)g_worldStateSize);
		worldStateSize = g_worldStateSize;
		g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
		g_worldMessageBufferedCount = 0;
	}
}

// FUNCTION: XWA 0x4F8F90
void FlightSync_HandleWorldChecksumPacket(int senderDpid, const int* packet) {
	int playerSlot;
	int mismatch;

	if (packet[1] != (int)g_flightNetWorldChecksumEpoch) {
		return;
	}

	playerSlot = NetSession_FindPlayerSlotByDpid(senderDpid);
	if (g_playerAbortFlags[playerSlot]) {
		return;
	}
	if (!g_players[playerSlot].connectedFlag) {
		return;
	}

	mismatch = 0;
	if (playerSlot != g_localPlayer) {
		int checksumIndex;
		int packetPeerChecksumSum;
		int localPeerChecksumSum;

		packetPeerChecksumSum = 0;
		localPeerChecksumSum = 0;
		for (checksumIndex = 0; checksumIndex < 16; ++checksumIndex) {
			int packetChecksum;

			packetPeerChecksumSum += packet[18 + checksumIndex];
			localPeerChecksumSum += (int)peerChecksum[checksumIndex];
			packetChecksum = packet[2 + checksumIndex];
			if (packetChecksum != (int)g_worldChecksum[checksumIndex]) {
				DebugPrintfChannel(
					0x20000, "Player %d checksum[%d] (%lx) is different from world checksum[%d] (%lx).\n",
					playerSlot, checksumIndex, (unsigned long)packetChecksum, checksumIndex,
					(unsigned long)g_worldChecksum[checksumIndex]);
				mismatch = 1;
			}
		}
		(void)packetPeerChecksumSum;
		(void)localPeerChecksumSum;

		if (mismatch) {
			if (FlightNet_SendWorldStateResyncToPlayer(senderDpid, g_worldStateDupBuffer, worldStateSize)) {
				FlightNet_SendWorldStateResyncApplyRequest(senderDpid, worldStateSize);
			}
			mismatch = 1;
		}
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		int playerIdx;
		int aggregateStatus;

		g_flightNetWorldChecksumPeerStatus[playerSlot] = mismatch == 1 ? 2 : 1;
		aggregateStatus = 3;
		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag) {
				aggregateStatus &= g_flightNetWorldChecksumPeerStatus[playerIdx];
			}
		}
		if ((aggregateStatus & 1) != 0) {
			g_flightNetBufferWorldMessagesUntilChecksum = 0;
		}
	}
}

// FUNCTION: XWA 0x4F9110
void FlightSync_HandleServerChecksumPacket(const uint32_t* packet) {
	int i;
	int mismatch;

	if (NetSession_GetLocalPlayerId() != 0) {
		return;
	}
	if (packet[1] != g_flightNetWorldChecksumEpoch) {
		return;
	}

	packet += 2;
	mismatch = 0;
	for (i = 0; i < 16; ++i) {
		if (packet[i] != g_worldChecksum[i]) {
			DebugPrintfChannel(0x20000,
							   "Server checksum[%d] (%lx) is different from world checksum[%d] (%lx).\n", i,
							   (unsigned long)packet[i], i, (unsigned long)g_worldChecksum[i]);
			mismatch = 1;
		}
	}

	if (!mismatch) {
		g_flightNetBufferWorldMessagesUntilChecksum = 0;
		g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
		g_worldMessageBufferedCount = 0;
	}
}

// FUNCTION: XWA 0x4F9190
void FlightSync_CopyWorldStateResyncChunk(const void* chunkData, int dstOffset, uint32_t chunkSize) {
	memcpy(&g_worldStateDupBuffer[dstOffset], chunkData, chunkSize);
}

// FUNCTION: XWA 0x4F91C0
void FlightSync_ReplayResyncMessages(unsigned int savedWorldStateSize, int serverTickTime) {
	int packetOffset;

	worldStateSize = (int)savedWorldStateSize;
	g_flightNetDirtyAllObjectTransformsAfterRestore = 1;
	memcpy(g_worldStateBuffer, g_worldStateDupBuffer, (size_t)savedWorldStateSize);
	g_worldStateSize = worldStateSize;
	g_serverTickTime = serverTickTime;

	Flight_ChecksumWorldState(0, 0);
	FlightNet_SendWorldChecksumToLocalPlayer((const int*)g_worldChecksum, (const int*)peerChecksum, 16);
	memcpy(g_worldStateDupBuffer, g_worldStateBuffer, (size_t)g_worldStateSize);
	worldStateSize = g_worldStateSize;
	g_flightNetBufferWorldMessagesUntilChecksum = 0;

	packetOffset = 0;
	while (g_worldMessageBufferedCount != 0) {
		uint8_t* packet;
		uint8_t* cursor;
		int connectedSections;
		uint32_t packetTick;

		--g_worldMessageBufferedCount;
		packet = g_worldMessageBuffer + packetOffset;
		packetOffset += 9;
		cursor = packet + 9;

		connectedSections = packet[8];
		while (connectedSections > 0) {
			int frameCount;

			frameCount = *cursor++;
			++packetOffset;
			while (frameCount > 0) {
				uint8_t code;

				code = *cursor++;
				++packetOffset;
				if ((code & 0x7f) == 0x7f) {
					cursor += 4;
					packetOffset += 4;
				}
				if ((code & 0x80) != 0) {
					++cursor;
					++packetOffset;
				}
				cursor += 3;
				packetOffset += 3;
				--frameCount;
			}
			--connectedSections;
		}

		packetTick = FlightSync_PeekPacketDword(packet + 4) & 0x7fffffffu;
		memcpy(packet + 4, &packetTick, sizeof(packetTick));
		FlightSync_ApplyWorldMessagePacket(packet);
	}

	g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
}

// FUNCTION: XWA 0x4F9300
void FlightSync_ResetWorldMessageBufferCursor(void) {
	g_worldMessageBufferedCount = 0;
	g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
}
