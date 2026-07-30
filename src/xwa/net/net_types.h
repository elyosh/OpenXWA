#ifndef XWA_NET_NET_TYPES_H
#define XWA_NET_NET_TYPES_H

#include <stddef.h>
#include <stdint.h>

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

typedef char xwa_guid_size[(sizeof(XwaGuid) == 0x10) ? 1 : -1];
typedef char xwa_net_pending_payload_size[(sizeof(NetPendingPayload) == 0x208) ? 1 : -1];
typedef char xwa_net_queued_packet_size[(sizeof(NetQueuedPacket) == 0x210) ? 1 : -1];
typedef char xwa_net_reliable_peer_slot_size[(sizeof(NetReliablePeerSlot) == 0x238) ? 1 : -1];
typedef char xwa_net_reliable_peer_slot_packet_drop_count_offset
	[(offsetof(NetReliablePeerSlot, packetDropCount) == 0x22C) ? 1 : -1];

#endif
