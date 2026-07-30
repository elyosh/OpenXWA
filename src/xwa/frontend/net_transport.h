#ifndef XWA_FRONTEND_NET_TRANSPORT_H
#define XWA_FRONTEND_NET_TRANSPORT_H

#include "xwa/net/net_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NetPlayerInfo {
	char playerName[16];
	char sessionName[16];
	int32_t playerId;
	int32_t readyFlag;
} NetPlayerInfo;

typedef struct NetPlayerConnectionStats {
	int32_t playerId;
	uint32_t latencyTotalMs;
	int32_t packetCount;
	int32_t packetDropCount;
	int32_t packetRetryCount;
	uint32_t latencySampleCount;
} NetPlayerConnectionStats;

typedef struct NetDirectPlayRuntimeState {
	NetPlayerInfo localPlayer;
	uint32_t broadcastSeqCounter;
	NetPendingPayload broadcastPendingPayload;
	uint32_t groupSeqCounter;
	NetPendingPayload groupPendingPayload;
	uint8_t gap440[8];
	int32_t reliableRetryLongTimeoutMode;
	NetQueuedPacket recvQueue[1024];
	int32_t recvQueueWriteIndex;
	int32_t recvQueueReadIndex;
	int32_t recvQueueCount;
} NetDirectPlayRuntimeState;

typedef char xwa_net_player_info_size[(sizeof(NetPlayerInfo) == 0x28) ? 1 : -1];
typedef char xwa_net_player_connection_stats_size[(sizeof(NetPlayerConnectionStats) == 0x18) ? 1 : -1];
typedef char xwa_net_direct_play_runtime_recv_queue_offset
	[(offsetof(NetDirectPlayRuntimeState, recvQueue) == 0x44C) ? 1 : -1];

extern NetPlayerInfo g_netPlayers[32];
extern NetPlayerConnectionStats g_netPlayerConnectionStats[40];
extern void* g_netDirectPlayInterface;
extern NetDirectPlayRuntimeState g_netDirectPlayRuntimeState;
extern int g_netIsHost;
extern int g_netPlayerCount;
extern NetReliablePeerSlot g_netRuntimeReliablePeerSlots[40];
extern unsigned short g_networkPort;
extern uint32_t g_netSequenceCount;

int Net_PumpIncomingPackets(void);
int Net_SetNetworkPort(const unsigned short* port);
int Net_SetSerialPortSettings(const uint8_t* serialSettings);
int Net_StartNetworkSession(XwaGuid appGuid, const char* localPlayerInfo, const char* localPlayerName,
							int hostFlag, const char* sessionName, int networkType, int waitForPlayerCount,
							int unusedA11, const char* connectionAddress,
							const void* joinSessionInstanceGuid);
int Net_CountReadyPlayers(void);
NetPlayerInfo* Net_GetPlayerRoster(int* outCount);
unsigned int Net_GetAverageLatencyMs(int playerId);
int Net_GetPacketDropRateBasisPoints(int playerId);
int Net_GetPlayerPacketCount(int playerId);
int Net_GetPlayerPacketDropCount(int playerId);
int Net_GetPlayerPacketRetryCount(int playerId);
int Net_GetLocalPlayerId(void);
int Net_GetPlayerCount(void);
NetPlayerInfo* Net_FindPlayer(int playerId);
int Net_MarkPlayerReadyNoLock(int playerId);
int Net_SetPlayerReady(int playerId);
int Net_UpdateKeepaliveSequences(void);
int Net_SendSequenceKeepalives(void);
void Net_ShutdownDirectPlaySession(void);
void Net_ShutdownDirectPlaySessionForQuit(void);
int Net_IsHost(void);
int Net_HasQueuedPacketTypeOrBacklog(int packetType);
int Net_HasQueuedJoinRequestOrBacklog(void);
int Net_SendPacketAndFlush(int toPlayerId, const void* packet, unsigned int packetSize);
int Net_SendDirectPlayPacket(int destPlayerId, const void* packet, int packetSize, int flags);
int Net_SendSequencedDirectPlayPacket(int destPlayerId, int sequenceMode, int sequenceId, const void* packet,
									  unsigned int packetSize);
int Net_GetHostPlayerId(void);
int Net_ClearPlayerReadyFlagWithLockGuard(int playerId);
int Net_SetPlayerNameWithLockGuard(int playerId, const char* longName, const char* shortName);
int Net_ResetRosterToLocalPlayerWithLockGuard(void);
int NetSession_CompactReliablePeerSlotsForRoster(void);
unsigned int Net_AddSequence(int directPlayId);
int Net_CheckAndRecordIncomingSequence(int playerId, int sequenceId, int useChannel0, int useChannel2);

#ifdef __cplusplus
}
#endif

#endif
