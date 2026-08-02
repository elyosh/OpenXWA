#include "xwa/assets/opt_model.h"

#include "aeron/log.h"

#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif
#include "xwa/assets/file_io.h"
#include "xwa/assets/model_texture.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/flight/net_session.h"
#include "xwa/render/renderer.h"
#include "xwa/render/std3d_device.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/string.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef XWA_MODERN
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#endif

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
} NativeOptimizedPolyObject;

typedef struct OptPatchVisitedList {
	uint32_t* items;
	int count;
	int capacity;
} OptPatchVisitedList;

static NativeOptimizedPolyObject* g_optTextureUploadNative;

static int OptModel_BuildNativeGraph(NativeOptimizedPolyObject* native);

// GLOBAL: XWA 0x7CA6E0
LoadedModelHandleTable g_loadedModels;
// GLOBAL: XWA 0x91AC9E
uint16_t g_cockpitModel;
// GLOBAL: XWA 0x91AB74
uint16_t g_exteriorModel;

// GLOBAL: XWA 0x9B6D06
int g_modelVertCapacity;
// GLOBAL: XWA 0x9B6D0A
int g_modelEdgeCapacity;

// GLOBAL: XWA 0x5A9E00
const float g_optModelBrightnessConfigScale = 0.125f;
// GLOBAL: XWA 0x5A9E04
const float g_optModelBrightnessRangeScale = 1.75f;
// GLOBAL: XWA 0x5A9E08
const float g_optModelBrightnessOffset = -1.0f;

// GLOBAL: XWA 0x74C26C
static uint16_t g_loadOptBufHandle;
// GLOBAL: XWA 0x74C270
static int g_loadOptBufSize;

static uint16_t OptModel_NormalizeHandle(int modelHandle) {
	return (uint16_t)((uint16_t)modelHandle & 0x7fff);
}

static NativeOptimizedPolyObject* OptModel_AsNative(OptimizedPolyObject* model) {
	return (NativeOptimizedPolyObject*)model;
}

static void OptModel_WriteU32(uint8_t* ptr, uint32_t value) {
	ptr[0] = (uint8_t)value;
	ptr[1] = (uint8_t)(value >> 8);
	ptr[2] = (uint8_t)(value >> 16);
	ptr[3] = (uint8_t)(value >> 24);
}

static int OptModel_AddressToOffset(const NativeOptimizedPolyObject* native, uint32_t address, uint32_t size,
									uint32_t* outOffset) {
	uint32_t offset;

	if (address == 0 || native == NULL || outOffset == NULL) {
		return 0;
	}
	if (address < native->serializedBase) {
		return 0;
	}

	offset = address - native->serializedBase;
	if (offset > (uint32_t)native->serializedSize || size > (uint32_t)native->serializedSize - offset) {
		return 0;
	}

	*outOffset = offset;
	return 1;
}

static void* OptModel_AddressToPtr(const NativeOptimizedPolyObject* native, uint32_t address, uint32_t size) {
	uint32_t offset;

	if (!OptModel_AddressToOffset(native, address, size, &offset)) {
		return NULL;
	}

	return native->serializedData + offset;
}

/* Resolve a texture node's serialized palette address to a runtime pointer. Anchored
 * on the node's translated param2 so override nodes from other models resolve against
 * their own serialized image. */
void* OptModel_ResolveTexturePalette(const OptNode* textureNode) {
	const NativeOptNode* nativeNode;
	const OptTextureData* textureData;

	nativeNode = (const NativeOptNode*)textureNode;
	textureData = (const OptTextureData*)textureNode->param2;
	if (textureData == NULL || textureData->paletteAddress == 0) {
		return NULL;
	}

	return (uint8_t*)textureNode->param2 +
		   (int32_t)(textureData->paletteAddress - nativeNode->param2Address);
}

static uint32_t OptModel_PtrToAddress(const NativeOptimizedPolyObject* native, const void* ptr) {
	const uint8_t* bytes;

	if (native == NULL || ptr == NULL) {
		return 0;
	}

	bytes = (const uint8_t*)ptr;
	if (bytes < native->serializedData || bytes > native->serializedData + native->serializedSize) {
		return 0;
	}

	return native->serializedBase + (uint32_t)(bytes - native->serializedData);
}

static NativeOptNode* OptModel_FindNativeNodeByAddress(NativeOptimizedPolyObject* native, uint32_t address) {
	int i;

	for (i = 0; i < native->nodeCount; ++i) {
		if (native->nodes[i]->serializedAddress == address) {
			return native->nodes[i];
		}
	}

	return NULL;
}

#ifdef XWA_MODERN
static D3DInfoNode* D3DInfo_FindByBridgeRefId(uint32_t bridgeRefId) {
	D3DInfoNode* node;

	if (bridgeRefId == 0) {
		return NULL;
	}

	for (node = g_d3dInfoListHead; node != NULL; node = node->next) {
		if (node->bridgeRefId == bridgeRefId) {
			return node;
		}
	}
	return NULL;
}

static D3DInfoNode* D3DInfo_FindActiveNodeByPointer(intptr_t value) {
	D3DInfoNode* node;

	for (node = g_d3dInfoListHead; node != NULL; node = node->next) {
		if ((intptr_t)node == value) {
			return node;
		}
	}
	return NULL;
}
#endif

static int OptModel_AddNativeNode(NativeOptimizedPolyObject* native, NativeOptNode* node) {
	NativeOptNode** grown;
	int newCapacity;

	if (native->nodeCount == native->nodeCapacity) {
		newCapacity = native->nodeCapacity == 0 ? 64 : native->nodeCapacity * 2;
		grown = (NativeOptNode**)realloc(native->nodes, (size_t)newCapacity * sizeof(*native->nodes));
		if (grown == NULL) {
			return 0;
		}

		native->nodes = grown;
		native->nodeCapacity = newCapacity;
	}

	native->nodes[native->nodeCount++] = node;
	return 1;
}

static void OptModel_FreeNativeGraph(NativeOptimizedPolyObject* native) {
	int i;

	if (native == NULL) {
		return;
	}

	if (native->nodes != NULL) {
		for (i = 0; i < native->nodeCount; ++i) {
			free(native->nodes[i]->node.pChildren);
			free(native->nodes[i]);
		}
		free(native->nodes);
	}

	free(native->model.rootNodes);
	native->model.rootNodes = NULL;
	native->nodes = NULL;
	native->nodeCount = 0;
	native->nodeCapacity = 0;
}

static void OptModel_DestroyNativeModel(NativeOptimizedPolyObject* native) {
	if (native == NULL) {
		return;
	}

	OptModel_FreeNativeGraph(native);
	free(native->serializedData);
	native->serializedData = NULL;
	native->serializedSize = 0;
}

static int OptModel_CloneNativeModel(NativeOptimizedPolyObject* dst, const NativeOptimizedPolyObject* src) {
	uint8_t* serializedCopy;

	if (dst == NULL || src == NULL || src->serializedData == NULL || src->serializedSize <= 0) {
		return 0;
	}

	serializedCopy = (uint8_t*)malloc((size_t)src->serializedSize);
	if (serializedCopy == NULL) {
		return 0;
	}
	memcpy(serializedCopy, src->serializedData, (size_t)src->serializedSize);

	memset(dst, 0, sizeof(*dst));
	dst->serializedData = serializedCopy;
	dst->serializedSize = src->serializedSize;
	dst->serializedBase = src->serializedBase;
	dst->rootNodesAddress = src->rootNodesAddress;
	dst->model.selfMarker = &dst->model;
	dst->model.reserved = src->model.reserved;
	dst->model.rootNodeCount = src->model.rootNodeCount;

	if (!OptModel_BuildNativeGraph(dst)) {
		OptModel_DestroyNativeModel(dst);
		return 0;
	}

	return 1;
}

static NativeOptNode* OptModel_ParseNode(NativeOptimizedPolyObject* native, uint32_t address) {
	uint8_t* raw;
	NativeOptNode* existing;
	NativeOptNode* node;
	uint32_t offset;
	uint32_t nameAddress;
	uint32_t childrenAddress;
	int32_t nodeType;
	int32_t childCount;
	int32_t param1;
	uint32_t param2Address;
	int i;

	if (!OptModel_AddressToOffset(native, address, 24, &offset)) {
		Aeron_LogError("xwa.assets", "Invalid OPT node address 0x%08x", address);
		return NULL;
	}

	existing = OptModel_FindNativeNodeByAddress(native, address);
	if (existing != NULL) {
		return existing;
	}

	raw = native->serializedData + offset;
	nameAddress = ByteOrder_ReadU32Le(raw + 0);
	nodeType = ByteOrder_ReadI32Le(raw + 4);
	childCount = ByteOrder_ReadI32Le(raw + 8);
	childrenAddress = ByteOrder_ReadU32Le(raw + 12);
	param1 = ByteOrder_ReadI32Le(raw + 16);
	param2Address = ByteOrder_ReadU32Le(raw + 20);

	if (childCount < 0) {
		Aeron_LogError("xwa.assets", "Invalid OPT node child count %d at 0x%08x", childCount, address);
		return NULL;
	}

	node = (NativeOptNode*)calloc(1, sizeof(*node));
	if (node == NULL) {
		return NULL;
	}

	node->serializedAddress = address;
	node->param2Address = param2Address;
	node->node.nodeType = (OptNodeType)nodeType;
	node->node.childCount = childCount;
	node->node.param1 = param1;
	node->node.pName = (char*)OptModel_AddressToPtr(native, nameAddress, 1);

	if (param2Address != 0) {
		if (node->node.nodeType == OPT_TEXTURE_REF) {
#ifdef XWA_MODERN
			D3DInfoNode* d3dInfo;

			d3dInfo = D3DInfo_FindByBridgeRefId(param2Address);
			node->node.param2 = d3dInfo != NULL ? d3dInfo : (void*)(uintptr_t)param2Address;
#else
			node->node.param2 = (void*)(uintptr_t)param2Address;
#endif
		} else {
			node->node.param2 = OptModel_AddressToPtr(native, param2Address, 1);
		}
	}

	if (!OptModel_AddNativeNode(native, node)) {
		free(node);
		return NULL;
	}

	if (node->node.nodeType == OPT_NODEREF && param1 != 0) {
		uint32_t unusedOffset;

		node->node.param1 = 0;
		if (OptModel_AddressToOffset(native, (uint32_t)param1, 24, &unusedOffset)) {
			NativeOptNode* refNode = OptModel_ParseNode(native, (uint32_t)param1);
			node->node.param1 = (intptr_t)(refNode ? &refNode->node : NULL);
		}
	}

	if (childCount > 0 && childrenAddress != 0) {
		uint8_t* children =
			(uint8_t*)OptModel_AddressToPtr(native, childrenAddress, (uint32_t)childCount * 4);
		if (children == NULL) {
			Aeron_LogError("xwa.assets", "Invalid OPT children table 0x%08x", childrenAddress);
			return NULL;
		}

		node->node.pChildren = (OptNode**)calloc((size_t)childCount, sizeof(*node->node.pChildren));
		if (node->node.pChildren == NULL) {
			return NULL;
		}

		for (i = 0; i < childCount; ++i) {
			uint32_t childAddress = ByteOrder_ReadU32Le(children + i * 4);
			if (childAddress != 0) {
				NativeOptNode* child = OptModel_ParseNode(native, childAddress);
				if (child == NULL) {
					return NULL;
				}
				node->node.pChildren[i] = &child->node;
			}
		}
	}

	return node;
}

static int OptModel_BuildNativeGraph(NativeOptimizedPolyObject* native) {
	uint8_t* rootTable;
	int i;

	OptModel_FreeNativeGraph(native);

	if (native->model.rootNodeCount < 0) {
		Aeron_LogError("xwa.assets", "Invalid OPT root node count %d", native->model.rootNodeCount);
		return 0;
	}

	if (native->model.rootNodeCount == 0) {
		return 1;
	}

	rootTable = (uint8_t*)OptModel_AddressToPtr(native, native->rootNodesAddress,
												(uint32_t)native->model.rootNodeCount * 4);
	if (rootTable == NULL) {
		Aeron_LogError("xwa.assets", "Invalid OPT root node table 0x%08x", native->rootNodesAddress);
		return 0;
	}

	native->model.rootNodes =
		(OptNode**)calloc((size_t)native->model.rootNodeCount, sizeof(*native->model.rootNodes));
	if (native->model.rootNodes == NULL) {
		return 0;
	}

	for (i = 0; i < native->model.rootNodeCount; ++i) {
		uint32_t rootAddress = ByteOrder_ReadU32Le(rootTable + i * 4);
		if (rootAddress != 0) {
			NativeOptNode* root = OptModel_ParseNode(native, rootAddress);
			if (root == NULL) {
				return 0;
			}
			native->model.rootNodes[i] = &root->node;
		}
	}

	return 1;
}

static int OptModel_InitNativeModelFromBytes(NativeOptimizedPolyObject* native, uint8_t* data, int size) {
	uint32_t selfMarker;

	if (native == NULL || data == NULL || size < 14) {
		return 0;
	}

	memset(native, 0, sizeof(*native));
	native->serializedData = data;
	native->serializedSize = size;

	selfMarker = ByteOrder_ReadU32Le(data);
	native->serializedBase = selfMarker;
	native->model.selfMarker = &native->model;
	native->model.reserved = ByteOrder_ReadU16Le(data + 4);
	native->model.rootNodeCount = ByteOrder_ReadI32Le(data + 6);
	native->rootNodesAddress = ByteOrder_ReadU32Le(data + 10);

	return OptModel_BuildNativeGraph(native);
}

static int OptModel_PatchVisitedContains(const OptPatchVisitedList* visited, uint32_t address) {
	int i;

	for (i = 0; i < visited->count; ++i) {
		if (visited->items[i] == address) {
			return 1;
		}
	}

	return 0;
}

static int OptModel_PatchVisitedAdd(OptPatchVisitedList* visited, uint32_t address) {
	uint32_t* grown;
	int newCapacity;

	if (OptModel_PatchVisitedContains(visited, address)) {
		return 0;
	}

	if (visited->count == visited->capacity) {
		newCapacity = visited->capacity == 0 ? 64 : visited->capacity * 2;
		grown = (uint32_t*)realloc(visited->items, (size_t)newCapacity * sizeof(*visited->items));
		if (grown == NULL) {
			return 0;
		}

		visited->items = grown;
		visited->capacity = newCapacity;
	}

	visited->items[visited->count++] = address;
	return 1;
}

static uint32_t OptModel_AdjustSerializedAddress(uint32_t address, uint32_t thresholdAddress, int delta) {
	if (address != 0 && address >= thresholdAddress) {
		return (uint32_t)((int32_t)address + delta);
	}

	return address;
}

static void OptModel_PatchAddressField(uint8_t* field, uint32_t thresholdAddress, int delta) {
	uint32_t address = ByteOrder_ReadU32Le(field);
	OptModel_WriteU32(field, OptModel_AdjustSerializedAddress(address, thresholdAddress, delta));
}

static int OptModel_PatchRawNodeRecursive(NativeOptimizedPolyObject* native, uint32_t nodeAddress,
										  uint32_t thresholdAddress, int delta,
										  OptPatchVisitedList* visited) {
	uint8_t* raw;
	uint32_t offset;
	int32_t nodeType;
	int32_t childCount;
	uint32_t childrenAddress;
	int i;

	if (nodeAddress == 0 || !OptModel_PatchVisitedAdd(visited, nodeAddress)) {
		return 1;
	}

	if (!OptModel_AddressToOffset(native, nodeAddress, 24, &offset)) {
		return 0;
	}

	raw = native->serializedData + offset;
	nodeType = ByteOrder_ReadI32Le(raw + 4);
	childCount = ByteOrder_ReadI32Le(raw + 8);
	childrenAddress = ByteOrder_ReadU32Le(raw + 12);

	OptModel_PatchAddressField(raw + 0, thresholdAddress, delta);
	if (ByteOrder_ReadU32Le(raw + 20) != 0 && nodeType != OPT_TEXTURE_REF) {
		OptModel_PatchAddressField(raw + 20, thresholdAddress, delta);
	}
	if (nodeType == OPT_NODEREF && ByteOrder_ReadU32Le(raw + 16) != 0) {
		OptModel_PatchAddressField(raw + 16, thresholdAddress, delta);
	}
	if (nodeType == OPT_TEXTURE && ByteOrder_ReadU32Le(raw + 20) != 0) {
		uint8_t* texture = (uint8_t*)OptModel_AddressToPtr(native, ByteOrder_ReadU32Le(raw + 20), 8);
		if (texture != NULL && ByteOrder_ReadI32Le(texture + 4) == 0) {
			OptModel_PatchAddressField(texture, thresholdAddress, delta);
		}
	}

	if (childrenAddress != 0 && childCount > 0) {
		uint8_t* children;

		OptModel_PatchAddressField(raw + 12, thresholdAddress, delta);
		childrenAddress = ByteOrder_ReadU32Le(raw + 12);
		children = (uint8_t*)OptModel_AddressToPtr(native, childrenAddress, (uint32_t)childCount * 4);
		if (children == NULL) {
			return 0;
		}

		for (i = 0; i < childCount; ++i) {
			uint8_t* childField = children + i * 4;
			uint32_t childAddress;

			OptModel_PatchAddressField(childField, thresholdAddress, delta);
			childAddress = ByteOrder_ReadU32Le(childField);
			if (childAddress != 0 &&
				!OptModel_PatchRawNodeRecursive(native, childAddress, thresholdAddress, delta, visited)) {
				return 0;
			}
		}
	}

	return 1;
}

static int OptModel_PatchNativeSerializedPointers(NativeOptimizedPolyObject* native,
												  uint32_t thresholdAddress, int delta) {
	OptPatchVisitedList visited;
	uint8_t* rootTable;
	int i;

	memset(&visited, 0, sizeof(visited));

	native->rootNodesAddress =
		OptModel_AdjustSerializedAddress(native->rootNodesAddress, thresholdAddress, delta);
	OptModel_WriteU32(native->serializedData + 10, native->rootNodesAddress);

	rootTable = (uint8_t*)OptModel_AddressToPtr(native, native->rootNodesAddress,
												(uint32_t)native->model.rootNodeCount * 4);
	if (rootTable == NULL) {
		return 0;
	}

	for (i = 0; i < native->model.rootNodeCount; ++i) {
		uint8_t* rootField = rootTable + i * 4;
		uint32_t rootAddress;

		OptModel_PatchAddressField(rootField, thresholdAddress, delta);
		rootAddress = ByteOrder_ReadU32Le(rootField);
		if (rootAddress != 0 &&
			!OptModel_PatchRawNodeRecursive(native, rootAddress, thresholdAddress, delta, &visited)) {
			free(visited.items);
			return 0;
		}
	}

	free(visited.items);
	return 1;
}

static void OptModel_SetNodeParam1(NativeOptimizedPolyObject* native, OptNode* node, int32_t value) {
	NativeOptNode* nativeNode;
	uint8_t* raw;

	if (native == NULL || node == NULL) {
		return;
	}

	nativeNode = (NativeOptNode*)node;
	node->param1 = value;
	raw = (uint8_t*)OptModel_AddressToPtr(native, nativeNode->serializedAddress, 24);
	if (raw != NULL) {
		OptModel_WriteU32(raw + 16, (uint32_t)value);
	}
}

static void OptModel_WriteNodeTextureRef(NativeOptimizedPolyObject* native, OptNode* node, int param1,
										 intptr_t textureRef) {
	NativeOptNode* nativeNode;
	uint8_t* raw;
	uint32_t serializedTextureRef;

	if (native == NULL || node == NULL) {
		return;
	}

	nativeNode = (NativeOptNode*)node;
	serializedTextureRef = (uint32_t)textureRef;
#ifdef XWA_MODERN
	if (textureRef != 0) {
		D3DInfoNode* d3dInfo = D3DInfo_FindActiveNodeByPointer(textureRef);

		if (d3dInfo != NULL && d3dInfo->bridgeRefId != 0) {
			serializedTextureRef = d3dInfo->bridgeRefId;
		}
	}
#endif
	node->nodeType = OPT_TEXTURE_REF;
	node->childCount = 0;
	node->pChildren = NULL;
	node->param1 = param1;
	node->param2 = (void*)textureRef;
	nativeNode->param2Address = serializedTextureRef;

	raw = (uint8_t*)OptModel_AddressToPtr(native, nativeNode->serializedAddress, 24);
	if (raw != NULL) {
		OptModel_WriteU32(raw + 4, OPT_TEXTURE_REF);
		OptModel_WriteU32(raw + 8, 0);
		OptModel_WriteU32(raw + 12, 0);
		OptModel_WriteU32(raw + 16, (uint32_t)param1);
		OptModel_WriteU32(raw + 20, serializedTextureRef);
	}
}

static uint8_t* OptModel_GetSerializedNodePayloadStart(NativeOptimizedPolyObject* native, OptNode* node) {
	NativeOptNode* nativeNode;
	uint8_t* raw;

	nativeNode = (NativeOptNode*)node;
	raw = (uint8_t*)OptModel_AddressToPtr(native, nativeNode->serializedAddress, 24);
	if (raw == NULL) {
		return NULL;
	}

	raw += 24;
	if (node->pName != NULL) {
		raw += strlen(node->pName) + 1;
	}

	return raw;
}

// FUNCTION: XWA 0x484AC0
void OptModel_AdjustOptimizedNodePointers(OptNode* node, intptr_t base) {
	(void)node;
	(void)base;
	/* The original converted serialized 32-bit addresses into absolute x86 pointers in place.
	   The portable port performs that conversion when building the native OPT graph. */
}

// FUNCTION: XWA 0x4849C0
void OptModel_AdjustOptimizedPolyObjectPointers(OptimizedPolyObject* model) {
	NativeOptimizedPolyObject* native;

	if (model == NULL) {
		return;
	}

	native = OptModel_AsNative(model);
	if (native->serializedData == NULL) {
		return;
	}

	model->selfMarker = model;
	if (model->rootNodes == NULL) {
		OptModel_BuildNativeGraph(native);
	}
}

// FUNCTION: XWA 0x484CF0
unsigned int OptModel_GetSerializedNodeSize(OptNode* node, int* parentState, int texturePageCount) {
	unsigned int size;
	void* param2;
	int nodeType;
	int childIndex;
	int localParentState[71];

	if (node == NULL) {
		return 0;
	}

	size = 24;
	if (node->pName != NULL) {
		size = (unsigned int)strlen(node->pName) + 25;
	}

	param2 = node->param2;
	nodeType = node->nodeType;
	if (param2 != NULL) {
		switch (nodeType) {
			case OPT_NULL:
			case OPT_GROUP:
			case OPT_TYPE_10:
			case 12:
			case 14:
			case OPT_NODESWITCH:
			case OPT_TEXTURE_REF:
				break;
			case OPT_FACEDATA:
			case OPT_FACEDATA_15:
			case OPT_FACEDATA_16:
			case OPT_FACEDATA_17: {
				int edgeCount = ByteOrder_ReadI32Le((const uint8_t*)param2);
				if (edgeCount > g_modelEdgeCapacity) {
					g_modelEdgeCapacity = edgeCount;
				}
				size += (unsigned int)(100 * (int)node->param1 + 4);
				if (parentState[33] == 0) {
					size += (unsigned int)(12 * g_curVertexCount);
				}
				break;
			}
			case OPT_TYPE_2:
			case OPT_ROTSCALE:
				size += 48;
				break;
			case OPT_MESHVERTS:
				g_curVertexCount = (int)node->param1;
				size += (unsigned int)(12 * g_curVertexCount);
				if ((int)node->param1 > g_modelVertCapacity) {
					g_modelVertCapacity = (int)node->param1;
				}
				break;
			case OPT_TYPE_4:
			case OPT_TYPE_6:
			case OPT_TYPE_19:
				size += 12;
				break;
			case OPT_TYPE_5:
				size += 36;
				break;
			case OPT_NODEREF:
				size += (unsigned int)strlen((const char*)param2) + 1;
				break;
			case 9:
				g_curMeshFlags = (intptr_t)param2;
				size += (unsigned int)(56 * (int)node->param1);
				break;
			case OPT_VERTNORMALS:
				g_curVertNormals = (intptr_t)param2;
				parentState[33] = 1;
				size += (unsigned int)(12 * (int)node->param1);
				break;
			case OPT_TEXCOORDS:
				size += (unsigned int)(8 * (int)node->param1);
				break;
			case OPT_TEXTURE: {
				const NativeOptNode* nativeNode;
				const uint8_t* texture;
				int textureSize;
				int dataSize;
				int texelCount;
				uint32_t paletteAddress;

				nativeNode = (const NativeOptNode*)node;
				texture = (const uint8_t*)param2;
				textureSize = ByteOrder_ReadI32Le(texture + 8);
				dataSize = ByteOrder_ReadI32Le(texture + 12);
				texelCount = ByteOrder_ReadI32Le(texture + 16) * ByteOrder_ReadI32Le(texture + 20);
				paletteAddress = ByteOrder_ReadU32Le(texture);
				size += 24;
				if (texelCount == textureSize) {
					size += (unsigned int)dataSize;
				} else {
					size += (unsigned int)texelCount;
				}
				if (ByteOrder_ReadI32Le(texture + 4) != 0) {
					size += (unsigned int)(texturePageCount << 12);
				} else {
					int payloadSize = texelCount == textureSize ? dataSize : texelCount;
					if (nativeNode->param2Address + 24u + (uint32_t)payloadSize == paletteAddress) {
						size += (unsigned int)(texturePageCount << 12);
					}
				}
				break;
			}
			case OPT_FACEGROUP:
				size += (unsigned int)(4 * (int)node->param1);
				break;
			case OPT_HARDPOINT:
				size += 16;
				break;
			case OPT_MESHDESC:
			case OPT_ENGINEGLOW:
				size += 72;
				break;
			case OPT_TEXALPHA:
				size += (unsigned int)node->param1;
				break;
			default:
				DebugPrintf("Bad nodetype (with fieldPtr) in IVGetNodeSize(): %d", node->nodeType);
				break;
		}
	} else {
		switch (nodeType) {
			case OPT_NULL:
			case OPT_GROUP:
			case OPT_TYPE_10:
			case 12:
			case 14:
			case OPT_NODESWITCH:
				break;
			default:
				DebugPrintf("Bad nodetype (no fieldPtr) in IVGetNodeSize(): %d", node->nodeType);
				break;
		}
	}

	if (node->childCount != 0) {
		memcpy(localParentState, parentState, sizeof(localParentState));
		g_modelNodeWalkUnusedScratch0 = 0;
		g_modelNodeWalkUnusedScratch1 = 0;
		g_curVertNormals = 0;
		g_modelNodeWalkUnusedScratch2 = 0;
		g_curMeshFlags = 0;
		size += (unsigned int)(4 * node->childCount);
		for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
			size += OptModel_GetSerializedNodeSize(node->pChildren[childIndex], localParentState,
												   texturePageCount);
		}
	}

	return size;
}

// FUNCTION: XWA 0x484C60
int OptModel_ComputeProcessedModelSize(OptimizedPolyObject* model, int texturePageCount) {
	int parentState[71];
	int rootIndex;
	int size;

	memset(parentState, 0, sizeof(parentState));
	g_modelNodeWalkUnusedScratch0 = 0;
	g_modelNodeWalkUnusedScratch1 = 0;
	g_curVertNormals = 0;
	g_modelNodeWalkUnusedScratch2 = 0;
	g_curMeshFlags = 0;
	g_curVertexCount = 0;

	size = 4 * model->rootNodeCount + 14;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		size +=
			(int)OptModel_GetSerializedNodeSize(model->rootNodes[rootIndex], parentState, texturePageCount);
	}

	return size;
}

// FUNCTION: XWA 0x484B70
void OptModel_AdjustNodePointersRecursive(OptNode* node, intptr_t base, const void* threshold,
										  intptr_t delta) {
	(void)node;
	(void)base;
	(void)threshold;
	(void)delta;
	/* The native graph never stores serialized addresses in pointer fields; byte-range patching is
	   performed on the serialized backing store by OptModel_AdjustOptimizedPolyObjectPatchPointers. */
}

// FUNCTION: XWA 0x484A30
void OptModel_AdjustOptimizedPolyObjectPatchPointers(OptimizedPolyObject* model, const void* threshold,
													 int delta) {
	NativeOptimizedPolyObject* native;
	uint32_t thresholdAddress;

	if (model == NULL) {
		return;
	}

	native = OptModel_AsNative(model);
	thresholdAddress = OptModel_PtrToAddress(native, threshold);
	if (thresholdAddress == 0) {
		return;
	}

	if (OptModel_PatchNativeSerializedPointers(native, thresholdAddress, delta)) {
		OptModel_BuildNativeGraph(native);
	}
}

// FUNCTION: XWA 0x4CCB00
uint16_t OptModel_LoadFileToHandle(const char* fileName, int* outVersion) {
	XwaFile* stream;
	uint8_t header[4];
	int32_t firstDword;
	int version;
	int payloadSize;
	uint8_t* payload;
	uint16_t handle;
	NativeOptimizedPolyObject* native;

	if (outVersion != NULL) {
		*outVersion = 0;
	}

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	if (stream == NULL) {
		Aeron_LogError("xwa.assets", "Failed to open OPT '%s'", fileName);
		return 0;
	}

	if (!File_ReadCount(stream, header, sizeof(header))) {
		Aeron_LogError("xwa.assets", "Failed to read OPT '%s' header", fileName);
		File_Close(stream);
		return 0;
	}

	firstDword = ByteOrder_ReadI32Le(header);
	if (firstDword > 0) {
		version = 0;
		payloadSize = firstDword;
	} else {
		version = -firstDword;
		if (!File_ReadCount(stream, header, sizeof(header))) {
			Aeron_LogError("xwa.assets", "Failed to read OPT '%s' size", fileName);
			File_Close(stream);
			return 0;
		}
		payloadSize = ByteOrder_ReadI32Le(header);
	}

	if (version < 2 || payloadSize <= 0) {
		File_Close(stream);
		Aeron_LogError("xwa.assets", "Unsupported OPT '%s' version %d size %d", fileName, version, payloadSize);
		return 0;
	}

	payload = (uint8_t*)malloc((size_t)payloadSize);
	if (payload == NULL) {
		Aeron_LogError("xwa.assets", "Failed to allocate %d bytes for OPT '%s'", payloadSize, fileName);
		File_Close(stream);
		return 0;
	}

	if (!File_ReadCount(stream, payload, (size_t)payloadSize)) {
		Aeron_LogError("xwa.assets", "Failed to read OPT '%s' payload size %d", fileName, payloadSize);
		free(payload);
		File_Close(stream);
		return 0;
	}

	File_Close(stream);

	if (g_loadOptBufHandle != 0) {
		OptModel_FreeHandle(g_loadOptBufHandle);
	}

	handle = Memory_AllocHandle(0, sizeof(NativeOptimizedPolyObject));
	if (handle == 0) {
		Aeron_LogError("xwa.assets", "Failed to allocate native OPT handle for '%s'", fileName);
		free(payload);
		return 0;
	}

	native = (NativeOptimizedPolyObject*)Memory_LockHandle(handle);
	if (native == NULL) {
		Aeron_LogError("xwa.assets", "Failed to lock native OPT handle %u for '%s'", (unsigned)handle, fileName);
		Memory_FreeHandle(0, handle);
		free(payload);
		return 0;
	}

	if (!OptModel_InitNativeModelFromBytes(native, payload, payloadSize)) {
		Aeron_LogError("xwa.assets", "Failed to parse OPT '%s' payload size %d", fileName, payloadSize);
		if (native != NULL) {
			OptModel_DestroyNativeModel(native);
			Memory_UnlockHandle(handle);
		}
		Memory_FreeHandle(0, handle);
		return 0;
	}

	Memory_UnlockHandle(handle);
	if (outVersion != NULL) {
		*outVersion = version;
	}
	g_loadOptBufHandle = handle;
	g_loadOptBufSize = payloadSize;
	return handle;
}

// FUNCTION: XWA 0x4850F0
void OptModel_DeleteBytes(OptimizedPolyObject* model, int modelSize, void* cutPoint, int cutSize) {
	NativeOptimizedPolyObject* native;
	uint8_t* cut;
	uint8_t* end;
	uint8_t* afterCut;
	uint32_t thresholdAddress;

	if (model == NULL || modelSize < 1) {
		DebugPrintf("bad parameters to DeleteBytesFromModel()");
		return;
	}
	if (cutSize < 0) {
		DebugPrintf("negative cutSize in DeleteBytesFromModel()");
		return;
	}

	native = OptModel_AsNative(model);
	cut = (uint8_t*)cutPoint;
	end = native->serializedData + native->serializedSize;
	if (cut < native->serializedData || cut >= end) {
		DebugPrintf("cutPoint outside of model in DeleteBytesFromModel()");
		return;
	}

	if (cutSize == 0) {
		return;
	}

	afterCut = cut + cutSize;
	if (afterCut - 1 >= end) {
		DebugPrintf("cut range exceeds model data in DeleteBytesFromModel()");
		return;
	}

	thresholdAddress = OptModel_PtrToAddress(native, afterCut);
	memmove(cut, afterCut, (size_t)(end - afterCut));
	native->serializedSize -= cutSize;
	if (thresholdAddress != 0 && OptModel_PatchNativeSerializedPointers(native, thresholdAddress, -cutSize)) {
		OptModel_BuildNativeGraph(native);
	}
}

static __inline int OptModel_RoundPaletteComponent(float value, int maxValue) {
	int component;

	component = (int)(value - -0.5f);
	if (component > maxValue) {
		component = maxValue;
	}

	return component;
}

// FUNCTION: XWA 0x4CCE50
void OptModel_ScaleTexturePaletteBrightness16Bpp(float brightnessScale, const uint16_t* srcPalette,
												 uint16_t* dstPalette, int entryCount, int isRgb555) {
	int i;

	if (entryCount <= 0) {
		return;
	}

	for (i = 0; i < entryCount; ++i) {
		uint16_t src;
		int blue5;
		int green6;
		int red5;

		src = srcPalette[i];
		blue5 = src & 0x1f;
		src >>= 5;
		green6 = src & 0x3f;
		red5 = (src >> 6) & 0x1f;

		if (red5 != 0 || green6 != 0 || blue5 != 0) {
			float red;
			float green;
			float blue;
			float maxComponent;
			float scaledMax;
			float scale;
			float scaledRed;
			float scaledGreen;
			float scaledBlue;
			int outRed;
			int outGreen;
			int outBlue;
			int packed;

			blue = (float)blue5 * 0.032258064f;
			red = (float)red5 * 0.032258064f;
			green = (float)green6 * 0.015873017f;
			maxComponent = red;
			if (green > maxComponent) {
				maxComponent = green;
			}
			if (blue > maxComponent) {
				maxComponent = blue;
			}

			scaledMax = maxComponent * brightnessScale;
			if (scaledMax > 1.0f) {
				scaledMax = 1.0f;
			}
			scale = scaledMax / maxComponent;

			scaledRed = scale * red;
			scaledGreen = scale * green;
			scaledBlue = scale * blue;

			if (isRgb555) {
				outRed = OptModel_RoundPaletteComponent(scaledRed * 31.0f, 31);
				outGreen = OptModel_RoundPaletteComponent(scaledGreen * 31.0f, 31);
				outBlue = OptModel_RoundPaletteComponent(scaledBlue * 31.0f, 31);
				packed = (outRed << 10) | (outGreen << 5) | outBlue;
			} else {
				outRed = OptModel_RoundPaletteComponent(scaledRed * 31.0f, 31);
				outGreen = OptModel_RoundPaletteComponent(scaledGreen * 63.0f, 63);
				outBlue = OptModel_RoundPaletteComponent(scaledBlue * 31.0f, 31);
				packed = (outRed << 11) | (outGreen << 5) | outBlue;
			}

			dstPalette[i] = packed;
		}
	}
}

// FUNCTION: XWA 0x4CCD40
void OptModel_PrepareTexturePalette(uint16_t* palette) {
	uint16_t* brightPalette;
	float brightnessScale;
	int isRgb555;

	if (!g_useHardware3D) {
		RgbTriplet softwarePalette[4096];
		int i;

		for (i = 0; i < 4096; ++i) {
			unsigned int color;

			color = palette[i];
			softwarePalette[i].b = (uint8_t)((color & 0x1f) << 1);
			color >>= 5;
			softwarePalette[i].g = (uint8_t)(color & 0x3f);
			softwarePalette[i].r = (uint8_t)(((color >> 6) & 0x1f) << 1);
		}

		FlightPalette_Build16BppRange(softwarePalette, palette, 0, 4096);
		return;
	}

	brightPalette = palette + 2048;
	ModelTexture_FilterHardwarePalette(palette);
	brightnessScale =
		(float)g_gameConfig.brightness[NetSession_GetPlayerCount() > 1] * g_optModelBrightnessConfigScale;
	brightnessScale = brightnessScale * g_optModelBrightnessRangeScale - g_optModelBrightnessOffset;

	if (brightPalette[256] != 0) {
		isRgb555 = Display_IsPixelFormat555();
		OptModel_ScaleTexturePaletteBrightness16Bpp(1.0f, palette, palette, 256, isRgb555);
	}

	isRgb555 = Display_IsPixelFormat555();
	OptModel_ScaleTexturePaletteBrightness16Bpp(brightnessScale, brightPalette, brightPalette, 256, isRgb555);
}

// FUNCTION: XWA 0x4CCC40
int OptModel_PrepareTextures(OptNode* node, OptimizedPolyObject* model, int modelSize) {
#ifndef XWA_MODERN
	if (node == NULL) {
		return modelSize;
	}

	if (node->nodeType != OPT_NODEREF) {
		if (node->nodeType == OPT_TEXTURE) {
			OptTextureData* textureData;
			int dataSize;
			uint16_t* palette;
			uint8_t* texels;

			textureData = (OptTextureData*)node->param2;
			dataSize = textureData->width;
			palette = (uint16_t*)(uintptr_t)textureData->paletteAddress;
			texels = (uint8_t*)(textureData + 1);
			if (dataSize * textureData->height == textureData->textureSize) {
				dataSize = textureData->dataSize;
			} else {
				dataSize *= textureData->height;
			}
			if ((uint8_t*)palette == texels + dataSize) {
				OptModel_PrepareTexturePalette(palette);
			}

			if (!g_keepFullResTextures && textureData->width > 8 && textureData->height > 8) {
				int oldPixelBytes;
				int width;
				int height;

				width = textureData->width;
				height = textureData->height;
				oldPixelBytes = width * height;
				height >>= 1;
				width >>= 1;
				textureData->height = height;
				textureData->dataSize -= oldPixelBytes;
				textureData->textureSize = height * width;
				textureData->width = width;

				OptModel_DeleteBytes(model, modelSize, texels, oldPixelBytes);
				modelSize -= oldPixelBytes;
				if (node->childCount != 0) {
					void* alphaData;

					alphaData = node->pChildren[0]->param2;
					node->pChildren[0]->param1 -= oldPixelBytes;
					OptModel_DeleteBytes(model, modelSize, alphaData, oldPixelBytes);
					modelSize -= oldPixelBytes;
				}
			}
		}
	} else {
		node->param1 = 0;
	}

	{
		int childIndex;

		for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
			modelSize = OptModel_PrepareTextures(*(node->pChildren + childIndex), model, modelSize);
		}
	}

	return modelSize;
#else
	NativeOptimizedPolyObject* native;
	uint32_t nodeAddress;
	int newModelSize;
	int childIndex;

	if (node == NULL) {
		return modelSize;
	}

	native = OptModel_AsNative(model);
	nodeAddress = ((NativeOptNode*)node)->serializedAddress;

	if (node->nodeType == OPT_NODEREF) {
		OptModel_SetNodeParam1(native, node, 0);
	} else if (node->nodeType == OPT_TEXTURE) {
		OptTextureData* textureData;
		int dataSize;
		uint16_t* palette;

		textureData = (OptTextureData*)node->param2;
		dataSize = textureData->height * textureData->width;
		/* XWA_MODERN keeps OPT address fields serialized; resolve them through the native model. */
		palette = (uint16_t*)OptModel_AddressToPtr(native, textureData->paletteAddress, 2);
		if (dataSize == textureData->textureSize) {
			dataSize = textureData->dataSize;
		}
		if ((uint8_t*)(textureData + 1) + dataSize == (uint8_t*)palette) {
			OptModel_PrepareTexturePalette(palette);
		}

		if (!g_keepFullResTextures && textureData->width > 8 && textureData->height > 8) {
			NativeOptNode* refreshedNode;
			int oldPixelBytes;

			oldPixelBytes = textureData->width * textureData->height;
			textureData->height = textureData->height >> 1;
			textureData->textureSize = (textureData->width >> 1) * textureData->height;
			textureData->dataSize -= oldPixelBytes;
			textureData->width = textureData->width >> 1;

			OptModel_DeleteBytes(model, modelSize, textureData + 1, oldPixelBytes);
			newModelSize = modelSize - oldPixelBytes;
			refreshedNode = OptModel_FindNativeNodeByAddress(native, nodeAddress);
			if (refreshedNode == NULL) {
				return newModelSize;
			}
			node = &refreshedNode->node;
			if (node->childCount != 0) {
				OptNode* child;
				void* alphaData;

				child = node->pChildren[0];
				alphaData = child->param2;
				OptModel_SetNodeParam1(native, child, (int32_t)(child->param1 - oldPixelBytes));
				OptModel_DeleteBytes(model, newModelSize, alphaData, oldPixelBytes);
				newModelSize -= oldPixelBytes;
				refreshedNode = OptModel_FindNativeNodeByAddress(native, nodeAddress);
				if (refreshedNode == NULL) {
					return newModelSize;
				}
				node = &refreshedNode->node;
			}
			goto recurse_children;
		}
	}

	newModelSize = modelSize;

recurse_children:
	for (childIndex = 0;; ++childIndex) {
		NativeOptNode* currentNativeNode;
		OptNode* child;

		currentNativeNode = OptModel_FindNativeNodeByAddress(native, nodeAddress);
		if (currentNativeNode == NULL) {
			break;
		}
		node = &currentNativeNode->node;
		if (childIndex >= node->childCount) {
			break;
		}
		child = node->pChildren[childIndex];
		if (child == NULL) {
			continue;
		}
		newModelSize = OptModel_PrepareTextures(child, model, newModelSize);
	}

	return newModelSize;
#endif
}

// FUNCTION: XWA 0x4CD1A0
int OptModel_ReplaceTextureNodesWithRefsRecursive(OptNode* node, OptimizedPolyObject* model,
												  const intptr_t* textureIds, int textureRefIndex,
												  int* modelSize) {
	NativeOptimizedPolyObject* native;
	uint32_t nodeAddress;
	int childIndex;
	int result;

	if (node == NULL) {
		return textureRefIndex;
	}

	native = OptModel_AsNative(model);
	nodeAddress = ((NativeOptNode*)node)->serializedAddress;

	if (node->nodeType == OPT_TEXTURE) {
		OptTextureData* textureData;
		uint16_t* palette;
		int textureSize;
		int dataSize;
		int bytesToDelete;
		uint8_t* deleteStart;

		textureData = (OptTextureData*)node->param2;
		textureSize = textureData->textureSize;
		dataSize = textureData->height * textureData->width;
		if (dataSize == textureSize) {
			bytesToDelete = textureData->dataSize + 24;
		} else {
			bytesToDelete = dataSize + 24;
		}
		if (dataSize == textureSize) {
			dataSize = textureData->dataSize;
		}

		palette = (uint16_t*)OptModel_AddressToPtr(native, textureData->paletteAddress, 2);
		if ((uint8_t*)(textureData + 1) + dataSize == (uint8_t*)palette) {
			bytesToDelete += 0x2000;
		}
		if (node->childCount != 0) {
			bytesToDelete += (int)node->pChildren[0]->param1 + 24;
		}

		if (textureIds[textureRefIndex] == 0) {
			return textureRefIndex + 1;
		}

		deleteStart = OptModel_GetSerializedNodePayloadStart(native, node);
		OptModel_WriteNodeTextureRef(native, node, 1, textureIds[textureRefIndex]);
		OptModel_DeleteBytes(model, *modelSize, deleteStart, bytesToDelete);
		*modelSize -= bytesToDelete;
		return textureRefIndex + 1;
	}

	result = textureRefIndex;
	for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
		result = OptModel_ReplaceTextureNodesWithRefsRecursive(node->pChildren[childIndex], model, textureIds,
															   result, modelSize);
		node = &OptModel_FindNativeNodeByAddress(native, nodeAddress)->node;
	}

	return result;
}

// FUNCTION: XWA 0x4CD2A0
void OptModel_ResolveTextureRefsRecursive(OptNode* node, OptimizedPolyObject* model, int* modelSize) {
	NativeOptimizedPolyObject* native;
	uint32_t nodeAddress;
	int childIndex;

	if (node == NULL) {
		return;
	}

	native = OptModel_AsNative(model);
	nodeAddress = ((NativeOptNode*)node)->serializedAddress;

	if (node->nodeType == OPT_NODEREF) {
		OptNode* resolvedNode;

		resolvedNode = OptModel_ResolveNodeRef(node, model);
		if (resolvedNode == NULL) {
			return;
		}
		if (resolvedNode->nodeType == OPT_TEXTURE_REF) {
			char* refName;
			int bytesToDelete;
			intptr_t textureRef;

			refName = (char*)node->param2;
			textureRef = (intptr_t)resolvedNode->param2;
			bytesToDelete = (int)strlen(refName) + 1;
			OptModel_WriteNodeTextureRef(native, node, 0, textureRef);
			OptModel_DeleteBytes(model, *modelSize, refName, bytesToDelete);
			*modelSize -= bytesToDelete;
			return;
		}
	}

	for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
		OptModel_ResolveTextureRefsRecursive(node->pChildren[childIndex], model, modelSize);
		node = &OptModel_FindNativeNodeByAddress(native, nodeAddress)->node;
	}
}

// FUNCTION: XWA 0x441750
void D3DInfo_InitPool(void) {
#ifdef XWA_MODERN
	int i;

	for (i = 0; i < XWA_D3DINFO_POOL_COUNT; ++i) {
		memset(&g_d3dInfoPool[i], 0, sizeof(g_d3dInfoPool[i]));
		g_d3dInfoPool[i].next = i + 1 < XWA_D3DINFO_POOL_COUNT ? &g_d3dInfoPool[i + 1] : NULL;
		g_d3dInfoPool[i].prev = NULL;
	}
	g_d3dInfoListHead = NULL;
	g_d3dInfoActiveCount = 0;
	g_d3dInfoFreeListHead = g_d3dInfoPool;
	g_nextD3DInfoBridgeRefId = 1;
#else
	D3DInfoNode* node;

	node = &g_d3dInfoPool[1];
	do {
		memset(node - 1, 0, sizeof(*node));
		(node - 1)->next = node;
		(node - 1)->prev = NULL;
		++node;
	} while (node < (D3DInfoNode*)&g_clipCountB);

	g_d3dInfoListHead = NULL;
	memset(&g_d3dInfoPool[XWA_D3DINFO_POOL_COUNT - 1], 0, sizeof(g_d3dInfoPool[XWA_D3DINFO_POOL_COUNT - 1]));
	g_d3dInfoPool[XWA_D3DINFO_POOL_COUNT - 1].next = NULL;
	g_d3dInfoPool[XWA_D3DINFO_POOL_COUNT - 1].prev = NULL;
	g_d3dInfoActiveCount = 0;
	g_d3dInfoFreeListHead = g_d3dInfoPool;
#endif
}

// FUNCTION: XWA 0x441860
void D3DInfo_ReleaseAll(void) {
	if (g_d3dInfoActiveCount != 0) {
		while (g_d3dInfoListHead != NULL) {
			g_d3dInfoListHead->refCount = 1;
			D3DInfo_Release(g_d3dInfoListHead);
		}
	}
}

static __inline int D3DInfo_ComputeMipLevelCount(const OptTextureData* textureData) {
	int mipLevelCount;
	int minDimension;

	mipLevelCount = 1;
	if (textureData->width * textureData->height == textureData->textureSize) {
		minDimension = textureData->width;
		if (textureData->width >= textureData->height) {
			minDimension = textureData->height;
		}
		if (minDimension >= 256) {
			mipLevelCount = 6;
		} else if (minDimension >= 128) {
			mipLevelCount = 5;
		} else if (minDimension >= 64) {
			mipLevelCount = 4;
		} else if (minDimension >= 32) {
			mipLevelCount = 3;
		} else {
			mipLevelCount = (minDimension >= 16) + 1;
		}
	}
	return mipLevelCount;
}

static __inline D3DInfoNode* D3DInfo_AllocOrReuse(int textureId) {
	D3DInfoNode* node;

#ifdef XWA_MODERN
	if (g_d3dInfoFreeListHead == NULL && g_d3dInfoListHead == NULL && g_d3dInfoActiveCount == 0) {
		D3DInfo_InitPool();
	}
#endif

	if (textureId != 0) {
		for (node = g_d3dInfoListHead; node != NULL; node = node->next) {
			if (node->textureId == textureId) {
				++node->refCount;
				return node;
			}
		}
	}

	node = g_d3dInfoFreeListHead;
	if (node == NULL) {
#ifdef XWA_MODERN
		DebugPrintf("%s", "Not enough D3DTextures\n");
#else
		g_OutputDebugStringA("Not enough D3DTextures\n");
#endif
		++g_d3dInfoListHead->refCount;
		return g_d3dInfoListHead;
	}

	g_d3dInfoFreeListHead = node->next;
#ifdef XWA_MODERN
	memset(node, 0, sizeof(*node));
#endif
	if (g_d3dInfoListHead != NULL) {
		g_d3dInfoListHead->prev = node;
	}
	node->next = g_d3dInfoListHead;
	node->prev = NULL;
	g_d3dInfoListHead = node;
	++g_d3dInfoActiveCount;
	node->textureId = textureId;
	node->refCount = 1;
#ifdef XWA_MODERN
	node->bridgeRefId = g_nextD3DInfoBridgeRefId++;
	if (node->bridgeRefId == 0) {
		node->bridgeRefId = g_nextD3DInfoBridgeRefId++;
	}
#endif
	return node;
}

/* Scratch buffer holding the extracted lightmap plane for the whole mip chain
 * (256x256 base + mips = 87360 texels). Reused per texture; consumed inside
 * D3DInfo_CreateFromOptTexture before the next call. */
// GLOBAL: XWA 0x64D1B0
uint8_t g_d3dLightmapScratchPixels[87360];

// FUNCTION: XWA 0x4418A0
D3DInfoNode* D3DInfo_CreateFromOptTexture(char* textureName, int textureId, OptTextureData* textureData,
										  void* pixelData, void* alphaData, uint16_t* brightPalette,
										  uint16_t* palette) {
	OptTextureData* sourceTexture;
	int width;
	int height;
	D3DInfoNode* d3dInfo;
	int mipLevelCount;
	int mipLevel;
	int lightmapRemapIndex;
	int lightmapEnabled;
	Std3DTextureFormatMode fmt;
	Std3DVBuffer baseVBuffers[6];
	Std3DVBuffer lightmapVBuffers[6];
	Std3DVBuffer* apSrcRasters[6];
	Std3DTextureSurface* apLightmapSurfaces[6];
	void* apPalettes[6];
	Std3DTextureSurface* apOutSurfaces[6];
	Std3DVBuffer* apSrcVBuffers[6];
	void* apAlphaMasks[6];

	sourceTexture = textureData;
	width = sourceTexture->width;
	height = sourceTexture->height;
	if (width != 0 && height != 0) {

		d3dInfo = D3DInfo_AllocOrReuse(textureId);
		if (d3dInfo->mipLevelCount != 0) {
			return d3dInfo;
		}

		mipLevelCount = D3DInfo_ComputeMipLevelCount(sourceTexture);
		lightmapEnabled = brightPalette[256];
		lightmapRemapIndex = palette[256];
		if (lightmapRemapIndex < 0) {
			DebugPrintf("NEGATIVE REMAP INDEX");
		}

		{
			int lightmapCount;
			int lightmapStopped;

			/* Two parallel arrays of six source rasters: the base texel data and, for
			 * self-illuminated textures, the extracted lightmap plane. std3D_CreateMipSurface
			 * consumes them level-by-level and fills the surface handle arrays. */
			memset(baseVBuffers, 0, sizeof(baseVBuffers));
			memset(lightmapVBuffers, 0, sizeof(lightmapVBuffers));
			memset(apSrcRasters, 0, sizeof(apSrcRasters));
			memset(apSrcVBuffers, 0, sizeof(apSrcVBuffers));
			memset(apOutSurfaces, 0, sizeof(apOutSurfaces));
			memset(apLightmapSurfaces, 0, sizeof(apLightmapSurfaces));
			memset(apPalettes, 0, sizeof(apPalettes));
			memset(apAlphaMasks, 0, sizeof(apAlphaMasks));
			for (mipLevel = 0; mipLevel < 6; ++mipLevel) {
				apSrcRasters[mipLevel] = &baseVBuffers[mipLevel];
				apSrcVBuffers[mipLevel] = &lightmapVBuffers[mipLevel];
			}

			{
				int offset = 0;

				lightmapCount = 0;
				lightmapStopped = 0;
				for (mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
					baseVBuffers[mipLevel].pixels = (uint8_t*)pixelData + offset;
					baseVBuffers[mipLevel].storageType = 0;
					baseVBuffers[mipLevel].raster.width = (uint32_t)width;
					baseVBuffers[mipLevel].raster.height = (uint32_t)height;
					baseVBuffers[mipLevel].raster.rowPitch = (uint32_t)width;
					baseVBuffers[mipLevel].raster.sourceType = 0; /* 8-bit indexed source */
					baseVBuffers[mipLevel].raster.bitsPerPixel = 8;
					if (alphaData != NULL) {
						apPalettes[mipLevel] = (uint8_t*)alphaData + offset;
					}

					/* Extract this mip's lightmap plane while the chain is still consecutive.
					 * brightPalette[256] gates lightmap generation; palette[] flags lit texels
					 * and palette[256] remaps index 0 so the glow survives color-key. */
					if (lightmapEnabled != 0 && textureName != NULL && textureName[0] != '_' &&
						!lightmapStopped) {
						const uint8_t* srcPixel = (const uint8_t*)pixelData + offset;
						uint8_t* lightmapPixel = &g_d3dLightmapScratchPixels[offset];
						int hasLightmap = 0;
						int remainingPixels = width * height;

						while (remainingPixels != 0) {
							uint8_t index = *srcPixel++;

							if (palette[index] != 0) {
								hasLightmap = 1;
								if (index == 0) {
									index = (uint8_t)lightmapRemapIndex;
								}
								*lightmapPixel = index;
							} else {
								*lightmapPixel = 0;
							}
							++lightmapPixel;
							--remainingPixels;
						}
						if (!hasLightmap) {
							lightmapStopped = 1;
						} else {
							lightmapVBuffers[mipLevel].pixels = &g_d3dLightmapScratchPixels[offset];
							lightmapVBuffers[mipLevel].storageType = 0;
							lightmapVBuffers[mipLevel].raster.width = (uint32_t)width;
							lightmapVBuffers[mipLevel].raster.height = (uint32_t)height;
							lightmapVBuffers[mipLevel].raster.rowPitch = (uint32_t)width;
							lightmapVBuffers[mipLevel].raster.sourceType = 0;
							lightmapVBuffers[mipLevel].raster.bitsPerPixel = 8;
							if (lightmapCount != mipLevel) {
								DebugPrintf("**WARNING: non-consecutive lightmaps in CreateD3DfromTexture()");
							}
							++lightmapCount;
						}
					}

					offset += width * height;
					width >>= 1;
					height >>= 1;
				}
			}

			/* Select the base destination texel format and prime its converter from the
			 * bright palette: RGBA4444 when a separate alpha plane exists, PAL8 when the
			 * device keeps palettized textures, otherwise straight RGB565. */
			if (alphaData != NULL) {
				fmt = STD3D_TEXFMT_RGBA4444;
				std3D_ConvertTexTo4444(brightPalette, NULL, 256);
			} else if (g_usePalettizedTextures) {
				fmt = STD3D_TEXFMT_PAL8;
				std3D_CreatePaletteForTexture(brightPalette);
			} else {
				fmt = STD3D_TEXFMT_RGB565;
				std3D_CopyPaletteToScratch16(brightPalette, 256);
			}
			std3D_CreateMipSurface(apOutSurfaces, apSrcRasters, fmt, apPalettes, mipLevelCount,
								   g_hwMipmapFilter);

			/* Lightmap surfaces are RGBA1555. palette[0] is temporarily forced to the
			 * color-escape-bypass glow color (saved into the remap slot and restored). */
			if (lightmapVBuffers[0].pixels != NULL && lightmapCount != 0) {
				int8_t hasAlphaTexture = g_pStd3DCurDevice->caps.bAlphaTexture;

				palette[lightmapRemapIndex] = palette[0];
				if (hasAlphaTexture) {
					uint16_t glow = g_flightTextPalette[g_flightColorEscapeBypassChar];
					palette[0] = glow;
					std3D_ConvertTexTo1555(palette, glow, 256);
					palette[0] = palette[lightmapRemapIndex];
				} else {
					palette[0] = g_flightTextPalette[g_flightColorEscapeBypassChar];
					std3D_CopyPaletteToScratch16(palette, 256);
					palette[0] = palette[lightmapRemapIndex];
				}
				palette[lightmapRemapIndex] = 0;
				std3D_CreateMipSurface(apLightmapSurfaces, apSrcVBuffers, STD3D_TEXFMT_RGBA1555, apAlphaMasks,
									   lightmapCount, g_hwMipmapFilter);
			}

			d3dInfo->mipLevelCount = mipLevelCount;
			for (mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
				d3dInfo->baseMipSurfaces[mipLevel] = apOutSurfaces[mipLevel];
				d3dInfo->lightmapMipSurfaces[mipLevel] = apLightmapSurfaces[mipLevel];
			}
			d3dInfo->hasAlphaData = (uint8_t)(uintptr_t)alphaData;
			d3dInfo->textureData = *textureData;
			return d3dInfo;
		}
	}

	return D3DInfo_AllocOrReuse(textureId);
}

// FUNCTION: XWA 0x4CD100
int OptModel_UploadTextureNodesRecursive(OptNode* node, intptr_t* textureIds, int textureCount,
										 int maxTextureIds) {
	int childIndex;

	if (node == NULL) {
		return textureCount;
	}

	if (node->nodeType == OPT_TEXTURE) {
		OptTextureData* textureData;
		void* alphaData;
		uint16_t* palette;
		uint16_t* brightPalette;

		(void)maxTextureIds;
		textureData = (OptTextureData*)node->param2;
		palette = (uint16_t*)OptModel_AddressToPtr(g_optTextureUploadNative, textureData->paletteAddress, 2);
		if (palette == NULL) {
			Aeron_LogError("xwa.assets", "Invalid OPT texture palette address 0x%08x",
					  textureData->paletteAddress);
			textureIds[textureCount] = 0;
			return textureCount + 1;
		}
		brightPalette = palette + 2048;
		alphaData = node->childCount != 0 ? node->pChildren[0]->param2 : NULL;
		textureIds[textureCount] = (intptr_t)D3DInfo_CreateFromOptTexture(
			node->pName, (int)node->param1, textureData, textureData + 1, alphaData, brightPalette, palette);
		return textureCount + 1;
	}

	for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
		textureCount = OptModel_UploadTextureNodesRecursive(node->pChildren[childIndex], textureIds,
															textureCount, maxTextureIds);
	}
	return textureCount;
}

// FUNCTION: XWA 0x4CD030
int OptModel_BuildHardwareData(OptimizedPolyObject* model, int modelSize) {
	NativeOptimizedPolyObject* native;
	int textureIndex;
	int rootIndex;
	intptr_t textureIds[200];

	if (model == NULL) {
		return modelSize;
	}

	native = OptModel_AsNative(model);
	g_optTextureUploadNative = native;
	std3D_FreePalettes();
	textureIndex = 0;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		textureIndex =
			OptModel_UploadTextureNodesRecursive(model->rootNodes[rootIndex], textureIds, textureIndex, 200);
	}
	std3D_FreePalettes();
	textureIndex = 0;
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		textureIndex = OptModel_ReplaceTextureNodesWithRefsRecursive(model->rootNodes[rootIndex], model,
																	 textureIds, textureIndex, &modelSize);
	}
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptModel_ResolveTextureRefsRecursive(model->rootNodes[rootIndex], model, &modelSize);
	}
	g_optTextureUploadNative = NULL;
	return modelSize;
}

// FUNCTION: XWA 0x485090
OptNode* OptModel_FindNodeByName(OptNode* node, const char* name) {
	OptNode* result;
	int childIndex;

	if (node == NULL) {
		return NULL;
	}

	if (node->pName != NULL && Xwa_CrtStricmp(node->pName, name) == 0) {
		return node;
	}

	for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
		result = OptModel_FindNodeByName(node->pChildren[childIndex], name);
		if (result != NULL) {
			return result;
		}
	}

	return NULL;
}

// FUNCTION: XWA 0x484FE0
OptNode* OptModel_ResolveNodeRef(OptNode* refNode, OptimizedPolyObject* model) {
	OptNode* resolvedNode;
	int rootIndex;

	while (refNode != NULL && refNode->nodeType == OPT_NODEREF) {
		resolvedNode = (OptNode*)refNode->param1;
		if (resolvedNode == NULL) {
			const char* refName = (const char*)refNode->param2;

			for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
				OptNode* root = model->rootNodes[rootIndex];
				int childIndex;

				if (root != NULL) {
					if (root->pName != NULL && Xwa_CrtStricmp(root->pName, refName) == 0) {
						resolvedNode = root;
					} else {
						for (childIndex = 0; childIndex < root->childCount; ++childIndex) {
							resolvedNode = OptModel_FindNodeByName(root->pChildren[childIndex], refName);
							if (resolvedNode != NULL) {
								break;
							}
						}
					}
				}

				if (resolvedNode != NULL) {
					break;
				}
			}
			refNode->param1 = (intptr_t)resolvedNode;
		}
		refNode = resolvedNode;
	}

	return refNode;
}

// FUNCTION: XWA 0x4417B0
void D3DInfo_Release(D3DInfoNode* d3dInfo) {
	int mipLevel;
	D3DInfoNode* next;
	D3DInfoNode* prev;

	if (d3dInfo->refCount <= 0) {
		DebugPrintf("bad D3DINFO in RleaseD3DINFO()");
	}
	--d3dInfo->refCount;
	if (d3dInfo->refCount != 0) {
		return;
	}

	for (mipLevel = 0; mipLevel < d3dInfo->mipLevelCount; ++mipLevel) {
		if (d3dInfo->baseMipSurfaces[mipLevel] != NULL) {
			std3D_DeleteTextureSurface(d3dInfo->baseMipSurfaces[mipLevel]);
			d3dInfo->baseMipSurfaces[mipLevel] = NULL;
		}
		if (d3dInfo->lightmapMipSurfaces[mipLevel] != NULL) {
			std3D_DeleteTextureSurface(d3dInfo->lightmapMipSurfaces[mipLevel]);
			d3dInfo->lightmapMipSurfaces[mipLevel] = NULL;
		}
	}
	next = d3dInfo->next;
	prev = d3dInfo->prev;
	if (prev != NULL) {
		prev->next = next;
	} else {
		g_d3dInfoListHead = next;
	}
	if (next != NULL) {
		next->prev = prev;
	}
	--g_d3dInfoActiveCount;
	memset(d3dInfo, 0, sizeof(*d3dInfo));
	d3dInfo->prev = NULL;
	d3dInfo->next = g_d3dInfoFreeListHead;
	g_d3dInfoFreeListHead = d3dInfo;
}

// FUNCTION: XWA 0x4CD340
void OptModel_ReleaseD3DInfoRecursive(OptNode* node) {
	int childIndex;

	if (node == NULL) {
		return;
	}

	if (node->nodeType == OPT_TEXTURE_REF) {
		if (node->param1 == 1) {
			D3DInfo_Release(node->param2);
		}
		node->param2 = NULL;
	}

	for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
		OptModel_ReleaseD3DInfoRecursive(node->pChildren[childIndex]);
	}
}

static void OptModel_RetainD3DInfoRecursive(OptNode* node) {
	int childIndex;

	if (node == NULL) {
		return;
	}

	if (node->nodeType == OPT_TEXTURE_REF && node->param1 == 1 && node->param2 != NULL) {
		D3DInfoNode* d3dInfo = (D3DInfoNode*)node->param2;

		++d3dInfo->refCount;
	}

	for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
		OptModel_RetainD3DInfoRecursive(node->pChildren[childIndex]);
	}
}

static void OptModel_RetainD3DInfoRefs(OptimizedPolyObject* model) {
	int rootIndex;

	if (model == NULL) {
		return;
	}

	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptModel_RetainD3DInfoRecursive(model->rootNodes[rootIndex]);
	}
}

// FUNCTION: XWA 0x4CCA60
int OptModel_FreeHandle(uint16_t modelHandle) {
	uint16_t handle;
	NativeOptimizedPolyObject* native;
	int rootIndex;

	handle = OptModel_NormalizeHandle(modelHandle);
	if (handle == 0) {
		return 1;
	}

	native = (NativeOptimizedPolyObject*)Memory_LockHandle(handle);
	if (native != NULL) {
		OptModel_AdjustOptimizedPolyObjectPointers(&native->model);
		for (rootIndex = 0; rootIndex < native->model.rootNodeCount; ++rootIndex) {
			OptNode* root;

			root = native->model.rootNodes[rootIndex];
			if (root != NULL) {
				int childIndex;

				if (root->nodeType == OPT_TEXTURE_REF) {
					if (root->param1 == 1) {
						D3DInfo_Release(root->param2);
					}
					root->param2 = NULL;
				}

				for (childIndex = 0; childIndex < root->childCount; ++childIndex) {
					OptModel_ReleaseD3DInfoRecursive(root->pChildren[childIndex]);
				}
			}
		}
		OptModel_DestroyNativeModel(native);
		Memory_UnlockHandle(handle);
	}

	if (g_loadOptBufHandle == handle) {
		g_loadOptBufHandle = 0;
		g_loadOptBufSize = 0;
	}

#ifdef XWA_MODERN
	XwaSnapshot_NoteOptFree((uint16_t)(handle | 0x8000u));
#endif
	Memory_FreeHandle(0, handle);
	return 1;
}

// FUNCTION: XWA 0x4CC940
uint16_t OptModel_LoadHandle(const char* fileName) {
	int version;
	uint16_t tempHandle;
	uint16_t modelHandle;
	NativeOptimizedPolyObject* tempNative;
	NativeOptimizedPolyObject* modelNative;
	int modelSize;
	int rootIndex;
	int meshCount;

	tempHandle = OptModel_LoadFileToHandle(fileName, &version);
	if (tempHandle == 0) {
		return 0;
	}

	(void)Memory_GetHandleSize(tempHandle);
	tempNative = (NativeOptimizedPolyObject*)Memory_LockHandle(tempHandle);
	if (tempNative == NULL) {
		return 0;
	}

	OptModel_AdjustOptimizedPolyObjectPointers(&tempNative->model);
	modelSize = OptModel_ComputeProcessedModelSize(&tempNative->model, 2);
	sw3d_AllocSceneModelLists();

	for (rootIndex = 0; rootIndex < tempNative->model.rootNodeCount; ++rootIndex) {
		modelSize =
			OptModel_PrepareTextures(tempNative->model.rootNodes[rootIndex], &tempNative->model, modelSize);
	}

	if (g_useHardware3D) {
		modelSize = OptModel_BuildHardwareData(&tempNative->model, modelSize);
	}

	modelHandle = Memory_AllocHandle(0, sizeof(NativeOptimizedPolyObject));
	if (modelHandle == 0) {
		Memory_UnlockHandle(tempHandle);
		return 0;
	}

	modelNative = (NativeOptimizedPolyObject*)Memory_LockHandle(modelHandle);
	if (modelNative == NULL || !OptModel_CloneNativeModel(modelNative, tempNative)) {
		if (modelNative != NULL) {
			Memory_UnlockHandle(modelHandle);
		}
		Memory_FreeHandle(0, modelHandle);
		Memory_UnlockHandle(tempHandle);
		return 0;
	}

	(void)modelSize;
	OptModel_AdjustOptimizedPolyObjectPointers(&modelNative->model);
	OptModel_RetainD3DInfoRefs(&modelNative->model);

	meshCount = modelNative->model.rootNodeCount;
	if (meshCount > 0 && (modelNative->model.rootNodes[0]->nodeType == OPT_TEXTURE ||
						  modelNative->model.rootNodes[0]->nodeType == OPT_TEXTURE_REF)) {
		--meshCount;
	}
	Aeron_LogInfo("xwa.assets",
			  "Loaded OPT '%s' handle=%u public=0x%04x version=%d roots=%d meshes=%d hardware=%d", fileName,
			  (unsigned)modelHandle, (unsigned)(modelHandle | 0x8000), version,
			  modelNative->model.rootNodeCount, meshCount, g_useHardware3D);

#ifdef XWA_MODERN
	/* Snapshot the processed model's lifetime. The remaster mirrors this
	 * authoritative set after the tick instead of loading from draw calls. */
	XwaSnapshot_NoteOptLoad((uint16_t)(modelHandle | 0x8000), fileName);
#endif

	Memory_UnlockHandle(tempHandle);
	Memory_UnlockHandle(modelHandle);

	return (uint16_t)(modelHandle | 0x8000);
}
