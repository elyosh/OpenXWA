#include "xwa/frontend/net_transport.h"

#include "xwa/flight/net_session.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/net/directplay_private.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/time.h"

#include <stdio.h>
#include <string.h>

#ifndef XWA_MODERN
void Net_ShutdownDirectPlaySessionEx(int suppressRestart, int allowJoinAbortExit);
#endif

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
// GLOBAL: XWA 0xAB6819
NetReliablePeerSlot g_netRuntimeReliablePeerSlots[40];
// GLOBAL: XWA 0xABC71F
unsigned short g_networkPort;
// GLOBAL: XWA 0xABC721
static uint8_t g_netSerialPortSettings[5];
// GLOBAL: XWA 0xABC311
uint32_t g_netSequenceCount;

#ifndef XWA_MODERN
#pragma pack(push, 1)
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
#pragma pack(pop)

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

// FLAGS: /O2 /G6
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
	g_networkPort = *port;
	return 1;
}

// FUNCTION: XWA 0x531690
int Net_SetSerialPortSettings(const uint8_t* serialSettings) {
	memcpy(g_netSerialPortSettings, serialSettings, sizeof(g_netSerialPortSettings));
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
				localPlayerId = ((XwaDirectPlay4*)g_netDirectPlayInterface)
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
									   &savedHostPeer.lastPiggybackType, savedHostPeer.piggybackLength);
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
	int wasBackBufferLocked;
	int playerIndex;

	if (g_netDirectPlayInterface == NULL) {
		return 0;
	}

	wasBackBufferLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();

	for (playerIndex = 0; playerIndex < g_netPlayerCount; ++playerIndex) {
		if (g_netPlayers[playerIndex].playerId == playerId) {
			break;
		}
	}

	if (playerIndex == g_netPlayerCount) {
		if (wasBackBufferLocked) {
			g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
		}
		return 0;
	}

	g_netPlayers[playerIndex].readyFlag = 1;
	if (wasBackBufferLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}
	return 1;
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

#ifndef XWA_MODERN
void Net_ShutdownDirectPlaySessionEx(int suppressRestart, int allowJoinAbortExit);
#endif

// FUNCTION: XWA 0x52BBE0
void Net_ShutdownDirectPlaySession(void) {
#ifdef XWA_MODERN
	NetSession_Shutdown();
#else
	Net_ShutdownDirectPlaySessionEx(0, 1);
#endif
}

// FUNCTION: XWA 0x52BBD0
void Net_ShutdownDirectPlaySessionForQuit(void) {
#ifdef XWA_MODERN
	Net_ShutdownDirectPlaySession();
#else
	Net_ShutdownDirectPlaySessionEx(1, 1);
#endif
}

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

// FUNCTION: XWA 0x52FB50
int Net_SetPlayerNameWithLockGuard(int playerId, const char* longName, const char* shortName) {
	XwaDirectPlay4* dplay;
	XwaDirectPlayName name;
	int wasBackBufferLocked;
	HRESULT result;

	wasBackBufferLocked = g_backBufferLocked.word & 0xff;
	FrontendDisplay_UnlockBackBuffer();
	dplay = (XwaDirectPlay4*)g_netDirectPlayInterface;
	if (dplay == NULL) {
		return 0;
	}

	memset(&name, 0, sizeof(name));
	name.longName = longName;
	name.size = sizeof(name);
	name.shortName = shortName;
	result = dplay->lpVtbl->SetPlayerName(dplay, (uint32_t)playerId, &name, 0);

	if (wasBackBufferLocked) {
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	}

	return result == 0;
}

// FUNCTION: XWA 0x52FBD0
int Net_ResetRosterToLocalPlayerWithLockGuard(void) {
	/* TODO: Reimplement Net_ResetRosterToLocalPlayerWithLockGuard @ 0x52FBD0. */
	return 1;
}
