#include "xwa/net/net.h"

#include "xwa/assets/string_table.h"
#include "xwa_runtime/compat/directx/dx_win_types.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/time.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct XwaDirectPlay4 XwaDirectPlay4;

typedef struct XwaDirectPlay4Vtbl {
	void* reserved0[2];
	uint32_t(XWA_DXAPI* Release)(XwaDirectPlay4* self);
	void* reserved3;
	HRESULT(XWA_DXAPI* Close)(XwaDirectPlay4* self);
	HRESULT(XWA_DXAPI* CreateGroup)(XwaDirectPlay4* self, int32_t* outGroupId, void* name, void* data,
								   uint32_t dataSize, uint32_t flags);
	void* reserved6[3];
	HRESULT(XWA_DXAPI* DestroyPlayer)(XwaDirectPlay4* self, int32_t playerId);
	void* reserved10[4];
	HRESULT(XWA_DXAPI* GetCaps)(XwaDirectPlay4* self, void* caps, uint32_t flags);
	void* reserved15[11];
	HRESULT(XWA_DXAPI* Send)(XwaDirectPlay4* self, int32_t fromPlayerId, int32_t toPlayerId, uint32_t flags,
							 void* data, uint32_t dataSize);
} XwaDirectPlay4Vtbl;

struct XwaDirectPlay4 {
	const XwaDirectPlay4Vtbl* lpVtbl;
};

// GLOBAL: XWA 0xA21449
void* g_netDirectPlayInterface;
#ifndef XWA_MODERN
// GLOBAL: XWA 0xA21445
XwaDirectPlay4* g_netTempDirectPlayInterface;
// GLOBAL: XWA 0xA2144D
XwaDirectPlay4* g_netUnusedComInterface;
// GLOBAL: XWA 0xA21459
XwaGuid g_netAppGuid;
// GLOBAL: XWA 0xA21479
int g_netHostPlayerId;
// GLOBAL: XWA 0xA2147D
int g_netGroupDplayId;
// GLOBAL: XWA 0xA2148D
char g_netSessionName[32];
// GLOBAL: XWA 0x7829D8
int g_netActiveTransportType;
// GLOBAL: XWA 0x7829DC
int g_netSessionStartContinue;
// GLOBAL: XWA 0xAA5E05
NetQueuedPacket g_netRuntimeRecvHistory[128];
// GLOBAL: XWA 0xAB6605
int g_netRuntimeRecvHistoryCount;
// GLOBAL: XWA 0xABC315
NetQueuedPacket* g_netExportRecvQueuePtr;
// GLOBAL: XWA 0xABC319
int g_netExportRecvQueueHighWater;
// GLOBAL: XWA 0x5AB8D0
const XwaGuid g_netXwaDirectPlayAppGuid = {
	0x09438c20u,
	0xe01fu,
	0x11cfu,
	{ 0x86u, 0x81u, 0x11u, 0xaau, 0x15u, 0x3du, 0x4eu, 0x58u },
};
#endif
// GLOBAL: XWA 0xA214AD
NetPlayerInfo g_netPlayers[32];
// GLOBAL: XWA 0xA219AD
NetDirectPlayRuntimeState g_netDirectPlayRuntimeState;
// GLOBAL: XWA 0x782A08
NetPlayerConnectionStats g_netPlayerConnectionStats[40];
// GLOBAL: XWA 0xA21481
int g_netIsHost;
// GLOBAL: XWA 0xA21485
int g_netPlayerCount;
// GLOBAL: XWA 0x9AFEE0
NetSessionState g_netSessionState;
// GLOBAL: XWA 0x749AA4
int g_netRecvQueueWriteIndex;
// GLOBAL: XWA 0x749AA8
int g_netRecvQueueReadIndex;
// GLOBAL: XWA 0x749AAC
int g_netRecvQueueCount;
// GLOBAL: XWA 0x694280
NetQueuedPacket g_netSessionRecvQueue[1024];
// GLOBAL: XWA 0x718290
NetQueuedPacket g_netSessionRecvHistory[128];
// GLOBAL: XWA 0x728A90
NetQueuedPacket g_netSessionExportRecvQueue[256];
// GLOBAL: XWA 0x749A9C
int recvHistoryCount;
// GLOBAL: XWA 0x749AA0
int recvQueueHighWater;
// GLOBAL: XWA 0x749A94
int g_netLastDeliveredRecvSequence;
// GLOBAL: XWA 0xAB6819
NetReliablePeerSlot g_netRuntimeReliablePeerSlots[40];
// GLOBAL: XWA 0xABC311
uint32_t g_netSequenceCount;
// GLOBAL: XWA 0x631860
char g_emptyString[1];
// GLOBAL: XWA 0x770E7C
int dpid;
// GLOBAL: XWA 0x80DA20
int g_playerAbortFlags[8];
// GLOBAL: XWA 0x7CAB5C
int g_pingIndicator;
// GLOBAL: XWA 0x8BF360
int g_lagIndicator;
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
// GLOBAL: XWA 0x694078
uint32_t g_netSessionScratchPacket[128];
// GLOBAL: XWA 0x8C1648
int g_netSessionFlightHandshakeActive;
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

#ifndef XWA_MODERN
typedef struct NetDirectPlayCaps {
	uint32_t size;
	uint32_t fields[9];
} NetDirectPlayCaps;

typedef struct NetRosterPeerRecord {
	int32_t directPlayId;
	uint8_t prevRecvSeqChannelA;
	uint8_t prevRecvSeqChannelB;
	uint8_t recvSeqChannelA;
	uint8_t recvSeqChannelB;
} NetRosterPeerRecord;

typedef struct NetRosterSyncPacket {
	uint32_t packetType;
	uint32_t unused;
	uint32_t peerCount;
	uint32_t hostTimestamp;
	NetRosterPeerRecord peers[40];
} NetRosterSyncPacket;

void Net_DisableAutoDialRegistrySetting(void);
void Net_RestoreAutoDialRegistrySetting(void);
const XwaGuid* Net_GetDirectPlayServiceProviderGuid(int networkType);
int Net_OpenDirectPlaySession(XwaGuid appGuid, int hostFlag, const char* sessionName, int networkType,
							  const char* connectionAddress, const void* joinSessionInstanceGuid);
int Net_CreateDirectPlayPlayer(const char* localPlayerInfo, const char* localPlayerName);
int Net_RefreshPlayerRoster(void);
int* Net_WaitForAppPacket(int* outPlayerId, int* outPacketType, int timeoutSeconds);
HRESULT XWA_DXAPI DirectPlayCreate(const XwaGuid* providerGuid, XwaDirectPlay4** outDirectPlay, void* outer);
#endif
#pragma pack(pop)

typedef char flight_net_options_wire_size[(sizeof(FlightNetOptionsWire) == 52) ? 1 : -1];
typedef char flight_net_options_packet_size[(sizeof(FlightNetOptionsPacket) == 56) ? 1 : -1];
typedef char flight_net_taunt_packet_size[(sizeof(FlightNetTauntPacket) == 288) ? 1 : -1];

// FUNCTION: XWA 0x52C130
int Net_PumpIncomingPackets(void) {
	/*
	 * Original 0x52C130 pumps IDirectPlay4::Receive, decodes the compact
	 * reliable-packet stream, and queues packets into g_netSessionRecvQueue.
	 * DirectPlay is a legacy platform boundary in the port; local delivery is
	 * handled by NetSession_SendPacket and related helpers instead.
	 *
	 * TODO: Replace this boundary stub with Aeron-backed network ingress when
	 * multiplayer transport is implemented.
	 */
	return 0;
}

// FUNCTION: XWA 0x531670
int Net_SetNetworkPort(const unsigned short* port) {
	(void)port;

	/* TODO: Reimplement Net_SetNetworkPort @ 0x531670. */
	return 1;
}

#ifdef XWA_MODERN
// FUNCTION: XWA 0x52B3A0
int Net_StartNetworkSession(XwaGuid appGuid, const char* localPlayerInfo, const char* localPlayerName,
							int hostFlag, const char* sessionName, int networkType, int waitForPlayerCount,
							int unusedA11, const char* connectionAddress,
							const void* joinSessionInstanceGuid) {
	(void)unusedA11;
	{
		char displaySessionName[32];

		(void)appGuid;
		(void)joinSessionInstanceGuid;
		if (networkType == NET_TRANSPORT_TCPIP) {
			strcpy(displaySessionName, "TCPIP game.");
		} else if (networkType == NET_TRANSPORT_MODEM) {
			strcpy(displaySessionName, "Dial a New Number.");
		} else if (networkType == NET_TRANSPORT_SERIAL) {
			strcpy(displaySessionName, "Direct serial game.");
		} else if (sessionName[0] != '\0') {
			strncpy(displaySessionName, sessionName, sizeof(displaySessionName));
			displaySessionName[sizeof(displaySessionName) - 1] = '\0';
		} else {
			snprintf(displaySessionName, sizeof(displaySessionName), "%s's Game.", localPlayerName);
		}

		g_netIsHost = hostFlag;
		if (!NetSession_InitGameSession(displaySessionName, localPlayerName, 1, sessionName,
										connectionAddress, waitForPlayerCount > 0 ? waitForPlayerCount : 1,
										0)) {
			return 0;
		}
		strncpy(g_netPlayers[0].playerName, localPlayerInfo, sizeof(g_netPlayers[0].playerName));
		g_netPlayers[0].playerName[sizeof(g_netPlayers[0].playerName) - 1] = '\0';
		strncpy(g_netPlayers[0].sessionName, localPlayerName, sizeof(g_netPlayers[0].sessionName));
		g_netPlayers[0].sessionName[sizeof(g_netPlayers[0].sessionName) - 1] = '\0';
		return 1;
	}
}
#else
// FUNCTION: XWA 0x52B3A0
int Net_StartNetworkSession(XwaGuid appGuid, const char* localPlayerInfo, const char* localPlayerName,
							int hostFlag, const char* sessionName, int networkType, int waitForPlayerCount,
							int unusedA11, const char* connectionAddress,
							const void* joinSessionInstanceGuid) {
	(void)unusedA11;
	{
		uint32_t startTick;
		int hostPlayerId;
		uint32_t hostTimestamp;
		int wasBackBufferLocked;
		char modemSessionName[32] = "Dial a New Number.";
		char tcpIpSessionName[32] = "TCPIP game.";
		int packetType;
		uint32_t ackPacket[6];
		char displaySessionName[32];
		char serialSessionName[32] = "Direct serial game.";
		NetDirectPlayCaps caps;
		NetReliablePeerSlot savedHostPeer;
		char errorText[256];
		g_netSessionStartContinue = 1;
		startTick = GetTickCount();
		for (;;) {
			unsigned int slot;
			const XwaGuid* providerGuid;
			int localPlayerId;
			wasBackBufferLocked = g_backBufferLocked.word & 0xff;
			FrontendDisplay_UnlockBackBuffer();
			Net_DisableAutoDialRegistrySetting();
			g_netAppGuid = g_netXwaDirectPlayAppGuid;
			g_netRuntimeRecvHistoryCount = 0;
			g_netDirectPlayRuntimeState.broadcastSeqCounter = 0;
			g_netDirectPlayRuntimeState.broadcastPendingPayload.pendingFlush = 1;
			g_netDirectPlayRuntimeState.broadcastPendingPayload.payload[0] = 57;
			g_netDirectPlayRuntimeState.broadcastPendingPayload.payloadLength = 1;
			g_netDirectPlayRuntimeState.groupSeqCounter = 0;
			g_netDirectPlayRuntimeState.groupPendingPayload.pendingFlush = 1;
			g_netDirectPlayRuntimeState.groupPendingPayload.payload[0] = 57;
			g_netDirectPlayRuntimeState.groupPendingPayload.payloadLength = 1;
			g_netSequenceCount = 0;
			g_netDirectPlayRuntimeState.reliableRetryLongTimeoutMode = 0;
			g_netGroupDplayId = 0;
			g_netHostPlayerId = 0;
			g_netExportRecvQueuePtr = NULL;
			g_netExportRecvQueueHighWater = 0;

			for (slot = 0; slot < 40; ++slot) {
				NetReliablePeerSlot* peer;

				peer = &g_netRuntimeReliablePeerSlots[slot];
				peer->prevRecvSeqDefault = 127;
				peer->prevRecvSeqChannelA = 127;
				peer->prevRecvSeqChannelB = 127;
				peer->recvSeqDefault = 127;
				peer->recvSeqChannelA = 127;
				peer->recvSeqChannelB = 127;
				peer->sendSeq = 0;
				peer->directPlayId = 0;
				peer->lastPiggybackType = 57;
				peer->piggybackLength = 1;
				peer->lastActivityMs = 0;
				peer->lastKeepaliveMs = 0;
				peer->packetCount = 0;
				peer->packetDropCount = 0;
				peer->packetRetryCount = 0;
			}
			memset(g_netRuntimeRecvHistory, 0, sizeof(g_netRuntimeRecvHistory));
			memset(g_netPlayerConnectionStats, 0, sizeof(g_netPlayerConnectionStats));
			g_directDraw->lpVtbl->FlipToGDISurface(g_directDraw);

			if (g_netDirectPlayInterface != NULL) {
				goto initialSessionFailure;
			}
			providerGuid = Net_GetDirectPlayServiceProviderGuid(networkType);
			if (providerGuid == NULL) {
				goto initialSessionFailure;
			}
			localPlayerId = DirectPlayCreate(providerGuid, &g_netTempDirectPlayInterface, NULL);
			if (localPlayerId != 0) {
				if (!ErrorText_LoadLine(6, errorText)) {
					FrontendDisplay_ShowGameMessageBox(
						"WARNING:  Connection failure!\n\nMake sure your Windows 95/98 network\n"
						"settings are properly configured\nfor this type of network game.\n\n"
						"Press Enter to continue.");
				} else {
					FrontendDisplay_ShowGameMessageBox(errorText);
				}
				if (wasBackBufferLocked) {
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				}
				Net_RestoreAutoDialRegistrySetting();
				return 0;
			}

			g_netTempDirectPlayInterface->lpVtbl->Release(g_netTempDirectPlayInterface);
			g_netTempDirectPlayInterface = NULL;
			g_netIsHost = hostFlag;
			strncpy(g_netPlayers[0].playerName, localPlayerInfo, 16);
			g_netPlayers[0].playerName[15] = '\0';
			strncpy(g_netPlayers[0].sessionName, localPlayerName, 16);
			g_netPlayers[0].sessionName[15] = '\0';

			{
				const char* selectedSessionName;

				if (networkType == NET_TRANSPORT_TCPIP) {
					selectedSessionName = tcpIpSessionName;
				} else if (networkType == NET_TRANSPORT_MODEM) {
					selectedSessionName = modemSessionName;
				} else if (networkType == NET_TRANSPORT_SERIAL) {
					selectedSessionName = serialSessionName;
				} else if (sessionName[0] != '\0') {
					selectedSessionName = sessionName;
				} else {
					sprintf(displaySessionName, "%s's Game.", localPlayerName);
					selectedSessionName = displaySessionName;
				}
				if (selectedSessionName != displaySessionName) {
					strcpy(displaySessionName, selectedSessionName);
				}
			}
			strncpy(g_netSessionName, displaySessionName, 32);
			g_netSessionName[31] = '\0';

			localPlayerId = Net_OpenDirectPlaySession(appGuid, hostFlag, displaySessionName, networkType,
													 connectionAddress, joinSessionInstanceGuid);
			if (localPlayerId == 0) {
				goto initialSessionFailure;
			}
			memset(&caps, 0, sizeof(caps));
			caps.size = sizeof(caps);
			((XwaDirectPlay4*)g_netDirectPlayInterface)
				->lpVtbl->GetCaps((XwaDirectPlay4*)g_netDirectPlayInterface, &caps, 0);

			localPlayerId = Net_CreateDirectPlayPlayer(localPlayerInfo, localPlayerName);
			if (localPlayerId == 0) {
				((XwaDirectPlay4*)g_netDirectPlayInterface)
					->lpVtbl->Close((XwaDirectPlay4*)g_netDirectPlayInterface);
				((XwaDirectPlay4*)g_netDirectPlayInterface)
					->lpVtbl->Release((XwaDirectPlay4*)g_netDirectPlayInterface);
				g_netDirectPlayInterface = NULL;
				if (g_netUnusedComInterface != NULL) {
					g_netUnusedComInterface->lpVtbl->Release(g_netUnusedComInterface);
					g_netUnusedComInterface = NULL;
				}
				goto initialSessionFailure;
			}
			goto localPlayerCreated;

initialSessionFailure:
			if (wasBackBufferLocked) {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}
			Net_RestoreAutoDialRegistrySetting();
			if (g_netSessionStartContinue == 0) {
				return 0;
			}
			g_netSessionStartContinue = 0;
			if ((int)(GetTickCount() - startTick) >= 60000) {
				return 0;
			}
			continue;

localPlayerCreated:
			g_netPlayers[0].playerId = localPlayerId;
			g_netDirectPlayRuntimeState.localPlayer = g_netPlayers[0];
			if (hostFlag != 0) {
				localPlayerId =
					((XwaDirectPlay4*)g_netDirectPlayInterface)
						->lpVtbl->CreateGroup((XwaDirectPlay4*)g_netDirectPlayInterface,
											 &g_netGroupDplayId, NULL, NULL, 0, 0);
				if (localPlayerId != 0) {
					goto destroyLocalPlayer;
				}
				g_netRuntimeReliablePeerSlots[0].directPlayId = g_netGroupDplayId;
				g_netSequenceCount = 1;
			}

			g_netPlayerCount = 1;
			Net_RefreshPlayerRoster();
			g_netDirectPlayRuntimeState.recvQueueWriteIndex = 0;
			g_netDirectPlayRuntimeState.recvQueueReadIndex = 0;
			g_netDirectPlayRuntimeState.recvQueueCount = 0;
			if (waitForPlayerCount > 0) {
				while (Net_GetPlayerCount() < waitForPlayerCount) {
					Net_PumpIncomingPackets();
				}
				goto sessionStarted;
			}
			if (hostFlag != 0) {
				g_netHostPlayerId = g_netDirectPlayRuntimeState.localPlayer.playerId;
				goto sessionStarted;
			}
			{
				const NetRosterSyncPacket* rosterPacket;
				int* packetData;

				do {
					packetData = Net_WaitForAppPacket(&hostPlayerId, &packetType, 5);
					if (packetData == NULL) {
						break;
					}
				} while (packetData[0] != 59);
				if (packetData != NULL) {
					unsigned int peerCount;

					g_netHostPlayerId = hostPlayerId;
					memset(&savedHostPeer, 0, sizeof(savedHostPeer));
					for (slot = 0; slot < g_netSequenceCount; ++slot) {
						if (g_netRuntimeReliablePeerSlots[slot].directPlayId == hostPlayerId) {
							savedHostPeer = g_netRuntimeReliablePeerSlots[slot];
							break;
						}
					}

					rosterPacket = (const NetRosterSyncPacket*)packetData;
					peerCount = rosterPacket->peerCount;
					g_netSequenceCount = peerCount;
					hostTimestamp = rosterPacket->hostTimestamp;
					for (slot = 0; slot < peerCount; ++slot) {
						g_netRuntimeReliablePeerSlots[slot].directPlayId =
							rosterPacket->peers[slot].directPlayId;
						g_netRuntimeReliablePeerSlots[slot].prevRecvSeqChannelA =
							rosterPacket->peers[slot].prevRecvSeqChannelA;
						g_netRuntimeReliablePeerSlots[slot].prevRecvSeqChannelB =
							rosterPacket->peers[slot].prevRecvSeqChannelB;
						g_netRuntimeReliablePeerSlots[slot].recvSeqChannelA =
							rosterPacket->peers[slot].recvSeqChannelA;
						g_netRuntimeReliablePeerSlots[slot].recvSeqChannelB =
							rosterPacket->peers[slot].recvSeqChannelB;
						g_netRuntimeReliablePeerSlots[slot].prevRecvSeqDefault = 127;
						g_netRuntimeReliablePeerSlots[slot].recvSeqDefault = 127;
						g_netRuntimeReliablePeerSlots[slot].sendSeq = 0;
						g_netRuntimeReliablePeerSlots[slot].lastPiggybackType = 57;
						g_netRuntimeReliablePeerSlots[slot].piggybackLength = 1;
						g_netRuntimeReliablePeerSlots[slot].lastActivityMs = GetTickCount();
						g_netRuntimeReliablePeerSlots[slot].lastKeepaliveMs = GetTickCount();
						peerCount = g_netSequenceCount;
					}

					if (savedHostPeer.directPlayId != 0) {
						for (slot = 0; slot < peerCount; ++slot) {
							if (g_netHostPlayerId == g_netRuntimeReliablePeerSlots[slot].directPlayId) {
								g_netRuntimeReliablePeerSlots[slot].prevRecvSeqDefault =
									savedHostPeer.prevRecvSeqDefault;
								g_netRuntimeReliablePeerSlots[slot].recvSeqDefault =
									savedHostPeer.recvSeqDefault;
								g_netRuntimeReliablePeerSlots[slot].sendSeq = savedHostPeer.sendSeq;
								memcpy(&g_netRuntimeReliablePeerSlots[slot].lastPiggybackType,
									   &savedHostPeer.lastPiggybackType,
									   savedHostPeer.piggybackLength);
								g_netRuntimeReliablePeerSlots[slot].piggybackLength =
									savedHostPeer.piggybackLength;
								g_netRuntimeReliablePeerSlots[slot].lastActivityMs =
									savedHostPeer.lastActivityMs;
								g_netRuntimeReliablePeerSlots[slot].lastKeepaliveMs =
									savedHostPeer.lastKeepaliveMs;
								peerCount = g_netSequenceCount;
							}
						}
					}

					ackPacket[0] = 54;
					ackPacket[1] = hostTimestamp;
					memset(&ackPacket[2], 0, 3 * sizeof(ackPacket[0]));
					Net_SendDirectPlayPacket(g_netHostPlayerId, ackPacket, 20, 0);
					goto sessionStarted;
				}

destroyLocalPlayer:
				((XwaDirectPlay4*)g_netDirectPlayInterface)
					->lpVtbl->DestroyPlayer((XwaDirectPlay4*)g_netDirectPlayInterface,
											g_netDirectPlayRuntimeState.localPlayer.playerId);
				((XwaDirectPlay4*)g_netDirectPlayInterface)
					->lpVtbl->Close((XwaDirectPlay4*)g_netDirectPlayInterface);
				((XwaDirectPlay4*)g_netDirectPlayInterface)
					->lpVtbl->Release((XwaDirectPlay4*)g_netDirectPlayInterface);
				g_netDirectPlayInterface = NULL;
				if (g_netUnusedComInterface != NULL) {
					g_netUnusedComInterface->lpVtbl->Release(g_netUnusedComInterface);
					g_netUnusedComInterface = NULL;
				}
				if (wasBackBufferLocked) {
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				}
				Net_RestoreAutoDialRegistrySetting();
				if (g_netSessionStartContinue == 0) {
					return 0;
				}
				g_netSessionStartContinue = 0;
				if ((int)(GetTickCount() - startTick) >= 60000) {
					return 0;
				}
				continue;
			}

sessionStarted:
			if (wasBackBufferLocked) {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}
			Net_RestoreAutoDialRegistrySetting();
			g_netActiveTransportType = networkType;
			return 1;
		}
	}
}
#endif

// FUNCTION: XWA 0x52EFF0
int Net_GetLocalPlayerId(void) { return g_netPlayers[0].playerId; }

// FUNCTION: XWA 0x52DD60
// Session player count, clamped to a minimum of 1.
int Net_GetPlayerCount(void) {
	if (g_netPlayerCount == 0) {
		return 1;
	}
	return g_netPlayerCount;
}

// FUNCTION: XWA 0x52F1B0
int Net_CountReadyPlayers(void) {
	int count;
	int i;

	count = 0;
	for (i = 0; i < 32; ++i) {
		if (g_netPlayers[i].readyFlag == 1) {
			++count;
		}
	}

	return count;
}

// FUNCTION: XWA 0x52DD40
NetPlayerInfo* Net_GetPlayerRoster(int* outCount) {
	*outCount = g_netPlayerCount;
	return g_netPlayers;
}

// FUNCTION: XWA 0x52FAC0
unsigned int Net_GetAverageLatencyMs(int playerId) {
	int playerIndex;

	for (playerIndex = 0; playerIndex < 40; ++playerIndex) {
		if (g_netPlayerConnectionStats[playerIndex].playerId == playerId) {
			if (g_netPlayerConnectionStats[playerIndex].latencySampleCount == 0) {
				return 1;
			}

			return g_netPlayerConnectionStats[playerIndex].latencyTotalMs /
				   g_netPlayerConnectionStats[playerIndex].latencySampleCount;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x52FD10
int Net_GetPacketDropRateBasisPoints(int playerId) {
	int packetCount;
	int droppedPacketCount;
	int dropRate;

	if (g_netIsHost) {
		unsigned int slot;
		int playerIndex;

		slot = Net_AddSequence(playerId);
		if (slot < g_netSequenceCount && slot < 40) {
			packetCount = g_netRuntimeReliablePeerSlots[slot].packetCount;
			droppedPacketCount = g_netRuntimeReliablePeerSlots[slot].packetDropCount +
								 2 * g_netRuntimeReliablePeerSlots[slot].packetRetryCount;
		} else {
			packetCount = 0;
			droppedPacketCount = 0;
		}

		for (playerIndex = 0; playerIndex < 40; ++playerIndex) {
			if (g_netPlayerConnectionStats[playerIndex].playerId == playerId) {
				packetCount += g_netPlayerConnectionStats[playerIndex].packetCount;
				droppedPacketCount += g_netPlayerConnectionStats[playerIndex].packetDropCount +
									  2 * g_netPlayerConnectionStats[playerIndex].packetRetryCount;
			}
		}

		if (packetCount == 0) {
			packetCount = 1;
		}

		dropRate = droppedPacketCount * 10000 / packetCount;
	} else {
		int playerIndex;

		for (playerIndex = 0; playerIndex < 40; ++playerIndex) {
			if (g_netPlayerConnectionStats[playerIndex].playerId == playerId) {
				packetCount = g_netPlayerConnectionStats[playerIndex].packetCount;
				if (packetCount == 0) {
					packetCount = 1;
				}

				droppedPacketCount = g_netPlayerConnectionStats[playerIndex].packetDropCount +
									 2 * g_netPlayerConnectionStats[playerIndex].packetRetryCount;
				dropRate = (unsigned int)(droppedPacketCount * 10000) / (unsigned int)packetCount;
				if (dropRate > 10000) {
					dropRate = 10000;
				}

				return dropRate;
			}
		}

		return 0;
	}

	if (dropRate > 10000) {
		dropRate = 10000;
	}

	return dropRate;
}

// FUNCTION: XWA 0x530130
int Net_GetPlayerPacketCount(int playerId) {
	unsigned int slot;
	int packetCount;
	int playerIndex;

	slot = Net_AddSequence(playerId);
	if (slot < g_netSequenceCount && slot < 40) {
		packetCount = g_netRuntimeReliablePeerSlots[slot].packetCount;
	} else {
		packetCount = 0;
	}

	for (playerIndex = 0; playerIndex < 40; ++playerIndex) {
		if (g_netPlayerConnectionStats[playerIndex].playerId == playerId) {
			return packetCount + g_netPlayerConnectionStats[playerIndex].packetCount;
		}
	}

	return packetCount;
}

// FUNCTION: XWA 0x530190
int Net_GetPlayerPacketDropCount(int playerId) {
	unsigned int slot;
	int dropCount;
	int playerIndex;

	slot = Net_AddSequence(playerId);
	if (slot < g_netSequenceCount && slot < 40) {
		dropCount = g_netRuntimeReliablePeerSlots[slot].packetDropCount;
	} else {
		dropCount = 0;
	}

	for (playerIndex = 0; playerIndex < 40; ++playerIndex) {
		if (g_netPlayerConnectionStats[playerIndex].playerId == playerId) {
			Net_AddSequence(playerId);
			return dropCount + g_netPlayerConnectionStats[playerIndex].packetDropCount;
		}
	}

	return dropCount;
}

// FUNCTION: XWA 0x530200
int Net_GetPlayerPacketRetryCount(int playerId) {
	unsigned int slot;
	int retryCount;
	int playerIndex;

	slot = Net_AddSequence(playerId);
	if (slot < g_netSequenceCount && slot < 40) {
		retryCount = g_netRuntimeReliablePeerSlots[slot].packetRetryCount;
	} else {
		retryCount = 0;
	}

	for (playerIndex = 0; playerIndex < 40; ++playerIndex) {
		if (g_netPlayerConnectionStats[playerIndex].playerId == playerId) {
			return retryCount + g_netPlayerConnectionStats[playerIndex].packetRetryCount;
		}
	}

	return retryCount;
}

// FUNCTION: XWA 0x52F0B0
NetPlayerInfo* Net_FindPlayer(int playerId) {
	int playerIndex;

	for (playerIndex = 0; playerIndex < g_netPlayerCount; ++playerIndex) {
		if (g_netPlayers[playerIndex].playerId == playerId) {
			break;
		}
	}

	if (playerIndex == g_netPlayerCount) {
		return NULL;
	}

	return &g_netPlayers[playerIndex];
}

// FUNCTION: XWA 0x52F000
int Net_MarkPlayerReadyNoLock(int playerId) {
	int playerIndex;

	for (playerIndex = 0; playerIndex < g_netPlayerCount; ++playerIndex) {
		if (g_netPlayers[playerIndex].playerId == playerId) {
			break;
		}
	}

	if (playerIndex != g_netPlayerCount) {
		g_netPlayers[playerIndex].readyFlag = 1;
	}

	return playerIndex;
}

// FUNCTION: XWA 0x52F0F0
int Net_SetPlayerReady(int playerId) {
	(void)playerId;

	/* TODO: Reimplement Net_SetPlayerReady @ 0x52F0F0. */
	return 0;
}

static uint8_t Net_IncrementSeq7(int* sequence) {
	uint32_t nextSeq;

	nextSeq = (uint32_t)*sequence + 1u;
	if (nextSeq > 0x7fu) {
		nextSeq = 0;
	}
	*sequence = (int32_t)nextSeq;
	return (uint8_t)nextSeq;
}

static void Net_QueueKeepaliveRecord(int directPlayId, uint32_t opcode, uint8_t packetClass,
									 uint8_t sequenceByte) {
	NetQueuedPacket* packet;
	int writeIndex;

	if (g_netRecvQueueCount >= 1024) {
		return;
	}

	writeIndex = g_netRecvQueueWriteIndex;
	packet = &g_netSessionRecvQueue[writeIndex];
	memcpy(packet->payload, &opcode, sizeof(opcode));
	packet->directPlayId = directPlayId;
	packet->payloadSize = 4;
	packet->aux = 0;
	packet->meta0 = 0;
	packet->queuedFlag = 0;
	packet->packetClass = packetClass;
	packet->sequenceByte = sequenceByte;

	++g_netRecvQueueCount;
	++writeIndex;
	g_netRecvQueueWriteIndex = writeIndex;
	if (writeIndex >= 1024) {
		g_netRecvQueueWriteIndex = 0;
	}
}

// FUNCTION: XWA 0x52FE10
int Net_UpdateKeepaliveSequences(void) {
	uint32_t tickNow;

	if (g_netIsHost) {
		unsigned int playerIdx;

		for (playerIdx = 0; playerIdx < 32u; ++playerIdx) {
			int playerId;

			playerId = g_netPlayers[playerIdx].playerId;
			if (playerId != 0 && playerId != g_netSessionState.localDplayId &&
				playerId != g_netSessionState.groupDplayId && g_netPlayers[playerIdx].readyFlag != 0) {
				unsigned int slot;

				slot = Net_AddSequence(playerId);
				if (slot < g_netSequenceCount) {
					NetReliablePeerSlot* peer;

					peer = &g_netRuntimeReliablePeerSlots[slot];
					tickNow = GetTickCount();
					if (tickNow - peer->lastKeepaliveMs > 0xafc8u) {
						uint32_t packet;

						peer->lastKeepaliveMs = GetTickCount();
						packet = 91;
						Net_SendPacketAndFlush(playerId, &packet, 4u);
						Net_QueueKeepaliveRecord(playerId, 71, 1, Net_IncrementSeq7(&peer->recvSeqDefault));
					}
				}
			}
		}

		return 1;
	}

	{
		unsigned int slot;

		slot = Net_AddSequence(g_netSessionState.hostDplayId);
		if (slot >= g_netSequenceCount) {
			return 0;
		}

		tickNow = GetTickCount();
		if (tickNow - g_netRuntimeReliablePeerSlots[slot].lastKeepaliveMs > 0xafc8u) {
			NetReliablePeerSlot* peer;

			peer = &g_netRuntimeReliablePeerSlots[slot];
			peer->lastKeepaliveMs = GetTickCount();
			Net_QueueKeepaliveRecord(g_netSessionState.hostDplayId, 70, 0,
									 Net_IncrementSeq7(&peer->recvSeqChannelA));
		}
	}

	return 1;
}

// FUNCTION: XWA 0x52F740
int Net_SendSequenceKeepalives(void) { return Net_UpdateKeepaliveSequences(); }

// FUNCTION: XWA 0x52BBE0
int Net_ShutdownDirectPlaySession(void) {
	/* TODO: Reimplement Net_ShutdownDirectPlaySession @ 0x52BBE0. */
	return 0;
}

// FUNCTION: XWA 0x52BBD0
void Net_ShutdownDirectPlaySessionForQuit(void) { Net_ShutdownDirectPlaySession(); }

// FUNCTION: XWA 0x52DD80
int Net_IsHost(void) { return g_netIsHost; }

// FUNCTION: XWA 0x52DD90
int Net_HasQueuedPacketTypeOrBacklog(int packetType) {
	int wasBackBufferLocked;
	int queueIndex;
	int remaining;

	if (g_netDirectPlayInterface != 0) {
		wasBackBufferLocked = g_backBufferLocked.word & 0xff;
		FrontendDisplay_UnlockBackBuffer();
		Net_PumpIncomingPackets();
		if (g_netDirectPlayRuntimeState.recvQueueCount > 512) {
			if (wasBackBufferLocked) {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}
			return 1;
		}

		queueIndex = g_netDirectPlayRuntimeState.recvQueueReadIndex;
		remaining = g_netDirectPlayRuntimeState.recvQueueCount;
		while (remaining > 0) {
			int directPlayId;

			directPlayId = g_netDirectPlayRuntimeState.recvQueue[queueIndex].directPlayId;
#ifdef XWA_MODERN
			if (directPlayId != 0 &&
				ByteOrder_ReadI32Le(g_netDirectPlayRuntimeState.recvQueue[queueIndex].payload) ==
					packetType) {
#else
			if (directPlayId != 0 &&
				*(const int32_t*)g_netDirectPlayRuntimeState.recvQueue[queueIndex].payload == packetType) {
#endif
				if (wasBackBufferLocked) {
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				}
				return 1;
			}
			if (++queueIndex >= 1024) {
				queueIndex = 0;
			}
			--remaining;
		}

		if (wasBackBufferLocked) {
			g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
		}
	}
	return 0;
}

// FUNCTION: XWA 0x52DE30
int Net_HasQueuedJoinRequestOrBacklog(void) {
	int wasBackBufferLocked;
	int queueIndex;
	int remaining;

	if (g_netDirectPlayInterface != 0) {
		wasBackBufferLocked = g_backBufferLocked.word & 0xff;
		FrontendDisplay_UnlockBackBuffer();
		Net_PumpIncomingPackets();
		if (g_netDirectPlayRuntimeState.recvQueueCount > 512) {
			if (wasBackBufferLocked) {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}
			return 1;
		}

		queueIndex = g_netDirectPlayRuntimeState.recvQueueReadIndex;
		remaining = g_netDirectPlayRuntimeState.recvQueueCount;
		while (remaining > 0) {
			int directPlayId;

			directPlayId = g_netDirectPlayRuntimeState.recvQueue[queueIndex].directPlayId;
#ifdef XWA_MODERN
			if (directPlayId == 0 &&
				ByteOrder_ReadI32Le(g_netDirectPlayRuntimeState.recvQueue[queueIndex].payload) == 3) {
#else
			if (directPlayId == 0 &&
				*(const int32_t*)g_netDirectPlayRuntimeState.recvQueue[queueIndex].payload == 3) {
#endif
				if (wasBackBufferLocked) {
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				}
				return 1;
			}
			if (++queueIndex >= 1024) {
				queueIndex = 0;
			}
			--remaining;
		}

		if (wasBackBufferLocked) {
			g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
		}
	}
	return 0;
}

// FUNCTION: XWA 0x52CEE0
int Net_SendPacketAndFlush(int toPlayerId, const void* packet, unsigned int packetSize) {
	(void)toPlayerId;
	(void)packet;
	(void)packetSize;

	/* TODO: Reimplement Net_SendPacketAndFlush @ 0x52CEE0. */
	return 1;
}

static int NetSession_PacketHasSizeTrailer(uint32_t packetType) {
	return packetType < 0x3cu || packetType >= 0x40u;
}

static int NetSession_BuildCompactPacket(uint8_t* outPacket, uint16_t headerWord, const uint32_t* payload,
										 int payloadSize, int hasSizeTrailer) {
	uint8_t* payloadDst;
	int payloadBytes;
	int headerSize;

	payloadBytes = payloadSize - 4;
	ByteOrder_WriteU16Le(outPacket, headerWord);
	payloadDst = outPacket + 2;
	headerSize = 2;
	if (hasSizeTrailer) {
		ByteOrder_WriteU16Le(outPacket + 2, (uint16_t)payloadBytes);
		payloadDst = outPacket + 4;
		headerSize = 4;
	}

	memcpy(payloadDst, payload + 1, (size_t)payloadBytes);
	return headerSize + payloadBytes;
}

static void NetSession_UpdatePendingPayload(NetPendingPayload* pending, uint8_t compactType,
											const uint32_t* payload, int payloadSize) {
	pending->payload[0] = compactType;
	memcpy(&pending->payload[1], payload + 1, (size_t)(payloadSize - 4));
	pending->payloadLength = payloadSize - 3;
}

static int NetSession_AppendPendingPayload(uint8_t* outPacket, int compactSize, NetPendingPayload* pending) {
	uint8_t* appendDst;

	appendDst = outPacket + compactSize;
	if (pending->pendingFlush) {
		*appendDst = 57;
		pending->pendingFlush = 0;
		return compactSize + 1;
	}

	memcpy(appendDst, pending->payload, (size_t)pending->payloadLength);
	return compactSize + pending->payloadLength;
}

static void NetSession_RecordSendHistory(int directPlayId, const uint32_t* payload, int payloadSize,
										 uint16_t headerWord) {
	NetQueuedPacket* packet;

	if (payload[0] == 2) {
		packet = &g_netSessionExportRecvQueue[recvQueueHighWater];
		memcpy(packet->payload, payload, (size_t)payloadSize);
		packet->directPlayId = directPlayId;
		packet->payloadSize = payloadSize;
		packet->aux = 0;
		packet->meta0 = 0;
		packet->packetClass = 0;
		packet->sequenceByte = (uint8_t)((headerWord >> 8) & 0x7fu);

		++recvQueueHighWater;
		if (recvQueueHighWater >= 256) {
			recvQueueHighWater = 0;
		}
	}

	packet = &g_netSessionRecvHistory[recvHistoryCount];
	memcpy(packet->payload, payload, (size_t)payloadSize);
	packet->directPlayId = directPlayId;
	packet->payloadSize = payloadSize;
	packet->aux = 0;
	packet->meta0 = 0;
	if (directPlayId == 0) {
		packet->packetClass = 0;
	} else if (directPlayId == g_netSessionState.groupDplayId) {
		packet->packetClass = 2;
	} else {
		packet->packetClass = 1;
	}
	packet->sequenceByte = (uint8_t)((headerWord >> 8) & 0x7fu);

	++recvHistoryCount;
	if (recvHistoryCount >= 128) {
		recvHistoryCount = 0;
	}
}

static void NetSession_UpdateLocalRecvSequence(unsigned int peerSlot, uint8_t packetClass,
											   uint8_t sequenceByte) {
	NetReliablePeerSlot* peer;

	if (peerSlot >= g_netSessionState.netSlotCount || peerSlot >= 40u) {
		return;
	}

	peer = &g_netSessionState.reliablePeerSlots[peerSlot];
	if (packetClass == 0) {
		peer->recvSeqChannelA = sequenceByte;
	} else if (packetClass == 2) {
		peer->recvSeqChannelB = sequenceByte;
	} else {
		peer->recvSeqDefault = sequenceByte;
	}
}

static void NetSession_QueueLocalReceivePacket(int directPlayId, const uint32_t* payload, int payloadSize,
											   uint32_t packetType, uint16_t headerWord) {
	NetQueuedPacket* packet;
	unsigned int peerSlot;
	int writeIndex;
	uint8_t packetClass;
	uint8_t sequenceByte;

	if (g_netRecvQueueCount >= 1024) {
		NetReliable_CompactLocalReceiveQueue();
		if (g_netRecvQueueCount >= 1024) {
			return;
		}
	}

	writeIndex = g_netRecvQueueWriteIndex;
	packet = &g_netSessionRecvQueue[writeIndex];
	memcpy(packet->payload, payload, (size_t)payloadSize);
	packet->directPlayId = g_netSessionState.localDplayId;
	packet->payloadSize = payloadSize;
	packet->aux = 0;
	packet->meta0 = 0;
	packet->queuedFlag = 0;

	peerSlot = NetReliable_FindOrCreatePeerSlot(g_netSessionState.localDplayId);
	sequenceByte = (uint8_t)((headerWord >> 8) & 0x7fu);
	if (packetType == 1 && g_gameConfig.asyncFlag == 1) {
		packetClass = 2;
	} else if (directPlayId == 0) {
		packetClass = 0;
	} else if (directPlayId == g_netSessionState.groupDplayId) {
		packetClass = 2;
	} else {
		packetClass = 1;
	}

	packet->packetClass = packetClass;
	packet->sequenceByte = sequenceByte;
	NetSession_UpdateLocalRecvSequence(peerSlot, packetClass, sequenceByte);

	++g_netRecvQueueCount;
	++writeIndex;
	g_netRecvQueueWriteIndex = writeIndex;
	if (writeIndex >= 1024) {
		g_netRecvQueueWriteIndex = 0;
	}
}

// FUNCTION: XWA 0x49C100
int NetSession_SendPacket(int directPlayId, const uint32_t* payload, int payloadSize) {
	uint8_t compactPacket[1024];
	uint32_t packetType;
	uint8_t compactType;
	int hasSizeTrailer;
	uint16_t headerWord;
	int compactSize;
	unsigned int peerSlot;

	if (payloadSize < 4) {
		return 0;
	}
	if (g_netSessionState.dplayInterface == NULL) {
		return 1;
	}

	packetType = payload[0];
	compactType = (uint8_t)(packetType & 0x7fu);
	hasSizeTrailer = NetSession_PacketHasSizeTrailer(packetType);
	peerSlot = 40u;

	if (packetType == 1 && g_gameConfig.asyncFlag == 1) {
		headerWord =
			(uint16_t)(compactType | 0x8080u | ((uint16_t)(g_netSessionState.groupSeqCounter & 0x7f) << 8));
		Net_IncrementSeq7(&g_netSessionState.groupSeqCounter);
		compactSize =
			NetSession_BuildCompactPacket(compactPacket, headerWord, payload, payloadSize, hasSizeTrailer);
		if (hasSizeTrailer) {
			compactSize = NetSession_AppendPendingPayload(compactPacket, compactSize,
														  &g_netSessionState.groupPendingPayload);
		}
		NetSession_UpdatePendingPayload(&g_netSessionState.groupPendingPayload, 1, payload, payloadSize);
	} else if (directPlayId == 0) {
		headerWord =
			(uint16_t)(compactType | ((uint16_t)(g_netSessionState.broadcastSeqCounter & 0x7f) << 8));
		Net_IncrementSeq7(&g_netSessionState.broadcastSeqCounter);
		compactSize =
			NetSession_BuildCompactPacket(compactPacket, headerWord, payload, payloadSize, hasSizeTrailer);
		if (hasSizeTrailer) {
			compactSize = NetSession_AppendPendingPayload(compactPacket, compactSize,
														  &g_netSessionState.broadcastPendingPayload);
		}
		NetSession_UpdatePendingPayload(&g_netSessionState.broadcastPendingPayload, compactType, payload,
										payloadSize);
	} else if (directPlayId == g_netSessionState.groupDplayId) {
		headerWord =
			(uint16_t)(compactType | 0x8080u | ((uint16_t)(g_netSessionState.groupSeqCounter & 0x7f) << 8));
		Net_IncrementSeq7(&g_netSessionState.groupSeqCounter);
		compactSize =
			NetSession_BuildCompactPacket(compactPacket, headerWord, payload, payloadSize, hasSizeTrailer);
		if (hasSizeTrailer) {
			compactSize = NetSession_AppendPendingPayload(compactPacket, compactSize,
														  &g_netSessionState.groupPendingPayload);
		}
		NetSession_UpdatePendingPayload(&g_netSessionState.groupPendingPayload, compactType, payload,
										payloadSize);
	} else {
		peerSlot = NetReliable_FindOrCreatePeerSlot(directPlayId);
		headerWord = (uint16_t)(compactType | 0x8000u);
		if (peerSlot < g_netSessionState.netSlotCount && peerSlot < 40u) {
			NetReliablePeerSlot* peer;

			peer = &g_netSessionState.reliablePeerSlots[peerSlot];
			headerWord = (uint16_t)(headerWord | ((uint16_t)(peer->sendSeq & 0x7f) << 8));
			Net_IncrementSeq7(&peer->sendSeq);
		}

		compactSize =
			NetSession_BuildCompactPacket(compactPacket, headerWord, payload, payloadSize, hasSizeTrailer);
		if (hasSizeTrailer && peerSlot < 40u) {
			NetReliablePeerSlot* peer;

			peer = &g_netSessionState.reliablePeerSlots[peerSlot];
			memcpy(compactPacket + compactSize, &peer->lastPiggybackType, (size_t)peer->piggybackLength);
			compactSize += peer->piggybackLength;
		}

		if (peerSlot < 40u) {
			NetReliablePeerSlot* peer;

			peer = &g_netSessionState.reliablePeerSlots[peerSlot];
			peer->lastPiggybackType = compactType;
			memcpy(peer->piggybackPayload, payload + 1, (size_t)(payloadSize - 4));
			peer->piggybackLength = payloadSize - 3;
		}
	}

	if ((packetType != 1 || g_gameConfig.asyncFlag != 1) && directPlayId != g_netSessionState.localDplayId) {
		NetSession_RecordSendHistory(directPlayId, payload, payloadSize, headerWord);
	}

	if (directPlayId == g_netSessionState.localDplayId || directPlayId == 0 ||
		directPlayId == g_netSessionState.groupDplayId || g_netSessionState.dplayInterface == NULL) {
		NetSession_QueueLocalReceivePacket(directPlayId, payload, payloadSize, packetType, headerWord);
	}

	if (g_netSessionState.dplayInterface == NULL) {
		return 1;
	}
	if (directPlayId == g_netSessionState.localDplayId) {
		return 1;
	}

	return Net_SendPacketAndFlush(directPlayId, compactPacket, (unsigned int)compactSize) == 1;
}

// FUNCTION: XWA 0x49C0B0
int NetSession_BroadcastPacketToPlayers(const uint32_t* payload, int payloadSize) {
	int result;
	int playerIdx;

	result = g_netSessionState.playerCount;
	playerIdx = 0;
	while (playerIdx < result) {
		result = g_netSessionState.players[playerIdx].dplayId;
		if (result != 0 && g_netSessionState.players[playerIdx].activeFlag != 0) {
			NetSession_SendPacket(g_netSessionState.players[playerIdx].dplayId, payload, payloadSize);
		}
		result = g_netSessionState.playerCount;
		++playerIdx;
	}

	return result;
}

static int NetSession_NextSequenceByte(int sequenceByte) {
	++sequenceByte;
	if (sequenceByte > 127) {
		sequenceByte = 0;
	}
	return sequenceByte;
}

static int* NetSession_PrevReceiveSequencePtr(NetReliablePeerSlot* peer, uint8_t packetClass) {
	if (packetClass == 0) {
		return &peer->prevRecvSeqChannelA;
	}
	if (packetClass == 2) {
		return &peer->prevRecvSeqChannelB;
	}
	return &peer->prevRecvSeqDefault;
}

static int NetSession_IsStaleQueuedSequence(int sequenceByte, int expectedSequence) {
	int delta;

	delta = sequenceByte - expectedSequence;
	if (delta < 0) {
		delta += 128;
	}
	return delta >= 100;
}

static int* NetSession_ReturnQueuedPacket(unsigned int queueIndex, int* outSenderDirectPlayId,
										  int* outPayloadSize) {
	memcpy(&g_netSessionState.receiveScratchPacket, &g_netSessionRecvQueue[queueIndex],
		   sizeof(g_netSessionState.receiveScratchPacket));
	*outSenderDirectPlayId = g_netSessionState.receiveScratchPacket.directPlayId;
	*outPayloadSize = g_netSessionState.receiveScratchPacket.payloadSize;
	NetReliable_RemoveQueuedPacket(queueIndex);
	return (int*)g_netSessionState.receiveScratchPacket.payload;
}

// FUNCTION: XWA 0x49C970
int* NetSession_ReceiveGamePacket(int* outSenderDpid, int* outAux) {
	int* packet;

	while (1) {
		packet = NetSession_ReceivePacket(outSenderDpid, outAux);
		if (packet == NULL || *outSenderDpid != 0) {
			break;
		}
		NetSession_HandleHandshakePacket(*packet, packet);
	}

	return packet;
}

// FUNCTION: XWA 0x49E320
int* NetSession_WaitForGamePacket(int* outDpid, int* outAux, int timeoutSeconds) {
	uint32_t timeoutMs;
	uint32_t startTime;
	int senderDpid;
	int aux;
	int* packet;

	timeoutMs = (uint32_t)(timeoutSeconds * 1000);
	startTime = timeGetTime();

	while (1) {
		if (timeGetTime() - startTime > timeoutMs) {
			return NULL;
		}

		while (1) {
			packet = NetSession_ReceivePacket(&senderDpid, &aux);
			if (packet == NULL || senderDpid != 0) {
				break;
			}
			NetSession_HandleHandshakePacket(*packet, packet);
		}

		if (packet != NULL) {
			break;
		}
	}

	*outDpid = senderDpid;
	*outAux = aux;
	return packet;
}

// FUNCTION: XWA 0x49C9A0
int* NetSession_ReceivePacket(int* outSenderDirectPlayId, int* outPayloadSize) {
	unsigned int queueIndex;
	int remaining;

	if (g_netSessionState.dplayInterface != NULL) {
		/*
		 * The original function decodes IDirectPlay4::Receive packets here.
		 * Aeron owns the modern transport; this hook lets that boundary enqueue
		 * packets before the recovered reliable-ordering logic below runs.
		 *
		 * TODO: Port compact packet decoding if Aeron preserves the original
		 * DirectPlay wire stream instead of enqueuing expanded packets.
		 */
		Net_PumpIncomingPackets();
	}

	remaining = g_netRecvQueueCount;
	queueIndex = (unsigned int)g_netRecvQueueReadIndex;
	while (remaining > 0) {
		NetQueuedPacket* packet;

		packet = &g_netSessionRecvQueue[queueIndex];
		if (packet->directPlayId == 0) {
			if ((int)queueIndex == g_netRecvQueueReadIndex) {
				return NetSession_ReturnQueuedPacket(queueIndex, outSenderDirectPlayId, outPayloadSize);
			}
		} else {
			unsigned int peerSlot;
			NetReliablePeerSlot* peer;
			int* prevSequence;
			int expectedSequence;
			int sequenceByte;
			uint32_t packetType;

			peerSlot = NetReliable_FindOrCreatePeerSlot(packet->directPlayId);
			if (peerSlot >= g_netSessionState.netSlotCount || peerSlot >= 40u) {
				if (NetReliable_RemoveQueuedPacket(queueIndex) != 0) {
					++queueIndex;
					if (queueIndex >= 1024u) {
						queueIndex = 0;
					}
				}
				--remaining;
				continue;
			}

			peer = &g_netSessionState.reliablePeerSlots[peerSlot];
			prevSequence = NetSession_PrevReceiveSequencePtr(peer, packet->packetClass);
			expectedSequence = NetSession_NextSequenceByte(*prevSequence);
			sequenceByte = packet->sequenceByte;
			packetType = 0;
			if (packet->payloadSize >= 4) {
				memcpy(&packetType, packet->payload, sizeof(packetType));
			}

			if (NetSession_IsStaleQueuedSequence(sequenceByte, expectedSequence)) {
				if (NetReliable_RemoveQueuedPacket(queueIndex) != 0) {
					++queueIndex;
					if (queueIndex >= 1024u) {
						queueIndex = 0;
					}
				}
				--remaining;
				continue;
			}

			if (sequenceByte == expectedSequence || (packetType == 1 && g_gameConfig.asyncFlag == 1)) {
				peer->lastActivityMs = timeGetTime();
				*prevSequence = sequenceByte;
				g_netLastDeliveredRecvSequence = sequenceByte;
				if (packetType != 1 || g_gameConfig.asyncFlag != 1) {
					++peer->packetCount;
				}
				return NetSession_ReturnQueuedPacket(queueIndex, outSenderDirectPlayId, outPayloadSize);
			}

			if (packet->queuedFlag != 0 && (int)queueIndex == g_netRecvQueueReadIndex) {
				if (NetReliable_RemoveQueuedPacket(queueIndex) != 0) {
					++queueIndex;
					if (queueIndex >= 1024u) {
						queueIndex = 0;
					}
				}
				--remaining;
				continue;
			}
		}

		++queueIndex;
		if (queueIndex >= 1024u) {
			queueIndex = 0;
		}
		--remaining;
	}

	return NULL;
}

// FUNCTION: XWA 0x52D6F0
int Net_SendDirectPlayPacket(int destPlayerId, const void* packet, int packetSize, int flags) {
	uint8_t compactPacket[1024];
	uint32_t packetType;
	uint16_t compactType;
	uint8_t* payloadDst;
	int headerSize;
	int payloadSize;
	int compactSize;

	(void)flags;
	if (g_netSessionState.dplayInterface == NULL) {
		return 1;
	}

	packetType = *(const uint32_t*)packet;
	compactType = (uint16_t)(packetType & 0x7fu);
	if (destPlayerId != 0 && destPlayerId == g_netSessionState.groupDplayId) {
		compactType = (uint16_t)(compactType | 0x8080u);
	}

	payloadSize = packetSize - 4;
	ByteOrder_WriteU16Le(compactPacket, compactType);
	payloadDst = compactPacket + 2;
	headerSize = 2;
	if (packetType < 0x3cu || packetType >= 0x40u) {
		if (NetSession_ExitStub((int)packetType) == 0) {
			ByteOrder_WriteU16Le(compactPacket + 2, (uint16_t)payloadSize);
			payloadDst = compactPacket + 4;
			headerSize = 4;
		}
	}

	memcpy(payloadDst, (const uint8_t*)packet + 4, (size_t)payloadSize);
	compactSize = headerSize + payloadSize;
	if (packetType < 0x3cu || packetType >= 0x40u) {
		payloadDst[payloadSize] = 57;
		++compactSize;
	}

	if (destPlayerId == g_netSessionState.localDplayId) {
		return 1;
	}

	return Net_SendPacketAndFlush(destPlayerId, compactPacket, (unsigned int)compactSize) == 1;
}

// FUNCTION: XWA 0x52D840
int Net_SendSequencedDirectPlayPacket(int destPlayerId, int sequenceMode, int sequenceId, const void* packet,
									  unsigned int packetSize) {
	uint16_t compactPacket[512];
	char debugLabel[256];
	uint32_t packetType;
	uint8_t packetTypeByte;
	uint16_t headerWord;
	uint8_t* compactBytes;
	uint8_t* payloadDst;
	int compactSize;
	int hasSizeTrailer;
	int sendResult;

	sendResult = 0;
	if (g_netDirectPlayInterface == NULL) {
		return 1;
	}

	packetType = *(const uint32_t*)packet;
	packetTypeByte = (uint8_t)packetType;
	packetTypeByte = (uint8_t)(packetTypeByte & 0x7fu);
	headerWord = packetTypeByte;
	hasSizeTrailer = packetType < 0x3cu || packetType >= 0x40u;
	if (sequenceMode == 0) {
		sprintf(debugLabel, "(RSB %u) ", sequenceId);
	} else if (sequenceMode == 2) {
		sprintf(debugLabel, "(RSG %u) ", sequenceId);
	} else {
		sprintf(debugLabel, "(RSS %u) ", sequenceId);
	}
	(void)debugLabel;

	compactBytes = (uint8_t*)compactPacket;
	compactBytes[2] = (uint8_t)sequenceMode;
	payloadDst = compactBytes + 3;
	compactSize = 3;
	compactPacket[0] = (uint16_t)(headerWord | (((uint16_t)sequenceId & 0x7fu) << 8) | 0x80u);

	if (hasSizeTrailer && NetSession_ExitStub((int)packetType) == 0) {
#ifdef XWA_MODERN
		ByteOrder_WriteU16Le(payloadDst, (uint16_t)(packetSize - 4u));
#else
		*(uint16_t*)payloadDst = (uint16_t)(packetSize - 4u);
#endif
		payloadDst = compactBytes + 5;
		compactSize = 5;
	}

	memcpy(payloadDst, (const uint32_t*)packet + 1, packetSize - 4u);
	compactSize += packetSize - 4u;
	if (hasSizeTrailer) {
		payloadDst[packetSize - 4u] = 57;
		++compactSize;
	}

	if (destPlayerId != g_netDirectPlayRuntimeState.localPlayer.playerId) {
		sendResult = ((XwaDirectPlay4*)g_netDirectPlayInterface)
						 ->lpVtbl->Send((XwaDirectPlay4*)g_netDirectPlayInterface,
										g_netDirectPlayRuntimeState.localPlayer.playerId, destPlayerId, 0,
										compactPacket, (uint32_t)compactSize);
	} else if (g_netDirectPlayRuntimeState.recvQueueCount < 1024) {
		memcpy(g_netDirectPlayRuntimeState.recvQueue[g_netDirectPlayRuntimeState.recvQueueWriteIndex].payload,
			   packet, packetSize);
		g_netDirectPlayRuntimeState.recvQueue[g_netDirectPlayRuntimeState.recvQueueWriteIndex].directPlayId =
			g_netDirectPlayRuntimeState.localPlayer.playerId;
		g_netDirectPlayRuntimeState.recvQueue[g_netDirectPlayRuntimeState.recvQueueWriteIndex].payloadSize =
			(int32_t)packetSize;
		g_netDirectPlayRuntimeState.recvQueue[g_netDirectPlayRuntimeState.recvQueueWriteIndex].packetClass =
			(uint8_t)sequenceMode;
		g_netDirectPlayRuntimeState.recvQueue[g_netDirectPlayRuntimeState.recvQueueWriteIndex].sequenceByte =
			(uint8_t)sequenceId;
		g_netDirectPlayRuntimeState.recvQueue[g_netDirectPlayRuntimeState.recvQueueWriteIndex].queuedFlag = 1;
		g_netDirectPlayRuntimeState.recvQueue[g_netDirectPlayRuntimeState.recvQueueWriteIndex].aux = 0;
		g_netDirectPlayRuntimeState.recvQueue[g_netDirectPlayRuntimeState.recvQueueWriteIndex].meta0 = 0;

		++g_netDirectPlayRuntimeState.recvQueueCount;
		++g_netDirectPlayRuntimeState.recvQueueWriteIndex;
		if (g_netDirectPlayRuntimeState.recvQueueWriteIndex >= 1024) {
			g_netDirectPlayRuntimeState.recvQueueWriteIndex = 0;
		}
	}

	return sendResult == 0;
}

// FUNCTION: XWA 0x49E040
int NetSession_SendCompactGamePacket(int destDplayId, const uint32_t* packet, int packetSize) {
	return Net_SendDirectPlayPacket(destDplayId, packet, packetSize, 0);
}

// FUNCTION: XWA 0x49E160
int NetSession_SendSequencedGamePacket(int destDplayId, uint8_t localSeq, uint8_t remoteSeq,
									   const uint32_t* packet, unsigned int packetSize) {
	uint16_t compactPacket[512];
	uint8_t* compactBytes;
	uint32_t packetType;
	uint8_t packetTypeByte;
	uint16_t headerWord;
	int hasSizeTrailer;
	uint8_t* payloadDst;
	unsigned int compactSize;
	int sendResult;

	sendResult = 0;

	if (g_netSessionState.dplayInterface == NULL) {
		return 1;
	}

	packetType = packet[0];
	packetTypeByte = (uint8_t)packetType;
	packetTypeByte = (uint8_t)(packetTypeByte & 0x7fu);
	headerWord = packetTypeByte;
	hasSizeTrailer = packetType < 0x3cu || packetType >= 0x40u;

	compactBytes = (uint8_t*)compactPacket;
	headerWord = (uint16_t)(headerWord | (((uint16_t)remoteSeq & 0x7fu) << 8));
	headerWord = (uint16_t)(headerWord | 0x80u);
	compactPacket[0] = headerWord;
	compactBytes[2] = localSeq;
	payloadDst = compactBytes + 3;
	compactSize = 3u;

	if (hasSizeTrailer) {
		ByteOrder_WriteU16Le(compactBytes + 3, (uint16_t)(packetSize - 4u));
		payloadDst = compactBytes + 5;
		compactSize = 5u;
	}

	memcpy(payloadDst, packet + 1, packetSize - 4u);
	compactSize += packetSize - 4u;
	if (hasSizeTrailer) {
		payloadDst[packetSize - 4u] = 57;
		++compactSize;
	}

	if (destDplayId != g_netSessionState.localDplayId) {
		XwaDirectPlay4* dplay;

		dplay = (XwaDirectPlay4*)g_netSessionState.dplayInterface;
		sendResult = dplay->lpVtbl->Send(dplay, g_netSessionState.localDplayId, destDplayId, 0, compactPacket,
										 compactSize);
	} else {
		if (g_netRecvQueueCount < 1024 ||
			(NetReliable_CompactLocalReceiveQueue(), g_netRecvQueueCount < 1024)) {
			int writeIndex;

			writeIndex = g_netRecvQueueWriteIndex;
			memcpy(g_netSessionRecvQueue[writeIndex].payload, packet, packetSize);
			g_netSessionRecvQueue[writeIndex].directPlayId = g_netSessionState.localDplayId;
			g_netSessionRecvQueue[writeIndex].payloadSize = (int32_t)packetSize;
			g_netSessionRecvQueue[writeIndex].packetClass = localSeq;
			g_netSessionRecvQueue[writeIndex].sequenceByte = remoteSeq;
			g_netSessionRecvQueue[writeIndex].queuedFlag = 1;
			g_netSessionRecvQueue[writeIndex].aux = 0;
			g_netSessionRecvQueue[writeIndex].meta0 = 0;

			++g_netRecvQueueCount;
			++writeIndex;
			g_netRecvQueueWriteIndex = writeIndex;
			if (writeIndex >= 1024) {
				g_netRecvQueueWriteIndex = 0;
			}
		}
	}

	return sendResult == 0;
}

// FUNCTION: XWA 0x52EFE0
int Net_GetHostPlayerId(void) {
	/* TODO: Reimplement Net_GetHostPlayerId @ 0x52EFE0. */
	return g_mpRoster[0].playerId;
}

// FUNCTION: XWA 0x52F170
int Net_ClearPlayerReadyFlagWithLockGuard(int playerId) {
	(void)playerId;

	/* TODO: Reimplement Net_ClearPlayerReadyFlagWithLockGuard @ 0x52F170. */
	return 1;
}

// FUNCTION: XWA 0x52F550
int NetSession_CompactReliablePeerSlotsForRoster(void) {
	/* TODO: Reimplement NetSession_CompactReliablePeerSlotsForRoster @ 0x52F550. */
	return 1;
}

// FUNCTION: XWA 0x49E950
void NetReliable_ResetRecvQueueState(void) {
	uint32_t slot;

	g_netRecvQueueReadIndex = g_netRecvQueueWriteIndex;
	g_netRecvQueueCount = 0;

	for (slot = 0; slot < g_netSessionState.netSlotCount; ++slot) {
		NetReliablePeerSlot* peer;

		peer = &g_netSessionState.reliablePeerSlots[slot];
		peer->prevRecvSeqDefault = peer->recvSeqDefault;
		peer->prevRecvSeqChannelA = peer->recvSeqChannelA;
		peer->prevRecvSeqChannelB = peer->recvSeqChannelB;
		peer->lastActivityMs = timeGetTime();
	}
}

// FUNCTION: XWA 0x49E890
unsigned int NetReliable_FindOrCreatePeerSlot(int directPlayId) {
	unsigned int slot;
	NetReliablePeerSlot* peer;

	slot = 0;
	if (g_netSessionState.netSlotCount != 0) {
		peer = g_netSessionState.reliablePeerSlots;
		while (peer->directPlayId != directPlayId) {
			++slot;
			++peer;
			if (slot >= g_netSessionState.netSlotCount) {
				break;
			}
		}
	}

	if (slot == g_netSessionState.netSlotCount && slot < 40u) {
		peer = &g_netSessionState.reliablePeerSlots[slot];
		peer->directPlayId = directPlayId;
		peer->prevRecvSeqDefault = 127;
		peer->prevRecvSeqChannelA = 127;
		peer->prevRecvSeqChannelB = 127;
		peer->recvSeqDefault = 127;
		peer->recvSeqChannelA = 127;
		peer->recvSeqChannelB = 127;
		peer->sendSeq = 0;
		peer->lastPiggybackType = 57;
		peer->piggybackLength = 1;
		peer->lastActivityMs = timeGetTime();
		peer->packetCount = 0;
		peer->packetDropCount = 0;
		++g_netSessionState.netSlotCount;
	}

	return slot;
}

// FUNCTION: XWA 0x52FC20
unsigned int Net_AddSequence(int directPlayId) {
	unsigned int slot;
	char debugMessage[256];

	for (slot = 0; slot < g_netSequenceCount; ++slot) {
		if (g_netRuntimeReliablePeerSlots[slot].directPlayId == directPlayId) {
			return slot;
		}
	}

	if (slot == g_netSequenceCount && slot < 40u) {
		g_netRuntimeReliablePeerSlots[slot].directPlayId = directPlayId;
		g_netRuntimeReliablePeerSlots[slot].prevRecvSeqDefault = 127;
		g_netRuntimeReliablePeerSlots[slot].prevRecvSeqChannelA = 127;
		g_netRuntimeReliablePeerSlots[slot].prevRecvSeqChannelB = 127;
		g_netRuntimeReliablePeerSlots[slot].recvSeqDefault = 127;
		g_netRuntimeReliablePeerSlots[slot].recvSeqChannelA = 127;
		g_netRuntimeReliablePeerSlots[slot].recvSeqChannelB = 127;
		g_netRuntimeReliablePeerSlots[slot].sendSeq = 0;
		g_netRuntimeReliablePeerSlots[slot].lastPiggybackType = 57;
		g_netRuntimeReliablePeerSlots[slot].piggybackLength = 1;
		g_netRuntimeReliablePeerSlots[slot].lastActivityMs = GetTickCount();
		g_netRuntimeReliablePeerSlots[slot].lastKeepaliveMs = GetTickCount();
		g_netRuntimeReliablePeerSlots[slot].packetCount = 0;
		g_netRuntimeReliablePeerSlots[slot].packetDropCount = 0;
		g_netRuntimeReliablePeerSlots[slot].packetRetryCount = 0;
		++g_netSequenceCount;
		sprintf(debugMessage, "SAdding new net sequence %u\n", (unsigned int)directPlayId);
	}

	return slot;
}

// FUNCTION: XWA 0x52F850
int Net_CheckAndRecordIncomingSequence(int playerId, int sequenceId, int useChannel0, int useChannel2) {
	uint32_t oldSequenceCount;
	unsigned int slot;
	NetReliablePeerSlot* peer;
	int recvSeq;
	int delta;

	oldSequenceCount = g_netSequenceCount;
	slot = Net_AddSequence(playerId);
	if (oldSequenceCount != g_netSequenceCount || slot >= 40u) {
		return 0;
	}

	peer = &g_netRuntimeReliablePeerSlots[slot];
	if (useChannel0) {
		recvSeq = peer->recvSeqChannelA;
	} else if (useChannel2) {
		recvSeq = peer->recvSeqChannelB;
	} else {
		recvSeq = peer->recvSeqDefault;
	}

	delta = sequenceId - recvSeq;
	if (delta >= -64 && (delta <= 0 || delta >= 64)) {
		return 1;
	}

	if (useChannel0) {
		peer->recvSeqChannelA = sequenceId;
	} else if (useChannel2) {
		peer->recvSeqChannelB = sequenceId;
	} else {
		peer->recvSeqDefault = sequenceId;
	}
	return 0;
}

// FUNCTION: XWA 0x49E610
int NetReliable_IsDuplicateRecvSequence(int directPlayId, int sequence, int channelA, int channelB) {
	uint32_t savedSlotCount;
	unsigned int slot;
	NetReliablePeerSlot* peer;
	int recvSeq;
	int delta;

	savedSlotCount = g_netSessionState.netSlotCount;
	slot = NetReliable_FindOrCreatePeerSlot(directPlayId);
	if (savedSlotCount != g_netSessionState.netSlotCount || slot >= 40u) {
		return 0;
	}

	peer = &g_netSessionState.reliablePeerSlots[slot];
	if (channelA) {
		recvSeq = peer->recvSeqChannelA;
	} else if (channelB) {
		recvSeq = peer->recvSeqChannelB;
	} else {
		recvSeq = peer->recvSeqDefault;
	}

	delta = sequence - recvSeq;
	if (delta >= -64 && (delta <= 0 || delta >= 64)) {
		return 1;
	}

	if (channelA) {
		peer->recvSeqChannelA = sequence;
	} else if (channelB) {
		peer->recvSeqChannelB = sequence;
	} else {
		peer->recvSeqDefault = sequence;
	}
	return 0;
}

// FUNCTION: XWA 0x49E6D0
int NetReliable_FindQueuedRecvPacket(int unused, int remoteSeq, int wantType0, int wantType2, int peerSlot) {
	int queueIndex;
	int remaining;

	(void)unused;

	queueIndex = g_netRecvQueueReadIndex;
	remaining = g_netRecvQueueCount;
	while (remaining != 0) {
		NetQueuedPacket* packet;

		packet = &g_netSessionRecvQueue[queueIndex];
		if (packet->queuedFlag != 0) {
			uint8_t packetClass;
			int sequenceByte;
			int isClass0;
			int isClass2;
			uint32_t slot;

			packetClass = packet->packetClass;
			sequenceByte = packet->sequenceByte;
			isClass0 = packetClass == 0;
			isClass2 = packetClass == 2;

			slot = 0;
			while (slot < g_netSessionState.netSlotCount) {
				if (g_netSessionState.reliablePeerSlots[slot].directPlayId == packet->directPlayId) {
					break;
				}
				++slot;
			}

			if ((int)slot == peerSlot) {
				if (wantType0) {
					if (isClass0 && sequenceByte == remoteSeq) {
						return queueIndex;
					}
				} else if (wantType2) {
					if (isClass2 && sequenceByte == remoteSeq) {
						return queueIndex;
					}
				} else if (!isClass0 && !isClass2 && sequenceByte == remoteSeq) {
					return queueIndex;
				}
			}
		}

		++queueIndex;
		if ((unsigned int)queueIndex >= 1024u) {
			queueIndex = 0;
		}
		--remaining;
	}

	return -1;
}

// FUNCTION: XWA 0x49E7B0
int NetReliable_RemoveQueuedPacket(unsigned int queueIndex) {
	if ((int)queueIndex == g_netRecvQueueReadIndex) {
		++g_netRecvQueueReadIndex;
		--g_netRecvQueueCount;
		if (g_netRecvQueueReadIndex >= 1024) {
			g_netRecvQueueReadIndex = 0;
		}
		return 1;
	}

	{
		unsigned int dstIndex;
		unsigned int srcIndex;
		unsigned int endIndex;

		dstIndex = queueIndex;
		srcIndex = queueIndex + 1;
		if (srcIndex >= 1024u) {
			srcIndex = 0;
		}

		endIndex = (unsigned int)(g_netRecvQueueReadIndex + g_netRecvQueueCount);
		if (endIndex >= 1024u) {
			endIndex -= 1024u;
		}

		while (srcIndex != endIndex) {
			g_netSessionRecvQueue[dstIndex] = g_netSessionRecvQueue[srcIndex];
			++dstIndex;
			if (dstIndex >= 1024u) {
				dstIndex = 0;
			}
			++srcIndex;
			if (srcIndex >= 1024u) {
				srcIndex = 0;
			}
		}
	}

	--g_netRecvQueueCount;
	if (g_netRecvQueueWriteIndex == 0) {
		g_netRecvQueueWriteIndex = 1023;
	} else {
		--g_netRecvQueueWriteIndex;
	}
	return 0;
}

#if defined(_MSC_VER) && _MSC_VER <= 1100
#pragma function(memcpy)
#endif
// FUNCTION: XWA 0x49E9F0
int NetReliable_CompactLocalReceiveQueue(void) {
	unsigned int dstIndex;
	unsigned int srcIndex;
	unsigned int keptCount;
	unsigned int slot;

	dstIndex = (unsigned int)g_netRecvQueueReadIndex;
	srcIndex = (unsigned int)g_netRecvQueueReadIndex;
	keptCount = 0;

	while (g_netRecvQueueCount > 0) {
		if (g_netSessionRecvQueue[srcIndex].directPlayId == 0 ||
			g_netSessionRecvQueue[srcIndex].directPlayId == g_netSessionState.hostDplayId) {
			memcpy(&g_netSessionRecvQueue[dstIndex], &g_netSessionRecvQueue[srcIndex],
				   sizeof(g_netSessionRecvQueue[srcIndex]));
			++dstIndex;
			if (dstIndex >= 1024u) {
				dstIndex = 0;
			}
			++keptCount;
		}

		++srcIndex;
		if (srcIndex >= 1024u) {
			srcIndex = 0;
		}
		--g_netRecvQueueCount;
	}

	g_netRecvQueueCount = (int)keptCount;
	g_netRecvQueueWriteIndex = (int)dstIndex;

	for (slot = 0; slot < g_netSessionState.netSlotCount; ++slot) {
		if (g_netSessionState.reliablePeerSlots[slot].directPlayId != g_netSessionState.hostDplayId) {
			g_netSessionState.reliablePeerSlots[slot].prevRecvSeqDefault =
				g_netSessionState.reliablePeerSlots[slot].recvSeqDefault;
			g_netSessionState.reliablePeerSlots[slot].prevRecvSeqChannelA =
				g_netSessionState.reliablePeerSlots[slot].recvSeqChannelA;
			g_netSessionState.reliablePeerSlots[slot].prevRecvSeqChannelB =
				g_netSessionState.reliablePeerSlots[slot].recvSeqChannelB;
			g_netSessionState.reliablePeerSlots[slot].lastActivityMs = timeGetTime();
		}
	}

	return 1;
}
#if defined(_MSC_VER) && _MSC_VER <= 1100
#pragma intrinsic(memcpy)
#endif

// FUNCTION: XWA 0x49C960
int NetSession_GetLocalPlayerId(void) { return g_netSessionState.localPlayerId; }

// FUNCTION: XWA 0x49E3C0
int NetSession_GetHostDplayId(void) { return g_netSessionState.hostDplayId; }

// FUNCTION: XWA 0x49E3D0
int NetSession_GetLocalDplayId(void) { return g_netSessionState.localDplayId; }

// FUNCTION: XWA 0x49E5F0
int NetSession_StubReturnTrue(void) { return 1; }

// FUNCTION: XWA 0x49E600
int NetSession_ExitStub(int packetType) {
	(void)packetType;
	return 0;
}

// FUNCTION: XWA 0x49C950
int NetSession_GetPlayerCount(void) {
	if (g_netSessionState.playerCount == 0) {
		return 1;
	}

	return g_netSessionState.playerCount;
}

// FUNCTION: XWA 0x49E3E0
char* NetSession_GetPlayerName(int playerSlot) {
	int playerCount;
	int rosterIdx;

	playerCount = g_netSessionState.playerCount;
	if (playerCount == 1) {
		return g_netSessionState.localPlayerName;
	}

	rosterIdx = 0;
	while (rosterIdx < playerCount) {
		int directPlayId;
		int slot;

		directPlayId = g_netSessionState.players[rosterIdx].dplayId;
		slot = 0;
		while (slot < 8 && directPlayId != g_netSessionState.players[slot].dplayId) {
			++slot;
		}

		if (slot == playerSlot) {
			return g_netSessionState.players[rosterIdx].playerName;
		}

		++rosterIdx;
	}

	return g_emptyString;
}

// FUNCTION: XWA 0x49C930
SessionPlayerInfo* NetSession_GetPlayerRoster(int* outCount) {
	*outCount = g_netSessionState.playerCount;
	return g_netSessionState.players;
}

// FUNCTION: XWA 0x49E3A0
int NetSession_FindPlayerSlotByDpid(int dpid) {
	int slot;

	for (slot = 0; slot < 8; ++slot) {
		if (dpid == g_netSessionState.players[slot].dplayId) {
			break;
		}
	}
	return slot;
}

// FUNCTION: XWA 0x49E5D0
int NetSession_CountActivePlayers(void) {
	int count;
	int slot;

	count = 0;
	for (slot = 0; slot < 8; ++slot) {
		if (g_netSessionState.players[slot].activeFlag == 1) {
			++count;
		}
	}
	return count;
}

int NetReliable_GetPeerPacketDropCountByDpid(int directPlayId) {
	uint32_t slot;

	if (g_netSessionState.netSlotCount == 0) {
		return -1;
	}

	for (slot = 0; slot < g_netSessionState.netSlotCount; ++slot) {
		if (g_netSessionState.reliablePeerSlots[slot].directPlayId == directPlayId) {
			return g_netSessionState.reliablePeerSlots[slot].packetDropCount;
		}
	}

	return -1;
}

static int NetSession_FindStatePlayerIndexByDpid(int directPlayId) {
	int playerIdx;

	playerIdx = 0;
	while (playerIdx < g_netSessionState.playerCount) {
		if (g_netSessionState.players[playerIdx].dplayId == directPlayId) {
			return playerIdx;
		}
		++playerIdx;
	}

	return -1;
}

static void NetSession_ResetReliablePeerSlot(NetReliablePeerSlot* peer) {
	peer->directPlayId = 0;
	peer->prevRecvSeqDefault = 127;
	peer->prevRecvSeqChannelA = 127;
	peer->prevRecvSeqChannelB = 127;
	peer->recvSeqDefault = 127;
	peer->recvSeqChannelA = 127;
	peer->recvSeqChannelB = 127;
	peer->sendSeq = 0;
	peer->lastPiggybackType = 57;
	peer->piggybackLength = 1;
	peer->lastActivityMs = 0;
	peer->packetCount = 0;
	peer->packetDropCount = 0;
}

static int NetSession_RemoveReliablePeerSlotByDpid(int directPlayId) {
	uint32_t slotCount;
	uint32_t slot;

	slotCount = g_netSessionState.netSlotCount;
	slot = 0;
	while (slot < slotCount) {
		if (g_netSessionState.reliablePeerSlots[slot].directPlayId == directPlayId) {
			g_netSessionState.netSlotCount = slotCount - 1u;
			g_netSessionState.reliablePeerSlots[slot] = g_netSessionState.reliablePeerSlots[slotCount - 1u];
			NetSession_ResetReliablePeerSlot(
				&g_netSessionState.reliablePeerSlots[g_netSessionState.netSlotCount]);
			return 1;
		}
		++slot;
	}

	return 0;
}

static int NetSession_FillReliablePeerSummaryPacket(void) {
	uint8_t* dst;
	uint32_t slot;
	uint32_t slotCount;

	slotCount = g_netSessionState.netSlotCount;
	g_netSessionScratchPacket[0] = 59;
	g_netSessionScratchPacket[1] = 0;
	g_netSessionScratchPacket[2] = slotCount;
	g_netSessionScratchPacket[3] = timeGetTime();

	dst = (uint8_t*)g_netSessionScratchPacket + 16;
	for (slot = 0; slot < slotCount; ++slot) {
		NetReliablePeerSlot* peer;

		peer = &g_netSessionState.reliablePeerSlots[slot];
		memcpy(dst, &peer->directPlayId, sizeof(peer->directPlayId));
		dst[4] = (uint8_t)peer->prevRecvSeqChannelA;
		dst[5] = (uint8_t)peer->prevRecvSeqChannelB;
		dst[6] = (uint8_t)peer->recvSeqChannelA;
		dst[7] = (uint8_t)peer->recvSeqChannelB;
		dst += 8;
	}

	return (int)(8u * slotCount + 16u);
}

static void NetSession_RefreshPlayersFromPortRoster(void) {
	int playerCount;

	/* DirectPlay originally re-enumerated players here; the port keeps the roster in session state. */
	playerCount = g_netSessionState.playerCount;
	if (playerCount > 8) {
		playerCount = 8;
	}
	g_netSessionState.playerCount = playerCount;
}

static void NetSession_DeletePlayerFromGroupPortBoundary(int directPlayId) {
	(void)directPlayId;

	/* DirectPlay group membership is not modeled by Aeron; active flags carry this state. */
}

// FUNCTION: XWA 0x49BB50
void NetSession_HandleHandshakePacket(int packetOpcode, const int* packet) {
	if (packetOpcode == 3) {
		int handshakeState;

		if (g_netSessionState.localPlayerId == 0) {
			return;
		}

		handshakeState = packet[1];
		if (g_netSessionFlightHandshakeActive) {
			if (handshakeState == 1 && packet[2] != g_netSessionState.localDplayId) {
				int packetSize;
				uint64_t versionMarker;

				packetSize = NetSession_FillReliablePeerSummaryPacket();
				NetSession_SendPacket(packet[2], g_netSessionScratchPacket, packetSize);

				g_netSessionScratchPacket[0] = 58;
				g_netSessionScratchPacket[1] = (uint32_t)Mission_GetElapsedClockSeconds();
				versionMarker = 3157554u;
				memcpy((uint8_t*)g_netSessionScratchPacket + 8, &versionMarker, sizeof(versionMarker));
				NetSession_SendPacket(packet[2], g_netSessionScratchPacket, 16);
			}
			return;
		}

		if (handshakeState == 1) {
			if (packet[2] != g_netSessionState.localDplayId) {
				int packetSize;

				packetSize = NetSession_FillReliablePeerSummaryPacket();
				NetSession_SendPacket(packet[2], g_netSessionScratchPacket, packetSize);

				g_netSessionScratchPacket[0] = 58;
				g_netSessionScratchPacket[1] = 0;
				NetSession_SendPacket(packet[2], g_netSessionScratchPacket, 8);
			}

			NetSession_RefreshPlayersFromPortRoster();
			for (handshakeState = 0; handshakeState < g_netSessionState.playerCount; ++handshakeState) {
				if (g_netSessionState.players[handshakeState].dplayId != g_netSessionState.localDplayId) {
					NetSession_DeletePlayerFromGroupPortBoundary(
						g_netSessionState.players[handshakeState].dplayId);
				}
			}
		}
		return;
	}

	if (packetOpcode == 5) {
		if (g_netSessionState.localPlayerId == 0) {
			return;
		}

		if (g_netSessionFlightHandshakeActive) {
			if (packet[1] == 1) {
				int playerIdx;

				playerIdx = NetSession_FindStatePlayerIndexByDpid(packet[2]);
				if (playerIdx >= 0) {
					if (g_netSessionState.players[playerIdx].activeFlag != 0) {
						g_netSessionScratchPacket[0] = 13;
						NetSession_SendPacket(g_netSessionState.localDplayId, g_netSessionScratchPacket, 4);
					}
					NetSession_DeletePlayerFromGroupPortBoundary(packet[2]);
					g_netSessionState.players[playerIdx].activeFlag = 0;
				}
				NetSession_RemoveReliablePeerSlotByDpid(packet[2]);
			}
			return;
		}

		if (packet[1] == 1) {
			NetSession_RefreshPlayersFromPortRoster();
			NetSession_DeletePlayerFromGroupPortBoundary(packet[2]);
			NetSession_RemoveReliablePeerSlotByDpid(packet[2]);
		}
		return;
	}

	if (packetOpcode == 259 && packet[1] == 1) {
		int playerIdx;

		playerIdx = NetSession_FindStatePlayerIndexByDpid(packet[2]);
		if (playerIdx >= 0) {
			const char* playerName;

			playerName = (const char*)packet + 28;
			strcpy(g_netSessionState.players[playerIdx].playerName, playerName);
			strcpy(g_netSessionState.players[playerIdx].sessionName, playerName + strlen(playerName) + 1);
			g_netSessionState.players[playerIdx].playerName[12] = '\0';
			g_netSessionState.players[playerIdx].sessionName[12] = '\0';
		}
	}
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

// FUNCTION: XWA 0x49AFC0
int NetSession_InitGameSession(const char* sessionName, const char* pilotName, int localId,
							   const char* mpGameName, const char* networkType, int numHumanPlayers,
							   int inProgressLaunch) {
	int slot;

	(void)mpGameName;
	(void)inProgressLaunch;

	/* TODO: Reimplement NetSession_InitGameSession @ 0x49AFC0. */
	g_netPlayerCount = numHumanPlayers > 0 ? numHumanPlayers : 1;
	if (g_netPlayerCount > 32) {
		g_netPlayerCount = 32;
	}

	memset(&g_netSessionState, 0, sizeof(g_netSessionState));
	memset(g_netPlayers, 0, sizeof(g_netPlayers));

	g_netSessionState.networkType = networkType;
	g_netSessionState.playerCount = numHumanPlayers > 0 ? numHumanPlayers : 1;
	if (g_netSessionState.playerCount > 8) {
		g_netSessionState.playerCount = 8;
	}
	g_netSessionState.localPlayerId = localId > 0 ? localId : 1;
	g_netSessionState.localDplayId = localId > 0 ? localId : 1;
	g_netSessionState.unusedLocalDplayIdMirror = g_netSessionState.localDplayId;
	g_netSessionState.hostDplayId = g_netSessionState.localDplayId;

	if (sessionName != 0) {
		int i;

		for (i = 0; i < 15 && sessionName[i] != '\0'; ++i) {
			g_netSessionState.sessionName[i] = sessionName[i];
			g_netSessionState.players[0].sessionName[i] = sessionName[i];
		}
		g_netSessionState.sessionName[i] = '\0';
		g_netSessionState.players[0].sessionName[i] = '\0';
	}
	if (pilotName != 0) {
		int i;

		for (i = 0; i < 15 && pilotName[i] != '\0'; ++i) {
			g_netSessionState.localPlayerName[i] = pilotName[i];
			g_netPlayers[0].playerName[i] = pilotName[i];
			g_netSessionState.players[0].playerName[i] = pilotName[i];
		}
		g_netSessionState.localPlayerName[i] = '\0';
		g_netPlayers[0].playerName[i] = '\0';
		g_netSessionState.players[0].playerName[i] = '\0';
	}
	for (slot = 0; slot < g_netSessionState.playerCount; ++slot) {
		int directPlayId;

		directPlayId = slot == 0 ? g_netSessionState.localDplayId : slot + 1;
		g_netPlayers[slot].playerId = directPlayId;
		g_netPlayers[slot].readyFlag = 1;
		g_netSessionState.players[slot].dplayId = directPlayId;
		g_netSessionState.players[slot].activeFlag = 1;
		if (slot != 0) {
			g_netSessionState.players[slot].sessionName[0] = '\0';
			g_netSessionState.players[slot].playerName[0] = '\0';
		}
	}
	return 1;
}

// FUNCTION: XWA 0x49B9D0
void NetSession_Shutdown(void) {
	/* TODO: Reimplement NetSession_Shutdown @ 0x49B9D0. */
	g_netPlayerCount = 0;
	g_netSessionState.playerCount = 0;
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

// FUNCTION: XWA 0x52FB50
int Net_SetPlayerNameWithLockGuard(int playerId, const char* longName, const char* shortName) {
	(void)playerId;
	(void)longName;
	(void)shortName;

	/* TODO: Reimplement Net_SetPlayerNameWithLockGuard @ 0x52FB50. */
	return 1;
}

// FUNCTION: XWA 0x52FBD0
int Net_ResetRosterToLocalPlayerWithLockGuard(void) {
	/* TODO: Reimplement Net_ResetRosterToLocalPlayerWithLockGuard @ 0x52FBD0. */
	return 1;
}
