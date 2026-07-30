#ifndef XWA_FLIGHT_FLIGHT_SYNC_H
#define XWA_FLIGHT_FLIGHT_SYNC_H

#include "xwa/flight/flight.h"

#ifdef __cplusplus
extern "C" {
#endif

void FlightSync_QueuePredictedRemoteInputFrames(int predictedFrameDelta);
void FlightSync_DiscardPredictedInputFrames(int playerIdx);
InputFrame* FlightSync_InsertInputFrame(int playerIdx, int timestamp, const FlightInputFrameRecord* input);
InputFrame* FlightSync_FindLastNonzeroInputFrame(int playerIdx);
void FlightSync_ResetRemotePlayerRenderSmoothing(void);
void FlightSync_CaptureRemotePlayerRenderSamples(void);
void FlightSync_ApplyRemotePlayerRenderSmoothing(void);
void FlightSync_ApplyWorldMessagePacket(uint8_t* packet);
void FlightSync_HandleWorldChecksumPacket(int senderDpid, const int* packet);
void FlightSync_HandleServerChecksumPacket(const uint32_t* packet);
void FlightSync_CopyWorldStateResyncChunk(const void* chunkData, int dstOffset, uint32_t chunkSize);
void FlightSync_ReplayResyncMessages(unsigned int savedWorldStateSize, int serverTickTime);
void FlightSync_ResetWorldMessageBufferCursor(void);

#ifdef __cplusplus
}
#endif

#endif
