#include "xwa/flight/fediskio.h"
#include "aeron/log.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_texture.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/assets/string_table.h"
#include "xwa/audio/fsfx.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/film.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_net.h"
#include "xwa/flight/flight_text.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/net_session.h"
#include "xwa/flight/yard.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/render/std3d_device.h"
#include "xwa/util/color.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <stdio.h>
#include <string.h>

#ifndef XWA_MODERN
#define XWA_FEDISKIO_STDCALL __stdcall
extern void(XWA_FEDISKIO_STDCALL* g_OutputDebugStringA)(const char* lpOutputString);
#else
#define XWA_FEDISKIO_STDCALL
static void FeDiskIo_OutputDebugString(const char* outputString) { DebugPrintf("%s", outputString); }
#define g_OutputDebugStringA FeDiskIo_OutputDebugString
#endif

/* Map-room icon buffer: a pointer table (one slot per icon) followed by the icon
 * byte payload. */
enum {
	MAP_ROOM_ICON_TABLE_ENTRIES = 5570,
	MAP_ROOM_ICON_DATA_BYTES = 0xd05c - 0x5708,
};

/*
 * fediskio-owned resource handles + craft-load state (XWA 0x63CF38..0x63CF68),
 * allocated/freed by FeDiskIo_Init/FreeGlobalBuffers and set during craft load;
 * read by render/text/flight consumers via fediskio.h.
 */
// GLOBAL: XWA 0x63CF38
MemoryHandle g_visibleObjectsHandle;
// GLOBAL: XWA 0x63CF3C
MemoryHandle g_flightLog1BufferHandle;
// GLOBAL: XWA 0x63CF40
MemoryHandle g_flightTinyFontHandle;
// GLOBAL: XWA 0x63CF4C
MemoryHandle g_flightMicroFontHandle;
// GLOBAL: XWA 0x63CF50
MemoryHandle g_flightSmallFontHandle;
// GLOBAL: XWA 0x63CF58
uint8_t g_cockpitUsesTieHitEffectPlanes;
// GLOBAL: XWA 0x63CF68
char g_exteriorModelLoaded;
// GLOBAL: XWA 0x8D9640
char Buffer[256];
// GLOBAL: XWA 0x7D4B98
uint8_t* g_flightRetryPromptSaveBuffer;

// FUNCTION: XWA 0x4311A0
char FeDiskIo_LoadFlightSfxBanks(void) {
	char baseWaveDir[8];
	char fileName[40];
	char waveDir[40];
	char result;
	const char* qualityDir;

	strcpy(baseWaveDir, "wave\\");
	result = (char)g_flightConfSfxEnabled;
	if (g_flightConfSfxEnabled) {
		fsfx_ResetFlightSfxState(1);
		if (g_gameConfig.sfxExteriorEnabled || g_gameConfig.sfxInteriorEnabled ||
			(result = (char)g_gameConfig.sfxEngineEnabled) != 0) {
			strcpy(fileName, baseWaveDir);
			strcpy(waveDir, baseWaveDir);
			qualityDir = "FE_Low_Res\\";
			if (g_gameConfig.sfxQuality) {
				qualityDir = "FE_High_Res\\";
			}
			strcat(waveDir, qualityDir);

			strcat(fileName, "SfxBlastNew.lst");
			fsfx_LoadSfxList(fileName, 4, waveDir);

			if (g_flightPlayerCount == 1 && g_missionHeader.body.missionType != XWA_MISSION_TYPE_DEATH_STAR) {
				strcpy(fileName, baseWaveDir);
				strcat(fileName, "HangarSFX.lst");
				fsfx_LoadSfxList(fileName, 139, waveDir);

				if (g_gameConfig.sfxExteriorEnabled && !g_pilotData.hangarType) {
					strcpy(fileName, baseWaveDir);
					strcat(fileName, "HangarVoiceSFX.lst");
					fsfx_LoadSfxList(fileName, 2618, baseWaveDir);
				}
			}

			result = (char)g_provingGroundsModeActive;
			if (g_provingGroundsModeActive) {
				strcpy(fileName, baseWaveDir);
				strcat(fileName, "YardSFX.lst");
				result = (char)fsfx_LoadSfxList(fileName, 172, waveDir);
			}

			if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) {
				strcpy(fileName, baseWaveDir);
				strcat(fileName, "DSSfx.lst");
				return (char)fsfx_LoadSfxList(fileName, 161, waveDir);
			}
		}
	}

	return result;
}

// FUNCTION: XWA 0x4343C0
uint8_t* FeDiskIo_GetMeshVertexComponentMap(int modelType, int meshIndex) {
	MemoryHandle handle;

	handle = g_modelFloatHardpointDataHandles[modelType];
	if (!handle) {
		return 0;
	}

	return ((uint8_t**)Memory_LockHandle(handle))[meshIndex];
}

// FUNCTION: XWA 0x4343F0
ModelFloatHardpoint* FeDiskIo_GetMeshFloatHardpoint(int modelType, uint8_t hardpointIndex) {
	ModelFloatHardpoint* hardpoints;

	hardpoints = (ModelFloatHardpoint*)Memory_LockHandle(g_modelFloatHardpointDataHandles[modelType]);
	return &hardpoints[hardpointIndex];
}

static const uint8_t g_modelDefSpecialHardpointAction[21] = {
	0, 12, 1, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 12, 12, 12, 12, 12, 12, 11,
};

typedef union ModelDefLocalOutputBuffer {
	char debugText[2048];
	int floatXStorage[512];
} ModelDefLocalOutputBuffer;

// GLOBAL: XWA 0x5B30E0
const uint8_t g_optHardpointWeaponGroupKindByType[40] = {
	0, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// GLOBAL: XWA 0x5B2F78
const char g_craftModelListNames[2][32] = { "Spacecraft0", "Equipment0" };

// GLOBAL: XWA 0x5B2FB8
const int g_pilotKillScoreBaseByAiLevel[7] = { 3, 4, 4, 8, 12, 14, 0 };
// GLOBAL: XWA 0x5B2FD4
const int g_pilotRatingPromotionPointThresholds[25] = {
	250,  500,  750,  1250, 1750, 2250, 2750, 3250, 3750, 4250,  4750,  5250,  5750,
	6250, 6500, 6500, 7000, 7250, 7500, 7750, 8000, 9000, 10000, 11000, 11000,
};
// GLOBAL: XWA 0x5B304C
const uint8_t g_kalidorCrescentAwardByPerformanceTable[21] = {
	0xf8, 0x2a, 0, 0, 0, 0, 0, 5, 0, 0, 5, 0, 0, 4, 5, 0, 3, 4, 0, 2, 3,
};
// GLOBAL: XWA 0x5B306C
// TODO: currently unused — FeDiskIo_CommitFlightResults' Kalidor-Crescent
// score-loss path (XWA 0x42FA93) is not yet ported; table retained as data.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"
#endif
const int g_kalidorCrescentScoreLossThresholds[5] = {
	50000, 40000, 30000, 20000, 10000,
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
// GLOBAL: XWA 0x5B308C
const int g_missionAwardScoreMarginThresholds[3] = { 50000, 20000, 0 };
// GLOBAL: XWA 0x5B30A8
const unsigned int g_kalidorCrescentBonusScoreThresholds[6] = {
	35000, 70000, 105000, 140000, 175000, 210000,
};
// GLOBAL: XWA 0x5B30C0
const unsigned int g_pilotRankPromotionScoreThresholds[8] = {
	100, 10000, 17500, 25000, 33000, 41000, 52000, 65000,
};

// FUNCTION: XWA 0x4328B0
int FeDiskIo_BuildModelDef(uint16_t modelEntryIdx, uint16_t loadedModelSlot) {
	int modelSlot;
	int result;
	int meshCount;
	int meshIndex;
	int floatCount;
	ModelDefLocalOutputBuffer outputBuffer;
	int floatZAndNegY[512];
	int outY;
	int outZ;
	int outX;
	OptHardpointType outType;

	modelSlot = loadedModelSlot & -(g_flightRenderToFrontend == 0);
	result = ModelBounds_GetMaxExtent(modelSlot);
	g_modelTypeTable[modelSlot].maxBoundsExtent = result;

	if (modelEntryIdx == 0xffffu) {
		if (loadedModelSlot == 293) {
			g_spaceBombEngineGlowCount = 0;
			g_unusedSpaceBombEngineGlowInitByte = 0;
			meshCount = ModelMesh_GetCount(modelSlot);
			for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
				int glowCount;
				int glowIndex;
				int remainingGlowCount;

				glowCount = ModelMesh_CountEngineGlows(modelSlot, meshIndex);
				if ((uint16_t)glowCount != 0) {
					glowIndex = 0;
					remainingGlowCount = (uint16_t)glowCount;
					do {
						OptEngineGlow* engineGlow;

						engineGlow = ModelMesh_GetEngineGlowParam(modelSlot, meshIndex, glowIndex);
						g_spaceBombEngineGlows[g_spaceBombEngineGlowCount + glowIndex] = engineGlow;
						++glowIndex;
						--remainingGlowCount;
						g_spaceBombEngineGlowMeshIdx[g_spaceBombEngineGlowCount + glowIndex - 1] =
							(uint8_t)meshIndex;
					} while (remainingGlowCount != 0);
				}
				g_spaceBombEngineGlowCount = (uint8_t)(g_spaceBombEngineGlowCount + glowCount);
				result = glowCount;
			}
		}
		return result;
	}

	{
		int sizeX;
		int sizeY;
		int sizeZ;
		uint16_t shift;

		sizeX = ModelBounds_GetSizeX(modelSlot);
		if (sizeX < 0) {
			sizeX = -sizeX;
		}
		sizeY = ModelBounds_GetSizeY(modelSlot);
		if (sizeY < 0) {
			sizeY = -sizeY;
		}
		sizeZ = ModelBounds_GetSizeZ(modelSlot);
		if (sizeZ < 0) {
			sizeZ = -sizeZ;
		}

		shift = 0;
		while (sizeX > 640 || sizeY > 640 || sizeZ > 640) {
			sizeX >>= 1;
			sizeY >>= 1;
			sizeZ >>= 1;
			++shift;
		}
		g_modelDefs[modelEntryIdx].boundSizeShift = shift;
		g_modelDefs[modelEntryIdx].boundSizeX = (uint16_t)sizeX;
		g_modelDefs[modelEntryIdx].boundSizeY = (uint16_t)sizeY;
		g_modelDefs[modelEntryIdx].boundSizeZ = (uint16_t)sizeZ;
	}

	if (!g_modelDefs[modelEntryIdx].meshAttachData[3]) {
		g_modelDefs[modelEntryIdx].meshAttachData[3] = ModelBounds_GetMinZ(modelSlot);
		g_modelDefs[modelEntryIdx].meshAttachData[4] = ModelBounds_GetMinZ(modelSlot);
	}
	if (!g_modelDefs[modelEntryIdx].meshAttachData[1]) {
		g_modelDefs[modelEntryIdx].meshAttachData[1] = ModelBounds_GetMaxZ(modelSlot);
		g_modelDefs[modelEntryIdx].meshAttachData[2] = ModelBounds_GetMaxZ(modelSlot);
	}

	g_modelDefs[modelEntryIdx].childMountPoints[0] = 0;
	g_modelDefs[modelEntryIdx].childMountPoints[2] = ModelBounds_GetMinY(modelSlot);
	g_modelDefs[modelEntryIdx].childMountPoints[1] = 0;
	g_modelDefs[modelEntryIdx].childMountPoints[3] = 0;
	g_modelDefs[modelEntryIdx].childMountPoints[5] = ModelBounds_GetMaxY(modelSlot);
	g_modelDefs[modelEntryIdx].childMountPoints[4] = 0;
	g_modelDefs[modelEntryIdx].dockPointCount = 0;
	g_modelDefs[modelEntryIdx].engineGlowCount = 0;
	g_modelDefs[modelEntryIdx].auxHardpointCount = 0;
	g_modelDefs[modelEntryIdx].jammingPointCount = 0;

	meshCount = ModelMesh_GetCount(modelSlot);
	floatCount = 0;
	for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
		int glowCount;
		int hardpointCount;
		uint16_t hardpointIndex;
		MeshType meshType;

		glowCount = ModelMesh_CountEngineGlows(modelSlot, meshIndex);
		if ((uint16_t)glowCount != 0) {
			int glowIndex;
			int remainingGlowCount;

			glowIndex = 0;
			remainingGlowCount = (uint16_t)glowCount;
			do {
				OptEngineGlow* engineGlow;

				engineGlow = ModelMesh_GetEngineGlowParam(modelSlot, meshIndex, glowIndex);
				g_modelDefs[modelEntryIdx]
					.engineGlows[g_modelDefs[modelEntryIdx].engineGlowCount + glowIndex] = engineGlow;
				++glowIndex;
				--remainingGlowCount;
				g_modelDefs[modelEntryIdx]
					.engineGlowMeshIdx[g_modelDefs[modelEntryIdx].engineGlowCount + glowIndex - 1] =
					(uint8_t)meshIndex;
			} while (remainingGlowCount != 0);
		}
		g_modelDefs[modelEntryIdx].engineGlowCount =
			(uint8_t)(g_modelDefs[modelEntryIdx].engineGlowCount + glowCount);

		hardpointCount = ModelMesh_CountHardpoints(modelSlot, meshIndex);
		if (!hardpointCount) {
			continue;
		}

		meshType = ModelMesh_GetType(modelSlot, meshIndex);
		{
			int* floatZWrite;
			int* floatNegYWrite;
			int* floatXWrite;

			floatZWrite = &floatZAndNegY[floatCount];
			floatNegYWrite = &floatZAndNegY[floatCount + 256];
			floatXWrite = &outputBuffer.floatXStorage[floatCount + 256];
			for (hardpointIndex = 0; hardpointIndex < hardpointCount; ++hardpointIndex) {
				uint16_t handled;

				handled = 0;
				ModelMesh_GetHardpoint(modelSlot, meshIndex, hardpointIndex, &outType, &outX, &outY, &outZ);
				if ((unsigned int)(outType - OPT_HARDPOINT_Gunner) <=
					(unsigned int)(OPT_HARDPOINT_JammingPoint - OPT_HARDPOINT_Gunner)) {
					switch (g_modelDefSpecialHardpointAction[outType - OPT_HARDPOINT_Gunner]) {
						case 7:
							g_modelDefs[modelEntryIdx].meshAttachData[1] = outZ;
							g_modelDefs[modelEntryIdx].meshAttachData[0] = outY;
							handled = 1;
							break;
						case 9:
							g_modelDefs[modelEntryIdx].meshAttachData[3] = outZ;
							g_modelDefs[modelEntryIdx].meshAttachData[0] = outY;
							handled = 1;
							break;
						case 6:
							g_modelDefs[modelEntryIdx].meshAttachData[2] = outZ;
							g_modelDefs[modelEntryIdx].meshAttachData[0] = outY;
							handled = 1;
							break;
						case 8:
							g_modelDefs[modelEntryIdx].meshAttachData[4] = outZ;
							g_modelDefs[modelEntryIdx].meshAttachData[0] = outY;
							handled = 1;
							break;
						case 4:
							g_modelDefs[modelEntryIdx].meshAttachData[5] = outX;
							g_modelDefs[modelEntryIdx].meshAttachData[6] = outZ;
							g_modelDefs[modelEntryIdx].meshAttachData[7] = outY;
							handled = 1;
							break;
						case 5:
							g_modelDefs[modelEntryIdx].meshAttachData[8] = outX;
							g_modelDefs[modelEntryIdx].meshAttachData[9] = outZ;
							g_modelDefs[modelEntryIdx].meshAttachData[10] = outY;
							handled = 1;
							break;
						case 10:
							g_modelDefs[modelEntryIdx].primaryHardpointZ = (int16_t)outZ;
							g_modelDefs[modelEntryIdx].primaryHardpointY = (int16_t)outY;
							handled = 1;
							break;
						case 1:
							if (g_modelDefs[modelEntryIdx].dockPointCount < 9u) {
								g_modelDefs[modelEntryIdx]
									.dockPoints[g_modelDefs[modelEntryIdx].dockPointCount]
									.x = outX;
								g_modelDefs[modelEntryIdx]
									.dockPoints[g_modelDefs[modelEntryIdx].dockPointCount]
									.y = outY;
								g_modelDefs[modelEntryIdx]
									.dockPoints[g_modelDefs[modelEntryIdx].dockPointCount]
									.z = outZ;
								++g_modelDefs[modelEntryIdx].dockPointCount;
							}
							handled = 1;
							break;
						case 2:
							g_modelDefs[modelEntryIdx].childMountPoints[0] = outX;
							g_modelDefs[modelEntryIdx].childMountPoints[2] = outY;
							g_modelDefs[modelEntryIdx].childMountPoints[1] = outZ;
							handled = 1;
							break;
						case 3:
							g_modelDefs[modelEntryIdx].childMountPoints[3] = outX;
							g_modelDefs[modelEntryIdx].childMountPoints[5] = outY;
							g_modelDefs[modelEntryIdx].childMountPoints[4] = outZ;
							/* fall through */
						case 0:
							g_modelDefs[modelEntryIdx]
								.auxHardpoints[g_modelDefs[modelEntryIdx].auxHardpointCount]
								.x = outX;
							g_modelDefs[modelEntryIdx]
								.auxHardpoints[g_modelDefs[modelEntryIdx].auxHardpointCount]
								.y = outY;
							g_modelDefs[modelEntryIdx]
								.auxHardpoints[g_modelDefs[modelEntryIdx].auxHardpointCount]
								.z = outZ;
							++g_modelDefs[modelEntryIdx].auxHardpointCount;
							handled = 1;
							break;
						case 11:
							if (g_modelDefs[modelEntryIdx].jammingPointCount < 8u) {
								g_modelDefs[modelEntryIdx]
									.jammingPoints[g_modelDefs[modelEntryIdx].jammingPointCount]
									.x = outX;
								g_modelDefs[modelEntryIdx]
									.jammingPoints[g_modelDefs[modelEntryIdx].jammingPointCount]
									.y = outY;
								g_modelDefs[modelEntryIdx]
									.jammingPoints[g_modelDefs[modelEntryIdx].jammingPointCount]
									.z = outZ;
								++g_modelDefs[modelEntryIdx].jammingPointCount;
							} else {
								sprintf(outputBuffer.debugText,
										"ERROR:  Species %i has over %i jamming points.\r\n", modelSlot, 8);
								g_OutputDebugStringA(outputBuffer.debugText);
							}
							handled = 1;
							break;
						case 12:
							break;
					}
				}

				if (!handled) {
					if ((g_modelTypeTable[loadedModelSlot].flags & MODEL_TYPE_FLAG_EXPANDED_TARGET_PROBE) ==
						0) {
						uint16_t weaponType = (uint16_t)(outType + 279);
						int foundGroup;
						int group;

						foundGroup = 0;
						for (group = 0; group < 3; ++group) {
							if (g_modelDefs[modelEntryIdx].laserGroupWeaponType[group] == weaponType) {
								foundGroup = 1;
								break;
							}
						}
						if (!foundGroup) {
							for (group = 0; group < 2; ++group) {
								if (g_modelDefs[modelEntryIdx].warheadLauncherType[group] == weaponType) {
									foundGroup = 1;
									break;
								}
							}
						}
						if (!foundGroup) {
							if (g_optHardpointWeaponGroupKindByType[outType] == 1) {
								for (group = 0; group < 3; ++group) {
									if (!g_modelDefs[modelEntryIdx].laserGroupWeaponType[group]) {
										break;
									}
								}
								if (group < 3) {
									g_modelDefs[modelEntryIdx].laserGroupWeaponType[group] = weaponType;
									if (meshType == MESH_GunTurret || meshType == MESH_RotaryGunTurret ||
										meshType == MESH_SmallGun ||
										g_modelTypeTable[loadedModelSlot].genusId == GENUS_Freighter ||
										g_modelTypeTable[loadedModelSlot].genusId == GENUS_Container ||
										g_modelTypeTable[loadedModelSlot].genusId == GENUS_Platform ||
										g_modelTypeTable[loadedModelSlot].genusId == GENUS_Starship) {
										g_modelDefs[modelEntryIdx].laserGroupMountType[group] = 4;
									} else if (outType == OPT_HARDPOINT_IonCannon ||
											   outType == OPT_HARDPOINT_MagPulse) {
										g_modelDefs[modelEntryIdx].laserGroupMountType[group] = 2;
									} else {
										g_modelDefs[modelEntryIdx].laserGroupMountType[group] = 1;
									}
								}
							} else if (g_optHardpointWeaponGroupKindByType[outType] == 2) {
								for (group = 0; group < 2; ++group) {
									if (!g_modelDefs[modelEntryIdx].warheadLauncherType[group]) {
										break;
									}
								}
								if (group < 2) {
									g_modelDefs[modelEntryIdx].warheadLauncherType[group] = weaponType;
								}
							}
						}
					} else {
						if (outType == OPT_HARDPOINT_None ||
							g_optHardpointWeaponGroupKindByType[outType] == 1) {
							int originalX = outX;
							int floatSlot;

							floatSlot = floatCount;
							if (floatSlot < 256) {
								*floatXWrite = outX;
								*floatNegYWrite = -outY;
								*floatZWrite = outZ;
								++floatSlot;
								floatCount = floatSlot;
								++floatXWrite;
								++floatNegYWrite;
								++floatZWrite;
							}
							if (floatSlot < 256 && (originalX > 256 || originalX < -256)) {
								*floatXWrite = -originalX;
								*floatNegYWrite = -outY;
								*floatZWrite = outZ;
								floatCount = floatSlot + 1;
								++floatXWrite;
								++floatNegYWrite;
								++floatZWrite;
							}
						}
					}
				}
			}
		}
	}

	if ((g_modelTypeTable[loadedModelSlot].flags & MODEL_TYPE_FLAG_EXPANDED_TARGET_PROBE) == 0) {
		uint8_t weaponSlot;
		int remainingGroups;

		weaponSlot = 0;
		{
			uint16_t* laserGroupWeaponType;
			uint8_t* laserGroupFirstSlot;

			laserGroupWeaponType = g_modelDefs[modelEntryIdx].laserGroupWeaponType;
			laserGroupFirstSlot = g_modelDefs[modelEntryIdx].laserGroupFirstSlot;
			remainingGroups = 3;
			do {
				uint16_t targetType;
				uint8_t startSlot;
				uint16_t scanMesh;

				if (weaponSlot == 16) {
					*laserGroupWeaponType = 0;
					laserGroupFirstSlot[9] = 0;
				} else {
					targetType = (uint16_t)(*laserGroupWeaponType - 279);
					startSlot = weaponSlot;
					if (meshCount > 0) {
						scanMesh = 0;
						do {
							int hardpointCount;
							uint16_t scanHardpoint;
							MeshType meshType;
							uint16_t pairedSlot;

							hardpointCount = ModelMesh_CountHardpoints(modelSlot, scanMesh);
							if (hardpointCount) {
								meshType = ModelMesh_GetType(modelSlot, scanMesh);
								pairedSlot = 255;
								for (scanHardpoint = 0; scanHardpoint < hardpointCount; ++scanHardpoint) {
									ModelMesh_GetHardpoint(modelSlot, scanMesh, scanHardpoint, &outType,
														   &outX, &outY, &outZ);
									if (outType != (OptHardpointType)targetType) {
										continue;
									}

									if (pairedSlot == 255) {
										if (loadedModelSlot == 139) {
											outX >>= 1;
											outY >>= 1;
											outZ >>= 1;
										}
										g_modelDefs[modelEntryIdx].weaponHardpoints[weaponSlot].x =
											(int16_t)outX;
										g_modelDefs[modelEntryIdx].weaponHardpoints[weaponSlot].y =
											(int16_t)outY;
										g_modelDefs[modelEntryIdx].weaponHardpoints[weaponSlot].z =
											(int16_t)outZ;
										g_modelDefs[modelEntryIdx].weaponHardpoints[weaponSlot].meshIdx =
											(uint8_t)scanMesh;
										g_modelDefs[modelEntryIdx]
											.weaponHardpoints[weaponSlot]
											.alternateMeshHardpointIdx = 0xff;
										if (loadedModelSlot != 317 && (meshType == MESH_GunTurret ||
																	   meshType == MESH_RotaryGunTurret)) {
											pairedSlot = weaponSlot;
										}
										++weaponSlot;
										if (weaponSlot == 16) {
											break;
										}
									} else {
										g_modelDefs[modelEntryIdx]
											.weaponHardpoints[pairedSlot]
											.alternateMeshHardpointIdx =
											(uint8_t)ModelMesh_GetAlternateHardpointIndex(modelSlot, scanMesh,
																						  scanHardpoint);
										pairedSlot = 255;
									}
								}
								if (weaponSlot == 16) {
									break;
								}
							}
							++scanMesh;
						} while (scanMesh < meshCount);
					}
					if (weaponSlot != startSlot) {
						*laserGroupFirstSlot = startSlot;
						laserGroupFirstSlot[3] = (uint8_t)(weaponSlot - 1);
						laserGroupFirstSlot[6] = (uint8_t)(weaponSlot - startSlot);
					}
				}
				++laserGroupWeaponType;
				++laserGroupFirstSlot;
				--remainingGroups;
			} while (remainingGroups != 0);
		}

		{
			uint16_t* warheadLauncherType;
			uint8_t* warheadLauncherLastSlot;

			warheadLauncherType = g_modelDefs[modelEntryIdx].warheadLauncherType;
			warheadLauncherLastSlot = g_modelDefs[modelEntryIdx].warheadLauncherLastSlot;
			remainingGroups = 2;
			do {
				uint16_t targetType;
				uint8_t startSlot;
				uint16_t scanMesh;

				if (weaponSlot == 16) {
					*warheadLauncherType = 0;
				} else {
					targetType = (uint16_t)(*warheadLauncherType - 279);
					startSlot = weaponSlot;
					if (meshCount > 0) {
						scanMesh = 0;
						do {
							int hardpointCount;
							uint16_t scanHardpoint;

							hardpointCount = ModelMesh_CountHardpoints(modelSlot, scanMesh);
							if (hardpointCount) {
								ModelMesh_GetType(modelSlot, scanMesh);
								for (scanHardpoint = 0; scanHardpoint < hardpointCount; ++scanHardpoint) {
									ModelMesh_GetHardpoint(modelSlot, scanMesh, scanHardpoint, &outType,
														   &outX, &outY, &outZ);
									if (outType != (OptHardpointType)targetType) {
										continue;
									}
									if (loadedModelSlot == 139) {
										outX >>= 1;
										outY >>= 1;
										outZ >>= 1;
									}
									g_modelDefs[modelEntryIdx].weaponHardpoints[weaponSlot].x = (int16_t)outX;
									g_modelDefs[modelEntryIdx].weaponHardpoints[weaponSlot].y = (int16_t)outY;
									g_modelDefs[modelEntryIdx].weaponHardpoints[weaponSlot].z = (int16_t)outZ;
									g_modelDefs[modelEntryIdx].weaponHardpoints[weaponSlot].meshIdx =
										(uint8_t)scanMesh;
									++weaponSlot;
									if (weaponSlot == 16) {
										break;
									}
								}
								if (weaponSlot == 16) {
									break;
								}
							}
							++scanMesh;
						} while (scanMesh < meshCount);
					}
					if (weaponSlot != startSlot) {
						warheadLauncherLastSlot[-2] = startSlot;
						*warheadLauncherLastSlot = (uint8_t)(weaponSlot - 1);
						warheadLauncherLastSlot[2] = (uint8_t)(weaponSlot - startSlot);
					}
				}
				++warheadLauncherType;
				++warheadLauncherLastSlot;
				--remainingGroups;
			} while (remainingGroups != 0);
		}
	} else {
		if (g_flightRenderToFrontend == 0) {
			if (floatCount) {
				MemoryHandle handle;
				ModelFloatHardpoint* floatHardpoints;
				int* floatZRead;
				int i;

				handle = Memory_AllocHandleZeroed("FLOATHARDPTS",
												  sizeof(ModelFloatHardpoint) * (size_t)floatCount);
				if (!handle) {
					FeDiskIo_FatalError(0);
				}
				g_modelFloatHardpointDataHandles[modelSlot] = handle;
				floatHardpoints = (ModelFloatHardpoint*)Memory_LockHandle(handle);
				floatZRead = floatZAndNegY;
				for (i = 0; i < floatCount; ++i) {
					floatHardpoints->x = outputBuffer.floatXStorage[i + 256];
					floatHardpoints->negY = floatZAndNegY[i + 256];
					floatHardpoints->z = *floatZRead++;
					++floatHardpoints;
				}
			}
			g_modelDefs[modelEntryIdx].floatHardpointCount = (uint8_t)floatCount;
		} else {
			g_modelDefs[modelEntryIdx].floatHardpointCount = 0;
		}
	}

	result = ComputeCraftCombatRating((ObjectTypeId)loadedModelSlot);
	g_modelDefs[modelEntryIdx].craftPointValue = (uint16_t)result;
	return result;
}

// FUNCTION: XWA 0x433760
void FeDiskIo_LoadExteriorGlowEmitters(unsigned int modelSlot) {
	uint16_t exteriorModel;
	uint16_t loadedModelHandle;
	int meshCount;
	int meshIndex;
	int meshIndexCounter;

	loadedModelHandle = g_loadedModels.byObjectType[modelSlot];
	exteriorModel = g_exteriorModel;
	if (exteriorModel != 0) {
		g_drawingOwnCraft = 1;
		g_loadedModels.byObjectType[modelSlot] = exteriorModel;
	}

	g_exteriorEngineGlowCount = 0;
	meshCount = ModelMesh_GetCount((int)modelSlot);
	meshIndexCounter = 0;
	if (meshCount > 0) {
		meshIndex = 0;
		do {
			int glowCount;

			glowCount = ModelMesh_CountEngineGlows((int)modelSlot, meshIndex);
			if ((uint16_t)glowCount > 0) {
				int glowIndex;
				int remainingGlowCount;

				glowIndex = 0;
				remainingGlowCount = (uint16_t)glowCount;
				do {
					OptEngineGlow* engineGlow;

					engineGlow = ModelMesh_GetEngineGlowParam((int)modelSlot, meshIndex, glowIndex);
					g_exteriorEngineGlows[glowIndex + g_exteriorEngineGlowCount] = engineGlow;
					g_exteriorEngineGlowMeshIdx[g_exteriorEngineGlowCount + glowIndex] =
						(uint8_t)meshIndexCounter;
					++glowIndex;
					--remainingGlowCount;
				} while (remainingGlowCount != 0);
			}
			g_exteriorEngineGlowCount = (uint8_t)(g_exteriorEngineGlowCount + glowCount);
			++meshIndexCounter;
			meshIndex = meshIndexCounter & 0xffff;
		} while (meshIndex < meshCount);
	}

	{
		uint16_t restoreModelHandle;

		restoreModelHandle = loadedModelHandle;
		g_loadedModels.byObjectType[modelSlot] = restoreModelHandle;
	}
	g_drawingOwnCraft = 0;
}

// FUNCTION: XWA 0x433580
void FeDiskIo_LoadCockpitGlowEmitters(unsigned int modelSlot) {
	uint16_t loadedModelHandle;
	int meshCount;
	uint16_t meshIndex;

	g_cockpitViewActive = 1;
	loadedModelHandle = g_loadedModels.byObjectType[modelSlot];
	g_loadedModels.byObjectType[modelSlot] = g_cockpitModel;

	g_cockpitEngineGlowCount = 0;
	g_cockpitSparkHardpointCount = 0;
	meshCount = ModelMesh_GetCount((int)modelSlot);
	for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
		uint16_t glowCount;
		int hardpointCount;
		uint16_t hardpointIndex;

		glowCount = (uint16_t)ModelMesh_CountEngineGlows((int)modelSlot, meshIndex);
		if (glowCount > 0) {
			int glowIndex;
			int remainingGlowCount;

			glowIndex = 0;
			remainingGlowCount = glowCount;
			do {
				OptEngineGlow* engineGlow;

				engineGlow = ModelMesh_GetEngineGlowParam((int)modelSlot, meshIndex, glowIndex);
				g_cockpitEngineGlows[g_cockpitEngineGlowCount + glowIndex] = engineGlow;
				++glowIndex;
				g_cockpitEngineGlowMeshIdx[g_cockpitEngineGlowCount + glowIndex - 1] = (uint8_t)meshIndex;
				--remainingGlowCount;
			} while (remainingGlowCount != 0);
		}
		g_cockpitEngineGlowCount = (uint8_t)(g_cockpitEngineGlowCount + glowCount);

		hardpointCount = ModelMesh_CountHardpoints((int)modelSlot, meshIndex);
		if (hardpointCount != 0) {
			ModelMesh_GetType((int)modelSlot, meshIndex);
			for (hardpointIndex = 0; hardpointIndex < hardpointCount; ++hardpointIndex) {
				OptHardpointType hardpointType;
				int hardpointX;
				int hardpointY;
				int hardpointZ;

				ModelMesh_GetHardpoint((int)modelSlot, meshIndex, hardpointIndex, &hardpointType, &hardpointX,
									   &hardpointY, &hardpointZ);
				if (hardpointType == OPT_HARDPOINT_CockpitSparks) {
					CockpitSparkHardpoint* spark;

					spark = &g_cockpitSparkHardpoints[g_cockpitSparkHardpointCount];
					spark->localX = (float)hardpointX;
					spark->localZ = (float)hardpointZ;
					spark->localY = (float)hardpointY;
					spark->hardpointIndex = (uint8_t)hardpointIndex;
					++g_cockpitSparkHardpointCount;
					if (g_cockpitSparkHardpointCount == 16) {
						hardpointIndex = (uint16_t)hardpointCount;
						meshIndex = (uint16_t)meshCount;
					}
				}
			}
		}
	}

	g_loadedModels.byObjectType[modelSlot] = loadedModelHandle;
	g_cockpitViewActive = 0;
}

// FUNCTION: XWA 0x432120
int16_t FeDiskIo_LoadTexturesForType(uint16_t modelType) {
	uint16_t levelCount;
	int spriteIndex;
	uint16_t flags;
	int format;
	int baseShift;

	FlightNet_BroadcastStillLoadingPulse();

	if (modelType == MODEL_TEXTURE_PAIR_MODEL_TYPE) {
		FeDiskIo_LoadTexturesForType(MODEL_TEXTURE_PAIR_LOAD_TYPE);
	}

	levelCount = 4;
	spriteIndex = 0;
	if (g_useHardware3D) {
		flags = g_modelTypeTable[modelType].flags;
		if ((flags & 0x8000) != 0) {
			format = SPRITE_TEXTURE_FORMAT_ARGB4444;
		} else if ((flags & 0x4000) != 0) {
			format = SPRITE_TEXTURE_FORMAT_RGB565_CK;
		} else {
			format = SPRITE_TEXTURE_FORMAT_RGB565;
		}
		if ((flags & 0x0400) != 0) {
			format |= SPRITE_TEXTURE_FORMAT_CK_BORDER;
		}
	} else if ((g_modelTypeTable[modelType].flags & 0xc000) != 0) {
		format =
			Display_IsPixelFormat555() ? SPRITE_TEXTURE_FORMAT_PAL555_CK : SPRITE_TEXTURE_FORMAT_PAL565_CK;
	} else {
		format = Display_IsPixelFormat555() ? SPRITE_TEXTURE_FORMAT_PAL555_OPAQUE
											: SPRITE_TEXTURE_FORMAT_PAL565_OPAQUE;
	}

	flags = g_modelTypeTable[modelType].flags;
	if ((flags & MODEL_TYPE_FLAG_SINGLE_MIP_LEVEL) != 0) {
		baseShift = 0;
		levelCount = 1;
	} else {
		baseShift = 2 - g_explosionResLevel;
	}

	if ((flags & 0x0100) != 0) {
		uint16_t spriteCount;

		spriteCount = SpriteResource_GetGroupSpriteCount((int16_t)g_modelTypeTable[modelType].textureGroup);
		g_modelTypeTable[modelType].frameCount = spriteCount;
		if (spriteCount == 0 || spriteCount == 0xffff) {
			return (int16_t)SpriteResource_UnloadGroup((int16_t)g_modelTypeTable[modelType].textureGroup);
		}
		{
			TexLevel* texLevel;

			g_modelTypeTable[modelType].texLevels = (TexLevel*)Memory_CallocTagged(
				MODEL_TEXTURE_SPECIESTMINFO_TAG, (uint16_t)(levelCount * spriteCount), sizeof(TexLevel));
			texLevel = g_modelTypeTable[modelType].texLevels;
			if (g_modelTypeTable[modelType].texLevels == NULL) {
				FeDiskIo_FatalError(0);
			}

			for (; (uint16_t)spriteIndex < spriteCount; ++spriteIndex) {
				SpriteTexture_ConvertByIndex((int16_t)g_modelTypeTable[modelType].textureGroup, spriteIndex,
											 format, texLevel, (char)baseShift, levelCount);
				if (g_useHardware3D) {
					int levelIndex;

					if (levelCount > 0) {
						for (levelIndex = 0; levelIndex < levelCount; ++levelIndex) {
							if (modelType != MODEL_TEXTURE_FONT_MODEL_TYPE) {
								FeDiskIo_UploadTexLevelToStd3D(texLevel++);
							} else {
								++texLevel;
							}
						}
					}
				} else {
					texLevel += levelCount;
				}
				DebugPrintf((const char*)(intptr_t)(int16_t)g_modelTypeTable[modelType].textureGroup,
							spriteIndex);
			}

			if (g_useHardware3D && modelType == MODEL_TEXTURE_FONT_MODEL_TYPE) {
				TexLevel* remappedTexLevels;
				uint16_t remapCount;

				DebugPrintfChannel(0x10000, "Starting to remap fonts..\n");
				remappedTexLevels = FlightText_RemapHardwareFontTextures(
					g_modelTypeTable[MODEL_TEXTURE_FONT_MODEL_TYPE].texLevels);
				remapCount = FlightText_GetHardwareRemapTextureCount();
				if (remapCount > 0) {
					int remapRemaining;

					remapRemaining = remapCount;
					do {
						FeDiskIo_UploadTexLevelToStd3D(remappedTexLevels++);
						--remapRemaining;
					} while (remapRemaining != 0);
				}
				DebugPrintfChannel(0x10000, "Finished remapping fonts..\n");
			}
		}
	} else {
		TexLevel* texLevel;

		texLevel =
			(TexLevel*)Memory_CallocTagged(MODEL_TEXTURE_SPECIESTMINFO_TAG, levelCount, sizeof(TexLevel));
		SpriteTexture_ConvertById(g_modelTypeTable[modelType].textureGroup,
								  g_modelTypeTable[modelType].frameCount, format, texLevel, (char)baseShift,
								  levelCount);
		g_modelTypeTable[modelType].curTexLevel = texLevel;
		if (g_useHardware3D) {
			if (levelCount > 0) {
				int singleLevelsRemaining;

				singleLevelsRemaining = levelCount;
				do {
					FeDiskIo_UploadTexLevelToStd3D(texLevel++);
					--singleLevelsRemaining;
				} while (singleLevelsRemaining != 0);
			}
		}
	}

#ifdef XWA_MODERN
	{
		int16_t unloadResult;

		unloadResult = (int16_t)SpriteResource_UnloadGroup((int16_t)g_modelTypeTable[modelType].textureGroup);
		XwaSnapshot_NoteTextureLoad(modelType);
		return unloadResult;
	}
#else
	return (int16_t)SpriteResource_UnloadGroup((int16_t)g_modelTypeTable[modelType].textureGroup);
#endif
}

// FUNCTION: XWA 0x432400
void FeDiskIo_FreeTexturesForType(uint16_t modelType) {
	TexLevel* texLevels;
	uint16_t levelIndex;
	uint16_t frameCount;
	uint16_t levelCount;

	levelCount = (g_modelTypeTable[modelType].flags & MODEL_TYPE_FLAG_SINGLE_MIP_LEVEL) != 0 ? 1 : 4;
	if (modelType == MODEL_TEXTURE_PAIR_MODEL_TYPE) {
		FeDiskIo_FreeTexturesForType(MODEL_TEXTURE_PAIR_LOAD_TYPE);
	}

	if ((g_modelTypeTable[modelType].flags & 0x0100) != 0) {
		frameCount = g_modelTypeTable[modelType].frameCount;
		g_modelTypeTable[modelType].frameCount = 0;
		texLevels = g_modelTypeTable[modelType].texLevels;
	} else {
		texLevels = g_modelTypeTable[modelType].curTexLevel;
		frameCount = 1;
	}

	levelIndex = 0;
	g_modelTypeTable[modelType].texLevels = NULL;
	g_modelTypeTable[modelType].curTexLevel = NULL;
	if (texLevels != NULL) {
		for (; levelIndex < levelCount * frameCount; ++levelIndex) {
			if (texLevels[levelIndex].image != NULL) {
				if (g_useHardware3D) {
					std3D_DeleteTextureSurface(texLevels[levelIndex].image);
				} else {
					Memory_FreeTagged(MODEL_TEXTURE_RESOURCEITEM_TAG, texLevels[levelIndex].image);
				}
			}
		}
	}

	Memory_FreeTagged(MODEL_TEXTURE_SPECIESTMINFO_TAG, texLevels);
#ifdef XWA_MODERN
	XwaSnapshot_NoteTextureFree(modelType);
#endif
}

// FUNCTION: XWA 0x432750
int16_t FeDiskIo_SelectTextureFrame(uint16_t modelType, uint16_t frame, int screenSize) {
	TexLevel* texLevels;
	uint16_t levelsPerFrame;
	TexLevel* selectedLevel;
	TexLevel* nextLevel;
	int scaledScreenSize;

	selectedLevel = NULL;
	if (g_useHardware3D == 0 && modelType == 481) {
		if (frame == 2) {
			modelType = 453;
			frame = 7;
		} else if (frame == 3) {
			modelType = 453;
			frame = 5;
		} else if (frame == 4 || frame == 5) {
			g_modelTypeTable[481].curTexLevel = selectedLevel;
			return (int16_t)screenSize;
		}
	}

	texLevels = g_modelTypeTable[modelType].texLevels;
	if (texLevels == NULL) {
		return (int16_t)screenSize;
	}

	if (frame <= 0 || frame > g_modelTypeTable[modelType].frameCount) {
		g_modelTypeTable[modelType].curTexLevel = selectedLevel;
		return (int16_t)screenSize;
	}

	levelsPerFrame = (g_modelTypeTable[modelType].flags & 0x20) != 0 ? 1 : 4;
	selectedLevel = &texLevels[(uint16_t)((frame - 1) * levelsPerFrame)];
	nextLevel = selectedLevel;
	do {
		selectedLevel = nextLevel;
		nextLevel = selectedLevel + 1;
		--levelsPerFrame;
		scaledScreenSize = screenSize << selectedLevel->shift;
		/* Original checks nextLevel first; modern builds avoid the one-past probe
		 * when this texture has no further mip levels.
		 */
#ifdef XWA_MODERN
	} while (levelsPerFrame > 1 && (uint16_t)screenSize <= 0x00c0 && nextLevel->width != 0);
#else
	} while (nextLevel->width != 0 && levelsPerFrame > 1 && (uint16_t)screenSize <= 0x00c0);
#endif

	g_modelTypeTable[modelType].curTexLevel = selectedLevel;
	return (int16_t)scaledScreenSize;
}

// FUNCTION: XWA 0x4324E0
void FeDiskIo_UploadTexLevelToStd3D(TexLevel* level) {
	Sprite* const image = level->image;
	Std3DTextureFormatMode fmt;
	Std3DVBuffer vb;
	Std3DVBuffer* srcVBuffer;
	void* alphaPlane;
	Std3DTextureSurface* outSurface;

	if (level->image != NULL) {
		/* Build a single-level source raster from the RESOURCEITEM (Sprite-header
		 * format), choosing the destination texel format and source ColorInfo from
		 * the item's TextureFormat type. */
		srcVBuffer = &vb;
		alphaPlane = NULL;
#ifdef XWA_MODERN
		outSurface = NULL;
#endif
		memset(&vb, 0, sizeof(vb));
		fmt = STD3D_TEXFMT_RGB565;

		switch (image->type) {
			case SPRITE_TEXTURE_FORMAT_RGB565_CK:
			case SPRITE_TEXTURE_FORMAT_PAL565_CK:
				fmt = STD3D_TEXFMT_RGBA1555;
				/* fallthrough: shares the R5G6B5 source layout below */
			case SPRITE_TEXTURE_FORMAT_RGB565:
			case SPRITE_TEXTURE_FORMAT_PAL565_OPAQUE:
				vb.raster.sourceType = 1;
				vb.raster.bitsPerPixel = 16;
				vb.raster.alphaBPP = 0;
				vb.raster.redBPP = 5;
				vb.raster.greenBPP = 6;
				vb.raster.blueBPP = 5;
				vb.raster.alphaPosShift = 0;
				vb.raster.redPosShift = 11;
				vb.raster.greenPosShift = 5;
				vb.raster.bluePosShift = 0;
				vb.transparentColor = image->colorKey;
				break;
			case SPRITE_TEXTURE_FORMAT_RGB555:
			case SPRITE_TEXTURE_FORMAT_PAL555_CK:
				fmt = STD3D_TEXFMT_RGBA1555;
				/* fallthrough: shares the X1R5G5B5 source layout below */
			case SPRITE_TEXTURE_FORMAT_RGB555_OPAQUE:
			case SPRITE_TEXTURE_FORMAT_PAL555_OPAQUE:
				vb.raster.sourceType = 1;
				vb.raster.bitsPerPixel = 16;
				vb.raster.alphaBPP = 1;
				vb.raster.redBPP = 5;
				vb.raster.greenBPP = 5;
				vb.raster.blueBPP = 5;
				vb.raster.alphaPosShift = 15;
				vb.raster.redPosShift = 10;
				vb.raster.greenPosShift = 5;
				vb.raster.bluePosShift = 0;
				break;
			case SPRITE_TEXTURE_FORMAT_ARGB8888:
				vb.raster.sourceType = 2;
				vb.raster.bitsPerPixel = 32;
				vb.raster.alphaBPP = 8;
				vb.raster.redBPP = 8;
				vb.raster.greenBPP = 8;
				vb.raster.blueBPP = 8;
				vb.raster.alphaPosShift = 24;
				vb.raster.redPosShift = 16;
				fmt = STD3D_TEXFMT_RGBA4444;
				break;
			case SPRITE_TEXTURE_FORMAT_ARGB4444:
				vb.raster.sourceType = 2;
				vb.raster.bitsPerPixel = 16;
				vb.raster.alphaBPP = 4;
				vb.raster.redBPP = 4;
				vb.raster.greenBPP = 4;
				vb.raster.blueBPP = 4;
				vb.raster.alphaPosShift = 12;
				vb.raster.redPosShift = 8;
				fmt = STD3D_TEXFMT_RGBA4444;
				break;
			default: /* unknown type: leave the zeroed raster in place */
				break;
		}
		if (fmt == STD3D_TEXFMT_RGBA4444) {
			vb.raster.greenPosShift = vb.raster.blueBPP;
			vb.raster.bluePosShift = 0;
		}

		{
			uint16_t width;
			uint16_t height;

			vb.raster.bluePosShiftRight = (-vb.raster.blueBPP) & 7u;
			vb.raster.redPosShiftRight = (-vb.raster.redBPP) & 7u;
			vb.pixels = (uint8_t*)image + 18; /* item+18: pixels follow the header */
			width = level->width;
			vb.raster.rowPitch = width * (vb.raster.bitsPerPixel >> 3);
			vb.raster.greenPosShiftRight = (-vb.raster.greenBPP) & 7u;
			height = level->height;
			vb.raster.width = width;
			vb.raster.height = height;
			vb.raster.alphaPosShiftRight = (-vb.raster.alphaBPP) & 7u;

			std3D_CreateMipSurface(&outSurface, &srcVBuffer, fmt, &alphaPlane, 1, 0);
			level->image = (Sprite*)outSurface;
			Memory_FreeTagged(MODEL_TEXTURE_RESOURCEITEM_TAG, image);
		}
	}
}

static void FlightResources_LoadFileIntoBuffer(const char* fileName, uint8_t* dst) {
	XwaFile* stream;
	uint8_t buffer[512];
	size_t bytesRead;

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	g_stream = stream;
	if (stream == NULL) {
		FeDiskIo_FatalError(1);
		return;
	}

	do {
		bytesRead = File_ReadPartial(stream, buffer, sizeof(buffer));
		if (bytesRead != 0) {
			memcpy(dst, buffer, bytesRead);
			dst += bytesRead;
		}
	} while (bytesRead == sizeof(buffer));

	File_Close(stream);
	g_stream = NULL;
}

#ifndef XWA_MODERN
int File_OpenGlobalStream(const char* fileName, const char* mode, int promptOnFail, int locationMode) {
	int retryCount;
	int16_t key;

	strcpy(FileName, fileName);
	while (1) {
		if (locationMode != 2) {
			g_stream = (XwaFile*)fopen(fileName, mode);
			if (g_stream != NULL) {
				return 1;
			}
		}
		if (locationMode != 1) {
			strcpy(FileName, "D:\\");
			FileName[0] = File_GetCdDriveLetter();
			strcat(FileName, fileName);
			retryCount = 3;
			do {
				g_stream = (XwaFile*)fopen(FileName, mode);
				if (g_stream != NULL) {
					return 1;
				}
			} while (retryCount-- != 0);
		}
		if (!promptOnFail) {
			break;
		}
		while (1) {
			key = (int16_t)FeDiskIo_ShowRetryFailPrompt();
			if (key == 'R' || key == 'r') {
				break;
			}
			if (key == 'F' || key == 'f') {
				FeDiskIo_FatalError(1);
			}
		}
	}
	g_stream = NULL;
	return 0;
}
#endif

// FUNCTION: XWA 0x4309E0
void FeDiskIo_InitGlobalBuffers(void) {
	int16_t allocFailed;
#ifndef XWA_MODERN
	FILE* savedStream;
	uint8_t buffer[512];
#endif

	allocFailed = 0;
	g_unusedFlightResourceInitZero = 0;
	g_flightFontSmallSw = NULL;
	g_flightFontMediumSw = NULL;

	if (g_filmWriteBufferHandle != 0 || g_flightTinyFontHandle != 0 || g_flightMicroFontHandle != 0 ||
		g_flightSmallFontHandle != 0 || g_flightLog1BufferHandle != 0 || g_mapRoomIconsHandle != 0 ||
		g_messageLogHandle != 0 || g_visibleObjectsHandle != 0) {
		DebugPrintf("memory leak in InitGlobalBuffers()");
	}

	g_filmWriteBufferHandle = 0;
	g_flightTinyFontHandle = 0;
	g_flightMicroFontHandle = 0;
	g_flightSmallFontHandle = 0;
	g_flightLog1BufferHandle = 0;
	g_mapRoomIconsHandle = 0;
	g_messageLogHandle = 0;
	g_visibleObjectsHandle = 0;
	g_filmWriteBufferSize = 0x2000;
	g_filmWriteBufferHandle = Memory_AllocHandle("FILMBUFFER", 0x2000u);
	g_filmWriteBuffer = (uint8_t*)Memory_LockHandle(g_filmWriteBufferHandle);

	if (!g_useHardware3D) {
		g_flightTinyFontHandle = Memory_AllocHandle("TINYFONT", 0x8728u);
		if (g_flightTinyFontHandle == 0) {
			allocFailed = 1;
		}
		g_flightSmallFontHandle = Memory_AllocHandle("SMALLFONT", 0x8728u);
		if (g_flightSmallFontHandle == 0) {
			allocFailed = 1;
		}
	}

	g_unusedFlightResourceInitZero = 0;
	if (g_flightTinyFontHandle != 0) {
		g_flightFontSmallSw = (uint8_t*)Memory_LockHandle(g_flightTinyFontHandle);
	} else {
		g_flightFontSmallSw = NULL;
	}
	if (g_flightSmallFontHandle != 0) {
		g_flightFontMediumSw = (uint8_t*)Memory_LockHandle(g_flightSmallFontHandle);
	} else {
		g_flightFontMediumSw = NULL;
	}

	if (!g_useHardware3D) {
#ifdef XWA_MODERN
		FlightResources_LoadFileIntoBuffer("MICRO48.FNT", g_flightFontSmallSw);
		FlightResources_LoadFileIntoBuffer("MICRO64.FNT", g_flightFontMediumSw);
#else
		uint8_t* dst;
		FILE* stream;
		size_t bytesRead;
		int totalRead;

		dst = g_flightFontSmallSw;
		File_OpenGlobalStream("MICRO48.FNT", "rb", 1, 0);
		stream = (FILE*)g_stream;
		savedStream = stream;
		if (stream == NULL) {
			FeDiskIo_FatalError(1);
		}
		totalRead = 0;
		bytesRead = 0x200;
		do {
			bytesRead = fread(buffer, 1u, (uint16_t)bytesRead, stream);
			if ((uint16_t)bytesRead > 0) {
				memcpy(dst, buffer, (uint16_t)bytesRead);
				dst += (uint16_t)bytesRead;
				stream = savedStream;
			}
			totalRead += bytesRead;
		} while ((uint16_t)bytesRead == 0x200);
		if ((((FILE*)g_stream)->_flag & 0x20) == 0) {
			fclose((FILE*)g_stream);
		}

		dst = g_flightFontMediumSw;
		File_OpenGlobalStream("MICRO64.FNT", "rb", 1, 0);
		stream = (FILE*)g_stream;
		savedStream = stream;
		if (stream == NULL) {
			FeDiskIo_FatalError(1);
		}
		totalRead = 0;
		bytesRead = 0x200;
		do {
			bytesRead = fread(buffer, 1u, (uint16_t)bytesRead, stream);
			if ((uint16_t)bytesRead > 0) {
				memcpy(dst, buffer, (uint16_t)bytesRead);
				dst += (uint16_t)bytesRead;
				stream = savedStream;
			}
			totalRead += bytesRead;
		} while ((uint16_t)bytesRead == 0x200);
		if ((((FILE*)g_stream)->_flag & 0x20) == 0) {
			fclose((FILE*)g_stream);
		}
#endif
	}

	if (allocFailed) {
		FeDiskIo_FatalError(0);
	}

	if (!g_useHardware3D) {
		g_flightLog1BufferHandle = Memory_AllocHandle(
			"LOG1BUFFER", (size_t)(g_flight16bppBytesPerPixel * g_screenHeight * g_screenWidth));
		if (g_flightLog1BufferHandle == 0) {
			allocFailed = 1;
		} else {
			g_flightRetryPromptSaveBuffer = Memory_LockHandle(g_flightLog1BufferHandle);
			memset(g_flightRetryPromptSaveBuffer, 0x40,
				   (size_t)(g_flight16bppBytesPerPixel * g_screenHeight * g_screenWidth));
			FlightSw_SetRotatedSpriteDestBuffer(g_flightRetryPromptSaveBuffer);
		}
	}

	g_mapRoomIconsHandle = Memory_AllocHandle(
		"MAPROOMICONS", MAP_ROOM_ICON_TABLE_ENTRIES * sizeof(uint8_t*) + MAP_ROOM_ICON_DATA_BYTES);
	if (g_mapRoomIconsHandle == 0) {
		allocFailed = 1;
	} else {
#ifdef XWA_MODERN
		XwaFile* stream;

		g_mapRoomIconsBuffer = (uint8_t*)Memory_LockHandle(g_mapRoomIconsHandle);
		g_mapRoomIconsResourcePath = "RESOURCE\\icons640.ico";
		stream = File_Open(AERON_VFS_ROOT_ASSET, g_mapRoomIconsResourcePath, "rb");
		g_stream = stream;
		if (stream != NULL) {
			uint8_t** iconTable;
			uint8_t* iconData;
			uint8_t* iconDataEnd;
			int iconCount;
			int done;

			iconTable = (uint8_t**)g_mapRoomIconsBuffer;
			iconData = g_mapRoomIconsBuffer + MAP_ROOM_ICON_TABLE_ENTRIES * sizeof(uint8_t*);
			iconDataEnd = iconData + MAP_ROOM_ICON_DATA_BYTES;
			iconCount = 0;
			done = 0;
			do {
				uint8_t ch = 0;

				iconTable[iconCount] = iconData;
				while (File_ReadByte(stream, &ch)) {
					/* Leave room for the 0xff terminator written below. */
					if (ch == 0xffu || iconData + 1 >= iconDataEnd) {
						break;
					}
					*iconData++ = ch;
				}
				if (ch != 0xffu) {
					done = 1;
				}
				*iconData++ = 0xffu;
				++iconCount;
			} while (!done && iconCount < MAP_ROOM_ICON_TABLE_ENTRIES && iconData + 1 < iconDataEnd);

			File_Close(stream);
			g_stream = NULL;
			g_unusedMapRoomIconCount = iconCount;
		} else {
			g_stream = NULL;
			g_unusedMapRoomIconCount = 0;
		}
#else
		uint8_t* iconTableBase;
		uint8_t* iconData;
		FILE* stream;
		int openResult;
		int16_t ch;
		int16_t iconCount;

		g_mapRoomIconsBuffer = (uint8_t*)Memory_LockHandle(g_mapRoomIconsHandle);
		g_mapRoomIconsResourcePath = "RESOURCE\\icons640.ico";
		iconTableBase = g_mapRoomIconsBuffer;
		iconData = g_mapRoomIconsBuffer + 0x5708;
		openResult = File_OpenGlobalStream("RESOURCE\\icons640.ico", "rb", 1, 0);
		if (openResult) {
			stream = (FILE*)g_stream;
			iconCount = 0;
			if ((stream->_flag & 0x10) == 0) {
				do {
					((uint8_t**)iconTableBase)[iconCount] = iconData;
					for (ch = (int16_t)fgetc(stream); (stream->_flag & 0x10) == 0;
						 ch = (int16_t)fgetc(stream)) {
						if (ch == 0xff) {
							break;
						}
						*iconData++ = (uint8_t)ch;
					}
					*iconData++ = 0xffu;
					++iconCount;
				} while ((stream->_flag & 0x10) == 0);
				stream = (FILE*)g_stream;
			}
			if ((stream->_flag & 0x20) == 0) {
				fclose(stream);
			}
			openResult = iconCount;
		}
		g_unusedMapRoomIconCount = openResult;
#endif
	}

	g_messageLogHandle = Memory_AllocHandle("MESSAGELOG", 0x7d00u);
	if (g_messageLogHandle == 0) {
		allocFailed = 1;
	}
	g_visibleObjectsHandle = Memory_AllocHandle(
		"VISIBLEOBJECTS", (size_t)RENDER_OBJECT_LIST_CAPACITY * sizeof(RenderObjectListEntry));
	if (g_visibleObjectsHandle == 0) {
		allocFailed = 1;
	} else {
		g_renderObjectListEntries = (RenderObjectListEntry*)Memory_LockHandle(g_visibleObjectsHandle);
	}

	if (allocFailed) {
		FeDiskIo_FatalError(0);
	}
}

// FUNCTION: XWA 0x430E90
void FeDiskIo_FreeGlobalBuffers(void) {
	if (g_filmWriteBufferHandle != 0) {
		Memory_UnlockHandle(g_filmWriteBufferHandle);
		Memory_FreeHandle("FILMBUFFER", g_filmWriteBufferHandle);
		g_filmWriteBufferHandle = 0;
		g_filmWriteBuffer = NULL;
	}

	if (g_flightTinyFontHandle != 0) {
		Memory_UnlockHandle(g_flightTinyFontHandle);
		Memory_FreeHandle("TINYFONT", g_flightTinyFontHandle);
		g_flightTinyFontHandle = 0;
	}

	if (g_flightMicroFontHandle != 0) {
		Memory_UnlockHandle(g_flightMicroFontHandle);
		Memory_FreeHandle("MICROFONT", g_flightMicroFontHandle);
		g_flightMicroFontHandle = 0;
	}

	if (g_flightSmallFontHandle != 0) {
		Memory_UnlockHandle(g_flightSmallFontHandle);
		Memory_FreeHandle("SMALLFONT", g_flightSmallFontHandle);
		g_flightSmallFontHandle = 0;
	}

	if (g_flightLog1BufferHandle != 0) {
		Memory_UnlockHandle(g_flightLog1BufferHandle);
		Memory_FreeHandle("LOG1BUFFER", g_flightLog1BufferHandle);
		g_flightLog1BufferHandle = 0;
	}

	if (g_mapRoomIconsHandle != 0) {
		Memory_UnlockHandle(g_mapRoomIconsHandle);
		Memory_FreeHandle("MAPROOMICONS", g_mapRoomIconsHandle);
		g_mapRoomIconsHandle = 0;
	}

	if (g_messageLogHandle != 0) {
		Memory_UnlockHandle(g_messageLogHandle);
		Memory_FreeHandle("MESSAGELOG", g_messageLogHandle);
		g_messageLogHandle = 0;
	}

	if (g_visibleObjectsHandle != 0) {
		Memory_UnlockHandle(g_visibleObjectsHandle);
		Memory_FreeHandle("VISIBLEOBJECTS", g_visibleObjectsHandle);
		g_visibleObjectsHandle = 0;
	}
}

// FUNCTION: XWA 0x431020
void FeDiskIo_FreeModelResources(void) {
	int modelType;
	uint16_t textureModelType;
	uint8_t missionType;

	Mission_FreeObjectStorageHandles();
	fsfx_UnloadAllEffects_Thunk();
	FeDiskIo_FreeGlobalBuffers();

	for (modelType = 0; modelType < XWA_LOADED_MODEL_COUNT; ++modelType) {
		if (g_loadedModels.byObjectType[modelType] != 0) {
			int duplicateModelType;

			for (duplicateModelType = modelType + 1; duplicateModelType < XWA_LOADED_MODEL_COUNT;
				 ++duplicateModelType) {
				if (g_loadedModels.byObjectType[duplicateModelType] ==
					g_loadedModels.byObjectType[modelType]) {
					g_modelTypeTable[duplicateModelType].curTexLevel = NULL;
					g_loadedModels.byObjectType[duplicateModelType] = 0;
					g_modelTypeTable[duplicateModelType].texLevels = NULL;
				}
			}

			OptModel_FreeHandle(g_loadedModels.byObjectType[modelType]);
			g_modelTypeTable[modelType].curTexLevel = NULL;
			g_loadedModels.byObjectType[modelType] = 0;
#ifdef XWA_MODERN
			/* The original writes g_modelTypeTable[XWA_LOADED_MODEL_COUNT].texLevels
			 * here because duplicateModelType has reached the loop bound. That landed
			 * in linker padding in the 32-bit image; omit the non-semantic overflow in
			 * the portable build without changing any valid model-table state. */
#else
			g_modelTypeTable[duplicateModelType].texLevels = NULL;
#endif
		}

		if (g_modelFloatHardpointDataHandles[modelType] != 0) {
			Memory_FreeHandle("FLOATHARDPTS", g_modelFloatHardpointDataHandles[modelType]);
			g_modelFloatHardpointDataHandles[modelType] = 0;
		}
	}

	missionType = g_missionHeader.body.missionType;
	for (textureModelType = 0; textureModelType < OBJ_Count; ++textureModelType) {
		uint8_t assetFlags;

		if ((g_modelTypeTable[textureModelType].recordFlags & MODEL_TYPE_RECORD_TEXTURE_BACKED) == 0) {
			continue;
		}

		assetFlags = g_modelTypeTable[textureModelType].assetFlags;
		if ((assetFlags & MODEL_TYPE_ASSET_TEXTURE_UNLOAD_CLASS_MASK) != 0 &&
			((assetFlags & MODEL_TYPE_ASSET_SPECIAL_MODE_ONLY) == 0 || g_provingGroundsModeActive) &&
			(assetFlags & MODEL_TYPE_ASSET_TEXTURE_READY) != 0 &&
			((assetFlags & MODEL_TYPE_ASSET_DEATH_STAR_ONLY) == 0 ||
			 missionType == XWA_MISSION_TYPE_DEATH_STAR) &&
			((assetFlags & MODEL_TYPE_ASSET_NOT_DEATH_STAR) == 0 ||
			 missionType != XWA_MISSION_TYPE_DEATH_STAR) &&
			(g_useHardware3D ||
			 (g_modelTypeTable[textureModelType].flags & MODEL_TYPE_FLAG_HARDWARE_ONLY) == 0) &&
			(assetFlags & MODEL_TYPE_ASSET_TEXTURE_DRAW) != 0) {
			FeDiskIo_FreeTexturesForType(textureModelType);
			missionType = g_missionHeader.body.missionType;
		}
	}

	if (g_cockpitModel != 0) {
		OptModel_FreeHandle(g_cockpitModel);
		g_cockpitModel = 0;
	}

	if (g_exteriorModel != 0) {
		OptModel_FreeHandle(g_exteriorModel);
		g_exteriorModel = 0;
	}

	Mission_FreeOverrideStringHandles();
}

static __inline void PilotData_AddCommonStats(int statType, int score, int bonusScoreTenths,
											  unsigned int numFlightGroups) {
	uint16_t* flightGroupKills;
	unsigned int fgIdx;
	unsigned int ratingIdx;
	unsigned int aiRatingIdx;

	g_pilotData.objectStats.totalScorePerMT[0] = bonusScoreTenths;
	++g_pilotData.totalMissionsPlayedCount;
	++g_pilotData.factionStatistics[g_pilotData.currentFactionId].totalMissionsPlayedCount;

	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.totalScorePerMT[statType] += score;
	g_pilotData.mainStats.totalScorePerMT[statType] += score;
	++g_pilotData.factionStatistics[g_pilotData.currentFactionId]
		  .stats.totalMissionsPlayedPerMT[g_pilotData.missionDirectoryId];
	++g_pilotData.mainStats.totalMissionsPlayedPerMT[g_pilotData.missionDirectoryId];

	if (numFlightGroups != 0) {
		flightGroupKills = g_players[g_localPlayer].perMissionKills.killsFullOnFlightGroup;
		for (fgIdx = 0; fgIdx < numFlightGroups; ++fgIdx) {
			unsigned int craftType;

			craftType = g_missionFlightGroups[fgIdx].fg.craftType;
			if (craftType >= 512) {
				++flightGroupKills;
				continue;
			}
			{
				enum {
					FlightGroupKillStatCount = sizeof(g_players[0].perMissionKills.killsFullOnFlightGroup) /
											   sizeof(g_players[0].perMissionKills.killsFullOnFlightGroup[0])
				};
				uint16_t fullKills;

				fullKills = flightGroupKills[0];
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.totalKillsPerMT[statType] +=
					fullKills;
				g_pilotData.mainStats.totalKillsPerMT[statType] += fullKills;
				g_pilotData.objectStats.totalKillsPerMT[0] += fullKills;
				g_pilotData.factionStatistics[g_pilotData.currentFactionId]
					.stats.killsPerCraftPerMT[statType][craftType] += fullKills;
				g_pilotData.mainStats.killsPerCraftPerMT[statType][craftType] += fullKills;
				g_pilotData.objectStats.killsPerCraftPerMT[0][craftType] += fullKills;
				g_pilotData.factionStatistics[g_pilotData.currentFactionId]
					.stats.killsSharedPerCraftPerMT[statType][craftType] +=
					flightGroupKills[FlightGroupKillStatCount];
				g_pilotData.mainStats.killsSharedPerCraftPerMT[statType][craftType] +=
					flightGroupKills[FlightGroupKillStatCount];
				g_pilotData.objectStats.killsSharedPerCraftPerMT[0][craftType] +=
					flightGroupKills[FlightGroupKillStatCount];
				g_pilotData.factionStatistics[g_pilotData.currentFactionId]
					.stats.killsAssistsPerCraftPerMT[statType][craftType] +=
					flightGroupKills[2 * FlightGroupKillStatCount];
				g_pilotData.mainStats.killsAssistsPerCraftPerMT[statType][craftType] +=
					flightGroupKills[2 * FlightGroupKillStatCount];
				g_pilotData.objectStats.killsAssistsPerCraftPerMT[0][craftType] +=
					flightGroupKills[2 * FlightGroupKillStatCount];
			}
			++flightGroupKills;
		}
	}

	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.totalFriendliesKilledPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.friendliesKilled;
	g_pilotData.mainStats.totalFriendliesKilledPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.friendliesKilled;
	g_pilotData.objectStats.totalFriendliesKilledPerMT[0] +=
		g_players[g_localPlayer].perMissionKills.friendliesKilled;

	for (ratingIdx = 0; ratingIdx < 25; ++ratingIdx) {
		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.killsFullOnPlayerRatingPerMT[statType][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsFullOnPlayerRating[ratingIdx];
		g_pilotData.mainStats.killsFullOnPlayerRatingPerMT[statType][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsFullOnPlayerRating[ratingIdx];
		g_pilotData.objectStats.killsFullOnPlayerRatingPerMT[0][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsFullOnPlayerRating[ratingIdx];
		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.killsSharedOnPlayerRatingPerMT[statType][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsSharedOnPlayerRating[ratingIdx];
		g_pilotData.mainStats.killsSharedOnPlayerRatingPerMT[statType][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsSharedOnPlayerRating[ratingIdx];
		g_pilotData.objectStats.killsSharedOnPlayerRatingPerMT[0][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsSharedOnPlayerRating[ratingIdx];
		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.killsAssistOnPlayerRatingPerMT[statType][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsAssistOnPlayerRating[ratingIdx];
		g_pilotData.mainStats.killsAssistOnPlayerRatingPerMT[statType][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsAssistOnPlayerRating[ratingIdx];
		g_pilotData.objectStats.killsAssistOnPlayerRatingPerMT[0][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsAssistOnPlayerRating[ratingIdx];
		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.killedByPlayerRatingPerMT[statType][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killedByPlayerRating[ratingIdx];
		g_pilotData.mainStats.killedByPlayerRatingPerMT[statType][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killedByPlayerRating[ratingIdx];
		g_pilotData.objectStats.killedByPlayerRatingPerMT[0][ratingIdx] +=
			g_players[g_localPlayer].perMissionKills.killedByPlayerRating[ratingIdx];
	}

	for (aiRatingIdx = 0; aiRatingIdx < 6; ++aiRatingIdx) {
		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.killsFullOnAIRatingPerMT[statType][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsFullOnAiRating[aiRatingIdx];
		g_pilotData.mainStats.killsFullOnAIRatingPerMT[statType][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsFullOnAiRating[aiRatingIdx];
		g_pilotData.objectStats.killsFullOnAIRatingPerMT[0][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsFullOnAiRating[aiRatingIdx];
		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.killsSharedOnAIRatingPerMT[statType][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsSharedOnAiRating[aiRatingIdx];
		g_pilotData.mainStats.killsSharedOnAIRatingPerMT[statType][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsSharedOnAiRating[aiRatingIdx];
		g_pilotData.objectStats.killsSharedOnAIRatingPerMT[0][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsSharedOnAiRating[aiRatingIdx];
		g_pilotData.factionStatistics[g_pilotData.currentFactionId]
			.stats.killsAssistOnAIRatingPerMT[statType][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsAssistOnAiRating[aiRatingIdx];
		g_pilotData.mainStats.killsAssistOnAIRatingPerMT[statType][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsAssistOnAiRating[aiRatingIdx];
		g_pilotData.objectStats.killsAssistOnAIRatingPerMT[0][aiRatingIdx] +=
			g_players[g_localPlayer].perMissionKills.killsAssistOnAiRating[aiRatingIdx];
	}

	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.killedByAIRatingPerMT[statType][0] +=
		g_players[g_localPlayer].perMissionKills.killedByAiRating;
	g_pilotData.mainStats.killedByAIRatingPerMT[statType][0] +=
		g_players[g_localPlayer].perMissionKills.killedByAiRating;
	g_pilotData.objectStats.killedByAIRatingPerMT[0][0] +=
		g_players[g_localPlayer].perMissionKills.killedByAiRating;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.numSpecialInspectedPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.numSpecialInspected;
	g_pilotData.mainStats.numSpecialInspectedPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.numSpecialInspected;
	g_pilotData.objectStats.numSpecialInspectedPerMT[0] +=
		g_players[g_localPlayer].perMissionKills.numSpecialInspected;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.energyFiredPerMT[statType] +=
		g_players[g_localPlayer].missionStats.laserShotsFired;
	g_pilotData.mainStats.energyFiredPerMT[statType] += g_players[g_localPlayer].missionStats.laserShotsFired;
	g_pilotData.objectStats.energyFiredPerMT[0] += g_players[g_localPlayer].missionStats.laserShotsFired;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.energyFiredPerMT[statType] +=
		g_players[g_localPlayer].missionStats.ionShotsFired;
	g_pilotData.mainStats.energyFiredPerMT[statType] += g_players[g_localPlayer].missionStats.ionShotsFired;
	g_pilotData.objectStats.energyFiredPerMT[0] += g_players[g_localPlayer].missionStats.ionShotsFired;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.energyHitsPerMT[statType] +=
		g_players[g_localPlayer].missionStats.laserHitsScored;
	g_pilotData.mainStats.energyHitsPerMT[statType] += g_players[g_localPlayer].missionStats.laserHitsScored;
	g_pilotData.objectStats.energyHitsPerMT[0] += g_players[g_localPlayer].missionStats.laserHitsScored;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.energyHitsPerMT[statType] +=
		g_players[g_localPlayer].missionStats.ionHitsScored;
	g_pilotData.mainStats.energyHitsPerMT[statType] += g_players[g_localPlayer].missionStats.ionHitsScored;
	g_pilotData.objectStats.energyHitsPerMT[0] += g_players[g_localPlayer].missionStats.ionHitsScored;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.warheadsFiredPerMT[statType] +=
		g_players[g_localPlayer].warheadsFired;
	g_pilotData.mainStats.warheadsFiredPerMT[statType] += g_players[g_localPlayer].warheadsFired;
	g_pilotData.objectStats.warheadsFiredPerMT[0] += g_players[g_localPlayer].warheadsFired;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.warheadsHitsPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.warheadHits;
	g_pilotData.mainStats.warheadsHitsPerMT[statType] += g_players[g_localPlayer].perMissionKills.warheadHits;
	g_pilotData.objectStats.warheadsHitsPerMT[0] += g_players[g_localPlayer].perMissionKills.warheadHits;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.totalCraftLossesPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.totalCraftLosses;
	g_pilotData.mainStats.totalCraftLossesPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.totalCraftLosses;
	g_pilotData.objectStats.totalCraftLossesPerMT[0] +=
		g_players[g_localPlayer].perMissionKills.totalCraftLosses;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.lossesByCollisionsPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.lossesByCollisions;
	g_pilotData.mainStats.lossesByCollisionsPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.lossesByCollisions;
	g_pilotData.objectStats.lossesByCollisionsPerMT[0] +=
		g_players[g_localPlayer].perMissionKills.lossesByCollisions;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.lossesByStarshipsPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.lossesByStarships;
	g_pilotData.mainStats.lossesByStarshipsPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.lossesByStarships;
	g_pilotData.objectStats.lossesByStarshipsPerMT[0] +=
		g_players[g_localPlayer].perMissionKills.lossesByStarships;
	g_pilotData.factionStatistics[g_pilotData.currentFactionId].stats.lossesByMinesPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.lossesByMines;
	g_pilotData.mainStats.lossesByMinesPerMT[statType] +=
		g_players[g_localPlayer].perMissionKills.lossesByMines;
	g_pilotData.objectStats.lossesByMinesPerMT[0] += g_players[g_localPlayer].perMissionKills.lossesByMines;

	g_pilotData.factionStatistics[g_pilotData.currentFactionId].totalScore += score;
	g_pilotData.totalScore += score;
	g_pilotData.missionScore = score;
	sprintf(Buffer, "Promo points: %d    Worse Promo points: %d\n",
			g_players[g_localPlayer].missionStats.ratingPromoPoints,
			g_players[g_localPlayer].missionStats.worseRatingPromoPoints);
}

static __inline void PilotData_UpdatePromotion(unsigned int missionCategory) {
	int currentPromo;
	int threshold;
	unsigned int pilotRating;

	g_pilotData.newPromotion = 0;

	if (missionCategory == 3 || missionCategory == 0 || missionCategory == 1) {
		pilotRating = g_pilotData.pilotRating;
		if (pilotRating < (g_missionHeader.body.missionType == XWA_MISSION_TYPE_SIMULATOR_2 ? 24 : 7)) {
			currentPromo = g_pilotData.currentRatingPromoPoints +
						   g_players[g_localPlayer].missionStats.ratingPromoPoints;
			threshold = g_pilotRatingPromotionPointThresholds[pilotRating];
			g_pilotData.currentRatingPromoPoints = currentPromo;
			if (g_pilotData.currentRatingWorsePromoPoints < threshold / 2) {
				currentPromo += g_players[g_localPlayer].missionStats.worseRatingPromoPoints;
				g_pilotData.currentRatingWorsePromoPoints +=
					g_players[g_localPlayer].missionStats.worseRatingPromoPoints;
				g_pilotData.currentRatingPromoPoints = currentPromo;
			}
			if (currentPromo >= threshold) {
				g_pilotData.currentRatingPromoPoints = 0;
				g_pilotData.currentRatingWorsePromoPoints = 0;
				++g_pilotData.pilotRating;
				g_pilotData.newPromotion = 1;
				g_pilotData.totalMissionsPlayedCountPerRating[g_pilotData.pilotRating] =
					g_pilotData.totalMissionsPlayedCount;
				currentPromo = g_pilotData.currentRatingPromoPoints;
				pilotRating = g_pilotData.pilotRating;
			}

			if (currentPromo >= 0) {
				g_pilotData.nextPromotionPercent =
					100 * currentPromo / g_pilotRatingPromotionPointThresholds[pilotRating];
				if (g_pilotData.nextPromotionPercent > 100) {
					g_pilotData.nextPromotionPercent = 100;
				}
			} else if (pilotRating != 0) {
				g_pilotData.nextPromotionPercent = 100 * currentPromo / 2000;
				if (g_pilotData.nextPromotionPercent < -100) {
					g_pilotData.nextPromotionPercent = -100;
				}
			} else {
				currentPromo = 0;
				g_pilotData.currentRatingWorsePromoPoints = 0;
				g_pilotData.currentRatingPromoPoints = 0;
				g_pilotData.nextPromotionPercent = 0;
			}
		} else {
			currentPromo = g_pilotData.currentRatingPromoPoints;
			if (g_players[g_localPlayer].missionStats.ratingPromoPoints < 0) {
				currentPromo += g_players[g_localPlayer].missionStats.ratingPromoPoints;
				g_pilotData.currentRatingPromoPoints = currentPromo;
			}
		}

		if (pilotRating != 0 && currentPromo < -2000) {
			pilotRating = g_pilotData.pilotRating - 1;
			g_pilotData.currentRatingPromoPoints = 0;
			g_pilotData.currentRatingWorsePromoPoints = 0;
			g_pilotData.pilotRating = pilotRating;
			g_pilotData.newPromotion = -1;
			g_pilotData.nextPromotionPercent = 0;
			if (pilotRating < 2) {
				g_pilotData.totalMissionsPlayedCountPerRating[pilotRating] =
					g_pilotData.totalMissionsPlayedCount;
			}
		}
	} else if (missionCategory == 4 || missionCategory == 5 || missionCategory == 6) {
		pilotRating = g_pilotData.pilotRating;
		if (pilotRating < 24) {
			currentPromo = g_pilotData.currentRatingPromoPoints +
						   g_players[g_localPlayer].missionStats.ratingPromoPoints;
			threshold = g_pilotRatingPromotionPointThresholds[pilotRating];
			g_pilotData.currentRatingPromoPoints = currentPromo;
			if (g_pilotData.currentRatingWorsePromoPoints < threshold / 2) {
				currentPromo += g_players[g_localPlayer].missionStats.worseRatingPromoPoints;
				g_pilotData.currentRatingWorsePromoPoints +=
					g_players[g_localPlayer].missionStats.worseRatingPromoPoints;
				g_pilotData.currentRatingPromoPoints = currentPromo;
			}
			if (currentPromo >= threshold) {
				g_pilotData.currentRatingPromoPoints = currentPromo - threshold;
				g_pilotData.currentRatingWorsePromoPoints = 0;
				++g_pilotData.pilotRating;
				g_pilotData.newPromotion = 1;
				g_pilotData.totalMissionsPlayedCountPerRating[g_pilotData.pilotRating] =
					g_pilotData.totalMissionsPlayedCount;
				currentPromo = g_pilotData.currentRatingPromoPoints;
				pilotRating = g_pilotData.pilotRating;
			}

			if (currentPromo >= 0) {
				g_pilotData.nextPromotionPercent =
					100 * currentPromo / g_pilotRatingPromotionPointThresholds[pilotRating];
				if (g_pilotData.nextPromotionPercent > 100) {
					g_pilotData.nextPromotionPercent = 100;
				}
			} else if (pilotRating != 0) {
				g_pilotData.nextPromotionPercent = 100 * currentPromo / 2000;
				if (g_pilotData.nextPromotionPercent < -100) {
					g_pilotData.nextPromotionPercent = -100;
				}
			} else {
				currentPromo = 0;
				g_pilotData.currentRatingWorsePromoPoints = 0;
				g_pilotData.currentRatingPromoPoints = 0;
				g_pilotData.nextPromotionPercent = 0;
			}
		} else {
			currentPromo = g_pilotData.currentRatingPromoPoints;
			if (g_players[g_localPlayer].missionStats.ratingPromoPoints < 0) {
				currentPromo += g_players[g_localPlayer].missionStats.ratingPromoPoints;
				g_pilotData.currentRatingPromoPoints = currentPromo;
			}
		}

		if (pilotRating != 0 && currentPromo < -2000) {
			pilotRating = g_pilotData.pilotRating - 1;
			g_pilotData.currentRatingPromoPoints = 0;
			g_pilotData.currentRatingWorsePromoPoints = 0;
			g_pilotData.pilotRating = pilotRating;
			g_pilotData.newPromotion = -1;
			g_pilotData.nextPromotionPercent = 0;
			if (pilotRating < 2) {
				g_pilotData.totalMissionsPlayedCountPerRating[pilotRating] =
					g_pilotData.totalMissionsPlayedCount;
			}
		}
	}
}

static __inline void PilotData_SetAward(PilotMission* mission, unsigned int award, int awardSet) {
	unsigned int oldAward;

	oldAward = mission->awardId;
	if (award == 0) {
		if (oldAward == 6) {
			mission->awardId = 0;
			if (awardSet == 1) {
				if (g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[5] != 0) {
					--g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[5];
				}
			} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[5] != 0) {
				--g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[5];
			}
		}
		return;
	}
	if (oldAward != 0 && award >= oldAward) {
		return;
	}
	if (oldAward != 0) {
		if (awardSet == 1) {
			if (g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[oldAward - 1] != 0) {
				--g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[oldAward - 1];
			}
		} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[oldAward - 1] !=
				   0) {
			--g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[oldAward - 1];
		}
	}
	mission->awardId = award;
	if (awardSet == 1) {
		++g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[award - 1];
	} else {
		++g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[award - 1];
	}
}

static __inline void PilotData_SetMultiplayerAward(PilotMission* mission, unsigned int award, int awardSet) {
	unsigned int oldAward;

	oldAward = mission->awardId;
	if (award != 0) {
		if (award < oldAward || oldAward == 0) {
			if (oldAward == 6) {
				if (awardSet == 1) {
					if (g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[5] != 0) {
						--g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[5];
					}
				} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[5] != 0) {
					--g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[5];
				}
			}
			mission->awardId = award;
		}
		if (award != 6 || mission->awardId == 0) {
			if (awardSet == 1) {
				++g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[award - 1];
			} else {
				++g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[award - 1];
			}
		}
	} else if (oldAward == 6) {
		mission->awardId = 0;
		if (awardSet == 1) {
			if (g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[5] != 0) {
				--g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount1[5];
			}
		} else if (g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[5] != 0) {
			--g_pilotData.factionStatistics[g_pilotData.currentFactionId].awardsCount2[5];
		}
	}
}

// FUNCTION: XWA 0x42E750
int16_t FeDiskIo_CommitFlightResults(void) {
	uint8_t activeTeamFgCount[10];
	unsigned int connectedHumans;
	unsigned int activeTeamCount;
	unsigned int numFlightGroups;
	int campaignMode;
	unsigned int missionCategory;
	int statType;
	int score;
	int bonusScoreTenths;
	int localPlayerIff;
	int campaignModeZero;
	int award;
	int placement;
	int margin;
	uint8_t status1;
	uint8_t missionType;
	unsigned idx;
	int i;

	g_pilotData.tacOfficerAnnounceNewRank = 0;
	g_pilotData.familyNewMedal = 0;
	connectedHumans = 0;
	for (idx = 0; idx < XWA_PLAYER_COUNT; ++idx) {
		if (g_players[idx].network.directPlayId != 0) {
			++connectedHumans;
		}
	}

	memset(activeTeamFgCount, 0, sizeof(activeTeamFgCount));
	numFlightGroups = g_missionHeader.numFlightGroups;
	for (idx = 0; idx < numFlightGroups; ++idx) {
		if (g_missionFlightGroups[idx].fg.playerNumber != 0 && g_missionFgStats[idx].hasArrived) {
			++activeTeamFgCount[g_missionFlightGroups[idx].fg.team];
		}
	}

	activeTeamCount = 0;
	for (idx = 0; idx < 10; ++idx) {
		if (activeTeamFgCount[idx] != 0) {
			++activeTeamCount;
		}
	}

	campaignMode = g_pilotData.campaignMode;
	campaignModeZero = campaignMode == 0;
	missionType = g_missionHeader.body.missionType;
	if (missionType == XWA_MISSION_TYPE_SIMULATOR_1) {
		return 0;
	}
	if (missionType == XWA_MISSION_TYPE_JUNKYARD || missionType == XWA_MISSION_TYPE_SIMULATOR_2) {
		missionCategory = 3;
		statType = 2;
	} else if (missionType == XWA_MISSION_TYPE_ALLIANCE_CAMPAIGN ||
			   missionType == XWA_MISSION_TYPE_DEATH_STAR) {
		missionCategory = 0;
		statType = campaignMode != 0 ? 0 : 2;
	} else if (missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
		missionCategory = 1;
		statType = campaignMode != 0 ? 1 : 2;
	} else if (missionType == XWA_MISSION_TYPE_QUICK_START) {
		missionCategory = 4;
		statType = 2;
	} else if (missionType == XWA_MISSION_TYPE_SKIRMISH) {
		missionCategory = 6;
		statType = 2;
	} else {
		int imperialPlayers;
		int rebelPlayers;
		unsigned fgIdx;

		missionCategory = 5;
		statType = 2;
		imperialPlayers = 0;
		rebelPlayers = 0;
		for (fgIdx = 0; fgIdx < (unsigned)numFlightGroups; ++fgIdx) {
			if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 &&
				g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
				if (g_missionFlightGroups[fgIdx].fg.team == 0) {
					++rebelPlayers;
				} else if (g_missionFlightGroups[fgIdx].fg.team == 1) {
					++imperialPlayers;
				}
			}
		}
		if (connectedHumans == 1) {
			if (imperialPlayers == 0) {
				if (g_missionFlightRuntimeState.teamGoalStatus[0][TEAM_GOAL_PRIMARY] != 1) {
					g_missionFlightRuntimeState.teamGoalStatus[0][TEAM_GOAL_SECONDARY] = 1;
					g_missionFlightRuntimeState.teamGoalStatus[1][TEAM_GOAL_PRIMARY] = 1;
				}
			} else if (rebelPlayers == 0 &&
					   g_missionFlightRuntimeState.teamGoalStatus[1][TEAM_GOAL_PRIMARY] != 1) {
				g_missionFlightRuntimeState.teamGoalStatus[1][TEAM_GOAL_SECONDARY] = 1;
				g_missionFlightRuntimeState.teamGoalStatus[0][TEAM_GOAL_PRIMARY] = 1;
			}
		}
	}

	memset(&g_pilotData.objectStats, 0, sizeof(g_pilotData.objectStats));

	localPlayerIff = (uint16_t)g_players[g_localPlayer].playerIff;
	score = g_players[g_localPlayer].missionStats.missionScore;
	bonusScoreTenths = g_players[g_localPlayer].missionStats.missionBonusScoreTenths +
					   g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][localPlayerIff];
	if (bonusScoreTenths < 0) {
		bonusScoreTenths = 0;
	}

	status1 = g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1;
	if (status1 == 20 ||
		g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status2 == 20) {
		score = score / 20;
		bonusScoreTenths = 0;
	} else if (status1 == 21) {
		score = score / 10;
		bonusScoreTenths = 0;
	}

	PilotData_AddCommonStats(statType, score, bonusScoreTenths, numFlightGroups);
	PilotData_UpdatePromotion(missionCategory);
	{
		unsigned int playerIdx;
		unsigned int fgIdx;
		unsigned int teamIdx;

		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			unsigned int networkIdx;

			if (g_players[playerIdx].network.directPlayId == 0) {
				continue;
			}
			for (networkIdx = 0; networkIdx < XWA_PLAYER_COUNT; ++networkIdx) {
				if (g_pilotData.networkPlayers[networkIdx].directPlayId ==
					g_players[playerIdx].network.directPlayId) {
					g_pilotData.killsFullOnPlayer[networkIdx] =
						g_players[g_localPlayer].perMissionKills.killsFullOnPlayer[playerIdx];
					g_pilotData.killsSharedOnPlayer[networkIdx] =
						g_players[g_localPlayer].perMissionKills.killsSharedOnPlayer[playerIdx];
					g_pilotData.killsFullFromPlayer[networkIdx] =
						g_players[g_localPlayer].perMissionKills.killsFullFromPlayer[playerIdx];
					g_pilotData.killsSharedFromPlayer[networkIdx] =
						g_players[g_localPlayer].perMissionKills.killsSharedFromPlayer[playerIdx];
					break;
				}
			}
		}

		for (fgIdx = 0; fgIdx < numFlightGroups; ++fgIdx) {
			g_pilotData.killsFullOnFlightGroup[fgIdx] =
				g_players[g_localPlayer].perMissionKills.killsFullOnFlightGroup[fgIdx];
			g_pilotData.killsSharedOnFlightGroup[fgIdx] =
				g_players[g_localPlayer].perMissionKills.killsSharedOnFlightGroup[fgIdx];
			g_pilotData.killsFullFromFlightGroup[fgIdx] =
				g_players[g_localPlayer].perMissionKills.killsFullFromFlightGroup[fgIdx];
			g_pilotData.killsSharedFromFlightGroup[fgIdx] =
				g_players[g_localPlayer].perMissionKills.killsSharedFromFlightGroup[fgIdx];
			if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_QUICK_START ||
				g_missionHeader.body.missionType == XWA_MISSION_TYPE_SKIRMISH) {
				int groupAI;
				int rating;

				groupAI = g_missionFlightGroups[fgIdx].fg.groupAI;
				rating = g_pilotKillScoreBaseByAiLevel[groupAI];
				if (groupAI != 0) {
					rating += fgIdx & 3;
				}
				g_pilotData.flightGroupRating[fgIdx] = rating;
			}
		}

		if (missionCategory == 3) {
			for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
				if (g_players[playerIdx].connectedFlag != 0) {
					teamIdx = (uint16_t)g_players[playerIdx].playerIff;
					g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL][teamIdx] =
						(uint16_t)g_yardContext.playerChallengeStates[playerIdx].field_38;
					g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][teamIdx] =
						g_yardContext.playerChallengeStates[playerIdx].score;
				}
			}
		}

		{
			PilotTeam* team;
			uint8_t (*goalStatus)[TEAM_GOAL_KIND_COUNT];
			uint16_t* fullKillCount;
			uint16_t* sharedKillCount;
			uint16_t* assistKillCount;
			int* teamScore;
			int* teamTime;

			team = g_pilotData.teamsStatistics;
			goalStatus = g_missionFlightRuntimeState.teamGoalStatus;
			fullKillCount = g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL];
			sharedKillCount = g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_SHARED];
			assistKillCount = g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_ASSIST];
			teamScore = g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION];
			teamTime = g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds;
			for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
				team->isMissionCompleted =
					(*goalStatus)[TEAM_GOAL_PRIMARY] == 1 && (*goalStatus)[TEAM_GOAL_SECONDARY] != 1;
				team->missionScore = 0;
				team->missionTime = *teamTime;
				team->kills = *fullKillCount;
				team->killsShared = *sharedKillCount;
				team->killsAssist = *assistKillCount;
				if (missionCategory == 4 || missionCategory == 6 || missionCategory == 3) {
					team->missionScore += *teamScore;
				} else {
					for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
						if (g_players[playerIdx].network.directPlayId != 0 &&
							(uint16_t)g_players[playerIdx].playerIff == teamIdx) {
							team->missionScore += g_players[playerIdx].missionStats.missionScore;
						}
					}
				}
				++team;
				++goalStatus;
				++fullKillCount;
				++sharedKillCount;
				++assistKillCount;
				++teamScore;
				++teamTime;
			}
		}

		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			unsigned int networkIdx;

			if (g_players[playerIdx].network.directPlayId == 0) {
				continue;
			}
			for (networkIdx = 0; networkIdx < XWA_PLAYER_COUNT; ++networkIdx) {
				if (g_pilotData.networkPlayers[networkIdx].directPlayId ==
					g_players[playerIdx].network.directPlayId) {
					for (fgIdx = 0; fgIdx < numFlightGroups; ++fgIdx) {
						g_pilotData.networkPlayers[networkIdx].kills +=
							g_players[playerIdx].perMissionKills.killsFullOnFlightGroup[fgIdx];
						g_pilotData.networkPlayers[networkIdx].killsShared +=
							g_players[playerIdx].perMissionKills.killsSharedOnFlightGroup[fgIdx];
						g_pilotData.networkPlayers[networkIdx].killsAssist +=
							g_players[playerIdx].perMissionKills.killsAssistOnFlightGroup[fgIdx];
					}
					g_pilotData.networkPlayers[networkIdx].totalScore =
						g_players[playerIdx].missionStats.missionScore +
						g_players[playerIdx].missionStats.missionBonusScoreTenths +
						g_missionFlightRuntimeState
							.teamScores[TEAM_SCORE_BONUS_TENTHS][(uint16_t)g_players[playerIdx].playerIff];
					g_pilotData.networkPlayers[networkIdx].totalLosses =
						g_players[playerIdx].perMissionKills.totalCraftLosses;
					break;
				}
			}
		}
	}

	for (i = 0; i < 4; ++i) {
		g_pilotData.factionStatistics[g_pilotData.currentFactionId].kalidorCrescentPoints[i] = 0;
	}

	placement = 0;
	margin = 0;
	if (missionCategory == 3 || missionCategory < 2) {
		if (statType != 2) {
			award = 0;
		} else {
			int playerTeam;

			playerTeam = (uint16_t)g_players[g_localPlayer].playerIff;
			if (g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_PRIMARY] != 1 ||
				g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_SECONDARY] == 1) {
				award = score > 0 ? 0 : 6;
			} else {
				const int* threshold;
				int remaining;

				award = 1;
				remaining = 3;
				if (score < g_missionAwardScoreMarginThresholds[0]) {
					threshold = g_missionAwardScoreMarginThresholds;
					do {
						++award;
						++threshold;
						--remaining;
					} while (remaining != 0 && score < *threshold);
				}
				if (g_flightDifficulty == 1) {
					++award;
				} else if (g_flightDifficulty == 0) {
					award += 2;
				}
				if (g_playerFlightGroupWaveMode == 2 && award < 5) {
					award = 5;
				}
				if (award >= 6) {
					award = 0;
				}
			}
		}
		placement = campaignModeZero;
		if (award != 0) {
			g_pilotData.factionStatistics[g_pilotData.currentFactionId].kalidorCrescentPoints[2] = award;
		}
	} else if (missionCategory == 4 || missionCategory == 6) {
		int playerTeam;
		int playerScore;
		int opponentCount;
		int place;
		int teamIdx;

		playerTeam = (uint16_t)g_players[g_localPlayer].playerIff;
		playerScore = g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][playerTeam] +
					  g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][playerTeam];
		opponentCount = 0;
		for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
			int hasOpponent;
			int otherScore;
			int fgIdx;

			if (teamIdx == playerTeam) {
				continue;
			}
			hasOpponent = 0;
			for (fgIdx = 0; fgIdx < numFlightGroups; ++fgIdx) {
				if (g_missionFlightGroups[fgIdx].fg.team == teamIdx &&
					(g_missionFlightGroups[fgIdx].playerOwnerIdx != -1 ||
					 (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 && g_aiOpponentsEnabled))) {
					hasOpponent = 1;
				}
			}
			if (!hasOpponent) {
				continue;
			}
			otherScore = g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][teamIdx] +
						 g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx];
			if (otherScore > playerScore) {
				++opponentCount;
			} else if (playerScore - otherScore < margin || margin == 0) {
				margin = playerScore - otherScore;
			}
		}

		place = opponentCount + 1;
		award = 0;
		if (activeTeamCount <= 1 || connectedHumans <= 1) {
			if (g_flightDifficulty != 0) {
				if (g_flightDifficulty == 1) {
					if (opponentCount == 0) {
						if (score <= 0) {
							award = 5;
						} else if (margin > 10000) {
							award = 2;
						} else if (margin > 5000) {
							award = 3;
						} else {
							award = 4;
						}
					} else if (opponentCount == 1 && activeTeamCount > 2) {
						award = 5;
					} else if (place > activeTeamCount || place > 6) {
						award = 6;
					}
				} else if (g_flightDifficulty == 2) {
					if (opponentCount == 0) {
						if (score <= 0) {
							award = 5;
						} else if (margin > 10000) {
							award = 1;
						} else if (margin > 5000) {
							award = 2;
						} else {
							award = 3;
						}
					} else if (opponentCount == 1) {
						if (activeTeamCount > 4) {
							award = 4;
						} else if (activeTeamCount > 2) {
							award = 5;
						}
					} else if (opponentCount == 2 && activeTeamCount > 4) {
						award = 5;
					} else if (place > 7) {
						award = 6;
					}
				}
			} else {
				if (opponentCount == 0) {
					award = 5;
				} else if (place > 4) {
					award = 6;
				}
			}
		} else if (place == activeTeamCount && activeTeamCount >= 4) {
			award = 6;
		} else if (score > 5000 && place < 4) {
			award = g_kalidorCrescentAwardByPerformanceTable[3 * activeTeamCount + place];
			if (award != 0 && award != 6) {
				if (score < 1250) {
					award += 2;
				} else if (score < 2500) {
					++award;
				}
				if (award > 5) {
					award = 5;
				}
			}
			if (opponentCount == 0 && award > 0 && award != 6) {
				if (margin > 15000) {
					award -= 3;
				} else if (margin > 10000) {
					award -= 2;
				} else if (margin > 5000) {
					--award;
				}
				if (award < 1) {
					award = 1;
				}
			}
		}
		placement = place;
		if (award != 0) {
			g_pilotData.factionStatistics[g_pilotData.currentFactionId].kalidorCrescentPoints[0] = award;
		}
		sprintf(Buffer, "Player's team score: %d   Place: %d   Margin: %d   Award: %d\n", playerScore,
				placement, margin, award);
	} else {
		localPlayerIff = (uint16_t)g_players[g_localPlayer].playerIff;
		if (g_missionFlightRuntimeState.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] == 2 ||
			g_missionFlightRuntimeState.teamGoalStatus[localPlayerIff][TEAM_GOAL_SECONDARY] == 1) {
			award = 6;
		} else if (connectedHumans == 1) {
			if (g_missionFlightRuntimeState.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] != 1) {
				award = score > 0 ? 0 : 6;
			} else {
				const int* threshold;
				int remaining;

				award = 1;
				remaining = 3;
				if (score < g_missionAwardScoreMarginThresholds[0]) {
					threshold = g_missionAwardScoreMarginThresholds;
					do {
						++award;
						++threshold;
						--remaining;
					} while (remaining != 0 && score < *threshold);
				}
				if (g_flightDifficulty == 1) {
					++award;
				} else if (g_flightDifficulty == 0) {
					award += 2;
				}
				if (g_playerFlightGroupWaveMode == 2) {
					award = 5;
				}
				if (award > 5) {
					award = 0;
				}
			}
		} else if (g_missionFlightRuntimeState.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] != 1) {
			award = score < -25000 ? 6 : 0;
		} else {
			const int* threshold;

			award = 1;
			threshold = g_kalidorCrescentScoreLossThresholds;
			do {
				if (score >= *threshold) {
					break;
				}
				++threshold;
				++award;
			} while (threshold <= &g_kalidorCrescentScoreLossThresholds[4]);
			if (g_playerFlightGroupWaveMode == 2) {
				award = 5;
			} else if (award >= 6) {
				award = 0;
			}
		}
		if (award != 0) {
			g_pilotData.factionStatistics[g_pilotData.currentFactionId].kalidorCrescentPoints[2] = award;
		}
		placement = campaignModeZero;
	}

	if (connectedHumans == 1) {
		PilotMission* mission;
		int playerTeam;
		uint8_t primary;

		playerTeam = (uint16_t)g_players[g_localPlayer].playerIff;
		primary = g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_PRIMARY];

		switch (missionCategory) {
			case 0:
				i = g_pilotData.missionDescriptionIds[4] + 255 * campaignModeZero;
				++g_pilotData.tourOfDutyMissions[i].numberTimesFlown;
				if (primary == 1) {
					g_pilotData.tourOfDutyMissions[i].completedCount = 1;
					g_pilotData.factionStatistics[g_pilotData.currentFactionId].score += score;
					if (statType == 0 && (unsigned int)g_pilotData.pilotRank < 8 &&
						(unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId].score >=
							g_pilotRankPromotionScoreThresholds[g_pilotData.pilotRank]) {
						g_pilotData.tacOfficerAnnounceNewRank = ++g_pilotData.pilotRank;
					}
					g_pilotData.factionStatistics[g_pilotData.currentFactionId].bonusScore +=
						bonusScoreTenths;
					if (statType == 0 && (unsigned int)g_pilotData.kalidorCresent < 6 &&
						(unsigned int)g_pilotData.factionStatistics[g_pilotData.currentFactionId]
								.bonusScore >=
							g_kalidorCrescentBonusScoreThresholds[g_pilotData.kalidorCresent]) {
						g_pilotData.familyNewMedal = ++g_pilotData.kalidorCresent;
					}
				}
				if (primary == 2 ||
					g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_SECONDARY] == 1) {
					++g_pilotData.tourOfDutyMissions[i].failedCount;
				}
				if (g_playerFlightGroupWaveMode != 2) {
					if (score > g_pilotData.tourOfDutyMissions[i].bestScore) {
						g_pilotData.tourOfDutyMissions[i].bestScore = score;
					}
					if (bonusScoreTenths > g_pilotData.tourOfDutyMissions[i].bestBonus) {
						g_pilotData.tourOfDutyMissions[i].bestBonus = bonusScoreTenths;
					}
					if (g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] != 0 &&
						(g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] <
							 g_pilotData.tourOfDutyMissions[i].bestTime ||
						 g_pilotData.tourOfDutyMissions[i].bestTime == 0)) {
						g_pilotData.tourOfDutyMissions[i].bestTime =
							g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam];
					}
				}
				PilotData_SetAward(&g_pilotData.tourOfDutyMissions[i], award, 2);
				break;

			case 1:
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].score += score;
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].bonusScore += bonusScoreTenths;
				i = g_pilotData.missionDescriptionIds[4] + 255 * campaignModeZero;
				++g_pilotData.tourOfDutyMissions[i].numberTimesFlown;
				if (primary == 1) {
					g_pilotData.tourOfDutyMissions[i].completedCount = 1;
				}
				if (primary == 2 ||
					g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_SECONDARY] == 1) {
					++g_pilotData.tourOfDutyMissions[i].failedCount;
				}
				if (g_playerFlightGroupWaveMode != 2) {
					if (score > g_pilotData.tourOfDutyMissions[i].bestScore) {
						g_pilotData.tourOfDutyMissions[i].bestScore = score;
					}
					if (bonusScoreTenths > g_pilotData.tourOfDutyMissions[i].bestBonus) {
						g_pilotData.tourOfDutyMissions[i].bestBonus = bonusScoreTenths;
					}
					if (g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] != 0 &&
						(g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] <
							 g_pilotData.tourOfDutyMissions[i].bestTime ||
						 g_pilotData.tourOfDutyMissions[i].bestTime == 0)) {
						g_pilotData.tourOfDutyMissions[i].bestTime =
							g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam];
					}
				}
				PilotData_SetAward(&g_pilotData.tourOfDutyMissions[i], award, 2);
				break;

			case 3:
				i = g_pilotData.missionDescriptionIds[0];
				mission = &g_pilotData.tourOfDutyMissions[i + 255 * campaignModeZero];
				++mission->numberTimesFlown;
				if (primary == 1) {
					++mission->completedCount;
				}
				if (primary == 2 ||
					g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_SECONDARY] == 1) {
					++mission->failedCount;
				}
				if (g_playerFlightGroupWaveMode != 2) {
					if (score > mission->bestScore) {
						mission->bestScore = score;
					}
					if (g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] != 0 &&
						(g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] <
							 g_pilotData.tourOfDutyMissions[i].bestTime ||
						 mission->bestTime == 0)) {
						mission->bestTime =
							g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam];
					}
				}
				PilotData_SetAward(mission, award, 2);
				break;

			case 4:
				i = g_pilotData.missionDescriptionIds[1];
				mission = &g_pilotData.tourOfDutyMissions[i + 255 * campaignModeZero];
				++mission->numberTimesFlown;
				if (primary == 1) {
					++mission->completedCount;
				}
				if (primary == 2 ||
					g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_SECONDARY] == 1) {
					++mission->failedCount;
				}
				if (score > mission->bestScore) {
					mission->bestScore = score;
				}
				if (g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] != 0 &&
					(g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] <
						 g_pilotData.combatChamberMissions[i].bestTime ||
					 mission->bestTime == 0)) {
					mission->bestTime =
						g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam];
				}
				if (placement != 0 && (placement < mission->bestPlacement || mission->bestPlacement == 0)) {
					mission->bestPlacement = placement;
				}
				if (placement == 1 && margin > mission->bestBonus && margin > 0) {
					mission->bestBonus = margin;
				}
				PilotData_SetAward(mission, award, 1);
				break;

			case 5:
				i = g_pilotData.missionDescriptionIds[2];
				mission = &g_pilotData.tourOfDutyMissions[i + 255 * campaignModeZero];
				++mission->numberTimesFlown;
				if (primary == 1) {
					++mission->completedCount;
				}
				if (primary == 2 ||
					g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_SECONDARY] == 1) {
					++mission->failedCount;
				}
				if (g_playerFlightGroupWaveMode != 2) {
					if (score > mission->bestScore) {
						mission->bestScore = score;
					}
					if (g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] != 0 &&
						(g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] <
							 mission->bestTime ||
						 mission->bestTime == 0)) {
						mission->bestTime =
							g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam];
					}
				}
				PilotData_SetAward(mission, award, 2);
				break;

			case 6:
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].score += score;
				g_pilotData.factionStatistics[g_pilotData.currentFactionId].bonusScore += bonusScoreTenths;
				return 0;
		}
	} else if (missionCategory == 3) {
		int playerTeam;
		uint8_t primary;

		playerTeam = (uint16_t)g_players[g_localPlayer].playerIff;
		primary = g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_PRIMARY];
		i = g_pilotData.missionDescriptionIds[0] + 255 * campaignModeZero;
		++g_pilotData.tourOfDutyMissions[i].numberTimesFlown;
		if (primary == 1) {
			++g_pilotData.tourOfDutyMissions[i].completedCount;
		}
		if (primary == 2 ||
			g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_SECONDARY] == 1) {
			++g_pilotData.tourOfDutyMissions[i].failedCount;
		}
		if (g_playerFlightGroupWaveMode != 2) {
			if (score > g_pilotData.tourOfDutyMissions[i].bestScore) {
				g_pilotData.tourOfDutyMissions[i].bestScore = score;
			}
			if (g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] != 0 &&
				(g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] <
					 g_pilotData.tourOfDutyMissions[i].bestTime ||
				 g_pilotData.tourOfDutyMissions[i].bestTime == 0)) {
				g_pilotData.tourOfDutyMissions[i].bestTime =
					g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam];
			}
		}
		PilotData_SetMultiplayerAward(&g_pilotData.tourOfDutyMissions[i], award, 2);
	} else if (missionCategory == 4) {
		PilotMission* mission;
		int playerTeam;
		uint8_t primary;

		playerTeam = (uint16_t)g_players[g_localPlayer].playerIff;
		primary = g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_PRIMARY];
		i = g_pilotData.missionDescriptionIds[1];
		mission = &g_pilotData.tourOfDutyMissions[i + 255 * campaignModeZero];
		++mission->numberTimesFlown;
		if (primary == 1) {
			++mission->completedCount;
		}
		if (primary == 2 ||
			g_missionFlightRuntimeState.teamGoalStatus[playerTeam][TEAM_GOAL_SECONDARY] == 1) {
			++mission->failedCount;
		}
		if (score > mission->bestScore) {
			mission->bestScore = score;
		}
		if (g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] != 0 &&
			(g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam] < mission->bestTime ||
			 mission->bestTime == 0)) {
			mission->bestTime = g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[playerTeam];
		}
		if (placement == 1) {
			++mission->firstPlaceCount;
		}
		if (placement == 2) {
			++mission->secondPlaceCount;
		}
		if (placement == 3) {
			++mission->thirdPlaceCount;
		}
		if (placement != 0 && (placement < mission->bestPlacement || mission->bestPlacement == 0)) {
			mission->bestPlacement = placement;
		}
		if (placement == 1 && margin > mission->bestBonus && margin > 0) {
			mission->bestBonus = margin;
		}
		PilotData_SetMultiplayerAward(mission, award, 1);
	}

	return 0;
}

// FUNCTION: XWA 0x433FF0
int16_t FeDiskIo_CloseGlobalStream(int16_t removeFileOnError) {
	int16_t closeError;

	closeError = 0;
#ifdef XWA_MODERN
	if (g_stream == NULL || File_Close(g_stream) == -1) {
		closeError = 1;
	}
	g_stream = NULL;

	if (removeFileOnError && closeError) {
		File_Remove(g_lastOpenedFileRoot, FileName);
	}
#else
	if ((((FILE*)g_stream)->_flag & 0x20) != 0 || fclose((FILE*)g_stream) == -1) {
		closeError = 1;
	}

	if (removeFileOnError && closeError) {
		remove(FileName);
	}
#endif

	return closeError;
}

// FUNCTION: XWA 0x430910
uint16_t FeDiskIo_ReadAllBytesOrFatal(const char* fileName, void* dst) {
#ifdef XWA_MODERN
	XwaFile* stream;
#else
	FILE* stream;
#endif
	uint8_t* cursor;
	uint8_t buffer[512];
	uint32_t totalRead;
	size_t bytesRead;

#ifdef XWA_MODERN
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	g_stream = stream;
#else
	File_OpenGlobalStream(fileName, "rb", 1, 0);
	stream = (FILE*)g_stream;
#endif
	if (stream == NULL) {
		FeDiskIo_FatalError(1);
#ifdef XWA_MODERN
		return 0;
#endif
	}

	cursor = (uint8_t*)dst;
	totalRead = 0;
	bytesRead = sizeof(buffer);
	do {
#ifdef XWA_MODERN
		bytesRead = File_ReadPartial(stream, buffer, (uint16_t)bytesRead);
#else
		bytesRead = fread(buffer, 1, (uint16_t)bytesRead, stream);
#endif
		if ((uint16_t)bytesRead > 0) {
			memcpy(cursor, buffer, (uint16_t)bytesRead);
			cursor += (uint16_t)bytesRead;
		}
		totalRead += (uint32_t)bytesRead;
	} while ((uint16_t)bytesRead == sizeof(buffer));

#ifdef XWA_MODERN
	File_Close(stream);
	g_stream = NULL;
#else
	if (!ferror((FILE*)g_stream)) {
		fclose((FILE*)g_stream);
	}
#endif
	return (uint16_t)totalRead;
}

// FUNCTION: XWA 0x433850
int FeDiskIo_ShowFatalErrorMessageAndWaitKey(const char* message) {
	int16_t savedCursorX;
	int16_t savedCursorY;
	int16_t savedClipLeft;
	int16_t savedClipTop;
	int16_t savedClipRight;
	int16_t savedClipBottom;
	uint16_t savedWordWrap;
	int16_t savedReservedState;
	uint16_t savedClearLineBg;
	uint8_t savedTextColor;
	uint8_t savedBgColor;
	uint8_t savedShadowColor;
	uint8_t savedShadowEnabled;
	uint8_t savedFontTier;
	int savedLockCount;
	int remainingLocks;
	uint8_t savedDisplaySurfacesActive;
	uint16_t nextKey;
	int currentLockCount;
	int lineHeight;
	char str[256];

	savedCursorX = g_flightCursorX;
	savedCursorY = g_flightCursorY;
	savedClipLeft = g_flightClipLeft;
	savedClipTop = g_flightClipTop;
	savedClipRight = g_flightClipRight;
	savedClipBottom = g_flightClipBottom;
	savedWordWrap = g_flightWordWrapEnabled;
	savedReservedState = g_flightTextReservedState91079E;
	savedClearLineBg = g_flightClearLineBgEnabled;
	savedTextColor = g_flightTextColorIndex;
	savedBgColor = g_flightTextBgColor;
	savedShadowColor = g_flightTextShadowColor;
	savedShadowEnabled = g_flightTextShadowEnabled;
	savedFontTier = g_flightFontTier;

	savedLockCount = FlightSurface_GetLockCount();
	if (savedLockCount > 0) {
		remainingLocks = savedLockCount;
		do {
			FlightSurface_Unlock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}

	savedDisplaySurfacesActive = g_flightDisplaySurfacesActive;
	g_flightDisplaySurfacesActive = 1;
	FlightSurface_Lock();
	FlightText_SetFontTier(1);
	FlightText_SetClipRect(g_screenWidth >> 4, (g_screenHeight >> 1) - 2 * g_flightFontLineHeight,
						   (uint16_t)(g_screenWidth - (g_screenWidth >> 4)),
						   (uint16_t)(2 * g_flightFontLineHeight + (g_screenHeight >> 1)));
	g_flightTextBgColor = 0xf9;
	g_flightFillClipRectFn();
	FlightText_SetClipRect((g_screenWidth >> 4) + 1, (g_screenHeight >> 1) - 2 * g_flightFontLineHeight + 1,
						   (uint16_t)(g_screenWidth - (g_screenWidth >> 4) - 1),
						   (uint16_t)(2 * g_flightFontLineHeight + (g_screenHeight >> 1) - 1));
	g_flightTextBgColor = 0;
	g_flightFillClipRectFn();
	lineHeight = g_flightFontLineHeight;
	g_flightTextColorIndex = 0xf9;
	g_flightTextShadowColor = 0;
	g_flightTextShadowEnabled = 0;
	FlightText_SetCursor(0, (int16_t)((g_screenHeight >> 1) - lineHeight - 2));
	strcpy(str, message);
	FlightText_DrawStringCentered(str);
	FlightText_SetCursor(0, (int16_t)((g_screenHeight >> 1) + 2));
	FlightText_DrawStringCentered(g_strFileErrorMessages[3]);
	FlightSurface_Unlock();
	if (!g_useHardware3D) {
		FlightDisplay_BlitRenderSurface();
	}
	FlightDisplay_Flip();
	nextKey = (uint16_t)FlightInput_GetNextKey();

	g_flightDisplaySurfacesActive = savedDisplaySurfacesActive;
	if (savedLockCount > 0) {
		remainingLocks = savedLockCount;
		do {
			FlightSurface_Lock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}

	FlightText_SetFontTier(savedFontTier & 0xff);
	g_flightCursorX = savedCursorX;
	g_flightCursorY = savedCursorY;
	g_flightClipLeft = savedClipLeft;
	g_flightClipTop = savedClipTop;
	g_flightClipRight = savedClipRight;
	g_flightClipBottom = savedClipBottom;
	g_flightWordWrapEnabled = savedWordWrap;
	g_flightTextReservedState91079E = savedReservedState;
	g_flightClearLineBgEnabled = savedClearLineBg;
	g_flightTextColorIndex = savedTextColor;
	g_flightTextBgColor = savedBgColor;
	g_flightTextShadowColor = savedShadowColor;
	g_flightTextShadowEnabled = savedShadowEnabled;

	currentLockCount = FlightSurface_GetLockCount();
	if (currentLockCount > 0) {
		remainingLocks = currentLockCount;
		do {
			FlightSurface_Unlock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}
	FlightDisplay_BlitRenderSurface();
	FlightDisplay_Flip();
	if (currentLockCount > 0) {
		remainingLocks = currentLockCount;
		do {
			FlightSurface_Lock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}

	return nextKey;
}

// FUNCTION: XWA 0x433C50
int16_t FeDiskIo_ShowRetryFailPrompt(void) {
	int16_t savedCursorY;
	int16_t savedClipLeft;
	int16_t savedClipTop;
	int16_t savedClipRight;
	int16_t savedClipBottom;
	uint16_t savedWordWrap;
	int16_t savedReservedState;
	int16_t savedCursorX;
	uint16_t savedClearLineBg;
	uint8_t savedTextColor;
	uint8_t savedBgColor;
	uint8_t savedShadowColor;
	uint8_t savedShadowEnabled;
	uint8_t savedFontTier;
	int16_t* savedPixels;
	int lockCount;
	int remainingLocks;
	int16_t nextKey;
	int currentLockCount;
	int savedRectHeight;
	char str[256];

	savedCursorY = g_flightCursorY;
	savedClipLeft = g_flightClipLeft;
	savedClipTop = g_flightClipTop;
	savedClipRight = g_flightClipRight;
	savedClipBottom = g_flightClipBottom;
	savedWordWrap = g_flightWordWrapEnabled;
	savedReservedState = g_flightTextReservedState91079E;
	savedCursorX = g_flightCursorX;
	savedClearLineBg = g_flightClearLineBgEnabled;
	savedTextColor = g_flightTextColorIndex;
	savedBgColor = g_flightTextBgColor;
	savedShadowColor = g_flightTextShadowColor;
	savedShadowEnabled = g_flightTextShadowEnabled;
	savedFontTier = g_flightFontTier;

	FlightSurface_Lock();
	FlightText_SetFontTier(1);
	savedRectHeight = 4 * g_flightFontLineHeight + 1;
	savedPixels = (int16_t*)(g_flightRetryPromptSaveBuffer +
							 g_screenWidth * g_flight16bppBytesPerPixel * (g_screenHeight - savedRectHeight));
	g_flightSaveScreenRectFn(savedPixels, 0, (g_screenHeight >> 1) - 2 * g_flightFontLineHeight,
							 (int16_t)g_screenWidth, savedRectHeight);
	FlightText_SetClipRect(g_screenWidth >> 4, (g_screenHeight >> 1) - 2 * g_flightFontLineHeight,
						   (uint16_t)(g_screenWidth - (g_screenWidth >> 4)),
						   (uint16_t)(2 * g_flightFontLineHeight + (g_screenHeight >> 1)));
	g_flightTextBgColor = 0xf9;
	g_flightFillClipRectFn();
	FlightText_SetClipRect((g_screenWidth >> 4) + 1, (g_screenHeight >> 1) - 2 * g_flightFontLineHeight + 1,
						   (uint16_t)(g_screenWidth - (g_screenWidth >> 4) - 1),
						   (uint16_t)(2 * g_flightFontLineHeight + (g_screenHeight >> 1) - 1));
	g_flightTextBgColor = 0;
	g_flightFillClipRectFn();
	g_flightTextColorIndex = 0xf9;
	g_flightTextShadowColor = 0;
	g_flightTextShadowEnabled = 0;
	FlightText_SetCursor(0, (int16_t)((g_screenHeight >> 1) - g_flightFontLineHeight - 2));
	strcpy(str, FileName);
	strcat(str, ": ");
	strcat(str, g_strDiskIoMessages[5]);
	FlightText_DrawStringCentered(str);
	FlightText_SetCursor(0, (int16_t)((g_screenHeight >> 1) + 2));
	FlightText_DrawStringCentered(g_strDiskIoMessages[6]);

	lockCount = FlightSurface_GetLockCount();
	if (lockCount > 0) {
		remainingLocks = lockCount;
		do {
			FlightSurface_Unlock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}
	if (!g_useHardware3D) {
		FlightDisplay_BlitRenderSurface();
	}
	FlightDisplay_Flip();
	nextKey = FlightInput_GetNextKey();

	if (lockCount > 0) {
		remainingLocks = lockCount;
		do {
			FlightSurface_Lock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}
	g_flightRestoreScreenRectFn(savedPixels, 0, (g_screenHeight >> 1) - 2 * g_flightFontLineHeight,
								(int16_t)g_screenWidth, savedRectHeight);
	FlightText_SetFontTier((char)savedFontTier);
	g_flightCursorY = savedCursorY;
	g_flightClipLeft = savedClipLeft;
	g_flightClipTop = savedClipTop;
	g_flightClipRight = savedClipRight;
	g_flightClipBottom = savedClipBottom;
	g_flightWordWrapEnabled = savedWordWrap;
	g_flightTextReservedState91079E = savedReservedState;
	g_flightClearLineBgEnabled = savedClearLineBg;
	g_flightTextColorIndex = savedTextColor;
	g_flightCursorX = savedCursorX;
	g_flightTextBgColor = savedBgColor;
	g_flightTextShadowColor = savedShadowColor;
	g_flightTextShadowEnabled = savedShadowEnabled;
	FlightSurface_Unlock();

	currentLockCount = FlightSurface_GetLockCount();
	if (currentLockCount > 0) {
		remainingLocks = currentLockCount;
		do {
			FlightSurface_Unlock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}
	FlightDisplay_BlitRenderSurface();
	FlightDisplay_Flip();
	if (currentLockCount > 0) {
		remainingLocks = currentLockCount;
		do {
			FlightSurface_Lock();
			--remainingLocks;
		} while (remainingLocks != 0);
	}

	return nextKey;
}

// FUNCTION: XWA 0x434030
size_t FeDiskIo_ReadWithRetryPrompt(void* dst, size_t elemSize, size_t elemCount, XwaFile* stream) {
	uint8_t* cursor;
	size_t remainingCount;
	int attemptsLeft;

	cursor = (uint8_t*)dst;
	remainingCount = elemCount;
	attemptsLeft = 15;
	while (1) {
		size_t bytesRead;
		size_t readCount;

		bytesRead = 0;
		if (elemSize != 0) {
			AeronVfs_Read(stream, cursor, elemSize * remainingCount, &bytesRead);
			readCount = bytesRead / elemSize;
		} else {
			readCount = 0;
		}

		cursor += elemSize * readCount;
		remainingCount -= readCount;
		--attemptsLeft;
		if (remainingCount == 0) {
			break;
		}
		if (attemptsLeft == 0) {
			int16_t key;

			while (1) {
				key = FeDiskIo_ShowRetryFailPrompt();
				if (key == 'R' || key == 'r') {
					break;
				}
				if (key == 'F' || key == 'f') {
					g_fileReadAbortFlag = 1;
					FeDiskIo_FatalError(1u);
					return 0;
				}
			}
			attemptsLeft = 5;
		}
	}

	g_fileReadAbortFlag = 0;
	return elemCount;
}

// FUNCTION: XWA 0x4340D0
void FeDiskIo_FatalError(uint16_t errorCode) {
	char message[128];
	const char* errorMessage;
	size_t i;
	size_t j;
	int exitCode;

	message[0] = '\0';
	if (errorCode < 4) {
		errorMessage = g_strFileErrorMessages[errorCode];
		if (errorMessage != NULL) {
			for (i = 0; i < sizeof(message); ++i) {
				message[i] = errorMessage[i];
				if (errorMessage[i] == '\0') {
					break;
				}
			}

			if (errorCode == 1) {
				for (j = 0; i < sizeof(message); ++i, ++j) {
					message[i] = FileName[j];
					if (FileName[j] == '\0') {
						break;
					}
				}
				if (i >= sizeof(message)) {
					i = sizeof(message) - 1;
				}
				message[i] = '\n';
				if (i + 1 < sizeof(message)) {
					message[i + 1] = '\0';
				}
			}
		}
	}

	exitCode = -255 - (int)errorCode;
	Aeron_LogInfo("xwa.file", "%s", message);
#ifdef XWA_MODERN
	Aeron_FatalError("OpenXWA", message);
#else
	Aeron_FatalError("X-Wing Alliance", message);
#endif
	g_fileFatalExitCode = exitCode;
	g_fileFatalErrorPending = 1;
}

// GLOBAL: XWA 0x63CF5C
static int g_craftNamesLoaded;
// GLOBAL: XWA 0x6343F8
char g_craftModelNames[256][64];

// FUNCTION: XWA 0x4341B0
char* FeDiskIo_GetCraftModelName(unsigned int craftType) {
	char path[256];
	char line[256];
	uint16_t textureGroup;
	const char* listName;
	XwaFile* stream;

	if (!g_craftNamesLoaded) {
		g_craftNamesLoaded = 1;
		memset(g_craftModelNames, 0, sizeof(g_craftModelNames));
		textureGroup = 0;
		listName = g_craftModelListNames[0];
		do {
			int rowIndex;

			strcpy(path, "FlightModels\\");
			strcat(path, listName);
			strcat(path, ".LST");
#ifdef XWA_MODERN
			stream = File_Open(AERON_VFS_ROOT_ASSET, path, "rb");
			g_stream = stream;
#else
			File_OpenGlobalStream(path, "rb", 1, 0);
			stream = g_stream;
#endif
			rowIndex = 0;
#ifdef XWA_MODERN
			while (File_ReadLine(stream, line, sizeof(line))) {
#else
			while (fgets(line, sizeof(line), (FILE*)stream) != NULL) {
#endif
				char* cursor;
				unsigned int objectType;

#ifdef XWA_MODERN
				for (cursor = line; *cursor != '\0' && *cursor != '\r' && *cursor != '\n'; ++cursor) {
				}
#else
				for (cursor = line; *cursor != '\n' && *cursor != '\r'; ++cursor) {
				}
#endif
				*cursor = '\0';
				if (line[0] != '\0') {
					++rowIndex;
					for (objectType = 0; objectType < OBJ_Count; ++objectType) {
						if ((g_modelTypeTable[objectType].recordFlags & MODEL_TYPE_RECORD_TEXTURE_BACKED) !=
								0 &&
							g_modelTypeTable[objectType].textureGroup == textureGroup &&
							g_modelTypeTable[objectType].frameCount == (uint16_t)rowIndex - 1) {
#ifdef XWA_MODERN
							if (objectType < sizeof(g_craftModelNames) / sizeof(g_craftModelNames[0])) {
								strncpy(g_craftModelNames[objectType], line,
										sizeof(g_craftModelNames[objectType]) - 1);
								g_craftModelNames[objectType][sizeof(g_craftModelNames[objectType]) - 1] =
									'\0';
							}
#else
							strcpy(g_craftModelNames[objectType], line);
#endif
						}
					}
				}
			}
			++textureGroup;
			listName += sizeof(g_craftModelListNames[0]);
		} while (textureGroup < 2);
	}

	if (craftType >= OBJ_Count) {
		return NULL;
	}

#ifdef XWA_MODERN
	if (craftType >= XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT ||
		g_objectTypeTables.craftTypeToObjectType[craftType] >=
			sizeof(g_craftModelNames) / sizeof(g_craftModelNames[0])) {
		return NULL;
	}
#endif
	return g_craftModelNames[g_objectTypeTables.craftTypeToObjectType[craftType]][0] != '\0'
			   ? g_craftModelNames[g_objectTypeTables.craftTypeToObjectType[craftType]]
			   : NULL;
}

// FUNCTION: XWA 0x431B70
void FeDiskIo_LoadResources(void) {
	char path[256];
	char line[256];
#ifdef XWA_MODERN
	XwaFile* stream;
#else
	FILE* stream;
#endif
	uint8_t hardpointCount;
	int builtModelCount;
	ModelTypeInfo* modelInfo;
	int rowIndex;
	TexLevel* texLevel;
	const char* listName;
	MemoryHandle modelHandle;
	int loadedModelType;
	uint16_t loadedModelSlot;
	uint16_t textureGroup;
	int remainingModelTypes;

	modelInfo = g_modelTypeTable;
	memset(g_modelFloatHardpointDataHandles, 0, sizeof(g_modelFloatHardpointDataHandles));
	memset(&g_loadedModels, 0, sizeof(g_loadedModels));
	builtModelCount = 0;
	remainingModelTypes = XWA_LOADED_MODEL_COUNT;
	do {
		modelInfo->curTexLevel = NULL;
		modelInfo->texLevels = NULL;
		++modelInfo;
		--remainingModelTypes;
	} while (remainingModelTypes != 0);

	textureGroup = 0;
	listName = g_craftModelListNames[0];
	do {
		strcpy(path, "FlightModels\\");
		strcat(path, listName);
		strcat(path, ".LST");
#ifdef XWA_MODERN
		stream = File_Open(AERON_VFS_ROOT_ASSET, path, "rb");
		g_stream = stream;
		if (stream == NULL) {
			FeDiskIo_FatalError(1);
			return;
		}
#else
		File_OpenGlobalStream(path, "rb", 1, 0);
		stream = (FILE*)g_stream;
#endif

		rowIndex = 0;
#ifdef XWA_MODERN
		while (File_ReadLine(stream, line, sizeof(line))) {
#else
		while (fgets(line, sizeof(line), stream) != NULL) {
#endif
			char* cursor;
			uint16_t matchingModelType;
			char matched;
			uint8_t matchedAssetFlags;

#ifdef XWA_MODERN
			for (cursor = line; *cursor != '\0' && *cursor != '\r' && *cursor != '\n'; ++cursor) {
			}
#else
			for (cursor = line; *cursor != '\n' && *cursor != '\r'; ++cursor) {
			}
#endif
			*cursor = '\0';
			if (line[0] == '\0') {
				continue;
			}

			++rowIndex;
			matched = 0;
			for (matchingModelType = 0; matchingModelType < XWA_LOADED_MODEL_COUNT; ++matchingModelType) {
				uint8_t assetFlags;

				assetFlags = g_modelTypeTable[matchingModelType].assetFlags;
				if ((g_modelTypeTable[matchingModelType].recordFlags & MODEL_TYPE_RECORD_TEXTURE_BACKED) !=
						0 &&
					g_modelTypeTable[matchingModelType].textureGroup == textureGroup &&
					g_modelTypeTable[matchingModelType].frameCount == (uint16_t)rowIndex - 1 &&
					(assetFlags & MODEL_TYPE_ASSET_TEXTURE_READY) == 0 &&
					(assetFlags & MODEL_TYPE_ASSET_TEXTURE_UNLOAD_CLASS_MASK) != 0 &&
					((assetFlags & MODEL_TYPE_ASSET_SPECIAL_MODE_ONLY) == 0 || g_provingGroundsModeActive) &&
					((assetFlags & MODEL_TYPE_ASSET_DEATH_STAR_ONLY) == 0 ||
					 g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) &&
					((assetFlags & MODEL_TYPE_ASSET_NOT_DEATH_STAR) == 0 ||
					 g_missionHeader.body.missionType != XWA_MISSION_TYPE_DEATH_STAR)) {
					matched = 1;
					matchedAssetFlags = (uint8_t)assetFlags;
					break;
				}
			}

			if (matched) {
				char hasLoadedModel;

				modelHandle = 0;
				texLevel = NULL;
				hasLoadedModel = (matchedAssetFlags & MODEL_TYPE_ASSET_MODEL_LOADED) != 0;
				if (hasLoadedModel) {
					modelHandle = OptModel_LoadHandle(line);
					FlightNet_BroadcastStillLoadingPulse();
				#ifdef XWA_MODERN
					if (modelHandle == 0) {
						DebugPrintf("Failed to load required flight model '%s'", line);
						File_Close(stream);
						g_stream = NULL;
						FeDiskIo_FatalError(1);
						return;
					}
				#endif
					texLevel = (TexLevel*)Memory_LockHandle(modelHandle);
				#ifdef XWA_MODERN
					if (texLevel == NULL) {
						DebugPrintf("Failed to lock required flight model '%s' handle %u", line,
									(unsigned int)modelHandle);
						OptModel_FreeHandle(modelHandle);
						File_Close(stream);
						g_stream = NULL;
						FeDiskIo_FatalError(1);
						return;
					}
				#endif
				}

				loadedModelSlot = 0;
				for (; loadedModelSlot < XWA_LOADED_MODEL_COUNT; ++loadedModelSlot) {
					uint8_t assetFlags;
					uint16_t modelIndex;

					loadedModelType = loadedModelSlot;
					assetFlags = g_modelTypeTable[loadedModelType].assetFlags;
					if ((g_modelTypeTable[loadedModelType].recordFlags & MODEL_TYPE_RECORD_TEXTURE_BACKED) !=
							0 &&
						g_modelTypeTable[loadedModelType].textureGroup == textureGroup &&
						g_modelTypeTable[loadedModelType].frameCount == (uint16_t)rowIndex - 1 &&
						(assetFlags & MODEL_TYPE_ASSET_TEXTURE_READY) == 0 &&
						(assetFlags & MODEL_TYPE_ASSET_TEXTURE_UNLOAD_CLASS_MASK) != 0 &&
						((assetFlags & MODEL_TYPE_ASSET_SPECIAL_MODE_ONLY) == 0 ||
						 g_provingGroundsModeActive) &&
						((assetFlags & MODEL_TYPE_ASSET_DEATH_STAR_ONLY) == 0 ||
						 g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) &&
						((assetFlags & MODEL_TYPE_ASSET_NOT_DEATH_STAR) == 0 ||
						 g_missionHeader.body.missionType != XWA_MISSION_TYPE_DEATH_STAR)) {
						g_modelTypeTable[loadedModelType].curTexLevel = texLevel;
						modelIndex = (uint16_t)g_modelTypeTable[loadedModelType].modelIndex;
						g_loadedModels.byObjectType[loadedModelType] = modelHandle;
						if (hasLoadedModel) {
							FeDiskIo_BuildModelDef(modelIndex, loadedModelSlot);
							++builtModelCount;
						}

						if ((g_modelTypeTable[loadedModelType].flags &
							 MODEL_TYPE_FLAG_EXPANDED_TARGET_PROBE) != 0) {
							if (g_modelDefs[(uint16_t)g_modelTypeTable[loadedModelType].modelIndex]
									.floatHardpointCount != 0) {
								ModelFloatHardpoint* hardpoint;
								int remainingHardpoints;

								hardpointCount =
									g_modelDefs[(uint16_t)g_modelTypeTable[loadedModelType].modelIndex]
										.floatHardpointCount;
								hardpoint = (ModelFloatHardpoint*)Memory_LockHandle(
									g_modelFloatHardpointDataHandles[loadedModelType]);
								remainingHardpoints = hardpointCount;
								do {
									hardpoint->componentIndex =
										(uint8_t)ModelMesh_FindFloatHardpointComponent(
											loadedModelType, hardpoint->x, hardpoint->negY, hardpoint->z);
									++hardpoint;
									--remainingHardpoints;
								} while (remainingHardpoints != 0);
							} else {
								OptimizedPolyObject* model;
								size_t allocSize;
								int rootNodeIdx;
								MemoryHandle handle;
								/* One slot per root node, read back as a pointer table by
								 * FeDiskIo_GetMeshVertexComponentMap. */
								uint8_t** rootMap;

								model = (OptimizedPolyObject*)Memory_LockHandle(
									g_loadedModels.byObjectType[loadedModelType]);
								OptModel_AdjustOptimizedPolyObjectPointers(model);
								allocSize = sizeof(*rootMap) * (size_t)model->rootNodeCount;
								for (rootNodeIdx = 0; rootNodeIdx < model->rootNodeCount; ++rootNodeIdx) {
									OptNode* rootNode;

									rootNode = model->rootNodes[rootNodeIdx];
									if (rootNode->nodeType != OPT_TEXTURE &&
										rootNode->nodeType != OPT_TEXTURE_REF) {
										MeshDescriptor* descriptor;

										descriptor = ModelMesh_FindDescriptorNodeRecursive(rootNode, model);
										if (descriptor != NULL && descriptor->meshType == MESH_MainHull) {
											int vertexParam;

											vertexParam =
												(int)ModelMesh_FindFirstMeshVertsNode(rootNode)->param1;
											if (vertexParam > 2) {
												vertexParam -= 2;
											}
											allocSize += (size_t)vertexParam;
										}
									}
								}

								handle = Memory_AllocHandleZeroed("FLOATHARDPTS2", allocSize);
								if (handle == 0) {
									FeDiskIo_FatalError(0);
								}
								g_modelFloatHardpointDataHandles[loadedModelType] = handle;
								rootMap = (uint8_t**)Memory_LockHandle(handle);
								for (rootNodeIdx = 0; rootNodeIdx < model->rootNodeCount; ++rootNodeIdx) {
									rootMap[rootNodeIdx] = NULL;
								}
								Memory_UnlockHandle(handle);
								Memory_UnlockHandle(g_loadedModels.byObjectType[loadedModelType]);
							}
						}
					}
				}
			}
		}

		g_stream = (XwaFile*)stream;
#ifdef XWA_MODERN
		File_Close(stream);
		g_stream = NULL;
#else
		if ((stream->_flag & 0x20) == 0) {
			fclose(stream);
		}
#endif
		++textureGroup;
		listName += sizeof(g_craftModelListNames[0]);
	} while (textureGroup < 2u);

	FlightLoading_PulseAndDrawProgressScreen(3);

#ifdef XWA_MODERN
	DebugPrintf(NULL);
#else
	DebugPrintf();
#endif

	{
		uint8_t missionType;
		int modelType;
		ModelTypeInfo* info;

		missionType = g_missionHeader.body.missionType;
		info = g_modelTypeTable;
		for (modelType = 0; (uint16_t)modelType < XWA_LOADED_MODEL_COUNT; ++modelType, ++info) {
			if ((info->recordFlags & MODEL_TYPE_RECORD_TEXTURE_BACKED) != 0) {
				uint8_t assetFlags;

				assetFlags = info->assetFlags;
				if ((assetFlags & MODEL_TYPE_ASSET_TEXTURE_UNLOAD_CLASS_MASK) != 0 &&
					((assetFlags & MODEL_TYPE_ASSET_SPECIAL_MODE_ONLY) == 0 || g_provingGroundsModeActive) &&
					(assetFlags & MODEL_TYPE_ASSET_TEXTURE_READY) != 0 &&
					((assetFlags & MODEL_TYPE_ASSET_DEATH_STAR_ONLY) == 0 ||
					 missionType == XWA_MISSION_TYPE_DEATH_STAR) &&
					((assetFlags & MODEL_TYPE_ASSET_NOT_DEATH_STAR) == 0 ||
					 missionType != XWA_MISSION_TYPE_DEATH_STAR) &&
					(g_useHardware3D || (info->flags & MODEL_TYPE_FLAG_HARDWARE_ONLY) == 0) &&
					(assetFlags & MODEL_TYPE_ASSET_TEXTURE_DRAW) != 0) {
					FeDiskIo_LoadTexturesForType(modelType);
					DebugPrintf((const char*)(uintptr_t)modelType);
					missionType = g_missionHeader.body.missionType;
				}
			}
		}
	}

#ifdef XWA_MODERN
	DebugPrintf(NULL);
#else
	DebugPrintf();
#endif
	FlightLoading_PulseAndDrawProgressScreen(4);
	LensFlare_InitQueue();
	FlightLoading_PulseAndDrawProgressScreen(5);
	FlightLoading_PulseAndDrawProgressScreen(6);
#ifdef XWA_MODERN
	(void)builtModelCount;
#endif
}

// FUNCTION: XWA 0x432850
void FeDiskIo_InitResources(void) {
	RgbTriplet targetRgb;

	g_loadingModel = 1;
	FeDiskIo_LoadResources();
	g_loadingModel = 0;
	ModelMesh_BuildObjectTypeMeshCache();

	targetRgb.r = 0;
	targetRgb.g = 0;
	targetRgb.b = 2;
	g_flightColorEscapeBypassChar =
		(uint8_t)Color_FindNearestRgbTripletIndex(&targetRgb, g_swPalette, 0, 256u);
	g_unusedFlightRenderColorByte = g_flightColorEscapeBypassChar;
}

static __inline int FlightModel_IsTieCockpitObjectType(uint16_t objectType) {
	return objectType == OBJ_TIEFighter || objectType == OBJ_TIEInterceptor ||
		   objectType == OBJ_TIEAdvanced || objectType == OBJ_TIEDefender || objectType == OBJ_TIEBizarro ||
		   objectType == OBJ_TIEBigGun || objectType == OBJ_TIEWarheads || objectType == OBJ_TIEBomb ||
		   objectType == OBJ_TIEBomber || objectType == OBJ_TIEBooster;
}

static __inline void FeDiskIo_FinalizeCockpitModelLoad(uint16_t objectType) {
	if (g_cockpitModel != 0) {
		uint16_t previousSlot0Handle;
		int meshCount;
		int meshIdx;

		g_flightRenderToFrontend = 1;

		previousSlot0Handle = g_loadedModels.byObjectType[0];
		g_loadedModels.byObjectType[0] = g_cockpitModel;
		meshCount = ModelMesh_GetCount(0);
		g_cockpitModelMeshCache.meshCount = meshCount;
		meshIdx = 0;
		if (meshCount > 0) {
			do {
				g_cockpitModelMeshCache.meshTypes[meshIdx] = ModelMesh_GetType(0, meshIdx);
				g_cockpitModelMeshCache.meshDescriptors[meshIdx] = ModelMesh_GetDescriptor(0, meshIdx);
				DebugPrintf(NULL, meshIdx);
				++meshIdx;
			} while (meshIdx < meshCount);
		}

		g_flightRenderToFrontend = 0;
		g_loadedModels.byObjectType[0] = previousSlot0Handle;
	}

	{
		uint16_t turretModelIndex;

		turretModelIndex =
			g_modelDefs[(uint16_t)GetModelIndexFromType((ObjectTypeId)objectType)].turretModelIndex[0];
		if (turretModelIndex != 0) {
			if (g_loadedModels.byObjectType[turretModelIndex] != 0) {
				g_players[g_localPlayer].cockpitToggleAvailable = 1;
				return;
			}

			g_players[g_localPlayer].cockpitToggleAvailable = 0;
		}
	}
}

// FUNCTION: XWA 0x4314B0
void FeDiskIo_LoadCockpitModel(void) {
	char path[256];
#ifdef XWA_MODERN
	uint16_t objectType = 0;
	const char* modelName = "";
#else
	uint16_t objectType;
	const char* modelName;
#endif
	char* pathBuffer;
	int localDplayId;
	const char* flightModelsDir;
	const char* cockpitOptName;

	localDplayId = NetSession_GetLocalDplayId();
	pathBuffer = path;
	flightModelsDir = "FlightModels\\";
	cockpitOptName = "Cockpit.opt";
	g_cockpitUsesTieHitEffectPlanes = 0;

	if (g_cockpitModel != 0) {
		OptModel_FreeHandle(g_cockpitModel);
		g_cockpitModel = 0;
	}

	if ((int32_t)g_players[g_localPlayer].boundObjectSignature > 0) {
		objectType = (uint16_t)g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
		modelName = g_modelDefs[(uint16_t)GetModelIndexFromType((ObjectTypeId)objectType)].name;
	} else {
		int playerIdx;
		PilotNetworkPlayer* networkPlayer;

		networkPlayer = g_pilotData.networkPlayers;
		for (playerIdx = 0; playerIdx < 8; ++networkPlayer, ++playerIdx) {
			if (localDplayId == networkPlayer->directPlayId) {
				int flightGroupIdx;
				uint8_t craftType;

				flightGroupIdx = g_pilotData.networkPlayers[playerIdx].flightGroupId;
				craftType = g_missionFlightGroups[flightGroupIdx].fg.craftType;
				objectType = (uint16_t)g_objectTypeTables.craftTypeToObjectType[craftType];
				modelName = g_modelDefs[(uint16_t)GetModelIndexFromType((ObjectTypeId)objectType)].name;
				break;
			}
		}
	}

	g_cockpitObjectTypeForFilmHeader = objectType;

	strcpy(pathBuffer, flightModelsDir);
	strcat(pathBuffer, modelName);
	strcat(pathBuffer, cockpitOptName);
	{
		uint16_t modelHandle;

		modelHandle = OptModel_LoadHandle(pathBuffer);

		if (modelHandle != 0) {
			g_cockpitModel = modelHandle;
			g_players[g_localPlayer].cockpitLookAvailable = 1;
			FeDiskIo_LoadCockpitGlowEmitters(objectType);
			if (FlightModel_IsTieCockpitObjectType(objectType)) {
				g_cockpitUsesTieHitEffectPlanes = 1;
			}
			FeDiskIo_FinalizeCockpitModelLoad(objectType);
			return;
		} else {
			g_cockpitModel = 0;
			if (FlightModel_IsTieCockpitObjectType(objectType)) {
				strcpy(pathBuffer, flightModelsDir);
				strcat(pathBuffer, "Tie");
				strcat(pathBuffer, cockpitOptName);
				modelHandle = OptModel_LoadHandle(pathBuffer);
				g_cockpitModel = modelHandle;
				if (modelHandle != 0) {
					g_cockpitUsesTieHitEffectPlanes = 1;
				}
			}

			if (modelHandle == 0) {
				strcpy(pathBuffer, flightModelsDir);
				strcat(pathBuffer, "CombatSim");
				strcat(pathBuffer, cockpitOptName);
				modelHandle = OptModel_LoadHandle(pathBuffer);
				g_cockpitModel = modelHandle;
			}

			if (modelHandle != 0) {
				g_players[g_localPlayer].cockpitLookAvailable = 1;
				FeDiskIo_LoadCockpitGlowEmitters(objectType);
			} else {
				g_players[g_localPlayer].cockpitLookAvailable = 0;
			}
		}
	}

	FeDiskIo_FinalizeCockpitModelLoad(objectType);
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsometimes-uninitialized"
#endif
// FUNCTION: XWA 0x431960
void FeDiskIo_LoadExteriorModel(void) {
	char path[256];
	char* pathBuffer;
	unsigned int objectType;
	const char* modelName;
	uint16_t previousSlot0Handle;
	int localDplayId;
	int meshCount;
	int meshIdx;
	volatile uint16_t* loadedModelSlot0;
	const char* flightModelsDir;
	const char* exteriorOptName;

	localDplayId = NetSession_GetLocalDplayId();
	pathBuffer = path;
	flightModelsDir = "FlightModels\\";
	exteriorOptName = "Exterior.opt";

	if (g_exteriorModel != 0) {
		OptModel_FreeHandle(g_exteriorModel);
		g_exteriorModel = 0;
	}

	if ((int32_t)g_players[g_localPlayer].boundObjectSignature > 0) {
		objectType = (uint16_t)g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
	} else {
		int playerIdx;

		for (playerIdx = 0; playerIdx < 8; ++playerIdx) {
			if (localDplayId == g_pilotData.networkPlayers[playerIdx].directPlayId) {
				int flightGroupIdx;
				uint8_t craftType;

				flightGroupIdx = g_pilotData.networkPlayers[playerIdx].flightGroupId;
				craftType = g_missionFlightGroups[flightGroupIdx].fg.craftType;
				objectType = (uint16_t)g_objectTypeTables.craftTypeToObjectType[craftType];
				goto resolve_model_name;
			}
		}

		goto load_model;
	}

resolve_model_name:
	modelName = g_modelDefs[(uint16_t)GetModelIndexFromType((ObjectTypeId)objectType)].name;

load_model:
	strcpy(pathBuffer, flightModelsDir);
	strcat(pathBuffer, modelName);
	strcat(pathBuffer, exteriorOptName);

	g_exteriorModel = OptModel_LoadHandle(pathBuffer);
	FeDiskIo_LoadExteriorGlowEmitters(objectType);

	if (g_exteriorModel != 0) {
		g_exteriorModelLoaded = 1;
		g_flightRenderToFrontend = 1;

		loadedModelSlot0 = &g_loadedModels.byObjectType[0];
		previousSlot0Handle = *loadedModelSlot0;
		g_loadedModels.byObjectType[0] = g_exteriorModel;
		meshCount = ModelMesh_GetCount(0);
		g_exteriorModelMeshCache.meshCount = meshCount;
		meshIdx = 0;
		if (meshCount > 0) {
			do {
				g_exteriorModelMeshCache.meshTypes[meshIdx] = ModelMesh_GetType(0, meshIdx);
				g_exteriorModelMeshCache.meshDescriptors[meshIdx] = ModelMesh_GetDescriptor(0, meshIdx);
				DebugPrintf(NULL, meshIdx);
				++meshIdx;
			} while (meshIdx < meshCount);
		}

		g_flightRenderToFrontend = 0;
		g_loadedModels.byObjectType[0] = previousSlot0Handle;
	} else {
		g_exteriorModelLoaded = 0;
	}
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
