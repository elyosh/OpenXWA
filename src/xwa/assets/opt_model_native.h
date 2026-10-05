#ifndef XWA_ASSETS_OPT_MODEL_NATIVE_H
#define XWA_ASSETS_OPT_MODEL_NATIVE_H

/* Port: native layout behind an OptimizedPolyObject handle. The original
 * fixed up raw pointers inside the serialized bytes; the port keeps the bytes
 * and builds a node graph over them. OptimizedPolyObject is the first member,
 * so a locked model handle can be read as a NativeOptimizedPolyObject. */

#include "xwa/assets/opt_model.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct XwaOptSynthData;

typedef struct NativeOptNode {
	OptNode node;
	uint32_t serializedAddress;
	uint32_t param2Address;
} NativeOptNode;

typedef struct NativeOptimizedPolyObject {
	OptimizedPolyObject model;
	uint8_t* serializedData;
	int serializedSize;
	uint32_t serializedBase;
	uint32_t rootNodesAddress;
	NativeOptNode** nodes;
	int nodeCount;
	int nodeCapacity;
#ifdef XWA_MODERN
	struct XwaOptSynthData* synth; /* non-NULL only for GLB-sourced models */
#endif
} NativeOptimizedPolyObject;

extern uint16_t g_loadOptBufHandle;
extern int g_loadOptBufSize;

/* Takes ownership of `data` (the payload after the OPT header). */
int OptModel_InitNativeModelFromBytes(NativeOptimizedPolyObject* native, uint8_t* data, int size);
void OptModel_DestroyNativeModel(NativeOptimizedPolyObject* native);

#ifdef __cplusplus
}
#endif

#endif
