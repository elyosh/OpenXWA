#include "xwa/flight/fediskio.h"
#include "xwa/flight/net_session.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer_internal.h"

#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/object/laser.h"

#ifndef XWA_MODERN
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* outputString);
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#else
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#define g_OutputDebugStringA OutputDebugStringA
#endif

// GLOBAL: XWA 0x5A9EEC
static float g_shieldGlowTurboScale = 0.60000002f;
// GLOBAL: XWA 0x5A9EF0
static float g_shieldGlowProjectileScale = 0.40000001f;
// GLOBAL: XWA 0x5A9EF4
static float g_shieldGlowHeavyWarheadScale = 0.25f;
// GLOBAL: XWA 0x5A9FC0
static const float g_glowMarkSurfaceEffectBaseScale = 0.1f;
// GLOBAL: XWA 0x5A9FF4
static const float g_glowMarkRandU16ToUnitFloatScale = 0.00001525902f;
// GLOBAL: XWA 0x5A9FF8
static const float g_glowMarkDamageEffectScale = -0.75f;
// GLOBAL: XWA 0x5A9FFC
static const float g_glowMarkAccelRingEffectScale = -2.75f;
// GLOBAL: XWA 0x5A9F0C
const float g_glowMarkPlaneDistanceLimit = 1.0f;
// GLOBAL: XWA 0x5A9F10
const float g_glowMarkPlaneZero = 0.0f;
// GLOBAL: XWA 0x5A9F14
const float g_glowMarkUvMin = -0.5f;
// GLOBAL: XWA 0x5A9F18
const float g_glowMarkUvMax = 0.5f;
// GLOBAL: XWA 0x5A9F2C
const float g_localPlayerHitRandomScale = 0.00001525902f;
// GLOBAL: XWA 0x5A9F30
const float g_localPlayerHitScaleRange = -15.0f;
// GLOBAL: XWA 0x5A9F34
const float g_localPlayerHitScaleBase = 5.0f;

// GLOBAL: XWA 0x5A9408
const float g_engineGlowZero = 0.0f;
// GLOBAL: XWA 0x5A9410
const double g_engineGlowPowerMarginScale = 0.0625;
// GLOBAL: XWA 0x5A9418
const float g_engineGlowOutputScale = 0.000015259022f;
// GLOBAL: XWA 0x5A941C
const float g_engineGlowFlickerScale = 0.0040000002f;
// GLOBAL: XWA 0x5A9420
const float g_engineGlowOne = 1.0f;
// GLOBAL: XWA 0x5A9428
const double g_engineGlowThrottleScale = 0.00001525902189669642;
// GLOBAL: XWA 0x5A9430
const float g_engineGlowMinimumScale = 0.34999999f;
// GLOBAL: XWA 0x5A9438
const float g_engineGlowBoostScale = -11.0f;
// GLOBAL: XWA 0x5A9458
const float g_engineGlowRectScaleLimit = 0.80000001f;
// GLOBAL: XWA 0x5A9460
const float g_engineGlowRectHalfScale = 0.5f;
// GLOBAL: XWA 0x5A9468
const double g_engineGlowRectQ15Scale = 0.00003051850947599719;

enum {
	GLOW_MARK_PATCH_POOL_COUNT = 24,
	BLAST_MARK_PATCH_POOL_COUNT = 32,
};

// FUNCTION: XWA 0x4E6FE0
void GlowMark_InitFrameScalesAndPools(void) {
	ObjectMeshTextureLayerBlock* patch;
	uint16_t patchIndex;

	{
		ModelTypeInfo* modelInfo;
		uint16_t maxExtent;
		uint16_t frame;

		modelInfo = &g_modelTypeTable[OBJ_AnimationTextureGroup3100];
		maxExtent = 0;
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup3100, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				int extent;
				uint16_t width;

				width = texLevel->width;
				extent = (int)width << texLevel->shift;
				if (extent > maxExtent) {
					maxExtent = (uint16_t)(width << texLevel->shift);
				}
			}
			++frame;
		}

		if (g_shieldGlowFrameScales != NULL) {
			Memory_FreeTagged("SHIELDGLOWSCALEARRAY", g_shieldGlowFrameScales);
		}

		g_shieldGlowFrameScales = (uint8_t*)Memory_AllocTagged("SHIELDGLOWSCALEARRAY", modelInfo->frameCount);
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup3100, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				g_shieldGlowFrameScales[frame] = (uint8_t)((maxExtent >> texLevel->shift) / texLevel->width);
			} else {
				g_shieldGlowFrameScales[frame] = 0;
			}
			++frame;
		}
	}

	{
		ModelTypeInfo* modelInfo;
		uint16_t maxExtent;
		uint16_t frame;

		modelInfo = &g_modelTypeTable[OBJ_AnimationTextureGroup2007];
		maxExtent = 0;
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup2007, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				int extent;
				uint16_t width;

				width = texLevel->width;
				extent = (int)width << texLevel->shift;
				if (extent > maxExtent) {
					maxExtent = (uint16_t)(width << texLevel->shift);
				}
			}
			++frame;
		}

		if (g_magElectFrameScales != NULL) {
			Memory_FreeTagged("MAGELECTSCALEARRAY", g_magElectFrameScales);
		}

		g_magElectFrameScales = (uint8_t*)Memory_AllocTagged("MAGELECTSCALEARRAY", modelInfo->frameCount);
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup2007, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				g_magElectFrameScales[frame] = (uint8_t)((maxExtent >> texLevel->shift) / texLevel->width);
			} else {
				g_magElectFrameScales[frame] = 0;
			}
			++frame;
		}
	}

	{
		ModelTypeInfo* modelInfo;
		uint16_t maxExtent;
		uint16_t frame;

		modelInfo = &g_modelTypeTable[OBJ_AnimationTextureGroup3055];
		maxExtent = 0;
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup3055, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				int extent;
				uint16_t width;

				width = texLevel->width;
				extent = (int)width << texLevel->shift;
				if (extent > maxExtent) {
					maxExtent = (uint16_t)(width << texLevel->shift);
				}
			}
			++frame;
		}

		if (g_tractorBeamFrameScales != NULL) {
			Memory_FreeTagged("TRACTORBEAMSCALEARRAY", g_tractorBeamFrameScales);
		}

		g_tractorBeamFrameScales =
			(uint8_t*)Memory_AllocTagged("TRACTORBEAMSCALEARRAY", modelInfo->frameCount);
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup3055, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				g_tractorBeamFrameScales[frame] = (uint8_t)((maxExtent >> texLevel->shift) / texLevel->width);
			} else {
				g_tractorBeamFrameScales[frame] = 0;
			}
			++frame;
		}
	}

	{
		ModelTypeInfo* modelInfo;
		uint16_t maxExtent;
		uint16_t frame;

		modelInfo = &g_modelTypeTable[OBJ_AnimationTextureGroup3005];
		maxExtent = 0;
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup3005, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				int extent;
				uint16_t width;

				width = texLevel->width;
				extent = (int)width << texLevel->shift;
				if (extent > maxExtent) {
					maxExtent = (uint16_t)(width << texLevel->shift);
				}
			}
			++frame;
		}

		if (g_hullLightFrameScales != NULL) {
			Memory_FreeTagged("HULLLIGHTSCALEARRAY", g_hullLightFrameScales);
		}

		g_hullLightFrameScales = (uint8_t*)Memory_AllocTagged("HULLLIGHTSCALEARRAY", modelInfo->frameCount);
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup3005, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				g_hullLightFrameScales[frame] = (uint8_t)((maxExtent >> texLevel->shift) / texLevel->width);
			} else {
				g_hullLightFrameScales[frame] = 0;
			}
			++frame;
		}
	}

	{
		ModelTypeInfo* modelInfo;
		uint16_t maxExtent;
		uint16_t frame;

		modelInfo = &g_modelTypeTable[OBJ_AnimationTextureGroup2008];
		maxExtent = 0;
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup2008, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				int extent;
				uint16_t width;

				width = texLevel->width;
				extent = (int)width << texLevel->shift;
				if (extent > maxExtent) {
					maxExtent = (uint16_t)(width << texLevel->shift);
				}
			}
			++frame;
		}

		if (g_ionElectFrameScales != NULL) {
			Memory_FreeTagged("IONELECTSCALEARRAY", g_ionElectFrameScales);
		}

		g_ionElectFrameScales = (uint8_t*)Memory_AllocTagged("IONELECTSCALEARRAY", modelInfo->frameCount);
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_AnimationTextureGroup2008, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				g_ionElectFrameScales[frame] = (uint8_t)((maxExtent >> texLevel->shift) / texLevel->width);
			} else {
				g_ionElectFrameScales[frame] = 0;
			}
			++frame;
		}
	}

	{
		ModelTypeInfo* modelInfo;
		uint16_t maxExtent;
		uint16_t frame;

		modelInfo = &g_modelTypeTable[OBJ_BlastMarkTextureGroup3050];
		maxExtent = 0;
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_BlastMarkTextureGroup3050, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				int extent;
				uint16_t width;

				width = texLevel->width;
				extent = (int)width << texLevel->shift;
				if (extent > maxExtent) {
					maxExtent = (uint16_t)(width << texLevel->shift);
				}
			}
			++frame;
		}

		if (g_blastMarkFrameScales != NULL) {
			Memory_FreeTagged("BLASTMARKSCALEARRAY", g_blastMarkFrameScales);
		}

		g_blastMarkFrameScales = (uint8_t*)Memory_AllocTagged("BLASTMARKSCALEARRAY", modelInfo->frameCount);
		frame = 0;
		while (frame < modelInfo->frameCount) {
			TexLevel* texLevel;

			FeDiskIo_SelectTextureFrame(OBJ_BlastMarkTextureGroup3050, (uint16_t)(frame + 1u), 256);
			texLevel = modelInfo->curTexLevel;
			if (texLevel != NULL) {
				g_blastMarkFrameScales[frame] = (uint8_t)((maxExtent >> texLevel->shift) / texLevel->width);
			} else {
				g_blastMarkFrameScales[frame] = 0;
			}
			++frame;
		}
	}

	memset(g_glowMarkPatchPool, 0, sizeof(g_glowMarkPatchPool));
	patch = g_glowMarkPatchPool;
	for (patchIndex = 0; patchIndex < GLOW_MARK_PATCH_POOL_COUNT; ++patchIndex) {
		patch[patchIndex].freeNext = &patch[patchIndex + 1];
		patch[patchIndex].poolIndex = patchIndex;
	}
	g_glowMarkFreeList = g_glowMarkPatchPool;
	patch[patchIndex - 1].freeNext = NULL;
	g_glowMarkMaxActiveIndex = -1;

	memset(g_blastMarkPatchPool, 0, sizeof(g_blastMarkPatchPool));
	patch = g_blastMarkPatchPool;
	for (patchIndex = 0; patchIndex < BLAST_MARK_PATCH_POOL_COUNT; ++patchIndex) {
		patch[patchIndex].freeNext = &patch[patchIndex + 1];
		patch[patchIndex].poolIndex = patchIndex;
	}
	g_blastMarkFreeList = g_blastMarkPatchPool;
	patch[patchIndex - 1].freeNext = NULL;
	g_blastMarkMaxActiveIndex = -1;
}

// FUNCTION: XWA 0x4E7660
void GlowMark_ShutdownFrameScalesAndPools(void) {
	ObjectMeshTextureLayerBlock* block;
	MeshExtraTextureLayer* facePatch;
	void* allocation;
	int blockIndex;
	int faceIndex;

	g_glowMarkRequestCount = 0;

	allocation = g_shieldGlowFrameScales;
	if (allocation != NULL) {
		Memory_FreeTagged("SHIELDGLOWSCALEARRAY", allocation);
		g_shieldGlowFrameScales = NULL;
	}

	allocation = g_magElectFrameScales;
	if (allocation != NULL) {
		Memory_FreeTagged("MAGELECTSCALEARRAY", allocation);
		g_magElectFrameScales = NULL;
	}

	allocation = g_hullLightFrameScales;
	if (allocation != NULL) {
		Memory_FreeTagged("HULLLIGHTSCALEARRAY", allocation);
		g_hullLightFrameScales = NULL;
	}

	allocation = g_ionElectFrameScales;
	if (allocation != NULL) {
		Memory_FreeTagged("IONELECTSCALEARRAY", allocation);
		g_ionElectFrameScales = NULL;
	}

	allocation = g_blastMarkFrameScales;
	if (allocation != NULL) {
		Memory_FreeTagged("BLASTMARKSCALEARRAY", allocation);
		g_blastMarkFrameScales = NULL;
	}

	allocation = g_tractorBeamFrameScales;
	if (allocation != NULL) {
		Memory_FreeTagged("TRACTORBEAMSCALEARRAY", allocation);
		g_tractorBeamFrameScales = NULL;
	}

	for (blockIndex = 0; blockIndex < GLOW_MARK_PATCH_POOL_COUNT; ++blockIndex) {
		block = &g_glowMarkPatchPool[blockIndex];
		for (faceIndex = 0; faceIndex < 16; ++faceIndex) {
			facePatch = &block->facePatches[faceIndex];
			allocation = facePatch->faceEnabled;
			if (allocation != NULL) {
				Memory_FreeTagged("GLOWMARKFACEON", allocation);
				facePatch->faceEnabled = NULL;
			}
			allocation = facePatch->texCoords;
			if (allocation != NULL) {
				Memory_FreeTagged("GLOWMARKUVARRAY", allocation);
				facePatch->texCoords = NULL;
			}
		}
	}

	for (blockIndex = 0; blockIndex < BLAST_MARK_PATCH_POOL_COUNT; ++blockIndex) {
		block = &g_blastMarkPatchPool[blockIndex];
		for (faceIndex = 0; faceIndex < 16; ++faceIndex) {
			facePatch = &block->facePatches[faceIndex];
			allocation = facePatch->faceEnabled;
			if (allocation != NULL) {
				Memory_FreeTagged("GLOWMARKFACEON", allocation);
				facePatch->faceEnabled = NULL;
			}
			allocation = facePatch->texCoords;
			if (allocation != NULL) {
				Memory_FreeTagged("GLOWMARKUVARRAY", allocation);
				facePatch->texCoords = NULL;
			}
		}
	}
}

// FUNCTION: XWA 0x4E77C0
ObjectMeshTextureLayerBlock* GlowMark_AllocShieldPatch(uint16_t sourceModelType, int unused) {
	ObjectMeshTextureLayerBlock* freeNext;
	ObjectMeshTextureLayerBlock* patch;
	ObjectTypeId textureModelType;
	TexLevel* texLevel;
	Std3DTextureSurface* texture;
	int faceIndex;

	(void)unused;

	patch = NULL;
	if (g_glowMarkFreeList != NULL) {
		patch = g_glowMarkFreeList;

		if (g_glowMarkFreeList->poolIndex > g_glowMarkMaxActiveIndex) {
			g_glowMarkMaxActiveIndex = g_glowMarkFreeList->poolIndex;
		}

		g_glowMarkFreeList->active = 1;
		freeNext = patch->freeNext;
		patch->persistentUntilCleared = 0;
		patch->currentFrame = 1;
		g_glowMarkFreeList = freeNext;
		patch->effectParam = -1;
		patch->baseScale = 1.0f;

		switch ((ObjectTypeId)sourceModelType) {
			case OBJ_LaserRebel:
			case OBJ_LaserRebelTurbo:
			case OBJ_WarheadLaser1:
			case OBJ_WarheadLaser3:
			case OBJ_LaserRebelTurbo_301:
			case OBJ_LaserRebelTurbo_302:
				textureModelType = OBJ_AnimationTextureGroup3200;
				break;

			case OBJ_LaserImperial:
			case OBJ_LaserImperialTurbo:
			case OBJ_WarheadLaser2:
			case OBJ_LaserImperialTurbo_303:
			case OBJ_LaserImperialTurbo_304:
			case OBJ_LaserImperialTurbo_305:
			case OBJ_LaserImperialDS:
				textureModelType = OBJ_AnimationTextureGroup3300;
				break;

			case OBJ_LaserIon:
			case OBJ_LaserIonTurbo:
			case OBJ_WarheadIon:
			case OBJ_WarheadIonPulse:
				textureModelType = OBJ_AnimationTextureGroup3100;
				break;

			case OBJ_WarheadMagPulse:
			case OBJ_WarheadFlare:
				textureModelType = OBJ_AnimationTextureGroup3400;
				break;

			case OBJ_WarheadMissile:
			case OBJ_WarheadAdvancedMissile:
			case OBJ_WarheadRocket:
				textureModelType = OBJ_AnimationTextureGroup3500;
				break;

			default:
				textureModelType = OBJ_AnimationTextureGroup3500;
				break;
		}

		FeDiskIo_SelectTextureFrame((uint16_t)textureModelType, 1u, 256);
		texLevel = g_modelTypeTable[(uint16_t)textureModelType].curTexLevel;
		texture = NULL;
		if (texLevel != NULL && texLevel->image != NULL) {
			texture = (Std3DTextureSurface*)texLevel->image;
		}

		switch ((ObjectTypeId)sourceModelType) {
			case OBJ_LaserRebelTurbo:
			case OBJ_LaserImperialTurbo:
			case OBJ_LaserIonTurbo: {
				float baseScale = patch->baseScale;

				baseScale *= g_shieldGlowTurboScale;
				patch->baseScale = baseScale;
				break;
			}

			case OBJ_WarheadTorpedo:
			case OBJ_WarheadMissile:
			case OBJ_WarheadLaser1:
			case OBJ_WarheadLaser2:
			case OBJ_WarheadIon:
			case OBJ_WarheadIonPulse:
			case OBJ_WarheadLaser3:
			case OBJ_LaserRebelTurbo_301:
			case OBJ_LaserRebelTurbo_302:
			case OBJ_LaserImperialTurbo_303:
			case OBJ_LaserImperialTurbo_304:
			case OBJ_LaserImperialTurbo_305:
			case OBJ_LaserImperialDS: {
				float baseScale = patch->baseScale;

				baseScale *= g_shieldGlowProjectileScale;
				patch->baseScale = baseScale;
				break;
			}

			case OBJ_WarheadAdvancedTorpedo:
			case OBJ_WarheadAdvancedMissile:
			case OBJ_WarheadSpaceBomb:
			case OBJ_WarheadRocket: {
				float baseScale = patch->baseScale;

				baseScale *= g_shieldGlowHeavyWarheadScale;
				patch->baseScale = baseScale;
				break;
			}

			default:
				break;
		}

		for (faceIndex = 0; faceIndex < 16; ++faceIndex) {
			MeshExtraTextureLayer* facePatch;

			facePatch = &patch->facePatches[faceIndex];
			if (facePatch->faceEnabled != NULL) {
				Memory_FreeTagged("GLOWMARKFACEON", facePatch->faceEnabled);
				facePatch->faceEnabled = NULL;
			}
			if (facePatch->texCoords != NULL) {
				Memory_FreeTagged("GLOWMARKUVARRAY", facePatch->texCoords);
				facePatch->texCoords = NULL;
			}

			facePatch->passCount = 1;
			facePatch->passes[0].texture = texture;
			facePatch->passes[0].uvScale = (float)(unsigned int)g_shieldGlowFrameScales[0] * patch->baseScale;
		}

		patch->modelType = (uint16_t)textureModelType;
#ifdef XWA_MODERN
		XwaSnapshot_NoteGlowMarkAllocation(patch, 1);
#endif
	}
	return patch;
}

// FUNCTION: XWA 0x4E7DF0
ObjectMeshTextureLayerBlock* GlowMark_AllocAnimatedPatch(uint16_t effectParamIndex, uint16_t modelType) {
	ObjectMeshTextureLayerBlock* patch;
	int faceIndex;

	patch = NULL;
	if (modelType == OBJ_BlastMarkTextureGroup3050) {
		Std3DTextureSurface* texture;
		unsigned int damage;
		uint16_t frame;
		float uvScale;
		TexLevel* texLevel;

		if (g_blastMarkFreeList == NULL) {
			GlowMark_EvictOldestBlastMarkPatch();
		}

		if (g_blastMarkFreeList != NULL) {
			patch = g_blastMarkFreeList;
			if (g_blastMarkFreeList->poolIndex > g_blastMarkMaxActiveIndex) {
				g_blastMarkMaxActiveIndex = g_blastMarkFreeList->poolIndex;
			}
			g_blastMarkFreeList->active = 1;
			patch->persistentUntilCleared = 0;
			g_blastMarkFreeList = patch->freeNext;
			patch->effectParam = -1;
			patch->baseScale = 1.0f;

			damage = (unsigned int)g_projectileDamageByType[effectParamIndex - OBJ_LaserRebel];
			if (damage < 1000u) {
				frame = (uint16_t)(GameRand2() & 3u);
				if (frame == 3u) {
					frame = (uint16_t)((GameRand2() & 3u) + 3u);
				}
				if (damage < 499u) {
					uvScale = 4.0f;
				} else {
					uvScale = 2.0f;
				}
			} else {
				if (damage < 9000u) {
					frame = (uint16_t)((GameRand2() & 3u) + 3u);
					if (damage < 2000u) {
						uvScale = 1.75f;
					} else {
						uvScale = 1.5f;
					}
				} else {
					frame = (uint16_t)((GameRand2() & 3u) + 7u);
					uvScale = 1.25f;
					if (damage >= 15001u) {
						uvScale = 1.0f;
					}
				}
			}

			patch->currentFrame = (int)frame + 1;
			FeDiskIo_SelectTextureFrame(OBJ_BlastMarkTextureGroup3050, (uint16_t)patch->currentFrame, 256);
			texture = NULL;
			texLevel = g_modelTypeTable[OBJ_BlastMarkTextureGroup3050].curTexLevel;
			if (texLevel != NULL && texLevel->image != NULL) {
				texture = (Std3DTextureSurface*)texLevel->image;
			}

			for (faceIndex = 0; faceIndex < 16; ++faceIndex) {
				MeshExtraTextureLayer* facePatch;

				facePatch = &patch->facePatches[faceIndex];
				if (facePatch->faceEnabled != NULL) {
					Memory_FreeTagged("GLOWMARKFACEON", facePatch->faceEnabled);
					facePatch->faceEnabled = NULL;
				}
				if (facePatch->texCoords != NULL) {
					Memory_FreeTagged("GLOWMARKUVARRAY", facePatch->texCoords);
					facePatch->texCoords = NULL;
				}
				facePatch->passCount = 1;
				facePatch->passes[0].texture = texture;
				facePatch->passes[0].uvScale = uvScale * patch->baseScale;
				facePatch->passes[0].color = (uint32_t)patch->effectParam;
			}

			patch->modelType = OBJ_BlastMarkTextureGroup3050;
#ifdef XWA_MODERN
			XwaSnapshot_NoteGlowMarkAllocation(patch, (uint16_t)patch->currentFrame);
#endif
			patch->currentFrame = 0;
			return patch;
		}
		return patch;
	}

	{
		Std3DTextureSurface* texture;
		uint8_t* frameScales;
		TexLevel* texLevel;

		texture = NULL;
		switch (modelType) {
			case OBJ_AnimationTextureGroup3005:
				frameScales = g_hullLightFrameScales;
				break;

			case OBJ_AnimationTextureGroup2007:
				frameScales = g_magElectFrameScales;
				break;

			case OBJ_AnimationTextureGroup3055:
				frameScales = g_tractorBeamFrameScales;
				break;

			default:
				frameScales = g_ionElectFrameScales;
				break;
		}

		if (g_glowMarkFreeList != NULL) {
			patch = g_glowMarkFreeList;
			if (g_glowMarkFreeList->poolIndex > g_glowMarkMaxActiveIndex) {
				g_glowMarkMaxActiveIndex = g_glowMarkFreeList->poolIndex;
			}
			g_glowMarkFreeList->active = 1;
			patch->currentFrame = 1;
			g_glowMarkFreeList = patch->freeNext;
			patch->effectParam = -1;
			patch->baseScale = 1.0f;

			FeDiskIo_SelectTextureFrame(modelType, 1u, 256);
			texLevel = g_modelTypeTable[modelType].curTexLevel;
			if (texLevel != NULL && texLevel->image != NULL) {
				texture = (Std3DTextureSurface*)texLevel->image;
			}

			for (faceIndex = 0; faceIndex < 16; ++faceIndex) {
				MeshExtraTextureLayer* facePatch;

				facePatch = &patch->facePatches[faceIndex];
				if (facePatch->faceEnabled != NULL) {
					Memory_FreeTagged("GLOWMARKFACEON", facePatch->faceEnabled);
					facePatch->faceEnabled = NULL;
				}
				if (facePatch->texCoords != NULL) {
					Memory_FreeTagged("GLOWMARKUVARRAY", facePatch->texCoords);
					facePatch->texCoords = NULL;
				}
				facePatch->passCount = 1;
				facePatch->passes[0].texture = texture;
				facePatch->passes[0].uvScale = (float)(unsigned int)frameScales[0] * patch->baseScale;
			}
			patch->modelType = modelType;
#ifdef XWA_MODERN
			XwaSnapshot_NoteGlowMarkAllocation(patch, 1);
#endif
		}
	}

	return patch;
}

// FUNCTION: XWA 0x4E7A10
void GlowMark_UpdateActivePatches(void) {
	int lastGlowActive;
	int patchIndex;
	int oldBlastMax;
	int lastBlastActive;
	ObjectMeshTextureLayerBlock* patch;

	lastGlowActive = -1;
	patchIndex = 0;
	patch = g_glowMarkPatchPool;
	while (patchIndex <= g_glowMarkMaxActiveIndex) {
		int modelType;
		unsigned int currentFrame;
		int* currentFramePtr;
		uint8_t* frameScales;
		uint16_t* activePtr;
		uint16_t* modelTypePtr;

		activePtr = &patch->active;
		modelTypePtr = &patch->modelType;
		currentFramePtr = &patch->currentFrame;
		if (*activePtr != 0) {
			currentFrame = (unsigned int)*currentFramePtr + 1u;
			*currentFramePtr = (int)currentFrame;
			modelType = *modelTypePtr;
			if (modelType <= OBJ_AnimationTextureGroup3055) {
				switch (modelType) {
					case OBJ_AnimationTextureGroup3055:
						frameScales = g_tractorBeamFrameScales;
						break;
					case OBJ_AnimationTextureGroup3005:
						frameScales = g_hullLightFrameScales;
						break;
					case OBJ_AnimationTextureGroup2007:
						frameScales = g_magElectFrameScales;
						break;
					case OBJ_AnimationTextureGroup2008:
						frameScales = g_ionElectFrameScales;
						break;
					default:
						frameScales = g_shieldGlowFrameScales;
						break;
				}
			} else {
				frameScales = g_shieldGlowFrameScales;
			}
			if (currentFrame <= (unsigned int)g_modelTypeTable[modelType].frameCount) {
				lastGlowActive = patchIndex;
				FeDiskIo_SelectTextureFrame(*modelTypePtr, (uint16_t)*currentFramePtr, 256);
				{
					TexLevel* texLevel;
					int faceIndex;

					texLevel = g_modelTypeTable[*modelTypePtr].curTexLevel;
					if (texLevel != NULL && texLevel->image != NULL) {
						for (faceIndex = 0; faceIndex < patch->facePatchCount; ++faceIndex) {
							MeshExtraTextureLayer* facePatch;
							int passIndex;

							facePatch = &patch->facePatches[faceIndex];
							for (passIndex = 0; passIndex < facePatch->passCount; ++passIndex) {
								facePatch->passes[passIndex].texture = (Std3DTextureSurface*)texLevel->image;
								facePatch->passes[passIndex].uvScale =
									(float)(unsigned int)frameScales[*currentFramePtr - 1] * patch->baseScale;
								facePatch->passes[passIndex].color = (uint32_t)patch->effectParam;
							}
						}
					} else {
						for (faceIndex = 0; faceIndex < patch->facePatchCount; ++faceIndex) {
							MeshExtraTextureLayer* facePatch;
							int passIndex;

							facePatch = &patch->facePatches[faceIndex];
							for (passIndex = 0; passIndex < facePatch->passCount; ++passIndex) {
								facePatch->passes[passIndex].texture = NULL;
							}
						}
						*activePtr = 0;
						*currentFramePtr = g_modelTypeTable[*modelTypePtr].frameCount;
					}
				}
			} else {
				ObjectMeshTextureLayerBlock** glowMarkTail;

				*activePtr = 0;
				*currentFramePtr = 0;
				patch->freeNext = g_glowMarkFreeList;
				g_glowMarkFreeList = patch;

				glowMarkTail = &g_objRenderState[patch->objectIndex].glowMarkTail;
				if (*glowMarkTail == patch) {
					*glowMarkTail = patch->prevActive;
				}
				if (patch->prevActive != NULL) {
					patch->prevActive->nextActive = patch->nextActive;
				}
				if (patch->nextActive != NULL) {
					patch->nextActive->prevActive = patch->prevActive;
				}
				patch->prevActive = NULL;
				patch->nextActive = NULL;
			}
		}
		++patchIndex;
		++patch;
	}
	if (lastGlowActive < g_glowMarkMaxActiveIndex) {
		g_glowMarkMaxActiveIndex = lastGlowActive;
	}

	oldBlastMax = g_blastMarkMaxActiveIndex;
	lastBlastActive = -1;
	patchIndex = 0;
	patch = g_blastMarkPatchPool;
	while (patchIndex <= oldBlastMax) {
		int* currentFrame;
		uint16_t* activePtr;

		currentFrame = &patch->currentFrame;
		activePtr = &patch->active;
		if (*activePtr != 0) {
			if (g_objectTable[patch->objectIndex].objectType != 0 &&
				(patch->persistentUntilCleared != 0 || (unsigned int)*currentFrame < 10024u)) {
				lastBlastActive = patchIndex;
				++*currentFrame;
			} else {
				ObjectMeshTextureLayerBlock** glowMarkTail;

				*activePtr = 0;
				*currentFrame = 0;
				patch->freeNext = g_blastMarkFreeList;
				g_blastMarkFreeList = patch;

				glowMarkTail = &g_objRenderState[patch->objectIndex].glowMarkTail;
				if (*glowMarkTail == patch) {
					*glowMarkTail = patch->prevActive;
				}
				if (patch->prevActive != NULL) {
					patch->prevActive->nextActive = patch->nextActive;
				}
				if (patch->nextActive != NULL) {
					patch->nextActive->prevActive = patch->prevActive;
				}
				patch->prevActive = NULL;
				patch->nextActive = NULL;
				oldBlastMax = g_blastMarkMaxActiveIndex;
			}
		}
		++patchIndex;
		++patch;
	}
	if (lastBlastActive < oldBlastMax) {
		g_blastMarkMaxActiveIndex = lastBlastActive;
	}
}

// FUNCTION: XWA 0x4E8180
void GlowMark_BeginMeshFacePatch(ObjectMeshTextureLayerBlock* patch) {
	int vertexIndex;
	int coordinateIndex;
	size_t texCoordIndex;

	if (patch->facePatchCount >= 16) {
		return;
	}

	patch->facePatches[patch->facePatchCount].texCoords = (OptTexCoord*)Memory_AllocTagged(
		"GLOWMARKUVARRAY", sizeof(OptTexCoord) * (size_t)g_glowMarkMeshVertexCount);
	patch->facePatches[patch->facePatchCount].faceEnabled =
		(uint16_t*)Memory_AllocTagged("GLOWMARKFACEON", sizeof(uint32_t) * (size_t)g_glowMarkMeshVertexCount);
	patch->meshVertexArrays[patch->facePatchCount] = (Vec3f*)g_glowMarkMeshVertexArray;

	if (g_glowMarkMeshVertexCount > 511) {
		g_OutputDebugStringA("TooManyVerts");
	}

	coordinateIndex = 0;
	texCoordIndex = 0;
	for (vertexIndex = 0; vertexIndex < g_glowMarkMeshVertexCount; ++vertexIndex) {
		float planeDistance;
		float projectedX;
		float projectedY;
		float projectedZ;
		float u;
		float v;

		planeDistance = g_glowMarkMeshVertexArray[coordinateIndex + 1] * patch->normal.y +
						g_glowMarkMeshVertexArray[coordinateIndex] * patch->normal.x;
		planeDistance += g_glowMarkMeshVertexArray[coordinateIndex + 2] * patch->normal.z;
		planeDistance += patch->planeD;

		projectedX =
			g_glowMarkMeshVertexArray[coordinateIndex] - planeDistance * patch->normal.x - patch->center.x;
		projectedY = g_glowMarkMeshVertexArray[coordinateIndex + 1] - planeDistance * patch->normal.y -
					 patch->center.y;
		projectedZ = g_glowMarkMeshVertexArray[coordinateIndex + 2] - planeDistance * patch->normal.z -
					 patch->center.z;

		g_glowMarkVertexProj[vertexIndex].planeDistance = planeDistance;
		if (planeDistance < g_glowMarkPlaneZero) {
			g_glowMarkVertexProj[vertexIndex].planeDistance = -planeDistance;
			g_glowMarkVertexProj[vertexIndex].planeSide = -1.0f;
		} else {
			g_glowMarkVertexProj[vertexIndex].planeSide = 1.0f;
		}

		u = projectedZ * patch->uAxis.z + projectedX * patch->uAxis.x;
		u = (u + projectedY * patch->uAxis.y) * g_glowMarkInvScaleU;
		v = (projectedX * patch->vAxis.x + projectedY * patch->vAxis.y + projectedZ * patch->vAxis.z) *
			g_glowMarkInvScaleV;

		g_glowMarkVertexProj[vertexIndex].clipMask = 0;
		if (u < g_glowMarkUvMin) {
			g_glowMarkVertexProj[vertexIndex].clipMask = 1;
		} else if (u > g_glowMarkUvMax) {
			g_glowMarkVertexProj[vertexIndex].clipMask = 2;
		}
		if (v < g_glowMarkUvMin) {
			g_glowMarkVertexProj[vertexIndex].clipMask |= 4;
		} else if (v > g_glowMarkUvMax) {
			g_glowMarkVertexProj[vertexIndex].clipMask |= 8;
		}

		coordinateIndex += 3;
		patch->facePatches[patch->facePatchCount].texCoords[texCoordIndex].u = u;
		++texCoordIndex;
		patch->facePatches[patch->facePatchCount].texCoords[texCoordIndex - 1].v = v;
	}

	++patch->facePatchCount;
}

// FUNCTION: XWA 0x4E83D0
void GlowMark_MarkFaceData(ObjectMeshTextureLayerBlock* patch, OptNode* faceDataNode) {
	int faceIndex;
	Vec3f* faceNormals;
	FaceRecord* faceRecords;
	int patchIndex;
	ObjectMeshTextureLayerBlock* currentPatch;

	currentPatch = patch;
	if (currentPatch->facePatchCount > 16) {
		return;
	}

	patchIndex = (int)currentPatch->facePatchCount - 1;
	faceRecords = (FaceRecord*)((uint8_t*)faceDataNode->param2 + sizeof(int));
	faceNormals = (Vec3f*)&faceRecords[faceDataNode->param1];

	faceIndex = 0;
	while (faceIndex < (int)faceDataNode->param1) {
		float normalDot;

		normalDot = faceNormals->x * currentPatch->normal.x + faceNormals->y * currentPatch->normal.y +
					faceNormals->z * currentPatch->normal.z;

		if (g_glowMarkWorldSegmentMode != 0) {
			currentPatch->facePatches[patchIndex].faceEnabled[g_faceIdCounter] = 1;
		} else {
			unsigned int vertexCount;
			int sharedClipMask;

			vertexCount = (faceRecords->vertexIdx[3] != -1) + 3;
			sharedClipMask = g_glowMarkVertexProj[faceRecords->vertexIdx[0]].clipMask &
							 g_glowMarkVertexProj[faceRecords->vertexIdx[1]].clipMask &
							 g_glowMarkVertexProj[faceRecords->vertexIdx[2]].clipMask;
			if (vertexCount == 4) {
				sharedClipMask &= g_glowMarkVertexProj[faceRecords->vertexIdx[3]].clipMask;
			}

			if (sharedClipMask != 0 || normalDot < g_glowMarkPlaneZero ||
				(g_glowMarkVertexProj[faceRecords->vertexIdx[0]].planeDistance >
					 g_glowMarkPlaneDistanceLimit &&
				 g_glowMarkVertexProj[faceRecords->vertexIdx[1]].planeDistance >
					 g_glowMarkPlaneDistanceLimit &&
				 g_glowMarkVertexProj[faceRecords->vertexIdx[2]].planeDistance >
					 g_glowMarkPlaneDistanceLimit &&
				 (vertexCount == 3 || g_glowMarkVertexProj[faceRecords->vertexIdx[3]].planeDistance >
										  g_glowMarkPlaneDistanceLimit))) {
				currentPatch->facePatches[patchIndex].faceEnabled[g_faceIdCounter] = 0;
			} else {
				float uNormalOffset;
				float vNormalOffset;

				currentPatch->facePatches[patchIndex].faceEnabled[g_faceIdCounter] = 1;
				uNormalOffset =
					(currentPatch->uAxis.z * faceNormals->z + currentPatch->uAxis.y * faceNormals->y +
					 currentPatch->uAxis.x * faceNormals->x) *
					g_glowMarkInvScaleU;
				vNormalOffset =
					(currentPatch->vAxis.y * faceNormals->y + currentPatch->vAxis.z * faceNormals->z +
					 currentPatch->vAxis.x * faceNormals->x) *
					g_glowMarkInvScaleV;

				if (vertexCount > 0) {
					const int* vertexIndices;
					unsigned int remainingVertices;

					vertexIndices = faceRecords->vertexIdx;
					remainingVertices = vertexCount;
					do {
						int vertexIndex;

						vertexIndex = *vertexIndices;
						++vertexIndices;
						currentPatch->facePatches[patchIndex].texCoords[vertexIndex].u -=
							g_glowMarkVertexProj[vertexIndex].planeDistance *
							g_glowMarkVertexProj[vertexIndex].planeSide * uNormalOffset;
						currentPatch->facePatches[patchIndex].texCoords[vertexIndex].v -=
							g_glowMarkVertexProj[vertexIndex].planeDistance *
							g_glowMarkVertexProj[vertexIndex].planeSide * vNormalOffset;
						--remainingVertices;
					} while (remainingVertices != 0);
				}
			}
		}

		++g_faceIdCounter;
		++faceNormals;
		++faceRecords;
		++faceIndex;
	}
}

// FUNCTION: XWA 0x4E92D0
void GlowMark_CollectModelFaces(ObjectMeshTextureLayerBlock* patch, OptimizedPolyObject* model,
								OptNode* node) {
	int childIndex;
	OptNode* curNode;

	curNode = OptModel_ResolveNodeRef(node, model);

	while (curNode != NULL) {
		int followFirstChild;

		followFirstChild = 0;
		switch (curNode->nodeType) {
			case OPT_FACEGROUP:
				followFirstChild = 1;
				break;

			case OPT_MESHVERTS:
				if (patch->facePatchCount != 16) {
					g_glowMarkMeshVertexCount = (int)curNode->param1;
					g_glowMarkMeshVertexArray = (float*)curNode->param2;
					g_faceIdCounter = 0;
					GlowMark_BeginMeshFacePatch(patch);
				} else {
					g_glowMarkTraversalActive = 0;
				}
				break;

			case OPT_FACEDATA:
			case OPT_FACEDATA_15:
			case OPT_FACEDATA_16:
			case OPT_FACEDATA_17:
				if (g_glowMarkTraversalActive != 0) {
					GlowMark_MarkFaceData(patch, curNode);
				}
				break;

			case OPT_ROTSCALE:
				followFirstChild = followFirstChild;
				break;
		}

		if (curNode->childCount == 0) {
			return;
		}
		if (g_glowMarkTraversalActive == 0) {
			return;
		}

		if (followFirstChild == 0) {
			for (childIndex = 0; childIndex < curNode->childCount; ++childIndex) {
				GlowMark_CollectModelFaces(patch, model, curNode->pChildren[childIndex]);
			}
			return;
		}

		if (followFirstChild == -1) {
			break;
		}
		curNode = OptModel_ResolveNodeRef(curNode->pChildren[followFirstChild - 1], model);
	}
}

// FUNCTION: XWA 0x4E9F50
void GlowMark_CreateEngineKnockoutBlastMark(unsigned int objectIndex, uint8_t emitterIndex) {
	uint16_t modelIndex;
	EngineGlowKnockoutMark* existingMark;
	EngineGlowKnockoutMark* oldHead;
	EngineGlowKnockoutMark* mark;
	OptEngineGlow* glow;
	GlowMarkRequest* request;

	modelIndex = g_modelTypeTable[(uint16_t)g_objectTable[objectIndex].objectType].modelIndex;
	if ((uint8_t)emitterIndex >= g_modelDefs[modelIndex].engineGlowCount) {
		return;
	}

	existingMark = g_objRenderState[objectIndex].engineGlowKnockouts;
	while (existingMark != NULL && existingMark->emitterIndex != (uint8_t)emitterIndex) {
		existingMark = existingMark->next;
	}
	if (existingMark != NULL) {
		return;
	}

	oldHead = g_objRenderState[objectIndex].engineGlowKnockouts;
	mark = EngineGlow_AllocKnockoutMark();
	g_objRenderState[objectIndex].engineGlowKnockouts = mark;
	mark->emitterIndex = (uint8_t)emitterIndex;
	mark->objectIndex = objectIndex;
	mark->modelIndex = modelIndex;
	mark->next = oldHead;

	glow = g_modelDefs[modelIndex].engineGlows[(uint8_t)emitterIndex];
	if (glow != NULL) {
		float scaleU;
		float scaleV;

		scaleV = glow->dimensions.y * 0.5f;
		scaleU = glow->dimensions.x * 0.5f;
		if (!g_hitEffectsEnabled) {
			request = NULL;
		} else {
			request = NULL;
			if (g_glowMarkRequestCount < 64) {
				request = &g_glowMarkRequestPool[g_glowMarkRequestCount];
				request->next = NULL;
				++g_glowMarkRequestCount;
			}

			if (request != NULL) {
				GlowMarkRequest* oldPending;

				request->worldSegmentMode = 0;
				request->deriveUvAxesFromReference = 0;
				memcpy(&request->geom.plane, &glow->position, sizeof(request->geom.plane));
				request->scaleU = scaleU;
				request->scaleV = scaleV;
				request->persistentUntilCleared = 0;
				request->effectParam = OBJ_WarheadSpaceBomb;
				request->objectIndex = objectIndex;
				request->modelType = OBJ_BlastMarkTextureGroup3050;
				oldPending = g_objRenderState[objectIndex].pendingGlowMarks;
				g_objRenderState[objectIndex].pendingGlowMarks = request;
				g_objRenderState[objectIndex].pendingGlowMarks->next = oldPending;
			}
		}

		if (request != NULL) {
			request->persistentUntilCleared = 1;
			GlowMark_ProcessPendingRequests(objectIndex);
			mark->blastMark = g_objRenderState[objectIndex].glowMarkTail;
		}
	}

	if (mark->blastMark == NULL) {
		g_objRenderState[objectIndex].engineGlowKnockouts = oldHead;
		EngineGlow_FreeKnockoutMarkList(mark);
		OutputDebugStringA("Didn't create a knockout for that model...\n");
	}
}

// FUNCTION: XWA 0x4E9900
char GlowMark_SpawnLocalPlayerHitEffects(void) {
	uint16_t objectIndex;
	int16_t objectType;
	uint16_t* loadedModelSlot;
	uint16_t savedModelHandle;
	uint8_t sparkIndex;
	float localSegmentEnd[3];
	int spawnCount;
	int i;
	char result;

	objectIndex = (uint16_t)g_players[g_localPlayer].objectIndex;
	result = (char)g_hitEffectsEnabled;
	if (!g_hitEffectsEnabled) {
		return result;
	}

	g_glowMarkSavedCollisionSegmentStartX = g_collisionSegmentStartWorldX;
	g_glowMarkSavedCollisionSegmentStartY = g_collisionSegmentStartWorldY;
	g_glowMarkSavedCollisionSegmentStartZ = g_collisionSegmentStartWorldZ;
	g_glowMarkSavedCollisionProbeWorldX = g_collisionProbeWorldX;
	g_glowMarkSavedCollisionProbeWorldY = g_collisionProbeWorldY;
	g_glowMarkSavedCollisionProbeWorldZ = g_collisionProbeWorldZ;
	g_glowMarkSavedCollisionSweepStartX = g_collisionSweepStartX;
	g_glowMarkSavedCollisionSweepStartY = g_collisionSweepStartY;
	g_glowMarkSavedCollisionSweepStartZ = g_collisionSweepStartZ;
	g_glowMarkSavedCollisionSweepEndX = g_collisionSweepEndX;
	g_glowMarkSavedCollisionSweepEndY = g_collisionSweepEndY;
	g_glowMarkSavedCollisionSweepEndZ = g_collisionSweepEndZ;
	g_glowMarkSavedCollisionHitOffsetX = g_collisionHitOffsetX;
	g_glowMarkSavedCollisionHitOffsetY = g_collisionHitOffsetY;
	g_glowMarkSavedCollisionHitOffsetZ = g_collisionHitOffsetZ;

	spawnCount = (int)((uint16_t)GameRand2() % 5) + 5;
	objectType = g_objectTable[objectIndex].objectType;
	loadedModelSlot = &g_loadedModels.byObjectType[(uint16_t)objectType];
	savedModelHandle = *loadedModelSlot;
	if (g_players[g_localPlayer].currentSeatIdx != 0) {
		ModelIndex modelIndex;
		uint16_t turretModelType;

		modelIndex = GetModelIndexFromType(objectType);
		turretModelType =
			g_modelDefs[modelIndex].turretModelIndex[g_players[g_localPlayer].currentSeatIdx - 1];
		if (turretModelType != 0) {
			*loadedModelSlot = g_loadedModels.byObjectType[turretModelType];
		} else {
			g_OutputDebugStringA("Can't find cockpit model\n");
		}
	} else {
		*loadedModelSlot = g_cockpitModel;
	}

	g_cockpitViewActive = 1;
	localSegmentEnd[0] = g_players[g_localPlayer].hardpointLocalX;
	localSegmentEnd[1] = g_players[g_localPlayer].hardpointLocalY;
	localSegmentEnd[2] = g_players[g_localPlayer].hardpointLocalZ;

	for (i = 0; i < spawnCount; ++i) {
		float localSegmentStart[3];

		sparkIndex = (uint8_t)((uint16_t)GameRand2() % (int)g_cockpitSparkHardpointCount);
		localSegmentStart[0] = g_cockpitSparkHardpoints[sparkIndex].localX;
		localSegmentStart[2] = g_cockpitSparkHardpoints[sparkIndex].localZ;
		localSegmentStart[1] = -g_cockpitSparkHardpoints[sparkIndex].localY;

		if (!g_cockpitUsesTieHitEffectPlanes) {
			if (collide_CheckLocalSweepAgainstObjectModel(objectIndex, objectIndex, localSegmentEnd,
														  localSegmentStart, -1, 0)) {
				float scale;

				scale = (float)(uint16_t)GameRand2() * g_localPlayerHitRandomScale;
				scale *= g_localPlayerHitScaleRange;
				scale = g_localPlayerHitScaleBase - scale;
				GlowMark_QueueRequest(objectIndex, 0, OBJ_AnimationTextureGroup3005, scale, scale);
			} else {
				GlowMarkRequest* request;
				double scale;
				GlowMarkPlaneGeometry geom = {
					{ localSegmentStart[0], localSegmentStart[1], localSegmentStart[2] },
					{ 0.0f, 0.0f, 0.0f },
					{ -1.0f, 0.0f, 0.0f },
					{ 1.0f, 0.0f, 0.0f },
				};

				scale = (float)(uint16_t)GameRand2() * g_localPlayerHitRandomScale;
				scale *= g_localPlayerHitScaleRange;
				scale = g_localPlayerHitScaleBase - scale;
				request = NULL;
				if (g_hitEffectsEnabled && g_glowMarkRequestCount < 64) {
					request = &g_glowMarkRequestPool[g_glowMarkRequestCount];
					request->next = NULL;
					++g_glowMarkRequestCount;
				}

				if (request != NULL) {
					GlowMarkRequest* oldHead;

					request->scaleU = scale;
					request->scaleV = scale;
					request->worldSegmentMode = 0;
					memcpy(&request->geom.plane, &geom, sizeof(request->geom.plane));
					request->deriveUvAxesFromReference = 0;
					request->objectIndex = objectIndex;
					request->persistentUntilCleared = 0;
					request->effectParam = 0;
					request->modelType = OBJ_AnimationTextureGroup3005;

					oldHead = g_objRenderState[objectIndex].pendingGlowMarks;
					g_objRenderState[objectIndex].pendingGlowMarks = request;
					g_objRenderState[objectIndex].pendingGlowMarks->next = oldHead;
				}
			}
		} else {
			GlowMarkPlaneGeometry geom;

			geom.center.x = localSegmentStart[0];
			geom.center.y = localSegmentStart[1];
			geom.center.z = localSegmentStart[2];
			geom.normal.x = 0.0f;
			geom.normal.y = 1.0f;
			geom.normal.z = 0.0f;
			geom.uAxis.x = 0.0f;
			geom.uAxis.z = 1.0f;
			geom.vAxis.x = 1.0f;
			geom.vAxis.y = 0.0f;
			geom.vAxis.z = 0.0f;
			if (g_hitEffectsEnabled) {
				GlowMarkRequest* request;

				request = NULL;
				if (g_glowMarkRequestCount < 64) {
					request = &g_glowMarkRequestPool[g_glowMarkRequestCount];
					request->next = NULL;
					++g_glowMarkRequestCount;
				}

				if (request != NULL) {
					GlowMarkRequest* oldHead;

					request->worldSegmentMode = 0;
					memcpy(&request->geom.plane, &geom, sizeof(request->geom.plane));
					request->deriveUvAxesFromReference = 0;
					request->scaleU = 50.0f;
					request->scaleV = 50.0f;
					request->objectIndex = objectIndex;
					request->persistentUntilCleared = 0;
					request->effectParam = 0;
					request->modelType = OBJ_AnimationTextureGroup3005;

					oldHead = g_objRenderState[objectIndex].pendingGlowMarks;
					g_objRenderState[objectIndex].pendingGlowMarks = request;
					g_objRenderState[objectIndex].pendingGlowMarks->next = oldHead;
				}
			}
		}
	}

	{
		sparkIndex = (uint8_t)((uint16_t)GameRand2() % (int)g_cockpitSparkHardpointCount);
		g_glowMarkPlaneScratch.center.y = -g_cockpitSparkHardpoints[sparkIndex].localY;
		g_glowMarkPlaneScratch.center.x = g_cockpitSparkHardpoints[sparkIndex].localX;
		g_glowMarkPlaneScratch.center.z = g_cockpitSparkHardpoints[sparkIndex].localZ;
		g_glowMarkScratchNormalVec->y = 0.0f;
		g_glowMarkScratchNormalVec->x = 0.0f;
		g_glowMarkScratchNormalVec->z = 1.0f;
		if ((GameRand2() & 3) != 0) {
			Particle_AttachEffectToObject(2, objectIndex, &g_glowMarkPlaneScratch.center,
										  g_glowMarkScratchNormalVec);
		} else {
			Particle_AttachEffectToObject(4, objectIndex, &g_glowMarkPlaneScratch.center,
										  g_glowMarkScratchNormalVec);
		}
	}

	if (g_objRenderState[objectIndex].pendingGlowMarks != NULL) {
		GlowMark_ProcessPendingRequests(objectIndex);
	}

	{
		int savedCollisionSegmentStartX = g_glowMarkSavedCollisionSegmentStartX;

		*loadedModelSlot = savedModelHandle;
		g_collisionSegmentStartWorldX = savedCollisionSegmentStartX;
	}
	g_collisionSegmentStartWorldY = g_glowMarkSavedCollisionSegmentStartY;
	g_collisionSegmentStartWorldZ = g_glowMarkSavedCollisionSegmentStartZ;
	g_collisionProbeWorldX = g_glowMarkSavedCollisionProbeWorldX;
	g_collisionProbeWorldY = g_glowMarkSavedCollisionProbeWorldY;
	g_collisionProbeWorldZ = g_glowMarkSavedCollisionProbeWorldZ;
	g_collisionSweepStartX = g_glowMarkSavedCollisionSweepStartX;
	g_collisionSweepStartY = g_glowMarkSavedCollisionSweepStartY;
	g_collisionSweepStartZ = g_glowMarkSavedCollisionSweepStartZ;
	g_collisionSweepEndX = g_glowMarkSavedCollisionSweepEndX;
	g_collisionSweepEndY = g_glowMarkSavedCollisionSweepEndY;
	g_collisionSweepEndZ = g_glowMarkSavedCollisionSweepEndZ;
	g_cockpitViewActive = 0;
	g_collisionHitOffsetX = g_glowMarkSavedCollisionHitOffsetX;
	g_collisionHitOffsetY = g_glowMarkSavedCollisionHitOffsetY;
	g_collisionHitOffsetZ = g_glowMarkSavedCollisionHitOffsetZ;
	return (char)g_collisionHitOffsetZ;
}

// FUNCTION: XWA 0x4EA140
void GlowMark_ClearEngineKnockoutBlastMarks(unsigned int objectIndex) {
	EngineGlowKnockoutMark* mark;

	mark = g_objRenderState[objectIndex].engineGlowKnockouts;
	while (mark != NULL) {
		if (mark->blastMark != NULL) {
			mark->blastMark->persistentUntilCleared = 0;
			mark->blastMark->currentFrame = 10024;
			mark->blastMark = NULL;
		}

#ifdef XWA_MODERN
		{
			EngineGlowKnockoutMark* next = mark->next;

			EngineGlow_FreeKnockoutMark(mark);
			mark = next;
		}
#else
		mark = mark->next;
		EngineGlow_FreeKnockoutMark(mark);
#endif
	}

	g_objRenderState[objectIndex].engineGlowKnockouts = NULL;
}

// FUNCTION: XWA 0x4E7D30
void GlowMark_EvictOldestBlastMarkPatch(void) {
	int oldestIndex;
	unsigned int oldestFrame;
	int index;

	oldestIndex = 0;
	oldestFrame = 0;
	for (index = 0; index < 32; ++index) {
		if (g_blastMarkPatchPool[index].active != 0 &&
			(unsigned int)g_blastMarkPatchPool[index].currentFrame > oldestFrame) {
			oldestIndex = index;
			oldestFrame = (unsigned int)g_blastMarkPatchPool[index].currentFrame;
		}
	}

	g_blastMarkPatchPool[oldestIndex].active = 0;
	g_blastMarkPatchPool[oldestIndex].currentFrame = 0;
	g_blastMarkPatchPool[oldestIndex].freeNext = g_blastMarkFreeList;
	g_blastMarkFreeList = &g_blastMarkPatchPool[oldestIndex];

	if (g_objRenderState[g_blastMarkPatchPool[oldestIndex].objectIndex].glowMarkTail ==
		&g_blastMarkPatchPool[oldestIndex]) {
		g_objRenderState[g_blastMarkPatchPool[oldestIndex].objectIndex].glowMarkTail =
			g_blastMarkPatchPool[oldestIndex].prevActive;
	}
	if (g_blastMarkPatchPool[oldestIndex].prevActive != NULL) {
		g_blastMarkPatchPool[oldestIndex].prevActive->nextActive =
			g_blastMarkPatchPool[oldestIndex].nextActive;
	}
	if (g_blastMarkPatchPool[oldestIndex].nextActive != NULL) {
		g_blastMarkPatchPool[oldestIndex].nextActive->prevActive =
			g_blastMarkPatchPool[oldestIndex].prevActive;
	}
	g_blastMarkPatchPool[oldestIndex].prevActive = NULL;
	g_blastMarkPatchPool[oldestIndex].nextActive = NULL;
}

// FUNCTION: XWA 0x4E9580
void GlowMark_QueueRandomObjectSurfaceEffect(unsigned int objectIndex, int16_t effectModelType, float scale) {
	int objectTableIndex;
	uint16_t objectType;
	Vec3f* boundsMax;
	Vec3f* boundsMin;
	int localPosition[3];

	if (!g_hitEffectsEnabled) {
		return;
	}

	g_glowMarkSavedCollisionSegmentStartX = g_collisionSegmentStartWorldX;
	g_glowMarkSavedCollisionSegmentStartY = g_collisionSegmentStartWorldY;
	g_glowMarkSavedCollisionSegmentStartZ = g_collisionSegmentStartWorldZ;
	g_glowMarkSavedCollisionProbeWorldX = g_collisionProbeWorldX;
	g_glowMarkSavedCollisionProbeWorldY = g_collisionProbeWorldY;
	g_glowMarkSavedCollisionProbeWorldZ = g_collisionProbeWorldZ;
	g_glowMarkSavedCollisionSweepStartX = g_collisionSweepStartX;
	g_glowMarkSavedCollisionSweepStartY = g_collisionSweepStartY;
	g_glowMarkSavedCollisionSweepStartZ = g_collisionSweepStartZ;
	g_glowMarkSavedCollisionSweepEndX = g_collisionSweepEndX;
	g_glowMarkSavedCollisionSweepEndY = g_collisionSweepEndY;
	g_glowMarkSavedCollisionSweepEndZ = g_collisionSweepEndZ;
	g_glowMarkSavedCollisionHitOffsetX = g_collisionHitOffsetX;
	g_glowMarkSavedCollisionHitOffsetY = g_collisionHitOffsetY;
	g_glowMarkSavedCollisionHitOffsetZ = g_collisionHitOffsetZ;

	objectTableIndex = (uint16_t)objectIndex;
	objectType = g_objectTable[objectTableIndex].objectType;
	boundsMax = ModelBounds_GetMaxVector(objectType);
	boundsMin = ModelBounds_GetMinVector(objectType);

	localPosition[0] = (int)(Particle_RandSignedUnitFloat() * (boundsMax->x - boundsMin->x) + boundsMin->x);
	localPosition[1] = (int)(Particle_RandSignedUnitFloat() * (boundsMax->y - boundsMin->y) + boundsMin->y);
	localPosition[2] = (int)(Particle_RandSignedUnitFloat() * (boundsMax->z - boundsMin->z) + boundsMin->z);

	pai_RotateLocalVectorToWorldScratchMaybeStatic(&g_objectTable[objectTableIndex], localPosition[0],
												   localPosition[2], -localPosition[1]);
	g_collisionProbeWorldX = g_rotatedX + g_objectTable[objectTableIndex].world_x;
	g_collisionProbeWorldY = g_rotatedY + g_objectTable[objectTableIndex].world_y;
	g_collisionProbeWorldZ = g_rotatedZ + g_objectTable[objectTableIndex].world_z;

	trig2_rho = (int)g_modelTypeTable[(uint16_t)g_objectTable[objectTableIndex].objectType].maxBoundsExtent;
	trig2_theta = (uint16_t)GameRand2();
	trig2_phi = (uint16_t)GameRand2();
	trig2_ptoc3dim();

	g_collisionSegmentStartWorldX = trig2_xoffset + g_collisionProbeWorldX;
	g_collisionSegmentStartWorldY = trig2_yoffset + g_collisionProbeWorldY;
	g_collisionSegmentStartWorldZ = trig2_zoffset + g_collisionProbeWorldZ;
	g_collisionProbeWorldX -= trig2_xoffset;
	g_collisionProbeWorldY -= trig2_yoffset;
	g_collisionProbeWorldZ -= trig2_zoffset;

	if (collide_CheckSweptModelCollision(objectIndex, objectIndex) > 0 || g_provingGroundsModeActive) {
		if (g_glowMarkWorldSegmentMode) {
			g_glowMarkSegmentEndWorld[0] = g_collisionSegmentStartWorldX;
			g_glowMarkSegmentEndWorld[1] = g_collisionSegmentStartWorldY;
			g_glowMarkSegmentEndWorld[2] = g_collisionSegmentStartWorldZ;
			if (g_collisionHitOffsetX || g_collisionHitOffsetY || g_collisionHitOffsetZ) {
				g_glowMarkSegmentStartWorld[0] = g_collisionSegmentStartWorldX + g_collisionHitOffsetX;
				g_glowMarkSegmentStartWorld[1] = g_collisionSegmentStartWorldY + g_collisionHitOffsetY;
				g_glowMarkSegmentStartWorld[2] = g_collisionSegmentStartWorldZ + g_collisionHitOffsetZ;
			} else {
				g_glowMarkSegmentStartWorld[0] = g_collisionProbeWorldX;
				g_glowMarkSegmentStartWorld[1] = g_collisionProbeWorldY;
				g_glowMarkSegmentStartWorld[2] = g_collisionProbeWorldZ;
			}
		}
		GlowMark_QueueRequest(objectIndex, 0, effectModelType, scale, scale);
	}

	g_collisionSegmentStartWorldX = g_glowMarkSavedCollisionSegmentStartX;
	g_collisionSegmentStartWorldY = g_glowMarkSavedCollisionSegmentStartY;
	g_collisionSegmentStartWorldZ = g_glowMarkSavedCollisionSegmentStartZ;
	g_collisionProbeWorldX = g_glowMarkSavedCollisionProbeWorldX;
	g_collisionProbeWorldY = g_glowMarkSavedCollisionProbeWorldY;
	g_collisionProbeWorldZ = g_glowMarkSavedCollisionProbeWorldZ;
	g_collisionSweepStartX = g_glowMarkSavedCollisionSweepStartX;
	g_collisionSweepStartY = g_glowMarkSavedCollisionSweepStartY;
	g_collisionSweepStartZ = g_glowMarkSavedCollisionSweepStartZ;
	g_collisionSweepEndX = g_glowMarkSavedCollisionSweepEndX;
	g_collisionSweepEndY = g_glowMarkSavedCollisionSweepEndY;
	g_collisionSweepEndZ = g_glowMarkSavedCollisionSweepEndZ;
	g_collisionHitOffsetX = g_glowMarkSavedCollisionHitOffsetX;
	g_collisionHitOffsetY = g_glowMarkSavedCollisionHitOffsetY;
	g_collisionHitOffsetZ = g_glowMarkSavedCollisionHitOffsetZ;
}

// FUNCTION: XWA 0x4F3A80
void GlowMark_QueueCraftDamageSurfaceEffects(uint16_t objectIndex) {
	MobileObject* mobj;
	CraftData* craft;

	mobj = g_objectTable[objectIndex].mobj;
	if (mobj == NULL) {
		return;
	}

	craft = mobj->pCraft;
	if (craft == NULL) {
		return;
	}

	if ((craft->weaponFireInhibitTimer != 0 || craft->beamEffectAccum[2] != 0) && (GameRand2() & 0x0f) != 0) {
		float scale;

		scale = g_glowMarkSurfaceEffectBaseScale -
				(((float)(uint16_t)GameRand2() * g_glowMarkRandU16ToUnitFloatScale) *
				 g_modelTypeTable[g_objectTable[objectIndex].objectType].maxBoundsExtent) *
					g_glowMarkDamageEffectScale;
		GlowMark_QueueRandomObjectSurfaceEffect(objectIndex, OBJ_AnimationTextureGroup2007, scale);
	}

	craft = g_objectTable[objectIndex].mobj->pCraft;
	if (((craft->workingSubsystems == 0 && craft->subsystemDamage != 0) ||
		 craft->subsystemDamage > g_modelDefs[craft->modelIndex].systemStrength) &&
		(GameRand2() & 0x0f) != 0) {
		float scale;

		scale = g_glowMarkSurfaceEffectBaseScale -
				(((float)(uint16_t)GameRand2() * g_glowMarkRandU16ToUnitFloatScale) *
				 g_modelTypeTable[g_objectTable[objectIndex].objectType].maxBoundsExtent) *
					g_glowMarkDamageEffectScale;
		GlowMark_QueueRandomObjectSurfaceEffect(objectIndex, OBJ_AnimationTextureGroup3005, scale);
	}

	if (g_objectTable[objectIndex].mobj->pCraft->beamEffectAccum[1] != 0 && (GameRand2() & 0x0f) != 0) {
		float scale;

		scale = g_glowMarkSurfaceEffectBaseScale -
				(((float)(uint16_t)GameRand2() * g_glowMarkRandU16ToUnitFloatScale) *
				 g_modelTypeTable[g_objectTable[objectIndex].objectType].maxBoundsExtent) *
					g_glowMarkDamageEffectScale;
		GlowMark_QueueRandomObjectSurfaceEffect(objectIndex, OBJ_AnimationTextureGroup3055, scale);
	}

	if (g_provingGroundsModeActive) {
		switch ((int)g_objectTable[objectIndex].objectType) {
			case OBJ_AccelRing2:
			case OBJ_AccelRing3:
				if (g_objectTable[objectIndex].mobj->pCraft->componentHp[4] == 0 &&
					(uint16_t)GameRand2() % 100 < 10) {
					float scale;

					scale = g_glowMarkSurfaceEffectBaseScale -
							(((float)(uint16_t)GameRand2() * g_glowMarkRandU16ToUnitFloatScale) *
							 g_modelTypeTable[g_objectTable[objectIndex].objectType].maxBoundsExtent) *
								g_glowMarkAccelRingEffectScale;
					if ((unsigned int)g_objectTable[objectIndex]
							.mobj->pCraft->damageFromPlayer[g_localPlayer] > 0) {
						GlowMark_QueueRandomObjectSurfaceEffect(objectIndex, OBJ_AnimationTextureGroup2007,
																scale);
					} else {
						GlowMark_QueueRandomObjectSurfaceEffect(objectIndex, OBJ_AnimationTextureGroup3005,
																scale);
					}
				}
				break;
		}
	}
}

// FUNCTION: XWA 0x42E380
void EngineGlow_BuildRectQuadCorners(const OptEngineGlow* glow, int* viewQuad, float scale,
									 const int* unusedNormal, const int* upAxisView,
									 const int* rightAxisView) {
	double clampedScale;
	float upScale;
	double rightScale;
	int upX;
	int upY;
	int upZ;
	int xMinusUp;
	int yMinusUp;
	int zMinusUp;
	Vec3i rightOffset;

	(void)unusedNormal;

	if (scale < g_engineGlowRectScaleLimit) {
		clampedScale = scale;
	} else {
		clampedScale = g_engineGlowRectScaleLimit;
	}

	upScale = (float)(glow->dimensions.x * clampedScale * g_engineGlowRectHalfScale);
	rightScale = clampedScale * glow->dimensions.y * g_engineGlowRectHalfScale;

	upX = (int)((double)upAxisView[0] * upScale * g_engineGlowRectQ15Scale);
	upY = (int)((double)upAxisView[1] * upScale * g_engineGlowRectQ15Scale);
	upZ = (int)((double)upAxisView[2] * upScale * g_engineGlowRectQ15Scale);

	rightOffset.x = (int)((double)rightAxisView[0] * rightScale * g_engineGlowRectQ15Scale);
	rightOffset.y = (int)((double)rightAxisView[1] * rightScale * g_engineGlowRectQ15Scale);
	rightOffset.z = (int)((double)rightAxisView[2] * rightScale * g_engineGlowRectQ15Scale);

	xMinusUp = viewQuad[0] - upX;
	yMinusUp = viewQuad[1] - upY;

	viewQuad[3] = xMinusUp + rightOffset.x;
	viewQuad[4] = yMinusUp + rightOffset.y;
	zMinusUp = viewQuad[2] - upZ;
	viewQuad[5] = zMinusUp + rightOffset.z;

	viewQuad[6] = viewQuad[0] + upX + rightOffset.x;
	viewQuad[7] = viewQuad[1] + upY + rightOffset.y;
	viewQuad[8] = viewQuad[2] + upZ + rightOffset.z;

	viewQuad[9] = viewQuad[0] + upX - rightOffset.x;
	viewQuad[10] = viewQuad[1] + upY - rightOffset.y;
	viewQuad[11] = viewQuad[2] + upZ - rightOffset.z;

	viewQuad[12] = xMinusUp - rightOffset.x;
	viewQuad[13] = yMinusUp - rightOffset.y;
	viewQuad[14] = zMinusUp - rightOffset.z;
}

// FUNCTION: XWA 0x42E4F0
void EngineGlow_ExtrudeQuadAlongViewNormal(int* viewQuad, const int* lookAxisView, int depthScaleQ15) {
	int centerDot;
	int cornerIndex;
	int lookX;
	int lookY;
	int lookZ;
	int centerX;
	int centerY;
	int centerZ;

	lookZ = lookAxisView[2];
	lookY = lookAxisView[1];
	lookX = lookAxisView[0];
	centerZ = viewQuad[2];
	centerY = viewQuad[1];
	centerX = viewQuad[0];
	centerDot = Xwa_Dot3Q15ReuseXSlot(lookX, lookY, lookZ, centerX, centerY, centerZ);

	for (cornerIndex = 1; cornerIndex < 5; ++cornerIndex) {
		int zDelta;
		int otherZ;
		int deltaX;
		int deltaY;
		int deltaZ;
		int scaleQ15;

		if (centerDot < 0) {
			zDelta = viewQuad[cornerIndex * 3 + 2];
			otherZ = viewQuad[2];
		} else {
			zDelta = viewQuad[2];
			otherZ = viewQuad[cornerIndex * 3 + 2];
		}
		zDelta -= otherZ;

		deltaX = Xwa_Q15MulReuseFirstSlot(zDelta, lookAxisView[0]);
		deltaY = Xwa_Q15MulReuseFirstSlot(zDelta, lookAxisView[1]);
		deltaZ = Xwa_Q15MulReuseFirstSlot(zDelta, lookAxisView[2]);
		if (zDelta < 0) {
			scaleQ15 = 10000;
		} else {
			scaleQ15 = depthScaleQ15;
		}

		deltaX = Xwa_Q15MulReuseFirstSlot(deltaX, scaleQ15);
		deltaY = Xwa_Q15MulReuseFirstSlot(deltaY, scaleQ15);
		deltaZ = Xwa_Q15MulReuseFirstSlot(deltaZ, scaleQ15);
		viewQuad[cornerIndex * 3 + 2] += deltaZ;
		viewQuad[cornerIndex * 3] += deltaX;
		viewQuad[cornerIndex * 3 + 1] += deltaY;
	}
}

static __inline int EngineGlow_MeshRotationAngleQ16(unsigned char meshRotation) {
	return (int16_t)((uint16_t)(unsigned char)-meshRotation << 8);
}

// FUNCTION: XWA 0x42DB60
int16_t EngineGlow_BuildProjectedQuad(unsigned int objIdx, const OptEngineGlow* glow, unsigned char meshIdx,
									  int* outViewQuad, float scale) {
	ObjectTypeId modelTypeForMesh;
	CraftData* craft;
	uint16_t meshRotation;
	int positionZ;
	int lookAxisView[3];
	int firstAxisView[3];
	int secondAxisView[3];
	float projectedSize;
	float dimensionRatio;

	if (g_flightRenderToFrontend) {
		modelTypeForMesh = (ObjectTypeId)g_modelPreviewModelType;
	} else {
		modelTypeForMesh = g_objectTable[objIdx].objectType;
	}
	if (!glow) {
		return 0;
	}

	meshRotation = 0;
	craft = g_objectTable[objIdx].mobj->pCraft;
	if (craft) {
		meshRotation = craft->meshRotation[meshIdx];
	}

	g_rotatedX = (int)glow->position.x;
	g_rotatedY = (int)glow->position.y;
	positionZ = (int)glow->position.z;
	g_rotatedZ = positionZ;
	if (meshRotation) {
		ModelMesh_ApplyAnimatedMeshRotationToPoint(
			(int16_t)EngineGlow_MeshRotationAngleQ16((unsigned char)meshRotation), (uint16_t)modelTypeForMesh,
			meshIdx, g_rotatedX, g_rotatedY, positionZ);
		positionZ = g_rotatedZ;
	}

	pai_RotateLocalVectorToWorldScratchMaybeStatic(&g_objectTable[objIdx], g_rotatedX, positionZ,
												   -g_rotatedY);

	g_camRelWorldX = g_rotatedX + g_objectTable[objIdx].world_x;
	g_camRelWorldY = g_rotatedY + g_objectTable[objIdx].world_y;
	g_camRelWorldZ = g_rotatedZ + g_objectTable[objIdx].world_z;

	outViewQuad[0] = g_camRelWorldX - g_players[g_localPlayer].viewState.savedTargetX;
	outViewQuad[1] = g_camRelWorldY - g_players[g_localPlayer].viewState.savedTargetY;
	outViewQuad[2] = g_camRelWorldZ - g_players[g_localPlayer].viewState.savedTargetZ;

	viewX = TRANSFM2_CamMatDotRow0(outViewQuad[0], outViewQuad[1], outViewQuad[2]);
	viewY = TRANSFM2_CamMatDotRow1(outViewQuad[0], outViewQuad[1], outViewQuad[2]);
	viewZ = TRANSFM2_CamMatDotRow2(outViewQuad[0], outViewQuad[1], outViewQuad[2]);

	outViewQuad[0] = viewX;
	outViewQuad[1] = viewY;
	outViewQuad[2] = viewZ;

	projectedSize =
		((glow->dimensions.x <= glow->dimensions.y ? glow->dimensions.y : glow->dimensions.x) <=
				 glow->dimensions.z
			 ? glow->dimensions.z
			 : (glow->dimensions.x <= glow->dimensions.y ? glow->dimensions.y : glow->dimensions.x)) *
		256.0f / (double)viewZ;
	if (outViewQuad[2] < 1 || projectedSize < 1.0f) {
		return 0;
	}

	g_rotatedX = (int)(glow->lookAxis.x * 32767.0);
	g_rotatedY = (int)(glow->lookAxis.y * 32767.0);
	positionZ = (int)(glow->lookAxis.z * 32767.0);
	g_rotatedZ = positionZ;
	if (meshRotation) {
		ModelMesh_ApplyAnimatedMeshRotationToPoint(
			(int16_t)EngineGlow_MeshRotationAngleQ16((unsigned char)meshRotation), (uint16_t)modelTypeForMesh,
			meshIdx, g_rotatedX, g_rotatedY, positionZ);
		positionZ = g_rotatedZ;
	}
	pai_RotateLocalVectorToWorldScratchMaybeStatic(&g_objectTable[objIdx], g_rotatedX, positionZ,
												   -g_rotatedY);
	lookAxisView[0] = TRANSFM2_CamMatDotRow0(g_rotatedX, g_rotatedY, g_rotatedZ);
	lookAxisView[1] = TRANSFM2_CamMatDotRow1(g_rotatedX, g_rotatedY, g_rotatedZ);
	lookAxisView[2] = TRANSFM2_CamMatDotRow2(g_rotatedX, g_rotatedY, g_rotatedZ);

	g_rotatedX = (int)(glow->rightAxis.x * 32767.0);
	g_rotatedY = (int)(glow->rightAxis.y * 32767.0);
	positionZ = (int)(glow->rightAxis.z * 32767.0);
	g_rotatedZ = positionZ;
	if (meshRotation) {
		ModelMesh_ApplyAnimatedMeshRotationToPoint(
			(int16_t)EngineGlow_MeshRotationAngleQ16((unsigned char)meshRotation), (uint16_t)modelTypeForMesh,
			meshIdx, g_rotatedX, g_rotatedY, positionZ);
		positionZ = g_rotatedZ;
	}
	pai_RotateLocalVectorToWorldScratchMaybeStatic(&g_objectTable[objIdx], g_rotatedX, positionZ,
												   -g_rotatedY);
	firstAxisView[0] = TRANSFM2_CamMatDotRow0(g_rotatedX, g_rotatedY, g_rotatedZ);
	firstAxisView[1] = TRANSFM2_CamMatDotRow1(g_rotatedX, g_rotatedY, g_rotatedZ);
	firstAxisView[2] = TRANSFM2_CamMatDotRow2(g_rotatedX, g_rotatedY, g_rotatedZ);

	g_rotatedX = (int)(glow->upAxis.x * 32767.0);
	g_rotatedY = (int)(glow->upAxis.y * 32767.0);
	positionZ = (int)(glow->upAxis.z * 32767.0);
	g_rotatedZ = positionZ;
	if (meshRotation) {
		ModelMesh_ApplyAnimatedMeshRotationToPoint(
			(int16_t)EngineGlow_MeshRotationAngleQ16((unsigned char)meshRotation), (uint16_t)modelTypeForMesh,
			meshIdx, g_rotatedX, g_rotatedY, positionZ);
		positionZ = g_rotatedZ;
	}
	pai_RotateLocalVectorToWorldScratchMaybeStatic(&g_objectTable[objIdx], g_rotatedX, positionZ,
												   -g_rotatedY);
	secondAxisView[0] = TRANSFM2_CamMatDotRow0(g_rotatedX, g_rotatedY, g_rotatedZ);
	secondAxisView[1] = TRANSFM2_CamMatDotRow1(g_rotatedX, g_rotatedY, g_rotatedZ);
	secondAxisView[2] = TRANSFM2_CamMatDotRow2(g_rotatedX, g_rotatedY, g_rotatedZ);

	dimensionRatio = glow->dimensions.x / glow->dimensions.y;
	if (dimensionRatio <= 0.85000002f || dimensionRatio >= 1.2f) {
		EngineGlow_BuildRectQuadCorners(glow, outViewQuad, scale, lookAxisView, firstAxisView,
										secondAxisView);
	} else {
		float clampedScale;
		float radius;
		int dotFirst;
		int dotSecond;
		float dotSecondForLength;
		float dotLength;
		float firstX;
		float firstY;
		float firstZ;
		float secondX;
		float secondY;
		float secondZ;
		float majorX;
		float majorY;
		float majorZ;
		float minorX;
		float minorY;
		float minorZ;
		int majorOffsetX;
		int majorOffsetY;
		int majorOffsetZ;
		int minorOffsetX;
		int minorOffsetY;
		int minorOffsetZ;
		int centerX;
		int centerY;
		int centerZ;

		if (scale >= 0.80000001f) {
			clampedScale = 0.80000001f;
		} else {
			clampedScale = scale;
		}
		radius = clampedScale * glow->dimensions.x * 1.415f * 0.5f;

		centerX = outViewQuad[0];
		centerY = outViewQuad[1];
		centerZ = outViewQuad[2];

		dotFirst = Xwa_Q15Mul(firstAxisView[0], centerX) + Xwa_Q15Mul(firstAxisView[1], centerY) +
				   Xwa_Q15Mul(firstAxisView[2], centerZ);
		dotSecond = Xwa_Q15Mul(secondAxisView[0], centerX) + Xwa_Q15Mul(secondAxisView[1], centerY) +
					Xwa_Q15Mul(secondAxisView[2], centerZ);
		dotSecondForLength = (float)dotSecond;
		dotLength =
			(float)sqrt((double)dotFirst * (double)dotFirst + dotSecondForLength * dotSecondForLength);

		firstX = firstAxisView[0] * 0.000030518509f;
		firstY = firstAxisView[1] * 0.000030518509f;
		firstZ = firstAxisView[2] * 0.000030518509f;
		secondX = secondAxisView[0] * 0.000030518509f;
		secondY = secondAxisView[1] * 0.000030518509f;
		secondZ = secondAxisView[2] * 0.000030518509f;

		if (dotLength == 0.0f) {
			majorX = firstX;
			majorY = firstY;
			majorZ = firstZ;
			minorX = secondX;
			minorY = secondY;
			minorZ = secondZ;
		} else {
			double firstWeight;
			double secondWeight;

			firstWeight = (double)dotFirst / dotLength;
			secondWeight = dotSecondForLength / dotLength;
			majorX = (float)(secondX * secondWeight + firstX * firstWeight);
			majorY = (float)(secondY * secondWeight + firstY * firstWeight);
			majorZ = (float)(secondZ * secondWeight + firstZ * firstWeight);
			minorX = (float)(firstX * secondWeight - secondX * firstWeight);
			minorY = (float)(firstY * secondWeight - secondY * firstWeight);
			minorZ = (float)(firstZ * secondWeight - secondZ * firstWeight);
		}

		majorOffsetX = (int)(majorX * radius);
		majorOffsetY = (int)(majorY * radius);
		majorOffsetZ = (int)(majorZ * radius);
		minorOffsetX = (int)(minorX * radius);
		minorOffsetY = (int)(minorY * radius);
		minorOffsetZ = (int)(minorZ * radius);

		outViewQuad[3] = centerX + minorOffsetX;
		outViewQuad[4] = centerY + minorOffsetY;
		outViewQuad[5] = centerZ + minorOffsetZ;

		outViewQuad[6] = centerX + majorOffsetX;
		outViewQuad[7] = centerY + majorOffsetY;
		outViewQuad[8] = centerZ + majorOffsetZ;

		outViewQuad[9] = centerX - minorOffsetX;
		outViewQuad[10] = centerY - minorOffsetY;
		outViewQuad[11] = centerZ - minorOffsetZ;

		outViewQuad[12] = centerX - majorOffsetX;
		outViewQuad[13] = centerY - majorOffsetY;
		outViewQuad[14] = centerZ - majorOffsetZ;
	}

	EngineGlow_ExtrudeQuadAlongViewNormal(outViewQuad, lookAxisView,
										  (int)(scale * glow->dimensions.z * 32767.0));
	return 1;
}

// FUNCTION: XWA 0x42E640
int EngineGlow_AdjustSuperStarDestroyerDepth(int objIdx, int sortDepth, const OptEngineGlow* glow) {
	ObjectRecord* obj;
	MobileObject* mobj;
	int deltaX;
	int deltaY;
	int deltaZ;
	int cachedUpX;
	int cachedUpY;
	int cachedUpZ;
	int roughDistance;
	double viewUpRatio;

	obj = &g_objectTable[objIdx];

	deltaX = g_players[g_localPlayer].viewState.savedTargetX - obj->world_x;
	deltaY = g_players[g_localPlayer].viewState.savedTargetY - obj->world_y;
	deltaZ = g_players[g_localPlayer].viewState.savedTargetZ - obj->world_z;
	objIdx = deltaX;

	mobj = obj->mobj;
	cachedUpZ = mobj->cachedUpZ;
	cachedUpY = mobj->cachedUpY;
	cachedUpX = mobj->cachedUpX;
	objIdx = Xwa_Dot3Q15ReuseXSlot(cachedUpX, cachedUpY, cachedUpZ, objIdx, deltaY, deltaZ);

	roughDistance = collide_roughdistance3d(deltaX, deltaY, deltaZ);
	viewUpRatio = (double)objIdx / (double)(unsigned int)roughDistance;
	if (viewUpRatio > 0.15000001f) {
		sortDepth += (int)(glow->dimensions.x * viewUpRatio);
		return sortDepth;
	}

	sortDepth -= (int)glow->dimensions.x >> 2;
	return sortDepth;
}

// FUNCTION: XWA 0x42D590
void EngineGlow_RenderForObject(unsigned int objIdx) {
	unsigned int objectIndex;
	ObjectRecord* obj;
	unsigned int objectType;
	unsigned int modelIndex;
	uint8_t beamLevel;
	uint8_t laserRedirect;
	uint8_t shieldRedirect;
	OptEngineGlow** engineGlows = NULL;
	uint16_t engineGlowCount = 0;
	uint8_t* engineGlowMeshIdx = NULL;
	float scale;

	objectIndex = objIdx & 0xffffu;
	obj = &g_objectTable[objectIndex];
	objectType = (uint16_t)obj->objectType;
	modelIndex = (uint16_t)g_modelTypeTable[objectType].modelIndex;
	scale = 0.0f;

	if (objectIndex != (unsigned int)g_players[g_localPlayer].objectIndex) {
		if (modelIndex == 0xffffu) {
			if (objectType != OBJ_WarheadSpaceBomb) {
				return;
			}
			engineGlowCount = (uint16_t)g_spaceBombEngineGlowCount;
			engineGlows = g_spaceBombEngineGlows;
			engineGlowMeshIdx = g_spaceBombEngineGlowMeshIdx;
			if (!engineGlowCount) {
				return;
			}
			scale = (float)((double)obj->mobj->speed / (double)obj->mobj->pWarheadGuidance->minSpeed);
		} else {
			uint8_t modelEngineGlowCount;

			modelEngineGlowCount = g_modelDefs[modelIndex].engineGlowCount;
			if (!modelEngineGlowCount) {
				return;
			}
			engineGlowCount = (uint16_t)modelEngineGlowCount;
			engineGlows = g_modelDefs[modelIndex].engineGlows;
			engineGlowMeshIdx = g_modelDefs[modelIndex].engineGlowMeshIdx;
		}
	} else {
		if (modelIndex == 0xffffu) {
			return;
		}
		if (g_players[g_localPlayer].viewState.externalCameraActive ||
			(g_filmPlaybackMode && g_filmOverlayActive == 1)) {
			engineGlowCount = g_exteriorEngineGlowCount;
			engineGlows = g_exteriorEngineGlows;
			engineGlowMeshIdx = g_exteriorEngineGlowMeshIdx;
		} else {
			if (!g_players[g_localPlayer].cockpitVisible) {
				return;
			}
			engineGlowCount = g_cockpitEngineGlowCount;
			engineGlows = g_cockpitEngineGlows;
			engineGlowMeshIdx = g_cockpitEngineGlowMeshIdx;
		}
	}

	switch (engineGlowCount) {
		case 0:
			return;
		default:
			break;
	}

	g_curCraft = obj->mobj->pCraft;
	if (scale == g_engineGlowZero) {
		if (g_curCraft == NULL || !g_curCraft->engineOutputScale || !g_curCraft->workingSubsystems) {
			return;
		}
		switch (g_curCraft->objectKind) {
			case 5:
			case 6: {
				uint16_t maxSpeed;
				double boostWindow;
				double speedDelta;

				maxSpeed = (uint16_t)g_curCraft->aiFlight.maxSpeedCache;
				boostWindow = (double)(4 * (3600 - maxSpeed));
				speedDelta = (double)(obj->mobj->speed - maxSpeed);
				if (speedDelta > boostWindow) {
					scale = 12.0f;
				} else {
					scale = g_engineGlowOne - (float)(speedDelta / boostWindow) * g_engineGlowBoostScale;
				}
				break;
			}
			default: {
				float engineRedirectScale;

				beamLevel = g_curCraft->beamLevel;
				laserRedirect = g_curCraft->laserRedirect;
				shieldRedirect = g_curCraft->shieldRedirect;
				engineRedirectScale =
					(float)(((float)g_curCraft->engineOutputScale * g_engineGlowOutputScale) *
							((double)(16 - beamLevel - laserRedirect - shieldRedirect) *
							 g_engineGlowPowerMarginScale));
				scale = (float)((((double)g_curCraft->throttleSpeed * g_engineGlowThrottleScale) *
								 (g_engineGlowOne - (GameRand2() & 0xf) * g_engineGlowFlickerScale)) *
								engineRedirectScale);
				if (scale < g_engineGlowMinimumScale) {
					scale = 0.34999999f;
				}
				break;
			}
		}
	}

	{
		uint16_t emitterIndex;

		emitterIndex = 0;
		while (emitterIndex < engineGlowCount) {
			const OptEngineGlow* glow;
			EngineGlowKnockoutMark* knockout;
			uint8_t meshIdx;
			uint16_t drawEmitter;

			glow = engineGlows[emitterIndex];
			meshIdx = *engineGlowMeshIdx;
			drawEmitter = 1;
			knockout = g_objRenderState[objectIndex].engineGlowKnockouts;
			while (knockout != NULL && drawEmitter) {
				if (knockout->emitterIndex == emitterIndex) {
					drawEmitter = 0;
				}
				knockout = knockout->next;
			}

			if (drawEmitter && !glow->isDisabled) {
				TexLevel* texLevel;
				int viewQuad[15];

				FeDiskIo_SelectTextureFrame(OBJ_LightingEffectTextureGroup1000, 1u, 256);
				texLevel = g_modelTypeTable[OBJ_LightingEffectTextureGroup1000].curTexLevel;
				if (EngineGlow_BuildProjectedQuad(objectIndex, glow, meshIdx, viewQuad, scale)) {
					int minDepth;
					int cornerIndex;

					minDepth = 0x7fffffff;
					for (cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
						int cornerZ;

						if (minDepth > 0) {
							cornerZ = viewQuad[5 + 3 * cornerIndex];
							if (cornerZ < minDepth) {
								minDepth = cornerZ;
							}
						}
					}
					if (objectType == OBJ_SuperStarDestroyer) {
						minDepth = EngineGlow_AdjustSuperStarDestroyerDepth(objectIndex, minDepth, glow);
					}
					if (minDepth < 1) {
						minDepth = 1;
					}
					if (texLevel != NULL && texLevel->image != NULL) {
						/* Remaster: no draw-time capture — HD glows are
						 * state-derived (glb extras + eg_* craft state). */
						RenderQuad_DrawGlow(viewQuad, minDepth, (Std3DTextureSurface*)texLevel->image,
											(int)glow->outerColor, (int)glow->coreColor);
					}
				}
			}
			++emitterIndex;
			++engineGlowMeshIdx;
		}
	}
}

// FUNCTION: XWA 0x42D9C0
void EngineGlow_RenderSceneGlows(void) {
	unsigned int configIndex;

	configIndex = NetSession_GetPlayerCount() > 1;
	if (!g_gameConfig.engineGlow[configIndex]) {
		return;
	}

	if (g_flightRenderToFrontend) {
		ObjectTypeId modelType;
		int modelIndex;
		unsigned int engineGlowCount;
		OptEngineGlow** engineGlows;
		uint8_t* engineGlowMeshIdx;
		float scale;
		unsigned int glowIndex;

		modelType = (ObjectTypeId)g_modelPreviewModelType;
		modelIndex = g_modelTypeTable[modelType].modelIndex;
		if (modelIndex == -1) {
			return;
		}
		engineGlowCount = g_modelDefs[modelIndex].engineGlowCount;
		if (!engineGlowCount) {
			return;
		}

		engineGlows = g_modelDefs[modelIndex].engineGlows;
		engineGlowMeshIdx = g_modelDefs[modelIndex].engineGlowMeshIdx;
		scale = (float)(0.60000002 - (double)(GameRand2() & 0xf) * 0.0024999999);
		for (glowIndex = 0; glowIndex < engineGlowCount; ++glowIndex) {
			const OptEngineGlow* glow;
			TexLevel* texLevel;
			int viewQuad[15];

			glow = engineGlows[glowIndex];
			FeDiskIo_SelectTextureFrame(OBJ_LightingEffectTextureGroup1000, 1u, 256);
			texLevel = g_modelTypeTable[OBJ_LightingEffectTextureGroup1000].curTexLevel;
			if (EngineGlow_BuildProjectedQuad(0, glow, engineGlowMeshIdx[glowIndex], viewQuad, scale)) {
				int minDepth;
				int cornerIndex;

				minDepth = 0x7fffffff;
				for (cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
					int cornerZ;

					cornerZ = viewQuad[5 + 3 * cornerIndex];
					if (cornerZ < minDepth) {
						minDepth = cornerZ;
					}
				}
				if (modelType == OBJ_SuperStarDestroyer) {
					minDepth = EngineGlow_AdjustSuperStarDestroyerDepth(0, minDepth, glow);
				}
				if (minDepth < 1) {
					minDepth = 1;
				}
				if (texLevel != NULL && texLevel->image != NULL) {
					/* Remaster: no draw-time capture — the preview PiP
					 * derives glows from the glb extras + preview scale. */
					RenderQuad_DrawGlow(viewQuad, minDepth, (Std3DTextureSurface*)texLevel->image,
										(int)glow->outerColor, (int)glow->coreColor);
				}
			}
		}
	} else {
		RenderObjectListEntry* entry;

		for (entry = g_renderListHead; entry != NULL; entry = entry->next) {
			EngineGlow_RenderForObject((unsigned int)(uint16_t)entry->objectIdx);
		}
	}
}
