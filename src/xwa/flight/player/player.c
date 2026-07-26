#include "xwa/flight/player/player.h"
#include "xwa/flight/hangar.h"

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_light.h"
#include "xwa/audio/fsfx.h"
#include "xwa/flight/starfield.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/fixed.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/yard.h"
#include "xwa/flight/film.h"
#include "xwa/net/net.h"
#include "xwa/flight/object/object.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/time.h"

#include <stddef.h>
#include <string.h>

#ifndef XWA_MODERN
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#define PLAYER_OUTPUT_DEBUG_STRING g_OutputDebugStringA
#else
static void Player_OutputDebugString(const char* outputString) { DebugPrintf("%s", outputString); }
#define PLAYER_OUTPUT_DEBUG_STRING Player_OutputDebugString
#endif

// GLOBAL: XWA 0x8B94E0
PlayerData g_players[XWA_PLAYER_COUNT];
// GLOBAL: XWA 0x8C1D00
PlayerData g_localPlayerSnapshotOnFlightExit;
// GLOBAL: XWA 0x8C1CC8
int g_localPlayer;
// GLOBAL: XWA 0x77129C
uint8_t g_padlockMouseLookEnabled;
// GLOBAL: XWA 0x771298
uint8_t g_padlockMouseLookInvertPitch;
// GLOBAL: XWA 0x7712A0
uint8_t g_padlockMouseLookIgnoreNextDelta;

static inline int* Player_GetCraftShieldBank(CraftData* craft, uint16_t bank) {
	return &(&craft->shieldFront)[bank];
}

static inline int* Player_GetCraftShieldBankByOffset(CraftData* craft, int byteOffset) {
	return (int*)((char*)&craft->shieldFront + byteOffset);
}

// FUNCTION: XWA 0x506130
void Player_EmitRemotePlayerDepartedMessages(unsigned int playerIdx) {
	int connectedCount;
	unsigned int i;

	if (playerIdx != (unsigned int)g_localPlayer) {
		msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
		msg_emitInFlightMessage(MSG_PLAYER_NO_MORE, g_localPlayer);

		connectedCount = 0;
		for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
			if (g_players[i].connectedFlag == 1) {
				++connectedCount;
			}
		}

		if (g_players[g_localPlayer].connectedFlag == 1) {
			if (connectedCount == 1) {
				msg_emitInFlightMessage(MSG_1_PLAYER, g_localPlayer);
				return;
			}

			g_msgArgTable[0] = (uint16_t)connectedCount;
			msg_emitInFlightMessage(MSG_MORE_PLAYERS, g_localPlayer);
		}
	}
}

// FUNCTION: XWA 0x5079F0
void Player_ReleaseCarriedObject(unsigned int playerIdx) {
	CraftData* craft;
	int activeObjIdx;
	uint16_t carriedObjIdx;
	uint16_t invalidIndex;

	craft = NULL;
	invalidIndex = 0xffffu;
	if (g_players[playerIdx].mapCameraState) {
		activeObjIdx = g_players[playerIdx].altViewObjectIdx;
	} else {
		activeObjIdx = g_players[playerIdx].objectIndex;
	}

	if (activeObjIdx != invalidIndex) {
		craft = g_objectTable[activeObjIdx].mobj->pCraft;
	}
	if (craft->carriedObjectIndex == invalidIndex) {
		return;
	}

	carriedObjIdx = craft->carriedObjectIndex;
	craft->carriedObjectIndex = invalidIndex;
	craft->lastReleasedObjectIdx = carriedObjIdx;
	craft->aiFlight.motionScale = invalidIndex;
	craft->releaseClearTimer = 3;

	g_curCraft = g_objectTable[carriedObjIdx].mobj->pCraft;
	g_curCraft->carrierObjIdx = invalidIndex;

	if (g_provingGroundsModeActive) {
		g_objectTable[carriedObjIdx].mobj->speed = 0;
	} else {
		g_objectTable[carriedObjIdx].mobj->speed = g_objectTable[activeObjIdx].mobj->speed;
		if (g_objectTable[carriedObjIdx].mobj->speed > 5u) {
			g_objectTable[carriedObjIdx].mobj->speed =
				(uint16_t)(g_objectTable[carriedObjIdx].mobj->speed - 5u);
		}
	}

	fsfx_PlaySound(18, invalidIndex, playerIdx);
	msg_emitInFlightMessage(353, (int)playerIdx);
}

// FUNCTION: XWA 0x507B20
void Player_AutoGunnerToggle(unsigned int playerIdx) {
	unsigned int slotIdx;
	int playerObjIdx;
	CraftData* craft;

	craft = NULL;
	if (g_players[playerIdx].mapCameraState) {
		playerObjIdx = g_players[playerIdx].altViewObjectIdx;
	} else {
		playerObjIdx = g_players[playerIdx].objectIndex;
	}

	if (playerObjIdx != 0xffff) {
		MobileObject* mobj;

		mobj = g_objectTable[playerObjIdx].mobj;
		if (mobj != NULL) {
			craft = mobj->pCraft;
		}
	}

	if (craft == NULL) {
		PLAYER_OUTPUT_DEBUG_STRING("NULL Craft data pointer in AutoGunnerToggle()!\n");
		return;
	}

	if (g_objectTable[playerObjIdx].objectType == OBJ_None) {
		return;
	}

	if (g_objectTable[playerObjIdx].genusId == GENUS_Fighter) {
		ModelIndex modelIndex;

		modelIndex = (ModelIndex)GetModelIndexFromType(g_objectTable[playerObjIdx].objectType);
		if (g_modelDefs[(uint16_t)modelIndex].turretModelIndex[0] == 0 &&
			g_modelDefs[(uint16_t)modelIndex].turretModelIndex[1] == 0) {
			return;
		}
	}

	if (g_players[playerIdx].currentSeatIdx == 0) {
		if (g_players[playerIdx].currentTargetObjectIdx == 0xffffu) {
			g_players[playerIdx].turretAutoFireState = 1;
			msg_emitInFlightMessage(MSG_GUNNER_DEFENSIVE, (int)playerIdx);
		} else {
			uint16_t targetObjIdx;
			MobileObject* targetMobj;
			int targetTeam;
			int playerIff;

			targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
			playerIff = (uint16_t)g_players[playerIdx].playerIff;
			targetMobj = g_objectTable[targetObjIdx].mobj;
			if (targetMobj != NULL) {
				targetTeam = targetMobj->team;
			} else {
				targetTeam = g_missionFlightGroups[g_objectTable[targetObjIdx].flightGroupIdx].fg.team;
			}

			if (targetTeam == playerIff) {
				g_players[playerIdx].turretAutoFireState = 1;
				msg_emitInFlightMessage(MSG_GUNNER_DEFENSIVE, (int)playerIdx);
			} else {
				if (g_missionTeams[playerIff].allies[targetTeam] == 1) {
					g_players[playerIdx].turretAutoFireState = 1;
					msg_emitInFlightMessage(MSG_GUNNER_DEFENSIVE, (int)playerIdx);
				} else {
					for (slotIdx = 0; slotIdx < craft->laserSlotCount; ++slotIdx) {
						WarheadInventoryEntry* weapon;

						weapon = &craft->warheadData[slotIdx];
						if (weapon->weaponType >= 4u) {
							if (g_players[playerIdx].currentTargetObjectIdx == weapon->turretTargetObjIdx) {
								weapon->turretTargetObjIdx = -1;
								g_players[playerIdx].turretAutoFireState = 1;
								msg_emitInFlightMessage(MSG_GUNNER_DEFENSIVE, (int)playerIdx);
							} else {
								weapon->turretTargetObjIdx = g_players[playerIdx].currentTargetObjectIdx;
								weapon->turretRotBucket = (int16_t)(59 * (slotIdx & 3u));
								g_players[playerIdx].turretAutoFireState = 2;
								msg_emitInFlightMessage(MSG_GUNNER_AUTOFIRE, (int)playerIdx);
							}
						}
					}
				}
			}
		}
	} else if (g_players[playerIdx].inputDisabledFlag == 0) {
		if (g_players[playerIdx].currentTargetObjectIdx != 0xffffu) {
			g_players[playerIdx].inputDisabledFlag = 5;
			msg_emitInFlightMessage(MSG_PILOT_TARGET_FOLLOW_ON, (int)playerIdx);
		}
	} else if (g_players[playerIdx].inputDisabledFlag == 5) {
		g_players[playerIdx].inputDisabledFlag = 0;
		msg_emitInFlightMessage(MSG_PILOT_TARGET_FOLLOW_OFF, (int)playerIdx);
	}

	fsfx_PlaySound(SFX_AUTOGUNNER_TOGGLE, 0xffffu, playerIdx);
}

static __inline unsigned int Player_GetOrderLeaderPlanId(uint8_t flightGroupIdx, uint8_t regionIdx,
														 unsigned int orderSlot) {
	unsigned int order;

	order = g_missionFlightGroups[flightGroupIdx].fg.orders[4 * regionIdx + orderSlot].order;
	return g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
}

// FUNCTION: XWA 0x506CB0
void Player_HandleDockBoardCommand(unsigned int playerIdx) {
	CraftData* craft;
	unsigned int activeObjIdx;
	uint16_t targetObjIdx;
	uint8_t flightGroupIdx;
	int bestDockX;
	int bestDockY;
	int bestDockZ;

	craft = NULL;
	bestDockX = 0;
	bestDockY = 0;
	bestDockZ = 0;
	if (g_players[playerIdx].mapCameraState) {
		activeObjIdx = (unsigned int)g_players[playerIdx].altViewObjectIdx;
	} else {
		activeObjIdx = (unsigned int)g_players[playerIdx].objectIndex;
	}

	if (activeObjIdx != 0xffffu) {
		craft = g_objectTable[activeObjIdx].mobj->pCraft;
	}

	if (!g_players[playerIdx].inputDisabledFlag) {
		if (craft->carriedObjectIndex == 0xffffu) {
			ObjectRecord* objectTable;
			uint16_t rangeLimit;

			targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
			objectTable = g_objectTable;
			if (targetObjIdx != 0xffffu) {
				if ((unsigned int)targetObjIdx >= g_activeRegionObjectSlotStart &&
					(unsigned int)targetObjIdx < g_activeRegionCraftObjectSlotEnd &&
					(objectTable[targetObjIdx].objectType < OBJ_Rubble01 ||
					 objectTable[targetObjIdx].objectType > OBJ_Rubble12)) {

					if (objectTable[targetObjIdx].mobj == NULL ||
						objectTable[targetObjIdx].mobj->speed == 0) {
						if (objectTable[targetObjIdx].genusId == GENUS_Starship ||
							objectTable[targetObjIdx].genusId == GENUS_Platform) {
							rangeLimit = 0xc000u;
						} else {
							rangeLimit = 0x3000u;
						}
						pai_ObjectRefDirectionToObjectRef(targetObjIdx, activeObjIdx);
						if ((unsigned int)trig2_polardistance < rangeLimit) {
							AiController* aiController;
							unsigned int orderSlot;

							objectTable = g_objectTable;
							flightGroupIdx = objectTable[activeObjIdx].flightGroupIdx;
							orderSlot = 0;
							do {
								unsigned int planId;

								objectTable = g_objectTable;
								planId = Player_GetOrderLeaderPlanId(
									flightGroupIdx, objectTable[activeObjIdx].regionIdx, orderSlot);
								if (pai_IsBoardingPlanId(planId)) {
									g_paiContext.curOrderCoord.fields.orderSlot = (uint16_t)orderSlot;
									g_paiContext.curOrderCoord.fields.regionIdx =
										g_objectTable[activeObjIdx].regionIdx;
									g_paiContext.curOrderCoord.fields.flightGroupIdx = flightGroupIdx;
									if (pai_CurrentOrderTargetsMatchObject(targetObjIdx)) {
										break;
									}
								}
								++orderSlot;
							} while (orderSlot < 4u);

							aiController = pai_GetEffectiveAIController(craft);
							if (orderSlot < 4u) {
								aiController->currentOrderSlot = (char)orderSlot;
								aiController->currentPlanId = Player_GetOrderLeaderPlanId(
									flightGroupIdx, g_objectTable[activeObjIdx].regionIdx, orderSlot);
								g_players[playerIdx].inputDisabledFlag = 1;
							} else {
								g_players[playerIdx].inputDisabledFlag = 3;
							}
							aiController->maneuverMode = 18;
							aiController->maneuverPhase = 0;
							aiController->aiPlanState = 0;
							aiController->targetObjIdx = targetObjIdx;
							aiController->targetSignature = g_objectTable[targetObjIdx].objectSignature;
							aiController->hasLiveTarget = 1;
							craft->throttleSpeed = 0;
							craft->aiFlight.rollAccel = 0x4000;
							craft->aiFlight.turnAccel = 0x4000;
							craft->aiFlight.pitchAccel = 0x4000;
							msg_emitInFlightMessage(MSG_INITIATE_BOARDING, (int)playerIdx);
							return;
						}

						fsfx_PlaySound(63, 0xffffu, playerIdx);
						if (g_objectTable[targetObjIdx].genusId != GENUS_Starship &&
							g_objectTable[targetObjIdx].genusId != GENUS_Platform) {
							msg_emitInFlightMessage(MSG_DOCK_TOO_FAR_AWAY1, (int)playerIdx);
							return;
						}
					} else {
						fsfx_PlaySound(63, 0xffffu, playerIdx);
						msg_emitInFlightMessage(MSG_DOCK_NOT_STATIONARY, (int)playerIdx);
						return;
					}
				} else {
					fsfx_PlaySound(63, 0xffffu, playerIdx);
					msg_emitInFlightMessage(MSG_DOCK_NOT_VALID, (int)playerIdx);
					return;
				}
			} else {
				fsfx_PlaySound(63, 0xffffu, playerIdx);
				msg_emitInFlightMessage(MSG_DOCK_NOT_VALID, (int)playerIdx);
				return;
			}
		} else {
			ObjectRecord* objectTable;

			targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;

			if (targetObjIdx == 0xffffu || (unsigned int)targetObjIdx < g_activeRegionObjectSlotStart ||
				(unsigned int)targetObjIdx >= g_activeRegionCraftObjectSlotEnd) {
				fsfx_PlaySound(63, 0xffffu, playerIdx);
				msg_emitInFlightMessage(MSG_DOCK_NOT_VALID, (int)playerIdx);
				return;
			}
			objectTable = g_objectTable;
			if (objectTable[targetObjIdx].mobj->speed == 0) {
				pai_ObjectRefDirectionToObjectRef(targetObjIdx, activeObjIdx);
				if ((unsigned int)trig2_polardistance < 0xa000u) {

					{
						ModelIndex modelIndex;
						uint16_t dockPointCount;
						uint16_t bestDockPointIdx;
						uint32_t bestRangeScore;
						unsigned int dockIdx;

						objectTable = g_objectTable;
						modelIndex =
							g_modelTypeTable[(uint16_t)objectTable[targetObjIdx].objectType].modelIndex;
						dockPointCount = g_modelDefs[modelIndex].dockPointCount;
						if (dockPointCount != 0) {

							bestDockPointIdx = 0xffffu;
							bestRangeScore = UINT32_MAX;
							g_collisionSegmentStartWorldX = objectTable[activeObjIdx].world_x;
							g_collisionSegmentStartWorldY = objectTable[activeObjIdx].world_y;
							g_collisionSegmentStartWorldZ = objectTable[activeObjIdx].world_z;

							for (dockIdx = 0; dockIdx < dockPointCount; ++dockIdx) {
								int dockWorldX;
								int dockWorldY;
								int dockWorldZ;
								uint32_t rangeScore;
								uint8_t occupied;
								uint32_t objIdx;

								objectTable = g_objectTable;
								dockWorldX = objectTable[targetObjIdx].world_x +
											 g_modelDefs[modelIndex].dockPoints[dockIdx].x;
								dockWorldY = objectTable[targetObjIdx].world_y +
											 g_modelDefs[modelIndex].dockPoints[dockIdx].y;
								dockWorldZ = objectTable[targetObjIdx].world_z +
											 g_modelDefs[modelIndex].dockPoints[dockIdx].z;
								rangeScore = (uint32_t)collide_roughdistance3d(
									dockWorldX - objectTable[activeObjIdx].world_x,
									dockWorldY - objectTable[activeObjIdx].world_y,
									dockWorldZ - objectTable[activeObjIdx].world_z);
								g_targetRangeScore = (int)rangeScore;
								if (rangeScore >= bestRangeScore) {
									continue;
								}

								occupied = 0;
								for (objIdx = g_activeRegionObjectSlotStart;
									 objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
									objectTable = g_objectTable;
									if (objectTable[objIdx].objectType != OBJ_None &&
										objIdx != targetObjIdx) {
										AiController* scanAi;

										scanAi =
											pai_GetEffectiveAIController(objectTable[objIdx].mobj->pCraft);
										if (dockWorldX == scanAi->aimPointX &&
											dockWorldY == scanAi->aimPointY &&
											dockWorldZ == scanAi->aimPointZ) {
											occupied = 1;
											break;
										}
									}
								}

								if (!occupied) {
									g_collisionProbeWorldX = dockWorldX;
									g_collisionProbeWorldY = dockWorldY;
									g_collisionProbeWorldZ = dockWorldZ + 16;
									if (!collide_CheckSweptModelCollision(targetObjIdx, targetObjIdx)) {
										bestDockPointIdx = dockIdx;
										bestRangeScore = (uint32_t)g_targetRangeScore;
										bestDockX = dockWorldX;
										bestDockY = dockWorldY;
										bestDockZ = dockWorldZ;
									}
								}
							}

							if (bestDockPointIdx != 0xffffu) {
								{
									AiController* aiController;
									unsigned int orderSlot;

									aiController = pai_GetEffectiveAIController(craft);
									objectTable = g_objectTable;
									flightGroupIdx = objectTable[activeObjIdx].flightGroupIdx;
									orderSlot = 0;
									do {
										unsigned int planId;

										planId = Player_GetOrderLeaderPlanId(
											flightGroupIdx, g_objectTable[activeObjIdx].regionIdx, orderSlot);
										if (strcmp(g_planTable[planId].name, "deliverpln") == 0) {
											g_paiContext.curOrderCoord.fields.orderSlot = (uint16_t)orderSlot;
											g_paiContext.curOrderCoord.fields.regionIdx =
												g_objectTable[activeObjIdx].regionIdx;
											g_paiContext.curOrderCoord.fields.flightGroupIdx = flightGroupIdx;
											if (pai_CurrentOrderTargetsMatchObject(
													craft->carriedObjectIndex) &&
												g_objectTable[targetObjIdx].flightGroupIdx ==
													g_missionFlightGroups[flightGroupIdx]
														.fg
														.orders[4 * g_paiContext.curOrderCoord.fields
																		.regionIdx +
																orderSlot]
														.variable1) {
												break;
											}
										}
										++orderSlot;
									} while (orderSlot < 4u);

									aiController = pai_GetEffectiveAIController(craft);
									if (orderSlot < 4u) {
										aiController->currentOrderSlot = (char)orderSlot;
										aiController->currentPlanId = Player_GetOrderLeaderPlanId(
											flightGroupIdx, g_objectTable[activeObjIdx].regionIdx, orderSlot);
										g_players[playerIdx].inputDisabledFlag = 1;
										aiController->maneuverMode = 35;
									} else {
										g_players[playerIdx].inputDisabledFlag = 3;
										aiController->maneuverMode = 18;
									}
									aiController->maneuverPhase = 0;
									aiController->aiPlanState = 0;
									aiController->targetObjIdx = targetObjIdx;
									aiController->targetSignature =
										g_objectTable[targetObjIdx].objectSignature;
									aiController->hasLiveTarget = 1;
									aiController->aimPointX = bestDockX;
									aiController->aimPointY = bestDockY;
									aiController->aimPointZ = bestDockZ;
									g_players[playerIdx].inputDisabledFlag = 4;
									craft->throttleSpeed = 0;
									craft->aiFlight.rollAccel = 0x4000;
									craft->aiFlight.turnAccel = 0x4000;
									craft->aiFlight.pitchAccel = 0x4000;
									if (aiController->maneuverMode == 35) {
										msg_emitInFlightMessage(MSG_INITIATE_DOCKING, (int)playerIdx);
										return;
									}
									msg_emitInFlightMessage(MSG_INITIATE_BOARDING, (int)playerIdx);
									return;
								}
							}

							fsfx_PlaySound(63, 0xffffu, playerIdx);
							msg_emitInFlightMessage(MSG_DOCK_NO_LOCATIONS, (int)playerIdx);
							return;
						}

						fsfx_PlaySound(63, 0xffffu, playerIdx);
						msg_emitInFlightMessage(MSG_DOCK_NO_SERVICE, (int)playerIdx);
						return;
					}
				}

				fsfx_PlaySound(63, 0xffffu, playerIdx);
			} else {
				fsfx_PlaySound(63, 0xffffu, playerIdx);
				msg_emitInFlightMessage(MSG_DOCK_NOT_STATIONARY, (int)playerIdx);
				return;
			}
		}
		msg_emitInFlightMessage(MSG_DOCK_TOO_FAR_AWAY2, (int)playerIdx);
		return;
	}

	{
		AiController* aiController;

		aiController = pai_GetEffectiveAIController(craft);
		aiController->maneuverMode = 0;
		aiController->targetObjIdx = 0xffffu;
		aiController->targetSignature = 0;
		aiController->hasLiveTarget = 0;
		g_players[playerIdx].inputDisabledFlag = 0;
		msg_emitInFlightMessage(MSG_DOCK_ABORT, (int)playerIdx);
		return;
	}
}

// FUNCTION: XWA 0x507510
void Player_HandlePickupCommand(unsigned int playerIdx) {
	CraftData* craft;
	unsigned int activeObjIdx;

	craft = NULL;
	if (g_players[playerIdx].mapCameraState) {
		activeObjIdx = (unsigned int)g_players[playerIdx].altViewObjectIdx;
	} else {
		activeObjIdx = (unsigned int)g_players[playerIdx].objectIndex;
	}

	if (activeObjIdx != 0xffffu) {
		craft = g_objectTable[activeObjIdx].mobj->pCraft;
	}

	if (!g_players[playerIdx].inputDisabledFlag) {
		uint16_t targetObjIdx;
		unsigned int targetObjIdxWide;

		targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
		if (targetObjIdx != 0xffffu) {
			targetObjIdxWide = targetObjIdx;
			if (targetObjIdxWide >= g_activeRegionObjectSlotStart &&
				targetObjIdxWide < g_activeRegionCraftObjectSlotEnd &&
				g_objectTable[targetObjIdxWide].playerOwnerIdx == -1 &&
				(g_objectTable[targetObjIdxWide].objectType < OBJ_Rubble01 ||
				 g_objectTable[targetObjIdxWide].objectType > OBJ_Rubble12)) {
				if (craft->carriedObjectIndex == 0xffffu) {
					uint16_t fetchingObjIdx;

					fetchingObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
					if ((unsigned int)fetchingObjIdx < g_activeRegionCraftObjectSlotEnd) {
						for (;;) {
							if (g_objectTable[fetchingObjIdx].objectType != OBJ_None) {
								MobileObject* mobj;

								mobj = g_objectTable[fetchingObjIdx].mobj;
								if (g_players[playerIdx].playerIff == mobj->team && mobj->pCraft != NULL) {
									AiController* aiController;

									aiController = pai_GetEffectiveAIController(mobj->pCraft);
									if (aiController->targetObjIdx == targetObjIdx &&
										aiController->maneuverMode == 18) {
										break;
									}
								}
							}

							++fetchingObjIdx;
							if ((unsigned int)fetchingObjIdx >= g_activeRegionCraftObjectSlotEnd) {
								fetchingObjIdx = 0xffffu;
								break;
							}
						}
					} else {
						fetchingObjIdx = 0xffffu;
					}

					if (fetchingObjIdx == 0xffffu) {
						unsigned int targetObjOffset;
						unsigned int targetHalfExtent;
						ObjectRecord* targetObj;

						targetObjOffset = targetObjIdxWide;
						targetObj = &g_objectTable[targetObjOffset];
						if ((targetObj->mobj == NULL || targetObj->mobj->speed == 0 ||
							 g_provingGroundsModeActive) &&
							(g_modelTypeTable[(uint16_t)targetObj->objectType].flags & 4u) == 0) {
							targetHalfExtent = (unsigned int)g_modelTypeTable[(uint16_t)targetObj->objectType]
												   .maxBoundsExtent >>
											   1;
							if (targetObj->genusId == GENUS_Fighter || targetObj->genusId == GENUS_Utility ||
								targetObj->genusId == GENUS_PilotDroid ||
								targetObj->genusId == GENUS_WeaponEmplacement ||
								(unsigned int)
										g_modelTypeTable[(uint16_t)g_objectTable[activeObjIdx].objectType]
											.maxBoundsExtent > targetHalfExtent ||
								targetObj->objectType == OBJ_PropaneTank) {
								pai_ObjectRefDirectionToObjectRef(targetObjIdxWide, activeObjIdx);
								if ((unsigned int)trig2_polardistance < 0x2000u) {
									if (g_provingGroundsModeActive) {
										CraftData* targetCraft;

										targetCraft = g_objectTable[targetObjOffset].mobj->pCraft;
										if (targetCraft != NULL && targetCraft->carrierObjIdx == 0xffffu) {
											uint16_t workingSubsystems;

											workingSubsystems = targetCraft->workingSubsystems;
											paiman_TransferObjectToAiTeam(targetObjIdxWide, targetCraft,
																		  0x80u);
											craft->carriedObjectIndex = targetObjIdx;
											targetCraft->carrierObjIdx = (uint16_t)activeObjIdx;
											targetCraft->workingSubsystems = workingSubsystems;
											msg_emitInFlightMessage(MSG_PICKUP_SECURED, (int)playerIdx);
											return;
										}

										return;
									}

									{
										AiController* aiController;
										uint16_t objectSignature;
										uint8_t order;

										aiController = pai_GetEffectiveAIController(craft);
										aiController->currentOrderSlot = 0;
										order =
											g_missionFlightGroups[g_objectTable[activeObjIdx].flightGroupIdx]
												.fg.orders[4 * g_objectTable[activeObjIdx].regionIdx]
												.order;
										aiController->currentPlanId = g_builtinPlanIdByNameIndex
											[g_orderLeaderBuiltinPlanNameIndex[order]];
										aiController->maneuverMode = 18;
										aiController->maneuverPhase = 0;
										aiController->aiPlanState = 0;
										aiController->targetObjIdx = targetObjIdx;
										objectSignature = g_objectTable[targetObjOffset].objectSignature;
										aiController->hasLiveTarget = 1;
										aiController->targetSignature = objectSignature;
										g_players[playerIdx].inputDisabledFlag = 2;
										craft->throttleSpeed = 0;
										craft->aiFlight.rollAccel = 0x4000;
										craft->aiFlight.turnAccel = 0x4000;
										craft->aiFlight.pitchAccel = 0x4000;
										msg_emitInFlightMessage(MSG_INITIATE_PICKUP, (int)playerIdx);
										return;
									}
								}

								fsfx_PlaySound(63, 0xffffu, playerIdx);
								msg_emitInFlightMessage(MSG_PICKUP_TOO_FAR_AWAY, (int)playerIdx);
								return;
							}

							fsfx_PlaySound(63, 0xffffu, playerIdx);
							msg_emitInFlightMessage(MSG_PICKUP_TOO_LARGE, (int)playerIdx);
							return;
						}

						fsfx_PlaySound(63, 0xffffu, playerIdx);
						msg_emitInFlightMessage(MSG_PICKUP_NOT_STATIONARY, (int)playerIdx);
						return;
					}

					fsfx_PlaySound(63, 0xffffu, playerIdx);
					msg_addMessagePtr(
						0, g_missionFlightGroups[g_objectTable[fetchingObjIdx].flightGroupIdx].fg.name);
					msg_emitInFlightMessage(MSG_ALREADY_TARGETED, (int)playerIdx);
					return;
				}

				fsfx_PlaySound(63, 0xffffu, playerIdx);
				msg_emitInFlightMessage(MSG_PICKUP_ALREADY_CARRY, (int)playerIdx);
				return;
			}
		}

		fsfx_PlaySound(63, 0xffffu, playerIdx);
		if (targetObjIdx == 0xffffu) {
			msg_emitInFlightMessage(MSG_PICKUP_NO_TARGET, (int)playerIdx);
			return;
		}
		msg_emitInFlightMessage(MSG_PICKUP_NOT_VALID, (int)playerIdx);
		return;
	} else {
		AiController* aiController;

		aiController = pai_GetEffectiveAIController(craft);
		aiController->maneuverMode = 0;
		aiController->targetObjIdx = 0xffffu;
		aiController->targetSignature = 0;
		aiController->hasLiveTarget = 0;
		g_players[playerIdx].inputDisabledFlag = 0;
		msg_emitInFlightMessage(MSG_PICKUP_ABORTED, (int)playerIdx);
		return;
	}
}

// FUNCTION: XWA 0x508020
void Player_HandleReportInCommand(int playerIdx, int targetObjIdx) {
	int activeObjIdx;
	CraftData* craft;
	uint8_t mapCameraState;

	mapCameraState = g_players[playerIdx].mapCameraState;
	craft = NULL;
	if (mapCameraState) {
		activeObjIdx = g_players[playerIdx].altViewObjectIdx;
	} else {
		activeObjIdx = g_players[playerIdx].objectIndex;
	}

	if (activeObjIdx != 0xffff) {
		craft = g_objectTable[activeObjIdx].mobj->pCraft;
	}

	if (!mapCameraState && (craft == NULL || (craft->workingSubsystems & 0x200u) == 0)) {
		g_msgArgTable[0] = MSG_COMMUNICATIONS;
		g_msgArgTable[1] = MSG_DAMAGED;
		msg_emitInFlightMessage(MSG_SYSTEMCOND, playerIdx);
		return;
	}

	if (targetObjIdx == 0xffff) {
		uint16_t currentTargetObjIdx;
		ObjectRecord* targetObj;
		MobileObject* targetMobj;
		int targetTeam;
		uint16_t playerIff;

		currentTargetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
		if (currentTargetObjIdx != 0xffffu && currentTargetObjIdx >= g_activeRegionObjectSlotStart &&
			currentTargetObjIdx < g_activeRegionCraftObjectSlotEnd) {
			playerIff = (uint16_t)g_players[playerIdx].playerIff;
			targetObj = &g_objectTable[currentTargetObjIdx];
			targetMobj = targetObj->mobj;
			if (targetMobj != NULL) {
				targetTeam = targetMobj->team;
			} else {
				targetTeam = g_missionFlightGroups[targetObj->flightGroupIdx].fg.team;
			}

			if (targetTeam == playerIff || g_missionTeams[playerIff].allies[targetTeam] != 0) {
				if (targetObj->playerOwnerIdx == -1) {
					AiController* aiController;
					CraftData* reportCraft;
					uint16_t reportMessageId;
					uint16_t reportObjIdx;

					g_curCraft = targetObj->mobj->pCraft;
					aiController = pai_GetEffectiveAIController(g_curCraft);
					reportCraft = g_curCraft;
					reportMessageId = g_planReportMessageIdByPlanId[aiController->pendingPlanId];
					reportObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
					msg_reportmessage(reportObjIdx, reportCraft, reportMessageId);
					return;
				} else {
					int requesterIff;
					int otherPlayerIdx;
					int result;

					requesterIff = g_players[playerIdx].playerIff;
					result = g_localPlayer;
					for (otherPlayerIdx = 0; otherPlayerIdx < XWA_PLAYER_COUNT; ++otherPlayerIdx) {
						PlayerData* otherPlayer;

						otherPlayer = &g_players[otherPlayerIdx];
						if (otherPlayerIdx != playerIdx && otherPlayer->connectedFlag == 1 &&
							otherPlayer->playerIff == requesterIff &&
							otherPlayer->objectIndex == currentTargetObjIdx && otherPlayerIdx == result) {
							fsfx_PlaySound(SFX_REPORT_REQUEST, 0xffffu, (unsigned int)result);
							msg_addMessagePtr(0, NetSession_GetPlayerName(playerIdx));
							msg_emitInFlightMessage(MSG_REPORT_IN, g_localPlayer);
							result = g_localPlayer;
						}
					}
				}
			}
		}
	} else {
		ObjectRecord* targetObj;
		int targetPlayerIdx;

		targetObj = &g_objectTable[targetObjIdx];
		targetPlayerIdx = targetObj->playerOwnerIdx;
		if (targetPlayerIdx == -1) {
			AiController* aiController;

			g_curCraft = targetObj->mobj->pCraft;
			aiController = pai_GetEffectiveAIController(g_curCraft);
			msg_reportmessage(targetObjIdx, g_curCraft,
							  g_planReportMessageIdByPlanId[aiController->pendingPlanId]);
		} else {
			int requesterIff;
			int otherPlayerIdx;
			int result;

			requesterIff = g_players[playerIdx].playerIff;
			result = g_localPlayer;
			for (otherPlayerIdx = 0; otherPlayerIdx < XWA_PLAYER_COUNT; ++otherPlayerIdx) {
				PlayerData* otherPlayer;

				otherPlayer = &g_players[otherPlayerIdx];
				if (otherPlayerIdx != playerIdx && otherPlayer->connectedFlag == 1 &&
					otherPlayer->playerIff == requesterIff && otherPlayer->objectIndex == targetObjIdx &&
					otherPlayerIdx == result) {
					fsfx_PlaySound(SFX_REPORT_REQUEST, 0xffffu, (unsigned int)result);
					msg_addMessagePtr(0, NetSession_GetPlayerName(playerIdx));
					msg_emitInFlightMessage(MSG_REPORT_IN, g_localPlayer);
					result = g_localPlayer;
				}
			}
		}
	}
}

// FUNCTION: XWA 0x508860
void Player_HandleResupplyCommand(int playerIdx, int targetObjIdx) {
	CraftData* craft;
	int activeObjIdx;
	AiController* aiController;
	const char* pendingPlanName;

	craft = NULL;
	if (g_players[playerIdx].mapCameraState) {
		activeObjIdx = g_players[playerIdx].altViewObjectIdx;
	} else {
		activeObjIdx = g_players[playerIdx].objectIndex;
	}

	if (activeObjIdx != 0xffff) {
		craft = g_objectTable[activeObjIdx].mobj->pCraft;
	}

	if ((craft->workingSubsystems & 0x200u) != 0) {
		if (targetObjIdx == 0xffff) {
			uint16_t currentTargetObjIdx;

			currentTargetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
			if (currentTargetObjIdx == 0xffffu || currentTargetObjIdx < g_activeRegionObjectSlotStart ||
				currentTargetObjIdx >= g_activeRegionCraftObjectSlotEnd) {
				return;
			}

			g_curCraft = g_objectTable[currentTargetObjIdx].mobj->pCraft;
			aiController = pai_GetEffectiveAIController(g_curCraft);
			pai_setupcraftcontext((uint16_t)g_players[playerIdx].currentTargetObjectIdx);
			pendingPlanName = g_planTable[aiController->pendingPlanId].name;
			if ((strcmp(pendingPlanName, "boardtogivepln") == 0 ||
				 strcmp(pendingPlanName, "board3pln") == 0) &&
				pai_CurrentOrderTargetsMatchObject((uint16_t)activeObjIdx)) {
				aiController->candidateTargetIdx = (uint16_t)activeObjIdx;
				msg_radioMessage((uint16_t)g_players[playerIdx].currentTargetObjectIdx, g_curCraft,
								 MSG_RELOAD_ON_WAY, 0, 0);
				return;
			}
		} else {
			g_curCraft = g_objectTable[targetObjIdx].mobj->pCraft;
			aiController = pai_GetEffectiveAIController(g_curCraft);
			pai_setupcraftcontext(targetObjIdx);
			pendingPlanName = g_planTable[aiController->pendingPlanId].name;
			if ((strcmp(pendingPlanName, "boardtogivepln") == 0 ||
				 strcmp(pendingPlanName, "board3pln") == 0) &&
				pai_CurrentOrderTargetsMatchObject((uint16_t)activeObjIdx)) {
				aiController->candidateTargetIdx = (uint16_t)activeObjIdx;
				msg_radioMessage((uint16_t)g_players[playerIdx].currentTargetObjectIdx, g_curCraft,
								 MSG_RELOAD_ON_WAY, 0, 0);
				return;
			}
		}

		g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
		msg_emitInFlightMessage(MSG_NO_COMMLINK, playerIdx);
		return;
	}

	g_msgArgTable[0] = MSG_COMMUNICATIONS;
	g_msgArgTable[1] = MSG_DAMAGED;
	msg_emitInFlightMessage(MSG_SYSTEMCOND, playerIdx);
}

// FUNCTION: XWA 0x5082F0
void Player_HandleEvadeCommand(int playerIdx, int targetObjIdx) {
	CraftData* craft;
	int activeObjIdx;
	unsigned int otherPlayerIdx;

	craft = NULL;
	if (g_players[playerIdx].mapCameraState) {
		activeObjIdx = g_players[playerIdx].altViewObjectIdx;
	} else {
		activeObjIdx = g_players[playerIdx].objectIndex;
	}

	if (activeObjIdx != 0xffff) {
		craft = g_objectTable[activeObjIdx].mobj->pCraft;
	}

	if (!g_players[playerIdx].mapCameraState && (craft == NULL || (craft->workingSubsystems & 0x200u) == 0)) {
		g_msgArgTable[0] = MSG_COMMUNICATIONS;
		g_msgArgTable[1] = MSG_DAMAGED;
		msg_emitInFlightMessage(MSG_SYSTEMCOND, playerIdx);
		return;
	}

	if (targetObjIdx == 0xffff) {
		int16_t canCommand;

		canCommand =
			Player_CanRadioCommandCraft((uint16_t)g_players[playerIdx].currentTargetObjectIdx, playerIdx);
		if (canCommand) {
			AiController* aiController;

			g_curCraft = g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].mobj->pCraft;
			aiController = pai_GetEffectiveAIController(g_curCraft);
			if (strcmp(g_planTable[aiController->pendingPlanId].name, "craftwaitforgopln") == 0) {
				aiController->pendingPlanId = (uint8_t)aiController->savedPlanId;
				pai_setupcraftcontext((uint16_t)g_players[playerIdx].currentTargetObjectIdx);
				pai_ApplyPendingPlanTargetAndManeuver((uint16_t)g_players[playerIdx].currentTargetObjectIdx);
				aiController->candidateTargetIdx = 251;
				return;
			}

			aiController->candidateTargetIdx = 251;
			return;
		}

		for (otherPlayerIdx = 0; otherPlayerIdx < XWA_PLAYER_COUNT; ++otherPlayerIdx) {
			if (g_players[otherPlayerIdx].objectIndex ==
				(int)(uint16_t)g_players[playerIdx].currentTargetObjectIdx) {
				if (g_players[playerIdx].playerIff == g_players[otherPlayerIdx].playerIff) {
					g_players[otherPlayerIdx].pendingActionId = 9;
					g_players[otherPlayerIdx].pendingActionIssuerPlayerIdx = (int16_t)playerIdx;
					g_players[otherPlayerIdx].pendingActionTimer = 1416;
					if (otherPlayerIdx == g_localPlayer) {
						fsfx_PlaySound(127, 0xffffu, (unsigned int)g_localPlayer);
						msg_addMessagePtr(0, NetSession_GetPlayerName(playerIdx));
						msg_emitInFlightMessage(MSG_EVADE, g_localPlayer);
					}
				} else {
					g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
					msg_emitInFlightMessage(MSG_NO_COMMLINK, playerIdx);
				}
			}
		}
	} else {
		ObjectRecord* targetObj;

		targetObj = &g_objectTable[targetObjIdx];
		if (g_objectTable[targetObjIdx].playerOwnerIdx == -1) {
			AiController* aiController;

			g_curCraft = targetObj->mobj->pCraft;
			aiController = pai_GetEffectiveAIController(g_curCraft);
			if (strcmp(g_planTable[aiController->pendingPlanId].name, "craftwaitforgopln") == 0) {
				aiController->pendingPlanId = (uint8_t)aiController->savedPlanId;
				pai_setupcraftcontext(targetObjIdx);
				pai_ApplyPendingPlanTargetAndManeuver(targetObjIdx);
			}

			aiController->candidateTargetIdx = 251;
			return;
		} else {
			for (otherPlayerIdx = 0; otherPlayerIdx < XWA_PLAYER_COUNT; ++otherPlayerIdx) {
				if (g_players[otherPlayerIdx].objectIndex == targetObjIdx) {
					if (g_players[playerIdx].playerIff == g_players[otherPlayerIdx].playerIff) {
						g_players[otherPlayerIdx].pendingActionId = 9;
						g_players[otherPlayerIdx].pendingActionIssuerPlayerIdx = (int16_t)playerIdx;
						g_players[otherPlayerIdx].pendingActionTimer = 1416;
						if (otherPlayerIdx == g_localPlayer) {
							fsfx_PlaySound(127, 0xffffu, (unsigned int)g_localPlayer);
							msg_addMessagePtr(0, NetSession_GetPlayerName(playerIdx));
							msg_emitInFlightMessage(MSG_EVADE, g_localPlayer);
						}
					} else {
						g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
						msg_emitInFlightMessage(MSG_NO_COMMLINK, playerIdx);
					}
				}
			}
		}
	}
}

// FUNCTION: XWA 0x508AF0
void Player_HandleHyperspaceCommand(CraftData* craft, unsigned int playerIdx, char hyperMode) {
	if (g_flightSimSideEffectsSuppressed) {
		return;
	}

	if (craft->systemFlags & 0x80u) {
		uint16_t gravityWellActive;
		uint16_t objIdx;

		if (g_provingGroundsModeActive) {
			g_flightMissionEndPending = 1;
			g_players[playerIdx].connectedFlag = 2;
			return;
		}

		if ((craft->workingSubsystems & 0x80u) == 0 && !g_inHangarReady) {
			g_msgArgTable[0] = MSG_HYPERDRIVE;
			g_msgArgTable[1] = MSG_DAMAGED;
			msg_emitInFlightMessage(MSG_SYSTEMCOND, (int)playerIdx);
			return;
		}

		gravityWellActive = 0;
		for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
			if (g_objectTable[objIdx].objectType == OBJ_Interdictor2 ||
				g_objectTable[objIdx].objectType == OBJ_ModStrikeCruiser) {
				MobileObject* mobj;

				mobj = g_objectTable[objIdx].mobj;
				if ((uint8_t)mobj->iff != g_players[playerIdx].iff && mobj->pCraft->workingSubsystems) {
					gravityWellActive = 1;
				}
			}
		}

		if (gravityWellActive) {
			msg_emitInFlightMessage(MSG_NO_HYPERSPACE, (int)playerIdx);
			fsfx_PlaySound(119, 0xffffu, playerIdx);
			return;
		}

		if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
			Hud_SetHudViewState(g_players[g_localPlayer].viewState.hudStateLive, g_localPlayer);
			g_filmOverlayActive = 0;
			Hud_SyncLocalSoftwareHudMasks(1);
		}
		if (g_players[playerIdx].viewState.externalCameraActive) {
			Hud_SetHudEnabled((int)playerIdx, 1);
		}
		g_players[playerIdx].padlockActive = 0;
		g_players[playerIdx].lookYawOffset = 0;
		g_players[playerIdx].lookPitchOffset = 0;
		if ((int)playerIdx == g_localPlayer) {
			if (!g_useHardware3D) {
				Hud_EnableHudDrawElements();
			}
			Hud_ClearReadyMessageQueue();
			Flight_InitOutboundHyperspaceStreaks();
			FlightLight_SetLocalPlayerPulseEnabled(4, 0);
			FlightLight_SetLocalPlayerPulseEnabled(5, 0);
			FlightLight_SetLocalPlayerPulseEnabled(3, 1);
			g_localPlayerLightPulses[3].startTime = g_gameTime;
		}

		msg_emitInFlightMessage(MSG_HYPER_PREPARE, (int)playerIdx);
		g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_OUTBOUND;
		g_players[playerIdx].hyperspaceRuntime.targetRegionOrMode = hyperMode;
		g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks = 0;
		g_players[playerIdx].viewState.externalCameraActive = 0;
		g_players[playerIdx].viewState.playerInputBlocked = 0;
		g_players[playerIdx].viewState.cameraFocusObjIdx = g_players[playerIdx].objectIndex;
		Hud_SetHudViewState(19, (int)playerIdx);
		g_players[playerIdx].viewState.hudAimX = 0;
		g_players[playerIdx].viewState.hudAimY = 0;
		g_players[playerIdx].viewState.cameraPanDeltaX = 0;
		g_players[playerIdx].viewState.cameraPanDeltaY = 0;
		g_players[playerIdx].viewState.cameraPanDeltaZ = 0;
		g_players[playerIdx].viewState.cameraPitchDelta = 0;
		g_players[playerIdx].viewState.cameraYawDelta = 0;
		g_players[playerIdx].viewState.cameraRollDelta = 0;
		g_players[playerIdx].viewState.field_32 = 0;
		*(uint32_t*)&g_players[playerIdx].targetPresetSlot[0] = 0xffffffffu;
		*(uint32_t*)&g_players[playerIdx].targetPresetSlot[2] = 0xffffffffu;

		if (g_players[playerIdx].turretAutoFireState == 2) {
			uint16_t slotIdx;

			for (slotIdx = 0; slotIdx < craft->laserSlotCount; ++slotIdx) {
				if (craft->warheadData[slotIdx].weaponType >= 4u) {
					craft->warheadData[slotIdx].turretTargetObjIdx = -1;
				}
			}
		}

		if (!g_players[playerIdx].hasCheckpointFlag) {
			int16_t objectType;

			objectType = g_objectTable[g_players[playerIdx].objectIndex].objectType;
			if ((objectType == OBJ_XWing || objectType == OBJ_BWing) && (craft->sFoilState & 2u) == 0) {
				craft->sFoilState = (uint8_t)(craft->sFoilState | 3u);
				if ((int)playerIdx == g_localPlayer) {
					fsfx_PlaySound(120, 0xffffu, playerIdx);
				}
			}
		}

		if ((int)playerIdx == g_localPlayer) {
			return;
		}

		msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
		msg_emitInFlightMessage(MSG_INITIATING_HYPER, g_localPlayer);
		return;
	}

	{
		uint16_t alternateMothershipObjIdx;
		uint16_t departureMothershipObjIdx;
		uint16_t objIdx;
		uint8_t alternateMothershipUsed;
		uint8_t departureMethod;

		objIdx = (uint16_t)g_activeRegionObjectSlotStart;
		departureMothershipObjIdx = 0xffffu;
		alternateMothershipObjIdx = 0xffffu;
		for (; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
			departureMethod =
				g_missionFlightGroups[g_objectTable[g_players[playerIdx].objectIndex].flightGroupIdx]
					.fg.departMethod;
			alternateMothershipUsed =
				g_missionFlightGroups[g_objectTable[g_players[playerIdx].objectIndex].flightGroupIdx]
					.fg.alternateMothershipUsed;
			if (departureMethod == 1 && g_objectTable[objIdx].objectType != OBJ_None &&
				g_objectTable[objIdx].flightGroupIdx ==
					g_missionFlightGroups[g_objectTable[g_players[playerIdx].objectIndex].flightGroupIdx]
						.fg.departureMothership) {
				departureMothershipObjIdx = objIdx;
			}
			if (alternateMothershipUsed == 1 && g_objectTable[objIdx].objectType != OBJ_None &&
				g_objectTable[objIdx].flightGroupIdx ==
					g_missionFlightGroups[g_objectTable[g_players[playerIdx].objectIndex].flightGroupIdx]
						.fg.alternateMothership) {
				alternateMothershipObjIdx = objIdx;
			}
		}

		if (departureMothershipObjIdx != 0xffffu && alternateMothershipObjIdx != 0xffffu) {
			msg_formatObjectName(departureMothershipObjIdx, 0, g_flightTextScratchBuffer);
			msg_addMessagePtr(0, g_flightTextScratchBuffer);
			msg_formatObjectName(alternateMothershipObjIdx, 0, outName);
			msg_addMessagePtr(1, outName);
			msg_emitInFlightMessage(MSG_RETURN_HANGAR2, (int)playerIdx);
		} else if (departureMothershipObjIdx != 0xffffu) {
			msg_formatObjectName(departureMothershipObjIdx, 0, g_flightTextScratchBuffer);
			msg_addMessagePtr(0, g_flightTextScratchBuffer);
			msg_emitInFlightMessage(MSG_RETURN_HANGAR, (int)playerIdx);
		} else if (alternateMothershipObjIdx != 0xffffu) {
			msg_formatObjectName(alternateMothershipObjIdx, 0, g_flightTextScratchBuffer);
			msg_addMessagePtr(0, g_flightTextScratchBuffer);
			msg_emitInFlightMessage(MSG_RETURN_HANGAR, (int)playerIdx);
		}
	}
}

// FUNCTION: XWA 0x504BC0
void Player_IssueAiWingmanTargetOrder(uint16_t targetObjIdx, int wingmanObjIdx, int16_t hudMessageId,
									  uint16_t voiceVariant, int playerIdx) {
	int playerIff;
	ObjectRecord* targetObj;
	MobileObject* targetMobj;
	int targetTeam;
	int16_t commandedCount;
	uint16_t scanObjIdx;
	int objectIdx;
	uint16_t lastCommandedObjIdx;
	CraftData* craft;

	if (targetObjIdx != 0xffffu) {
		playerIff = (uint16_t)g_players[playerIdx].playerIff;
		targetObj = &g_objectTable[targetObjIdx];
		targetMobj = targetObj->mobj;
		if (targetMobj != NULL) {
			targetTeam = targetMobj->team;
		} else {
			targetTeam = g_missionFlightGroups[targetObj->flightGroupIdx].fg.team;
		}
		if (targetTeam == playerIff || g_missionTeams[playerIff].allies[targetTeam] == 1) {
			return;
		}
	}

	if (wingmanObjIdx == 0xffffu) {
		lastCommandedObjIdx = wingmanObjIdx;
		commandedCount = 0;
		scanObjIdx = g_activeRegionObjectSlotStart;
		objectIdx = (uint16_t)g_activeRegionObjectSlotStart;
		if ((uint16_t)g_activeRegionObjectSlotStart < g_activeRegionCraftObjectSlotEnd) {
			do {
				if (objectIdx != g_players[playerIdx].objectIndex) {
					ObjectRecord* scanObj;
					uint8_t scanFlightGroupIdx;

					scanObj = &g_objectTable[objectIdx];
					scanFlightGroupIdx = scanObj->flightGroupIdx;
					if (scanObj->playerOwnerIdx == -1 && scanObj->objectType != OBJ_None &&
						g_missionFlightGroups[scanFlightGroupIdx].fg.team == g_players[playerIdx].playerIff) {
						craft = scanObj->mobj->pCraft;
						if (craft->objectKind == 0) {
							int boundPlayerNumber;

							boundPlayerNumber =
								g_missionFlightGroups[(uint16_t)g_players[playerIdx].boundFlightGroupIdx]
									.fg.playerNumber -
								1;
							if (scanFlightGroupIdx == (uint16_t)g_players[playerIdx].boundFlightGroupIdx ||
								((g_missionFlightGroups[scanFlightGroupIdx].fg.globalUnit != 0 &&
								  g_missionFlightGroups[scanFlightGroupIdx].fg.globalUnit ==
									  g_missionFlightGroups[(uint16_t)g_players[playerIdx]
																.boundFlightGroupIdx]
										  .fg.globalUnit) ||
								 g_missionFlightGroups[scanFlightGroupIdx].fg.radio ==
									 boundPlayerNumber + 9)) {
								AiController* aiController;

								aiController = pai_GetEffectiveAIController(craft);
								if (hudMessageId != MSG_ACK_IGNORE_TARGET) {
									PlanRecord* pendingPlan;

									pendingPlan = &g_planTable[aiController->pendingPlanId];
									if (strcmp(pendingPlan->name, "nullpln") != 0 &&
										strcmp(pendingPlan->name, "stationaryldrpln") != 0 &&
										strcmp(pendingPlan->name, "stationaryflwpln") != 0 &&
										strcmp(pendingPlan->name, "formldr1pln") != 0 &&
										strcmp(pendingPlan->name, "formflw1pln") != 0 &&
										strcmp(pendingPlan->name, "formevadeldr1pln") != 0 &&
										strcmp(pendingPlan->name, "formevadeflw1pln") != 0 &&
										strcmp(pendingPlan->name, "flyhomeevadepln") != 0 &&
										strcmp(pendingPlan->name, "intohyperspacepln") != 0 &&
										strcmp(pendingPlan->name, "enterhangarpln") != 0) {
										if (strcmp(pendingPlan->name, "craftwaitforgopln") == 0) {
											aiController->pendingPlanId = (uint8_t)aiController->savedPlanId;
											g_curCraft = craft;
											pai_setupcraftcontext(objectIdx);
											pai_ApplyPendingPlanTargetAndManeuver(objectIdx);
										}

										aiController->candidateTargetIdx = targetObjIdx;
										if (targetObjIdx == (uint16_t)craft->playerCommandAvoidTargetObjIdx) {
											craft->playerCommandAvoidTargetObjIdx = -1;
										}
										lastCommandedObjIdx = scanObjIdx;
										++commandedCount;
									}
								} else {
									craft->playerCommandAvoidTargetObjIdx = (int16_t)targetObjIdx;
									if (aiController->candidateTargetIdx == targetObjIdx) {
										aiController->candidateTargetIdx = 0xffffu;
									}
									lastCommandedObjIdx = scanObjIdx;
									++commandedCount;
								}
							}
						}
					}
				}

				++scanObjIdx;
				objectIdx = scanObjIdx;
			} while (scanObjIdx < g_activeRegionCraftObjectSlotEnd);
		}
	} else {
		craft = g_objectTable[wingmanObjIdx].mobj->pCraft;
		if (craft->objectKind == 0) {
			AiController* aiController;

			aiController = pai_GetEffectiveAIController(craft);
			if (hudMessageId != MSG_ACK_IGNORE_TARGET) {
				PlanRecord* pendingPlan;

				pendingPlan = &g_planTable[aiController->pendingPlanId];
				if (strcmp(pendingPlan->name, "nullpln") != 0 &&
					strcmp(pendingPlan->name, "stationaryldrpln") != 0 &&
					strcmp(pendingPlan->name, "stationaryflwpln") != 0 &&
					strcmp(pendingPlan->name, "formldr1pln") != 0 &&
					strcmp(pendingPlan->name, "formflw1pln") != 0 &&
					strcmp(pendingPlan->name, "formevadeldr1pln") != 0 &&
					strcmp(pendingPlan->name, "formevadeflw1pln") != 0 &&
					strcmp(pendingPlan->name, "flyhomeevadepln") != 0 &&
					strcmp(pendingPlan->name, "intohyperspacepln") != 0 &&
					strcmp(pendingPlan->name, "enterhangarpln") != 0) {
					if (strcmp(pendingPlan->name, "craftwaitforgopln") == 0) {
						aiController->pendingPlanId = (uint8_t)aiController->savedPlanId;
						g_curCraft = craft;
						pai_setupcraftcontext(wingmanObjIdx);
						pai_ApplyPendingPlanTargetAndManeuver(wingmanObjIdx);
					}

					aiController->candidateTargetIdx = targetObjIdx;
					if (targetObjIdx == (uint16_t)craft->playerCommandAvoidTargetObjIdx) {
						craft->playerCommandAvoidTargetObjIdx = -1;
					}
				} else {
					return;
				}
			} else {
				craft->playerCommandAvoidTargetObjIdx = (int16_t)targetObjIdx;
				if (aiController->candidateTargetIdx == targetObjIdx) {
					aiController->candidateTargetIdx = 0xffffu;
				}
			}
		}
	}

	if (wingmanObjIdx == 0xffffu) {
		if (playerIdx == g_localPlayer && lastCommandedObjIdx != 0xffffu) {
			craft = g_objectTable[lastCommandedObjIdx].mobj->pCraft;
			if (commandedCount == 1) {
				msg_radioMessage(lastCommandedObjIdx, craft, hudMessageId, voiceVariant, 0);
				return;
			}
			msg_radioMessage(lastCommandedObjIdx, craft, hudMessageId, voiceVariant, 1);
		}
	} else {
		msg_radioMessage(wingmanObjIdx, g_objectTable[wingmanObjIdx].mobj->pCraft, hudMessageId, voiceVariant,
						 0);
	}
}

// FUNCTION: XWA 0x508630
void Player_HandleCoverMeCommand(int playerIdx, int targetObjIdx) {
	CraftData* craft;
	int activeObjIdx;
	uint16_t attackerObjIdx;
	unsigned int otherPlayerIdx;

	craft = NULL;
	if (g_players[playerIdx].mapCameraState) {
		activeObjIdx = g_players[playerIdx].altViewObjectIdx;
	} else {
		activeObjIdx = g_players[playerIdx].objectIndex;
	}

	if (activeObjIdx != 0xffff) {
		craft = g_objectTable[activeObjIdx].mobj->pCraft;
	}

	if ((craft->workingSubsystems & 0x200u) != 0) {
		attackerObjIdx = (uint16_t)Player_FindAttackerOfTarget((uint16_t)activeObjIdx, (int16_t)activeObjIdx);

		if (attackerObjIdx != 0xffffu) {
			if (targetObjIdx != 0xffff) {
				if (g_objectTable[targetObjIdx].playerOwnerIdx == -1) {
					Player_IssueAiWingmanTargetOrder(attackerObjIdx, targetObjIdx, MSG_ACK_COVER_ME, 9u,
													 playerIdx);
				}
			} else {
				Player_IssueAiWingmanTargetOrder(attackerObjIdx, 0xffffu, MSG_ACK_COVER_ME, 9u, playerIdx);
			}
		}

		for (otherPlayerIdx = 0; otherPlayerIdx < XWA_PLAYER_COUNT; ++otherPlayerIdx) {
			int otherObjIdx;
			unsigned int otherPlayerIff;
			unsigned int playerIff;

			if (otherPlayerIdx == playerIdx || g_players[otherPlayerIdx].connectedFlag != 1) {
				continue;
			}

			otherObjIdx = g_players[otherPlayerIdx].objectIndex;
			if (attackerObjIdx == otherObjIdx || otherObjIdx == 0xffff) {
				continue;
			}

			otherPlayerIff = (uint16_t)g_players[otherPlayerIdx].playerIff;
			playerIff = (uint16_t)g_players[playerIdx].playerIff;
			if (playerIff != otherPlayerIff && g_missionTeams[otherPlayerIff].allies[playerIff] == 0) {
				continue;
			}

			if (g_players[otherPlayerIdx].pendingActionId != 0) {
				continue;
			}

			craft = g_objectTable[otherObjIdx].mobj->pCraft;
			if (craft == NULL || (craft->workingSubsystems & 0x200u) == 0) {
				continue;
			}

			g_players[otherPlayerIdx].pendingActionId = 5;
			g_players[otherPlayerIdx].pendingActionIssuerPlayerIdx = (int16_t)playerIdx;
			if (attackerObjIdx != 0xffffu) {
				g_players[otherPlayerIdx].pendingActionParam = (int16_t)attackerObjIdx;
			} else {
				g_players[otherPlayerIdx].pendingActionParam = (int16_t)activeObjIdx;
			}

			g_players[otherPlayerIdx].pendingActionTimer = 1416;
			if (otherPlayerIdx == g_localPlayer) {
				fsfx_PlaySound(127, 0xffffu, (unsigned int)g_localPlayer);
				msg_addMessagePtr(0, NetSession_GetPlayerName(playerIdx));
				if (attackerObjIdx != 0xffffu) {
					msg_emitInFlightMessage(MSG_COVER_ME_ATTACKER, g_localPlayer);
				} else {
					msg_emitInFlightMessage(MSG_COVER_ME, g_localPlayer);
				}
			}
		}
	} else {
		g_msgArgTable[0] = MSG_COMMUNICATIONS;
		g_msgArgTable[1] = MSG_DAMAGED;
		msg_emitInFlightMessage(MSG_SYSTEMCOND, playerIdx);
	}
}

// FUNCTION: XWA 0x41EF60
int Player_BindToAvailableCraft(unsigned int playerIdx, unsigned int previousObjIdx,
								unsigned int preferredObjectSignature, int resetTargetState) {
	int savedRegionIdx;
	int originalRegionIdx;
	uint32_t selectedObjIdx;
	int searchRegionIdx;
	int regionsRemaining;
	int foundCraft;
	int matchedPreferredSignature;
	CraftData* selectedCraft;
	int linkedPlayerIdx;
	int groupIdx;
	unsigned int launcherIdx;

	g_unusedForceFeedbackPrevSpeedSnapshotLo = 0;
	g_forceFeedbackLocalSpeedSnapshot.packed = 0;
	g_unusedForceFeedbackPrevSpeedSnapshotHigh = 0;
	savedRegionIdx = regionIdx;
	g_forceFeedbackLocalSpeedSnapshotHigh = 0;

	originalRegionIdx = g_players[playerIdx].regionIndex;
	foundCraft = 0;
	matchedPreferredSignature = 0;

	if (preferredObjectSignature != 0) {
		searchRegionIdx = g_players[playerIdx].regionIndex;
		regionsRemaining = g_missionRegionCount - 1;
		if (regionsRemaining != 0) {
			do {
				Mission_SetActiveRegionObjectRanges(searchRegionIdx);
				selectedObjIdx = g_activeRegionObjectSlotStart;
				while (selectedObjIdx < g_activeRegionCraftObjectSlotEnd) {
					ObjectRecord* candidate;
					MobileObject* candidateMobj;
					uint8_t objectKind;

					candidate = &g_objectTable[selectedObjIdx];
					if (candidate->objectType != OBJ_None) {
						candidateMobj = candidate->mobj;
						if (g_missionFlightGroups[candidate->flightGroupIdx].playerOwnerIdx ==
							(int)playerIdx) {
							objectKind = candidateMobj->pCraft->objectKind;
							if (objectKind != 3 && objectKind != 5 && objectKind != 4 &&
								candidate->objectSignature == preferredObjectSignature) {
								foundCraft = 1;
								matchedPreferredSignature = 1;
								break;
							}
						}
					}
					++selectedObjIdx;
				}

				if (foundCraft == 1) {
					break;
				}

				++searchRegionIdx;
				if (searchRegionIdx >= g_missionRegionCount - 1) {
					searchRegionIdx = 0;
				}
				--regionsRemaining;
			} while (regionsRemaining != 0);
		}
	}

	if (!foundCraft) {
		searchRegionIdx = originalRegionIdx;
		regionsRemaining = g_missionRegionCount - 1;
		if (regionsRemaining != 0) {
			do {
				uint32_t scanObjIdx;
				uint32_t objectsRemaining;

				Mission_SetActiveRegionObjectRanges(searchRegionIdx);
				if (searchRegionIdx == originalRegionIdx && previousObjIdx != 0xffffu) {
					scanObjIdx = previousObjIdx;
				} else {
					scanObjIdx = g_activeRegionObjectSlotStart - 1u;
				}

				objectsRemaining = g_activeRegionCraftObjectSlotEnd - g_activeRegionObjectSlotStart;
				if (objectsRemaining != 0) {
					do {
						ObjectRecord* candidate;
						MobileObject* candidateMobj;

						++scanObjIdx;
						if (scanObjIdx >= g_activeRegionCraftObjectSlotEnd) {
							scanObjIdx = g_activeRegionObjectSlotStart;
						}
						candidate = &g_objectTable[scanObjIdx];
						if (candidate->objectType != OBJ_None) {
							candidateMobj = candidate->mobj;
							if (g_missionFlightGroups[candidate->flightGroupIdx].playerOwnerIdx ==
								(int)playerIdx) {
								uint8_t objectKind;

								objectKind = candidateMobj->pCraft->objectKind;
								if (objectKind != 3 && objectKind != 5 && objectKind != 4) {
									selectedObjIdx = scanObjIdx;
									foundCraft = 1;
									break;
								}
							}
						}

						--objectsRemaining;
					} while (objectsRemaining != 0);
				}

				if (foundCraft) {
					break;
				}

				++searchRegionIdx;
				if (searchRegionIdx >= g_missionRegionCount - 1) {
					searchRegionIdx = 0;
				}
				--regionsRemaining;
			} while (regionsRemaining != 0);
		}
	}

	if (!foundCraft) {
		Mission_SetActiveRegionObjectRanges(savedRegionIdx);
		return 1;
	}

	linkedPlayerIdx = Player_FindLinkedGunnerForFlightGroup((int)playerIdx);

	g_objectTable[selectedObjIdx].playerOwnerIdx = playerIdx;
	g_objectTable[selectedObjIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[selectedObjIdx].mobj->moveVectorDirty = 1;
	collide_ResetObjectProximityForSlot((uint16_t)selectedObjIdx);
	selectedCraft = g_objectTable[selectedObjIdx].mobj->pCraft;

	if (g_objectTable[selectedObjIdx].objectType == OBJ_CorellianTransport2 ||
		g_objectTable[selectedObjIdx].objectType == OBJ_MilleniumFalcon2) {
		uint16_t systemFlags;

		systemFlags = selectedCraft->systemFlags;
		if ((systemFlags & 0x10u) == 0) {
			systemFlags = (uint16_t)(systemFlags | 0x10u);
			selectedCraft->workingSubsystems = (uint16_t)(selectedCraft->workingSubsystems | 0x10u);
			selectedCraft->systemFlags = systemFlags;
		}
	}

	for (groupIdx = 0; groupIdx < 3; ++groupIdx) {
		ModelIndex modelIndex;
		uint8_t mountType;

		selectedCraft->laserLinkMode[groupIdx] = 1;
		selectedCraft->laserLinkMode[groupIdx + 3u] = 0;
		selectedCraft->laserLinkNextSlot[groupIdx] = 0;
		selectedCraft->laserFireCooldownTicks[groupIdx] = 0;
		selectedCraft->laserLastFireTimestamp[groupIdx] = 0;

		modelIndex = (ModelIndex)GetModelIndexFromType(g_objectTable[selectedObjIdx].objectType);
		if (selectedCraft->laserProjectileTypeId[groupIdx] != 0) {
			mountType = g_modelDefs[(uint16_t)modelIndex].laserGroupMountType[groupIdx];
			if (mountType == 1 || mountType == 2) {
				selectedCraft->laserLinkNextSlot[groupIdx] =
					g_modelDefs[(uint16_t)modelIndex].laserGroupFirstSlot[groupIdx];
			}
		}
	}
	selectedCraft->laserLinkMode[0] = (uint8_t)g_players[playerIdx].savedCraftSettingsRaw[6];
	selectedCraft->laserLinkMode[1] = (uint8_t)g_players[playerIdx].savedCraftSettingsRaw[7];

	for (launcherIdx = 0; launcherIdx < 2u; ++launcherIdx) {
		selectedCraft->warheadLauncherFlags[launcherIdx] =
			(uint8_t)((selectedCraft->warheadLauncherFlags[launcherIdx] & 0x80u) | 1u);
		selectedCraft->warheadLauncherCooldownTicks[launcherIdx] = 0;
	}
	selectedCraft->warheadLauncherFlags[0] =
		(uint8_t)((selectedCraft->warheadLauncherFlags[0] & 0x80u) |
				  (uint8_t)g_players[playerIdx].savedCraftSettingsRaw[8]);
	selectedCraft->warheadLauncherFlags[1] =
		(uint8_t)((selectedCraft->warheadLauncherFlags[1] & 0x80u) |
				  (uint8_t)g_players[playerIdx].savedCraftSettingsRaw[9]);
	selectedCraft->warheadLockTicks = 0;

	{
		int maxShieldPerFace;
		uint8_t shieldMode;

		maxShieldPerFace = 2 * g_modelDefs[selectedCraft->modelIndex].shieldStrength;
		shieldMode = (uint8_t)g_players[playerIdx].savedCraftSettingsRaw[5];
		selectedCraft->shieldDistribMode = shieldMode;
		switch (shieldMode) {
			case 0: {
				int shieldFront;

				shieldFront = selectedCraft->shieldFront;
				if (shieldFront > maxShieldPerFace) {
					selectedCraft->shieldFront = maxShieldPerFace;
					selectedCraft->shieldRear = shieldFront - maxShieldPerFace;
				}
				break;
			}
			case 1: {
				int halfFront;

				halfFront = selectedCraft->shieldFront >> 1;
				selectedCraft->shieldFront = halfFront;
				selectedCraft->shieldRear = halfFront;
				break;
			}
			case 2: {
				int shieldFront;

				shieldFront = selectedCraft->shieldFront;
				selectedCraft->shieldFront = 0;
				selectedCraft->shieldRear = shieldFront;
				if (shieldFront > maxShieldPerFace) {
					selectedCraft->shieldRear = maxShieldPerFace;
					selectedCraft->shieldFront = shieldFront - maxShieldPerFace;
				}
				break;
			}
		}
	}

	selectedCraft->shieldRedirect = (uint8_t)g_players[playerIdx].savedCraftSettingsRaw[3];
	selectedCraft->laserRedirect = (uint8_t)g_players[playerIdx].savedCraftSettingsRaw[2];
	selectedCraft->beamLevel = (uint8_t)g_players[playerIdx].savedCraftSettingsRaw[4];

	g_players[playerIdx].objectIndex = (uint16_t)selectedObjIdx;
	g_players[playerIdx].boundObjectSignature = g_objectTable[selectedObjIdx].objectSignature;
	g_players[playerIdx].regionIndex = g_objectTable[selectedObjIdx].regionIdx;
	if (g_players[playerIdx].savedHudEnabled) {
		Hud_SetHudEnabled((int)playerIdx, 1);
	}
	g_players[playerIdx].regionSessionId = 0;
	g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
	if (g_players[playerIdx].hasCheckpointFlag) {
		g_players[playerIdx].currentSeatIdx = 1;
	}
	if (playerIdx == (unsigned int)g_localPlayer) {
		FlightLight_SetLocalPlayerPulseEnabled(4, 0);
		FlightLight_SetLocalPlayerPulseEnabled(5, 0);
		FlightLight_SetLocalPlayerPulseEnabled(3, 0);
		ForceFeedback_EnableEffects();
	}
	g_objPrevX[playerIdx] = g_objectTable[selectedObjIdx].world_x;
	g_objPrevY[playerIdx] = g_objectTable[selectedObjIdx].world_y;
	g_objPrevZ[playerIdx] = g_objectTable[selectedObjIdx].world_z;

	if (matchedPreferredSignature || previousObjIdx != 0xffffu) {
		selectedCraft->throttleSpeed =
			(uint16_t)((uint8_t)g_players[playerIdx].savedCraftSettingsRaw[0] |
					   ((uint16_t)(uint8_t)g_players[playerIdx].savedCraftSettingsRaw[1] << 8));
	}

	if (!matchedPreferredSignature) {
		g_players[playerIdx].selectedWarhead = 0;
		g_players[playerIdx].selectedWeaponMode = 0;
	}
	g_players[playerIdx].targetCycleStart = -1;
	g_players[playerIdx].targetingState = -1;
	g_players[playerIdx].selectedTargetComponent = 0;
	if (resetTargetState == 1) {
		g_players[playerIdx].hyperspaceRuntime.targetBoxEnabled = 1;
		g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
		g_players[playerIdx].targetSubState = 0;
		g_players[playerIdx].targetPresetSlot[0] = -1;
		g_players[playerIdx].targetPresetSlot[1] = -1;
		g_players[playerIdx].targetPresetSlot[2] = -1;
		g_players[playerIdx].targetPresetSlot[3] = -1;
	}
	if (previousObjIdx != 0xffffu && g_objectTable[previousObjIdx].mobj != NULL) {
		AiController* ai;
		uint16_t targetObjIdx;

		g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
		ai = pai_GetEffectiveAIController(selectedCraft);
		targetObjIdx = ai->targetObjIdx;
		if (targetObjIdx < 0x8000u) {
			g_players[playerIdx].currentTargetObjectIdx = (int16_t)targetObjIdx;
			g_players[playerIdx].selectedTargetComponent = 0;
		}
	}
	if ((uint16_t)g_players[playerIdx].currentTargetObjectIdx == (uint16_t)selectedObjIdx) {
		g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
	}
	Player_ValidateCurrentTargets((int)playerIdx);
	g_players[playerIdx].missileLockState = 0;
	if (g_players[playerIdx].regionIndex != originalRegionIdx) {
		g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
	}
	selectedCraft->aiController.pendingPlanId = (uint8_t)pai_findplanbyname("nullpln");
	selectedCraft->aiController.targetObjIdx = -1;

	g_players[playerIdx].pendingActionTimer = 0;
	g_players[playerIdx].beamFireCooldownTimer = 0;
	g_players[playerIdx].gap71_field16 = 0;
	g_players[playerIdx].smoothedInputYaw = 0;
	g_players[playerIdx].smoothedInputPitch = 0;
	memset(&g_playerFlightTransientTimers[playerIdx], 0, 0x18u);
	g_players[playerIdx].smoothedInputRoll = 0;
	g_players[playerIdx].savedKeyMods = 0;
	g_players[playerIdx].keyModsHoldTimer = 0;
	g_players[playerIdx].engineWashSourceObjIdx = -1;
	g_playerFlightTransientTimers[playerIdx].field_18 = 0;

	(void)GetModelIndexFromType(g_objectTable[g_players[playerIdx].objectIndex].objectType);
	g_players[playerIdx].viewState.hudAimXSnapState = 0;
	g_players[playerIdx].lookYawOffset = 0;
	g_players[playerIdx].lookPitchOffset = 0;
	g_players[playerIdx].viewState.hudAimX = 0;
	g_players[playerIdx].viewState.hudAimY = 0;
	g_players[playerIdx].viewState.externalCameraActive = 0;
	g_players[playerIdx].viewState.cameraDistance = 1024;
	g_players[playerIdx].viewState.playerInputBlocked = 0;
	g_players[playerIdx].viewState.transitionTimer = 0;
	g_players[playerIdx].viewState.transitionDuration = 0;
	g_players[playerIdx].viewState.cameraFocusObjIdx = g_players[playerIdx].objectIndex;
	g_players[playerIdx].viewState.cameraPanDeltaX = 0;
	g_players[playerIdx].viewState.cameraPanDeltaY = 0;
	g_players[playerIdx].viewState.cameraPanDeltaZ = 0;
	g_players[playerIdx].viewState.cameraPitchDelta = 0;
	g_players[playerIdx].viewState.cameraYawDelta = 0;
	g_players[playerIdx].viewState.cameraRollDelta = 0;
	g_players[playerIdx].viewState.field_32 = 0;
	Hud_SetHudViewState(19, (int)playerIdx);
	if (playerIdx == (unsigned int)g_localPlayer) {
		fsfx_PlaySound(17, 0xffffu, playerIdx);
	}

	if (linkedPlayerIdx != -1) {
		g_players[linkedPlayerIdx].objectIndex = (uint16_t)selectedObjIdx;
		g_players[linkedPlayerIdx].boundObjectSignature = g_objectTable[selectedObjIdx].objectSignature;
		g_players[linkedPlayerIdx].regionIndex = g_objectTable[selectedObjIdx].regionIdx;
		g_players[linkedPlayerIdx].regionSessionId = 0;
		g_players[linkedPlayerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
		if (g_players[linkedPlayerIdx].hasCheckpointFlag) {
			g_players[linkedPlayerIdx].currentSeatIdx = 1;
		}
		if (linkedPlayerIdx == g_localPlayer) {
			FlightLight_SetLocalPlayerPulseEnabled(4, 0);
			FlightLight_SetLocalPlayerPulseEnabled(5, 0);
			FlightLight_SetLocalPlayerPulseEnabled(3, 0);
			ForceFeedback_EnableEffects();
		}
		g_objPrevX[linkedPlayerIdx] = g_objectTable[selectedObjIdx].world_x;
		g_objPrevY[linkedPlayerIdx] = g_objectTable[selectedObjIdx].world_y;
		g_objPrevZ[linkedPlayerIdx] = g_objectTable[selectedObjIdx].world_z;

		if (!matchedPreferredSignature) {
			g_players[linkedPlayerIdx].selectedWarhead = 0;
			g_players[linkedPlayerIdx].selectedWeaponMode = 0;
		}
		g_players[linkedPlayerIdx].targetCycleStart = -1;
		g_players[linkedPlayerIdx].targetingState = -1;
		g_players[linkedPlayerIdx].selectedTargetComponent = 0;
		if (resetTargetState == 1) {
			g_players[linkedPlayerIdx].hyperspaceRuntime.targetBoxEnabled = 1;
			g_players[linkedPlayerIdx].currentTargetObjectIdx = 0xffffu;
			g_players[linkedPlayerIdx].targetSubState = 0;
			g_players[linkedPlayerIdx].targetPresetSlot[0] = -1;
			g_players[linkedPlayerIdx].targetPresetSlot[1] = -1;
			g_players[linkedPlayerIdx].targetPresetSlot[2] = -1;
			g_players[linkedPlayerIdx].targetPresetSlot[3] = -1;
		}
		if (previousObjIdx != 0xffffu && g_objectTable[previousObjIdx].mobj != NULL) {
			AiController* ai;
			uint16_t targetObjIdx;

			g_players[linkedPlayerIdx].currentTargetObjectIdx = 0xffffu;
			ai = pai_GetEffectiveAIController(selectedCraft);
			targetObjIdx = ai->targetObjIdx;
			if (targetObjIdx < 0x8000u) {
				g_players[linkedPlayerIdx].currentTargetObjectIdx = (int16_t)targetObjIdx;
				g_players[linkedPlayerIdx].selectedTargetComponent = 0;
			}
		}
		if ((uint16_t)g_players[linkedPlayerIdx].currentTargetObjectIdx == (uint16_t)selectedObjIdx) {
			g_players[linkedPlayerIdx].currentTargetObjectIdx = 0xffffu;
		}
		Player_ValidateCurrentTargets((int)linkedPlayerIdx);
		g_players[linkedPlayerIdx].missileLockState = 0;
		if (g_players[linkedPlayerIdx].regionIndex != originalRegionIdx) {
			g_players[linkedPlayerIdx].currentTargetObjectIdx = 0xffffu;
		}

		g_players[linkedPlayerIdx].pendingActionTimer = 0;
		g_players[linkedPlayerIdx].beamFireCooldownTimer = 0;
		g_players[linkedPlayerIdx].gap71_field16 = 0;
		g_players[linkedPlayerIdx].smoothedInputYaw = 0;
		g_players[linkedPlayerIdx].smoothedInputPitch = 0;
		g_players[linkedPlayerIdx].smoothedInputRoll = 0;
		g_players[linkedPlayerIdx].savedKeyMods = 0;
		g_players[linkedPlayerIdx].keyModsHoldTimer = 0;
		memset(&g_playerFlightTransientTimers[linkedPlayerIdx], 0, 0x18u);
		g_playerFlightTransientTimers[linkedPlayerIdx].field_18 = 0;
		g_players[linkedPlayerIdx].engineWashSourceObjIdx = -1;

		(void)GetModelIndexFromType(g_objectTable[g_players[linkedPlayerIdx].objectIndex].objectType);
		g_players[linkedPlayerIdx].viewState.hudAimXSnapState = 0;
		g_players[linkedPlayerIdx].viewState.hudAimX = 0;
		g_players[linkedPlayerIdx].viewState.hudAimY = 0;
		g_players[linkedPlayerIdx].viewState.externalCameraActive = 0;
		g_players[linkedPlayerIdx].viewState.cameraDistance = 1024;
		g_players[linkedPlayerIdx].viewState.transitionTimer = 0;
		g_players[linkedPlayerIdx].viewState.transitionDuration = 0;
		g_players[linkedPlayerIdx].viewState.playerInputBlocked = 0;
		g_players[linkedPlayerIdx].viewState.cameraFocusObjIdx = g_players[linkedPlayerIdx].objectIndex;
		g_players[linkedPlayerIdx].viewState.cameraPanDeltaX = 0;
		g_players[linkedPlayerIdx].viewState.cameraPanDeltaY = 0;
		g_players[linkedPlayerIdx].viewState.cameraPanDeltaZ = 0;
		g_players[linkedPlayerIdx].viewState.cameraPitchDelta = 0;
		g_players[linkedPlayerIdx].viewState.cameraYawDelta = 0;
		g_players[linkedPlayerIdx].viewState.cameraRollDelta = 0;
		g_players[linkedPlayerIdx].viewState.field_32 = 0;
		Hud_SetHudViewState(19, (int)linkedPlayerIdx);
		if (linkedPlayerIdx == g_localPlayer) {
			fsfx_PlaySound(17, 0xffffu, linkedPlayerIdx);
		}
	}

	Mission_SetActiveRegionObjectRanges(savedRegionIdx);
	return 0;
}

// FUNCTION: XWA 0x41FA00
int Player_UnbindFromCurrentCraft(int playerIdx, int requireAnotherOwnedCraft, int restoreMissionAiPlan) {
	int objectIndex;
	int sharingPlayerIdx;
	unsigned int i;
	int savedRegionIdx;

	savedRegionIdx = regionIdx;

	if (requireAnotherOwnedCraft == 1) {
		int regionCursor;
		int remainingRegions;
		int craftCount;
		int found;

		craftCount = 0;
		found = 0;
		remainingRegions = g_missionRegionCount - 1;
		regionCursor = g_players[playerIdx].regionIndex;

		if (remainingRegions != 0) {
			do {
				uint32_t objectSlot;

				Mission_SetActiveRegionObjectRanges(regionCursor);
				for (objectSlot = g_activeRegionObjectSlotStart;
					 objectSlot < g_activeRegionCraftObjectSlotEnd; ++objectSlot) {
					ObjectRecord* scanObj;

					scanObj = &g_objectTable[objectSlot];
					if (scanObj->objectType != OBJ_None) {
						MobileObject* mobj;
						CraftData* scanCraft;
						uint8_t objectKind;

						mobj = scanObj->mobj;
						scanCraft = mobj->pCraft;
						if (g_missionFlightGroups[scanObj->flightGroupIdx].playerOwnerIdx == playerIdx) {
							objectKind = scanCraft->objectKind;
							if (objectKind != 3 && objectKind != 5 && objectKind != 4) {
								++craftCount;
								if ((uint16_t)craftCount > 1u) {
									found = 1;
									break;
								}
							}
						}
					}
				}

				if (found) {
					break;
				}

				++regionCursor;
				if (regionCursor >= g_missionRegionCount - 1) {
					regionCursor = 0;
				}
				--remainingRegions;
			} while (remainingRegions != 0);
		}

		Mission_SetActiveRegionObjectRanges(savedRegionIdx);
		if ((uint16_t)craftCount <= 1u) {
			return 0;
		}
	}

	sharingPlayerIdx = -1;
	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		if (i != (unsigned int)playerIdx && g_players[i].connectedFlag != 0 &&
			g_players[i].objectIndex == g_players[playerIdx].objectIndex) {
			sharingPlayerIdx = i;
		}
	}

	objectIndex = g_players[playerIdx].objectIndex;
	if (objectIndex == 0xffff) {
		return 0;
	}

	if (sharingPlayerIdx == -1) {
		CraftData* craft;
		int shieldTotal;

		g_objectTable[objectIndex].playerOwnerIdx = -1;
		g_objectTable[objectIndex].mobj->orientMatrixDirty = 1;
		g_objectTable[objectIndex].mobj->moveVectorDirty = 1;
		collide_ResetNeighborProximityLists((uint16_t)objectIndex);
		collide_ResetObjectProximityForSlot((uint16_t)objectIndex);
		Player_SaveCraftSettings(playerIdx);

		craft = g_objectTable[objectIndex].mobj->pCraft;
		for (i = 0; i < 3; ++i) {
			craft->laserLinkMode[i] = 0;
			craft->laserLinkMode[i + 3] = 0;
			craft->laserLinkNextSlot[i] = 0;
			craft->laserFireCooldownTicks[i] = 0;
			craft->laserLastFireTimestamp[i] = 0;
		}
		for (i = 0; i < 2; ++i) {
			craft->warheadLauncherFlags[i] = (uint8_t)((craft->warheadLauncherFlags[i] & 0x80u) | 1u);
			craft->warheadLauncherCooldownTicks[i] = 0;
		}
		shieldTotal = craft->shieldFront + craft->shieldRear;
		craft->warheadLockTicks = 0;
		craft->shieldFront = shieldTotal;
		craft->shieldRear = 0;
		craft->shieldDistribMode = 0;

		g_players[playerIdx].objectIndex = 0xffff;
		g_players[playerIdx].regionSessionId = 0;
		g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
		if (playerIdx == g_localPlayer) {
			FlightLight_SetLocalPlayerPulseEnabled(4, 0);
			FlightLight_SetLocalPlayerPulseEnabled(5, 0);
			FlightLight_SetLocalPlayerPulseEnabled(3, 0);
		}
		g_players[playerIdx].missileLockState = 0;
		g_players[playerIdx].gap71_field16 = 0;
		g_players[playerIdx].smoothedInputYaw = 0;
		g_players[playerIdx].smoothedInputPitch = 0;
		g_players[playerIdx].smoothedInputRoll = 0;
		g_players[playerIdx].savedKeyMods = 0;
		g_players[playerIdx].keyModsHoldTimer = 0;
		g_players[playerIdx].engineWashSourceObjIdx = -1;

		g_curCraft->aiController.currentOrderSlot = 0;
		if (restoreMissionAiPlan) {
			uint16_t planId;
			PlanRecord* plan;
			uint16_t throttle;

			planId = g_builtinPlanIdByNameIndex
				[g_orderLeaderBuiltinPlanNameIndex
					 [g_missionFlightGroups[g_objectTable[objectIndex].flightGroupIdx]
						  .fg.orders[4 * g_objectTable[objectIndex].regionIdx]
						  .order]];
			g_curCraft->aiController.currentPlanId = planId;
			g_curCraft->aiController.pendingPlanId = planId;
			plan = &g_planTable[planId];

			if (strcmp(plan->name, "nullpln") == 0 || strcmp(plan->name, "stationaryldrpln") == 0 ||
				strcmp(plan->name, "stationaryflwpln") == 0 || strcmp(plan->name, "disabledpln") == 0) {
				throttle = 0;
			} else if (strcmp(plan->name, "escortldr1pln") == 0) {
				throttle = 0x8000u;
			} else {
				throttle = g_orderThrottleToCraftThrottleSpeed
					[g_missionFlightGroups[g_objectTable[objectIndex].flightGroupIdx]
						 .fg.orders[4 * g_objectTable[objectIndex].regionIdx]
						 .throttle];
			}

			craft->throttleSpeed = throttle;
			g_objectTable[objectIndex].mobj->speed =
				(uint16_t)MATH2_fraction(g_modelDefs[craft->modelIndex].maxSpeed, throttle);
			g_objectTable[objectIndex].mobj->speedRemainder = 0;

			g_curCraft = craft;
			pai_setupcraftcontext(objectIndex);
			pai_ApplyPendingPlanTargetAndManeuver(objectIndex);

			if (g_players[playerIdx].currentTargetObjectIdx != 0xffffu) {
				uint32_t targetObjIdx;

				targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
				if ((targetObjIdx >= g_activeRegionObjectSlotStart &&
					 targetObjIdx < g_activeRegionCraftObjectSlotEnd) ||
					(targetObjIdx >= g_objScanStart && targetObjIdx < g_regionStaticObjectSlotEnd)) {
					int matchesOrderTarget;
					unsigned int orderSlot;

					matchesOrderTarget = 0;
					for (orderSlot = 0; orderSlot < 3u; ++orderSlot) {
						g_paiContext.curOrderCoord.fields.orderSlot = orderSlot;
						if (pai_CurrentOrderTargetsMatchObject(
								(uint16_t)g_players[playerIdx].currentTargetObjectIdx)) {
							matchesOrderTarget = 1;
						}
					}
					if (matchesOrderTarget) {
						pai_GetEffectiveAIController(craft)->candidateTargetIdx =
							(uint16_t)g_players[playerIdx].currentTargetObjectIdx;
						return 1;
					}
				}
			}

			return 1;
		}

		g_curCraft->aiController.pendingPlanId = (uint8_t)pai_findplanbyname("nullpln");
		g_curCraft->aiController.currentPlanId = g_curCraft->aiController.pendingPlanId;
		g_curCraft = craft;
		pai_setupcraftcontext(objectIndex);
		pai_ApplyPendingPlanTargetAndManeuver(objectIndex);
		craft->aiFlight.enterFlag = 0;
		craft->aiFlight.headingState = 0;
		craft->aiFlight.turnState = 0;
		craft->aiFlight.climbState = 0;
		craft->aiFlight.diveState = 0;
		return 1;
	}

	if (!g_players[playerIdx].hasCheckpointFlag) {
		Player_SaveCraftSettings(playerIdx);
		g_players[playerIdx].objectIndex = 0xffff;
		g_players[playerIdx].regionSessionId = 0;
		g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
		if (playerIdx == g_localPlayer) {
			FlightLight_SetLocalPlayerPulseEnabled(4, 0);
			FlightLight_SetLocalPlayerPulseEnabled(5, 0);
			FlightLight_SetLocalPlayerPulseEnabled(3, 0);
		}
		g_players[playerIdx].missileLockState = 0;
		g_players[playerIdx].gap71_field16 = 0;
		g_players[playerIdx].smoothedInputYaw = 0;
		g_players[playerIdx].smoothedInputPitch = 0;
		g_players[playerIdx].smoothedInputRoll = 0;
		g_players[playerIdx].savedKeyMods = 0;
		g_players[playerIdx].keyModsHoldTimer = 0;
		g_players[playerIdx].engineWashSourceObjIdx = -1;
		g_objectTable[objectIndex].playerOwnerIdx = sharingPlayerIdx;
		g_objectTable[objectIndex].mobj->orientMatrixDirty = 1;
		g_objectTable[objectIndex].mobj->moveVectorDirty = 1;
		g_players[sharingPlayerIdx].hasCheckpointFlag = 0;
		return 1;
	}

	g_players[playerIdx].hasCheckpointFlag = 0;
	Player_SaveCraftSettings(playerIdx);
	objectIndex = 0xffff;
	g_players[playerIdx].objectIndex = objectIndex;
	g_players[playerIdx].regionSessionId = 0;
	g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
	if (playerIdx == g_localPlayer) {
		FlightLight_SetLocalPlayerPulseEnabled(4, 0);
		FlightLight_SetLocalPlayerPulseEnabled(5, 0);
		FlightLight_SetLocalPlayerPulseEnabled(3, 0);
	}
	g_players[playerIdx].missileLockState = 0;
	g_players[playerIdx].gap71_field16 = 0;
	g_players[playerIdx].smoothedInputYaw = 0;
	g_players[playerIdx].smoothedInputPitch = 0;
	g_players[playerIdx].smoothedInputRoll = 0;
	g_players[playerIdx].savedKeyMods = 0;
	g_players[playerIdx].keyModsHoldTimer = 0;
	g_players[playerIdx].engineWashSourceObjIdx = (ObjectIndex)objectIndex;
	return 1;
}

// FUNCTION: XWA 0x508FF0
int Player_FindLinkedGunnerForFlightGroup(int playerIdx) {
	int i;

	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		if (i != playerIdx && g_players[i].connectedFlag && g_players[i].hasCheckpointFlag &&
			g_players[i].boundFlightGroupIdx == g_players[playerIdx].boundFlightGroupIdx) {
			return i;
		}
	}

	return -1;
}

// FUNCTION: XWA 0x506910
int Player_FindNearestEnemyFighter(int playerIdx, int excludeObjIdx) {
	uint32_t bestRange;
	uint16_t objectType;
	MobileObject* mobj;
	uint32_t scanIdx;
	int bestObjIdx;
	uint32_t objIdx;

	scanIdx = g_activeRegionObjectSlotStart;
	objIdx = (uint16_t)g_activeRegionObjectSlotStart;
	bestRange = UINT32_MAX;
	bestObjIdx = 0xffff;

	if (objIdx < g_activeRegionCraftObjectSlotEnd) {
		do {
			ObjectRecord* obj;

			objectType = g_objectTable[objIdx].objectType;
			obj = &g_objectTable[objIdx];
			if (objectType != OBJ_None) {
				if (objIdx != g_players[playerIdx].objectIndex && objIdx != excludeObjIdx) {
					mobj = obj->mobj;
					if (mobj->team != g_players[playerIdx].playerIff) {
						int playerTeam;
						int candidateTeam;

						playerTeam = (uint16_t)g_players[playerIdx].playerIff;
						if (mobj != NULL) {
							candidateTeam = mobj->team;
						} else {
							candidateTeam = g_missionFlightGroups[obj->flightGroupIdx].fg.team;
						}

						if (candidateTeam != playerTeam &&
							g_missionTeams[playerTeam].allies[candidateTeam] == 0) {
							ModelGenusId genusId;

							genusId = obj->genusId;
							if (genusId != GENUS_Explosion && genusId != GENUS_Starship &&
								genusId != GENUS_Rubble && genusId != GENUS_Freighter &&
								genusId != GENUS_Container && genusId != GENUS_Platform &&
								(genusId != GENUS_PilotDroid || objectType == OBJ_ZeroGStormtrooper) &&
								genusId != GENUS_SatelliteBuoy && genusId != GENUS_Utility &&
								(g_modelTypeTable[(uint16_t)objectType].flags & 4u) == 0) {
								CraftData* craft;

								craft = mobj->pCraft;
								if ((uint16_t)scanIdx == 0xffffu ||
									objIdx >= g_activeRegionCraftObjectSlotEnd || obj->playerOwnerIdx == -1 ||
									craft == NULL || (craft->workingSubsystems & 0x100u) == 0 ||
									craft->beamActive == 0 || craft->beamTypeId != 3 ||
									craft->beamTimer == 0) {
									craft = mobj->pCraft;
									if (craft->workingSubsystems != 0) {
										uint8_t objectKind;

										objectKind = craft->objectKind;
										if (objectKind == 0 || objectKind == 6) {
											unsigned int objectIndex;

											objectIndex = (unsigned int)g_players[playerIdx].objectIndex;
											if (objectIndex == 0xffffu) {
												int dx;
												int dy;
												int dz;

												Mission_ResolveObjectOrMissionPointWorldLoc(objIdx, 0, 0, 0);
												dz = worldlocz - g_players[playerIdx].viewState.savedTargetZ;
												dy = worldlocy - g_players[playerIdx].viewState.savedTargetY;
												dx = worldlocx - g_players[playerIdx].viewState.savedTargetX;
												trig2_ctop(dx, dy, dz);
											} else {
												pai_ObjectRefDirectionToObjectRef(objectIndex, objIdx);
											}
											if ((uint32_t)trig2_polardistance < bestRange) {
												bestObjIdx = objIdx;
												bestRange = (uint32_t)trig2_polardistance;
											}
										}
									}
								}
							}
						}
					}
				}
			}

			++scanIdx;
			objIdx = (uint16_t)scanIdx;
		} while (objIdx < g_activeRegionCraftObjectSlotEnd);
	}

	scanIdx = g_objScanStart;
	objIdx = (uint16_t)scanIdx;
	while (objIdx < g_regionStaticObjectSlotEnd) {
		ObjectRecord* obj;

		obj = &g_objectTable[objIdx];
		if (obj->objectType != OBJ_None && obj->genusId == GENUS_Mine && objIdx != excludeObjIdx &&
			obj->typeSpecificWord != 0) {
			int candidateTeam;
			int playerTeam;

			playerTeam = (uint16_t)g_players[playerIdx].playerIff;
			mobj = obj->mobj;
			if (mobj != NULL) {
				candidateTeam = mobj->team;
			} else {
				candidateTeam = g_missionFlightGroups[obj->flightGroupIdx].fg.team;
			}

			if (candidateTeam != playerTeam && g_missionTeams[playerTeam].allies[candidateTeam] == 0) {
				if (g_players[playerIdx].objectIndex == 0xffff) {
					int dx;
					int dy;
					int dz;

					Mission_ResolveObjectOrMissionPointWorldLoc(objIdx, 0, 0, 0);
					dz = worldlocz - g_players[playerIdx].viewState.savedTargetZ;
					dy = worldlocy - g_players[playerIdx].viewState.savedTargetY;
					dx = worldlocx - g_players[playerIdx].viewState.savedTargetX;
					trig2_ctop(dx, dy, dz);
				} else {
					pai_ObjectRefDirectionToObjectRef((unsigned int)g_players[playerIdx].objectIndex, objIdx);
				}
				if ((uint32_t)trig2_polardistance < bestRange) {
					bestObjIdx = objIdx;
					bestRange = (uint32_t)trig2_polardistance;
				}
			}
		}

		++scanIdx;
		objIdx = (uint16_t)scanIdx;
	}

	return bestObjIdx;
}

// FUNCTION: XWA 0x505450
int16_t Player_FindAttackerOfTarget(uint16_t targetObjIdx, int16_t playerObjIdx) {
	uint32_t bestRange;
	uint16_t bestObjIdx;
	uint32_t scanIdx;
	uint32_t scanIdx32;

	if (targetObjIdx == 0xffffu) {
		return (int16_t)0xffffu;
	}

	bestObjIdx = 0xffffu;
	bestRange = UINT32_MAX;
	scanIdx32 = g_activeRegionObjectSlotStart;
	for (scanIdx = (uint16_t)scanIdx32; (uint32_t)scanIdx < g_activeRegionCraftObjectSlotEnd;
		 scanIdx = (uint16_t)++scanIdx32) {
		CraftData* candidateCraft;
		int qualifies;
		char isBeamVictim;

		if (g_objectTable[scanIdx].objectType == OBJ_None || (uint16_t)scanIdx32 == targetObjIdx ||
			(uint16_t)scanIdx32 == (uint16_t)playerObjIdx ||
			g_objectTable[scanIdx].genusId == GENUS_Explosion) {
			continue;
		}

		qualifies = 0;
		candidateCraft = g_objectTable[scanIdx].mobj->pCraft;
		if (candidateCraft->workingSubsystems == 0 || candidateCraft->objectKind != 0) {
			continue;
		}

		if ((uint16_t)scanIdx32 != 0xffffu && (uint32_t)scanIdx < g_activeRegionCraftObjectSlotEnd &&
			g_objectTable[scanIdx].playerOwnerIdx != -1 && candidateCraft != NULL &&
			(candidateCraft->workingSubsystems & 0x100u) != 0 && candidateCraft->beamActive != 0 &&
			candidateCraft->beamTypeId == 3 && candidateCraft->beamTimer != 0) {
			isBeamVictim = 1;
		} else {
			isBeamVictim = 0;
		}
		if (isBeamVictim) {
			continue;
		}

		if (g_objectTable[scanIdx].playerOwnerIdx == -1) {
			AiController* ai;

			ai = pai_GetEffectiveAIController(candidateCraft);
			if (ai->targetObjIdx != targetObjIdx) {
				continue;
			}
			if (ai->maneuverMode != 12 && ai->maneuverMode != 23) {
				continue;
			}
			qualifies = 1;
		} else {
			int playerOwnerIdx;

			if ((uint32_t)targetObjIdx >= g_activeRegionObjectSlotStart &&
				(uint32_t)targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
				CraftData* targetCraft;

				targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
				if (targetCraft->lastAttackerObjIdx == (uint16_t)scanIdx32) {
					if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
						int nowSeconds;

						nowSeconds = Mission_GameTimeToSeconds(g_missionElapsedClock.hours,
															   g_missionElapsedClock.minutes,
															   g_missionElapsedClock.seconds);
						if ((uint16_t)nowSeconds - targetCraft->lastHitTimestamp < 5) {
							qualifies = 1;
						}
					} else {
						qualifies = 1;
					}
				}
			}

			playerOwnerIdx = g_objectTable[scanIdx].playerOwnerIdx;
			if ((uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx == targetObjIdx) {
				pai_ObjectRefUpdateApproxRangeScore((uint16_t)scanIdx32, targetObjIdx);
				if ((uint32_t)g_targetRangeScore < 0x10000u &&
					Targeting_ScoreCandidate(targetObjIdx, 0, playerOwnerIdx, 0xffffu)) {
					qualifies = 1;
				}

				if (candidateCraft->warheadLockTicks != 0) {
					qualifies = 1;
				}
			}
		}

		if (qualifies) {
			pai_ObjectRefDirectionToObjectRef(targetObjIdx, scanIdx);
			if ((uint32_t)trig2_polardistance < bestRange) {
				bestObjIdx = (uint16_t)scanIdx32;
				bestRange = (uint32_t)trig2_polardistance;
			}
		}
	}

	return (int16_t)bestObjIdx;
}

// FUNCTION: XWA 0x502710
uint16_t Player_PickTargetInSight(int playerIdx) {
	uint16_t priorityObjIdx;
	uint16_t bestAngle;
	uint16_t bestAngleIndex;
	uint32_t bestRange;
	unsigned int objIdx;

	bestRange = UINT32_MAX;
	bestAngleIndex = 0xffffu;
	bestAngle = bestAngleIndex;
	priorityObjIdx = bestAngleIndex;

	objIdx = g_regionMainObjectSlotStart;
	while ((uint16_t)objIdx < g_regionMainObjectSlotEnd) {
		int objectIndex;
		uint16_t objectType;

		objectIndex = (uint16_t)objIdx;
		objectType = g_objectTable[objectIndex].objectType;
		if (objectType != OBJ_None && objectIndex != g_players[playerIdx].objectIndex &&
			(g_modelTypeTable[objectType].flags & 1u) != 0) {
			if (Targeting_ScoreCandidate(objIdx, 1, playerIdx, 0xffffu)) {
				if ((uint32_t)g_targetRangeScore < bestRange) {
					priorityObjIdx = (uint16_t)objIdx;
					bestRange = (uint32_t)g_targetRangeScore;
				}
			} else if (g_targetAngleScore < bestAngle) {
				bestAngleIndex = (uint16_t)objIdx;
				bestAngle = g_targetAngleScore;
			}
		}
		++objIdx;
	}

	objIdx = g_objScanStart;
	while ((uint16_t)objIdx < g_regionStaticObjectSlotEnd) {
		int objectIndex;
		uint16_t objectType;

		objectIndex = (uint16_t)objIdx;
		objectType = g_objectTable[objectIndex].objectType;
		if (objectType != OBJ_None && (g_modelTypeTable[objectType].flags & 1u) != 0) {
			if (Targeting_ScoreCandidate(objIdx, 1, playerIdx, 0xffffu)) {
				if ((uint32_t)g_targetRangeScore < bestRange) {
					priorityObjIdx = (uint16_t)objIdx;
					bestRange = (uint32_t)g_targetRangeScore;
				}
			} else if (g_targetAngleScore < bestAngle) {
				bestAngleIndex = (uint16_t)objIdx;
				bestAngle = g_targetAngleScore;
			}
		}
		++objIdx;
	}

	if (priorityObjIdx == 0xffffu) {
		if (g_players[playerIdx].mapCameraState != 0) {
			if (bestAngle < 0xfau) {
				priorityObjIdx = bestAngleIndex;
			}
		} else if (bestAngle < 0x32u) {
			priorityObjIdx = bestAngleIndex;
		}
	}
	if (g_players[playerIdx].mapCameraState == 0) {
		char targetBlocked;

		if (priorityObjIdx != 0xffffu && (uint32_t)priorityObjIdx < g_activeRegionCraftObjectSlotEnd &&
			g_objectTable[priorityObjIdx].playerOwnerIdx != -1 &&
			g_objectTable[priorityObjIdx].mobj->pCraft != NULL &&
			(g_objectTable[priorityObjIdx].mobj->pCraft->workingSubsystems & 0x100u) != 0 &&
			g_objectTable[priorityObjIdx].mobj->pCraft->beamActive != 0 &&
			g_objectTable[priorityObjIdx].mobj->pCraft->beamTypeId == 3 &&
			g_objectTable[priorityObjIdx].mobj->pCraft->beamTimer != 0) {
			targetBlocked = 1;
		} else {
			targetBlocked = 0;
		}

		if (targetBlocked == 1) {
			priorityObjIdx = 0xffffu;
			msg_emitInFlightMessage(MSG_TARGET_BLOCKED, playerIdx);
		}
	}

	bestAngleIndex = 0xffffu;
	bestAngle = 0xffffu;
	if ((priorityObjIdx == (uint16_t)g_players[playerIdx].currentTargetObjectIdx ||
		 priorityObjIdx == 0xffffu) &&
		g_players[playerIdx].currentTargetObjectIdx != 0xffffu) {
		MeshType selectedMeshType;
		uint16_t targetType;

		targetType = g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].objectType;
		selectedMeshType = ModelMesh_GetObjectTypeMeshType(
			targetType, (uint16_t)g_players[playerIdx].selectedTargetComponent);
		if (selectedMeshType == MESH_WeaponSystem1 || selectedMeshType == MESH_WeaponSystem2) {
			uint16_t meshCount;
			uint16_t subsystemIdx;
			MobileObject* targetMobile;

			meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(targetType);
			subsystemIdx = 0;
			targetMobile = g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].mobj;
			if (subsystemIdx < meshCount) {
				int meshIdx;
				uint8_t* componentHp;

				meshIdx = 0;
				componentHp = targetMobile->pCraft->componentHp;
				do {
					if (*componentHp != 0) {
						MeshType meshType;

						meshType = ModelMesh_GetObjectTypeMeshType(targetType, meshIdx);
						if ((meshType == MESH_WeaponSystem1 || meshType == MESH_WeaponSystem2) &&
							Targeting_ScoreCandidate(g_players[playerIdx].currentTargetObjectIdx, 1,
													 playerIdx, subsystemIdx)) {
							if (g_targetAngleScore < bestAngle) {
								bestAngleIndex = subsystemIdx;
								bestAngle = g_targetAngleScore;
							}
						}
					}
					++subsystemIdx;
					++meshIdx;
					++componentHp;
				} while (subsystemIdx < meshCount);
			}
		}
	}

	if (bestAngleIndex != 0xffffu) {
		g_players[playerIdx].selectedTargetComponent = (int16_t)bestAngleIndex;
	}

	return priorityObjIdx;
}

// FUNCTION: XWA 0x420240
void Player_SaveCraftSettings(int playerIdx) {
	int objectIdx;
	CraftData* craft;
	char* saved;

	if (g_players[playerIdx].mapCameraState) {
		objectIdx = g_players[playerIdx].altViewObjectIdx;
	} else {
		objectIdx = g_players[playerIdx].objectIndex;
	}

	craft = g_objectTable[objectIdx].mobj->pCraft;
	if (craft == NULL) {
		DebugPrintf("NULL craft data pointer in Save_Player_Craft_Settings()!\n");
		return;
	}

	saved = g_players[playerIdx].savedCraftSettingsRaw;
	*(uint16_t*)saved = craft->throttleSpeed;
	saved[2] = (char)craft->laserRedirect;
	saved[3] = (char)craft->shieldRedirect;
	saved[4] = (char)craft->beamLevel;
	saved[5] = (char)craft->shieldDistribMode;
	saved[6] = (char)craft->laserLinkMode[0];
	saved[7] = (char)craft->laserLinkMode[1];
	saved[8] = (char)(craft->warheadLauncherFlags[0] & 3u);
	saved[9] = (char)(craft->warheadLauncherFlags[1] & 3u);
}

// FUNCTION: XWA 0x502670
void Player_UpdateHudViewForCameraFocus(int playerIdx) {
	if (g_players[playerIdx].viewState.externalCameraActive) {
		Hud_SetHudViewState(18, playerIdx);
		return;
	}

	g_players[playerIdx].viewState.playerInputBlocked = 0;
	if (g_players[playerIdx].viewState.cameraFocusObjIdx == g_players[playerIdx].objectIndex) {
		g_players[playerIdx].viewState.hudAimX = g_players[playerIdx].viewState.savedHudAimX;
		g_players[playerIdx].viewState.hudAimY = g_players[playerIdx].viewState.savedHudAimY;
		Hud_SetHudViewState(g_players[playerIdx].viewState.savedHudStateByte, playerIdx);
	} else {
		g_players[playerIdx].viewState.hudAimX = 0;
		g_players[playerIdx].viewState.hudAimY = 0;
		Hud_SetHudViewState(18, playerIdx);
		g_players[playerIdx].viewState.externalCameraActive = 1;
		g_players[playerIdx].viewState.transitionTimer = 0;
	}
}

// FUNCTION: XWA 0x4201E0
int Player_ResetGunnerSeatCameraState(unsigned int playerIdx, int objectIndex, int resetFlags) {
	g_players[playerIdx].lookYawOffset = 0;
	g_players[playerIdx].lookPitchOffset = 0;
	FlightView_UpdatePlayerCamera((int)playerIdx);
	if (g_players[playerIdx].inputDisabledFlag == 5) {
		g_players[playerIdx].inputDisabledFlag = 0;
	}
	if (playerIdx == (unsigned int)g_localPlayer) {
		fsfx_PlaySound(17, 0xffffu, playerIdx);
	}
	return 0;
}

// FUNCTION: XWA 0x501D90
void Player_CycleGunnerSeat(int playerIdx, void* forcePilotFlag) {
	CraftData* craft;
	int objectIndex;
	int objectType;
	int otherPlayerIdx;

	objectIndex = g_players[playerIdx].objectIndex;
	if (objectIndex != 0xffff) {
		craft = g_objectTable[objectIndex].mobj->pCraft;
	} else {
		craft = NULL;
	}

	if (g_flightSimSideEffectsSuppressed == 0 && objectIndex != 0xffff) {
		if (g_players[playerIdx].padlockActive != 0) {
			g_players[playerIdx].padlockActive = 0;
		}

		objectIndex = g_players[playerIdx].objectIndex;
		objectType = g_objectTable[objectIndex].objectType;
		switch (objectType) {
			case OBJ_MilleniumFalcon2:
			case OBJ_FamilyTransport:
				if (g_players[playerIdx].hasCheckpointFlag != 0) {
					++g_players[playerIdx].currentSeatIdx;
					if (g_players[playerIdx].currentSeatIdx > 2) {
						g_players[playerIdx].currentSeatIdx = 1;
					}
					Player_ResetGunnerSeatCameraState(playerIdx, g_players[playerIdx].objectIndex, 0);
				} else {
					otherPlayerIdx = 0;
					for (;;) {
						if (otherPlayerIdx >= XWA_PLAYER_COUNT) {
							otherPlayerIdx = -1;
							break;
						}
						if (otherPlayerIdx != (int)playerIdx &&
							g_players[otherPlayerIdx].connectedFlag != 0 &&
							g_players[otherPlayerIdx].hasCheckpointFlag != 0 &&
							g_players[otherPlayerIdx].boundFlightGroupIdx ==
								g_players[playerIdx].boundFlightGroupIdx) {
							break;
						}
						++otherPlayerIdx;
					}
					if (otherPlayerIdx == -1) {
						++g_players[playerIdx].currentSeatIdx;
						if (forcePilotFlag != NULL || g_players[playerIdx].currentSeatIdx > 2) {
							g_players[playerIdx].currentSeatIdx = 0;
						}
						Player_ResetGunnerSeatCameraState(playerIdx, g_players[playerIdx].objectIndex, 0);
					}
				}
				break;

			case OBJ_CorellianTransport2:
				if (g_players[playerIdx].hasCheckpointFlag == 0) {
					otherPlayerIdx = 0;
					for (;;) {
						if (otherPlayerIdx >= XWA_PLAYER_COUNT) {
							otherPlayerIdx = -1;
							break;
						}
						if (otherPlayerIdx != (int)playerIdx &&
							g_players[otherPlayerIdx].connectedFlag != 0 &&
							g_players[otherPlayerIdx].hasCheckpointFlag != 0 &&
							g_players[otherPlayerIdx].boundFlightGroupIdx ==
								g_players[playerIdx].boundFlightGroupIdx) {
							break;
						}
						++otherPlayerIdx;
					}
					if (otherPlayerIdx == -1) {
						++g_players[playerIdx].currentSeatIdx;
						if (forcePilotFlag != NULL || g_players[playerIdx].currentSeatIdx > 1) {
							g_players[playerIdx].currentSeatIdx = 0;
						}
						Player_ResetGunnerSeatCameraState(playerIdx, g_players[playerIdx].objectIndex, 0);
					}
				}
				break;
		}

		objectIndex = g_players[playerIdx].objectIndex;
		objectType = g_objectTable[objectIndex].objectType;
		if (objectType >= OBJ_CorellianTransport2 &&
			(objectType <= OBJ_MilleniumFalcon2 || objectType == OBJ_FamilyTransport)) {
			if (g_players[playerIdx].currentSeatIdx != 0) {
				g_players[playerIdx].turretAutoFireState = 1;
				msg_emitInFlightMessage(MSG_GUNNER, (int)playerIdx);
			} else {
				g_players[playerIdx].turretAutoFireState = 0;
				msg_emitInFlightMessage(MSG_PILOTING, (int)playerIdx);
			}

			{
				uint16_t slotIdx;

				for (slotIdx = 0; slotIdx < craft->laserSlotCount; ++slotIdx) {
					if (craft->warheadData[slotIdx].weaponType >= 4u) {
						craft->warheadData[slotIdx].turretTargetObjIdx = -1;
					}
				}
			}
		}
	}

	if (g_players[playerIdx].currentSeatIdx > 0) {
		g_players[playerIdx].selectedWarhead = 0;
		g_players[playerIdx].selectedWeaponMode = 0;
		g_players[playerIdx].lookYawOffset = 0;
		g_players[playerIdx].lookPitchOffset = 0;
		if (playerIdx == (unsigned int)g_localPlayer) {
			g_padlockMouseLookEnabled = 0;
		}
	}
}

// FUNCTION: XWA 0x509410
void Player_StepExtView(int playerIdx) {
	PlayerData* player;
	PlayerViewState* viewState;
	int16_t currentTargetObjectIdx;

	player = &g_players[playerIdx];
	viewState = &player->viewState;

	if (player->mapCameraState != 0) {
		currentTargetObjectIdx = player->currentTargetObjectIdx;
		if ((uint16_t)currentTargetObjectIdx != 0xffffu) {
			uint16_t targetObjIdx;

			targetObjIdx = (uint16_t)currentTargetObjectIdx;
			viewState->cameraFocusObjIdx = targetObjIdx;
			viewState->cameraDistance =
				4 * g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent;
			if (player->mapCameraState > 1u) {
				player->mapCameraState = 1;
				viewState->hudAimX = 0;
			}
		}
		return;
	}

	viewState->externalCameraActive = (uint16_t)(viewState->externalCameraActive == 0);
	viewState->transitionTimer = 0;
	viewState->hudAimY = 0;
	viewState->hudAimX = 0;

	if (playerIdx == g_localPlayer && viewState->externalCameraActive == 0) {
		FlightObject_AnimateCrewMeshRotations((uint16_t)player->objectIndex, 1);
	}

	if (viewState->externalCameraActive != 0 && viewState->cameraFocusObjIdx == player->objectIndex) {
		viewState->savedHudStateByte = viewState->hudStateLive;
		viewState->savedHudAimX = viewState->hudAimX;
		viewState->savedHudAimY = viewState->hudAimY;
	}

	Player_UpdateHudViewForCameraFocus(playerIdx);
}

// FUNCTION: XWA 0x505F20
void Player_ComputePolarToObjectRef(int playerIdx, unsigned int objectRef) {
	unsigned int objectIndex;

	objectIndex = (unsigned int)g_players[playerIdx].objectIndex;
	if (objectIndex == 0xffffu) {
		int relativeX;
		int relativeY;
		int relativeZ;

		Mission_ResolveObjectOrMissionPointWorldLoc(objectRef, 0, 0, 0);
		relativeX = worldlocx;
		relativeY = worldlocy;
		relativeZ = worldlocz;
		trig2_ctop(relativeX - g_players[playerIdx].viewState.savedTargetX,
				   relativeY - g_players[playerIdx].viewState.savedTargetY,
				   relativeZ - g_players[playerIdx].viewState.savedTargetZ);
	} else {
		pai_ObjectRefDirectionToObjectRef(objectIndex, objectRef);
	}
}

// FUNCTION: XWA 0x504A70
int8_t Player_CanRadioCommandCraft(unsigned int targetObjIdx, int playerIdx) {
	uint8_t radio;
	uint16_t playerFlightGroupIdx;
	uint8_t targetFlightGroupIdx;

	if (targetObjIdx == 0xffffu) {
		return 0;
	}

	if (targetObjIdx >= g_activeRegionCraftObjectSlotEnd) {
		return 0;
	}

	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
		return 0;
	}

	if (g_objectTable[targetObjIdx].mobj != NULL) {
		g_curCraft = g_objectTable[targetObjIdx].mobj->pCraft;
		if (g_curCraft != NULL) {
			if (g_curCraft->objectKind != 0) {
				return 0;
			}

			if (g_curCraft->workingSubsystems == 0) {
				return 0;
			}
		}
	}

	targetFlightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
	radio = g_missionFlightGroups[targetFlightGroupIdx].fg.radio;
	if (radio == 0) {
		return 0;
	}

	playerFlightGroupIdx = g_players[playerIdx].boundFlightGroupIdx;
	if (targetFlightGroupIdx == playerFlightGroupIdx) {
		return 1;
	}

	if (g_missionFlightGroups[targetFlightGroupIdx].fg.team == g_players[playerIdx].playerIff) {
		return 1;
	}

	if (g_missionFlightGroups[targetFlightGroupIdx].fg.globalUnit != 0 &&
		g_missionFlightGroups[targetFlightGroupIdx].fg.globalUnit ==
			g_missionFlightGroups[playerFlightGroupIdx].fg.globalUnit) {
		return 1;
	}

	if (radio == g_missionFlightGroups[playerFlightGroupIdx].fg.playerNumber + 8u) {
		return 1;
	}

	return radio == (uint16_t)g_players[playerIdx].playerIff + 1u;
}

// FUNCTION: XWA 0x503E10
void Player_SetTarget(uint16_t newTargetObjIdx, unsigned int playerIdx) {
	unsigned int targetObjIdx;
	int playerObjIdx;
	int targetingAvailable;

	if ((uint16_t)newTargetObjIdx == 0xffffu) {
		return;
	}

	targetObjIdx = (uint16_t)newTargetObjIdx;
	if (g_objectTable[targetObjIdx].objectType == OBJ_None) {
		return;
	}
	if (g_objectTable[targetObjIdx].genusId == GENUS_Explosion) {
		return;
	}

	playerObjIdx = g_players[playerIdx].objectIndex;
	if (playerObjIdx == targetObjIdx && !g_inHangarReady) {
		return;
	}

	targetingAvailable = 1;
	if (playerObjIdx != 0xffff) {
		if ((g_objectTable[playerObjIdx].mobj->pCraft->workingSubsystems & 4u) == 0) {
			targetingAvailable = 0;
		}
	}
	if (targetingAvailable) {
		if ((uint16_t)newTargetObjIdx == (uint16_t)g_players[playerIdx].currentTargetObjectIdx) {
			return;
		}

		fsfx_PlaySound(75, 0xffffu, playerIdx);
		g_players[playerIdx].currentTargetObjectIdx = (int16_t)newTargetObjIdx;
		g_players[playerIdx].selectedTargetComponent = 0;
		g_players[playerIdx].targetSubState = 0;

		if (targetObjIdx >= g_activeRegionObjectSlotStart &&
			targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
			if (g_players[playerIdx].mapCameraState) {
				int meshCount;
				uint16_t meshIdx;

				meshCount = ModelMesh_GetObjectTypeMeshCount(
					g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].objectType);
				for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
					MeshType meshType;

					meshType = ModelMesh_GetObjectTypeMeshType(
						g_objectTable[(uint16_t)g_players[playerIdx].currentTargetObjectIdx].objectType,
						meshIdx);
					if (meshType == MESH_MainHull || meshType == MESH_Fuselage) {
						g_players[playerIdx].selectedTargetComponent = (int16_t)(uint16_t)meshIdx;
						break;
					}
				}
			} else {
				ObjectRecord* objectTable;
				uint16_t selectedMeshIdx;
				uint16_t targetGenus;
				uint16_t nearestMeshIdx;
				unsigned int bestDistance;

				objectTable = g_objectTable;
				selectedMeshIdx = 0;
				bestDistance = 0x1000000u;
				nearestMeshIdx = 0;
				targetGenus = objectTable[targetObjIdx].genusId;
				if (targetGenus == GENUS_Starship || targetGenus == GENUS_Platform) {
					int meshCount;
					uint16_t meshIdx;

					meshCount = ModelMesh_GetObjectTypeMeshCount(objectTable[targetObjIdx].objectType);
					for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
						MeshType meshType;

						meshType =
							ModelMesh_GetObjectTypeMeshType(g_objectTable[targetObjIdx].objectType, meshIdx);
						if (meshType == MESH_MainHull || meshType == MESH_Fuselage) {
							unsigned int distance;

							distance = Object_DirectionAndDistanceToMeshCenter(
								g_players[playerIdx].objectIndex, newTargetObjIdx, meshIdx);
							if (distance < bestDistance) {
								nearestMeshIdx = (uint16_t)meshIdx;
								bestDistance = distance;
							}
						}
					}
					selectedMeshIdx = nearestMeshIdx;
				} else {
					int meshCount;

					meshCount = ModelMesh_GetObjectTypeMeshCount(objectTable[targetObjIdx].objectType);
					for (;;) {
						MeshType meshType;

						if (selectedMeshIdx >= meshCount) {
							selectedMeshIdx = 0;
							break;
						}
						meshType = ModelMesh_GetObjectTypeMeshType(g_objectTable[targetObjIdx].objectType,
																   selectedMeshIdx);
						if (meshType == MESH_MainHull || meshType == MESH_Fuselage) {
							break;
						}
						++selectedMeshIdx;
					}
				}

				g_players[playerIdx].selectedTargetComponent = (int16_t)selectedMeshIdx;
				if (g_provingGroundsModeActive) {
					unsigned int targetObjectType;

					targetObjectType = (uint16_t)g_objectTable[targetObjIdx].objectType;
					switch (targetObjectType) {
						case OBJ_AccelRing2:
							if (g_objectTable[targetObjIdx].mobj->pCraft->componentHp[4] != 0) {
								g_players[playerIdx].selectedTargetComponent = 4;
							}
							break;
						case OBJ_AccelRing3:
							if (g_objectTable[targetObjIdx].mobj->pCraft->componentHp[4] != 0) {
								g_players[playerIdx].selectedTargetComponent = 4;
							}
							break;
					}
				}
			}
		}

		g_players[playerIdx].missileLockState = 0;
		playerObjIdx = g_players[playerIdx].objectIndex;
		if (playerObjIdx != 0xffff) {
			CraftData* craft;

			craft = g_objectTable[playerObjIdx].mobj->pCraft;
			craft->warheadLockTicks = 0;
		}

		if (g_players[playerIdx].mapCameraState) {
			msg_BuildTargetDescription((int16_t)newTargetObjIdx, (int)playerIdx, 1, 0);
			return;
		}

		playerObjIdx = g_players[playerIdx].objectIndex;
		if ((g_objectTable[playerObjIdx].mobj->pCraft->activeHudFeatureMask & 1u) == 0) {
			return;
		}

		if (g_players[playerIdx].viewState.hudStateLive == 19 ||
			g_players[playerIdx].viewState.hudStateLive == 0 ||
			g_players[g_localPlayer].viewState.hudStateLive == 20) {
			msg_BuildTargetDescription((int16_t)newTargetObjIdx, (int)playerIdx, 1, 0);
		}
	} else {
		g_msgArgTable[0] = MSG_TARGCOMPUTER;
		g_msgArgTable[1] = MSG_DAMAGED;
		msg_emitInFlightMessage(MSG_SYSTEMCOND, (int)playerIdx);
	}
}

static __inline char Player_TargetHasActiveDecoyBeam(uint16_t objIdx, ObjectRecord* obj, MobileObject* mobj) {
	CraftData* craft;

	if (objIdx == 0xffffu || objIdx >= g_activeRegionCraftObjectSlotEnd) {
		return 0;
	}

	if (obj->playerOwnerIdx == -1) {
		return 0;
	}

	craft = mobj->pCraft;
	if (craft == NULL) {
		return 0;
	}

	return (craft->workingSubsystems & 0x100u) != 0 && craft->beamActive != 0 && craft->beamTypeId == 3 &&
		   craft->beamTimer != 0;
}

// FUNCTION: XWA 0x5061C0
void Player_ValidateCurrentTargets(int playerIdx) {
	int16_t originalTargetObjIdx;
	uint16_t targetObjIdx;
	ObjectRecord* targetObj;
	MobileObject* targetMobj;
	int playerObjIdx;
	uint8_t invalidTargetGenus;

	originalTargetObjIdx = g_players[playerIdx].currentTargetObjectIdx;
	invalidTargetGenus = GENUS_Explosion;

	if ((uint16_t)originalTargetObjIdx != 0xffffu) {
		targetObjIdx = (uint16_t)originalTargetObjIdx;
		targetObj = &g_objectTable[targetObjIdx];
		targetMobj = targetObj->mobj;

		if (targetMobj != NULL) {
			if (targetObj->objectType == OBJ_None || targetObj->genusId == invalidTargetGenus) {
				g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
			} else if (Player_TargetHasActiveDecoyBeam(targetObjIdx, targetObj, targetMobj)) {
				g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
			} else {
				CraftData* targetCraft;

				targetCraft = targetMobj->pCraft;
				if (targetCraft != NULL && targetCraft->objectKind == GENUS_NpcProjectile) {
					g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
				}
			}
		} else if (targetObj->objectType == OBJ_None || targetObj->genusId == invalidTargetGenus) {
			g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
		}

		playerObjIdx = g_players[playerIdx].objectIndex;
		if (playerObjIdx != 0xffff && !g_players[playerIdx].mapCameraState &&
			(g_objectTable[playerObjIdx].mobj->pCraft->workingSubsystems & 4u) == 0) {
			g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
		}

		if (g_players[playerIdx].currentTargetObjectIdx == 0xffffu) {
			g_players[playerIdx].targetCycleStart = originalTargetObjIdx;
			g_players[playerIdx].targetingState = 0;
		}
	}

	if (g_players[playerIdx].turretAutoFireState == 2) {
		playerObjIdx = g_players[playerIdx].objectIndex;
		if (playerObjIdx != 0xffff) {
			CraftData* craft;
			int i;

			craft = g_objectTable[playerObjIdx].mobj->pCraft;
			for (i = 0; i < craft->laserSlotCount; ++i) {
				ObjectRecord* turretTargetObj;

				if (craft->warheadData[i].weaponType >= 4u &&
					craft->warheadData[i].turretTargetObjIdx != -1) {
					turretTargetObj = &g_objectTable[(uint16_t)craft->warheadData[i].turretTargetObjIdx];
					if (turretTargetObj->objectType == OBJ_None ||
						turretTargetObj->genusId == GENUS_Explosion) {
						craft->warheadData[i].turretTargetObjIdx = -1;
						g_players[playerIdx].turretAutoFireState = 0;
					}
				}
			}
		}
	}
}

// FUNCTION: XWA 0x505FA0
void Player_EndFlightParticipation(int playerIdx) {
	PlayerData* player;
	int activePlayerCount;
	int i;

	player = &g_players[playerIdx];
	player->connectedFlag = 2;
	activePlayerCount = 0;
	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		if (g_players[i].connectedFlag == 1) {
			++activePlayerCount;
		}
	}

	if (activePlayerCount != 0) {
		Hud_EnterPlayerMapView(playerIdx);
		g_players[playerIdx].mapCameraState = 0xffu;
		Hud_SetHudViewState(21, playerIdx);
		g_players[playerIdx].viewState.playerInputBlocked = 1;
		g_players[playerIdx].viewState.externalCameraActive = 1;
		g_players[playerIdx].viewState.transitionTimer = 0;
		g_players[playerIdx].viewState.cameraDistance = 0x40000;
		g_players[playerIdx].viewState.cameraFocusObjIdx = 0xffff;
		g_players[playerIdx].viewState.aimTargetIdx = 0xffff;
		g_players[playerIdx].viewState.savedTargetZ = 0x40000;
		if (g_players[playerIdx].currentTargetObjectIdx != 0xffffu) {
			ObjectRecord* objectTable;

			objectTable = g_objectTable;
			g_players[playerIdx].viewState.savedTargetX =
				g_objectTable[g_players[playerIdx].currentTargetObjectIdx].world_x;
			g_players[playerIdx].viewState.savedTargetY =
				objectTable[g_players[playerIdx].currentTargetObjectIdx].world_y;
			g_players[playerIdx].viewState.cameraDistance =
				16 * g_modelTypeTable[(uint16_t)objectTable[g_players[playerIdx].currentTargetObjectIdx]
										  .objectType]
						 .maxBoundsExtent;
		}
		g_players[playerIdx].regionSessionId = 0;
		g_players[playerIdx].objectIndex = 0xffff;
		g_players[playerIdx].pendingActionTimer = 0;
		g_players[playerIdx].pendingActionId = 0;
		fsfx_UpdatePlayerEngineLoop();
		fsfx_UpdateChaffLoop();
		fsfx_UpdateBeamEffectLoops();
		return;
	}

	g_flightMissionEndPending = 1;
	if (g_flightPlayerCount == 1) {
		unsigned int playerIff;

		playerIff = (uint16_t)g_players[playerIdx].playerIff;
		if (g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] == 1) {
			if (!g_flightExitRequest && g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] = 2;
			}
			if (Mission_ShouldApplyEndMissionPenalty((unsigned int)playerIdx)) {
				g_missionFlightRuntimeState
					.teamGoalStatus[(uint16_t)g_players[playerIdx].playerIff][TEAM_GOAL_PRIMARY] = 2;
			}
		}
	}
}

// FUNCTION: XWA 0x506370
void Player_ValidateAllCurrentTargets(void) {
	int playerIdx;

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		if (g_players[playerIdx].connectedFlag) {
			Mission_SetActiveRegionObjectRanges(g_players[playerIdx].regionIndex);
			Player_ValidateCurrentTargets(playerIdx);
		}
	}
}

// FUNCTION: XWA 0x502A60
uint16_t Player_CycleTargetAnyIFF(int startObjIdx, int direction, int playerIdx) {
	uint32_t firstObjIdx;
	uint32_t endObjIdx;
	uint32_t remaining;
	uint16_t objectCount;
	int playerObjectIdx;
	int playerIdxLocal;
	ObjectRecord* obj;

	firstObjIdx = g_activeRegionObjectSlotStart;
	endObjIdx = g_regionStaticObjectSlotEnd;
	remaining = endObjIdx;
	remaining -= firstObjIdx;
	objectCount = (uint16_t)remaining;
	remaining += 0xffffu;
	if (objectCount == 0) {
		return 0xffffu;
	}

	playerIdxLocal = playerIdx;
	playerObjectIdx = g_players[playerIdxLocal].objectIndex;
	for (;;) {
		uint32_t resultObjIdx;

		startObjIdx += direction;
		if ((uint16_t)startObjIdx >= 0x8000u || (uint16_t)startObjIdx < firstObjIdx) {
			startObjIdx = (int)(endObjIdx - 1);
		} else if ((uint16_t)startObjIdx >= endObjIdx) {
			startObjIdx = (int)firstObjIdx;
		}

		resultObjIdx = (uint16_t)startObjIdx;
		if (resultObjIdx != (uint32_t)playerObjectIdx) {
			uint16_t objectType;

			obj = &g_objectTable[resultObjIdx];
			objectType = obj->objectType;
			if (g_provingGroundsModeActive) {
				int checkpointObjIdx;

				checkpointObjIdx =
					g_yardContext
						.courseSide1Checkpoints[g_yardContext.playerChallengeStates[playerIdxLocal]
													.nextCheckpointIdx]
						.objectIdx;
				if (g_yardContext.playerChallengeStates[playerIdxLocal].nextCourseSide != 1) {
					checkpointObjIdx =
						g_yardContext
							.courseSide2Checkpoints[g_yardContext.playerChallengeStates[playerIdxLocal]
														.nextCheckpointIdx]
							.objectIdx;
				}
				if (checkpointObjIdx != (int)resultObjIdx && objectType >= OBJ_ChuteMouth &&
					objectType <= OBJ_MoltenBlock) {
					goto continue_search;
				}
			}

			if (objectType != OBJ_None &&
				(g_modelTypeTable[(uint16_t)g_objectTable[resultObjIdx].objectType].flags &
				 MODEL_TYPE_FLAG_FILM_OVERLAY_SELECTABLE) != 0) {
				MobileObject* mobj;

				mobj = obj->mobj;
				if (mobj == NULL) {
					return (uint16_t)startObjIdx;
				}

				if (obj->genusId != GENUS_Explosion) {
					char beamActive;

					if ((uint16_t)startObjIdx != 0xffffu && resultObjIdx < g_activeRegionCraftObjectSlotEnd &&
						obj->playerOwnerIdx != -1 && mobj->pCraft != NULL &&
						(mobj->pCraft->workingSubsystems & 0x100u) != 0 && mobj->pCraft->beamActive != 0 &&
						mobj->pCraft->beamTypeId == 3 && mobj->pCraft->beamTimer != 0) {
						beamActive = 1;
					} else {
						beamActive = 0;
					}
					if (beamActive) {
						goto continue_search;
					}

					if (g_missionFlightGroups[obj->flightGroupIdx].fg.status1 != 27) {
						if (mobj->state != 0) {
							return (uint16_t)startObjIdx;
						}

						g_curCraft = mobj->pCraft;
						if (g_curCraft->objectKind != 3 && g_curCraft->objectKind != 4 &&
							g_curCraft->objectKind != 7) {
							return (uint16_t)startObjIdx;
						}
					}
				}
			}
		}

	continue_search:
		objectCount = (uint16_t)remaining;
		remaining += 0xffffu;
		if (objectCount == 0) {
			return 0xffffu;
		}
		endObjIdx = g_regionStaticObjectSlotEnd;
		firstObjIdx = g_activeRegionObjectSlotStart;
	}
}

// FUNCTION: XWA 0x502C50
uint16_t Player_CycleTarget(int startObjIdx, int direction, int playerIdx, int iffCategory, char modeFlags) {
	uint32_t firstObjIdx;
	uint32_t endObjIdx;
	uint32_t remaining;
	uint16_t objectCount;
	ObjectRecord* obj;
	MobileObject* mobj;

	firstObjIdx = g_activeRegionObjectSlotStart;
	endObjIdx = g_regionStaticObjectSlotEnd;
	remaining = endObjIdx;
	remaining -= firstObjIdx;
	objectCount = (uint16_t)remaining;
	remaining += 0xffffu;
	if (objectCount == 0) {
		return 0xffffu;
	}
	for (;;) {
		startObjIdx += direction;
		if ((uint16_t)startObjIdx >= 0x8000u || (uint16_t)startObjIdx < firstObjIdx) {
			startObjIdx = (int)(endObjIdx - 1);
		} else if ((uint16_t)startObjIdx >= endObjIdx) {
			startObjIdx = (int)firstObjIdx;
		}
		if ((uint16_t)startObjIdx != g_players[playerIdx].objectIndex &&
			((modeFlags & 4) == 0 || (uint16_t)startObjIdx < g_projectileObjectSlotStart ||
			 (uint16_t)startObjIdx >= g_projectileObjectSlotEnd) &&
			((modeFlags & 2) == 0 || (uint16_t)startObjIdx < g_objScanStart)) {
			uint16_t objectType;

			obj = &g_objectTable[(uint16_t)startObjIdx];
			objectType = g_objectTable[(uint16_t)startObjIdx].objectType;
			if (objectType != OBJ_None &&
				(g_modelTypeTable[(uint16_t)objectType].flags & MODEL_TYPE_FLAG_FILM_OVERLAY_SELECTABLE) !=
					0 &&
				((modeFlags & 1) == 0 || obj->genusId != GENUS_Mine)) {
				int team;
				int playerIff;

				mobj = obj->mobj;
				if (mobj == NULL) {
					team = g_missionFlightGroups[obj->flightGroupIdx].fg.team;
				} else {
					team = mobj->team;
				}

				switch (iffCategory) {
					case 0:
						goto target_iff_matches;
					case 1:
						playerIff = (uint16_t)g_players[playerIdx].playerIff;
						if (team != playerIff) {
							break;
						}
						goto target_iff_matches;
					case 2:
						playerIff = (uint16_t)g_players[playerIdx].playerIff;
						if (team == playerIff) {
							goto target_iff_matches;
						}
						if (g_missionTeams[playerIff].allies[team] != 1) {
							break;
						}
						goto target_iff_matches;
					case 3:
						playerIff = (uint16_t)g_players[playerIdx].playerIff;
						if (team == playerIff) {
							break;
						}
						if (g_missionTeams[playerIff].allies[team] != 0) {
							break;
						}
						goto target_iff_matches;
					case 4:
						if (obj->playerOwnerIdx == -1) {
							break;
						}
						goto target_iff_matches;
					case 5:
						playerIff = (uint16_t)g_players[playerIdx].playerIff;
						if (team == playerIff) {
							break;
						}
						if (g_missionTeams[playerIff].allies[team] == 1) {
							break;
						}
						if (team == (uint16_t)g_players[playerIdx].playerIff ||
							g_missionTeams[(uint16_t)g_players[playerIdx].playerIff].allies[team] != 0) {
							goto target_iff_matches;
						}
						break;
					default:
					target_iff_matches:
						if (mobj == NULL) {
							return (uint16_t)startObjIdx;
						}

						if (obj->genusId != GENUS_Explosion &&
							((uint16_t)startObjIdx == 0xffffu ||
							 (uint16_t)startObjIdx >= g_activeRegionCraftObjectSlotEnd ||
							 obj->playerOwnerIdx == -1 || mobj->pCraft == NULL ||
							 (mobj->pCraft->workingSubsystems & 0x100u) == 0 ||
							 mobj->pCraft->beamActive == 0 || mobj->pCraft->beamTypeId != 3 ||
							 mobj->pCraft->beamTimer == 0) &&
							g_missionFlightGroups[obj->flightGroupIdx].fg.status1 != 27) {
							if (mobj->state != 0) {
								return (uint16_t)startObjIdx;
							}

							g_curCraft = mobj->pCraft;
							if (g_curCraft->objectKind != GENUS_Freighter &&
								g_curCraft->objectKind != GENUS_Starship &&
								g_curCraft->objectKind != GENUS_NpcProjectile) {
								return (uint16_t)startObjIdx;
							}
						}
						break;
				}
			}
		}

		objectCount = (uint16_t)remaining;
		remaining += 0xffffu;
		if (objectCount == 0) {
			return 0xffffu;
		}
		endObjIdx = g_regionStaticObjectSlotEnd;
		firstObjIdx = g_activeRegionObjectSlotStart;
	}
}

static __inline void Player_BackDeathCameraAwayFromObject(unsigned int playerIdx) {
	ObjectRecord* obj;
	MobileObject* mobj;
	int modelIndex;
	int modelForwardOffset;

	obj = &g_objectTable[g_players[playerIdx].objectIndex];
	mobj = obj->mobj;
	if (mobj == NULL) {
		return;
	}

	modelIndex = GetModelIndexFromType(obj->objectType);
	if (modelIndex == 0xffff) {
		return;
	}

	modelForwardOffset =
		((int16_t)g_modelDefs[modelIndex].boundSizeY << (uint8_t)g_modelDefs[modelIndex].boundSizeShift) + 1;
	g_players[playerIdx].viewState.savedTargetX -= Xwa_Q15Mul(mobj->cachedFwdX, modelForwardOffset);
	g_players[playerIdx].viewState.savedTargetY -= Xwa_Q15Mul(mobj->cachedFwdY, modelForwardOffset);
	g_players[playerIdx].viewState.savedTargetZ -= Xwa_Q15Mul(mobj->cachedFwdZ, modelForwardOffset);
}

// FUNCTION: XWA 0x5064D0
// Per-step cleanup for player flight participation. Handles players whose craft
// binding disappeared or went stale, attempts skirmish/proving rebinding when
// allowed, ends participation when no craft remains (with checkpoint-partner
// handling), updates the player-count HUD messages, and processes explicit
// player abort/quit flags.
void Player_UpdateParticipationState(void) {
	int p;
	PlayerData* pl;

	for (p = 0, pl = g_players; p < g_flightPlayerCount; ++p, ++pl) {
		int objectIndex;
		int partner;

		if (pl->connectedFlag) {
			Mission_SetActiveRegionObjectRanges(pl->regionIndex);

			if (pl->regionSessionId) {
				// In-region player: detect a destroyed/replaced bound craft.
				if (!g_flightSimSideEffectsSuppressed) {
					objectIndex = pl->objectIndex;
					if (objectIndex != 0xFFFF && !pl->hasCheckpointFlag) {
						if (g_objectTable[objectIndex].objectType == OBJ_None ||
							pl->boundObjectSignature != g_objectTable[objectIndex].objectSignature) {
							Mission_ProcessFlightGroupWaveCompletion(pl->boundFlightGroupIdx);
							if ((g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH &&
								 !g_provingGroundsModeActive) ||
								Player_BindToAvailableCraft(p, 0xFFFFu, 0, 0)) {
								Player_EndFlightParticipation(p);
								if (p != g_localPlayer) {
									int activePlayerCount;
									unsigned int i;

									msg_addMessagePtr(0, NetSession_GetPlayerName(p));
									msg_emitInFlightMessage(MSG_PLAYER_NO_MORE, g_localPlayer);
									activePlayerCount = 0;
									i = 0;
									do {
										if (g_players[i].connectedFlag == 1) {
											++activePlayerCount;
										}
										++i;
									} while (i < (unsigned)XWA_PLAYER_COUNT);
									if (g_players[g_localPlayer].connectedFlag == 1) {
										if (activePlayerCount == 1) {
											msg_emitInFlightMessage(MSG_1_PLAYER, g_localPlayer);
										} else {
											g_msgArgTable[0] = (uint16_t)activePlayerCount;
											msg_emitInFlightMessage(MSG_MORE_PLAYERS, g_localPlayer);
										}
									}
								}

								partner = 0;
								for (;;) {
									if (partner != p && g_players[partner].connectedFlag &&
										g_players[partner].hasCheckpointFlag &&
										g_players[partner].boundFlightGroupIdx == pl->boundFlightGroupIdx) {
										break;
									}
									++partner;
									if (partner < XWA_PLAYER_COUNT) {
										continue;
									}
									partner = -1;
									break;
								}
								if (partner != -1) {
									Player_EndFlightParticipation(partner);
									if (partner != g_localPlayer) {
										int activePlayerCount;
										unsigned int i;

										msg_addMessagePtr(0, NetSession_GetPlayerName(partner));
										msg_emitInFlightMessage(MSG_PLAYER_NO_MORE, g_localPlayer);
										activePlayerCount = 0;
										i = 0;
										do {
											if (g_players[i].connectedFlag == 1) {
												++activePlayerCount;
											}
											++i;
										} while (i < (unsigned)XWA_PLAYER_COUNT);
										if (g_players[g_localPlayer].connectedFlag == 1) {
											if (activePlayerCount == 1) {
												msg_emitInFlightMessage(MSG_1_PLAYER, g_localPlayer);
											} else {
												g_msgArgTable[0] = (uint16_t)activePlayerCount;
												msg_emitInFlightMessage(MSG_MORE_PLAYERS, g_localPlayer);
											}
										}
									}
								}
							} else {
								if (p == g_localPlayer) {
									msg_emitLocalPlayerCraftMessage(MSG_PREVIOUS_DESTROYED);
								}

								partner = 0;
								for (;;) {
									if (partner != p && g_players[partner].connectedFlag &&
										g_players[partner].hasCheckpointFlag &&
										g_players[partner].boundFlightGroupIdx == pl->boundFlightGroupIdx) {
										break;
									}
									++partner;
									if (partner < XWA_PLAYER_COUNT) {
										continue;
									}
									partner = -1;
									break;
								}
								if (partner != -1 && partner == g_localPlayer) {
									msg_emitLocalPlayerCraftMessage(MSG_PREVIOUS_DESTROYED);
								}
							}
						}
					}
				}
			} else {
				// Out-of-region / map-camera player: end participation only when no
				// owned craft remains, even after running wave completion.
				if (pl->mapCameraState && pl->connectedFlag != 2 && !g_flightSimSideEffectsSuppressed) {
					if (!Player_HasAvailableOwnedCraft(p)) {
						Mission_ProcessFlightGroupWaveCompletion(pl->boundFlightGroupIdx);
						if (!Player_HasAvailableOwnedCraft(p)) {
							Player_EndFlightParticipation(p);
							if (p != g_localPlayer) {
								int activePlayerCount;
								unsigned int i;

								msg_addMessagePtr(0, NetSession_GetPlayerName(p));
								msg_emitInFlightMessage(MSG_PLAYER_NO_MORE, g_localPlayer);
								activePlayerCount = 0;
								i = 0;
								do {
									if (g_players[i].connectedFlag == 1) {
										++activePlayerCount;
									}
									++i;
								} while (i < (unsigned)XWA_PLAYER_COUNT);
								if (g_players[g_localPlayer].connectedFlag == 1) {
									if (activePlayerCount == 1) {
										msg_emitInFlightMessage(MSG_1_PLAYER, g_localPlayer);
									} else {
										g_msgArgTable[0] = (uint16_t)activePlayerCount;
										msg_emitInFlightMessage(MSG_MORE_PLAYERS, g_localPlayer);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// Explicit abort/quit flags: detach, clear the slot, sync, and broadcast.
	if (!g_flightSimSideEffectsSuppressed) {
		p = 0;
		do {
			if (g_playerAbortFlags[p] && g_players[p].connectedFlag) {
				if (g_players[p].objectIndex != 0xFFFF)
					Player_UnbindFromCurrentCraft(p, 0, 1);
				if (p == g_localPlayer)
					g_flightMissionEndPending = 1;
				g_players[p].connectedFlag = 0;
				Flight_UpdateActivePlayerCount();
				if (NetSession_GetHostDplayId() != g_players[p].network.directPlayId)
					FlightNet_MarkPilotNetworkPlayerLeft(p);
				msg_addMessagePtr(0, NetSession_GetPlayerName(p));
				msg_emitInFlightMessage(MSG_PLAYER_QUIT, g_localPlayer);
				if (p == g_localPlayer && NetSession_GetLocalPlayerId())
					FlightNet_BroadcastLocalPlayerLeft();
			}
			++p;
		} while (p < XWA_PLAYER_COUNT);
	}
}

// FUNCTION: XWA 0x507D60
// Handles destruction/loss of a player's current (or map-camera) craft. Saves
// player craft settings, processes flight-group wave completion for map-camera
// exits, records the craft loss and top attacker, notifies a linked checkpoint
// player, and finally turns the craft object into a tumbling destroyed object
// with a short lifetime. Returns 20 (the craft's post-destruction speed floor),
// or the fsfx_PlaySound result on the hyperspace/replay early-out.
int Player_HandleCraftDestruction(unsigned int playerIdx) {
	int altViewObjectIdx;
	int checkpointPlayerIdx;
	int i;
	CraftData* pCraft;
	int topAttackerPlayerIdx;
	uint16_t modelIndex;
	uint16_t maxTumbleAngle;
	uint16_t spinRate;
	MobileObject* mobj;

	pCraft = NULL;
	i = 0;
	altViewObjectIdx = g_players[playerIdx].mapCameraState ? g_players[playerIdx].altViewObjectIdx
														   : g_players[playerIdx].objectIndex;

	// Find a connected checkpoint partner bound to the same craft object.
	checkpointPlayerIdx = -1;
	do {
		if (g_players[i].connectedFlag && g_players[i].hasCheckpointFlag &&
			g_players[i].objectIndex == altViewObjectIdx)
			checkpointPlayerIdx = i;
		++i;
	} while (i < XWA_PLAYER_COUNT);

	if (altViewObjectIdx != 0xFFFF)
		pCraft = g_objectTable[altViewObjectIdx].mobj->pCraft;

	// Proving-grounds: stamp recovery state and impose a short respawn penalty.
	if (g_provingGroundsModeActive && altViewObjectIdx != 0xFFFF) {
		int seconds;
		int penaltyUntilSeconds;

		seconds = Mission_GameTimeToSeconds(g_missionElapsedClock.hours, g_missionElapsedClock.minutes,
											g_missionElapsedClock.seconds);
		Yard_SavePlayerRecoveryState(altViewObjectIdx);
		penaltyUntilSeconds = g_yardContext.playerChallengeStates[playerIdx].penaltyUntilSeconds;
		if (penaltyUntilSeconds > seconds)
			g_yardContext.playerChallengeStates[playerIdx].penaltyUntilSeconds = penaltyUntilSeconds + 5;
		else
			g_yardContext.playerChallengeStates[playerIdx].penaltyUntilSeconds = seconds + 5;
	}

	Player_SaveCraftSettings(playerIdx);
	if (checkpointPlayerIdx != -1)
		Player_SaveCraftSettings(checkpointPlayerIdx);

	if (g_players[playerIdx].mapCameraState) {
		Mission_ProcessFlightGroupWaveCompletion(g_players[playerIdx].boundFlightGroupIdx);
		g_players[playerIdx].mapCameraState = 0;
		if (!Player_BindToAvailableCraft(playerIdx, 0xFFFFu, g_players[playerIdx].boundObjectSignature, 0)) {
			Hud_RestorePlayerHudState(playerIdx);
			g_players[playerIdx].mapCameraState = 0;
			g_players[playerIdx].altViewObjectIdx = 0xFFFF;
			if (playerIdx == (unsigned int)g_localPlayer)
				ForceFeedback_EnableEffects();
		}
	}

	if (g_players[playerIdx].hyperspacePhase || g_replayViewMode)
		return fsfx_PlaySound(68, 0xFFFFu, playerIdx);

	topAttackerPlayerIdx = Mission_RecordPlayerCraftLoss(altViewObjectIdx, 1);
	Player_StartPostDestructionState(playerIdx, 0xFFFFu, topAttackerPlayerIdx);
	if (checkpointPlayerIdx != -1)
		Player_StartPostDestructionState(checkpointPlayerIdx, 0xFFFFu, topAttackerPlayerIdx);

	// Turn the craft into a tumbling destroyed object with a short lifetime.
	modelIndex = pCraft->modelIndex;
	spinRate = (uint16_t)((GameRand() & 0x3FFF) + 0x2000);
	maxTumbleAngle = g_modelDefs[modelIndex].maxTumbleAngle;
	while (spinRate > maxTumbleAngle)
		spinRate >>= 1;
	g_objectTable[altViewObjectIdx].mobj->rollImpulseRate = 0;
	g_objectTable[altViewObjectIdx].mobj->spinRate = spinRate;
	g_objectTable[altViewObjectIdx].mobj->spinRateFrac = 0;
	g_objectTable[altViewObjectIdx].mobj->renderOffsetX = 0;
	g_objectTable[altViewObjectIdx].mobj->renderOffsetY = 0;
	g_objectTable[altViewObjectIdx].mobj->renderOffsetZ = 0;
	MobileObject_SetRandomSpinAxis(altViewObjectIdx);
	pCraft->objectKind = 3;
	g_objectTable[altViewObjectIdx].mobj->lifetimeTimer = 472;
	mobj = g_objectTable[altViewObjectIdx].mobj;
	if (mobj->speed < 20u)
		mobj->speed = 20;
	return 20;
}

// FUNCTION: XWA 0x5056F0
void Player_StartPostDestructionState(unsigned int playerIdx, unsigned int killerObjIdx,
									  int killerPlayerIdx) {
	char nameBuffer[64];
	char assistNameBuffer[64];

	g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;

	if (g_players[playerIdx].mapCameraState) {
		if ((int)playerIdx == g_localPlayer) {
			Hud_ClearReadyMessageQueue();
			g_players[playerIdx].regionSessionId = 1;
		} else {
			g_players[playerIdx].regionSessionId = 1;
		}
		return;
	}

	if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
		g_filmOverlayActive = 0;
		Hud_SyncLocalSoftwareHudMasks(1);
	}

	g_players[playerIdx].viewState.cameraFocusObjIdx = 0xffff;
	g_players[playerIdx].viewState.savedTargetX =
		g_objectTable[g_players[playerIdx].objectIndex].mobj->prevWorldX;
	g_players[playerIdx].viewState.savedTargetY =
		g_objectTable[g_players[playerIdx].objectIndex].mobj->prevWorldY;
	g_players[playerIdx].viewState.savedTargetZ =
		g_objectTable[g_players[playerIdx].objectIndex].mobj->prevWorldZ;
	Player_BackDeathCameraAwayFromObject(playerIdx);
	Player_BackDeathCameraAwayFromObject(playerIdx);

	g_players[playerIdx].viewState.externalCameraActive = 1;
	g_players[playerIdx].viewState.transitionTimer = 0;
	g_players[playerIdx].viewState.playerInputBlocked = 1;
	g_players[playerIdx].padlockActive = 0;
	g_players[playerIdx].viewState.hudAimX = 0;
	g_players[playerIdx].viewState.hudAimY = 0;
	g_players[playerIdx].lookYawOffset = 0;
	g_players[playerIdx].lookPitchOffset = 0;

	Hud_SetHudEnabled((int)playerIdx, 0);
	if ((int)playerIdx == g_localPlayer) {
		Hud_ClearReadyMessageQueue();
	}
	fsfx_UpdateBeamSystemLoop(0, (int)playerIdx);
	fsfx_UpdateIncomingMissileWarning(0);
	fsfx_PlaySound(66, 0xffffu, playerIdx);
	Hud_SetHudViewState(18, (int)playerIdx);

	if ((int)playerIdx == g_localPlayer && killerObjIdx != 0xffffu &&
		killerObjIdx >= (unsigned int)g_activeRegionObjectSlotStart &&
		killerObjIdx < (unsigned int)g_activeRegionCraftObjectSlotEnd) {
		if (g_activeFlightPlayerCount > 1) {
			if (killerPlayerIdx != -1 && killerPlayerIdx != g_objectTable[killerObjIdx].playerOwnerIdx &&
				g_players[killerPlayerIdx].objectIndex != 0xffff) {
				Player_AppendKillMessageActorName(0, nameBuffer, (int)killerObjIdx);
				Player_AppendKillMessageActorName(1u, assistNameBuffer,
												  g_players[killerPlayerIdx].objectIndex);
				msg_emitInFlightMessage(MSG_KILLED_BY_MOST_DAMAGE, g_localPlayer);
			} else {
				Player_AppendKillMessageActorName(0, nameBuffer, (int)killerObjIdx);
				msg_emitInFlightMessage(MSG_KILLED_BY, g_localPlayer);
			}
		} else {
			Hud_AppendObjectDisplayName((uint16_t)killerObjIdx, 3);
			msg_addMessagePtr(0, g_flightTextScratchBuffer);
			msg_emitInFlightMessage(MSG_KILLED_BY, g_localPlayer);
		}
	}

	g_players[playerIdx].regionSessionId = 1;
}

// FUNCTION: XWA 0x505AF0
int Player_AppendKillMessageActorName(unsigned int msgSlot, char* nameBuffer, int objIdx) {
	ObjectRecord* obj;
	ObjectRecord* objectTable;
	CraftData* craft;
	int ownerPlayerIdx;
	int playerIff;
	int team;
	char* actorName;

	objectTable = g_objectTable;
	ownerPlayerIdx = objectTable[objIdx].playerOwnerIdx;
	craft = objectTable[objIdx].mobj->pCraft;

	if (!g_flightLocatePlayersEnabled &&
		(playerIff = (uint16_t)g_players[g_localPlayer].playerIff,
		 (int8_t)craft->iffVisibility[playerIff] < 1) &&
		(obj = &objectTable[(uint16_t)objIdx],
		 obj->mobj != NULL ? (team = obj->mobj->team)
						   : (team = g_missionFlightGroups[obj->flightGroupIdx].fg.team),
		 team != playerIff) &&
		g_missionTeams[playerIff].allies[team] == 0) {
		if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
			g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
			Hud_AppendObjectDisplayName(objIdx, 1);
		} else {
			Hud_AppendObjectDisplayName(objIdx, 3);
		}
		actorName = g_flightTextScratchBuffer;
	} else if (ownerPlayerIdx != -1) {
		actorName = NetSession_GetPlayerName(ownerPlayerIdx);
	} else {
		Hud_AppendObjectDisplayName(objIdx, 3);
		actorName = g_flightTextScratchBuffer;
	}

	strcpy(nameBuffer, actorName);
	msg_addMessagePtr(msgSlot, nameBuffer);
	return 1;
}

// FUNCTION: XWA 0x5063B0
int Player_HasAvailableOwnedCraft(int playerIdx) {
	int savedRegionIdx;
	int regionCursor;
	int found;
	int remainingRegions;

	savedRegionIdx = regionIdx;
	regionCursor = g_players[playerIdx].regionIndex;
	found = 0;
	remainingRegions = g_missionRegionCount - 1;

	if (remainingRegions != 0) {
		do {
			uint32_t objectSlotStart;
			uint32_t slotCount;
			uint32_t objIdx;

			Mission_SetActiveRegionObjectRanges(regionCursor);
			objectSlotStart = g_activeRegionObjectSlotStart;
			slotCount = g_activeRegionCraftObjectSlotEnd - objectSlotStart;
			objIdx = objectSlotStart - 1u;

			if (slotCount != 0) {
				ObjectRecord* objectTable;

				objectTable = g_objectTable;
				while (slotCount != 0) {
					ObjectRecord* obj;

					++objIdx;
					if (objIdx >= g_activeRegionCraftObjectSlotEnd) {
						objIdx = objectSlotStart;
					}
					if (objIdx < objectSlotStart) {
						objIdx = objectSlotStart;
					}

					obj = &objectTable[objIdx];
					if (obj->objectType != OBJ_None) {
						CraftData* craft;

						craft = obj->mobj->pCraft;
						if (g_missionFlightGroups[obj->flightGroupIdx].playerOwnerIdx == playerIdx) {
							uint8_t objectKind;

							objectKind = craft->objectKind;
							if (objectKind != GENUS_Freighter && objectKind != GENUS_Platform &&
								objectKind != GENUS_Starship && objectKind != GENUS_NpcProjectile) {
								found = 1;
								break;
							}
						}
					}

					--slotCount;
				}
			}

			if (found) {
				break;
			}

			++regionCursor;
			if (regionCursor >= g_missionRegionCount - 1) {
				regionCursor = 0;
			}
			--remainingRegions;
		} while (remainingRegions != 0);
	}

	Mission_SetActiveRegionObjectRanges(savedRegionIdx);
	return found;
}

// FUNCTION: XWA 0x501FF0
int16_t Player_FindNearestObjective(int goalCondType, int playerIdx) {
	unsigned int playerIff;
	uint32_t bestActionableDistance;
	uint16_t bestActionableObjIdx;
	uint32_t bestFallbackDistance;
	uint16_t bestFallbackObjIdx;
	uint32_t objIdx;

	playerIff = (uint16_t)g_players[playerIdx].playerIff;
	bestActionableDistance = UINT32_MAX;
	bestFallbackDistance = UINT32_MAX;
	bestActionableObjIdx = 0xffffu;
	bestFallbackObjIdx = 0xffffu;

	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
		ObjectTypeId objectType;
		unsigned int flightGroupIdx;
		int goalMatches;
		int goalIdx;
		unsigned int triggerCondition;

		objectType = g_objectTable[objIdx].objectType;
		if (objectType == OBJ_None || objIdx == (uint32_t)g_players[playerIdx].objectIndex ||
			(g_modelTypeTable[(uint16_t)objectType].flags & 3u) == 0) {
			continue;
		}

		if ((uint16_t)objIdx != 0xffffu && (uint16_t)objIdx < g_activeRegionCraftObjectSlotEnd &&
			g_objectTable[(uint16_t)objIdx].playerOwnerIdx != -1) {
			CraftData* craft;

			craft = g_objectTable[(uint16_t)objIdx].mobj->pCraft;
			if (craft != NULL && (craft->workingSubsystems & 0x100u) != 0 && craft->beamActive != 0 &&
				craft->beamTypeId == 3 && craft->beamTimer != 0) {
				continue;
			}
		}

		if (objectType == OBJ_RebelPilot || objectType == OBJ_ImperialPilot ||
			objectType == OBJ_CivilianPilot) {
			continue;
		}

		flightGroupIdx = g_objectTable[objIdx].flightGroupIdx;
		if (flightGroupIdx > (unsigned int)(int16_t)g_missionHeader.numFlightGroups) {
			continue;
		}

		goalMatches = 0;
		for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
			const XwaFlightGroupGoalPayload* goal;

			goal = &g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload;
			if (goal->enabledForTeam[playerIff] != 0 && goal->condition == (uint8_t)goalCondType &&
				goal->points >= 0 &&
				g_missionFgStats[flightGroupIdx].goalState[8 * playerIff + goalIdx] == 4) {
				goalMatches = 1;
			}
		}

		triggerCondition =
			g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[0].condition;
		if (triggerCondition != TRIGVAR_STATUS) {
			if (triggerCondition != TRIGVAR_NONE &&
				(uint16_t)Mission_ObjectMatchesTriggerVariable(
					(uint16_t)objIdx,
					g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[0].variableType,
					g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[0].variable) &&
				Mission_EvaluateCondition(
					&g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[0], 0,
					playerIff) == 4) {
				goalMatches = 1;
			}
		}
		triggerCondition =
			g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[1].condition;
		if (triggerCondition != TRIGVAR_STATUS) {
			if (triggerCondition != TRIGVAR_NONE &&
				(uint16_t)Mission_ObjectMatchesTriggerVariable(
					(uint16_t)objIdx,
					g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[1].variableType,
					g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[1].variable) &&
				Mission_EvaluateCondition(
					&g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[1], 0,
					playerIff) == 4) {
				goalMatches = 1;
			}
		}
		triggerCondition =
			g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[0].condition;
		if (triggerCondition != TRIGVAR_STATUS) {
			if (triggerCondition != TRIGVAR_NONE &&
				(uint16_t)Mission_ObjectMatchesTriggerVariable(
					(uint16_t)objIdx,
					g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[0].variableType,
					g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[0].variable) &&
				Mission_EvaluateCondition(
					&g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[0], 0,
					playerIff) == 4) {
				goalMatches = 1;
			}
		}
		triggerCondition =
			g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[1].condition;
		if (triggerCondition != TRIGVAR_STATUS) {
			if (triggerCondition != TRIGVAR_NONE &&
				(uint16_t)Mission_ObjectMatchesTriggerVariable(
					(uint16_t)objIdx,
					g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[1].variableType,
					g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[1].variable) &&
				Mission_EvaluateCondition(
					&g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[1], 0,
					playerIff) == 4) {
				goalMatches = 1;
			}
		}

		if (goalMatches) {
			CraftData* craft;

			craft = g_objectTable[objIdx].mobj->pCraft;
			if (craft->objectKind == GENUS_Fighter || craft->objectKind == GENUS_PlayerProjectile) {
				uint32_t objectRef;
				int playerObjectIdx;

				objectRef = objIdx;
				playerObjectIdx = g_players[playerIdx].objectIndex;
				if (playerObjectIdx == 0xffff) {
					Mission_ResolveObjectOrMissionPointWorldLoc(objectRef, 0, 0, 0);
					trig2_ctop(worldlocx - g_players[playerIdx].viewState.savedTargetX,
							   worldlocy - g_players[playerIdx].viewState.savedTargetY,
							   worldlocz - g_players[playerIdx].viewState.savedTargetZ);
				} else {
					pai_ObjectRefDirectionToObjectRef(playerObjectIdx, objectRef);
				}

				if (msg_BuildTargetDescription((int16_t)objectRef, playerIdx, 0, 1)) {
					if ((uint32_t)trig2_polardistance < bestActionableDistance) {
						bestActionableObjIdx = (uint16_t)objectRef;
						bestActionableDistance = (uint32_t)trig2_polardistance;
					}
				} else if ((uint32_t)trig2_polardistance < bestFallbackDistance) {
					bestFallbackObjIdx = (uint16_t)objectRef;
					bestFallbackDistance = (uint32_t)trig2_polardistance;
				}
			}
		}
	}

	for (objIdx = g_objScanStart; objIdx < g_regionStaticObjectSlotEnd; ++objIdx) {
		ObjectTypeId objectType;
		unsigned int flightGroupIdx;
		int goalIdx;

		objectType = g_objectTable[objIdx].objectType;
		if (objectType == OBJ_None || (g_modelTypeTable[(uint16_t)objectType].flags & 3u) == 0) {
			continue;
		}

		flightGroupIdx = g_objectTable[objIdx].flightGroupIdx;
		if (flightGroupIdx > (unsigned int)(int16_t)g_missionHeader.numFlightGroups ||
			(uint16_t)g_players[playerIdx].playerIff == g_missionFlightGroups[flightGroupIdx].fg.team) {
			continue;
		}

		for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
			const XwaFlightGroupGoalPayload* goal;

			goal = &g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload;
			if (goal->enabledForTeam[playerIff] != 0 && goal->condition == (uint8_t)goalCondType &&
				goal->points >= 0 &&
				g_missionFgStats[flightGroupIdx].goalState[8 * playerIff + goalIdx] == 4) {
				int playerObjectIdx;

				playerObjectIdx = g_players[playerIdx].objectIndex;
				if (playerObjectIdx == 0xffff) {
					Mission_ResolveObjectOrMissionPointWorldLoc(objIdx, 0, 0, 0);
					trig2_ctop(worldlocx - g_players[playerIdx].viewState.savedTargetX,
							   worldlocy - g_players[playerIdx].viewState.savedTargetY,
							   worldlocz - g_players[playerIdx].viewState.savedTargetZ);
				} else {
					pai_ObjectRefDirectionToObjectRef(playerObjectIdx, objIdx);
				}
				if ((uint32_t)trig2_polardistance < bestActionableDistance) {
					bestActionableObjIdx = (uint16_t)objIdx;
					bestActionableDistance = (uint32_t)trig2_polardistance;
				}
			}
		}
	}

	if (bestActionableObjIdx == 0xffffu) {
		return (int16_t)bestFallbackObjIdx;
	}
	return (int16_t)bestActionableObjIdx;
}

// FUNCTION: XWA 0x502570
void Player_TransferShieldBankEnergy(uint16_t dstBank, uint16_t srcBank, int playerIdx) {
	CraftData* craft;
	int* dstShield;
	int srcShieldOffset;
	int maxShield;
	int16_t dstCapacity;
	int srcEnergy;

	srcShieldOffset = (int)srcBank * (int)sizeof(int);
	if (*Player_GetCraftShieldBankByOffset(g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft,
										   srcShieldOffset) <= 0) {
		return;
	}

	maxShield = Craft_GetObjectMaxShield((uint16_t)g_players[playerIdx].objectIndex);
	craft = g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft;
	dstShield = Player_GetCraftShieldBank(craft, dstBank);
	dstCapacity = (int16_t)((uint16_t)maxShield - (uint16_t)*dstShield);
	if (dstCapacity <= 0) {
		return;
	}

	srcEnergy = *Player_GetCraftShieldBankByOffset(craft, srcShieldOffset);
	if (dstCapacity < srcEnergy) {
		*dstShield += dstCapacity;
		*Player_GetCraftShieldBankByOffset(g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft,
										   srcShieldOffset) -= dstCapacity;
	} else {
		*dstShield += srcEnergy;
		*Player_GetCraftShieldBankByOffset(g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft,
										   srcShieldOffset) = 0;
	}
}
