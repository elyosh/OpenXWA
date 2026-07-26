#ifndef XWA_FRONTEND_FRONTEND_NET_H
#define XWA_FRONTEND_FRONTEND_NET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define XWA_FRONTEND_NET_PACKED_STRUCT
#else
#define XWA_FRONTEND_NET_PACKED_STRUCT __attribute__((packed))
#endif

typedef struct XWA_FRONTEND_NET_PACKED_STRUCT MpRosterEntry {
	char name[14];
	int32_t playerId;
	int32_t rating;
	uint8_t gap_16[20];
} MpRosterEntry;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef XWA_FRONTEND_NET_PACKED_STRUCT

typedef enum NetworkTransportType {
	NET_TRANSPORT_IPX = 0,
	NET_TRANSPORT_TCPIP = 1,
	NET_TRANSPORT_MODEM = 2,
	NET_TRANSPORT_SERIAL = 3,
} NetworkTransportType;

typedef struct FrontendNetGuid {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t Data4[8];
} FrontendNetGuid;

typedef struct FrontendNetPacket {
	uint32_t packetType;
	uint8_t payload[508];
} FrontendNetPacket;

typedef char xwa_frontend_net_packet_size[(sizeof(FrontendNetPacket) == 0x200) ? 1 : -1];

extern MpRosterEntry g_mpRoster[8];
extern int g_mpRosterReadyFlags[8];
extern int isHost;
extern int g_localPilotNetworkPlayerIndex;
extern int g_frontendChatTeamOnly;
extern int g_frontendNetPacketArg0;
extern int g_frontendNetPacketArg1;
extern int g_frontendNetPacketSenderPlayerId;
extern FrontendNetGuid g_frontendNetXwaDirectPlayAppGuid;
extern FrontendNetPacket g_frontendNetPacketScratch;

int FrontendNet_JoinGameScreen(int frameCounter);
int FrontendNet_HostGameExit(int frameCounter);
int FrontendNet_HostGameScreen(int frameCounter);
int FrontendNet_IsTeamLocalPlayer(int team);
int FrontendNet_ProcessNetworkPackets(void);
int FrontendNet_UpdateAndDrawPanel(int frameCounter);
int MpRoster_CompactActiveEntries(void);
int CombatSim_ClearDisconnectedSlotOwners(void);

#ifdef __cplusplus
}
#endif

#endif
