#ifndef XWA_FLIGHT_FLIGHT_LIGHT_H
#define XWA_FLIGHT_FLIGHT_LIGHT_H

#include "xwa/math/vec3i.h"
#include "xwa/flight/object/object.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DirectionalLightFadeFields {
	float baseIntensity;
	float baseField1C;
	float baseColorR;
	float baseColorB;
	float baseColorG;
	float targetIntensity;
	float targetField1C;
	float targetColorR;
	float targetColorB;
	float targetColorG;
} DirectionalLightFadeFields;

typedef struct DirectionalLight {
	int worldDirX_Q15;
	int worldDirY_Q15;
	int worldDirZ_Q15;
	float localDirX;
	float localDirY;
	float localDirZ;
	float intensity;
	float field_1C;
	float colorR;
	float colorB;
	float colorG;
	DirectionalLightFadeFields fade;
} DirectionalLight;

#pragma pack(push, 1)
typedef struct LocalPlayerLightPulse {
	uint16_t enabled;
	int startTime;
	float cycleTicks;
	float invCycleTicks;
	float fadeTicks;
	float field12;
	float colorR;
	float colorG;
	float colorB;
	float intensity;
	float cullRadius;
} LocalPlayerLightPulse;
#pragma pack(pop)

typedef struct PointLight {
	Vec3i fixed;
	float x;
	float y;
	float z;
	float intensity;
	int cullRadius;
	float field20;
	float colorR;
	float colorB;
	float colorG;
} PointLight;

enum {
	XWA_OBJECT_POINT_LIGHT_COUNT = 8,
	XWA_SCENE_POINT_LIGHT_COUNT = 128,
	XWA_LOCAL_PLAYER_LIGHT_PULSE_COUNT = 6,
};

typedef char xwa_local_player_light_pulse_size[(sizeof(LocalPlayerLightPulse) == 0x2a) ? 1 : -1];
typedef char xwa_point_light_size[(sizeof(PointLight) == 0x30) ? 1 : -1];

extern int g_dirLightCount;
extern DirectionalLight g_directionalLights[8];
#ifdef XWA_MODERN
/* Modern snapshot provenance; -1 for directionals not created from a
 * current-region backdrop. Kept outside the recovered light layout. */
extern int16_t g_directionalLightBackdropIndices[8];
#endif
extern const float flt_5A9F54;
extern int g_objectPointLightCount;
extern PointLight g_objectPointLights[XWA_OBJECT_POINT_LIGHT_COUNT];
extern int g_scenePointLightCount;
extern PointLight g_scenePointLights[XWA_SCENE_POINT_LIGHT_COUNT];
extern int g_localPlayerLightPulseActive;
extern LocalPlayerLightPulse g_localPlayerLightPulses[XWA_LOCAL_PLAYER_LIGHT_PULSE_COUNT];
/* Point-light intensity tables (flight_light.c; consumed by the
 * classic append law and the remaster side accessor). */
extern const int g_flightLightSparkIntensityBySubtype[16];
extern const int g_flightLightExplosionIntensityOffsetBySubtype[16];
extern const int g_explosionPointLightIntensityOffsetBySubtype[15];
extern ObjectRecord* g_swFaceLightCachedObject;
extern void* g_swFaceLightCachedFace;

struct SceneFace;

void FlightLight_ResetSoftwareFaceSampleCache(void);
float FlightLight_ComputeSoftwareFaceSampleIntensity(struct SceneFace* face, int screenX, int screenY,
													 float viewZ);
ObjectRecord* Craft_ClearEffectiveAiObjectLink(CraftData* craft);
unsigned int Craft_ApplyEngineEmitterDamage(uint16_t objIdx, uint16_t engineEmitterIdx, unsigned int damage);
unsigned int Craft_DamageNearestEngineEmitterForMesh(uint16_t objIdx, int16_t componentId,
													 unsigned int damage);
int FlightLight_ClearDirectionalLights(void);
int FlightLight_AddDirectionalLight(int dx, int dy, int dz, float intensity, float red, float green,
									float blue);
void FlightLight_BlendDirectionalLightTargets(float blendFactor);
unsigned int FlightLight_AddCurrentRegionBackdropLights(void);
void FlightLight_InitLocalPlayerPulses(void);
void FlightLight_AppendLocalPlayerPulses(void);
void FlightLight_SetLocalPlayerPulseEnabled(int pulseSlot, int enabled);
void FlightLight_AppendScenePointLightForObject(ObjectRecord* obj);
void FlightLight_BuildObjectPointLights(ObjectRecord* obj);
void FlightLight_SetupObjectLighting(ObjectRecord* obj);
void FlightLight_SetupTargetInsetObjectLighting(unsigned int objectIdx);

#ifdef __cplusplus
}
#endif

#endif
