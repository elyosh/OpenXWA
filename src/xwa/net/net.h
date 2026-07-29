#ifndef XWA_NET_NET_H
#define XWA_NET_NET_H

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

typedef char xwa_net_player_info_size[(sizeof(NetPlayerInfo) == 0x28) ? 1 : -1];

typedef struct SessionPlayerInfo {
	char sessionName[16];
	char playerName[16];
	int32_t dplayId;
	int32_t activeFlag;
} SessionPlayerInfo;

typedef char xwa_session_player_info_size[(sizeof(SessionPlayerInfo) == 0x28) ? 1 : -1];

typedef struct NetPlayerConnectionStats {
	int32_t playerId;
	uint32_t latencyTotalMs;
	int32_t packetCount;
	int32_t packetDropCount;
	int32_t packetRetryCount;
	uint32_t latencySampleCount;
} NetPlayerConnectionStats;

typedef char xwa_net_player_connection_stats_size[(sizeof(NetPlayerConnectionStats) == 0x18) ? 1 : -1];

typedef struct XwaGuid {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t Data4[8];
} XwaGuid;

typedef struct NetPendingPayload {
	uint8_t payload[512];
	int32_t payloadLength;
	int32_t pendingFlush;
} NetPendingPayload;

typedef struct NetQueuedPacket {
	int32_t directPlayId;
	int32_t payloadSize;
	int32_t aux;
	uint8_t meta0;
	uint8_t packetClass;
	uint8_t sequenceByte;
	uint8_t queuedFlag;
	uint8_t payload[512];
} NetQueuedPacket;

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

typedef struct NetReliablePeerSlot {
	int32_t directPlayId;
	int32_t prevRecvSeqDefault;
	int32_t recvSeqDefault;
	int32_t prevRecvSeqChannelA;
	int32_t recvSeqChannelA;
	int32_t prevRecvSeqChannelB;
	int32_t recvSeqChannelB;
	int32_t sendSeq;
	uint8_t lastPiggybackType;
	uint8_t piggybackPayload[511];
	int32_t piggybackLength;
	uint32_t lastActivityMs;
	int32_t packetCount;
	int32_t packetDropCount;
	int32_t packetRetryCount;
	uint32_t lastKeepaliveMs;
} NetReliablePeerSlot;

typedef struct NetSessionState {
	int32_t field_00;
	void* dplayInterface;
	int32_t field_08;
	const char* networkType;
	XwaGuid appGuid;
	XwaGuid sessionInstanceGuid;
	int32_t groupDplayId;
	int32_t localPlayerId;
	int32_t hostDplayId;
	int32_t playerCount;
	int32_t unusedReceivePollFlag;
	uint8_t gap44[32];
	int32_t reliableUseFixedResendTimeouts;
	char sessionName[16];
	char localPlayerName[16];
	int32_t localDplayId;
	int32_t unusedLocalDplayIdMirror;
	SessionPlayerInfo players[8];
	uint8_t gap1D0[32];
	int32_t playersEndDirectPlayIdSentinel;
	int32_t playersActiveFlagEndSentinel;
	uint8_t gap1F8[280];
	int32_t broadcastSeqCounter;
	NetPendingPayload broadcastPendingPayload;
	int32_t groupSeqCounter;
	NetPendingPayload groupPendingPayload;
	NetReliablePeerSlot reliablePeerSlots[40];
	uint8_t gap5FE8[12];
	int32_t reliablePeerPrevRecvSeqChannelAEndSentinel;
	uint8_t gap5FF8[552];
	uint32_t netSlotCount;
	int32_t field_6224;
	NetQueuedPacket receiveScratchPacket;
} NetSessionState;

typedef char xwa_guid_size[(sizeof(XwaGuid) == 0x10) ? 1 : -1];
typedef char xwa_net_pending_payload_size[(sizeof(NetPendingPayload) == 0x208) ? 1 : -1];
typedef char xwa_net_queued_packet_size[(sizeof(NetQueuedPacket) == 0x210) ? 1 : -1];
typedef char xwa_net_direct_play_runtime_recv_queue_offset
	[(offsetof(NetDirectPlayRuntimeState, recvQueue) == 0x44C) ? 1 : -1];
typedef char xwa_net_reliable_peer_slot_size[(sizeof(NetReliablePeerSlot) == 0x238) ? 1 : -1];
typedef char xwa_net_reliable_peer_slot_packet_drop_count_offset
	[(offsetof(NetReliablePeerSlot, packetDropCount) == 0x22C) ? 1 : -1];

extern NetPlayerInfo g_netPlayers[32];
extern NetPlayerConnectionStats g_netPlayerConnectionStats[40];
extern void* g_netDirectPlayInterface;
extern NetDirectPlayRuntimeState g_netDirectPlayRuntimeState;
extern int g_netIsHost;
extern int g_netPlayerCount;
extern NetSessionState g_netSessionState;
extern char g_emptyString[1];
extern int g_netRecvQueueWriteIndex;
extern int g_netRecvQueueReadIndex;
extern int g_netRecvQueueCount;
extern NetQueuedPacket g_netSessionRecvQueue[1024];
extern NetQueuedPacket g_netSessionRecvHistory[128];
extern NetQueuedPacket g_netSessionExportRecvQueue[256];
extern int recvHistoryCount;
extern int recvQueueHighWater;
extern int g_netLastDeliveredRecvSequence;
extern NetReliablePeerSlot g_netRuntimeReliablePeerSlots[40];
extern uint32_t g_netSequenceCount;
extern int dpid;
extern int g_playerAbortFlags[8];
extern int g_pingIndicator;
extern int g_lagIndicator;
extern uint32_t g_flightNetScratchPacket[128];
extern uint32_t g_flightNetInputDeltaBatchPacket[128];
extern int g_flightNetInputDeltaBatchLen;
extern int g_flightNetLastInputBatchSendTime;
extern int g_flightNetInputBatchIntervalTicks;
extern const int g_flightNetSmallSessionPlayerThreshold;
extern int g_playerConnected[8];
extern uint32_t g_netSessionScratchPacket[128];
extern int g_netSessionFlightHandshakeActive;
extern char g_playerTauntText[8][280];

int Net_PumpIncomingPackets(void);
int Net_SetNetworkPort(const unsigned short* port);
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
int Net_ShutdownDirectPlaySession(void);
void Net_ShutdownDirectPlaySessionForQuit(void);
int Net_IsHost(void);
int Net_HasQueuedPacketTypeOrBacklog(int packetType);
int Net_HasQueuedJoinRequestOrBacklog(void);
int Net_SendPacketAndFlush(int toPlayerId, const void* packet, unsigned int packetSize);
int Net_SendDirectPlayPacket(int destPlayerId, const void* packet, int packetSize, int flags);
int Net_SendSequencedDirectPlayPacket(int destPlayerId, int sequenceMode, int sequenceId, const void* packet,
									  unsigned int packetSize);
int NetSession_SendPacket(int directPlayId, const uint32_t* payload, int payloadSize);
int* NetSession_ReceiveGamePacket(int* outSenderDpid, int* outAux);
int* NetSession_WaitForGamePacket(int* outDpid, int* outAux, int timeoutSeconds);
int* NetSession_ReceivePacket(int* outSenderDirectPlayId, int* outPayloadSize);
int NetSession_BroadcastPacketToPlayers(const uint32_t* payload, int payloadSize);
int NetSession_SendCompactGamePacket(int destDplayId, const uint32_t* packet, int packetSize);
int NetSession_SendSequencedGamePacket(int destDplayId, uint8_t localSeq, uint8_t remoteSeq,
									   const uint32_t* packet, unsigned int packetSize);
int Net_GetHostPlayerId(void);
int Net_ClearPlayerReadyFlagWithLockGuard(int playerId);
int Net_SetPlayerNameWithLockGuard(int playerId, const char* longName, const char* shortName);
int Net_ResetRosterToLocalPlayerWithLockGuard(void);
int NetSession_CompactReliablePeerSlotsForRoster(void);
void NetReliable_ResetRecvQueueState(void);
unsigned int Net_AddSequence(int directPlayId);
int Net_CheckAndRecordIncomingSequence(int playerId, int sequenceId, int useChannel0, int useChannel2);
unsigned int NetReliable_FindOrCreatePeerSlot(int directPlayId);
int NetReliable_IsDuplicateRecvSequence(int directPlayId, int sequence, int channelA, int channelB);
int NetReliable_FindQueuedRecvPacket(int unused, int remoteSeq, int wantType0, int wantType2, int peerSlot);
int NetReliable_RemoveQueuedPacket(unsigned int queueIndex);
int NetReliable_CompactLocalReceiveQueue(void);
int NetSession_GetLocalPlayerId(void);
int NetSession_GetHostDplayId(void);
int NetSession_GetLocalDplayId(void);
#if !defined(_MSC_VER) || _MSC_VER > 1100
int NetSession_StubReturnTrue(void);
#else
int NetSession_StubReturnTrue();
#endif
int NetSession_ExitStub(int packetType);
int NetSession_GetPlayerCount(void);
char* NetSession_GetPlayerName(int playerSlot);
SessionPlayerInfo* NetSession_GetPlayerRoster(int* outCount);
int NetSession_FindPlayerSlotByDpid(int dpid);
int NetSession_CountActivePlayers(void);
int NetReliable_GetPeerPacketDropCountByDpid(int directPlayId);
char* FlightNet_GetStatusPlayerName(void);
int NetSession_InitGameSession(const char* sessionName, const char* pilotName, int localId,
							   const char* mpGameName, const char* networkType, int numHumanPlayers,
							   int inProgressLaunch);
void NetSession_HandleHandshakePacket(int packetOpcode, const int* packet);
void NetSession_Shutdown(void);
int FlightNet_BroadcastStillLoadingPulse(void);
int FlightNet_SendStillLoadingPulse(void);
int FlightNet_BroadcastLocalPlayerLeft(void);
int FlightNet_BroadcastPlayerDisconnected(int playerIdx);
int FlightNet_BroadcastPlayerAbort(int playerIdx);
int FlightNet_SyncPlayerOptionsAndTaunts(void);
int FlightNet_SendWorldChecksumToLocalPlayer(const int* worldChecksum, const int* peerChecksum,
											 int checksumDwordCount);
int FlightNet_BroadcastWorldChecksum(const int* worldChecksum, const int* peerChecksum,
									 int checksumDwordCount);
void FlightNet_MarkPilotNetworkPlayerLeft(int playerIdx);

#ifdef __cplusplus
}
#endif

#endif
