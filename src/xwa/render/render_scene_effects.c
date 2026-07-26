#include "xwa/render/effects.h"
#include "xwa/render/renderer_internal.h"

#include <stdio.h>

#ifndef XWA_MODERN
#define XWA_HUD_STDCALL __stdcall
__declspec(dllimport) void XWA_HUD_STDCALL OutputDebugStringA(const char* lpOutputString);
#else
#define XWA_HUD_STDCALL
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#endif

// GLOBAL: XWA 0x5A94FC
const float flt_5A94FC = 0.25f;

// FUNCTION: XWA 0x448340
RenderBatch* RenderScene_FlushGeometry(void) {
	if (g_d3dVertexCount && g_d3dIndexCount) {
		if (g_std3DStartScenePending) {
			std3D_StartScene();
			g_std3DStartScenePending = 0;
		}
		std3D_LockExecuteBuffer();
		std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
		std3D_BeginInstructions();
		std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
		std3D_ExecuteBuffer();
		g_d3dVertexCount = 0;
		g_d3dIndexCount = 0;
	}
	return RenderScene_FlushDeferredMeshBatches();
}

// FUNCTION: XWA 0x448530
char RenderScene_EffectsPass(void) {
	char result;

	if (g_std3DStartScenePending) {
		std3D_StartScene();
		g_std3DStartScenePending = 0;
	}
	if (g_d3dVertexCount) {
		std3D_LockExecuteBuffer();
		std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
		std3D_BeginInstructions();
		std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
		std3D_ExecuteBuffer();
		g_d3dVertexCount = 0;
		g_d3dIndexCount = 0;
	}
	RenderScene_FlushDeferredMeshBatches();
	if (g_drawSceneEffects) {
		if (g_worldParticleEffects) {
			Particle_UpdateWorldEffects();
		}
		Particle_UpdateObjectEffects();
		ObjectTrail_RenderObjectTrails();
		SceneBillboard_RenderQueuedTextured(1);
		Hud_RenderHud();
		LensFlare_RenderQueuedSources();
		Targeting_DrawSceneObjectBoxes();
	} else {
		if (g_players[g_localPlayer].hyperspacePhase) {
			Hud_RenderHud();
			FlightText_FlushQueue();
		}
		SceneBillboard_RenderQueuedTextured(0);
	}

	result = (char)g_d3dVertexCount;
	g_sceneBillboardQueueCount = 0;
	if (g_d3dVertexCount && g_d3dIndexCount) {
		std3D_LockExecuteBuffer();
		std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
		std3D_BeginInstructions();
		std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
		result = std3D_ExecuteBuffer();
		g_d3dVertexCount = 0;
		g_d3dIndexCount = 0;
	}
	return result;
}

// FUNCTION: XWA 0x448660
void RenderScene_End3D(void) {
	int totalMemory;
	int availableMemory;
	int frameTriCount;
	uint8_t dumpRequested;
	void(XWA_HUD_STDCALL * debugPrint)(const char*);
	char debugLine[96];

	if (!g_std3DStartScenePending) {
		frameTriCount = g_frameTriCount;
		g_flightRenderStatStateChanges = g_frameStateChanges;
		g_flightRenderStatTexSwitches = g_frameTexSwitches;
		g_flightRenderStatBytesCached = g_frameBytesCached;
		g_flightRenderStatBytesPurged = g_frameBytesPurged;
		g_flightRenderStatVertCount = g_frameVertCount;
		g_flightRenderStatTriCount = frameTriCount;
		totalMemory = (int)g_pStd3DCurDevice->totalMemory;
		availableMemory = (int)g_pStd3DCurDevice->availableMemory;
		g_flightRenderStatTexCreateFailed = g_bTexCreateFailed;
		g_flightRenderStatTexCacheOverflow = g_bTexCacheOverflow;
		dumpRequested = g_flightRenderStatsDumpRequested;
		g_flightRenderStatTexMemUsedBytes = totalMemory - availableMemory;
		if (dumpRequested) {
			sprintf(debugLine, "sceneNumVerts = %d sceneNumTris = %d \n", g_frameVertCount, frameTriCount);
			debugPrint = OutputDebugStringA;
			debugPrint(debugLine);
			sprintf(debugLine, "sceneNumTexChanges = %d sceneNumStateChanges = %d \n", g_frameTexSwitches,
					g_frameStateChanges);
			debugPrint(debugLine);
			sprintf(debugLine, "Tex Active %d Tex Thrash %d Total Tex %d \n", g_frameBytesCached,
					g_frameBytesPurged, g_pStd3DCurDevice->totalMemory - g_pStd3DCurDevice->availableMemory);
			debugPrint(debugLine);
			sprintf(debugLine, "framerate = %3.2f\n",
					(g_fpsSampleHistory[1] + g_fpsSampleHistory[2] + g_fpsSampleHistory[3] +
					 g_fpsSampleHistory[4]) *
						flt_5A94FC);
			debugPrint(debugLine);
			g_flightRenderStatsDumpRequested = 0;
		}
		std3D_EndScene();
		g_std3DStartScenePending = 1;
	}
}
