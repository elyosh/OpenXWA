#include "xwa/flight/fediskio.h"
#include "xwa/render/renderer_internal.h"

#include "xwa/assets/file_io.h"

enum {
	SCENE_SPAN_DATA_COUNT = 40000,
	SCENE_SPAN_PTR_COUNT = 40000,
	SCENE_FACE_COUNT = 10000,
	/* One list head per scanline at the tallest flight resolution (1600x1200). */
	SCENE_SCANLINE_COUNT = 1200,
	SCENE_PHONG_SLOT_COUNT = 200,
	SCENE_PHONG_BASE_BYTES = 2412,
	SCENE_PHONG_WIDTH_SAMPLES = 0x640,
	SCENE_COMPOSITE_MESH_COUNT = 1000,
	MODEL_SCENE_VERTEX_CAPACITY = 400,
	MODEL_SCENE_EDGE_CAPACITY = 400,
};

// FUNCTION: XWA 0x488E90
void* sw3d_AllocSceneBuffers(void) {
	size_t scenePhongSize;

	if (g_sceneSpanDataHandle != 0 || g_sceneSpanPtrListHandle != 0 || g_visFaceListHandle != 0 ||
		g_sceneSclEdgeListHandle != 0 || g_scanlineSpanHeadsHandle != 0) {
		DebugPrintf("one or more buffers already allocated in AllocSceneBuffers()");
	}

	g_sceneSpanDataMax = SCENE_SPAN_DATA_COUNT;
	g_sceneSpanDataHandle =
		Memory_AllocHandle("SCENESPANDATA", (size_t)SCENE_SPAN_DATA_COUNT * sizeof(SceneSpan));
	if (g_sceneSpanDataHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_sceneSpanData = (uint8_t*)Memory_LockHandle(g_sceneSpanDataHandle);

	g_sceneSpanPtrMax = SCENE_SPAN_PTR_COUNT;
	g_sceneSpanPtrListHandle =
		Memory_AllocHandle("SCENESPANPTRLIST", (size_t)SCENE_SPAN_PTR_COUNT * sizeof(SceneSpan*));
	if (g_sceneSpanPtrListHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_sceneSpanPtrList = (uint8_t*)Memory_LockHandle(g_sceneSpanPtrListHandle);

	g_sceneFaceMax = SCENE_FACE_COUNT;
	g_visFaceListHandle = Memory_AllocHandle("SCENEFACELIST", (size_t)g_sceneFaceMax * sizeof(SceneFace));
	if (g_visFaceListHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_visFaceList = (SceneFace*)Memory_LockHandle(g_visFaceListHandle);

	g_sceneSclEdgeListHandle =
		Memory_AllocHandle("SCENESCLEDGELIST", (size_t)SCENE_SCANLINE_COUNT * sizeof(SceneEdge*));
	if (g_sceneSclEdgeListHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_sceneSclEdgeList = (uint8_t*)Memory_LockHandle(g_sceneSclEdgeListHandle);

	g_scanlineSpanHeadsHandle =
		Memory_AllocHandle("SCENESPANLIST", (size_t)SCENE_SCANLINE_COUNT * sizeof(SceneSpan*));
	if (g_scanlineSpanHeadsHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_scanlineSpanHeads = (uint8_t*)Memory_LockHandle(g_scanlineSpanHeadsHandle);

	scenePhongSize = (size_t)SCENE_PHONG_SLOT_COUNT * sizeof(SoftwareLightSample) *
						 (SCENE_PHONG_WIDTH_SAMPLES / (unsigned int)g_sw3dLightSampleBlockSize) +
					 SCENE_PHONG_BASE_BYTES;
	g_scenePhongDataHandle = Memory_AllocHandle("SCENEPHONGDATA", scenePhongSize);
	if (g_scenePhongDataHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_scenePhongData = (uint8_t*)Memory_LockHandle(g_scenePhongDataHandle);

	g_meshQueueMax = SCENE_COMPOSITE_MESH_COUNT;
	g_meshQueueHandle = Memory_AllocHandle("SCENECOMPDATA", (size_t)g_meshQueueMax * sizeof(SceneMesh));
	if (g_meshQueueHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_meshQueue = (SceneMesh*)Memory_LockHandle(g_meshQueueHandle);
	return g_meshQueue;
}

// FUNCTION: XWA 0x488E70
void sw3d_InitSceneBuffers(void) {
	sw3d_AllocSceneBuffers();
	g_modelEdgeCapacity = MODEL_SCENE_EDGE_CAPACITY;
	g_modelVertCapacity = MODEL_SCENE_VERTEX_CAPACITY;
	sw3d_AllocSceneModelLists();
}

// FUNCTION: XWA 0x4890B0
void sw3d_AllocSceneModelLists(void) {
	if (g_projVertListHandle == 0 || g_projVertMax < 4 * g_modelVertCapacity) {
		if (g_projVertListHandle != 0) {
			Memory_UnlockHandle(g_projVertListHandle);
			Memory_FreeHandle("SCENEVERTLIST", g_projVertListHandle);
		}
		g_projVertListHandle =
			Memory_AllocHandle("SCENEVERTLIST", (size_t)(4 * g_modelVertCapacity) * sizeof(ProjVertex));
		if (g_projVertListHandle == 0) {
			FeDiskIo_FatalError(0);
		}
		g_projVertList = (ProjVertex*)Memory_LockHandle(g_projVertListHandle);
		g_projVertMax = 4 * g_modelVertCapacity;
	}

	if (g_sceneEdgeListHandle == 0 || g_sceneEdgeMax < 4 * g_modelEdgeCapacity) {
		if (g_sceneEdgeListHandle != 0) {
			Memory_UnlockHandle(g_sceneEdgeListHandle);
			Memory_FreeHandle("SCENEEDGELIST", g_sceneEdgeListHandle);
		}
		g_sceneEdgeListHandle =
			Memory_AllocHandle("SCENEEDGELIST", (size_t)(4 * g_modelEdgeCapacity) * sizeof(SceneEdge));
		if (g_sceneEdgeListHandle == 0) {
			FeDiskIo_FatalError(0);
		}
		g_sceneEdgeList = Memory_LockHandle(g_sceneEdgeListHandle);
		g_sceneEdgeMax = 4 * g_modelEdgeCapacity;
	}

	if (g_vertexRemapHandle == 0 || g_vertexRemapCapacity < g_modelVertCapacity) {
		if (g_vertexRemapHandle != 0) {
			Memory_UnlockHandle(g_vertexRemapHandle);
			Memory_FreeHandle("SCENEVERTFLAGS", g_vertexRemapHandle);
		}
		g_vertexRemapHandle =
			Memory_AllocHandle("SCENEVERTFLAGS", (size_t)g_modelVertCapacity * sizeof(int));
		if (g_vertexRemapHandle == 0) {
			FeDiskIo_FatalError(0);
		}
		g_vertexRemap = (int*)Memory_LockHandle(g_vertexRemapHandle);
		g_vertexRemapCapacity = g_modelVertCapacity;
	}

	if (g_sceneEdgeFlagsHandle == 0 || g_sceneEdgeFlagsCapacity < g_modelEdgeCapacity) {
		if (g_sceneEdgeFlagsHandle != 0) {
			Memory_UnlockHandle(g_sceneEdgeFlagsHandle);
			Memory_FreeHandle("SCENEEDGEFLAGS", g_sceneEdgeFlagsHandle);
		}
		g_sceneEdgeFlagsHandle =
			Memory_AllocHandle("SCENEEDGEFLAGS", (size_t)g_modelEdgeCapacity * sizeof(int));
		if (g_sceneEdgeFlagsHandle == 0) {
			FeDiskIo_FatalError(0);
		}
		g_sceneEdgeFlags = Memory_LockHandle(g_sceneEdgeFlagsHandle);
		g_sceneEdgeFlagsCapacity = g_modelEdgeCapacity;
	}

	(void)g_projVertList;
	(void)g_sceneEdgeList;
	(void)g_vertexRemap;
	(void)g_sceneEdgeFlags;
}
