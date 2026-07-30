#include "xwa/flight/net_session.h"

#include "xwa/config/game_config.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/frontend/net_transport.h"
#include "xwa/net/directplay_private.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/string.h"
#include "xwa/util/time.h"

#include <string.h>

static uint8_t Net_IncrementSeq7(int* sequence) {
	uint8_t value = (uint8_t)*sequence;

	++*sequence;
	if (*sequence >= 128) {
		*sequence = 0;
	}

	return value;
}

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
// GLOBAL: XWA 0x694078
uint32_t g_netSessionScratchPacket[128];
// GLOBAL: XWA 0x8C1648
int g_netSessionFlightHandshakeActive;

// FLAGS: /O2
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

// FUNCTION: XWA 0x49E9B0
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
