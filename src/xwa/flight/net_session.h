#ifndef XWA_FLIGHT_NET_SESSION_H
#define XWA_FLIGHT_NET_SESSION_H

#include "xwa/net/net_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SessionPlayerInfo {
	char sessionName[16];
	char playerName[16];
	int32_t dplayId;
	int32_t activeFlag;
} SessionPlayerInfo;

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

typedef char xwa_session_player_info_size[(sizeof(SessionPlayerInfo) == 0x28) ? 1 : -1];

extern NetSessionState g_netSessionState;
extern int g_netRecvQueueWriteIndex;
extern int g_netRecvQueueReadIndex;
extern int g_netRecvQueueCount;
extern NetQueuedPacket g_netSessionRecvQueue[1024];
extern NetQueuedPacket g_netSessionRecvHistory[128];
extern NetQueuedPacket g_netSessionExportRecvQueue[256];
extern int recvHistoryCount;
extern int recvQueueHighWater;
extern int g_netLastDeliveredRecvSequence;
extern uint32_t g_netSessionScratchPacket[128];
extern int g_netSessionFlightHandshakeActive;

int NetSession_SendPacket(int directPlayId, const uint32_t* payload, int payloadSize);
int* NetSession_ReceiveGamePacket(int* outSenderDpid, int* outAux);
int* NetSession_WaitForGamePacket(int* outDpid, int* outAux, int timeoutSeconds);
int* NetSession_ReceivePacket(int* outSenderDirectPlayId, int* outPayloadSize);
int NetSession_BroadcastPacketToPlayers(const uint32_t* payload, int payloadSize);
int NetSession_SendCompactGamePacket(int destDplayId, const uint32_t* packet, int packetSize);
int NetSession_SendSequencedGamePacket(int destDplayId, uint8_t localSeq, uint8_t remoteSeq,
									   const uint32_t* packet, unsigned int packetSize);
void NetReliable_ResetRecvQueueState(void);
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
int NetSession_InitGameSession(const char* sessionName, const char* pilotName, int localId,
							   const char* mpGameName, const char* networkType, int numHumanPlayers,
							   int inProgressLaunch);
void NetSession_HandleHandshakePacket(int packetOpcode, const int* packet);
void NetSession_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
