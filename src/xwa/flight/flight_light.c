#include "xwa/flight/flight_light.h"
#include "xwa/flight/hangar.h"

#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight.h"
#include "xwa/math/fixed.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/time.h"

#include <stddef.h>
#include <string.h>

// GLOBAL: XWA 0x782848
int g_dirLightCount;
// GLOBAL: XWA 0x7D4FA0
DirectionalLight g_directionalLights[8];
#ifdef XWA_MODERN
int16_t g_directionalLightBackdropIndices[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
#endif
// GLOBAL: XWA 0x782844
int g_objectPointLightCount;
// GLOBAL: XWA 0x8D42C0
PointLight g_objectPointLights[XWA_OBJECT_POINT_LIGHT_COUNT];
// GLOBAL: XWA 0x782840
int g_scenePointLightCount;
// GLOBAL: XWA 0x7FA360
PointLight g_scenePointLights[XWA_SCENE_POINT_LIGHT_COUNT];
// GLOBAL: XWA 0x7CA224
int g_localPlayerLightPulseActive;
// GLOBAL: XWA 0x91ABA0
LocalPlayerLightPulse g_localPlayerLightPulses[XWA_LOCAL_PLAYER_LIGHT_PULSE_COUNT];
// GLOBAL: XWA 0x63D098
ObjectRecord* g_swFaceLightCachedObject;
// GLOBAL: XWA 0x63D0A0
void* g_swFaceLightCachedFace;

// GLOBAL: XWA 0x5FF5B4
/* Non-static (deviation from the original's internal linkage): the
 * remaster snapshot side accessor mirrors the point-light source laws
 * and reads these tables as the single source of truth. */
const int g_flightLightSparkIntensityBySubtype[16] = {
	30, 16, 24, 48, 64, 96, 96, 96, 96, 64, 48, 32, 16, 15, 100, 250,
};

// GLOBAL: XWA 0x5FF5E4
const int g_flightLightExplosionIntensityOffsetBySubtype[16] = {
	16, 15, 100, 250, 300, 300, 300, 300, 250, 200, 150, 75, 25, 15, 100, 250,
};

// GLOBAL: XWA 0x5FF644
const int g_explosionPointLightIntensityOffsetBySubtype[15] = {
	0, 15, 100, 250, 350, 350, 350, 350, 300, 320, 250, 192, 96, 48, 16,
};

// GLOBAL: XWA 0x5A9EC8
const float flt_5A9EC8 = 9.0000001e31f;
// GLOBAL: XWA 0x5A9F54
const float flt_5A9F54 = 0.000030518499f;
// GLOBAL: XWA 0x5A9494
const float g_directionalLightVectorScale = 32768.0f;
// GLOBAL: XWA 0x5A9498
float g_directionalLightColorAverageScale = 0.33333001f;

// FUNCTION: XWA 0x439150
void FlightLight_ResetSoftwareFaceSampleCache(void) {
	g_swFaceLightCachedObject = NULL;
	g_swFaceLightCachedFace = NULL;
}

// FUNCTION: XWA 0x439160
float FlightLight_ComputeSoftwareFaceSampleIntensity(SceneFace* face, int screenX, int screenY, float viewZ) {
	(void)face;
	(void)screenX;
	(void)screenY;
	(void)viewZ;
	/* TODO: Reimplement the software face-light sampler. */
	return 0.0f;
}

// FUNCTION: XWA 0x41D3F0
ObjectRecord* Craft_ClearEffectiveAiObjectLink(CraftData* craft) {
	ObjectRecord* linkedObject;

	linkedObject = craft->effectiveAiObjectLink;
	if (linkedObject != NULL) {
		linkedObject->objectType = OBJ_None;
		craft->effectiveAiObjectLink = NULL;
	}
	return linkedObject;
}

// FUNCTION: XWA 0x4E0FA0
unsigned int Craft_ApplyEngineEmitterDamage(uint16_t objIdx, uint16_t engineEmitterIdx, unsigned int damage) {
	uint8_t* health;
	unsigned int remainingDamage;
	unsigned int scaledHealth;

	health = &g_curCraft->engineEmitterHealth[engineEmitterIdx];
	if (*health == 0 || *health == 0xffu) {
		return damage;
	}

	remainingDamage = damage;
	if (remainingDamage == 0) {
		remainingDamage = 1;
	}

	scaledHealth = 16u * (unsigned int)*health;
	if (scaledHealth <= remainingDamage) {
		unsigned int destroyedCount;
		unsigned int engineGlowCount;
		unsigned int glowIndex;

		destroyedCount = 0;
		remainingDamage -= scaledHealth;
		*health = (uint8_t)destroyedCount;
		engineGlowCount = g_modelDefs[g_curCraft->modelIndex].engineGlowCount;
		for (glowIndex = 0; glowIndex < engineGlowCount; ++glowIndex) {
			if (g_curCraft->engineEmitterHealth[glowIndex] == 0) {
				++destroyedCount;
			}
		}

		g_curCraft->engineOutputScale =
			(uint16_t)(int)((g_engineWashFullIntensity - (float)destroyedCount / (float)engineGlowCount) *
							g_engineOutputScaleMax);
		if (g_useHardware3D) {
			GlowMark_CreateEngineKnockoutBlastMark(objIdx, engineEmitterIdx);
		}
	} else {
		int newHealth;

		newHealth = (int)(scaledHealth - remainingDamage) >> 4;
		if (newHealth == 0) {
			newHealth = 1;
		}
		*health = (uint8_t)newHealth;
		remainingDamage = 0;
	}

	return remainingDamage;
}

// FUNCTION: XWA 0x4E0E10
unsigned int Craft_DamageNearestEngineEmitterForMesh(uint16_t objIdx, int16_t componentId,
													 unsigned int damage) {
	ModelIndex modelIndex;
	float nearestDistSq;
	int emitterIdx;
	int meshIdx;
	int selectedEmitter;
	uint16_t engineGlowCount;

	selectedEmitter = -1;
	modelIndex = GetModelIndexFromType(g_objectTable[objIdx].objectType);
	nearestDistSq = flt_5A9EC8;
	meshIdx = componentId;
	meshIdx += 0xffff;
	engineGlowCount = g_modelDefs[modelIndex].engineGlowCount;

	for (emitterIdx = 0; emitterIdx < engineGlowCount; ++emitterIdx) {
		if (g_curCraft->engineEmitterHealth[emitterIdx] != 0 &&
			(uint16_t)g_modelDefs[modelIndex].engineGlowMeshIdx[emitterIdx] == (uint16_t)meshIdx) {
			OptEngineGlow* glow;
			float distSq;
			float radius;

			glow = g_modelDefs[modelIndex].engineGlows[emitterIdx];
			distSq = (g_glowMarkPlaneScratch.center.x - glow->position.x) *
					 (g_glowMarkPlaneScratch.center.x - glow->position.x);
			distSq += (g_glowMarkPlaneScratch.center.y - glow->position.y) *
					  (g_glowMarkPlaneScratch.center.y - glow->position.y);
			distSq += (g_glowMarkPlaneScratch.center.z - glow->position.z) *
					  (g_glowMarkPlaneScratch.center.z - glow->position.z);
			if (distSq < nearestDistSq) {
				radius = (double)glow->dimensions.z < ((double)glow->dimensions.x > (double)glow->dimensions.y
														   ? glow->dimensions.x
														   : glow->dimensions.y)
							 ? ((double)glow->dimensions.x > (double)glow->dimensions.y ? glow->dimensions.x
																						: glow->dimensions.y)
							 : glow->dimensions.z;
				if (distSq < radius * radius) {
					nearestDistSq = distSq;
					selectedEmitter = emitterIdx;
				}
			}
		}
	}

	if ((uint16_t)selectedEmitter != 0xffffu) {
		return Craft_ApplyEngineEmitterDamage(objIdx, (uint16_t)selectedEmitter, damage);
	}
	return damage;
}

// FUNCTION: XWA 0x438EF0
int FlightLight_ClearDirectionalLights(void) {
	g_dirLightCount = 0;
	return DebugPrintfChannel(0x40000, "Removed all global lights from world.\n");
}

// FUNCTION: XWA 0x438F10
int FlightLight_AddDirectionalLight(int dx, int dy, int dz, float intensity, float red, float green,
									float blue) {
	float normalizeScale;
	int lightIdx;
	int result;

	trig2_ctop(dx, dy, dz);
	result = trig2_polardistance;
	if (result != 0) {
		normalizeScale = g_directionalLightVectorScale / (float)trig2_polardistance;
		result = g_useHardware3D;
		if (!result) {
			float averageColor;

			averageColor = blue + green + red;
			averageColor *= g_directionalLightColorAverageScale;
			intensity *= averageColor;
		}

		if (g_dirLightCount < 8) {
			lightIdx = g_dirLightCount;
#ifdef XWA_MODERN
			g_directionalLightBackdropIndices[lightIdx] = -1;
#endif
			g_directionalLights[lightIdx].worldDirX_Q15 = (int)((float)dx * normalizeScale);
			g_directionalLights[lightIdx].worldDirY_Q15 = (int)((float)dy * normalizeScale);
			g_directionalLights[lightIdx].worldDirZ_Q15 = (int)((float)dz * normalizeScale);
			g_directionalLights[lightIdx].intensity = intensity;
			g_directionalLights[lightIdx].colorR = red;
			g_directionalLights[lightIdx].colorG = green;
			g_directionalLights[lightIdx].colorB = blue;

			DebugPrintfChannel(0x40000, "Added light %d at (%ld,%ld,%ld), i%f, r%f g%f b%f.\n", lightIdx, dx,
							   dy, dz, intensity, red, green, blue);
			g_dirLightCount = g_dirLightCount + 1;
			result = g_dirLightCount;
		}
	}
	return result;
}

// FUNCTION: XWA 0x4390D0
void FlightLight_BlendDirectionalLightTargets(float blendFactor) {
	int remainingLightCount;
	int lightIndex;
	double baseFactor;

	remainingLightCount = g_dirLightCount;
	if (remainingLightCount <= 0) {
		return;
	}

	baseFactor = g_sw3dLightUnit - blendFactor;
	lightIndex = 0;
	do {
		const float* targetIntensity;
		const float* targetField1C;
		const float* targetColorR;
		const float* targetColorG;
		const float* targetColorB;

		targetIntensity = &g_directionalLights[lightIndex].fade.targetIntensity;
		targetField1C = &g_directionalLights[lightIndex].fade.targetField1C;
		targetColorR = &g_directionalLights[lightIndex].fade.targetColorR;
		targetColorG = &g_directionalLights[lightIndex].fade.targetColorG;
		targetColorB = &g_directionalLights[lightIndex].fade.targetColorB;
		g_directionalLights[lightIndex].intensity =
			*targetIntensity * blendFactor + g_directionalLights[lightIndex].fade.baseIntensity * baseFactor;
		{
			double baseField1C;

			baseField1C = g_directionalLights[lightIndex].fade.baseField1C;
			g_directionalLights[lightIndex].field_1C =
				*targetField1C * blendFactor + baseField1C * baseFactor;
		}
		g_directionalLights[lightIndex].colorR =
			*targetColorR * blendFactor + g_directionalLights[lightIndex].fade.baseColorR * baseFactor;
		g_directionalLights[lightIndex].colorG =
			*targetColorG * blendFactor + g_directionalLights[lightIndex].fade.baseColorG * baseFactor;
		g_directionalLights[lightIndex].colorB =
			*targetColorB * blendFactor + g_directionalLights[lightIndex].fade.baseColorB * baseFactor;
		++lightIndex;
		--remainingLightCount;
	} while (remainingLightCount != 0);
}

// FUNCTION: XWA 0x439040
unsigned int FlightLight_AddCurrentRegionBackdropLights(void) {
	int regionIndex;
	unsigned int lightIdx;
	unsigned int lightCount;

	regionIndex = g_players[g_localPlayer].regionIndex;
	lightCount = (unsigned int)g_backdropCountByRegion[regionIndex];
	for (lightIdx = 0; lightIdx < lightCount; ++lightIdx) {
		WorldRectRecord* backdrop;
		float intensity;

		backdrop = &g_backdropRecordsByRegion[regionIndex][lightIdx];
		intensity = backdrop->intensity;
		if (intensity > g_sw3dLightZero) {
#ifdef XWA_MODERN
			int addedLightIdx = g_dirLightCount;
#endif
			FlightLight_AddDirectionalLight(backdrop->worldDirQ20.x >> 8, backdrop->worldDirQ20.y >> 8,
											backdrop->worldDirQ20.z >> 8, intensity, backdrop->colorR,
											backdrop->colorG, backdrop->colorB);
#ifdef XWA_MODERN
			if (g_dirLightCount > addedLightIdx) {
				g_directionalLightBackdropIndices[addedLightIdx] = (int16_t)lightIdx;
			}
#endif
		}
		lightCount = (unsigned int)g_backdropCountByRegion[regionIndex];
	}

	return lightCount;
}

// FUNCTION: XWA 0x4F3670
void FlightLight_InitLocalPlayerPulses(void) {
	g_localPlayerLightPulseActive = 0;

	g_localPlayerLightPulses[0].startTime = 0;
	g_localPlayerLightPulses[0].enabled = 0;
	g_localPlayerLightPulses[0].cycleTicks = 236.0f;
	g_localPlayerLightPulses[0].invCycleTicks = 0.0042372881f;
	g_localPlayerLightPulses[0].fadeTicks = 0.75f;
	g_localPlayerLightPulses[0].field12 = 1.0f;
	g_localPlayerLightPulses[0].colorR = 1.0f;
	g_localPlayerLightPulses[0].colorG = 0.0f;
	g_localPlayerLightPulses[0].colorB = 0.0f;
	g_localPlayerLightPulses[0].intensity = 50.0f;
	g_localPlayerLightPulses[0].cullRadius = 256.0f;

	g_localPlayerLightPulses[1].startTime = 0;
	g_localPlayerLightPulses[1].enabled = 0;
	g_localPlayerLightPulses[1].cycleTicks = 177.0f;
	g_localPlayerLightPulses[1].invCycleTicks = 0.0056497175f;
	g_localPlayerLightPulses[1].fadeTicks = 0.25f;
	g_localPlayerLightPulses[1].field12 = 1.0f;
	g_localPlayerLightPulses[1].colorR = 0.69999999f;
	g_localPlayerLightPulses[1].colorG = 0.80000001f;
	g_localPlayerLightPulses[1].colorB = 0.1f;
	g_localPlayerLightPulses[1].intensity = 40.0f;
	g_localPlayerLightPulses[1].cullRadius = 256.0f;

	g_localPlayerLightPulses[2].startTime = 0;
	g_localPlayerLightPulses[2].enabled = 0;
	g_localPlayerLightPulses[2].cycleTicks = 354.0f;
	g_localPlayerLightPulses[2].invCycleTicks = 0.0028248588f;
	g_localPlayerLightPulses[2].fadeTicks = 0.5f;
	g_localPlayerLightPulses[2].field12 = 1.0f;
	g_localPlayerLightPulses[2].colorR = 0.1f;
	g_localPlayerLightPulses[2].colorG = 0.2f;
	g_localPlayerLightPulses[2].colorB = 1.0f;
	g_localPlayerLightPulses[2].intensity = 50.0f;
	g_localPlayerLightPulses[2].cullRadius = 256.0f;

	g_localPlayerLightPulses[3].startTime = 0;
	g_localPlayerLightPulses[3].enabled = 0;
	g_localPlayerLightPulses[3].cycleTicks = 1180.0f;
	g_localPlayerLightPulses[3].invCycleTicks = 0.00084745762f;
	g_localPlayerLightPulses[3].fadeTicks = 1.0f;
	g_localPlayerLightPulses[3].field12 = 1.0f;
	g_localPlayerLightPulses[3].colorR = 1.0f;
	g_localPlayerLightPulses[3].colorG = 1.0f;
	g_localPlayerLightPulses[3].colorB = 1.0f;
	g_localPlayerLightPulses[3].intensity = 1024.0f;
	g_localPlayerLightPulses[3].cullRadius = 256.0f;

	g_localPlayerLightPulses[4].startTime = 0;
	g_localPlayerLightPulses[4].enabled = 0;
	g_localPlayerLightPulses[4].cycleTicks = 35.400002f;
	g_localPlayerLightPulses[4].invCycleTicks = 0.028248586f;
	g_localPlayerLightPulses[4].fadeTicks = 0.25f;
	g_localPlayerLightPulses[4].field12 = 1.0f;
	g_localPlayerLightPulses[4].colorR = 0.0f;
	g_localPlayerLightPulses[4].colorG = 0.1f;
	g_localPlayerLightPulses[4].colorB = 1.0f;
	g_localPlayerLightPulses[4].intensity = 128.0f;
	g_localPlayerLightPulses[4].cullRadius = 256.0f;

	g_localPlayerLightPulses[5].startTime = 0;
	g_localPlayerLightPulses[5].enabled = 0;
	g_localPlayerLightPulses[5].cycleTicks = 472.0f;
	g_localPlayerLightPulses[5].invCycleTicks = 0.0021186441f;
	g_localPlayerLightPulses[5].fadeTicks = 1.0f;
	g_localPlayerLightPulses[5].field12 = 1.0f;
	g_localPlayerLightPulses[5].colorR = 1.0f;
	g_localPlayerLightPulses[5].colorG = 1.0f;
	g_localPlayerLightPulses[5].colorB = 1.0f;
	g_localPlayerLightPulses[5].intensity = 1024.0f;
	g_localPlayerLightPulses[5].cullRadius = 256.0f;
}

// FUNCTION: XWA 0x4F38D0
void FlightLight_AppendLocalPlayerPulses(void) {
	int outLightCount;
	int pulseIdx;
	PointLight* pointLight;
	double elapsedTicks;
	double cyclePhaseDouble;
	float cyclePhase;
	float fadeTicks;
	float fadeScale;
	float halfFadePhase;
	float invCycleTicks;
	int playerIdx;

	outLightCount = g_objectPointLightCount;
	for (pulseIdx = 0;
		 pulseIdx < XWA_LOCAL_PLAYER_LIGHT_PULSE_COUNT && outLightCount != XWA_OBJECT_POINT_LIGHT_COUNT;
		 ++pulseIdx) {
		if (g_localPlayerLightPulses[pulseIdx].enabled != 0) {
			invCycleTicks = g_localPlayerLightPulses[pulseIdx].invCycleTicks;
			fadeTicks = g_localPlayerLightPulses[pulseIdx].fadeTicks;
			elapsedTicks = (double)(g_gameTime - g_localPlayerLightPulses[pulseIdx].startTime);
			cyclePhaseDouble = elapsedTicks - (double)(int)(elapsedTicks * invCycleTicks) *
												  g_localPlayerLightPulses[pulseIdx].cycleTicks;
			cyclePhase = cyclePhaseDouble * invCycleTicks;

			if (cyclePhase > 0.5f) {
				if (fadeTicks > 0.0f) {
					halfFadePhase = cyclePhase - 0.5f;
					halfFadePhase += halfFadePhase;
					if (halfFadePhase <= fadeTicks) {
						fadeScale = 1.0f - halfFadePhase / fadeTicks;
					} else {
						continue;
					}
				} else {
					continue;
				}
			} else {
				if (fadeTicks > 0.0f) {
					halfFadePhase = cyclePhase + cyclePhase;
					if (halfFadePhase <= fadeTicks) {
						fadeScale = halfFadePhase / fadeTicks;
					} else {
						fadeScale = 1.0f;
					}
				} else {
					fadeScale = 1.0f;
				}
			}

			pointLight = &g_objectPointLights[outLightCount];
			pointLight->intensity = g_localPlayerLightPulses[pulseIdx].intensity * fadeScale;
			if (!g_useHardware3D) {
				pointLight->intensity *= 0.2f;
			}

			pointLight->cullRadius = (int)(g_localPlayerLightPulses[pulseIdx].cullRadius * fadeScale);
			playerIdx = g_localPlayer;
			pointLight->field20 = g_localPlayerLightPulses[pulseIdx].field12;
			pointLight->colorR = g_localPlayerLightPulses[pulseIdx].colorR;
			pointLight->colorG = g_localPlayerLightPulses[pulseIdx].colorG;
			pointLight->colorB = g_localPlayerLightPulses[pulseIdx].colorB;

			pointLight->fixed.x = (int)g_players[playerIdx].hardpointLocalX;
			pointLight->fixed.y = (int)g_players[playerIdx].hardpointLocalY;
			pointLight->fixed.z = (int)g_players[playerIdx].hardpointLocalZ;
			pointLight->x = g_players[playerIdx].hardpointLocalX;
			pointLight->y = g_players[playerIdx].hardpointLocalY;
			pointLight->z = g_players[playerIdx].hardpointLocalZ;
			++outLightCount;
		}
	}

	g_objectPointLightCount = outLightCount;
}

// FUNCTION: XWA 0x4F35C0
void FlightLight_SetLocalPlayerPulseEnabled(int pulseSlot, int enabled) {
	int activeWasZero;

	if (enabled) {
		if (!g_localPlayerLightPulseActive) {
			g_localPlayerLightPulseActive = 1;
		}
		g_localPlayerLightPulses[pulseSlot].enabled = 1;
	} else {
		activeWasZero = g_localPlayerLightPulseActive == 0;
		g_localPlayerLightPulses[pulseSlot].enabled = 0;
		if (!activeWasZero && g_localPlayerLightPulses[0].enabled + g_localPlayerLightPulses[1].enabled +
									  g_localPlayerLightPulses[2].enabled ==
								  g_localPlayerLightPulses[3].enabled + g_localPlayerLightPulses[4].enabled +
									  g_localPlayerLightPulses[5].enabled) {
			g_localPlayerLightPulseActive = 0;
		}
	}
}

// FUNCTION: XWA 0x4F22B0
void FlightLight_AppendScenePointLightForObject(ObjectRecord* obj) {
	uint16_t objectType;
	ModelIndex modelIndex;
	int16_t appendLight;

	appendLight = 1;
	objectType = obj->objectType;
	modelIndex = g_modelTypeTable[(uint16_t)objectType].modelIndex;
	if (g_localLightsLevel != 2 && obj->genusId != GENUS_Explosion &&
		obj->genusId != GENUS_DeathStarTunnelSegment) {
		return;
	}

	if (g_scenePointLightCount != XWA_SCENE_POINT_LIGHT_COUNT &&
		(obj->genusId == GENUS_Explosion || obj->genusId == GENUS_PlayerProjectile ||
		 obj->genusId == GENUS_NpcProjectile || obj->genusId == GENUS_DeathStarTunnelSegment ||
		 objectType == OBJ_DSReactorCylinder ||
		 (objectType == OBJ_MilleniumFalcon2 &&
		  g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR && !g_dirLightCount) ||
		 (g_inHangarReady && (objectType == OBJ_HangarDroid || objectType == OBJ_HangarDroid2 ||
							  objectType == OBJ_Shuttle || objectType == OBJ_HangarRoofCrane)))) {
		int lightIdx;

		lightIdx = g_scenePointLightCount;
		g_scenePointLights[lightIdx].fixed.x = obj->world_x;
		g_scenePointLights[lightIdx].fixed.y = obj->world_y;
		g_scenePointLights[lightIdx].fixed.z = obj->world_z;

		if (obj->genusId == GENUS_Explosion) {
			uint8_t subtype;

			g_scenePointLights[lightIdx].intensity = 16.0f;
			g_scenePointLights[lightIdx].colorR = 1.0f;
			g_scenePointLights[lightIdx].colorB = 1.0f;
			g_scenePointLights[lightIdx].colorG = 1.0f;

			switch (obj->objectType) {
				case OBJ_SparkTextureGroup3000:
					if (obj->typeSpecificByte[0] == 0 || obj->typeSpecificByte[0] > 12u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						(float)g_flightLightSparkIntensityBySubtype[obj->typeSpecificByte[0]];
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.1f;
					g_scenePointLights[lightIdx].cullRadius = 2048;
					/* fall through */

				case OBJ_SparkTextureGroup3001:
					if (obj->typeSpecificByte[0] == 0 || obj->typeSpecificByte[0] > 12u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						(float)g_flightLightSparkIntensityBySubtype[obj->typeSpecificByte[0]];
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.0f;
					g_scenePointLights[lightIdx].cullRadius = 2048;
					break;

				case OBJ_SparkTextureGroup3002: {
					unsigned int subtype;

					subtype = obj->typeSpecificByte[0];
					if (subtype == 0 || subtype > 12u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						(float)g_flightLightSparkIntensityBySubtype[subtype];
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.0f;
					g_scenePointLights[lightIdx].cullRadius = 2048;
					break;
				}

				case OBJ_SparkTextureGroup3003:
					subtype = obj->typeSpecificByte[0];
					if (subtype == 0 || subtype > 13u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						subtype >= 12u ? 10.0f : (float)g_flightLightSparkIntensityBySubtype[subtype];
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.0f;
					g_scenePointLights[lightIdx].cullRadius = 2048;
					break;

				case OBJ_ExplosionTextureGroup2000:
				case OBJ_ExplosionTextureGroup2003:
					subtype = obj->typeSpecificByte[0];
					if (subtype == 0 || subtype > 12u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						(float)(((unsigned int)obj->mobj->instanceExtent >> 5) +
								g_flightLightExplosionIntensityOffsetBySubtype[subtype]);
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.0f;
					g_scenePointLights[lightIdx].cullRadius = 2 * obj->mobj->instanceExtent + 4096;
					break;

				case OBJ_ExplosionTextureGroup2001:
					subtype = obj->typeSpecificByte[0];
					if (subtype == 0 || subtype > 12u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						(float)(((unsigned int)obj->mobj->instanceExtent >> 5) +
								g_flightLightExplosionIntensityOffsetBySubtype[subtype]);
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.1f;
					g_scenePointLights[lightIdx].cullRadius = 2 * obj->mobj->instanceExtent + 4096;
					break;

				case OBJ_ExplosionTextureGroup2002:
					subtype = obj->typeSpecificByte[0];
					if (subtype == 0 || subtype > 12u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						(float)(((unsigned int)obj->mobj->instanceExtent >> 5) +
								g_flightLightExplosionIntensityOffsetBySubtype[subtype]);
					g_scenePointLights[lightIdx].colorR = 0.89999998f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.2f;
					g_scenePointLights[lightIdx].cullRadius = 2 * obj->mobj->instanceExtent + 4096;
					break;

				case OBJ_ExplosionTextureGroup2004:
					subtype = obj->typeSpecificByte[0];
					if (subtype == 0 || subtype > 11u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						(float)(((unsigned int)obj->mobj->instanceExtent >> 5) +
								g_explosionPointLightIntensityOffsetBySubtype[subtype]);
					g_scenePointLights[lightIdx].colorR = 0.80000001f;
					g_scenePointLights[lightIdx].colorG = 0.30000001f;
					g_scenePointLights[lightIdx].colorB = 0.0f;
					g_scenePointLights[lightIdx].cullRadius = 2 * obj->mobj->instanceExtent + 4096;
					break;

				case OBJ_ExplosionTextureGroup2005:
					subtype = obj->typeSpecificByte[0];
					if (subtype == 0 || subtype > 14u) {
						return;
					}
					g_scenePointLights[lightIdx].intensity =
						(float)(((unsigned int)obj->mobj->instanceExtent >> 5) +
								g_explosionPointLightIntensityOffsetBySubtype[subtype]);
					g_scenePointLights[lightIdx].colorR = 0.80000001f;
					g_scenePointLights[lightIdx].colorG = 0.34999999f;
					g_scenePointLights[lightIdx].colorB = 0.2f;
					g_scenePointLights[lightIdx].cullRadius = 2 * obj->mobj->instanceExtent + 0x2000;
					break;

				case OBJ_ExplosionTextureGroup2006:
					subtype = obj->typeSpecificByte[0];
					if (subtype == 0 || subtype > 42u) {
						return;
					}
					if (subtype < 6u) {
						g_scenePointLights[lightIdx].intensity =
							(float)((double)((unsigned int)obj->mobj->instanceExtent >> 5) +
									(double)subtype * 50.0);
					} else if (subtype <= 38u) {
						g_scenePointLights[lightIdx].intensity =
							(float)((double)((unsigned int)obj->mobj->instanceExtent >> 5) + 400.0);
					} else {
						g_scenePointLights[lightIdx].intensity =
							(float)((double)((unsigned int)obj->mobj->instanceExtent >> 5) +
									(double)(43 - subtype) * 50.0);
					}
					g_scenePointLights[lightIdx].colorR = 0.89999998f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.0f;
					g_scenePointLights[lightIdx].cullRadius = 2 * obj->mobj->instanceExtent + 0x4000;
					break;

				case OBJ_ChaffTextureGroup5000:
					g_scenePointLights[lightIdx].intensity = 5.0f;
					g_scenePointLights[lightIdx].colorR = 0.2f;
					g_scenePointLights[lightIdx].colorG = 0.2f;
					g_scenePointLights[lightIdx].colorB = 1.0f;
					break;

				case OBJ_DeathStarIITextureGroup17002:
					g_scenePointLights[lightIdx].intensity = (float)obj->typeSpecificWord;
					g_scenePointLights[lightIdx].cullRadius = 0x100000;
					g_scenePointLights[lightIdx].colorR = 0.60000002f;
					g_scenePointLights[lightIdx].colorG = 0.2f;
					g_scenePointLights[lightIdx].colorB = 0.0f;
					g_scenePointLights[lightIdx].colorG =
						(float)((obj->typeSpecificWord % 6 + 2) * 0.25 * g_scenePointLights[lightIdx].colorG);
					break;

				default:
					g_scenePointLights[lightIdx].intensity =
						(float)(unsigned int)(g_flightBrightnessScaleQ8 - 256);
					break;
			}

			g_scenePointLights[lightIdx].intensity *= 4.0f;
		} else {
			g_scenePointLights[lightIdx].cullRadius = 1024;
			switch (obj->objectType) {
				case OBJ_LaserRebel:
				case OBJ_LaserRebelTurbo:
				case OBJ_WarheadLaser1:
				case OBJ_WarheadLaser3:
				case OBJ_LaserRebelTurbo_301:
				case OBJ_LaserRebelTurbo_302:
					g_scenePointLights[lightIdx].intensity = 200.0f;
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.2f;
					g_scenePointLights[lightIdx].colorB = 0.0f;
					break;

				case OBJ_LaserImperial:
				case OBJ_LaserImperialTurbo:
				case OBJ_WarheadLaser2:
				case OBJ_LaserImperialTurbo_303:
				case OBJ_LaserImperialTurbo_304:
				case OBJ_LaserImperialTurbo_305:
					g_scenePointLights[lightIdx].intensity = 200.0f;
					g_scenePointLights[lightIdx].colorR = 0.0f;
					g_scenePointLights[lightIdx].colorG = 1.0f;
					g_scenePointLights[lightIdx].colorB = 0.2f;
					break;

				case OBJ_LaserImperialDS:
					g_scenePointLights[lightIdx].intensity = 50000.0f;
					g_scenePointLights[lightIdx].colorR = 0.0f;
					g_scenePointLights[lightIdx].colorG = 1.0f;
					g_scenePointLights[lightIdx].colorB = 0.2f;
					g_scenePointLights[lightIdx].cullRadius = 0x4000;
					break;

				case OBJ_LaserIon:
				case OBJ_LaserIonTurbo:
				case OBJ_WarheadIon:
					g_scenePointLights[lightIdx].intensity = 200.0f;
					g_scenePointLights[lightIdx].colorR = 0.2f;
					g_scenePointLights[lightIdx].colorG = 0.2f;
					g_scenePointLights[lightIdx].colorB = 1.0f;
					break;

				case OBJ_WarheadTorpedo:
				case OBJ_WarheadAdvancedTorpedo:
					g_scenePointLights[lightIdx].intensity = 250.0f;
					g_scenePointLights[lightIdx].colorR = 0.40000001f;
					g_scenePointLights[lightIdx].colorG = 0.2f;
					g_scenePointLights[lightIdx].colorB = 1.0f;
					break;

				case OBJ_WarheadMagPulse:
					g_scenePointLights[lightIdx].intensity = 250.0f;
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.2f;
					g_scenePointLights[lightIdx].colorB = 1.0f;
					break;

				case OBJ_WarheadIonPulse:
					g_scenePointLights[lightIdx].intensity = 250.0f;
					g_scenePointLights[lightIdx].colorR = 0.2f;
					g_scenePointLights[lightIdx].colorG = 0.2f;
					g_scenePointLights[lightIdx].colorB = 1.0f;
					break;

				case OBJ_WarheadMissile:
				case OBJ_WarheadAdvancedMissile:
				case OBJ_WarheadSpaceBomb:
				case OBJ_WarheadRocket:
					g_scenePointLights[lightIdx].intensity = 250.0f;
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 0.2f;
					break;

				case OBJ_WarheadFlare:
					g_scenePointLights[lightIdx].intensity = 200.0f;
					g_scenePointLights[lightIdx].colorR = 1.0f;
					g_scenePointLights[lightIdx].colorG = 0.40000001f;
					g_scenePointLights[lightIdx].colorB = 1.0f;
					break;

				case OBJ_MilleniumFalcon2:
					g_scenePointLights[lightIdx].intensity = 2048.0f;
					g_scenePointLights[lightIdx].cullRadius = 256;
					g_scenePointLights[lightIdx].colorR = 0.5f;
					g_scenePointLights[lightIdx].colorG = 0.5f;
					g_scenePointLights[lightIdx].colorB = 0.5f;
					break;

				case OBJ_DSAccelChamber:
					g_scenePointLights[lightIdx].intensity = (float)obj->typeSpecificWord;
					if ((obj->typeSpecificWord & 1u) != 0) {
						g_scenePointLights[lightIdx].cullRadius = 0x8000;
						g_scenePointLights[lightIdx].colorR = 0.2f;
						g_scenePointLights[lightIdx].colorG = 0.0f;
						g_scenePointLights[lightIdx].colorB = 0.80000001f;
					} else {
						g_scenePointLights[lightIdx].cullRadius = 1024;
						g_scenePointLights[lightIdx].colorR = 0.80000001f;
						g_scenePointLights[lightIdx].colorG = 0.0f;
						g_scenePointLights[lightIdx].colorB = 0.40000001f;
					}
					break;

				case OBJ_DSReactorCylinder:
					g_scenePointLights[lightIdx].intensity = (float)obj->typeSpecificWord;
					g_scenePointLights[lightIdx].cullRadius = 0x20000;
					g_scenePointLights[lightIdx].colorR = 0.30000001f;
					g_scenePointLights[lightIdx].colorG = 0.5f;
					g_scenePointLights[lightIdx].colorB = 0.89999998f;
					break;

				default:
					appendLight = 0;
					break;
			}
		}

		if (appendLight) {
			if (!g_useHardware3D) {
				g_scenePointLights[lightIdx].intensity *= 0.40000001f;
			}
			g_scenePointLightCount = lightIdx + 1;
		}
	} else if (modelIndex != (ModelIndex)0xffff) {
		int glowIdx;
		uint8_t engineGlowCount;

		engineGlowCount = g_modelDefs[modelIndex].engineGlowCount;
		if (engineGlowCount == 0) {
			return;
		}

		for (glowIdx = 0; glowIdx < engineGlowCount; ++glowIdx) {
			OptEngineGlow* glow;
			uint8_t meshIdx;
			uint8_t meshRotation;
			CraftData* craft;
			double engineScale;
			int localSide;
			int localUp;
			uint32_t color;

			if (g_scenePointLightCount == XWA_SCENE_POINT_LIGHT_COUNT) {
				break;
			}

			meshIdx = g_modelDefs[modelIndex].engineGlowMeshIdx[glowIdx];
			glow = g_modelDefs[modelIndex].engineGlows[glowIdx];
			if (glow == NULL || glow->isDisabled ||
				(glow->dimensions.x <= 2000.0f && glow->dimensions.y <= 2000.0f)) {
				continue;
			}

			craft = obj->mobj->pCraft;
			engineScale =
				(double)(16 - craft->laserRedirect - craft->shieldRedirect - craft->beamLevel) * 0.0625;
			engineScale = ((float)craft->engineOutputScale * 0.000015259022f) * engineScale;

			g_scenePointLights[g_scenePointLightCount].intensity =
				(float)(glow->dimensions.z * (float)engineScale * 300.0f);
			g_scenePointLights[g_scenePointLightCount].cullRadius = (int)((float)engineScale * 16384.0f);
			meshRotation = obj->mobj->pCraft->meshRotation[meshIdx];
			localSide = (int)glow->position.x;
			localUp = (int)glow->position.y;
			if (meshRotation != 0) {
				int16_t angleQ16;

				angleQ16 = (int16_t)((uint16_t)(uint8_t)(0u - meshRotation) << 8);
				ModelMesh_ApplyAnimatedMeshRotationToPoint(angleQ16, (uint16_t)obj->objectType, meshIdx,
														   localSide, localUp, (int)glow->position.z);
				localUp = g_rotatedY;
			} else {
				g_rotatedX = localSide;
				g_rotatedY = localUp;
				g_rotatedZ = (int)glow->position.z;
			}

			pai_RotateLocalVectorToWorldScratch(obj, g_rotatedX, g_rotatedZ, -localUp);
			g_scenePointLights[g_scenePointLightCount].fixed.x = g_rotatedX + obj->world_x;
			g_scenePointLights[g_scenePointLightCount].fixed.y = g_rotatedY + obj->world_y;
			g_scenePointLights[g_scenePointLightCount].fixed.z = g_rotatedZ + obj->world_z;
			g_scenePointLights[g_scenePointLightCount].field20 = 1.0f;
			color = glow->coreColor;
			g_scenePointLights[g_scenePointLightCount].field20 = (float)((color >> 24) & 0xffu) * 0.00390625f;
			g_scenePointLights[g_scenePointLightCount].colorR = (float)((color >> 16) & 0xffu) * 0.00390625f;
			g_scenePointLights[g_scenePointLightCount].colorG = (float)((color >> 8) & 0xffu) * 0.00390625f;
			g_scenePointLights[g_scenePointLightCount].colorB = (float)(color & 0xffu) * 0.00390625f;
			if (!g_useHardware3D) {
				g_scenePointLights[g_scenePointLightCount].intensity *= 0.40000001f;
			}
			++g_scenePointLightCount;
		}
	}
}

static inline int FlightLight_Dot3Q15ReuseFirstSlot(int x, int y, int z, int rowX, int rowY, int rowZ) {
	return Xwa_Dot3Q15ReuseXSlot(rowX, rowY, rowZ, x, y, z);
}

// FUNCTION: XWA 0x4F2F60
void FlightLight_BuildObjectPointLights(ObjectRecord* obj) {
	int sceneLightIdx;
	int sceneLightIter;
	int outLightCount;
	int cullDistance;
	float lightScale;
	int deltaX;
	int deltaY;
	int deltaZ;
	int worldX;
	int worldY;
	int worldZ;
	int objectIndex;
	PointLight* dstLight;

	sceneLightIdx = 0;
	lightScale = 1.0f;
	if (g_localLightsLevel) {
		g_objectPointLightCount = 0;
		outLightCount = 0;
		worldX = obj->world_x;
		worldY = obj->world_y;
		worldZ = obj->world_z;
		if (g_useHardware3D && obj->genusId == GENUS_Starship) {
			lightScale = 0.5f;
		}

		sceneLightIter = 0;
		if (g_scenePointLightCount > 0) {
			dstLight = g_objectPointLights;
			while (1) {
				deltaY = g_scenePointLights[sceneLightIdx].fixed.y - worldY;
				cullDistance = g_modelTypeTable[(uint16_t)obj->objectType].maxBoundsExtent +
							   g_scenePointLights[sceneLightIdx].cullRadius;
				deltaX = g_scenePointLights[sceneLightIdx].fixed.x - worldX;
				deltaZ = g_scenePointLights[sceneLightIdx].fixed.z - worldZ;

				if (collide_roughdistance3d(deltaX, deltaY, deltaZ) < cullDistance) {
					if (obj->mobj != NULL) {
						dstLight->fixed.x =
							FlightLight_Dot3Q15ReuseFirstSlot(deltaX, deltaY, deltaZ, obj->mobj->cachedSideX,
															  obj->mobj->cachedSideY, obj->mobj->cachedSideZ);
						dstLight->fixed.y =
							-FlightLight_Dot3Q15ReuseFirstSlot(deltaX, deltaY, deltaZ, obj->mobj->cachedFwdX,
															   obj->mobj->cachedFwdY, obj->mobj->cachedFwdZ);
						dstLight->fixed.z =
							FlightLight_Dot3Q15ReuseFirstSlot(deltaX, deltaY, deltaZ, obj->mobj->cachedUpX,
															  obj->mobj->cachedUpY, obj->mobj->cachedUpZ);
					} else {
						FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, NULL);
						dstLight->fixed.x = FlightLight_Dot3Q15ReuseFirstSlot(
							deltaX, deltaY, deltaZ, g_fviewSideX_Q15, g_fviewSideY_Q15, g_fviewSideZ_Q15);
						dstLight->fixed.y = -FlightLight_Dot3Q15ReuseFirstSlot(
							deltaX, deltaY, deltaZ, g_fviewFwdX_Q15, g_fviewFwdY_Q15, g_fviewFwdZ_Q15);
						dstLight->fixed.z = FlightLight_Dot3Q15ReuseFirstSlot(
							deltaX, deltaY, deltaZ, g_fviewUpX_Q15, g_fviewUpY_Q15, g_fviewUpZ_Q15);
					}

					dstLight->x = (float)dstLight->fixed.x;
					++outLightCount;
					++dstLight;
					dstLight[-1].y = (float)dstLight[-1].fixed.y;
					dstLight[-1].z = (float)dstLight[-1].fixed.z;
					dstLight[-1].intensity = g_scenePointLights[sceneLightIdx].intensity * lightScale;
					dstLight[-1].field20 = g_scenePointLights[sceneLightIdx].field20 * lightScale;
					dstLight[-1].colorR = g_scenePointLights[sceneLightIdx].colorR * lightScale;
					dstLight[-1].colorG = g_scenePointLights[sceneLightIdx].colorG * lightScale;
					dstLight[-1].colorB = g_scenePointLights[sceneLightIdx].colorB * lightScale;
					if (dstLight == &g_objectPointLights[XWA_OBJECT_POINT_LIGHT_COUNT]) {
						break;
					}
				}

				sceneLightIdx = (uint16_t)(++sceneLightIter);
				if (sceneLightIdx >= g_scenePointLightCount) {
					break;
				}
			}
		}

		objectIndex = g_players[g_localPlayer].objectIndex;
		g_objectPointLightCount = outLightCount;
		if (obj == &g_objectTable[objectIndex] && g_localPlayerLightPulseActive &&
			outLightCount != XWA_OBJECT_POINT_LIGHT_COUNT) {
			FlightLight_AppendLocalPlayerPulses();
		}
	}
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x4F32D0
void FlightLight_SetupObjectLighting(ObjectRecord* obj) {
	int lightIdx;
	int dot_Q15;

	lightIdx = 0;
	if (g_dirLightCount > 0) {
		do {
			dot_Q15 = g_curMatR0_X;
			dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * g_directionalLights[lightIdx].worldDirX_Q15 +
												  g_curMatR0_Y * g_directionalLights[lightIdx].worldDirY_Q15 +
												  g_curMatR0_Z * g_directionalLights[lightIdx].worldDirZ_Q15);
			g_directionalLights[lightIdx].localDirX = (float)dot_Q15 * flt_5A9F54;

			dot_Q15 = g_curMatR2_X;
			dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * g_directionalLights[lightIdx].worldDirX_Q15 +
												  g_curMatR2_Y * g_directionalLights[lightIdx].worldDirY_Q15 +
												  g_curMatR2_Z * g_directionalLights[lightIdx].worldDirZ_Q15);
			g_directionalLights[lightIdx].localDirY = (float)dot_Q15 * flt_5A9F54;

			dot_Q15 = g_curMatR1_X;
			dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * g_directionalLights[lightIdx].worldDirX_Q15 +
												  g_curMatR1_Y * g_directionalLights[lightIdx].worldDirY_Q15 +
												  g_curMatR1_Z * g_directionalLights[lightIdx].worldDirZ_Q15);
			g_directionalLights[lightIdx].localDirZ = (float)dot_Q15 * flt_5A9F54;

			++lightIdx;
		} while (lightIdx < g_dirLightCount);
	}

	FlightLight_BuildObjectPointLights(obj);
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x4F3420
void FlightLight_SetupTargetInsetObjectLighting(unsigned int objectIdx) {
	int lightIdx;
	ObjectRecord* obj;
	DirectionalLight* light;

	g_objectPointLightCount = 0;
	if (!g_localLightsLevel || objectIdx >= g_regionStaticObjectSlotEnd) {
		return;
	}

	if (g_useHardware3D && g_objRenderState[objectIdx].particleEffects != NULL) {
		Particle_AppendObjectEffectPointLights((uint16_t)objectIdx);
	}

	obj = &g_objectTable[objectIdx];
	lightIdx = 0;
	if (g_dirLightCount > 0) {
		light = g_directionalLights;
		do {
			objectIdx = (unsigned int)g_curMatR0_X;
			objectIdx = (unsigned int)Xwa_SaturateWrappedQ30ToQ15((int)objectIdx * light->worldDirX_Q15 +
																  g_curMatR0_Y * light->worldDirY_Q15 +
																  g_curMatR0_Z * light->worldDirZ_Q15);
			light->localDirX = (float)(int)objectIdx * flt_5A9F54;

			objectIdx = (unsigned int)g_curMatR2_X;
			objectIdx = (unsigned int)Xwa_SaturateWrappedQ30ToQ15((int)objectIdx * light->worldDirX_Q15 +
																  g_curMatR2_Y * light->worldDirY_Q15 +
																  g_curMatR2_Z * light->worldDirZ_Q15);
			light->localDirY = (float)(int)objectIdx * flt_5A9F54;

			objectIdx = (unsigned int)g_curMatR1_X;
			objectIdx = (unsigned int)Xwa_SaturateWrappedQ30ToQ15((int)objectIdx * light->worldDirX_Q15 +
																  g_curMatR1_Y * light->worldDirY_Q15 +
																  g_curMatR1_Z * light->worldDirZ_Q15);
			light->localDirZ = (float)(int)objectIdx * flt_5A9F54;

			++lightIdx;
			++light;
		} while (lightIdx < g_dirLightCount);
	}

	FlightLight_BuildObjectPointLights(obj);
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif
