#include "xwa/flight/flight_net.h"

#include "xwa/assets/string_table.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_debug.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_sync.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/net_session.h"
#include "xwa/flight/player/player.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/util/debug.h"
#include "xwa/util/time.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// GLOBAL: XWA 0x770E7C
int dpid;
// GLOBAL: XWA 0x80DA20
int g_playerAbortFlags[8];
// GLOBAL: XWA 0x76EA38
uint32_t g_flightNetScratchPacket[128];
// GLOBAL: XWA 0x76E820
uint32_t g_flightNetInputDeltaBatchPacket[128];
// GLOBAL: XWA 0x76EA2C
int g_flightNetInputDeltaBatchLen;
// GLOBAL: XWA 0x76E5D0
int g_flightNetLastInputBatchSendTime;
// GLOBAL: XWA 0x76EC58
int g_flightNetInputBatchIntervalTicks;
// GLOBAL: XWA 0x5FF3E4
const int g_flightNetSmallSessionPlayerThreshold = 3;
// GLOBAL: XWA 0x910680
int g_playerConnected[8];
// GLOBAL: XWA 0x80AD40
char g_playerTauntText[8][280];

#pragma pack(push, 1)
typedef struct FlightNetOptionsWire {
	uint32_t flightResolutionMode;
	uint32_t pilotRating;
	int32_t cockpitLookAvailable;
	int32_t cockpitToggleAvailable;
	uint32_t throttlePreset0;
	uint32_t laserPreset0;
	uint32_t shieldPreset0;
	uint32_t beamPreset0;
	uint32_t throttlePreset1;
	uint32_t laserPreset1;
	uint32_t shieldPreset1;
	uint32_t beamPreset1;
	uint32_t yawRollSwap;
} FlightNetOptionsWire;

typedef struct FlightNetOptionsPacket {
	uint32_t packetType;
	FlightNetOptionsWire options;
} FlightNetOptionsPacket;

typedef struct FlightNetRosterOptionsPacket {
	uint32_t packetType;
	uint32_t newNet;
	FlightNetOptionsWire options[8];
} FlightNetRosterOptionsPacket;

typedef struct FlightNetTauntPacket {
	uint32_t packetType;
	uint32_t playerIdx;
	char taunts[280];
} FlightNetTauntPacket;
#pragma pack(pop)

typedef char flight_net_options_wire_size[(sizeof(FlightNetOptionsWire) == 52) ? 1 : -1];
typedef char flight_net_options_packet_size[(sizeof(FlightNetOptionsPacket) == 56) ? 1 : -1];
typedef char flight_net_taunt_packet_size[(sizeof(FlightNetTauntPacket) == 288) ? 1 : -1];

// FLAGS: /O2
static __inline uint32_t FlightNet_PeekPacketDword(const uint8_t* packet) {
	uint32_t value;

	memcpy(&value, packet, sizeof(value));
	return value;
}
// FUNCTION: XWA 0x4EB010
char* FlightNet_GetStatusPlayerName(void) {
	int playerSlot;
	int statusDpid;

	statusDpid = dpid;
	if (statusDpid == 0) {
		statusDpid = NetSession_GetHostDplayId();
		dpid = statusDpid;
	}

	playerSlot = NetSession_FindPlayerSlotByDpid(statusDpid);
	if (g_playerAbortFlags[playerSlot]) {
		dpid = NetSession_GetHostDplayId();
		playerSlot = NetSession_FindPlayerSlotByDpid(NetSession_GetHostDplayId());
	}

	if (!g_players[playerSlot].connectedFlag) {
		dpid = NetSession_GetHostDplayId();
		playerSlot = NetSession_FindPlayerSlotByDpid(NetSession_GetHostDplayId());
	}

	return NetSession_GetPlayerName(playerSlot);
}

static uint32_t FlightNet_PresetThrottleToWire(uint8_t presetThrottle) {
	return 0xffffu * (uint32_t)presetThrottle / 100u;
}

static void FlightNet_CopyLocalOptionsToPlayer(PlayerData* player) {
	player->network.flightResolutionMode = (uint16_t)g_flightResolutionMode;
	player->pilotRating = (uint16_t)g_pilotData.pilotRating;
	player->throttlePreset[0] = (int16_t)FlightNet_PresetThrottleToWire(g_gameConfig.presetThrottle[0]);
	player->laserPreset[0] = g_gameConfig.presetLaser[0];
	player->shieldPreset[0] = g_gameConfig.presetShield[0];
	player->beamPreset[0] = g_gameConfig.presetBeam[0];
	player->throttlePreset[1] = (int16_t)FlightNet_PresetThrottleToWire(g_gameConfig.presetThrottle[1]);
	player->laserPreset[1] = g_gameConfig.presetLaser[1];
	player->shieldPreset[1] = g_gameConfig.presetShield[1];
	player->beamPreset[1] = g_gameConfig.presetBeam[1];
}

static void FlightNet_ResetHostPlayerOptions(void) {
	int playerIdx;

	for (playerIdx = 0; playerIdx < g_activeFlightPlayerCount; ++playerIdx) {
		PlayerData* player;

		player = &g_players[playerIdx];
		player->network.flightResolutionMode = 0;
		player->pilotRating = 0;
		player->throttlePreset[0] = 0x5555;
		player->laserPreset[0] = 0;
		player->shieldPreset[0] = 0;
		player->beamPreset[0] = 0;
		player->throttlePreset[1] = -1;
		player->laserPreset[1] = 2;
		player->shieldPreset[1] = 2;
		player->beamPreset[1] = 2;
		player->yawRollSwap = 0;
		if (playerIdx != g_localPlayer) {
			player->cockpitLookAvailable = 0;
			player->cockpitToggleAvailable = 0;
		}
	}
}

static void FlightNet_StoreOptionsFromWire(PlayerData* player, const FlightNetOptionsWire* options) {
	player->network.flightResolutionMode = (uint16_t)options->flightResolutionMode;
	player->pilotRating = (uint16_t)options->pilotRating;
	player->cockpitLookAvailable = (uint8_t)options->cockpitLookAvailable;
	player->cockpitToggleAvailable = (char)options->cockpitToggleAvailable;
	player->throttlePreset[0] = (int16_t)options->throttlePreset0;
	player->laserPreset[0] = (uint8_t)options->laserPreset0;
	player->shieldPreset[0] = (uint8_t)options->shieldPreset0;
	player->beamPreset[0] = (uint8_t)options->beamPreset0;
	player->throttlePreset[1] = (int16_t)options->throttlePreset1;
	player->laserPreset[1] = (uint8_t)options->laserPreset1;
	player->shieldPreset[1] = (uint8_t)options->shieldPreset1;
	player->beamPreset[1] = (uint8_t)options->beamPreset1;
	player->yawRollSwap = (uint8_t)options->yawRollSwap;
}

static void FlightNet_LoadOptionsWireFromPlayer(FlightNetOptionsWire* options, const PlayerData* player) {
	options->flightResolutionMode = player->network.flightResolutionMode;
	options->pilotRating = player->pilotRating;
	options->cockpitLookAvailable = (int8_t)player->cockpitLookAvailable;
	options->cockpitToggleAvailable = (int8_t)player->cockpitToggleAvailable;
	options->throttlePreset0 = (uint16_t)player->throttlePreset[0];
	options->laserPreset0 = player->laserPreset[0];
	options->shieldPreset0 = player->shieldPreset[0];
	options->beamPreset0 = player->beamPreset[0];
	options->throttlePreset1 = (uint16_t)player->throttlePreset[1];
	options->laserPreset1 = player->laserPreset[1];
	options->shieldPreset1 = player->shieldPreset[1];
	options->beamPreset1 = player->beamPreset[1];
	options->yawRollSwap = player->yawRollSwap;
}

static void FlightNet_FillLocalOptionsWire(FlightNetOptionsWire* options) {
	options->flightResolutionMode = (uint32_t)g_flightResolutionMode;
	options->pilotRating = (uint32_t)g_pilotData.pilotRating;
	options->cockpitLookAvailable = (int8_t)g_players[g_localPlayer].cockpitLookAvailable;
	options->cockpitToggleAvailable = (int8_t)g_players[g_localPlayer].cockpitToggleAvailable;
	options->throttlePreset0 = FlightNet_PresetThrottleToWire(g_gameConfig.presetThrottle[0]);
	options->laserPreset0 = g_gameConfig.presetLaser[0];
	options->shieldPreset0 = g_gameConfig.presetShield[0];
	options->beamPreset0 = g_gameConfig.presetBeam[0];
	options->throttlePreset1 = FlightNet_PresetThrottleToWire(g_gameConfig.presetThrottle[1]);
	options->laserPreset1 = g_gameConfig.presetLaser[1];
	options->shieldPreset1 = g_gameConfig.presetShield[1];
	options->beamPreset1 = g_gameConfig.presetBeam[1];
	options->yawRollSwap = 0;
}

static void FlightNet_DrawStillLoadingForSender(int senderDpid, int* stillLoadingAltText,
												uint32_t* lastStillLoadingUiTime, char* line1) {
	uint32_t nowMs;

	nowMs = timeGetTime();
	if ((int32_t)(nowMs - *lastStillLoadingUiTime) <= 200) {
		return;
	}

	*lastStillLoadingUiTime = nowMs;
	{
		int playerSlot;
		char* playerName;

		playerSlot = NetSession_FindPlayerSlotByDpid(senderDpid);
		playerName = NetSession_GetPlayerName(playerSlot);
		if (playerName != NULL) {
			strcpy(line1, playerName);
			strcat(line1, g_strDiskIoMessages[*stillLoadingAltText ? 35 : 34]);
			*stillLoadingAltText = !*stillLoadingAltText;
		} else {
			strcpy(line1, g_strDiskIoMessages[33]);
		}
	}

	FlightAlert_DrawBox(3, line1, NULL, 0x30u);
}

static void FlightNet_CopyRosterOptionsFromPacket(const FlightNetRosterOptionsPacket* packet) {
	int playerIdx;

	g_flightConfNewNet = (int)packet->newNet;
	for (playerIdx = 0; playerIdx < g_activeFlightPlayerCount; ++playerIdx) {
		FlightNet_StoreOptionsFromWire(&g_players[playerIdx], &packet->options[playerIdx]);
	}
}

static int FlightNet_SendLocalTauntsAndCollectAll(int* stillLoadingAltText, uint32_t* lastStillLoadingUiTime,
												  char* line1) {
	FlightNetTauntPacket tauntPacket;
	int receivedTauntCount;
	int senderDpid;
	int outAux;

	tauntPacket.packetType = 32;
	tauntPacket.playerIdx = (uint32_t)g_localPlayer;
	memcpy(tauntPacket.taunts, g_gameConfig.taunt1, sizeof(tauntPacket.taunts));
	memcpy(g_flightNetScratchPacket, &tauntPacket, sizeof(tauntPacket));
	NetSession_SendPacket(0, g_flightNetScratchPacket, sizeof(tauntPacket));

	receivedTauntCount = 0;
	NetSession_CountActivePlayers();
	while (receivedTauntCount < NetSession_CountActivePlayers()) {
		const FlightNetTauntPacket* packet;

		packet = (const FlightNetTauntPacket*)NetSession_WaitForGamePacket(&senderDpid, &outAux, 30);
		if (packet == NULL) {
			break;
		}

		if (packet->packetType == 29) {
			FlightNet_DrawStillLoadingForSender(senderDpid, stillLoadingAltText, lastStillLoadingUiTime,
												line1);
		} else if (packet->packetType == 32) {
			memcpy(g_playerTauntText[packet->playerIdx], packet->taunts, sizeof(tauntPacket.taunts));
			++receivedTauntCount;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x4EB090
int FlightNet_SyncPlayerOptionsAndTaunts(void) {
	int senderDpid;
	int outAux;
	int stillLoadingAltText;
	uint32_t lastStillLoadingUiTime;
	char line1[256];

	stillLoadingAltText = 1;
	lastStillLoadingUiTime = 0;
	senderDpid = NetSession_GetHostDplayId();

	if (g_activeFlightPlayerCount <= 1) {
		FlightNet_CopyLocalOptionsToPlayer(&g_players[0]);
		g_players[0].yawRollSwap = 0;
		memcpy(g_playerTauntText, g_gameConfig.taunt1, sizeof(g_gameConfig.taunt1) * 4u);
		return 1;
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		int receivedOptionsCount;
		int playerCount;
		uint32_t lastPacketTime;
		FlightNetRosterOptionsPacket rosterPacket;

		FlightNet_ResetHostPlayerOptions();
		NetSession_CountActivePlayers();
		FlightNet_CopyLocalOptionsToPlayer(&g_players[g_localPlayer]);
		if (g_activeFlightPlayerCount < XWA_PLAYER_COUNT) {
			g_players[g_activeFlightPlayerCount].yawRollSwap = 1;
		}

		FlightAlert_SaveBoxBackground();
		FlightAlert_DrawBox(1, g_strDiskIoMessages[32], NULL, 0x30u);

		receivedOptionsCount = 0;
		lastPacketTime = timeGetTime();
		while (receivedOptionsCount < NetSession_CountActivePlayers() - 1) {
			const FlightNetOptionsPacket* packet;
			uint32_t nowMs;

			packet = (const FlightNetOptionsPacket*)NetSession_WaitForGamePacket(&senderDpid, &outAux, 60);
			nowMs = timeGetTime();
			if (packet != NULL) {
				lastPacketTime = nowMs;
				if (packet->packetType == 29) {
					FlightNet_DrawStillLoadingForSender(senderDpid, &stillLoadingAltText,
														&lastStillLoadingUiTime, line1);
				}
				if (packet->packetType == 17) {
					int playerSlot;

					playerSlot = NetSession_FindPlayerSlotByDpid(senderDpid);
					FlightNet_StoreOptionsFromWire(&g_players[playerSlot], &packet->options);
					++receivedOptionsCount;
				}
			} else if (nowMs - lastPacketTime > 60000u) {
				return 0;
			}
		}

		FlightAlert_RestoreBoxBackground();

		playerCount = g_activeFlightPlayerCount;
		rosterPacket.packetType = 18;
		rosterPacket.newNet = (uint32_t)g_flightConfNewNet;
		for (senderDpid = 0; senderDpid < playerCount; ++senderDpid) {
			FlightNet_LoadOptionsWireFromPlayer(&rosterPacket.options[senderDpid], &g_players[senderDpid]);
		}
		memcpy(g_flightNetScratchPacket, &rosterPacket,
			   offsetof(FlightNetRosterOptionsPacket, options) +
				   (size_t)playerCount * sizeof(rosterPacket.options[0]));
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 52 * playerCount + 8);

		for (;;) {
			const FlightNetRosterOptionsPacket* packet;

			packet =
				(const FlightNetRosterOptionsPacket*)NetSession_WaitForGamePacket(&senderDpid, &outAux, 60);
			if (packet == NULL) {
				return 0;
			}
			if (packet->packetType == 18) {
				break;
			}
		}
	} else {
		FlightNetOptionsPacket optionsPacket;

		optionsPacket.packetType = 17;
		FlightNet_FillLocalOptionsWire(&optionsPacket.options);
		memcpy(g_flightNetScratchPacket, &optionsPacket, sizeof(optionsPacket));
		NetSession_SendPacket(senderDpid, g_flightNetScratchPacket, sizeof(optionsPacket));

		FlightAlert_SaveBoxBackground();
		FlightAlert_DrawBox(1, g_strDiskIoMessages[32], NULL, 0x30u);
		for (;;) {
			const FlightNetRosterOptionsPacket* packet;

			packet =
				(const FlightNetRosterOptionsPacket*)NetSession_WaitForGamePacket(&senderDpid, &outAux, 60);
			if (packet == NULL) {
				return 0;
			}
			if (packet->packetType == 29) {
				FlightNet_DrawStillLoadingForSender(senderDpid, &stillLoadingAltText, &lastStillLoadingUiTime,
													line1);
			}
			if (packet->packetType == 18) {
				FlightNet_CopyRosterOptionsFromPacket(packet);
				break;
			}
		}
	}

	{
		int playerIdx;

		for (playerIdx = 0; playerIdx < g_activeFlightPlayerCount; ++playerIdx) {
			g_players[playerIdx].cockpitVisible = g_players[playerIdx].cockpitLookAvailable != 0;
		}
	}

	FlightNet_SendLocalTauntsAndCollectAll(&stillLoadingAltText, &lastStillLoadingUiTime, line1);
	FlightAlert_RestoreBoxBackground();
	return 1;
}

// FUNCTION: XWA 0x4EBDD0
int FlightNet_BroadcastStillLoadingPulse(void) {
	g_flightNetScratchPacket[0] = 29;
	return NetSession_SendPacket(0, g_flightNetScratchPacket, 4);
}

// FUNCTION: XWA 0x4EBE10
int FlightNet_SendStillLoadingPulse(void) {
	g_flightNetScratchPacket[0] = 29;
	return NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 4);
}

// FUNCTION: XWA 0x4EBDF0
int FlightNet_BroadcastLocalPlayerLeft(void) {
	g_flightNetScratchPacket[0] = 8;
	return NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
}

// FUNCTION: XWA 0x4EBE30
int FlightNet_BroadcastPlayerDisconnected(int playerIdx) {
	int result;

	g_flightNetScratchPacket[0] = 3;
	g_flightNetScratchPacket[1] = (uint32_t)playerIdx;
	result = NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
	g_playerConnected[playerIdx] = 0;
	return result;
}

// FUNCTION: XWA 0x4EBE70
int FlightNet_BroadcastPlayerAbort(int playerIdx) {
	g_flightNetScratchPacket[0] = 21;
	g_flightNetScratchPacket[1] = (uint32_t)playerIdx;
	return NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
}

static int FlightNet_FillWorldChecksumPacket(uint32_t packetType, const int* worldChecksum,
											 const int* peerChecksum, int checksumDwordCount) {
	int checksumBytes;

	checksumBytes = 4 * checksumDwordCount;
	g_flightNetScratchPacket[0] = packetType;
	g_flightNetScratchPacket[1] = (uint32_t)g_serverTickTime;
	memcpy(&g_flightNetScratchPacket[2], worldChecksum, (size_t)checksumBytes);
	memcpy(&g_flightNetScratchPacket[2 + checksumDwordCount], peerChecksum, (size_t)checksumBytes);
	return 8 * checksumDwordCount + 8;
}

// FUNCTION: XWA 0x4EDB80
int FlightNet_SendWorldChecksumToLocalPlayer(const int* worldChecksum, const int* peerChecksum,
											 int checksumDwordCount) {
	int packetSize;

	packetSize = FlightNet_FillWorldChecksumPacket(4, worldChecksum, peerChecksum, checksumDwordCount);
	return NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, packetSize);
}

// FUNCTION: XWA 0x4EDC00
int FlightNet_BroadcastWorldChecksum(const int* worldChecksum, const int* peerChecksum,
									 int checksumDwordCount) {
	int packetSize;

	packetSize = FlightNet_FillWorldChecksumPacket(24, worldChecksum, peerChecksum, checksumDwordCount);
	return NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, packetSize);
}

// FUNCTION: XWA 0x4EBEA0
void FlightNet_MarkPilotNetworkPlayerLeft(int playerIdx) {
	int i;
	int directPlayId;

	i = 0;
	directPlayId = g_players[playerIdx].network.directPlayId;
	for (; i < 8; ++i) {
		if (g_pilotData.networkPlayers[i].directPlayId == directPlayId) {
			break;
		}
	}
	i &= 7;

	g_pilotData.networkPlayers[i].m60 = 1;
}

static void FlightNet_MarkLocalAbortAndPilotLeft(void) {
	int localPlayer;

	localPlayer = g_localPlayer;
	g_playerAbortFlags[localPlayer] = 1;
	g_flightMissionEndPending = 1;
	g_players[localPlayer].connectedFlag = 0;
	FlightNet_MarkPilotNetworkPlayerLeft(localPlayer);
}

static void FlightNet_SendPlayerAbortPacket(int directPlayId, int playerIdx, int broadcast) {
	g_flightNetScratchPacket[0] = 21;
	g_flightNetScratchPacket[1] = (uint32_t)playerIdx;
	if (broadcast) {
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
	} else {
		NetSession_SendPacket(directPlayId, g_flightNetScratchPacket, 8);
	}
}

static const uint8_t* FlightNet_DecodeRemoteInputFrame(int playerIdx, const uint8_t* cursor,
													   int* outTimestamp, FlightInputFrameRecord* input) {
	uint8_t code;
	uint8_t deltaCode;
	int timestamp;
	uint8_t highByte;
	uint8_t b0;
	uint8_t b1;
	uint8_t b2;

	memset(input, 0, sizeof(*input));

	code = *cursor++;
	deltaCode = code & 0x7f;
	if (deltaCode == 0x7f) {
		timestamp = (int)FlightNet_PeekPacketDword(cursor);
		cursor += 4;
	} else {
		int lastCode;

		lastCode = g_flightNetLastInputDeltaCodeByPlayer[playerIdx];
		if ((lastCode & 0x7f) > deltaCode) {
			lastCode += 128;
		}
		timestamp = (lastCode & 0xffffff80) | deltaCode;
	}

	if ((code & 0x80) != 0) {
		highByte = *cursor++;
	} else {
		highByte = 0;
	}

	g_flightNetLastInputDeltaCodeByPlayer[playerIdx] = timestamp;

	b0 = cursor[0];
	b1 = cursor[1];
	b2 = cursor[2];
	cursor += 3;

	input->key = (uint16_t)(highByte | ((uint16_t)(b2 & 1) << 8));
	input->axisX = (int8_t)(b0 & 0xfe);
	input->axisY = (int8_t)(b1 & 0xfe);
	input->axisR = (int8_t)(b2 & 0xfe);
	input->keyMods = (uint8_t)((b0 & 1) | ((b1 & 1) << 1));
	*outTimestamp = timestamp;
	return cursor;
}

static void FlightNet_InsertDecodedRemoteInput(int playerIdx, int timestamp,
											   const FlightInputFrameRecord* input) {
	InputFrame* inserted;

	inserted = FlightSync_InsertInputFrame(playerIdx, timestamp, input);
	if (inserted != NULL) {
		inserted->valid = 1;
		inserted->applied = NetSession_GetLocalPlayerId() != 0;
	}
}

static void FlightNet_HandleRemoteInputPacket(int senderDpid, const uint8_t* packet, int batched) {
	int playerIdx;

	playerIdx = NetSession_FindPlayerSlotByDpid(senderDpid);
	if (!g_players[playerIdx].connectedFlag) {
		FlightNet_SendPlayerAbortPacket(senderDpid, playerIdx, 0);
		return;
	}

	if (g_flightNetPeerSilenceTicks[playerIdx] > 0) {
		g_flightNetPeerSilenceTicks[playerIdx] = 0;
	}
	FlightSync_DiscardPredictedInputFrames(playerIdx);

	if (!batched) {
		FlightInputFrameRecord input;
		int timestamp;

		FlightNet_DecodeRemoteInputFrame(playerIdx, packet + 4, &timestamp, &input);
		FlightNet_InsertDecodedRemoteInput(playerIdx, timestamp, &input);
		return;
	}

	{
		int frameCount;
		const uint8_t* cursor;

		frameCount = packet[4];
		cursor = packet + 5;
		while (frameCount > 0) {
			FlightInputFrameRecord input;
			int timestamp;

			cursor = FlightNet_DecodeRemoteInputFrame(playerIdx, cursor, &timestamp, &input);
			FlightNet_InsertDecodedRemoteInput(playerIdx, timestamp, &input);
			--frameCount;
		}
	}
}

static void FlightNet_ResetPeerSilenceForPacket(int senderDpid) {
	if (senderDpid == NetSession_GetHostDplayId()) {
		g_flightNetHostTimeoutElapsedMs = 0;
	} else {
		int playerIdx;

		playerIdx = NetSession_FindPlayerSlotByDpid(senderDpid);
		if (g_players[playerIdx].connectedFlag && g_flightNetPeerSilenceTicks[playerIdx] > 0) {
			g_flightNetPeerSilenceTicks[playerIdx] = 0;
		}
	}
}

static void FlightNet_HandleAbortPacket(const int* packet) {
	unsigned int playerIdx;

	playerIdx = (unsigned int)packet[1];
	if (playerIdx < XWA_PLAYER_COUNT) {
		g_playerAbortFlags[playerIdx] = 1;
	}
	if (playerIdx == (unsigned int)g_localPlayer) {
		FlightNet_MarkLocalAbortAndPilotLeft();
	}
}

static void FlightNet_HandleClockProbePacket(int senderDpid, const int* packet) {
	int sample;
	int currentLead;

	g_flightNetScratchPacket[0] = 31;
	g_flightNetScratchPacket[1] = (uint32_t)packet[1];
	NetSession_SendPacket(senderDpid, g_flightNetScratchPacket, 8);

	sample = packet[2];
	if (!g_asyncFlag || g_activeFlightPlayerCount < g_flightNetSmallSessionPlayerThreshold) {
		sample >>= 1;
	}

	currentLead = g_flightNetClockLeadAllowanceMs;
	if (sample > currentLead) {
		int delta;

		delta = (sample - currentLead) >> 1;
		if (delta == 0) {
			delta = 1;
		}
		currentLead += delta;
	} else if (sample < currentLead) {
		int delta;

		delta = (currentLead - sample) >> 1;
		if (delta == 0) {
			delta = 1;
		}
		currentLead -= delta;
	}

	g_flightNetClockLeadAllowanceMs = currentLead;
	if (g_asyncFlag && g_flightNetClockLeadAllowanceMs < 130) {
		g_flightNetClockLeadAllowanceMs = 130;
	}
}

static void FlightNet_HandleClockProbeAckPacket(const int* packet) {
	if (NetSession_GetLocalPlayerId() == 0 && g_flightNetClockProbeTimestamp == packet[1]) {
		int sample;

		sample = g_inputTimestamp + g_flightNetClockAdjustAccumTicks + 20 - packet[1];
		if (sample < 472) {
			if (sample > g_flightNetClockLeadAllowanceMs) {
				int delta;

				delta = (sample - g_flightNetClockLeadAllowanceMs) >> 1;
				if (delta == 0) {
					delta = 1;
				}
				g_flightNetClockLeadAllowanceMs += delta;
			} else if (sample < g_flightNetClockLeadAllowanceMs) {
				int delta;

				delta = (g_flightNetClockLeadAllowanceMs - sample) >> 1;
				if (delta == 0) {
					delta = 1;
				}
				g_flightNetClockLeadAllowanceMs -= delta;
			}
		}
	}
}

static void FlightNet_PrepareResyncChunkAck(const int* packet) {
	const uint8_t* record;
	int dstOffset;

	record = (const uint8_t*)&packet[3];
	dstOffset = packet[3];
	while (dstOffset != -1) {
		int chunkSize;

		memcpy(&chunkSize, record + 4, sizeof(chunkSize));
		FlightSync_CopyWorldStateResyncChunk(record + 8, dstOffset, (uint32_t)chunkSize);
		record += 8 + chunkSize;
		memcpy(&dstOffset, record, sizeof(dstOffset));
	}

	g_flightNetScratchPacket[0] = 16;
	g_flightNetScratchPacket[1] = (uint32_t)packet[2];
}

static void FlightNet_HandleResyncChunkPacket(const int* packet) {
	FlightNet_PrepareResyncChunkAck(packet);
	NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 8);
}

static int FlightNet_RunClientWorldStateResyncWait(int packetSize) {
	int countdownState;

	countdownState = -1;
	while (1) {
		int hostDpid;

		hostDpid = NetSession_GetHostDplayId();
		NetSession_SendPacket(hostDpid, g_flightNetScratchPacket, packetSize);

		if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
			FlightNet_SendPlayerAbortPacket(0, g_localPlayer, 1);
			FlightNet_MarkLocalAbortAndPilotLeft();
			return 0;
		}

		while (1) {
			int senderDpid;
			int outAux;
			int* packet;
			int savedInputTimestamp;
			int timeoutBucket;

			savedInputTimestamp = g_inputTimestamp;
			g_inputTimestamp += (int)Time_GetFrameDelta();
			g_flightNetHostTimeoutElapsedMs += g_inputTimestamp - savedInputTimestamp;
			g_inputTimestamp = savedInputTimestamp;

			if (g_flightNetHostTimeoutElapsedMs > 7080) {
				FlightAlert_RestoreBoxBackground();
				return 0;
			}

			timeoutBucket = (7080 - g_flightNetHostTimeoutElapsedMs) / 118;
			if (timeoutBucket != countdownState) {
				countdownState = timeoutBucket;
				if (timeoutBucket < 50) {
					char line[80];

					sprintf(line, g_strDiskIoMessages[31], timeoutBucket / 2, 5 * (timeoutBucket & 1));
					FlightAlert_DrawBox(3, line, NULL, 0x30u);
				} else if ((timeoutBucket & 1) != 0) {
					FlightAlert_DrawBox(3, g_strDiskIoMessages[30], NULL, 0x30u);
				} else {
					FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x30u);
				}
			}

			packet = NetSession_ReceiveGamePacket(&senderDpid, &outAux);
			if (packet == NULL) {
				continue;
			}

			switch (packet[0]) {
				case 1:
					FlightNet_HandleRemoteInputPacket(senderDpid, (const uint8_t*)packet, 0);
					continue;
				case 2: {
					int savedTimestamp;

					savedTimestamp = g_inputTimestamp;
					FlightSync_ApplyWorldMessagePacket((uint8_t*)packet);
					Time_GetFrameDelta();
					g_inputTimestamp = savedTimestamp;
					if (g_flightMissionEndPending) {
						FlightAlert_RestoreBoxBackground();
						return 0;
					}
					continue;
				}
				case 8:
					g_flightMissionEndPending = 1;
					g_flightNetHostAbortReceived = 1;
					g_players[g_localPlayer].connectedFlag = 0;
					FlightAlert_RestoreBoxBackground();
					return 0;
				case 21:
					FlightNet_HandleAbortPacket(packet);
					if ((unsigned int)packet[1] == (unsigned int)g_localPlayer) {
						FlightAlert_RestoreBoxBackground();
						return 0;
					}
					continue;
				case 22:
					FlightNet_HandleRemoteInputPacket(senderDpid, (const uint8_t*)packet, 1);
					continue;
				case 23:
					dpid = packet[1];
					DebugPrintfChannel(
						0x20000, "HandleWorldPacket Received resync notification for playerid %d.\n", dpid);
					continue;
				case 29:
					FlightNet_ResetPeerSilenceForPacket(senderDpid);
					continue;
				case 61:
					FlightNet_SendPlayerAbortPacket(0, g_localPlayer, 1);
					FlightNet_MarkLocalAbortAndPilotLeft();
					FlightAlert_RestoreBoxBackground();
					return 0;
				case 62:
				case 63:
					if (packet[1] == (int)g_flightNetWorldChecksumEpoch) {
						if (packet[0] == 63) {
							FlightNet_PrepareResyncChunkAck(packet);
							packetSize = 8;
							break;
						}

						if (!g_playerAbortFlags[g_localPlayer]) {
							Time_GetFrameDelta();
							FlightSync_ReplayResyncMessages((unsigned int)packet[2], packet[1]);
							g_flightNetScratchPacket[0] = 27;
							NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 4);
							g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
							FlightAlert_RestoreBoxBackground();
							return 1;
						}

						FlightNet_MarkLocalAbortAndPilotLeft();
						FlightAlert_RestoreBoxBackground();
						return 0;
					}
					continue;
				default:
					continue;
			}

			break;
		}
	}
}

static int FlightNet_HandleWorldStateResyncPacket(int senderDpid, const int* packet) {
	(void)senderDpid;

	if (packet[1] != (int)g_flightNetWorldChecksumEpoch) {
		return 1;
	}

	if (packet[0] == 61) {
		int serializedWorldStateSize;
		int checksumBytes;

		g_inputTimestamp += (int)Time_GetFrameDelta();
		FlightAlert_SaveBoxBackground();
		FlightAlert_DrawBox(1, g_strDiskIoMessages[22], NULL, 0x30u);
		Flight_ApplyWorldStateObjectPresenceMap((const uint8_t*)packet + 8);

		g_flightNetScratchPacket[0] = 60;
		serializedWorldStateSize = Flight_GetSerializedWorldStateSize();
		checksumBytes = 4 * Flight_BuildWorldStateResyncSegmentChecksums(
								(int*)(g_flightNetScratchPacket + 1), Flight_GetDuplicateWorldStateBuffer(),
								serializedWorldStateSize);
		return FlightNet_RunClientWorldStateResyncWait(checksumBytes + 4);
	}

	if (packet[0] == 63) {
		FlightNet_HandleResyncChunkPacket(packet);
		return 1;
	}

	if (packet[0] == 62) {
		if (!g_playerAbortFlags[g_localPlayer]) {
			Time_GetFrameDelta();
			FlightSync_ReplayResyncMessages((unsigned int)packet[2], packet[1]);
			g_flightNetScratchPacket[0] = 27;
			NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 4);
			g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
			FlightAlert_RestoreBoxBackground();
		} else {
			FlightNet_MarkLocalAbortAndPilotLeft();
		}
	}
	return 1;
}

static int FlightNet_WriteWorldMessageFrame(uint8_t** cursor, InputFrame* frame, int packetTick) {
	uint8_t* out;
	int delta;
	uint8_t code;

	out = *cursor;
	delta = packetTick - frame->timestamp;
	if (delta < 0x10000) {
		if (delta < 368) {
			code = (uint8_t)(delta >= 125 ? 125 : delta);
		} else {
			code = 126;
		}
	} else {
		code = 127;
	}
	if (frame->input.key != 0) {
		code |= 0x80u;
	}
	*out++ = code;

	switch (code & 0x7f) {
		case 127:
			memcpy(out, &frame->timestamp, sizeof(frame->timestamp));
			out += 4;
			break;
		case 126: {
			uint16_t wordDelta;

			wordDelta = (uint16_t)(packetTick - (uint16_t)frame->timestamp);
			memcpy(out, &wordDelta, sizeof(wordDelta));
			out += 2;
			break;
		}
		case 125:
			*out++ = (uint8_t)(packetTick - (uint8_t)frame->timestamp - 125);
			break;
		default:
			break;
	}

	if ((code & 0x80u) != 0) {
		*out++ = (uint8_t)frame->input.key;
	}

	out[0] = (uint8_t)frame->input.axisX & 0xfeu;
	out[1] = (uint8_t)frame->input.axisY & 0xfeu;
	out[2] = (uint8_t)frame->input.axisR & 0xfeu;
	out[0] |= (uint8_t)(frame->input.keyMods & 1u);
	out[1] |= (uint8_t)((frame->input.keyMods >> 1) & 1u);
	out[2] |= (uint8_t)((frame->input.key >> 8) & 1u);
	out += 3;

	frame->applied = 0;
	delta = (int)(out - *cursor);
	*cursor = out;
	return delta;
}

static void FlightNet_MaybeSendWorldMessage(int currentTimestamp) {
	int adjustedTimestamp;
	int nextSendTimestamp;
	int elapsedSinceSend;
	int earliestAppliedFrame;
	int playerIdx;

	if (g_flightNetPendingAckCount || g_flightNetRecoveryUiActive) {
		return;
	}

	adjustedTimestamp = g_inputTimestamp + g_flightNetClockAdjustAccumTicks;
	nextSendTimestamp = g_flightNetNextClientInputSendTimestamp;
	if (nextSendTimestamp == 0) {
		nextSendTimestamp = adjustedTimestamp + (g_flightNetClockLeadAllowanceMs >> 3);
		g_flightNetNextClientInputSendTimestamp = nextSendTimestamp;
	}

	elapsedSinceSend = adjustedTimestamp - nextSendTimestamp;
	if (elapsedSinceSend < dtMs) {
		return;
	}

	if (elapsedSinceSend > 5 * dtMs) {
		g_flightNetNextClientInputSendTimestamp = nextSendTimestamp + dtMs;
	} else {
		earliestAppliedFrame = 0x7fffffff;
		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag) {
				InputFrame* lastFrame;

				lastFrame = FlightSync_FindLastNonzeroInputFrame(playerIdx);
				if (lastFrame == NULL) {
					earliestAppliedFrame = 0;
					break;
				}
				if (lastFrame->timestamp < earliestAppliedFrame) {
					earliestAppliedFrame = lastFrame->timestamp;
				}
			}
		}
		if (g_flightNetLastSentWorldMessageTimestamp + dtMs >= earliestAppliedFrame) {
			return;
		}
		g_flightNetNextClientInputSendTimestamp += dtMs;
	}

	++g_flightNetSentWorldMessageCount;
	if (g_flightPlayerCount > 1) {
		uint8_t* cursor;
		int packetLen;
		int packetTick;
		int* frameCountPtr;
		InputFrame* frameCursor;

		packetTick = g_flightNetLastSentWorldMessageTimestamp + dtMs;
		g_flightNetScratchPacket[0] = 2;
		g_flightNetScratchPacket[1] = (uint32_t)packetTick;
		g_flightNetLastSentWorldMessageTimestamp = packetTick;
		g_flightNetWorldChecksumResetAccumMs += packetTick - g_serverTickTime;
		if (g_flightNetWorldChecksumResetAccumMs > 472) {
			g_flightNetScratchPacket[1] = (uint32_t)packetTick | 0x80000000u;
			g_flightNetWorldChecksumResetAccumMs = 0;
			memset(g_flightNetWorldChecksumPeerStatus, 0, sizeof(g_flightNetWorldChecksumPeerStatus));
		}

		cursor = (uint8_t*)g_flightNetScratchPacket + 9;
		packetLen = 9;
		((uint8_t*)g_flightNetScratchPacket)[8] = 0;
		frameCursor = &g_inputHistory[0][0];
		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag) {
				int frameIdx;
				int count;

				++((uint8_t*)g_flightNetScratchPacket)[8];
				frameCountPtr = (int*)cursor;
				*cursor++ = 0;
				++packetLen;
				count = g_inputFrameCount[playerIdx];
				for (frameIdx = 0; frameIdx < count; ++frameIdx) {
					InputFrame* frame;

					frame = &g_inputHistory[playerIdx][frameIdx];
					if (frame->applied && frame->timestamp <= packetTick) {
						if ((int)(cursor - (uint8_t*)g_flightNetScratchPacket) >
							504 - g_activeFlightPlayerCount) {
							break;
						}
						++*(uint8_t*)frameCountPtr;
						packetLen += FlightNet_WriteWorldMessageFrame(&cursor, frame, packetTick);
					}
				}
			}
			frameCursor += XWA_INPUT_HISTORY_FRAME_COUNT;
			(void)frameCursor;
		}

		NetSession_SendPacket(0, g_flightNetScratchPacket, packetLen);
		if (g_inputLogEnabled == 1) {
			FILE* logFile;

			logFile = g_flightNetServerLogFile;
			if (logFile == NULL) {
				logFile = fopen("serverlog.txt", "w");
				g_flightNetServerLogFile = logFile;
			}
			if (logFile != NULL) {
				fprintf(logFile, "%8x\n", g_flightNetScratchPacket[1]);
				fflush(logFile);
			}
		}
	}

	(void)currentTimestamp;
}

// FUNCTION: XWA 0x4EBA20
int FlightNet_WaitForMissionStart(void) {
	int stillLoadingAltText;
	int activePlayerCount;
	int senderDpid;
	int recvAux;
	uint32_t lastStillLoadingUiTime;
	char line1[256];

	stillLoadingAltText = 1;
	memset(g_flightNetLastInputDeltaCodeByPlayer, 0, sizeof(g_flightNetLastInputDeltaCodeByPlayer));
	memset(g_flightNetPeerSilenceTicks, 0, sizeof(g_flightNetPeerSilenceTicks));
	lastStillLoadingUiTime = 0;
	g_lastFrameTime = 0;
	g_lastKeyframeTime = 0;
	dpid = 0;
	g_flightNetLastInputBatchSendTime = 0;
	g_flightNetInputDeltaBatchLen = 5;
	g_flightNetInputDeltaBatchPacket[0] = 22;
	g_flightNetInputDeltaBatchPacket[1] = 0;
	g_flightNetInputBatchIntervalTicks = 23;
	g_flightNetRecoveryUiActive = 0;
	g_flightNetPendingAckCount = 0;
	g_flightNetClockAdjustAccumTicks = 0;
	g_flightNetHostTimeoutElapsedMs = 0;

	if (g_activeFlightPlayerCount == 1) {
		g_serverTickTime = 0;
		g_flightNetClockLeadAllowanceMs = 30;
		g_gameTime = 0;
		g_inputTimestamp = 30;
		Time_GetFrameDelta();
		return 1;
	}

	activePlayerCount = NetSession_CountActivePlayers();
	{
		int hostDpid;

		hostDpid = NetSession_GetHostDplayId();
		g_flightNetScratchPacket[0] = 20;
		NetSession_SendPacket(hostDpid, g_flightNetScratchPacket, 4);
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		int readyPacketCount;

		readyPacketCount = 0;
		if (activePlayerCount > 0) {
			while (1) {
				int* readyPacket;

				readyPacket = NetSession_WaitForGamePacket(&senderDpid, &recvAux, 60);
				if (!readyPacket) {
					return 0;
				}
				if (readyPacket[0] == 20) {
					++readyPacketCount;
					if (readyPacketCount >= activePlayerCount) {
						break;
					}
				}
			}
		}

		g_flightNetScratchPacket[0] = 19;
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
	}

	FlightAlert_SaveBoxBackground();
	FlightAlert_DrawBox(1, g_strDiskIoMessages[32], NULL, 0x30u);
	while (1) {
		int* packet;

		packet = NetSession_WaitForGamePacket(&senderDpid, &recvAux, 60);
		if (!packet) {
			return 0;
		}

		if (packet[0] == 29) {
			uint32_t nowMs;

			nowMs = timeGetTime();
			if ((int)(nowMs - lastStillLoadingUiTime) > 200) {
				char* senderName;

				lastStillLoadingUiTime = nowMs;
				senderName = NetSession_GetPlayerName(NetSession_FindPlayerSlotByDpid(senderDpid));
				if (!senderName) {
					strcpy(line1, g_strDiskIoMessages[33]);
				} else {
					strcpy(line1, senderName);
					stillLoadingAltText = !stillLoadingAltText;
					strcat(line1, stillLoadingAltText ? g_strDiskIoMessages[34] : g_strDiskIoMessages[35]);
				}
				FlightAlert_DrawBox(3, line1, NULL, 0x30u);
			}
		}

		if (packet[0] == 19) {
			FlightAlert_RestoreBoxBackground();
			{
				int hostDpid;

				hostDpid = NetSession_GetHostDplayId();
				g_flightNetScratchPacket[0] = 27;
				NetSession_SendPacket(hostDpid, g_flightNetScratchPacket, 4);
			}
			Time_GetFrameDelta();
			g_serverTickTime = 0;
			g_gameTime = 0;
			g_inputTimestamp = 0;
			g_flightNetClockLeadAllowanceMs = g_asyncFlag != 0 ? 130 : 30;
			switch (g_gameConfig.serverUpdateRate) {
				case 4:
					dtMs = 59;
					break;
				case 6:
					dtMs = 39;
					break;
				case 8:
					dtMs = 29;
					break;
				default:
					dtMs = 29;
					break;
			}

			if (NetSession_GetLocalPlayerId() != 0) {
				int inputLead;

				g_flightNetPendingAckCount = (activePlayerCount != 1) + 1;
				g_flightNetNextClientInputSendTimestamp = 0;
				g_flightNetLastSentWorldMessageTimestamp = 0;
				g_unusedFlightNetMissionStartAckInitFlag = 1;
				while (g_flightNetPendingAckCount != 0 && (unsigned int)g_inputTimestamp < 100u) {
					FlightNet_ProcessIncomingPackets();
					g_inputTimestamp += (int)Time_GetFrameDelta();
				}
				g_flightNetPendingAckCount = 0;
				inputLead = g_inputTimestamp + (int)Time_GetFrameDelta();
				g_inputTimestamp = inputLead;
				g_flightNetClockLeadAllowanceMs = inputLead;
				if (inputLead < 35) {
					int adjustment;

					adjustment = 35 - inputLead;
					g_flightNetClockLeadAllowanceMs = 35;
					g_inputTimestamp += adjustment;
					g_flightNetClockAdjustAccumTicks -= adjustment;
				}
			}
			return 1;
		}
	}
}

// FUNCTION: XWA 0x4EBEE0
void FlightNet_ProcessIncomingPackets(void) {
	int startTimestamp;
	int currentTimestamp;
	int miscElapsed;
	int inputElapsed;
	int worldFrameElapsed;
	int worldMessageCount;

	if (!g_players[g_localPlayer].connectedFlag) {
		return;
	}

	inputElapsed = 0;
	miscElapsed = 0;
	worldFrameElapsed = 0;
	worldMessageCount = 0;

	if (g_flightPlayerCount == 1) {
		int senderDpid;
		int outAux;

		while (NetSession_ReceiveGamePacket(&senderDpid, &outAux) != NULL) {
		}
		return;
	}

	if (g_flightNetRecoveryUiActive) {
		currentTimestamp = g_flightNetRecoveryUiBlinkTime + 1 + (int)Time_GetFrameDelta();
		startTimestamp = g_flightNetRecoverySavedInputTimestamp - 826;
	} else {
		currentTimestamp = g_inputTimestamp + (int)Time_GetFrameDelta();
		startTimestamp = currentTimestamp;
	}

	while (1) {
		int* packet;
		int senderDpid;
		int outAux;

		if (currentTimestamp - startTimestamp > 826) {
			if (g_flightNetRecoveryUiActive) {
				if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
					g_flightNetRecoveryUiActive = 0;
					FlightAlert_RestoreBoxBackground();
					g_serverTickTime = g_gameTime;
					g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
					g_flightMissionEndPending = 1;
					g_players[g_localPlayer].connectedFlag = 0;
					FlightNet_SendPlayerAbortPacket(0, g_localPlayer, 1);
					if (NetSession_GetLocalPlayerId() != 0) {
						FlightNet_BroadcastLocalPlayerLeft();
					} else {
						FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
					}
					return;
				}
				if (currentTimestamp - g_flightNetRecoveryUiBlinkTime > 118) {
					int oldToggle;

					oldToggle = miscElapsed;
					g_flightNetRecoveryUiBlinkTime = currentTimestamp;
					miscElapsed = !miscElapsed;
					if (oldToggle) {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x34u);
					} else {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[30], NULL, 0x34u);
					}
				}
			} else {
				int hostDpid;
				int playerSlot;
				char* playerName;
				char line1[80];

				miscElapsed = 1;
				FlightAlert_SaveBoxBackground();
				strcpy(line1, g_strDiskIoMessages[24]);
				hostDpid = dpid != 0 ? dpid : NetSession_GetHostDplayId();
				dpid = hostDpid;
				playerSlot = NetSession_FindPlayerSlotByDpid(hostDpid);
				if (g_playerAbortFlags[playerSlot] || !g_players[playerSlot].connectedFlag) {
					dpid = NetSession_GetHostDplayId();
					playerSlot = NetSession_FindPlayerSlotByDpid(NetSession_GetHostDplayId());
				}
				playerName = NetSession_GetPlayerName(playerSlot);
				if (playerName != NULL) {
					strcat(line1, playerName);
				}
				FlightAlert_DrawBox(1, line1, NULL, 0x34u);
				g_flightNetRecoveryUiActive = 1;
				g_flightNetRecoveryUiBlinkTime = currentTimestamp;
				g_flightNetRecoverySavedInputTimestamp = currentTimestamp;
				if (NetSession_GetLocalPlayerId() == 0) {
					NetReliable_CompactLocalReceiveQueue();
					g_flightNetScratchPacket[0] = 3;
					g_flightNetScratchPacket[1] = (uint32_t)g_localPlayer;
					NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
					g_playerConnected[g_localPlayer] = 0;
				}
			}
		}

		currentTimestamp += (int)Time_GetFrameDelta();
		packet = NetSession_ReceiveGamePacket(&senderDpid, &outAux);
		{
			int frameDelta;

			frameDelta = (int)Time_GetFrameDelta();
			currentTimestamp += frameDelta;
			inputElapsed += frameDelta;
		}

		if (packet == NULL) {
			break;
		}

		switch (packet[0]) {
			case 1:
				FlightNet_HandleRemoteInputPacket(senderDpid, (const uint8_t*)packet, 0);
				worldFrameElapsed += (int)Time_GetFrameDelta();
				continue;
			case 2:
				g_flightNetHostTimeoutElapsedMs = 0;
				++g_flightNetReceivedWorldMessageCount;
				FlightSync_ApplyWorldMessagePacket((uint8_t*)packet);
				if (NetSession_GetLocalPlayerId() != 0) {
					int playerIdx;

					for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
						if (playerIdx != g_localPlayer && g_players[playerIdx].connectedFlag &&
							g_flightNetPeerSilenceTicks[playerIdx] != -1) {
							g_flightNetPeerSilenceTicks[playerIdx] += dtMs;
							if (g_flightNetPeerSilenceTicks[playerIdx] > 7080) {
								FlightNet_SendPlayerAbortPacket(0, playerIdx, 1);
								g_flightNetPeerSilenceTicks[playerIdx] = 0;
							}
						}
					}
				}
				if (g_flightMissionEndPending) {
					if (!g_flightNetRecoveryUiActive) {
						return;
					}
					goto restore_recovery_ui;
				}
				worldMessageCount++;
				worldFrameElapsed += (int)Time_GetFrameDelta();
				continue;
			case 3:
				if ((unsigned int)packet[1] < XWA_PLAYER_COUNT) {
					g_playerConnected[packet[1]] = 0;
				}
				continue;
			case 4:
				if (g_players[NetSession_FindPlayerSlotByDpid(senderDpid)].connectedFlag) {
					FlightSync_HandleWorldChecksumPacket(senderDpid, packet);
				}
				continue;
			case 8:
				g_flightMissionEndPending = 1;
				g_flightNetHostAbortReceived = 1;
				g_players[g_localPlayer].connectedFlag = 0;
				goto maybe_return_or_restore;
			case 16:
				g_flightNetWorldStateAckReceivedFlag = 1;
				if ((unsigned int)packet[1] < 16u) {
					g_flightNetWorldStateChunkAcked[packet[1]] = 1;
				}
				continue;
			case 21:
				FlightNet_HandleAbortPacket(packet);
				if ((unsigned int)packet[1] == (unsigned int)g_localPlayer) {
					goto maybe_return_or_restore;
				}
				continue;
			case 22:
				FlightNet_HandleRemoteInputPacket(senderDpid, (const uint8_t*)packet, 1);
				continue;
			case 23:
				dpid = packet[1];
				DebugPrintfChannel(
					0x20000, "ProcessIncomingNetworkPackets Received resync notification for playerid %d.\n",
					dpid);
				continue;
			case 24:
				FlightSync_HandleServerChecksumPacket((const uint32_t*)packet);
				g_flightNetClockProbeTimestamp = g_inputTimestamp + g_flightNetClockAdjustAccumTicks;
				g_flightNetScratchPacket[0] = 30;
				g_flightNetScratchPacket[1] = (uint32_t)g_flightNetClockProbeTimestamp;
				g_flightNetScratchPacket[2] = (uint32_t)g_flightNetClockLeadAllowanceMs;
				NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 12);
				continue;
			case 27:
				if (g_flightNetPendingAckCount != 0 && --g_flightNetPendingAckCount == 0) {
					g_flightNetNextClientInputSendTimestamp = 0;
					goto maybe_return_or_restore;
				}
				continue;
			case 28:
				g_flightNetClockLeadAllowanceMs = packet[1];
				continue;
			case 29:
				FlightNet_ResetPeerSilenceForPacket(senderDpid);
				continue;
			case 30:
				FlightNet_HandleClockProbePacket(senderDpid, packet);
				continue;
			case 31:
				FlightNet_HandleClockProbeAckPacket(packet);
				continue;
			case 60:
				g_flightNetRemoteResyncChecksumsReceivedFlag = 1;
				memcpy(g_flightNetRemoteResyncChecksums, packet + 1,
					   sizeof(g_flightNetRemoteResyncChecksums));
				continue;
			case 61:
			case 62:
			case 63:
				if (!FlightNet_HandleWorldStateResyncPacket(senderDpid, packet)) {
					goto maybe_return_or_restore;
				}
				if (g_flightMissionEndPending || !g_players[g_localPlayer].connectedFlag) {
					goto maybe_return_or_restore;
				}
				continue;
			default:
				continue;
		}
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		FlightNet_MaybeSendWorldMessage(currentTimestamp);
	}

	if (g_flightNetRecoveryUiActive) {
	restore_recovery_ui:
		g_flightNetRecoveryUiActive = 0;
		FlightAlert_RestoreBoxBackground();
		g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
		return;
	}

	g_inputTimestamp = currentTimestamp + (int)Time_GetFrameDelta();
	{
		char line1[80];

		sprintf(line1, "RcvMsg:%-2d In2Svr:%-2d SvrSnd:%-2d SvrFrm:%-2d NumFrm:%-2d AllPIN:%-2d Misc:%-2d\n",
				inputElapsed, worldFrameElapsed, 0, worldFrameElapsed, worldMessageCount,
				g_inputTimestamp - startTimestamp,
				g_inputTimestamp - inputElapsed - startTimestamp - worldFrameElapsed);
	}
	return;

maybe_return_or_restore:
	if (!g_flightNetRecoveryUiActive) {
		return;
	}
	goto restore_recovery_ui;
}

// FUNCTION: XWA 0x4EDC70
int FlightNet_SendWorldStateResyncApplyRequest(int directPlayId, int worldStateSize) {
	int retryCount;
	int stillLoadingElapsed;

	g_flightNetScratchPacket[1] = g_flightNetWorldChecksumEpoch;
	g_flightNetScratchPacket[0] = 62;
	g_flightNetScratchPacket[2] = (uint32_t)worldStateSize;
	g_flightNetScratchPacket[3] = (uint32_t)g_inputTimestamp;
	NetSession_SendPacket(directPlayId, g_flightNetScratchPacket, 16);
	Time_GetFrameDelta();

	retryCount = 10;
	stillLoadingElapsed = 0;
	do {
		int savedInputTimestamp;

		savedInputTimestamp = g_inputTimestamp;
		g_flightNetPendingAckCount = 1;
		while (g_flightNetPendingAckCount != 0 &&
			   (unsigned int)(g_inputTimestamp - savedInputTimestamp) < 236u) {
			if (FlightInput_HasKeyReady()) {
				uint16_t key;

				key = (uint16_t)FlightInput_GetNextKey();
				if (key == 27) {
					retryCount = 1;
					g_inputTimestamp = 236 + g_inputTimestamp;
					break;
				}
			}
			FlightNet_ProcessIncomingPackets();
			g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
			if (g_flightNetWorldStateAckReceivedFlag) {
				g_flightNetWorldStateAckReceivedFlag = 0;
				g_inputTimestamp = savedInputTimestamp;
			}
		}

		stillLoadingElapsed += g_inputTimestamp - savedInputTimestamp;
		if (stillLoadingElapsed >= 236) {
			stillLoadingElapsed = 0;
			g_flightNetScratchPacket[0] = 29;
			NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
		}
		if (!g_flightNetPendingAckCount) {
			break;
		}
		--retryCount;
	} while (retryCount != 0);

	if (g_flightNetPendingAckCount == 1) {
		int playerSlot;

		playerSlot = NetSession_FindPlayerSlotByDpid(directPlayId);
		g_flightNetScratchPacket[0] = 21;
		g_flightNetScratchPacket[1] = (uint32_t)playerSlot;
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
		g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
		g_flightNetPendingAckCount = 0;
		g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
	} else {
		g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
		g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
	}

	g_flightNetScratchPacket[0] = 23;
	g_flightNetScratchPacket[1] = 0;
	return NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
}

// FUNCTION: XWA 0x4EDE30
int FlightNet_SendWorldStateResyncToPlayer(int directPlayId, uint8_t* worldState, int worldStateSize) {
	int alertToggle;
	int stillLoadingElapsed;
	int retryCount;
	char line1[256];
	int chunkSlot;
	int segmentIndex;
	int worldOffset;
	int packetFreeBytes;
	int segmentCount;
	int segmentSize;
	uint8_t* payload;
	int result;

	alertToggle = 0;
	stillLoadingElapsed = 0;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	FlightAlert_SaveBoxBackground();

	strcpy(line1, g_strDiskIoMessages[23]);
	{
		char* playerName;

		playerName = NetSession_GetPlayerName(NetSession_FindPlayerSlotByDpid(directPlayId));
		if (playerName != NULL) {
			strcat(line1, playerName);
		}
	}
	FlightAlert_DrawBox(1, line1, NULL, 0x30u);

	g_flightNetScratchPacket[0] = 23;
	g_flightNetScratchPacket[1] = (uint32_t)directPlayId;
	NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);

	g_flightNetScratchPacket[0] = 61;
	g_flightNetScratchPacket[1] = g_flightNetWorldChecksumEpoch;
	result = Flight_BuildWorldStateObjectPresenceMap((uint8_t*)g_flightNetScratchPacket + 8, worldState);
	NetSession_SendPacket(directPlayId, g_flightNetScratchPacket, result + 8);

	g_flightNetPendingAckCount = 1;
	retryCount = 10;
	do {
		int elapsedThisPass;

		g_flightNetRemoteResyncChecksumsReceivedFlag = 0;
		elapsedThisPass = 0;
		while (elapsedThisPass < 236) {
			int savedInputTimestamp;
			int delta;

			if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
				elapsedThisPass = 236;
				retryCount = 1;
				break;
			}

			savedInputTimestamp = g_inputTimestamp;
			FlightNet_ProcessIncomingPackets();
			g_inputTimestamp += (int)Time_GetFrameDelta();
			delta = g_inputTimestamp - savedInputTimestamp;
			g_inputTimestamp = savedInputTimestamp;
			elapsedThisPass += delta;
			if (g_flightNetRemoteResyncChecksumsReceivedFlag) {
				break;
			}
		}

		stillLoadingElapsed += elapsedThisPass;
		if (stillLoadingElapsed >= 236) {
			stillLoadingElapsed = 0;
			g_flightNetScratchPacket[0] = 29;
			NetSession_SendPacket(directPlayId, g_flightNetScratchPacket, 4);
			NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
			alertToggle = !alertToggle;
			if (alertToggle) {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[29], NULL, 0x30u);
			} else {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x30u);
			}
		}
		if (g_flightNetRemoteResyncChecksumsReceivedFlag) {
			break;
		}
		--retryCount;
	} while (retryCount != 0 && !g_flightNetRemoteResyncChecksumsReceivedFlag);

	if (!g_flightNetRemoteResyncChecksumsReceivedFlag) {
		g_flightNetScratchPacket[0] = 21;
		g_flightNetScratchPacket[1] = (uint32_t)NetSession_FindPlayerSlotByDpid(directPlayId);
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
		FlightAlert_RestoreBoxBackground();
		Time_GetFrameDelta();
		g_flightNetPendingAckCount = 0;
		return 0;
	}

	segmentCount = Flight_BuildWorldStateResyncSegmentChecksums(g_flightNetLocalResyncChecksums, worldState,
																worldStateSize);
	memset(g_flightNetWorldStateChunkAcked, 0, sizeof(g_flightNetWorldStateChunkAcked));

	chunkSlot = 0;
	worldOffset = 0;
	segmentSize = Flight_ComputeWorldStateResyncSegmentSize(worldStateSize);
	packetFreeBytes = 492;
	g_flightNetWorldStateChunkPackets[0].packetType = 63;
	g_flightNetWorldStateChunkPackets[0].baseChecksum = g_flightNetWorldChecksumEpoch;
	g_flightNetWorldStateChunkPackets[0].chunkIndex = 0;
	payload = g_flightNetWorldStateChunkPackets[0].payload;

	for (segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
		if (g_flightNetLocalResyncChecksums[segmentIndex] == g_flightNetRemoteResyncChecksums[segmentIndex]) {
			worldOffset += segmentSize;
			continue;
		}

		{
			int remainingSegmentBytes;

			remainingSegmentBytes = segmentSize;
			if (worldOffset + remainingSegmentBytes > worldStateSize) {
				remainingSegmentBytes = worldStateSize - worldOffset;
				if (remainingSegmentBytes < 0) {
					remainingSegmentBytes = 0;
				}
			}

			while (remainingSegmentBytes != 0) {
				int recordBytes;
				int dataBytes;

				recordBytes = remainingSegmentBytes + 8;
				if (recordBytes > packetFreeBytes) {
					recordBytes = packetFreeBytes;
				}
				dataBytes = recordBytes - 8;
				memcpy(payload, &worldOffset, sizeof(worldOffset));
				memcpy(payload + 4, &dataBytes, sizeof(dataBytes));
				memcpy(payload + 8, &worldState[worldOffset], (size_t)dataBytes);
				payload += recordBytes;
				packetFreeBytes -= recordBytes;
				worldOffset += dataBytes;
				remainingSegmentBytes -= dataBytes;

				if (packetFreeBytes < 32) {
					int terminator;

					terminator = -1;
					memcpy(payload, &terminator, sizeof(terminator));
					NetSession_SendPacket(directPlayId,
										  (uint32_t*)&g_flightNetWorldStateChunkPackets[chunkSlot],
										  508 - packetFreeBytes);
					++chunkSlot;
					if (chunkSlot == 16) {
						if (!FlightNet_WaitForWorldStateChunkAcks(directPlayId, 16)) {
							FlightAlert_RestoreBoxBackground();
							Time_GetFrameDelta();
							g_flightNetPendingAckCount = 0;
							return 0;
						}
						g_flightNetScratchPacket[0] = 29;
						NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
						if (alertToggle) {
							FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x30u);
						} else {
							FlightAlert_DrawBox(3, g_strDiskIoMessages[29], NULL, 0x30u);
						}
						alertToggle = !alertToggle;
						chunkSlot = 0;
						memset(g_flightNetWorldStateChunkAcked, 0, sizeof(g_flightNetWorldStateChunkAcked));
					}

					packetFreeBytes = 492;
					g_flightNetWorldStateChunkPackets[chunkSlot].packetType = 63;
					g_flightNetWorldStateChunkPackets[chunkSlot].baseChecksum = g_flightNetWorldChecksumEpoch;
					g_flightNetWorldStateChunkPackets[chunkSlot].chunkIndex = (uint32_t)chunkSlot;
					payload = g_flightNetWorldStateChunkPackets[chunkSlot].payload;
				}
			}
		}
	}

	result = 1;
	if (packetFreeBytes < 496) {
		int terminator;

		terminator = -1;
		memcpy(payload, &terminator, sizeof(terminator));
		NetSession_SendPacket(directPlayId, (uint32_t*)&g_flightNetWorldStateChunkPackets[chunkSlot],
							  508 - packetFreeBytes);
		if (!FlightNet_WaitForWorldStateChunkAcks(directPlayId, chunkSlot + 1)) {
			result = 0;
		}
	}

	FlightAlert_RestoreBoxBackground();
	Time_GetFrameDelta();
	g_flightNetPendingAckCount = 0;
	return result;
}


// FUNCTION: XWA 0x4EE330
int FlightNet_WaitForWorldStateChunkAcks(int directPlayId, int chunkCount) {
	int alertToggle;
	int stillLoadingElapsed;
	int lastAckCount;
	int retryCountdown;

	alertToggle = 0;
	stillLoadingElapsed = 0;
	lastAckCount = 0;
	retryCountdown = 20;

	do {
		int elapsedThisPass;
		int ackCount;
		int escapePressed;

		elapsedThisPass = 0;
		ackCount = lastAckCount;
		escapePressed = 0;
		while (1) {
			int savedInputTimestamp;
			int delta;

			if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
				escapePressed = 1;
				break;
			}

			savedInputTimestamp = g_inputTimestamp;
			FlightNet_ProcessIncomingPackets();
			g_inputTimestamp += (int)Time_GetFrameDelta();
			delta = g_inputTimestamp - savedInputTimestamp;
			g_inputTimestamp = savedInputTimestamp;
			elapsedThisPass += delta;

			ackCount = 0;
			while (ackCount < chunkCount && g_flightNetWorldStateChunkAcked[ackCount] != 0) {
				++ackCount;
			}
			if (ackCount == chunkCount) {
				return 1;
			}
			if (elapsedThisPass >= 236) {
				break;
			}
		}

		if (escapePressed) {
			ackCount = lastAckCount;
			elapsedThisPass = 236;
			retryCountdown = 1;
		}

		if (ackCount == chunkCount) {
			break;
		}

		stillLoadingElapsed += elapsedThisPass;
		if (stillLoadingElapsed >= 236) {
			stillLoadingElapsed = 0;
			g_flightNetScratchPacket[0] = 29;
			NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
			alertToggle = !alertToggle;
			if (alertToggle) {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[29], NULL, 0x30u);
			} else {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[28], NULL, 0x30u);
			}
		}

		if (ackCount == lastAckCount) {
			--retryCountdown;
		} else {
			retryCountdown = 20;
		}
		lastAckCount = ackCount;
	} while (retryCountdown != 0);

	if (retryCountdown != 0) {
		return 1;
	}

	g_flightNetScratchPacket[0] = 21;
	g_flightNetScratchPacket[1] = (uint32_t)NetSession_FindPlayerSlotByDpid(directPlayId);
	NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
	return 0;
}

// FUNCTION: XWA 0x4ED7F0
int FlightNet_SampleAndSendInput(void) {
	int playerIdx;
	int packetLen;
	int deltaCode;
	int framePayloadLen;
	int validFrame;
	char axisRByte;
	FILE* logFile;
	uint8_t* packetBytes;
	uint8_t* batchBytes;
	InputFrame* inserted;

	FlightInput_Read(-2);

	g_currentInputFrame.key = g_actionKey;
	g_currentInputFrame.axisX = (int8_t)((uint8_t)g_ctrlAxisX & 0xfeu);
	g_currentInputFrame.axisY = (int8_t)((uint8_t)g_ctrlAxisY & 0xfeu);
	g_currentInputFrame.axisR = (int8_t)((uint8_t)g_ctrlAxisR & 0xfeu);
	g_currentInputFrame.keyMods = (uint8_t)g_keyMods & 3u;
	validFrame = 1;
	g_flightNetScratchPacket[0] = (uint32_t)validFrame;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	deltaCode = g_inputTimestamp - g_lastFrameTime;
	if (deltaCode >= 127 || deltaCode < 0 || g_lastFrameTime == 0) {
		deltaCode = 127;
	} else {
		deltaCode = g_inputTimestamp & 0x7f;
	}
	if (g_lastFrameTime - g_lastKeyframeTime > 236) {
		deltaCode = 127;
	}

	packetBytes = (uint8_t*)g_flightNetScratchPacket;
	if (deltaCode == 127) {
		g_lastKeyframeTime = g_inputTimestamp;
		packetBytes[4] = (uint8_t)deltaCode;
		memcpy(packetBytes + 5, &g_inputTimestamp, sizeof(g_inputTimestamp));
		packetLen = 9;
	} else {
		packetBytes[4] = (uint8_t)deltaCode;
		packetLen = 5;
	}

	if (g_currentInputFrame.key != 0) {
		packetBytes[4] = (uint8_t)(deltaCode | 0x80);
		packetBytes[packetLen] = (uint8_t)g_currentInputFrame.key;
		++packetLen;
	}
	g_lastFrameTime = g_inputTimestamp;

	packetBytes[packetLen] = (uint8_t)g_currentInputFrame.axisX;
	packetBytes[packetLen + 1] = (uint8_t)g_currentInputFrame.axisY;
	packetBytes[packetLen + 2] = (uint8_t)g_currentInputFrame.axisR;
	if ((g_currentInputFrame.keyMods & 1u) != 0) {
		packetBytes[packetLen] |= 1u;
	}
	if ((g_currentInputFrame.keyMods & 2u) != 0) {
		packetBytes[packetLen + 1] |= 1u;
	}
	axisRByte = packetBytes[packetLen + 2];
	axisRByte = (uint8_t)(axisRByte | (uint8_t)(g_currentInputFrame.key > 0xffu));
	packetLen += 3;
	packetBytes[packetLen - 1] = axisRByte;

	if (g_flightPlayerCount > validFrame) {
		if (g_inputLogEnabled == validFrame) {
			logFile = g_inputLogFile;
			if (logFile != NULL ||
				(logFile = fopen("inputlog.txt", "w"), g_inputLogFile = logFile, logFile != NULL)) {
				fprintf(logFile, "%8x %4x %2x %2x %2x\n", g_flightNetScratchPacket[1],
						g_currentInputFrame.key, (uint8_t)g_currentInputFrame.axisX,
						(uint8_t)g_currentInputFrame.axisY, g_currentInputFrame.keyMods);
				fflush(g_inputLogFile);
			}
		}
		if (!g_asyncFlag) {
			for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
				if (g_players[playerIdx].connectedFlag &&
					(g_playerConnected[playerIdx] != 0 || playerIdx == g_localPlayer)) {
					NetSession_SendPacket(g_players[playerIdx].network.directPlayId, g_flightNetScratchPacket,
										  packetLen);
				}
			}
		} else {
			batchBytes = (uint8_t*)g_flightNetInputDeltaBatchPacket;
			++batchBytes[4];
			g_flightNetInputDeltaBatchPacket[0] = 22;
			framePayloadLen = packetLen - 4;
			memcpy(batchBytes + g_flightNetInputDeltaBatchLen, packetBytes + 4, (size_t)framePayloadLen);
			packetLen += g_flightNetInputDeltaBatchLen - 4;
			g_flightNetInputDeltaBatchLen = packetLen;
			if ((uint32_t)(g_inputTimestamp - g_flightNetLastInputBatchSendTime) >
				(uint32_t)g_flightNetInputBatchIntervalTicks) {
				g_flightNetLastInputBatchSendTime = g_inputTimestamp;
				NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetInputDeltaBatchPacket,
									  packetLen);
				if (g_activeFlightPlayerCount < g_flightNetSmallSessionPlayerThreshold) {
					PlayerData* playerCursor;
					int* connectedCursor;

					playerIdx = 0;
					playerCursor = g_players;
					connectedCursor = g_playerConnected;
					do {
						int destPlayerId;

						if (playerCursor->connectedFlag && playerIdx != g_localPlayer) {
							destPlayerId = playerCursor->network.directPlayId;
							if (destPlayerId != NetSession_GetHostDplayId() && *connectedCursor != 0) {
								NetSession_SendPacket(destPlayerId, g_flightNetInputDeltaBatchPacket,
													  g_flightNetInputDeltaBatchLen);
							}
						}
						++connectedCursor;
						++playerIdx;
						++playerCursor;
					} while (connectedCursor < g_playerConnected + XWA_PLAYER_COUNT);
				}
				g_flightNetInputDeltaBatchLen = 5;
				g_flightNetInputDeltaBatchPacket[0] = 22;
				batchBytes[4] = 0;
			}
		}
	}

	inserted = FlightSync_InsertInputFrame(g_localPlayer, g_inputTimestamp, &g_currentInputFrame);
	if (inserted != NULL) {
		inserted->applied = 0;
		inserted->valid = validFrame;
#ifdef XWA_MODERN
		FlightDebug_CaptureJoystickInputSample(g_inputTimestamp);
#endif
	}

	return Flight_PumpWindowMessages();
}
