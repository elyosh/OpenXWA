#include "xwa/flight/object/damage.h"

#include "xwa/assets/model_mesh.h"
#include "xwa/assets/string_table.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/audio/fsfx.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/random.h"

#include <math.h>
#include <string.h>

#ifndef XWA_MODERN
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#define DAMAGE_OUTPUT_DEBUG_STRING g_OutputDebugStringA
#else
static void Damage_OutputDebugString(const char* outputString) { DebugPrintf("%s", outputString); }
#define DAMAGE_OUTPUT_DEBUG_STRING Damage_OutputDebugString
#endif

enum {
	DAMAGE_MFD_KEY_PREVIOUS_SYSTEM = 0x00a6,
	DAMAGE_MFD_KEY_NEXT_SYSTEM = 0x00a7,
	DAMAGE_MFD_KEY_MOVE_TO_TOP = 0x00c1,
};

// GLOBAL: XWA 0x631864
int16_t g_damageMfdLastSelectedSystemId;
// GLOBAL: XWA 0x631868
int16_t g_damageMfdDamagedSystemCountCached;
// GLOBAL: XWA 0x63186C
int16_t g_damageMfdSelectedRowY;
// GLOBAL: XWA 0x631870
int16_t g_damageMfdRightPaneRedrawPending;
// GLOBAL: XWA 0x631874
int16_t g_damageMfdForceAllSystemsDamaged;
// GLOBAL: XWA 0x8D9404
int16_t g_damageMfdSelectedSystemId;

// GLOBAL: XWA 0x5AE200
const uint16_t g_subsystemRepairDuration[10] = {
	15u, 15u, 20u, 25u, 20u, 15u, 20u, 20u, 20u, 50u,
};

// GLOBAL: XWA 0x5AE218
// HUD-feature mask bit selected for a random failure slot (16 random buckets).
const uint16_t g_subsystemFailureHudMaskByRandomSlot[16] = {
	512u, 16u, 32u, 2u, 1024u, 384u, 16u, 8u, 2048u, 384u, 32u, 2u, 4096u, 1u, 32u, 8u,
};

// FUNCTION: XWA 0x4235D0
int16_t Damage_FindAdjacentDamagedSystem(int16_t currentSystemIdx, uint16_t directionStep) {
	CraftData* craft;
	uint8_t systemByDisplaySlot[10];
	int fillSystemIdx;
	uint16_t previousDamagedSystem;
	int displaySlot;

	craft = NULL;
	if (g_players[g_localPlayer].objectIndex != 0xffffu) {
		MobileObject* mobj;

		mobj = g_objectTable[g_players[g_localPlayer].objectIndex].mobj;
		if (mobj != NULL) {
			craft = mobj->pCraft;
		}
	}

	if (craft == NULL) {
		DAMAGE_OUTPUT_DEBUG_STRING("NULL Craft data pointer in nextsystem() in Damage.c\n");
		return 0;
	}

	for (fillSystemIdx = 0; fillSystemIdx < 10; ++fillSystemIdx) {
		systemByDisplaySlot[craft->systemDisplaySlotBySystem[fillSystemIdx]] = (uint8_t)fillSystemIdx;
	}

	previousDamagedSystem = 0xffffu;
	displaySlot = 0;
	while (1) {
		if (craft->systemHealth[systemByDisplaySlot[displaySlot]] == 0) {
			if (systemByDisplaySlot[displaySlot] == currentSystemIdx) {
				break;
			}
			previousDamagedSystem = systemByDisplaySlot[displaySlot];
		}

		++displaySlot;
		if (displaySlot >= 10) {
			return 0;
		}
	}

	if (directionStep == 0xffffu) {
		if (previousDamagedSystem != 0xffffu) {
			return previousDamagedSystem;
		}

		for (displaySlot = 9; displaySlot >= 0; --displaySlot) {
			if (craft->systemHealth[systemByDisplaySlot[displaySlot]] != 0) {
				return (int16_t)systemByDisplaySlot[displaySlot];
			}
		}
		return (int16_t)systemByDisplaySlot[9];
	}

	for (++displaySlot; displaySlot < 10; ++displaySlot) {
		if (craft->systemHealth[systemByDisplaySlot[displaySlot]] == 0) {
			return (int16_t)systemByDisplaySlot[displaySlot];
		}
	}

	for (displaySlot = 0; displaySlot < 10; ++displaySlot) {
		if (craft->systemHealth[systemByDisplaySlot[displaySlot]] != 0) {
			return (int16_t)systemByDisplaySlot[displaySlot];
		}
	}

	for (displaySlot = 0; displaySlot < 10; ++displaySlot) {
		if (craft->systemHealth[systemByDisplaySlot[displaySlot]] == 0) {
			return (int16_t)systemByDisplaySlot[displaySlot];
		}
	}

	return currentSystemIdx;
}

// FUNCTION: XWA 0x422B70
void Damage_DisplayMfdPage(int mfdSide, void* mfdTexPixels) {
	CraftData* craft;
	uint16_t systemByDisplaySlot[10];
	unsigned int systemIdx;
	int16_t systemNumber;
	int damagedSystemCount;
	int16_t allSystemsOk;
	uint16_t paneWidth;
	uint16_t paneHeight;
	int lineStep;
	int rowY;
	int rowBottom;
	int displaySlot;

	craft = NULL;
	if (g_players[g_localPlayer].mapCameraState != 0) {
		unsigned int altViewObjectIdx;

		altViewObjectIdx = (unsigned int)g_players[g_localPlayer].altViewObjectIdx;
		if (altViewObjectIdx >= g_activeRegionObjectSlotStart &&
			altViewObjectIdx < g_activeRegionCraftObjectSlotEnd &&
			g_objectTable[altViewObjectIdx].objectType != OBJ_None) {
			MobileObject* mobj;

			mobj = g_objectTable[altViewObjectIdx].mobj;
			if (mobj != NULL) {
				craft = mobj->pCraft;
			}
		}
	} else {
		MobileObject* mobj;

		mobj = g_objectTable[g_players[g_localPlayer].objectIndex].mobj;
		if (mobj != NULL) {
			craft = mobj->pCraft;
		}
	}
	if (craft == NULL) {
		DAMAGE_OUTPUT_DEBUG_STRING("NULL craft data pointer in DisplayDamage()\n");
		return;
	}

	if (g_useHardware3D) {
		if (mfdSide == 1) {
			FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		} else {
			FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdPaneWidth - g_hudMfdTextInsetX),
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		}
	} else {
		FlightSw_SetRenderTarget(mfdTexPixels, g_hudMfdPaneWidth, g_hudMfdPaneHeight,
								 g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	}

	for (systemNumber = 0; systemNumber < 10; ++systemNumber) {
		systemByDisplaySlot[craft->systemDisplaySlotBySystem[systemNumber]] = (uint16_t)systemNumber;
	}

	if (g_damageMfdForceAllSystemsDamaged != 0) {
		for (displaySlot = 0; displaySlot < 10; ++displaySlot) {
			systemIdx = systemByDisplaySlot[displaySlot];
			craft->systemHealth[systemIdx] = 0;
			craft->systemTimer[systemIdx] = g_subsystemRepairDuration[systemIdx];
		}
		g_damageMfdForceAllSystemsDamaged = 0;
	}

	allSystemsOk = 1;
	damagedSystemCount = 0;
	for (displaySlot = 0; displaySlot < 10; ++displaySlot) {
		systemIdx = systemByDisplaySlot[displaySlot];
		if (craft->systemHealth[systemIdx] == 0 &&
			(g_subsystemIdToFlag[systemIdx] & craft->systemFlags) != 0) {
			allSystemsOk = 0;
			++damagedSystemCount;
		}
	}

	FlightText_SetClipRect(0, 0, g_hudMfdPaneWidth, g_hudMfdPaneHeight);
	FlightText_SetCursor(0, 0);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	if (allSystemsOk) {
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetColor(0x52u);
		{
			int cursorY;

			cursorY = (g_hudMfdPaneHeight >> 1) - g_flightFontLineHeight;
			FlightText_SetCursor(0, (int16_t)cursorY);
		}
		FlightText_DrawStringCentered(g_strMfdStrings[MFD_STRING_ALL_SYSTEMS_GO]);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
			return;
		}
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		if (mfdSide == 1) {
			Blit16ToFlightSurface(mfdTexPixels, g_flightColorEscapeBypassChar, 0, 0, 5u,
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), g_hudMfdPaneWidth,
								  g_hudMfdPaneHeight,
								  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
			return;
		}
		Blit16ToFlightSurface(mfdTexPixels, g_flightColorEscapeBypassChar, 0, 0,
							  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), g_hudMfdPaneWidth,
							  g_hudMfdPaneHeight, (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
		return;
	}

	lineStep = g_flightFontLineHeight - (int)(g_flightHudScaleFactor * -2.0f);
	paneWidth = g_hudMfdPaneWidth;
	paneHeight = g_hudMfdPaneHeight;
	if ((mfdSide == 1 && g_mfdLeftNeedsRedraw) || (mfdSide == 2 && g_mfdRightNeedsRedraw)) {
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetColor(0x46u);
		FlightText_SetCursor(0, 0);
		FlightText_DrawStringCentered(g_strDamageSystemNames[10]);
		if (mfdSide == 1) {
			g_mfdLeftNeedsRedraw = 0;
		} else if (mfdSide == 2) {
			g_mfdRightNeedsRedraw = 0;
		}
	}
	if (g_useHardware3D) {
		FlightText_SetColor(0x46u);
		FlightText_SetCursor(0, 0);
		FlightText_DrawStringCentered(g_strDamageSystemNames[10]);
	}

	rowY = 2 * lineStep;
	if (damagedSystemCount != g_damageMfdDamagedSystemCountCached) {
		FlightText_SetClipRect(0, rowY, paneWidth, paneHeight);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		if (mfdSide == 1 && g_players[g_localPlayer].mfd.page[2] == 3) {
			g_damageMfdRightPaneRedrawPending = 1;
		}
#ifdef XWA_MODERN
		if (g_damageMfdSelectedSystemId >= 0 && craft->systemHealth[g_damageMfdSelectedSystemId] != 0) {
#else
		if (craft->systemHealth[g_damageMfdSelectedSystemId] != 0) {
#endif
			g_damageMfdSelectedSystemId = -1;
		}
		g_damageMfdDamagedSystemCountCached = damagedSystemCount;
	}

	if (mfdSide == 2 && g_damageMfdRightPaneRedrawPending != 0) {
		FlightText_SetClipRect(0, rowY, paneWidth, paneHeight);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
	}

	if (mfdSide == g_players[g_localPlayer].mfd.activeIndex && g_flightPlayerCount == 1) {
		switch (g_currentActionKey) {
			case DAMAGE_MFD_KEY_PREVIOUS_SYSTEM: {
				int16_t candidateSystem;
				uint16_t candidateFlag;

				candidateSystem = g_damageMfdSelectedSystemId;
				do {
					candidateSystem = Damage_FindAdjacentDamagedSystem(candidateSystem, 0xffff);
					candidateFlag = g_subsystemIdToFlag[candidateSystem];
					if ((craft->systemFlags & candidateFlag) != 0 &&
						craft->systemHealth[candidateSystem] == 0) {
						g_damageMfdSelectedSystemId = candidateSystem;
					}
				} while ((craft->systemFlags & candidateFlag) == 0);
				break;
			}
			case DAMAGE_MFD_KEY_NEXT_SYSTEM: {
				int16_t candidateSystem;

				candidateSystem = g_damageMfdSelectedSystemId;
				do {
					candidateSystem = Damage_FindAdjacentDamagedSystem(candidateSystem, 1);
					if ((g_subsystemIdToFlag[candidateSystem] & craft->systemFlags) != 0 &&
						craft->systemHealth[candidateSystem] == 0) {
						g_damageMfdSelectedSystemId = candidateSystem;
					}
				} while ((g_subsystemIdToFlag[g_damageMfdSelectedSystemId] & craft->systemFlags) == 0);
				break;
			}
			case DAMAGE_MFD_KEY_MOVE_TO_TOP: {
				uint8_t selectedDisplaySlot;

#ifdef XWA_MODERN
				if (g_damageMfdSelectedSystemId < 0 || g_damageMfdSelectedSystemId >= 10) {
					break;
				}
#endif
				selectedDisplaySlot = craft->systemDisplaySlotBySystem[g_damageMfdSelectedSystemId];
				for (systemNumber = 0; systemNumber < 10; ++systemNumber) {
					if (craft->systemDisplaySlotBySystem[systemNumber] < selectedDisplaySlot) {
						++craft->systemDisplaySlotBySystem[systemNumber];
					}
				}
				craft->systemDisplaySlotBySystem[g_damageMfdSelectedSystemId] = 0;
			} break;
		}
	}

	for (systemNumber = 0; systemNumber < 10; ++systemNumber) {
		systemByDisplaySlot[craft->systemDisplaySlotBySystem[systemNumber]] = (uint16_t)systemNumber;
	}
	rowBottom = rowY + lineStep;

	for (displaySlot = 0; displaySlot < 10; ++displaySlot) {
		uint16_t systemId;
		uint16_t systemFlag;

		systemId = systemByDisplaySlot[displaySlot];
		if (craft->systemHealth[systemId] == 0) {
			systemFlag = g_subsystemIdToFlag[systemId];
			if ((systemFlag & craft->systemFlags) != 0) {
				FlightText_SetColor(0x4au);
				if (rowY + lineStep < (int)paneHeight) {
					CraftData* localCraft;
					int objectIndex;
					char str[6];

					FlightText_SetClipRect(0, rowY, paneWidth, rowBottom);
					FlightText_SetCursor(0, (int16_t)rowY);
					if (g_damageMfdSelectedSystemId == -1) {
						g_damageMfdLastSelectedSystemId = systemId;
						g_damageMfdSelectedSystemId = (int16_t)systemId;
					}
					if (g_damageMfdSelectedSystemId == systemId) {
						g_damageMfdSelectedRowY = rowY;
						FlightText_SetBackgroundColor(0x33u);
					} else {
						FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
					}

					localCraft = NULL;
					objectIndex = g_players[g_localPlayer].objectIndex;
					if (objectIndex != 0xffff) {
						MobileObject* localMobj;

						localMobj = g_objectTable[objectIndex].mobj;
						if (localMobj != NULL) {
							localCraft = localMobj->pCraft;
						}
					}
					if (localCraft == NULL) {
						DAMAGE_OUTPUT_DEBUG_STRING("NULL Craft data pointer in outputsystem() in Damage.c\n");
						continue;
					}

					if ((systemFlag & localCraft->systemFlags) == 0) {
						FlightText_SetColor(0x41u);
						if (g_useHardware3D && systemId == g_damageMfdSelectedSystemId) {
							FlightText_SetColor(0x46u);
						}
						str[0] = 'N';
						str[1] = '/';
						str[2] = 'A';
						str[3] = '\0';
					} else {
						if (localCraft->systemHealth[systemId] == 0) {
							uint8_t minutes;
							uint8_t seconds;

							FlightText_SetColor(0x4au);
							if (g_useHardware3D && systemId == g_damageMfdSelectedSystemId) {
								FlightText_SetColor(0x46u);
							}
							minutes = (uint8_t)(localCraft->systemTimer[systemId] / 60);
							seconds = (uint8_t)(localCraft->systemTimer[systemId] - 60 * minutes);
							str[0] = (char)('0' + minutes / 10);
							str[1] = (char)('0' + minutes - 10 * (minutes / 10));
							str[2] = ':';
							str[3] = (char)('0' + seconds / 10);
							str[4] = (char)('0' + seconds - 10 * (seconds / 10));
							str[5] = '\0';
						} else if (localCraft->systemHealth[systemId] == 100u) {
							FlightText_SetColor(0x52u);
							if (g_useHardware3D && systemId == g_damageMfdSelectedSystemId) {
								FlightText_SetColor(0x46u);
							}
							str[0] = '1';
							str[1] = '0';
							str[2] = '0';
							str[3] = '%';
							str[4] = '\0';
						} else {
							uint8_t tens;

							FlightText_SetColor(0x4eu);
							if (g_useHardware3D && systemId == g_damageMfdSelectedSystemId) {
								FlightText_SetColor(0x46u);
							}
							tens = (uint8_t)(localCraft->systemHealth[systemId] / 10);
							str[0] = (char)('0' + tens);
							str[1] = (char)('0' + localCraft->systemHealth[systemId] - 10 * tens);
							str[2] = '%';
							str[3] = '\0';
						}
					}

					if (!g_useHardware3D) {
						g_flightFillClipRectFn();
					}
					FlightText_DrawString(g_strDamageSystemNames[systemId]);
					g_flightDrawCharFn(10);
					FlightText_SetCursor(0, (int16_t)rowY);
					FlightText_DrawStringRightAligned(str);
					rowY += lineStep;
					rowBottom += lineStep;
				}
			}
		}
	}

	g_damageMfdLastSelectedSystemId = g_damageMfdSelectedSystemId;
	FlightText_SetShadowEnabled(0);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
		return;
	}

	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	if (mfdSide == 1) {
		Blit16ToFlightSurface(mfdTexPixels, g_flightColorEscapeBypassChar, 0, 0, 5u,
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), g_hudMfdPaneWidth,
							  g_hudMfdPaneHeight, (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
		return;
	}
	Blit16ToFlightSurface(mfdTexPixels, g_flightColorEscapeBypassChar, 0, 0,
						  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
						  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), g_hudMfdPaneWidth,
						  g_hudMfdPaneHeight, (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
}

// FUNCTION: XWA 0x4E0100
unsigned int Craft_DamageComponent(uint16_t objIdx, int componentId, unsigned int damageAmount,
								   uint16_t sourceObjIdx) {
	uint16_t componentIdx;
	unsigned int relatedObjIdx;
	int playerIdx;

	componentId += 0xffff;
	componentIdx = (uint16_t)componentId;
	if (g_curCraft->componentHp[componentIdx] == 0) {
		return damageAmount;
	}

	if (g_curCraft->componentHp[componentIdx] == 0xffu &&
		(g_objectTable[objIdx].objectType != OBJ_SuperStarDestroyer ||
		 ModelMesh_GetObjectTypeMeshType(OBJ_SuperStarDestroyer, componentIdx) != MESH_Bridge)) {
		return damageAmount;
	}

	if (g_curCraft->shieldFront + g_curCraft->shieldRear > 0) {
		if (ModelMesh_GetObjectTypeMeshType(g_objectTable[objIdx].objectType, componentIdx) == MESH_Hatch) {
			return damageAmount;
		}

		if (g_flightDifficulty == 1) {
			damageAmount >>= 2;
		} else if (g_flightDifficulty == 0) {
			damageAmount >>= 1;
		}
	}
	if (damageAmount == 0) {
		damageAmount = 1;
	}

	if (g_missionFormatVersion >= 14 && g_objectTable[objIdx].objectType == OBJ_SuperStarDestroyer &&
		ModelMesh_GetObjectTypeMeshType(OBJ_SuperStarDestroyer, componentIdx) == MESH_Bridge) {
		int meshCount;
		int liveShieldGeneratorCount;
		int meshIdx;

		meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[objIdx].objectType);
		liveShieldGeneratorCount = 0;
		for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
			if (ModelMesh_GetObjectTypeMeshType(g_objectTable[objIdx].objectType, meshIdx) ==
					MESH_ShieldGenerator &&
				g_curCraft->componentHp[meshIdx] != 0) {
				++liveShieldGeneratorCount;
			}
		}

		if (liveShieldGeneratorCount != 0 || g_curCraft->shieldFront != 0 || g_curCraft->shieldRear != 0 ||
			sourceObjIdx == 0xffffu) {
			return damageAmount;
		}

		if (g_objectTable[sourceObjIdx].objectType == OBJ_Corvette2) {
			int hullMax;
			int hullDamage;
			int componentHp;

			hullMax = g_curCraft->hullMax;
			hullDamage = g_curCraft->hullDamage;
			componentHp = g_curCraft->componentHp[componentIdx];
			damageAmount =
				(unsigned int)(hullMax - 5 * (int)((uint32_t)hullMax / 100u) - hullDamage + 16 * componentHp);
		} else if (g_objectTable[sourceObjIdx].objectType == OBJ_AWing &&
				   g_objectTable[sourceObjIdx].mobj->pCraft->objectKind == 3) {
			int hullMax;
			int hullDamage;
			int componentHp;

			hullMax = g_curCraft->hullMax;
			hullDamage = g_curCraft->hullDamage;
			componentHp = g_curCraft->componentHp[componentIdx];
			damageAmount = (unsigned int)(hullMax - hullDamage + 16 * componentHp);
		} else {
			return damageAmount;
		}
	}

	{
		int remainingHp;

		remainingHp = (16 * g_curCraft->componentHp[componentIdx] - (int)damageAmount) >> 4;
		if (remainingHp <= 0) {
			remainingHp = 0;
		}
		g_curCraft->componentHp[componentIdx] = (uint8_t)remainingHp;
	}

	damageAmount = 0;
	if (g_curCraft->componentHp[componentIdx] != 0) {
		return damageAmount;
	}

	relatedObjIdx = objIdx;

	{
		if (ModelMesh_GetObjectTypeMeshType(g_objectTable[relatedObjIdx].objectType, componentIdx) ==
			MESH_ShieldGenerator) {
			int liveShieldGeneratorCount;
			int meshCount;
			int meshIdx;

			liveShieldGeneratorCount = 0;
			meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[relatedObjIdx].objectType);
			for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
				if (meshIdx != componentIdx &&
					ModelMesh_GetObjectTypeMeshType(g_objectTable[relatedObjIdx].objectType, meshIdx) ==
						MESH_ShieldGenerator &&
					g_curCraft->componentHp[meshIdx] != 0) {
					++liveShieldGeneratorCount;
				}
			}
			if (liveShieldGeneratorCount == 0) {
				g_curCraft->shieldRear = 0;
				g_curCraft->shieldFront = 0;
			}
		}

		if (g_objectTable[relatedObjIdx].objectType == OBJ_ImperialResearchCenter &&
			ModelMesh_GetObjectTypeMeshType(OBJ_ImperialResearchCenter, componentIdx) ==
				MESH_EnergyGenerator) {
			g_curCraft->shieldFront = 0;
			damageAmount = (unsigned int)(g_curCraft->hullMax - g_curCraft->hullDamage +
										  16 * g_curCraft->componentHp[componentIdx]);
		}

		if (!ModelMesh_IsObjectTypeMeshDamageable(g_objectTable[relatedObjIdx].objectType, componentIdx)) {
			return damageAmount;
		}

		g_curCraft->componentState[componentIdx] = 4;
		for (playerIdx = 0; playerIdx < 8; ++playerIdx) {
			PlayerData* player;

			player = &g_players[playerIdx];
			if (player->connectedFlag != 0 && objIdx == (uint16_t)player->currentTargetObjectIdx &&
				componentIdx == (uint16_t)player->selectedTargetComponent) {
				MeshType oldMeshType;
				int meshCount;

				oldMeshType = ModelMesh_GetObjectTypeMeshType(g_objectTable[relatedObjIdx].objectType,
															  player->selectedTargetComponent);
				meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[relatedObjIdx].objectType);

				do {
					++player->selectedTargetComponent;
					if ((uint16_t)player->selectedTargetComponent >= meshCount) {
						player->selectedTargetComponent = 0;
					}

					if (g_curCraft->componentState[(uint16_t)player->selectedTargetComponent] == 0 &&
						Craft_IsSelectableDamageComponentMesh(g_objectTable[relatedObjIdx].objectType,
															  (uint16_t)player->selectedTargetComponent) &&
						ModelMesh_GetObjectTypeMeshType(g_objectTable[relatedObjIdx].objectType,
														(uint16_t)player->selectedTargetComponent) ==
							oldMeshType) {
						break;
					}
				} while ((uint16_t)player->selectedTargetComponent != componentIdx);

				if ((uint16_t)player->selectedTargetComponent == componentIdx) {
					do {
						++player->selectedTargetComponent;
						if ((uint16_t)player->selectedTargetComponent >= meshCount) {
							player->selectedTargetComponent = 0;
						}

						if (g_curCraft->componentState[(uint16_t)player->selectedTargetComponent] == 0 &&
							Craft_IsSelectableDamageComponentMesh(
								g_objectTable[relatedObjIdx].objectType,
								(uint16_t)player->selectedTargetComponent)) {
							break;
						}
					} while ((uint16_t)player->selectedTargetComponent != componentIdx);
				}
			}
		}

		{
			uint16_t explosionObjIdx;

			explosionObjIdx = Object_AllocSlotForGenus(GENUS_Explosion);
			if (explosionObjIdx != 0xffffu) {
				int centerY;
				int centerX;
				int sfxSlot;

				g_objectTable[explosionObjIdx].world_x = g_objectTable[relatedObjIdx].world_x;
				g_objectTable[explosionObjIdx].world_y = g_objectTable[relatedObjIdx].world_y;
				g_objectTable[explosionObjIdx].world_z = g_objectTable[relatedObjIdx].world_z;

				centerX = ModelMesh_GetCenterX(g_objectTable[relatedObjIdx].objectType, componentIdx);
				centerY = ModelMesh_GetCenterY(g_objectTable[relatedObjIdx].objectType, componentIdx);
				g_rotatedY = ModelMesh_GetCenterZ(g_objectTable[relatedObjIdx].objectType, componentIdx);
				g_rotatedX = centerX;
				g_rotatedZ = -centerY;
				pai_RotateLocalVectorToWorldScratch(&g_objectTable[relatedObjIdx], centerX, g_rotatedY,
													-centerY);
				g_objectTable[explosionObjIdx].world_x += g_rotatedX;
				g_objectTable[explosionObjIdx].world_y += g_rotatedY;
				g_objectTable[explosionObjIdx].world_z += g_rotatedZ;

				g_objectTable[explosionObjIdx].objectType =
					(ObjectTypeId)(OBJ_ExplosionTextureGroup2001 + (GameRand() & 3u));
				g_objectTable[explosionObjIdx].genusId = GENUS_Explosion;
				g_objectTable[explosionObjIdx].mobj->state = 5;
				g_objectTable[explosionObjIdx].typeSpecificByte[0] = 1;
				g_objectTable[explosionObjIdx].mobj->framesAlive = 0;
				g_objectTable[explosionObjIdx].mobj->lifetimeTimer = 0;
				g_objectTable[explosionObjIdx].mobj->sourceObjIdx = -1;
				g_objectTable[explosionObjIdx].mobj->speed = g_objectTable[relatedObjIdx].mobj->speed;
				g_objectTable[explosionObjIdx].pitch = g_objectTable[relatedObjIdx].pitch;
				g_objectTable[explosionObjIdx].yaw = g_objectTable[relatedObjIdx].yaw;
				g_objectTable[explosionObjIdx].roll = 0;
				g_objectTable[explosionObjIdx].angleD = 0;
				g_objectTable[explosionObjIdx].mobj->orientMatrixDirty = 1;
				g_objectTable[explosionObjIdx].mobj->moveVectorDirty = 1;
				sfxSlot = fsfx_PickRandomSmallExplosionSfx();
				fsfx_PlaySound(sfxSlot, explosionObjIdx, (unsigned int)g_localPlayer);
				ForceFeedback_PlayProximityEffectForObject(1, explosionObjIdx);
				g_objectTable[explosionObjIdx].mobj->instanceExtent =
					ModelMesh_GetComponentMaxExtent(g_objectTable[relatedObjIdx].objectType, componentIdx);

				if (g_useHardware3D) {
					int minX;
					int minY;
					int maxX;
					int maxY;

					g_savedCollisionSegmentStartWorldY = g_collisionSegmentStartWorldY;
					g_savedCollisionSegmentStartWorldX = g_collisionSegmentStartWorldX;
					g_savedCollisionSegmentStartWorldZ = g_collisionSegmentStartWorldZ;
					g_savedCollisionProbeWorldY = g_collisionProbeWorldY;
					g_savedCollisionProbeWorldX = g_collisionProbeWorldX;
					g_savedCollisionProbeWorldZ = g_collisionProbeWorldZ;
					g_savedCollisionSweepStartY = g_collisionSweepStartY;
					g_savedCollisionSweepStartX = g_collisionSweepStartX;
					g_savedCollisionSweepStartZ = g_collisionSweepStartZ;
					g_savedCollisionSweepEndY = g_collisionSweepEndY;
					g_savedCollisionSweepEndX = g_collisionSweepEndX;
					g_savedCollisionSweepEndZ = g_collisionSweepEndZ;
					g_savedCollisionHitOffsetY = g_collisionHitOffsetY;
					g_savedCollisionHitOffsetX = g_collisionHitOffsetX;
					g_savedCollisionHitOffsetZ = g_collisionHitOffsetZ;

					minX = ModelMesh_GetBoundsMinX(g_objectTable[relatedObjIdx].objectType, componentIdx);
					minY = ModelMesh_GetBoundsMinY(g_objectTable[relatedObjIdx].objectType, componentIdx);
					g_rotatedY =
						ModelMesh_GetBoundsMinZ(g_objectTable[relatedObjIdx].objectType, componentIdx);
					g_rotatedX = minX;
					g_rotatedZ = -minY;
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[relatedObjIdx], minX, g_rotatedY,
														-minY);
					g_collisionSegmentStartWorldX = g_rotatedX + g_objectTable[relatedObjIdx].world_x;
					g_collisionSegmentStartWorldY = g_rotatedY + g_objectTable[relatedObjIdx].world_y;
					g_collisionSegmentStartWorldZ = g_rotatedZ + g_objectTable[relatedObjIdx].world_z;

					maxX = ModelMesh_GetBoundsMaxX(g_objectTable[relatedObjIdx].objectType, componentIdx);
					maxY = ModelMesh_GetBoundsMaxY(g_objectTable[relatedObjIdx].objectType, componentIdx);
					g_rotatedY =
						ModelMesh_GetBoundsMaxZ(g_objectTable[relatedObjIdx].objectType, componentIdx);
					g_rotatedX = maxX;
					g_rotatedZ = -maxY;
					pai_RotateLocalVectorToWorldScratch(&g_objectTable[relatedObjIdx], maxX, g_rotatedY,
														-maxY);
					g_collisionProbeWorldX = g_rotatedX + g_objectTable[relatedObjIdx].world_x;
					g_collisionProbeWorldY = g_rotatedY + g_objectTable[relatedObjIdx].world_y;
					g_collisionProbeWorldZ = g_rotatedZ + g_objectTable[relatedObjIdx].world_z;

					if (collide_CheckSweptModelCollision(objIdx, objIdx) > 0) {
						float maxBoundsSize;

						maxBoundsSize = (float)ModelMesh_GetBoundsSizeX(
							g_objectTable[relatedObjIdx].objectType, componentIdx);
						if ((float)ModelMesh_GetBoundsSizeY(g_objectTable[relatedObjIdx].objectType,
															componentIdx) < maxBoundsSize) {
							maxBoundsSize = (float)ModelMesh_GetBoundsSizeX(
								g_objectTable[relatedObjIdx].objectType, componentIdx);
						} else {
							maxBoundsSize = (float)ModelMesh_GetBoundsSizeY(
								g_objectTable[relatedObjIdx].objectType, componentIdx);
						}
						if ((float)ModelMesh_GetBoundsSizeZ(g_objectTable[relatedObjIdx].objectType,
															componentIdx) < maxBoundsSize) {
							maxBoundsSize = (float)ModelMesh_GetBoundsSizeX(
								g_objectTable[relatedObjIdx].objectType, componentIdx);
							if ((float)ModelMesh_GetBoundsSizeY(g_objectTable[relatedObjIdx].objectType,
																componentIdx) < maxBoundsSize) {
								maxBoundsSize = (float)ModelMesh_GetBoundsSizeX(
									g_objectTable[relatedObjIdx].objectType, componentIdx);
							} else {
								maxBoundsSize = (float)ModelMesh_GetBoundsSizeY(
									g_objectTable[relatedObjIdx].objectType, componentIdx);
							}
						} else {
							maxBoundsSize = (float)ModelMesh_GetBoundsSizeZ(
								g_objectTable[relatedObjIdx].objectType, componentIdx);
						}

						if (g_objRenderState[relatedObjIdx].drawnThisFrame) {
							ParticleEffect* effect;

							Particle_AttachEffectToObject(11, objIdx, &g_glowMarkPlaneScratch.center,
														  g_glowMarkScratchNormalVec);
							effect = Particle_AttachEffectToObject(8, objIdx, &g_glowMarkPlaneScratch.center,
																   g_glowMarkScratchNormalVec);
							if (effect != NULL) {
								effect->yawRandomRad = 0.52359873f;
								effect->pitchRandomRad = 0.52359873f;
							}
						}

						maxBoundsSize = maxBoundsSize * 0.75f;
						GlowMark_QueueRequest(objIdx, objIdx, OBJ_BlastMarkTextureGroup3050, maxBoundsSize,
											  maxBoundsSize);
					}

					g_collisionSegmentStartWorldX = g_savedCollisionSegmentStartWorldX;
					g_collisionSegmentStartWorldY = g_savedCollisionSegmentStartWorldY;
					g_collisionSegmentStartWorldZ = g_savedCollisionSegmentStartWorldZ;
					g_collisionProbeWorldX = g_savedCollisionProbeWorldX;
					g_collisionProbeWorldY = g_savedCollisionProbeWorldY;
					g_collisionProbeWorldZ = g_savedCollisionProbeWorldZ;
					g_collisionSweepStartX = g_savedCollisionSweepStartX;
					g_collisionSweepStartY = g_savedCollisionSweepStartY;
					g_collisionSweepStartZ = g_savedCollisionSweepStartZ;
					g_collisionSweepEndX = g_savedCollisionSweepEndX;
					g_collisionSweepEndY = g_savedCollisionSweepEndY;
					g_collisionSweepEndZ = g_savedCollisionSweepEndZ;
					g_collisionHitOffsetX = g_savedCollisionHitOffsetX;
					g_collisionHitOffsetY = g_savedCollisionHitOffsetY;
					g_collisionHitOffsetZ = g_savedCollisionHitOffsetZ;
				}
			}
		}
	}

	if (sourceObjIdx != g_players[g_localPlayer].objectIndex) {
		return damageAmount;
	}

	switch (ModelMesh_GetObjectTypeMeshType(g_objectTable[relatedObjIdx].objectType, componentIdx)) {
		case MESH_ShieldGenerator:
			fsfx_speakorderack(g_localPlayer, -1, 40, 3, relatedObjIdx, 0xffffu);
			break;

		case MESH_GunTurret:
		case MESH_SmallGun:
		case MESH_RotaryGunTurret:
			fsfx_speakorderack(g_localPlayer, -1, 40, 2, relatedObjIdx, 0x4000u);
			break;

		case MESH_Launcher:
		case MESH_RotaryLauncher:
			fsfx_speakorderack(g_localPlayer, -1, 40, 1, relatedObjIdx, 0xffffu);
			break;

		case MESH_CommunicationSystem:
		case MESH_BeamSystem:
		case MESH_RotaryCommSystem:
		case MESH_RotaryBeamSystem:
			fsfx_speakorderack(g_localPlayer, -1, 40, 4, relatedObjIdx, 0xffffu);
			break;

		default:
			break;
	}

	return damageAmount;
}

// FUNCTION: XWA 0x41DB40
void Craft_DetachDamageableComponent(uint16_t sourceObjIdx, int detachAll, uint16_t componentIdx) {
	CraftData* sourceCraft;
	int meshCount;
	uint16_t meshIdx;

	fsfx_PlaySound(33, sourceObjIdx, (unsigned int)g_localPlayer);

	meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[sourceObjIdx].objectType);
	if ((uint16_t)meshCount <= 1u) {
		return;
	}

	sourceCraft = g_objectTable[sourceObjIdx].mobj->pCraft;
	for (meshIdx = 0; meshIdx < (uint16_t)meshCount; meshIdx++) {
		uint16_t detachedObjIdx;
		MobileObject* detachedMobj;
		uint16_t spinRate;
		uint16_t yawDelta;
		uint16_t pitchDelta;

		if (componentIdx != 0xffffu) {
			meshIdx = componentIdx;
		}

		if (sourceCraft != NULL && sourceCraft->componentState[meshIdx] == 0 &&
			ModelMesh_IsObjectTypeMeshDamageable(g_objectTable[sourceObjIdx].objectType, meshIdx)) {
			detachedObjIdx = Object_SpawnDetachedComponent(sourceObjIdx, (uint8_t)meshIdx);
			if (detachedObjIdx != 0xffffu) {
				if ((uint16_t)detachAll != 0u) {
					spinRate = GameRand();
					yawDelta = GameRand();
					pitchDelta = GameRand();
				} else {
					spinRate = (GameRand() & 0x3fff) + 0x4000;
					yawDelta = (GameRand() & 0x07ff) + 1024;
					pitchDelta = (GameRand() & 0x0fff) + 1024;
					if (GameRand() & 1) {
						spinRate = -spinRate;
						yawDelta = -yawDelta;
					}
					if (GameRand() & 1) {
						pitchDelta = -pitchDelta;
					}
				}

				detachedMobj = g_objectTable[detachedObjIdx].mobj;
				detachedMobj->speed += (GameRand() & 0x3f) + 18;
				g_objectTable[detachedObjIdx].mobj->rollImpulseRate = 0;
				g_objectTable[detachedObjIdx].mobj->spinRate = (int16_t)spinRate;
				g_objectTable[detachedObjIdx].mobj->spinRateFrac = 0;
				g_objectTable[detachedObjIdx].mobj->renderOffsetX =
					ModelMesh_GetComponentFocusX(g_objectTable[sourceObjIdx].objectType, meshIdx);
				g_objectTable[detachedObjIdx].mobj->renderOffsetY =
					ModelMesh_GetComponentFocusY(g_objectTable[sourceObjIdx].objectType, meshIdx);
				g_objectTable[detachedObjIdx].mobj->renderOffsetZ =
					ModelMesh_GetComponentFocusZ(g_objectTable[sourceObjIdx].objectType, meshIdx);
				MobileObject_SetRandomSpinAxis(detachedObjIdx);
				g_objectTable[detachedObjIdx].yaw += yawDelta;
				g_objectTable[detachedObjIdx].pitch += pitchDelta;
				if (g_objectTable[detachedObjIdx].pitch >= 0x8000u) {
					g_objectTable[detachedObjIdx].pitch = -g_objectTable[detachedObjIdx].pitch;
					g_objectTable[detachedObjIdx].yaw += 0x8000u;
				}
				g_objectTable[detachedObjIdx].mobj->orientMatrixDirty = 1;
				g_objectTable[detachedObjIdx].mobj->moveVectorDirty = 1;
				{
					uint16_t lifetimeRand = GameRand();

					if ((lifetimeRand & 0xe000u) == 0xe000u) {
						g_objectTable[detachedObjIdx].mobj->lifetimeTimer = lifetimeRand % 7080 + 20;
					} else {
						g_objectTable[detachedObjIdx].mobj->lifetimeTimer = lifetimeRand % 1416 + 1;
					}
				}

				if (g_useHardware3D && g_flightSideEffectsEnabled &&
					g_objRenderState[sourceObjIdx].drawnThisFrame) {
					Vec3f* outVertex;

					ModelMesh_PickRandomVertex(g_objectTable[sourceObjIdx].objectType, meshIdx, &outVertex);
					Particle_AttachEffectToObject(3, detachedObjIdx, outVertex, NULL);
				}

				sourceCraft->componentState[meshIdx] = 4;
				if (g_objectTable[detachedObjIdx].mobj != NULL &&
					g_objectTable[detachedObjIdx].mobj->pCraft != NULL) {
					g_objectTable[detachedObjIdx]
						.mobj->pCraft->componentState[ModelMesh_GetObjectTypeMeshCount(
							g_objectTable[sourceObjIdx].objectType)] = 4;
				}
				if ((uint16_t)detachAll == 0u) {
					return;
				}
			}
		}

		if (componentIdx != 0xffffu) {
			return;
		}
	}
}

// FUNCTION: XWA 0x4E10B0
// Spawn main-hull explosion effects for a craft and apply nearby damage/SFX. Collects
// up to 16 MESH_MainHull meshes: forced mode explodes the first hull mesh (and applies
// proximity damage from the craft's own damage value); otherwise it randomly picks a
// still-intact hull mesh and attaches an explosion + particle effect. Only fires on a
// 1-in-~4 random roll unless forced. genus 18 objects skip the proximity damage.
void Craft_SpawnMainHullExplosionEffects(uint16_t sourceObjIdx, uint16_t forceAtFirstHullMesh) {
	uint16_t result;
	uint16_t objectType;
	uint16_t genusId;
	int meshCount;
	uint8_t hullMeshes[16];
	uint16_t hullMeshCount;
	uint16_t meshIdx;

	result = GameRand();
	if (result >= 0x1FFFu && !forceAtFirstHullMesh)
		return;

	g_curCraft = g_objectTable[sourceObjIdx].mobj->pCraft;
	objectType = g_objectTable[sourceObjIdx].objectType;
	genusId = g_objectTable[sourceObjIdx].genusId;
	if (g_objectTable[sourceObjIdx].mobj->orientMatrixDirty)
		FVIEW_SetObjectTransform(g_objectTable[sourceObjIdx].roll, g_objectTable[sourceObjIdx].pitch,
								 g_objectTable[sourceObjIdx].yaw, g_objectTable[sourceObjIdx].angleD,
								 &g_objectTable[sourceObjIdx]);

	meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
	hullMeshCount = 0;
	for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
		if (ModelMesh_GetObjectTypeMeshType((uint16_t)objectType, meshIdx) == MESH_MainHull) {
			hullMeshes[hullMeshCount++] = (uint8_t)meshIdx;
		}
		if (hullMeshCount == 16)
			break;
	}
	if (!hullMeshCount) {
		hullMeshes[0] = 0;
		hullMeshCount = 1;
	}

	if (forceAtFirstHullMesh) {
		unsigned int explosionIdx = Craft_SpawnExplosionObjectAtMesh(
			sourceObjIdx, hullMeshes[0], g_modelTypeTable[(uint16_t)objectType].maxBoundsExtent >> 4, 0);
		fsfx_PlaySound(24, sourceObjIdx, g_localPlayer);
		if (explosionIdx != 0xFFFF && genusId != 18) {
			if (g_objectTable[sourceObjIdx].objectType == OBJ_XiytiarTransport) {
				collide_ApplyDefaultProximityDamage(explosionIdx, 0x3E8u, sourceObjIdx);
				return;
			}
			collide_ApplyDefaultProximityDamage(explosionIdx, g_objectTable[sourceObjIdx].mobj->damageAmount,
												sourceObjIdx);
		}
		return;
	}

	result = hullMeshes[(uint16_t)((uint16_t)GameRand() % hullMeshCount)];
	if (g_curCraft->componentHp[result]) {
		result = (uint16_t)Craft_SpawnExplosionObjectAtMesh(
			sourceObjIdx, result, g_modelTypeTable[(uint16_t)objectType].maxBoundsExtent >> 4, 1);
		if (result != 0xFFFF && genusId != 18) {
			collide_ApplyDefaultProximityDamage(result, 0x7D0u, sourceObjIdx);
			fsfx_PlaySound(fsfx_PickRandomSmallExplosionSfx(), result, g_localPlayer);
			ForceFeedback_PlayProximityEffectForObject(1, result);
		}
	}
}

// FUNCTION: XWA 0x4E12E0
unsigned int Craft_SpawnExplosionObjectAtMesh(uint16_t sourceObjIdx, uint16_t meshIdx, int instanceExtent,
											  int useRandomVertex) {
	ObjectRecord* sourceObj;
	ObjectTypeId sourceObjectType;
	Vec3f* vertices;
	int vertexCount;
	int selectedVertexIdx;
	int modelType;
	int localX;
	int localY;
	int localZ;
	uint16_t explosionObjIdx;

	vertices = NULL;
	sourceObj = &g_objectTable[sourceObjIdx];
	sourceObjectType = sourceObj->objectType;
	modelType = (uint16_t)sourceObjectType;
	selectedVertexIdx = useRandomVertex;

	if (!useRandomVertex) {
		localX = ModelMesh_GetCenterX(modelType, meshIdx);
		localY = ModelMesh_GetCenterY((uint16_t)sourceObjectType, meshIdx);
		localZ = ModelMesh_GetCenterZ(modelType, meshIdx);
	} else {
		ModelMesh_GetVerticesData(modelType, meshIdx, &vertices, &vertexCount);
		if (vertices == NULL) {
			return 0xffff;
		}
		selectedVertexIdx = (uint16_t)GameRand() % vertexCount;
		localX = (int)vertices[(uint16_t)selectedVertexIdx].x;
		localY = (int)vertices[(uint16_t)selectedVertexIdx].y;
		localZ = (int)vertices[(uint16_t)selectedVertexIdx].z;
	}

	pai_RotateLocalVectorToWorldScratch(sourceObj, localX, localZ, -localY);
	explosionObjIdx = Object_AllocSlotForGenus(GENUS_Explosion);
	if (explosionObjIdx == 0xffffu) {
		return 0xffff;
	}

	localX = explosionObjIdx;
	g_objectTable[localX].world_x = sourceObj->world_x + g_rotatedX;
	g_objectTable[localX].world_y = sourceObj->world_y + g_rotatedY;
	g_objectTable[localX].world_z = sourceObj->world_z + g_rotatedZ;
	if ((uint16_t)selectedVertexIdx == 0u) {
		g_objectTable[localX].objectType = (ObjectTypeId)(OBJ_ExplosionTextureGroup2002 + (GameRand() & 3u));
	} else {
		g_objectTable[localX].objectType = (ObjectTypeId)(OBJ_ExplosionTextureGroup2000 + (GameRand() & 3u));
	}
	g_objectTable[localX].genusId = GENUS_Explosion;
	g_objectTable[localX].mobj->state = 5;
	g_objectTable[localX].typeSpecificByte[0] = 1;
	g_objectTable[localX].mobj->framesAlive = 0;
	g_objectTable[localX].mobj->lifetimeTimer = 0;
	g_objectTable[localX].mobj->instanceExtent = instanceExtent;
	g_objectTable[localX].mobj->speed = 0;
	g_objectTable[localX].mobj->sourceObjIdx = (int16_t)sourceObjIdx;
	g_objectTable[localX].pitch = 0;
	g_objectTable[localX].yaw = 0;
	g_objectTable[localX].roll = 0;
	g_objectTable[localX].angleD = 0;
	g_objectTable[localX].mobj->rollImpulseRate = 0;
	g_objectTable[localX].mobj->spinRate = 0;
	g_objectTable[localX].mobj->spinRateFrac = 0;
	g_objectTable[localX].mobj->velocityOverrideActive = 0;
	g_objectTable[localX].mobj->spinAngleQ16 = 0;
	g_objectTable[localX].mobj->orientMatrixDirty = 1;
	g_objectTable[localX].mobj->moveVectorDirty = 1;

	if (g_useHardware3D && g_flightSideEffectsEnabled && g_objRenderState[localX].drawnThisFrame) {
		uint16_t particleRand;
		Vec3f localOffset;
		Vec3f direction;

		particleRand = GameRand2();
		if (vertices == NULL) {
			ModelMesh_GetVerticesData(modelType, meshIdx, &vertices, &vertexCount);
			if (vertices == NULL) {
				return 4095;
			}
			selectedVertexIdx = (uint16_t)GameRand2() % vertexCount;
		}

		localOffset.x = vertices[(uint16_t)selectedVertexIdx].x;
		localOffset.y = vertices[(uint16_t)selectedVertexIdx].y;
		localOffset.z = vertices[(uint16_t)selectedVertexIdx].z;
		if (particleRand < 0x6000u) {
			Vec3f* normals;

			normals = NULL;
			ModelMesh_GetVertexNormalsData(modelType, meshIdx, &normals);
			if (normals != NULL) {
				direction.x = normals[(uint16_t)selectedVertexIdx].x;
				direction.y = normals[(uint16_t)selectedVertexIdx].y;
				direction.z = normals[(uint16_t)selectedVertexIdx].z;
			} else {
				float invLength;

				invLength = 1.0f / (float)sqrt((double)(localOffset.x * localOffset.x +
														localOffset.y * localOffset.y +
														localOffset.z * localOffset.z));
				direction.x = invLength * localOffset.x;
				direction.y = invLength * localOffset.y;
				direction.z = invLength * localOffset.z;
			}

			if (particleRand < 0x1000u) {
				Particle_AttachEffectToObject(8, sourceObjIdx, &localOffset, &direction);
			} else {
				Particle_AttachEffectToObject(5, sourceObjIdx, &localOffset, &direction);
			}
		} else if (particleRand < 0xa000u) {
			direction.x = 0.0f;
			direction.y = 1.0f;
			direction.z = 0.0f;
			Particle_AttachEffectToObject(0, sourceObjIdx, &localOffset, &direction);
		}
	}

	return explosionObjIdx;
}

// FUNCTION: XWA 0x42B980
void Damage_QueueCraftBillboards(uint16_t objectIndex) {
	uint16_t objectType;
	int objectTypeInt;
	uint16_t meshCount;
	uint16_t meshIdx;
	uint16_t billboardAngle;
	uint16_t haveBillboardAngle;

	haveBillboardAngle = 0;

	if (objectIndex < (uint32_t)g_activeRegionObjectSlotStart ||
		objectIndex >= (uint32_t)g_activeRegionCraftObjectSlotEnd) {
		return;
	}

	if (g_objectTable[objectIndex].mobj == NULL || g_objectTable[objectIndex].mobj->pCraft == NULL) {
		return;
	}

	objectType = g_objectTable[objectIndex].objectType;
	objectTypeInt = (uint16_t)objectType;
	meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(objectTypeInt);
	if (meshCount <= 0) {
		return;
	}

	for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
		CraftData* craft;
		int screenX;
		uint16_t frame;

		if ((uint16_t)ModelMesh_GetObjectTypeMeshType(objectTypeInt, meshIdx) == (uint16_t)MESH_Fuselage) {
			craft = g_objectTable[objectIndex].mobj->pCraft;
			if (craft->componentState[meshIdx] == 0) {
				frame = craft->componentState[49];
				if (frame != 0) {
					if (haveBillboardAngle == 0) {
						int angle;
						int matrixX;
						int matrixY;
						int matrixR0Z;
						int matrixR1Z;

						matrixR0Z = g_objViewMat_R0_Z;
						matrixR1Z = g_objViewMat_R1_Z;
#ifndef XWA_MODERN
						if (matrixR0Z < 0) {
							matrixR0Z = -matrixR0Z;
						}
						if (matrixR1Z < 0) {
							matrixR1Z = -matrixR1Z;
						}
#else
						matrixR0Z = Xwa_Abs32(matrixR0Z);
						matrixR1Z = Xwa_Abs32(matrixR1Z);
#endif
						if (matrixR0Z < matrixR1Z) {
							matrixX = g_objViewMat_R0_X;
							matrixY = g_objViewMat_R0_Y;
						} else {
							matrixX = g_objViewMat_R1_X;
							matrixY = g_objViewMat_R1_Y;
						}

						if (matrixX < 0) {
							angle = trig2_arctan(matrixY, -matrixX);
						} else {
							angle = -trig2_arctan(matrixY, matrixX);
						}
						billboardAngle = (uint16_t)angle;
						haveBillboardAngle = 1;
					}

					billboardAngle = (uint16_t)(billboardAngle + g_objectTable[objectIndex].roll);
					screenX = TRANSFM2_ProjectScreenX(viewX, viewZ);
					if (Xwa_IsProjectedCoordSigned16(screenX)) {
						int screenY;

						screenY = TRANSFM2_ProjectScreenY(viewY, viewZ);
						if (Xwa_IsProjectedCoordSigned16(screenY)) {
							uint16_t viewportHeight;
							int16_t queuedScreenX;
							int queuedScreenY;

							viewportHeight = (uint16_t)g_flightVpHeight;
							queuedScreenX = (int16_t)screenX;
							queuedScreenY = (int16_t)(2 * (viewportHeight >> 1) - screenY);
							g_objectTable[objectIndex].objectType = OBJ_AnimationTextureGroup2008;
							SceneBillboard_QueueProjectedTextured(objectIndex, frame, 256, queuedScreenX,
																  (int16_t)queuedScreenY, viewZ,
																  billboardAngle);
							g_objectTable[objectIndex].objectType = objectType;
						}
					}
				}
			}
		}
	}
}
