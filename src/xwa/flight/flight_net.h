#ifndef XWA_FLIGHT_FLIGHT_NET_H
#define XWA_FLIGHT_FLIGHT_NET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
extern char g_playerTauntText[8][280];

char* FlightNet_GetStatusPlayerName(void);
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
void FlightNet_ProcessIncomingPackets(void);
int FlightNet_WaitForMissionStart(void);
int FlightNet_WaitForWorldStateChunkAcks(int directPlayId, int chunkCount);
int FlightNet_SendWorldStateResyncApplyRequest(int directPlayId, int worldStateSize);
int FlightNet_SendWorldStateResyncToPlayer(int directPlayId, uint8_t* worldState, int worldStateSize);
int FlightNet_SampleAndSendInput(void);

#ifdef __cplusplus
}
#endif

#endif
