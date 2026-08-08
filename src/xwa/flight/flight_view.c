#include "xwa/flight/hangar.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer_internal.h"

#include "xwa/assets/flight_model.h"
#include "xwa/flight/object/damage.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_map.h"
#include "xwa/flight/starfield.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/yard.h"
#include "xwa/util/time.h"
#ifdef XWA_MODERN
#include "aeron/log.h"
#include "xwa/flight/flight_debug.h"
#include "xwa_runtime/hooks/orientation_hook.h"
#include "xwa_runtime/snapshot/snapshot_hud.h"
#endif

#ifdef XWA_MODERN
#include <math.h>
#endif

#ifndef XWA_MODERN
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* outputString);
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#define FLIGHT_VIEW_OUTPUT_DEBUG_STRING g_OutputDebugStringA
#else
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#define FLIGHT_VIEW_OUTPUT_DEBUG_STRING OutputDebugStringA
#endif

// GLOBAL: XWA 0x7712A4
int g_hardpointOriginOffset[3];

// GLOBAL: XWA 0x5A94B8
const float flt_5A94B8 = 0.000030518499f;

// GLOBAL: XWA 0x5A9F48
const float flt_5A9F48 = 1.1f;

// GLOBAL: XWA 0x5A9F4C
const float flt_5A9F4C = 0.9f;

// GLOBAL: XWA 0x5A9AB4
const float flt_5A9AB4 = 0.000030517578f;

// GLOBAL: XWA 0x5A9AB8
const float flt_5A9AB8 = 0.000095873722f;

// GLOBAL: XWA 0x5A9ABC
const float flt_5A9ABC = 0.000047936861f;

// GLOBAL: XWA 0x5A9AC0
const float flt_5A9AC0 = 0.000030518509f;

#ifdef XWA_MODERN
/* Double-precision shadow of the camera basis. The Q15 camera integers are
 * rebuilt through chained truncated rotations every frame, so their few-LSB
 * error requantizes with each pan step. Because the renderer applies the
 * object rotation (about the model origin) and the view translation in
 * separate stages, that common camera error displaces vertices by
 * error * distance-from-model-origin: on huge OPTs (Azzameen platform,
 * capital ships) whole surfaces visibly jump by many pixels while panning.
 * The shadow rebuilds the same rotation chain in double precision from the
 * same angle inputs, giving a smooth camera matrix for rendering. The Q15
 * integers are untouched. Each shadow is tagged with the exact Q15 camera
 * matrix published with it. Consumers use the shadow only while that tag still
 * matches g_camMat; temporary viewport cameras save and restore the complete
 * tagged state and the independently writable legacy float view matrix. */
static double g_camShadow[9];
static int g_camShadowSource[9];
static int g_camShadowValid;
static double g_savedViewportCamShadow[9];
static int g_savedViewportCamShadowSource[9];
static int g_savedViewportCamShadowValid;
static float g_savedViewportViewMtx[9];

/* Mirror of FVIEW_calcrotatemove's basis construction. */
static void FVIEW_ShadowRotMove(double m[9], int16_t angleA, int16_t angleB) {
	static const double angleScale = 6.283185307179586 / 65536.0;
	double angC;
	double angB;
	double cosNegB;
	double sinNegB;
	double cosC;
	double sinC;

	angC = (double)(uint16_t)(0xc000 - angleA) * angleScale;
	angB = (double)(uint16_t)(int16_t)-angleB * angleScale;
	cosNegB = cos(angB);
	sinNegB = sin(angB);
	cosC = cos(angC);
	sinC = sin(angC);

	m[0] = cosNegB;
	m[1] = sinNegB;
	m[2] = 0.0;
	m[3] = -sinNegB * sinC;
	m[4] = cosNegB * sinC;
	m[5] = -cosC;
	m[6] = -sinNegB * cosC;
	m[7] = cosNegB * cosC;
	m[8] = sinC;
}

/* Mirror of FVIEW_transformaxes: rotate the basis rows about a unit axis. */
static void FVIEW_ShadowRotateAxes(double m[9], double ax, double ay, double az, int16_t angleQ16) {
	static const double angleScale = 6.283185307179586 / 65536.0;
	double c;
	double s;
	double omc;
	double r00, r01, r02, r10, r11, r12, r20, r21, r22;
	int i;

	if (angleQ16 == 0) {
		return;
	}

	c = cos((double)(uint16_t)angleQ16 * angleScale);
	s = sin((double)(uint16_t)angleQ16 * angleScale);
	omc = 1.0 - c;

	r00 = c + omc * ax * ax;
	r01 = az * s + omc * ay * ax;
	r02 = -ay * s + omc * az * ax;
	r10 = -az * s + omc * ay * ax;
	r11 = c + omc * ay * ay;
	r12 = ax * s + omc * az * ay;
	r20 = ay * s + omc * az * ax;
	r21 = -ax * s + omc * az * ay;
	r22 = c + omc * az * az;

	for (i = 0; i < 9; i += 3) {
		double ox = m[i];
		double oy = m[i + 1];
		double oz = m[i + 2];

		m[i] = r20 * oz + r10 * oy + r00 * ox;
		m[i + 1] = r21 * oz + r11 * oy + r01 * ox;
		m[i + 2] = r22 * oz + r12 * oy + r02 * ox;
	}
}

/* Mirror of the calcrotateorient rotations applied by the camera builders. */
static void FVIEW_ShadowRotOrient(double m[9], Q16Angle angleC, Q16Angle angleD, ObjectRecord* objRecord) {
	FVIEW_ShadowRotateAxes(m, m[3], m[4], m[5], (int16_t)angleD);
	FVIEW_ShadowRotateAxes(m, m[6], m[7], m[8], (int16_t)angleC);
	if (objRecord != NULL && objRecord->mobj != NULL && objRecord->mobj->spinAngleQ16 != 0) {
		FVIEW_ShadowRotateAxes(m, objRecord->mobj->spinAxisX, objRecord->mobj->spinAxisY,
							   objRecord->mobj->spinAxisZ, objRecord->mobj->spinAngleQ16);
	}
}

static void FVIEW_CopyCamMatInts(int rows[9]) {
	rows[0] = g_camMatR0_X;
	rows[1] = g_camMatR0_Y;
	rows[2] = g_camMatR0_Z;
	rows[3] = g_camMatR1_X;
	rows[4] = g_camMatR1_Y;
	rows[5] = g_camMatR1_Z;
	rows[6] = g_camMatR2_X;
	rows[7] = g_camMatR2_Y;
	rows[8] = g_camMatR2_Z;
}

/* Ownership is exact, not inferred from angular proximity. Target-inset and
 * padlock cameras can legitimately be very close to the main camera, so a
 * tolerance can accept a smooth basis belonging to the wrong camera. */
static int FVIEW_CamShadowMatchesInts(void) {
	return g_camShadowValid && g_camShadowSource[0] == g_camMatR0_X && g_camShadowSource[1] == g_camMatR0_Y &&
		   g_camShadowSource[2] == g_camMatR0_Z && g_camShadowSource[3] == g_camMatR1_X &&
		   g_camShadowSource[4] == g_camMatR1_Y && g_camShadowSource[5] == g_camMatR1_Z &&
		   g_camShadowSource[6] == g_camMatR2_X && g_camShadowSource[7] == g_camMatR2_Y &&
		   g_camShadowSource[8] == g_camMatR2_Z;
}

static void FVIEW_CopyShadowToViewMtx(void) {
	g_viewMtx00 = (float)g_camShadow[0];
	g_viewMtx01 = (float)g_camShadow[1];
	g_viewMtx02 = (float)g_camShadow[2];
	g_viewMtx10 = (float)g_camShadow[3];
	g_viewMtx11 = (float)g_camShadow[4];
	g_viewMtx12 = (float)g_camShadow[5];
	g_viewMtx20 = (float)g_camShadow[6];
	g_viewMtx21 = (float)g_camShadow[7];
	g_viewMtx22 = (float)g_camShadow[8];
}

static void FVIEW_CopyViewMtx(float rows[9]) {
	rows[0] = g_viewMtx00;
	rows[1] = g_viewMtx01;
	rows[2] = g_viewMtx02;
	rows[3] = g_viewMtx10;
	rows[4] = g_viewMtx11;
	rows[5] = g_viewMtx12;
	rows[6] = g_viewMtx20;
	rows[7] = g_viewMtx21;
	rows[8] = g_viewMtx22;
}

static void FVIEW_RestoreViewMtx(const float rows[9]) {
	g_viewMtx00 = rows[0];
	g_viewMtx01 = rows[1];
	g_viewMtx02 = rows[2];
	g_viewMtx10 = rows[3];
	g_viewMtx11 = rows[4];
	g_viewMtx12 = rows[5];
	g_viewMtx20 = rows[6];
	g_viewMtx21 = rows[7];
	g_viewMtx22 = rows[8];
}

void FVIEW_SaveRenderCameraStateForViewport(void) {
	int i;

	for (i = 0; i < 9; ++i) {
		g_savedViewportCamShadow[i] = g_camShadow[i];
		g_savedViewportCamShadowSource[i] = g_camShadowSource[i];
	}
	g_savedViewportCamShadowValid = g_camShadowValid;
	FVIEW_CopyViewMtx(g_savedViewportViewMtx);
}

void FVIEW_RestoreRenderCameraStateForViewport(void) {
	int i;

	for (i = 0; i < 9; ++i) {
		g_camShadow[i] = g_savedViewportCamShadow[i];
		g_camShadowSource[i] = g_savedViewportCamShadowSource[i];
	}
	g_camShadowValid = g_savedViewportCamShadowValid;
	if (!FVIEW_CamShadowMatchesInts()) {
		g_camShadowValid = 0;
	}
	/* g_viewMtx is independently writable legacy render state. Restoring the
	 * exact saved floats avoids requantizing an unshadowed camera through Q15. */
	FVIEW_RestoreViewMtx(g_savedViewportViewMtx);
}

void FVIEW_CopyRenderCameraRows(float rows[9]) {
	int i;

	if (FVIEW_CamShadowMatchesInts()) {
		for (i = 0; i < 9; ++i) {
			rows[i] = (float)g_camShadow[i];
		}
		return;
	}

	rows[0] = (float)g_camMatR0_X * (1.0f / 32768.0f);
	rows[1] = (float)g_camMatR0_Y * (1.0f / 32768.0f);
	rows[2] = (float)g_camMatR0_Z * (1.0f / 32768.0f);
	rows[3] = (float)g_camMatR1_X * (1.0f / 32768.0f);
	rows[4] = (float)g_camMatR1_Y * (1.0f / 32768.0f);
	rows[5] = (float)g_camMatR1_Z * (1.0f / 32768.0f);
	rows[6] = (float)g_camMatR2_X * (1.0f / 32768.0f);
	rows[7] = (float)g_camMatR2_Y * (1.0f / 32768.0f);
	rows[8] = (float)g_camMatR2_Z * (1.0f / 32768.0f);
}

/* Publish the shadow and derive g_viewMtx from it. */
static void FVIEW_StoreCamShadow(const double m[9]) {
	int i;

	for (i = 0; i < 9; ++i) {
		g_camShadow[i] = m[i];
	}
	FVIEW_CopyCamMatInts(g_camShadowSource);
	g_camShadowValid = 1;
	FVIEW_CopyShadowToViewMtx();
}
#endif

// FUNCTION: XWA 0x497610
int ComputeHardpointWorldPos(int playerIdx) {
	int objectIdx;
	ModelIndex modelIndex;
	Vec3f vec;
	float axisAngle[4];
	Matrix3x3 out;
	int seatTableIdx;

	if ((uint16_t)g_players[playerIdx].objectIndex == 0xffffu) {
		return 0;
	}

	objectIdx = (uint16_t)g_players[playerIdx].objectIndex;
	if (g_objectTable[objectIdx].mobj == NULL || g_objectTable[objectIdx].mobj->state != 0) {
		return 0;
	}

	modelIndex = GetModelIndexFromType(g_objectTable[objectIdx].objectType);
	if (modelIndex == 0xffffu) {
		return 0;
	}

	seatTableIdx = g_players[playerIdx].currentSeatIdx - 1;
	if (seatTableIdx == -1) {
		vec.x = (float)(g_hardpointOriginOffset[0] + g_modelDefs[modelIndex].primaryHardpointX);
		vec.y = (float)(g_hardpointOriginOffset[2] - g_modelDefs[modelIndex].primaryHardpointY);
		vec.z = (float)(g_hardpointOriginOffset[1] + g_modelDefs[modelIndex].primaryHardpointZ);
	} else {
		CraftData* craft;
		OptRotationScale* launcherRotScale;
		OptRotationScale* beamRotScale;
		uint16_t turretModelType;
		uint16_t meshCount;
		int meshIdx;
		int remainingMeshCount;

		craft = g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft;
		beamRotScale = NULL;
		launcherRotScale = NULL;

		vec.x = (float)(g_hardpointOriginOffset[0] + g_modelDefs[modelIndex].turretSeatPosX[seatTableIdx]);
		vec.y = (float)(g_hardpointOriginOffset[1] - g_modelDefs[modelIndex].turretSeatPosY[seatTableIdx]);
		vec.z = (float)(g_hardpointOriginOffset[2] + g_modelDefs[modelIndex].turretSeatPosZ[seatTableIdx]);

		turretModelType = g_modelDefs[modelIndex].turretModelIndex[seatTableIdx];
		meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(turretModelType);
		if (meshCount > 0u) {
			meshIdx = 0;
			remainingMeshCount = meshCount;
			do {
				switch (ModelMesh_GetObjectTypeMeshType((int)turretModelType, meshIdx)) {
					case MESH_RotaryGunTurret:
						(void)ModelMesh_GetRotScaleData((int)turretModelType, meshIdx);
						break;
					case MESH_RotaryLauncher:
						launcherRotScale = ModelMesh_GetRotScaleData((int)turretModelType, meshIdx);
						break;
					case MESH_RotaryBeamSystem:
						beamRotScale = ModelMesh_GetRotScaleData((int)turretModelType, meshIdx);
						break;
					default:
						break;
				}
				++meshIdx;
				--remainingMeshCount;
			} while (remainingMeshCount != 0);
		}

		if (beamRotScale != NULL && launcherRotScale != NULL) {
			vec.x -= launcherRotScale->pivot.x;
			vec.y -= launcherRotScale->pivot.y;
			vec.z -= launcherRotScale->pivot.z;
			axisAngle[0] = launcherRotScale->rotationAxis.x * flt_5A9AB4;
			axisAngle[1] = launcherRotScale->rotationAxis.y * flt_5A9AB4;
			axisAngle[2] = launcherRotScale->rotationAxis.z * flt_5A9AB4;
			axisAngle[3] =
				(float)(-(int16_t)craft->turretAim.aimAngleA[g_players[playerIdx].currentSeatIdx - 1] *
						flt_5A9AB8);
			Math3D_BuildAxisAngleMatrix(&out, axisAngle);
			Math3D_RotateVec3(&vec, &out);
			vec.x += launcherRotScale->pivot.x;
			vec.y += launcherRotScale->pivot.y;
			vec.z += launcherRotScale->pivot.z;

			vec.x -= beamRotScale->pivot.x;
			vec.y -= beamRotScale->pivot.y;
			vec.z -= beamRotScale->pivot.z;
			axisAngle[0] = beamRotScale->rotationAxis.x * flt_5A9AB4;
			axisAngle[1] = beamRotScale->rotationAxis.y * flt_5A9AB4;
			axisAngle[2] = beamRotScale->rotationAxis.z * flt_5A9AB4;
			axisAngle[3] =
				(float)((int16_t)craft->turretAim.aimAngleB[g_players[playerIdx].currentSeatIdx - 1] *
						flt_5A9ABC);
			Math3D_BuildAxisAngleMatrix(&out, axisAngle);
			Math3D_RotateVec3(&vec, &out);
			vec.x += beamRotScale->pivot.x;
			vec.y += beamRotScale->pivot.y;
			vec.z += beamRotScale->pivot.z;
		}
	}

	out.m[0] = (float)g_objectTable[objectIdx].mobj->cachedSideX * flt_5A9AC0;
	out.m[1] = (float)g_objectTable[objectIdx].mobj->cachedSideY * flt_5A9AC0;
	out.m[2] = (float)g_objectTable[objectIdx].mobj->cachedSideZ * flt_5A9AC0;
	out.m[3] = (float)-g_objectTable[objectIdx].mobj->cachedFwdX * flt_5A9AC0;
	out.m[4] = (float)-g_objectTable[objectIdx].mobj->cachedFwdY * flt_5A9AC0;
	out.m[5] = (float)-g_objectTable[objectIdx].mobj->cachedFwdZ * flt_5A9AC0;
	out.m[6] = (float)g_objectTable[objectIdx].mobj->cachedUpX * flt_5A9AC0;
	out.m[7] = (float)g_objectTable[objectIdx].mobj->cachedUpY * flt_5A9AC0;
	out.m[8] = (float)g_objectTable[objectIdx].mobj->cachedUpZ * flt_5A9AC0;

	g_players[playerIdx].hardpointLocalX = vec.x;
	g_players[playerIdx].hardpointLocalY = vec.y;
	g_players[playerIdx].hardpointLocalZ = vec.z;

	Math3D_RotateVec3(&vec, &out);
	g_players[playerIdx].hardpointWorldX = vec.x;
	g_players[playerIdx].hardpointWorldY = vec.y;
	g_players[playerIdx].hardpointWorldZ = vec.z;
	return 0;
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x4EFB40
void FlightView_RenderCockpitModel(void) {
	int playerObjectIdx;
	uint16_t objectType;
	uint16_t* loadedModelSlot;
	uint16_t savedLoadedModel;
	ObjectRecord* playerObject;
	int lightIdx;

	playerObjectIdx = g_players[g_localPlayer].objectIndex;
	g_curCraft = g_objectTable[playerObjectIdx].mobj->pCraft;

	FVIEW_SetObjectTransform(0, 0x4000u, 0, 0, &g_objectTable[playerObjectIdx]);
	ComputeHardpointWorldPos(g_localPlayer);
	g_cockpitViewActive = 1;

	objectType = (uint16_t)g_objectTable[playerObjectIdx].objectType;
	loadedModelSlot = &g_loadedModels.byObjectType[objectType];
	savedLoadedModel = *loadedModelSlot;
	if (g_players[g_localPlayer].currentSeatIdx != 0) {
		ModelIndex modelIndex;
		uint16_t turretModelType;

		modelIndex = GetModelIndexFromType(objectType);
		turretModelType =
			g_modelDefs[modelIndex].turretModelIndex[g_players[g_localPlayer].currentSeatIdx - 1];
		if (turretModelType != 0) {
			*loadedModelSlot = g_loadedModels.byObjectType[turretModelType];
		} else {
			FLIGHT_VIEW_OUTPUT_DEBUG_STRING("Can't find cockpit model\n");
		}
	} else {
		*loadedModelSlot = g_cockpitModel;
	}

	lightIdx = 0;
	playerObject = &g_objectTable[playerObjectIdx];
	if (g_dirLightCount > 0) {
		do {
			int dot_Q15;

			dot_Q15 = g_curMatR0_X;
			dot_Q15 = dot_Q15 * g_directionalLights[lightIdx].worldDirX_Q15 +
					  g_curMatR0_Y * g_directionalLights[lightIdx].worldDirY_Q15 +
					  g_curMatR0_Z * g_directionalLights[lightIdx].worldDirZ_Q15;
			if (dot_Q15 >= 0x40000000) {
				dot_Q15 = 0x3fffffff;
			}
			if (dot_Q15 <= (int32_t)0xc0000000u) {
				dot_Q15 = (int32_t)0xc0010000u;
			}
			dot_Q15 >>= 15;
			g_directionalLights[lightIdx].localDirX = (float)dot_Q15 * flt_5A9F54;

			dot_Q15 = g_curMatR2_X;
			dot_Q15 = dot_Q15 * g_directionalLights[lightIdx].worldDirX_Q15 +
					  g_curMatR2_Y * g_directionalLights[lightIdx].worldDirY_Q15 +
					  g_curMatR2_Z * g_directionalLights[lightIdx].worldDirZ_Q15;
			if (dot_Q15 >= 0x40000000) {
				dot_Q15 = 0x3fffffff;
			}
			if (dot_Q15 <= (int32_t)0xc0000000u) {
				dot_Q15 = (int32_t)0xc0010000u;
			}
			dot_Q15 >>= 15;
			g_directionalLights[lightIdx].localDirY = (float)dot_Q15 * flt_5A9F54;

			dot_Q15 = g_curMatR1_X;
			dot_Q15 = dot_Q15 * g_directionalLights[lightIdx].worldDirX_Q15 +
					  g_curMatR1_Y * g_directionalLights[lightIdx].worldDirY_Q15 +
					  g_curMatR1_Z * g_directionalLights[lightIdx].worldDirZ_Q15;
			if (dot_Q15 >= 0x40000000) {
				dot_Q15 = 0x3fffffff;
			}
			if (dot_Q15 <= (int32_t)0xc0000000u) {
				dot_Q15 = (int32_t)0xc0010000u;
			}
			dot_Q15 >>= 15;
			g_directionalLights[lightIdx].localDirZ = (float)dot_Q15 * flt_5A9F54;
			++lightIdx;
		} while (lightIdx < g_dirLightCount);
	}

	FlightLight_BuildObjectPointLights(playerObject);
	if (g_useHardware3D && g_objRenderState[playerObjectIdx].particleEffects != NULL) {
		Particle_AppendObjectEffectPointLights((uint16_t)playerObjectIdx);
	}

	g_renderFlags = 64;
	Damage_QueueCraftBillboards((uint16_t)playerObjectIdx);
	RenderScene_DrawObjectModel(&g_objectTable[playerObjectIdx]);

	*loadedModelSlot = savedLoadedModel;
	g_objectPointLightCount = 0;
	g_cockpitViewActive = 0;
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x4EE4A0
int FlightView_FinishCameraFocusTransition(int playerIdx, uint16_t transitionDuration) {
	int cameraPlayerIdx;
	int player;
	ObjectRecord* focusObj;
	int speed;
	int moveScale;
	int predictedMoveX;
	int predictedMoveY;
	int predictedMoveZ;
	int savedMoveZ;

	cameraPlayerIdx = playerIdx;
	player = cameraPlayerIdx;
	g_players[player].viewState.transitionTimer = 1;
	focusObj = &g_objectTable[g_players[player].viewState.cameraFocusObjIdx];
	speed = focusObj->mobj->speed;
	g_players[player].viewState.transitionDuration = transitionDuration;

	if (speed != 0) {
		moveScale = (int)(uint16_t)transitionDuration * ((speed * 4660 + 128) >> 8);
	} else {
		moveScale = 0;
	}

	predictedMoveX = 0;
	predictedMoveY = 0;
	predictedMoveZ = 0;
	if (focusObj->mobj->velocityOverrideActive != 0) {
		predictedMoveX = Xwa_Q15MulReuseFirstSlot(focusObj->mobj->velocityOverrideDirX,
												  (int)focusObj->mobj->velocityOverrideSpeed);
		predictedMoveY = Xwa_Q15MulReuseFirstSlot(focusObj->mobj->velocityOverrideDirY,
												  (int)focusObj->mobj->velocityOverrideSpeed);
		predictedMoveZ = Xwa_Q15MulReuseFirstSlot(focusObj->mobj->velocityOverrideDirZ,
												  (int)focusObj->mobj->velocityOverrideSpeed);
	}

	if (focusObj->mobj->velocityOverrideActive == 0 || focusObj->mobj->velocityOverrideDuration != 0) {
		predictedMoveX += Xwa_Q15MulReuseFirstSlot(focusObj->mobj->moveX, moveScale);
		predictedMoveY += Xwa_Q15MulReuseFirstSlot(focusObj->mobj->moveY, moveScale);
		predictedMoveZ += Xwa_Q15MulReuseFirstSlot(focusObj->mobj->moveZ, moveScale);
	}

	g_players[player].viewState.viewRoll = focusObj->roll;
	g_players[player].viewState.viewPitch = focusObj->pitch;
	g_players[player].viewState.viewYaw = focusObj->yaw;

	FVIEW_BuildCameraOrient(
		g_players[player].viewState.viewRoll, (int16_t)g_players[player].viewState.viewPitch,
		(int16_t)g_players[player].viewState.viewYaw, 0, (int16_t)g_players[player].viewState.hudAimX,
		(int16_t)g_players[player].viewState.hudAimY, NULL, cameraPlayerIdx);

	g_players[player].viewState.savedTargetX = focusObj->world_x + predictedMoveX;
	g_players[player].viewState.savedTargetY = focusObj->world_y + predictedMoveY;
	savedMoveZ = predictedMoveZ;
	g_players[player].viewState.savedTargetZ = savedMoveZ + focusObj->world_z;

	g_players[player].viewState.savedTargetX -=
		Xwa_Q15MulReuseFirstSlot(g_players[player].viewState.cameraDistance, g_camMatR2_X);
	g_players[player].viewState.savedTargetY -=
		Xwa_Q15MulReuseFirstSlot(g_players[player].viewState.cameraDistance, g_camMatR2_Y);
	g_players[player].viewState.savedTargetZ -=
		Xwa_Q15MulReuseFirstSlot(g_players[player].viewState.cameraDistance, g_camMatR2_Z);

	{
		ObjectRecord* objectTable;
		int extentShift;
		int halfExtent;
		int clearanceX;
		int clearanceY;
		int clearanceZ;

		objectTable = g_objectTable;
		extentShift = 0;
		g_curModelMaxExtent =
			g_modelTypeTable[(uint16_t)objectTable[g_players[player].viewState.cameraFocusObjIdx].objectType]
				.maxBoundsExtent /
			2;
		halfExtent = g_curModelMaxExtent;
		if (halfExtent > 0x7fff) {
			do {
				halfExtent >>= 1;
				++extentShift;
			} while (halfExtent > 0x7fff);
			g_curModelMaxExtent = halfExtent;
		}

		clearanceX = Xwa_Q15MulReuseFirstSlot(halfExtent, g_camMatR2_X);
		clearanceY = Xwa_Q15MulReuseFirstSlot(g_curModelMaxExtent, g_camMatR2_Y);
		clearanceZ = Xwa_Q15MulReuseFirstSlot(g_curModelMaxExtent, g_camMatR2_Z);
		if (extentShift != 0) {
			clearanceX <<= extentShift;
			clearanceY <<= extentShift;
			clearanceZ <<= extentShift;
		}

		g_players[player].viewState.savedTargetX -= clearanceX;
		g_players[player].viewState.savedTargetY -= clearanceY;
		g_players[player].viewState.savedTargetZ -= clearanceZ;

		return clearanceZ;
	}
}

// FUNCTION: XWA 0x4EF280
void FlightView_UpdatePlaybackCamera(int playerIdx) {
	if (g_filmPlaybackMode) {
		if (g_filmOverlayActive == 1) {
			if (playerIdx == g_localPlayer) {
				int savedMapCameraState;

				memcpy(&g_savedPlayerViewStateForPlaybackCamera, &g_players[g_localPlayer].viewState,
					   sizeof(g_savedPlayerViewStateForPlaybackCamera));
				g_projOffsetY = 0;
				g_projOffsetYf = 0.0f;
				savedMapCameraState = g_players[g_localPlayer].mapCameraState;

				g_collisionScratchBackupSegmentStartWorldX = g_collisionSegmentStartWorldX;
				g_collisionScratchBackupSegmentStartWorldY = g_collisionSegmentStartWorldY;
				g_collisionScratchBackupSegmentStartWorldZ = g_collisionSegmentStartWorldZ;
				g_collisionScratchBackupProbeWorldX = g_collisionProbeWorldX;
				g_collisionScratchBackupProbeWorldY = g_collisionProbeWorldY;
				g_collisionScratchBackupProbeWorldZ = g_collisionProbeWorldZ;
				g_collisionScratchBackupSweepStartX = g_collisionSweepStartX;
				g_collisionScratchBackupSweepStartY = g_collisionSweepStartY;
				g_collisionScratchBackupSweepStartZ = g_collisionSweepStartZ;
				g_collisionScratchBackupSweepEndX = g_collisionSweepEndX;
				g_collisionScratchBackupSweepEndY = g_collisionSweepEndY;
				g_collisionScratchBackupSweepEndZ = g_collisionSweepEndZ;
				g_collisionScratchBackupHitOffsetX = g_collisionHitOffsetX;
				g_collisionScratchBackupHitOffsetY = g_collisionHitOffsetY;
				g_collisionScratchBackupHitOffsetZ = g_collisionHitOffsetZ;

				g_players[g_localPlayer].mapCameraState = 0;
				memcpy(&g_players[g_localPlayer].viewState, &g_filmOverlayViewState,
					   sizeof(g_players[g_localPlayer].viewState));

				if ((unsigned int)g_players[playerIdx].viewState.cameraFocusObjIdx == 0xffffu) {
					FVIEW_BuildCameraOrientNoTurret(g_players[playerIdx].viewState.viewRoll,
													(int16_t)g_players[playerIdx].viewState.viewPitch,
													(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
													(int16_t)g_players[playerIdx].viewState.hudAimX,
													(int16_t)g_players[playerIdx].viewState.hudAimY, NULL,
													playerIdx);
				} else if (g_players[playerIdx].viewState.externalCameraActive ||
						   g_players[playerIdx].viewState.playerInputBlocked) {
					uint16_t transitionTimer;

					transitionTimer = g_players[playerIdx].viewState.transitionTimer;
					if (transitionTimer == 0) {
						if ((unsigned int)g_players[playerIdx].viewState.aimTargetIdx != 0xffffu) {
							Mission_ResolveObjectOrMissionPointWorldLoc(
								(unsigned int)g_players[playerIdx].viewState.cameraFocusObjIdx, 0, 0, 0);
							g_players[playerIdx].viewState.savedTargetX = worldlocx;
							g_players[playerIdx].viewState.savedTargetY = worldlocy;
							g_players[playerIdx].viewState.savedTargetZ = worldlocz;

							trig2_ctop(g_objectTable[g_players[playerIdx].viewState.aimTargetIdx].world_x -
										   g_players[playerIdx].viewState.savedTargetX,
									   g_objectTable[g_players[playerIdx].viewState.aimTargetIdx].world_y -
										   g_players[playerIdx].viewState.savedTargetY,
									   g_objectTable[g_players[playerIdx].viewState.aimTargetIdx].world_z -
										   g_players[playerIdx].viewState.savedTargetZ);
							g_players[playerIdx].viewState.viewRoll = 0;
							g_players[playerIdx].viewState.viewPitch = targetPitch;
							g_players[playerIdx].viewState.viewYaw = trig2_xyangle;
							FVIEW_BuildCameraOrientNoTurret(g_players[playerIdx].viewState.viewRoll,
															(int16_t)g_players[playerIdx].viewState.viewPitch,
															(int16_t)g_players[playerIdx].viewState.viewYaw,
															0, 0, 0, NULL, -1);
						} else {
							g_players[playerIdx].viewState.viewRoll =
								g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].roll;
							g_players[playerIdx].viewState.viewPitch =
								g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].pitch;
							g_players[playerIdx].viewState.viewYaw =
								g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].yaw;
							FVIEW_BuildCameraOrientNoTurret(
								g_players[playerIdx].viewState.viewRoll,
								(int16_t)g_players[playerIdx].viewState.viewPitch,
								(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
								(int16_t)g_players[playerIdx].viewState.hudAimX,
								(int16_t)g_players[playerIdx].viewState.hudAimY, NULL, playerIdx);
						}

						Mission_ResolveObjectOrMissionPointWorldLoc(
							(unsigned int)g_players[playerIdx].viewState.cameraFocusObjIdx, 0, 0, 0);
						g_players[playerIdx].viewState.savedTargetX = worldlocx;
						g_players[playerIdx].viewState.savedTargetY = worldlocy;
						g_players[playerIdx].viewState.savedTargetZ = worldlocz;

						g_players[playerIdx].viewState.savedTargetX -= Xwa_Q15MulReuseFirstSlot(
							g_players[playerIdx].viewState.cameraDistance, g_camMatR2_X);
						g_players[playerIdx].viewState.savedTargetY -= Xwa_Q15MulReuseFirstSlot(
							g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Y);
						g_players[playerIdx].viewState.savedTargetZ -= Xwa_Q15MulReuseFirstSlot(
							g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Z);

						{
							int extentShift;
							int halfExtent;
							int clearanceX;
							int clearanceY;
							int clearanceZ;

							extentShift = 0;
							g_curModelMaxExtent =
								g_modelTypeTable
									[(uint16_t)g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx]
										 .objectType]
										.maxBoundsExtent /
								2;
							halfExtent = g_curModelMaxExtent;
							if (halfExtent > 0x7fff) {
								do {
									halfExtent >>= 1;
									++extentShift;
								} while (halfExtent > 0x7fff);
								g_curModelMaxExtent = halfExtent;
							}

							clearanceX = Xwa_Q15MulReuseFirstSlot(halfExtent, g_camMatR2_X);
							clearanceY = Xwa_Q15MulReuseFirstSlot(g_curModelMaxExtent, g_camMatR2_Y);
							clearanceZ = Xwa_Q15MulReuseFirstSlot(g_curModelMaxExtent, g_camMatR2_Z);
							if (extentShift != 0) {
								clearanceX <<= extentShift;
								clearanceY <<= extentShift;
								clearanceZ <<= extentShift;
							}

							g_players[playerIdx].viewState.savedTargetX -= clearanceX;
							g_players[playerIdx].viewState.savedTargetY -= clearanceY;
							g_players[playerIdx].viewState.savedTargetZ -= clearanceZ;
						}

						if ((unsigned int)g_players[playerIdx].objectIndex != 0xffffu) {
							int playerObjIdx;
							int collisionObjIdx;

							playerObjIdx = g_players[playerIdx].objectIndex;
							collisionObjIdx = g_objectTable[playerObjIdx].mobj->collisionObjIdx;
							if (!g_inHangarReady && (unsigned int)collisionObjIdx != 0xffffu &&
								playerObjIdx != collisionObjIdx) {
								g_collisionSegmentStartWorldX = g_players[playerIdx].viewState.savedTargetX;
								g_collisionSegmentStartWorldY = g_players[playerIdx].viewState.savedTargetY;
								g_collisionSegmentStartWorldZ = g_players[playerIdx].viewState.savedTargetZ;
								g_collisionProbeWorldX = g_objectTable[playerObjIdx].world_x;
								g_collisionProbeWorldY = g_objectTable[playerObjIdx].world_y;
								g_collisionProbeWorldZ = g_objectTable[playerObjIdx].world_z;

								if (collide_CheckSweptModelCollision(playerObjIdx, collisionObjIdx) > 0) {
									g_players[playerIdx].viewState.savedTargetX +=
										(int64_t)((float)g_collisionHitOffsetX * flt_5A9F48);
									g_players[playerIdx].viewState.savedTargetY +=
										(int64_t)((float)g_collisionHitOffsetY * flt_5A9F48);
									g_players[playerIdx].viewState.savedTargetZ +=
										(int64_t)((float)g_collisionHitOffsetZ * flt_5A9F48);
								} else if (g_flightPlayerCount == 1) {
									RenderObjectListEntry* entry;
									int originalCameraX;
									int originalCameraY;
									int originalCameraZ;

									originalCameraX = g_players[playerIdx].viewState.savedTargetX;
									originalCameraY = g_players[playerIdx].viewState.savedTargetY;
									originalCameraZ = g_players[playerIdx].viewState.savedTargetZ;
									for (entry = g_renderListHead; entry != NULL; entry = entry->next) {
										int entryObjIdx;
										uint16_t objectType;

										entryObjIdx = entry->objectIdx;
										objectType = (uint16_t)g_objectTable[entryObjIdx].objectType;
										if (entry->sortDepth - g_modelTypeTable[objectType].maxBoundsExtent <
												viewZ &&
											entryObjIdx != playerObjIdx && entryObjIdx != collisionObjIdx) {
											if (objectType < OBJ_DSTankwlights) {
												if (objectType >= OBJ_DSReactorCoreRoom) {
													g_collisionSegmentStartWorldX =
														g_players[playerIdx].viewState.savedTargetX;
													g_collisionSegmentStartWorldY =
														g_players[playerIdx].viewState.savedTargetY;
													g_collisionSegmentStartWorldZ =
														g_players[playerIdx].viewState.savedTargetZ;
													g_collisionProbeWorldX =
														g_objectTable[playerObjIdx].world_x;
													g_collisionProbeWorldY =
														g_objectTable[playerObjIdx].world_y;
													g_collisionProbeWorldZ =
														g_objectTable[playerObjIdx].world_z;

													if (collide_CheckSweptModelCollision(playerObjIdx,
																						 entryObjIdx) > 0) {
														g_players[playerIdx].viewState.savedTargetX =
															originalCameraX -
															(int64_t)((float)g_collisionHitOffsetX *
																	  flt_5A9F4C);
														g_players[playerIdx].viewState.savedTargetY =
															originalCameraY -
															(int64_t)((float)g_collisionHitOffsetY *
																	  flt_5A9F4C);
														g_players[playerIdx].viewState.savedTargetZ =
															originalCameraZ -
															(int64_t)((float)g_collisionHitOffsetZ *
																	  flt_5A9F4C);
													}
												}
											}
										}
									}
								}
							}
						}
					} else {
						uint16_t updatedTransitionTimer;

						updatedTransitionTimer = (uint16_t)g_elapsedTicks;
						updatedTransitionTimer += transitionTimer;
						g_players[playerIdx].viewState.transitionTimer = updatedTransitionTimer;
						if ((int)(uint16_t)updatedTransitionTimer >
							236 * (uint16_t)g_players[playerIdx].viewState.transitionDuration) {
							FlightView_FinishCameraFocusTransition(
								playerIdx, g_players[playerIdx].viewState.transitionDuration);
						} else {
							ObjectRecord* focusObj;

							focusObj = &g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx];
							trig2_ctop(focusObj->world_x - g_players[g_localPlayer].viewState.savedTargetX,
									   focusObj->world_y - g_players[g_localPlayer].viewState.savedTargetY,
									   focusObj->world_z - g_players[g_localPlayer].viewState.savedTargetZ);
							g_players[g_localPlayer].viewState.viewRoll = 0;
							g_players[g_localPlayer].viewState.viewPitch = targetPitch;
							g_players[g_localPlayer].viewState.viewYaw = trig2_xyangle;
							FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
													(int16_t)g_players[playerIdx].viewState.viewPitch,
													(int16_t)g_players[playerIdx].viewState.viewYaw, 0, 0, 0,
													NULL, -1);
						}
					}
				}

				g_collisionSegmentStartWorldX = g_collisionScratchBackupSegmentStartWorldX;
				g_collisionSegmentStartWorldY = g_collisionScratchBackupSegmentStartWorldY;
				g_collisionSegmentStartWorldZ = g_collisionScratchBackupSegmentStartWorldZ;
				g_collisionProbeWorldX = g_collisionScratchBackupProbeWorldX;
				g_collisionProbeWorldY = g_collisionScratchBackupProbeWorldY;
				g_collisionProbeWorldZ = g_collisionScratchBackupProbeWorldZ;
				g_collisionSweepStartX = g_collisionScratchBackupSweepStartX;
				g_collisionSweepStartY = g_collisionScratchBackupSweepStartY;
				g_collisionSweepStartZ = g_collisionScratchBackupSweepStartZ;
				g_collisionSweepEndX = g_collisionScratchBackupSweepEndX;
				g_collisionSweepEndY = g_collisionScratchBackupSweepEndY;
				g_collisionSweepEndZ = g_collisionScratchBackupSweepEndZ;
				g_collisionHitOffsetX = g_collisionScratchBackupHitOffsetX;
				g_collisionHitOffsetY = g_collisionScratchBackupHitOffsetY;
				g_collisionHitOffsetZ = g_collisionScratchBackupHitOffsetZ;

				g_players[g_localPlayer].mapCameraState = savedMapCameraState;
				memcpy(&g_filmOverlayViewState, &g_players[g_localPlayer].viewState,
					   sizeof(g_filmOverlayViewState));
				memcpy(&g_players[g_localPlayer].viewState, &g_savedPlayerViewStateForPlaybackCamera,
					   sizeof(g_players[g_localPlayer].viewState));
			}
		}
	}
}

// FUNCTION: XWA 0x4EE820
void FlightView_UpdatePlayerCamera(int playerIdx) {
	unsigned int cameraFocusObjIdx;

	if (g_players[playerIdx].mapCameraState) {
		FlightMap_UpdateCamera(playerIdx);
	} else {
		cameraFocusObjIdx = g_players[playerIdx].viewState.cameraFocusObjIdx;
		if (cameraFocusObjIdx == 0xffffu) {
			ObjectRecord* playerObj;

			playerObj = &g_objectTable[g_players[playerIdx].objectIndex];
			trig2_ctop(playerObj->world_x - g_players[playerIdx].viewState.savedTargetX,
					   playerObj->world_y - g_players[playerIdx].viewState.savedTargetY,
					   playerObj->world_z - g_players[playerIdx].viewState.savedTargetZ);
			g_players[playerIdx].viewState.viewRoll = 0;
			g_players[playerIdx].viewState.viewPitch = targetPitch;
			g_players[playerIdx].viewState.viewYaw = trig2_xyangle;
			FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
									(int16_t)g_players[playerIdx].viewState.viewPitch,
									(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
									(int16_t)g_players[playerIdx].viewState.hudAimX,
									(int16_t)g_players[playerIdx].viewState.hudAimY, NULL, playerIdx);
		} else if (g_players[playerIdx].viewState.externalCameraActive ||
				   g_players[playerIdx].viewState.playerInputBlocked) {
			int16_t transitionTimer;

			transitionTimer = (int16_t)g_players[playerIdx].viewState.transitionTimer;
			if (transitionTimer == 0) {
				g_players[playerIdx].viewState.viewRoll = g_objectTable[cameraFocusObjIdx].roll;
				g_players[playerIdx].viewState.viewPitch =
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].pitch;
				g_players[playerIdx].viewState.viewYaw =
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].yaw;
				FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
										(int16_t)g_players[playerIdx].viewState.viewPitch,
										(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
										(int16_t)g_players[playerIdx].viewState.hudAimX,
										(int16_t)g_players[playerIdx].viewState.hudAimY, NULL, playerIdx);

				Mission_ResolveObjectOrMissionPointWorldLoc(cameraFocusObjIdx, 0, 0, 0);
				g_players[playerIdx].viewState.savedTargetX = worldlocx;
				g_players[playerIdx].viewState.savedTargetY = worldlocy;
				g_players[playerIdx].viewState.savedTargetZ = worldlocz;

				g_players[playerIdx].viewState.savedTargetX -=
					Xwa_Q15Mul(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_X);
				g_players[playerIdx].viewState.savedTargetY -=
					Xwa_Q15Mul(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Y);
				g_players[playerIdx].viewState.savedTargetZ -=
					Xwa_Q15Mul(g_players[playerIdx].viewState.cameraDistance, g_camMatR2_Z);

				{
					int extentShift;
					int halfExtent;
					int clearanceX;
					int clearanceY;
					int clearanceZ;

					extentShift = 0;
					g_curModelMaxExtent =
						g_modelTypeTable[(uint16_t)
											 g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx]
												 .objectType]
							.maxBoundsExtent /
						2;
					halfExtent = g_curModelMaxExtent;
					if (halfExtent > 0x7fff) {
						do {
							halfExtent >>= 1;
							++extentShift;
						} while (halfExtent > 0x7fff);
						g_curModelMaxExtent = halfExtent;
					}

					clearanceX = Xwa_Q15Mul(halfExtent, g_camMatR2_X);
					clearanceY = Xwa_Q15Mul(g_curModelMaxExtent, g_camMatR2_Y);
					clearanceZ = Xwa_Q15Mul(g_curModelMaxExtent, g_camMatR2_Z);
					if (extentShift != 0) {
						clearanceX <<= extentShift;
						clearanceY <<= extentShift;
						clearanceZ <<= extentShift;
					}

					g_players[playerIdx].viewState.savedTargetX -= clearanceX;
					g_players[playerIdx].viewState.savedTargetY -= clearanceY;
					g_players[playerIdx].viewState.savedTargetZ -= clearanceZ;
				}

				if ((unsigned int)g_players[playerIdx].objectIndex != 0xffffu) {
					unsigned int playerObjIdx;
					unsigned int collisionObjIdx;

					playerObjIdx = (unsigned int)g_players[playerIdx].objectIndex;
					collisionObjIdx = (unsigned int)g_objectTable[playerObjIdx].mobj->collisionObjIdx;
					if (!g_inHangarReady && collisionObjIdx != 0xffffu && playerObjIdx != collisionObjIdx) {
						g_collisionScratchBackupSegmentStartWorldX = g_collisionSegmentStartWorldX;
						g_collisionScratchBackupSegmentStartWorldY = g_collisionSegmentStartWorldY;
						g_collisionScratchBackupSegmentStartWorldZ = g_collisionSegmentStartWorldZ;
						g_collisionScratchBackupProbeWorldX = g_collisionProbeWorldX;
						g_collisionScratchBackupProbeWorldY = g_collisionProbeWorldY;
						g_collisionScratchBackupProbeWorldZ = g_collisionProbeWorldZ;
						g_collisionScratchBackupSweepStartX = g_collisionSweepStartX;
						g_collisionScratchBackupSweepStartY = g_collisionSweepStartY;
						g_collisionScratchBackupSweepStartZ = g_collisionSweepStartZ;
						g_collisionScratchBackupSweepEndX = g_collisionSweepEndX;
						g_collisionScratchBackupSweepEndY = g_collisionSweepEndY;
						g_collisionScratchBackupSweepEndZ = g_collisionSweepEndZ;
						g_collisionScratchBackupHitOffsetX = g_collisionHitOffsetX;
						g_collisionScratchBackupHitOffsetY = g_collisionHitOffsetY;
						g_collisionScratchBackupHitOffsetZ = g_collisionHitOffsetZ;

						g_collisionSegmentStartWorldX = g_players[playerIdx].viewState.savedTargetX;
						g_collisionSegmentStartWorldY = g_players[playerIdx].viewState.savedTargetY;
						g_collisionSegmentStartWorldZ = g_players[playerIdx].viewState.savedTargetZ;
						g_collisionProbeWorldX = g_objectTable[playerObjIdx].world_x;
						g_collisionProbeWorldY = g_objectTable[playerObjIdx].world_y;
						g_collisionProbeWorldZ = g_objectTable[playerObjIdx].world_z;

						if (collide_CheckSweptModelCollision(playerObjIdx, collisionObjIdx) > 0) {
							g_players[playerIdx].viewState.savedTargetX +=
								(int64_t)((float)g_collisionHitOffsetX * 1.1f);
							g_players[playerIdx].viewState.savedTargetY +=
								(int64_t)((float)g_collisionHitOffsetY * 1.1f);
							g_players[playerIdx].viewState.savedTargetZ +=
								(int64_t)((float)g_collisionHitOffsetZ * 1.1f);
						} else if (g_flightPlayerCount == 1) {
							RenderObjectListEntry* entry;
							int originalCameraX;
							int originalCameraY;
							int originalCameraZ;

							entry = g_renderListHead;
							originalCameraX = g_players[playerIdx].viewState.savedTargetX;
							originalCameraY = g_players[playerIdx].viewState.savedTargetY;
							originalCameraZ = g_players[playerIdx].viewState.savedTargetZ;
							while (entry != NULL) {
								unsigned int entryObjIdx;
								uint16_t objectType;

								entryObjIdx = (unsigned int)entry->objectIdx;
								objectType = (uint16_t)g_objectTable[entryObjIdx].objectType;
								if (entry->sortDepth - g_modelTypeTable[objectType].maxBoundsExtent < viewZ &&
									entryObjIdx != playerObjIdx && entryObjIdx != collisionObjIdx) {
									if (objectType < OBJ_DSTankwlights) {
										if (objectType >= OBJ_DSReactorCoreRoom) {
											g_collisionSegmentStartWorldX =
												g_players[playerIdx].viewState.savedTargetX;
											g_collisionSegmentStartWorldY =
												g_players[playerIdx].viewState.savedTargetY;
											g_collisionSegmentStartWorldZ =
												g_players[playerIdx].viewState.savedTargetZ;
											g_collisionProbeWorldX = g_objectTable[playerObjIdx].world_x;
											g_collisionProbeWorldY = g_objectTable[playerObjIdx].world_y;
											g_collisionProbeWorldZ = g_objectTable[playerObjIdx].world_z;

											if (collide_CheckSweptModelCollision(playerObjIdx, entryObjIdx) >
												0) {
												g_players[playerIdx].viewState.savedTargetX =
													originalCameraX -
													(int64_t)((float)g_collisionHitOffsetX * 0.9f);
												g_players[playerIdx].viewState.savedTargetY =
													originalCameraY -
													(int64_t)((float)g_collisionHitOffsetY * 0.9f);
												g_players[playerIdx].viewState.savedTargetZ =
													originalCameraZ -
													(int64_t)((float)g_collisionHitOffsetZ * 0.9f);
											}
										}
									}
								}
								entry = entry->next;
							}
						}

						g_collisionSegmentStartWorldX = g_collisionScratchBackupSegmentStartWorldX;
						g_collisionSegmentStartWorldY = g_collisionScratchBackupSegmentStartWorldY;
						g_collisionSegmentStartWorldZ = g_collisionScratchBackupSegmentStartWorldZ;
						g_collisionProbeWorldX = g_collisionScratchBackupProbeWorldX;
						g_collisionProbeWorldY = g_collisionScratchBackupProbeWorldY;
						g_collisionProbeWorldZ = g_collisionScratchBackupProbeWorldZ;
						g_collisionSweepStartX = g_collisionScratchBackupSweepStartX;
						g_collisionSweepStartY = g_collisionScratchBackupSweepStartY;
						g_collisionSweepStartZ = g_collisionScratchBackupSweepStartZ;
						g_collisionSweepEndX = g_collisionScratchBackupSweepEndX;
						g_collisionSweepEndY = g_collisionScratchBackupSweepEndY;
						g_collisionSweepEndZ = g_collisionScratchBackupSweepEndZ;
						g_collisionHitOffsetX = g_collisionScratchBackupHitOffsetX;
						g_collisionHitOffsetY = g_collisionScratchBackupHitOffsetY;
						g_collisionHitOffsetZ = g_collisionScratchBackupHitOffsetZ;
					}
				}
			} else {
				uint16_t updatedTransitionTimer;

				updatedTransitionTimer = (uint16_t)((uint16_t)g_elapsedTicks + transitionTimer);
				g_players[playerIdx].viewState.transitionTimer = updatedTransitionTimer;
				if (updatedTransitionTimer >
					236u * (uint16_t)g_players[playerIdx].viewState.transitionDuration) {
					FlightView_FinishCameraFocusTransition(playerIdx,
														   g_players[playerIdx].viewState.transitionDuration);
				} else {
					ObjectRecord* focusObj;

					focusObj = &g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx];
					trig2_ctop(focusObj->world_x - g_players[g_localPlayer].viewState.savedTargetX,
							   focusObj->world_y - g_players[g_localPlayer].viewState.savedTargetY,
							   focusObj->world_z - g_players[g_localPlayer].viewState.savedTargetZ);
					g_players[g_localPlayer].viewState.viewRoll = 0;
					g_players[g_localPlayer].viewState.viewPitch = targetPitch;
					g_players[g_localPlayer].viewState.viewYaw = trig2_xyangle;
					FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
											(int16_t)g_players[playerIdx].viewState.viewPitch,
											(int16_t)g_players[playerIdx].viewState.viewYaw, 0, 0, 0, NULL,
											-1);
				}
			}
		} else {
			g_players[playerIdx].viewState.viewRoll = g_objectTable[cameraFocusObjIdx].roll;
			g_players[playerIdx].viewState.viewPitch =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].pitch;
			g_players[playerIdx].viewState.viewYaw =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].yaw;
			g_players[playerIdx].viewState.hudAimY = (uint16_t)g_players[playerIdx].lookYawOffset;
			g_players[playerIdx].viewState.hudAimX = (uint16_t)g_players[playerIdx].lookPitchOffset;
			g_players[playerIdx].viewState.viewYaw =
				(uint16_t)(g_players[playerIdx].viewState.viewYaw +
						   g_players[playerIdx].viewState.cameraYawDelta);
			g_players[playerIdx].viewState.viewPitch =
				(uint16_t)(g_players[playerIdx].viewState.viewPitch +
						   g_players[playerIdx].viewState.cameraPitchDelta);
			g_players[playerIdx].viewState.viewRoll =
				(uint16_t)(g_players[playerIdx].viewState.viewRoll +
						   g_players[playerIdx].viewState.cameraRollDelta);
			FVIEW_BuildCameraOrient(
				g_players[playerIdx].viewState.viewRoll, (int16_t)g_players[playerIdx].viewState.viewPitch,
				(int16_t)g_players[playerIdx].viewState.viewYaw, g_players[playerIdx].viewState.viewAngleD,
				(int16_t)g_players[playerIdx].viewState.hudAimX,
				(int16_t)g_players[playerIdx].viewState.hudAimY,
				&g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx], playerIdx);
			g_players[playerIdx].viewState.savedTargetX =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_x;
			g_players[playerIdx].viewState.savedTargetY =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_y;
			g_players[playerIdx].viewState.savedTargetZ =
				g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_z;
			if (g_players[playerIdx].viewState.cameraFocusObjIdx == g_players[playerIdx].objectIndex) {
				ComputeHardpointWorldPos(playerIdx);
				g_players[playerIdx].viewState.savedTargetX += (int)g_players[playerIdx].hardpointWorldX;
				g_players[playerIdx].viewState.savedTargetY += (int)g_players[playerIdx].hardpointWorldY;
				g_players[playerIdx].viewState.savedTargetZ += (int)g_players[playerIdx].hardpointWorldZ;
			}
			g_players[playerIdx].viewState.savedTargetX +=
				g_players[playerIdx].viewState.cameraPanDeltaX >> 4;
			g_players[playerIdx].viewState.savedTargetY +=
				g_players[playerIdx].viewState.cameraPanDeltaY >> 4;
			g_players[playerIdx].viewState.savedTargetZ +=
				g_players[playerIdx].viewState.cameraPanDeltaZ >> 4;
		}
	}

	if (g_players[playerIdx].hyperspacePhase != PLAYER_HYPERSPACE_PHASE_NONE) {
		g_players[playerIdx].viewState.viewRoll = 0;
		g_players[playerIdx].viewState.viewPitch = 0x4000;
		g_players[playerIdx].viewState.viewYaw = 0;
		FVIEW_BuildCameraOrient(
			g_players[playerIdx].viewState.viewRoll, (int16_t)g_players[playerIdx].viewState.viewPitch, 0,
			g_players[playerIdx].viewState.viewAngleD, (int16_t)g_players[playerIdx].viewState.hudAimX,
			(int16_t)g_players[playerIdx].viewState.hudAimY,
			&g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx], playerIdx);

		if (g_players[playerIdx].hyperspacePhase == PLAYER_HYPERSPACE_OUTBOUND) {
			int phaseTicks;

			phaseTicks = g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks;
			g_players[playerIdx].viewState.savedTargetY += 16 * phaseTicks;
		} else if (g_players[playerIdx].hyperspacePhase == PLAYER_HYPERSPACE_INBOUND) {
			int phaseTicks;

			phaseTicks = 236 - g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks;
			if (phaseTicks < 0) {
				phaseTicks = 0;
			}
			g_players[playerIdx].viewState.savedTargetY += 16 * phaseTicks;
		}
	}
}

// FUNCTION: XWA 0x4F1B00
void FlightView_ComputeObjectViewPosition(uint16_t objectIdx) {
	g_camRelWorldX = g_objectTable[objectIdx].world_x - g_players[g_localPlayer].viewState.savedTargetX;
	g_camRelWorldY = g_objectTable[objectIdx].world_y - g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldZ = g_objectTable[objectIdx].world_z - g_players[g_localPlayer].viewState.savedTargetZ;
	viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
}

// FUNCTION: XWA 0x4F1BB0
int FlightView_IsObjectSphereVisible(int objectIdx, int sphereRadius) {
	int zPlusRadius;
	int absCamX;
	int absCamY;

	g_camRelWorldX = g_objectTable[objectIdx].world_x - g_players[g_localPlayer].viewState.savedTargetX;
	g_camRelWorldY = g_objectTable[objectIdx].world_y - g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldZ = g_objectTable[objectIdx].world_z - g_players[g_localPlayer].viewState.savedTargetZ;

	zPlusRadius = viewZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	zPlusRadius += sphereRadius;
	if (zPlusRadius < 0) {
		return 0;
	}
	if ((zPlusRadius >> 8) > sphereRadius) {
		return 0;
	}

	absCamX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewX = absCamX;
	if (absCamX < 0) {
		absCamX = -absCamX;
	}
	if (absCamX - sphereRadius > zPlusRadius) {
		return 0;
	}

	absCamY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewY = absCamY;
	if (absCamY < 0) {
		absCamY = -absCamY;
	}
	return absCamY - sphereRadius <= zPlusRadius;
}

// FUNCTION: XWA 0x4F1610
char FlightView_IsLensFlareSourceVisible(int worldX, int worldY, int worldZ) {
	PlayerData* player;
	int localPlayerIdx;
	int playerObjIdx;
	RenderObjectListEntry* entry;
	ObjectRecord* objectTable;
	int visible;

	g_collisionScratchBackupSegmentStartWorldX = g_collisionSegmentStartWorldX;
	g_collisionScratchBackupSegmentStartWorldY = g_collisionSegmentStartWorldY;
	g_collisionScratchBackupSegmentStartWorldZ = g_collisionSegmentStartWorldZ;
	g_collisionScratchBackupProbeWorldX = g_collisionProbeWorldX;
	g_collisionScratchBackupProbeWorldY = g_collisionProbeWorldY;
	g_collisionScratchBackupProbeWorldZ = g_collisionProbeWorldZ;
	g_collisionScratchBackupSweepStartX = g_collisionSweepStartX;
	g_collisionScratchBackupSweepStartY = g_collisionSweepStartY;
	g_collisionScratchBackupSweepStartZ = g_collisionSweepStartZ;
	g_collisionScratchBackupSweepEndX = g_collisionSweepEndX;
	g_collisionScratchBackupSweepEndY = g_collisionSweepEndY;
	g_collisionScratchBackupSweepEndZ = g_collisionSweepEndZ;
	g_collisionScratchBackupHitOffsetX = g_collisionHitOffsetX;
	g_collisionScratchBackupHitOffsetY = g_collisionHitOffsetY;
	g_collisionScratchBackupHitOffsetZ = g_collisionHitOffsetZ;

	localPlayerIdx = g_localPlayer;
	player = &g_players[localPlayerIdx];
	playerObjIdx = player->objectIndex;
	visible = 1;

	g_collisionProbeWorldX = player->viewState.savedTargetX;
	g_collisionProbeWorldY = player->viewState.savedTargetY;
	g_collisionProbeWorldZ = player->viewState.savedTargetZ;
	g_collisionSegmentStartWorldX = worldX;
	g_collisionSegmentStartWorldZ = worldZ;
	objectTable = g_objectTable;
	g_collisionSegmentStartWorldY = worldY;

	for (entry = g_renderListHead; entry != NULL; entry = entry->next) {
		unsigned int objIdx;

		objIdx = (unsigned int)entry->objectIdx;
		if (entry->viewZ + g_modelTypeTable[(uint16_t)objectTable[objIdx].objectType].maxBoundsExtent > 0 &&
			playerObjIdx != (int)objIdx) {
			if (collide_CheckSweptModelCollision((unsigned int)playerObjIdx, objIdx) > 0) {
				visible = 0;
				break;
			}
		}
	}

	if (player->viewState.externalCameraActive && visible) {
		if (g_exteriorModelLoaded) {
			ObjectRecord* playerObj;
			uint16_t objectType;
			uint16_t savedModel;
			int setDrawingOwnCraft;

			playerObj = &g_objectTable[playerObjIdx];
			objectType = (uint16_t)playerObj->objectType;
			savedModel = g_loadedModels.byObjectType[objectType];
			setDrawingOwnCraft = 0;
			if (!g_drawingOwnCraft) {
				setDrawingOwnCraft = 1;
				g_drawingOwnCraft = 1;
			}

			g_loadedModels.byObjectType[objectType] = g_exteriorModel;
			if (collide_CheckSweptModelCollision((unsigned int)playerObjIdx, (unsigned int)playerObjIdx) >
				0) {
				visible = 0;
			}
			g_loadedModels.byObjectType[objectType] = savedModel;
			if (setDrawingOwnCraft) {
				g_drawingOwnCraft = 0;
			}
		} else if (collide_CheckSweptModelCollision((unsigned int)playerObjIdx, (unsigned int)playerObjIdx) >
				   0) {
			visible = 0;
		}
	} else if (player->cockpitVisible && player->currentSeatIdx == 0 && visible) {
		ObjectRecord* playerObj;
		MobileObject* mobj;
		uint16_t objectType;
		uint16_t savedModel;
		int setCockpitViewActive;
		float localSegmentStart[3];
		float localSegmentEnd[3];

		playerObj = &g_objectTable[playerObjIdx];
		objectType = (uint16_t)playerObj->objectType;
		savedModel = g_loadedModels.byObjectType[objectType];
		setCockpitViewActive = 0;
		if (!g_cockpitViewActive) {
			setCockpitViewActive = 1;
			g_cockpitViewActive = 1;
		}

		g_loadedModels.byObjectType[objectType] = g_cockpitModel;
		localSegmentEnd[0] = player->hardpointLocalX;
		localSegmentEnd[1] = player->hardpointLocalY;
		localSegmentEnd[2] = player->hardpointLocalZ;

		mobj = playerObj->mobj;
		worldX = worldX - playerObj->world_x;
		worldY = worldY - playerObj->world_y;
		worldZ = worldZ - playerObj->world_z;

		localSegmentStart[0] = (float)Xwa_Dot3Q15Inline(worldX, worldY, worldZ, mobj->cachedSideX,
														mobj->cachedSideY, mobj->cachedSideZ);
		localSegmentStart[1] = -(float)Xwa_Dot3Q15Inline(worldX, worldY, worldZ, mobj->cachedFwdX,
														 mobj->cachedFwdY, mobj->cachedFwdZ);
		localSegmentStart[2] = (float)Xwa_Dot3Q15Inline(worldX, worldY, worldZ, mobj->cachedUpX,
														mobj->cachedUpY, mobj->cachedUpZ);

		if (collide_CheckLocalSweepAgainstObjectModel((unsigned int)g_players[g_localPlayer].objectIndex,
													  (unsigned int)g_players[g_localPlayer].objectIndex,
													  localSegmentEnd, localSegmentStart, -1, 1) != 0) {
			visible = 0;
		}

		g_loadedModels.byObjectType[objectType] = savedModel;
		if (setCockpitViewActive) {
			g_cockpitViewActive = 0;
		}
	}

	g_collisionSegmentStartWorldX = g_collisionScratchBackupSegmentStartWorldX;
	g_collisionSegmentStartWorldY = g_collisionScratchBackupSegmentStartWorldY;
	g_collisionSegmentStartWorldZ = g_collisionScratchBackupSegmentStartWorldZ;
	g_collisionProbeWorldX = g_collisionScratchBackupProbeWorldX;
	g_collisionProbeWorldY = g_collisionScratchBackupProbeWorldY;
	g_collisionProbeWorldZ = g_collisionScratchBackupProbeWorldZ;
	g_collisionSweepStartX = g_collisionScratchBackupSweepStartX;
	g_collisionSweepStartY = g_collisionScratchBackupSweepStartY;
	g_collisionSweepStartZ = g_collisionScratchBackupSweepStartZ;
	g_collisionSweepEndX = g_collisionScratchBackupSweepEndX;
	g_collisionSweepEndY = g_collisionScratchBackupSweepEndY;
	g_collisionSweepEndZ = g_collisionScratchBackupSweepEndZ;
	g_collisionHitOffsetX = g_collisionScratchBackupHitOffsetX;
	g_collisionHitOffsetY = g_collisionScratchBackupHitOffsetY;
	g_collisionHitOffsetZ = g_collisionScratchBackupHitOffsetZ;
	return visible;
}

static __inline ViewportCullFlags FlightView_CullRelativeSphereToViewport(int sphereRadius,
																		  int* projectedRadiusOut,
																		  int storeRadiusBeforeScreenCull) {
	ViewportCullFlags clipFlags;
	int camZ;
	int zPlusRadius;
	int absCamX;
	int absCamY;
	int screenX;
	int screenY;
	int depthDivisor;
	int projectedRadius;
	int paddedProjectedRadius;
	uint32_t projectedRadiusProduct;

	clipFlags = VIEWPORT_CULL_NONE;
	camZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	zPlusRadius = camZ + sphereRadius;
	viewZ = camZ;
	if (zPlusRadius < 0) {
		return VIEWPORT_CULL_NONE;
	}

	if (camZ < sphereRadius) {
		clipFlags = VIEWPORT_CULL_NEAR;
	}

	if ((zPlusRadius >> 8) > sphereRadius) {
		return VIEWPORT_CULL_NONE;
	}

	absCamX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewX = absCamX;
	if (absCamX < 0) {
		absCamX = -absCamX;
	}
	if (absCamX - sphereRadius > zPlusRadius) {
		return VIEWPORT_CULL_NONE;
	}

	absCamY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewY = absCamY;
	if (absCamY < 0) {
		absCamY = -absCamY;
	}
	if (absCamY - sphereRadius > zPlusRadius) {
		return VIEWPORT_CULL_NONE;
	}

	screenX = TRANSFM2_ProjectScreenX(viewX, viewZ);
	screenY = TRANSFM2_ProjectScreenY(viewY, viewZ);
	depthDivisor = viewZ;
	if (depthDivisor <= 0) {
		depthDivisor = 1;
	}

	projectedRadiusProduct = (uint32_t)sphereRadius * (uint32_t)g_projScaleInt;
	projectedRadius = (int)(projectedRadiusProduct / (uint32_t)depthDivisor);
	if (projectedRadius < 0) {
		return VIEWPORT_CULL_NONE;
	}

	paddedProjectedRadius = projectedRadius + 2;
	if (storeRadiusBeforeScreenCull) {
		*projectedRadiusOut = paddedProjectedRadius;
	}

	if (screenX - paddedProjectedRadius <= 0) {
		clipFlags = (ViewportCullFlags)(clipFlags + VIEWPORT_CULL_LEFT);
		if (screenX + 2 * paddedProjectedRadius < 0) {
			return VIEWPORT_CULL_NONE;
		}
	}

	if (screenX + paddedProjectedRadius >= g_flightVpWidth) {
		clipFlags = (ViewportCullFlags)(clipFlags + VIEWPORT_CULL_RIGHT);
		if (screenX - 2 * paddedProjectedRadius > g_flightVpWidth) {
			return VIEWPORT_CULL_NONE;
		}
	}

	if (screenY - paddedProjectedRadius < 0) {
		clipFlags = (ViewportCullFlags)(clipFlags + VIEWPORT_CULL_TOP);
		if (screenY + 2 * paddedProjectedRadius < 0) {
			return VIEWPORT_CULL_NONE;
		}
	}

	if (screenY + paddedProjectedRadius > g_flightVpMaxY) {
		clipFlags = (ViewportCullFlags)(clipFlags + VIEWPORT_CULL_BOTTOM);
		if (screenY - 2 * paddedProjectedRadius > g_flightVpHeight) {
			return VIEWPORT_CULL_NONE;
		}
	}

	if (!storeRadiusBeforeScreenCull) {
		*projectedRadiusOut = paddedProjectedRadius;
	}

	if (clipFlags == VIEWPORT_CULL_NONE) {
		clipFlags = VIEWPORT_CULL_INSIDE;
	}
	return clipFlags;
}

// FUNCTION: XWA 0x4F1CA0
ViewportCullFlags FlightView_CullObjectSphereToViewport(int objectIdx, int sphereRadius,
														int* projectedRadiusOut) {
	ViewportCullFlags clipFlags;
	int zPlusRadius;
	int absCamX;
	int absCamY;
	int screenX;
	int screenY;
	int depthDivisor;
	int projectedRadius;
	int paddedProjectedRadius;
	uint32_t projectedRadiusProduct;

	clipFlags = VIEWPORT_CULL_NONE;

	g_camRelWorldX = g_objectTable[objectIdx].world_x - g_players[g_localPlayer].viewState.savedTargetX;
	g_camRelWorldY = g_objectTable[objectIdx].world_y - g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldZ = g_objectTable[objectIdx].world_z - g_players[g_localPlayer].viewState.savedTargetZ;

	zPlusRadius = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewZ = zPlusRadius;
	zPlusRadius += sphereRadius;
	if (zPlusRadius < 0) {
		return VIEWPORT_CULL_NONE;
	}

	if (viewZ < sphereRadius) {
		clipFlags = VIEWPORT_CULL_NEAR;
	}

	if ((zPlusRadius >> 8) > sphereRadius) {
		return VIEWPORT_CULL_NONE;
	}

	absCamX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewX = absCamX;
	if (absCamX < 0) {
		absCamX = -absCamX;
	}
	if (absCamX - sphereRadius > zPlusRadius) {
		return VIEWPORT_CULL_NONE;
	}

	absCamY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
	viewY = absCamY;
	if (absCamY < 0) {
		absCamY = -absCamY;
	}
	if (absCamY - sphereRadius > zPlusRadius) {
		return VIEWPORT_CULL_NONE;
	}

	screenX = TRANSFM2_ProjectScreenX(viewX, viewZ);
	screenY = TRANSFM2_ProjectScreenY(viewY, viewZ);
	depthDivisor = viewZ;
	if (depthDivisor <= 0) {
		depthDivisor = 1;
	}

	projectedRadiusProduct = (uint32_t)sphereRadius * (uint32_t)g_projScaleInt;
	projectedRadius = (int)(projectedRadiusProduct / (uint32_t)depthDivisor);
	if (projectedRadius < 0) {
		return VIEWPORT_CULL_NONE;
	}

	paddedProjectedRadius = projectedRadius + 2;
	*projectedRadiusOut = paddedProjectedRadius;

	if (screenX - paddedProjectedRadius <= 0) {
		clipFlags = (ViewportCullFlags)(clipFlags + VIEWPORT_CULL_LEFT);
		if (screenX + 2 * paddedProjectedRadius < 0) {
			return VIEWPORT_CULL_NONE;
		}
	}

	if (screenX + paddedProjectedRadius >= g_flightVpWidth) {
		clipFlags = (ViewportCullFlags)(clipFlags + VIEWPORT_CULL_RIGHT);
		if (screenX - 2 * paddedProjectedRadius > g_flightVpWidth) {
			return VIEWPORT_CULL_NONE;
		}
	}

	if (screenY - paddedProjectedRadius < 0) {
		clipFlags = (ViewportCullFlags)(clipFlags + VIEWPORT_CULL_TOP);
		if (screenY + 2 * paddedProjectedRadius < 0) {
			return VIEWPORT_CULL_NONE;
		}
	}

	if (screenY + paddedProjectedRadius > g_flightVpMaxY) {
		clipFlags = (ViewportCullFlags)(clipFlags + VIEWPORT_CULL_BOTTOM);
		if (screenY - 2 * paddedProjectedRadius > g_flightVpHeight) {
			return VIEWPORT_CULL_NONE;
		}
	}

	if (clipFlags == VIEWPORT_CULL_NONE) {
		clipFlags = VIEWPORT_CULL_INSIDE;
	}
	return clipFlags;
}

// FUNCTION: XWA 0x4F1E90
ViewportCullFlags FlightView_CullWorldSphereToViewport(int worldX, int worldY, int worldZ, int sphereRadius,
													   int* projectedRadiusOut) {
	g_camRelWorldX = worldX - g_players[g_localPlayer].viewState.savedTargetX;
	g_camRelWorldY = worldY - g_players[g_localPlayer].viewState.savedTargetY;
	g_camRelWorldZ = worldZ - g_players[g_localPlayer].viewState.savedTargetZ;

	return FlightView_CullRelativeSphereToViewport(sphereRadius, projectedRadiusOut, 0);
}

static __inline int FlightView_IsStaticRenderGenus(ModelGenusId genusId) {
	switch (genusId) {
		case GENUS_Mine:
		case GENUS_Asteroid:
		case GENUS_Debris:
		case GENUS_DeathStarTunnelSegment:
			return 1;
		default:
			return 0;
	}
}

static __inline void FlightView_BuildPointLightsIfLarge(const RenderObjectListEntry* entry,
														ObjectRecord* obj) {
	if (entry->projectedRadius > g_flightVpProjScaleX) {
		FlightLight_BuildObjectPointLights(obj);
	}
}

static __inline void FlightView_QueueHardwareGlowMarksIfNeeded(uint16_t objectIdx,
															   const RenderObjectListEntry* entry) {
	if (g_useHardware3D && objectIdx != (uint16_t)g_players[g_localPlayer].objectIndex &&
		(entry->projectedRadius > 5 ||
		 objectIdx == (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx)) {
		GlowMark_QueueCraftDamageSurfaceEffects(objectIdx);
		g_objRenderState[objectIdx].drawnThisFrame = 1;
		if (g_objRenderState[objectIdx].pendingGlowMarks != NULL) {
			GlowMark_ProcessPendingRequests(objectIdx);
		}
	}
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x4EFE00
char FlightView_Render(void) {
	int savedMapCameraState;
	int localPlayerIdx;
	uint32_t objIdx;
	uint16_t objectType;
	uint32_t objectIdx;
	int projectedRadius;
	int lightWorldX;
	int lightWorldY;
	int lightWorldZ;
	RenderObjectListEntry* entry;

	localPlayerIdx = g_localPlayer;
	if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
		PlayerViewState* viewState;

		viewState = &g_players[localPlayerIdx].viewState;
		memcpy(&g_savedPlayerViewStateForPlaybackCamera, viewState,
			   sizeof(g_savedPlayerViewStateForPlaybackCamera));
		savedMapCameraState = g_players[localPlayerIdx].mapCameraState;
		g_players[localPlayerIdx].mapCameraState = 0;
		memcpy(viewState, &g_filmOverlayViewState, sizeof(*viewState));
	}

	if (g_players[localPlayerIdx].mapCameraState) {
		FlightMap_RenderView();
		goto restore_film_camera;
	}

	if (g_players[localPlayerIdx].hyperspacePhase) {
		if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
			memcpy(&g_players[localPlayerIdx].viewState, &g_savedPlayerViewStateForPlaybackCamera,
				   sizeof(g_players[localPlayerIdx].viewState));
		}
		RenderScene_Initialize(1);
		g_renderFlags = RENDER_SCENE_CLIP_FORCE_FULL;
		if (g_players[g_localPlayer].cockpitVisible) {
			if (g_useHardware3D) {
				Flight_RenderHyperspaceTransitionEffects();
				FlightView_RenderCockpitModel();
			} else {
				FlightView_RenderCockpitModel();
				Flight_RenderHyperspaceTransitionEffects();
			}
		} else {
			Flight_RenderHyperspaceTransitionEffects();
		}
		if (g_useHardware3D || (Hud_RenderHud(), g_useHardware3D)) {
			Hud_UpdateHUD();
		}
		RenderScene_DrawVisibleFaces();
		if (!g_useHardware3D) {
			Hud_UpdateHUD();
		}
		goto restore_film_camera;
	}

	g_scenePointLightCount = 0;
	if (g_deathStarTunnelLaserRegions[regionIdx].enabled &&
		g_deathStarTunnelLaserRegions[regionIdx].beamLightActive) {
		PointLight* laserLight;

		laserLight = &g_scenePointLights[0];
		laserLight->fixed.x = g_deathStarTunnelLaserRegions[regionIdx].pointLightX;
		laserLight->fixed.y = g_deathStarTunnelLaserRegions[regionIdx].pointLightY;
		laserLight->fixed.z = g_deathStarTunnelLaserRegions[regionIdx].pointLightZ;
		laserLight->intensity = 100000.0f;
		laserLight->colorR = 0.0f;
		laserLight->colorG = 1.0f;
		laserLight->colorB = 0.2f;
		laserLight->cullRadius = 0x4000;
		g_scenePointLightCount = 1;
	}

	for (objIdx = g_activeRegionObjectSlotStart; (uint16_t)objIdx < g_explosionObjectSlotEnd; ++objIdx) {
		objectIdx = (uint16_t)objIdx;
		if (g_objectTable[objectIdx].objectType != OBJ_None &&
			(!g_provingGroundsModeActive || Yard_ShouldRenderChallengeObject(objectIdx))) {
			FlightLight_AppendScenePointLightForObject(&g_objectTable[objectIdx]);
		}
	}
	for (objIdx = g_localEffectSlotStart; (uint16_t)objIdx < (uint32_t)g_localTransientSlotEnd; ++objIdx) {
		ObjectRecord* obj;

		objectIdx = (uint16_t)objIdx;
		obj = &g_objectTable[objectIdx];
		if (obj->objectType != OBJ_None) {
			FlightLight_AppendScenePointLightForObject(obj);
		}
	}

	g_flightSurfaceAlreadyLocked = 0;
	if (g_flightInitialTextureCacheFlushPending) {
		if (g_useHardware3D) {
			Renderer_FlushTextureCacheAndReturnTrue();
		}
		g_flightInitialTextureCacheFlushPending = 0;
	}
	if (g_unusedLocalPlayerHitGlowMarksPending) {
		g_unusedLocalPlayerHitGlowMarksPending = 0;
		GlowMark_SpawnLocalPlayerHitEffects();
	}

	FlightSurface_Lock();
	FlightStarfield_Render();
	FlightSurface_Unlock();
	RenderScene_Initialize(1);
	g_sceneBillboardQueueCount = 0;
	Backdrop_RenderCurrentRegion();
	RenderList_Reset();

	for (objIdx = g_regionMainObjectSlotStart; (uint16_t)objIdx < g_regionMainObjectSlotEnd; ++objIdx) {
		ViewportCullFlags cullFlags;
		int maxBoundsExtent;

		objectIdx = (uint16_t)objIdx;
		objectType = g_objectTable[objectIdx].objectType;
		if (objectType == OBJ_None) {
			continue;
		}
		if (g_provingGroundsModeActive && !Yard_ShouldRenderChallengeObject(objectIdx)) {
			continue;
		}
		if (g_useHardware3D) {
			g_objRenderState[objectIdx].drawnThisFrame = 0;
		}

		if (objectIdx == (uint16_t)g_players[g_localPlayer].viewState.cameraFocusObjIdx &&
			!g_players[g_localPlayer].viewState.externalCameraActive &&
			(!g_filmPlaybackMode || g_filmOverlayActive != 1)) {
			if (g_players[g_localPlayer].cockpitVisible) {
				if (g_players[g_localPlayer].currentSeatIdx == 0 &&
					g_players[g_localPlayer].cockpitLookAvailable) {
					RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ,
										   VIEWPORT_CULL_RIGHT | VIEWPORT_CULL_LEFT | VIEWPORT_CULL_TOP |
											   VIEWPORT_CULL_BOTTOM | VIEWPORT_CULL_NEAR,
										   *(int*)&g_screenWidth);
				}
				if (g_players[g_localPlayer].currentSeatIdx > 0 &&
					g_players[g_localPlayer].cockpitToggleAvailable) {
					RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ,
										   VIEWPORT_CULL_RIGHT | VIEWPORT_CULL_LEFT | VIEWPORT_CULL_TOP |
											   VIEWPORT_CULL_BOTTOM | VIEWPORT_CULL_NEAR,
										   *(int*)&g_screenWidth);
				}
			}
			continue;
		}

		if (objectType == OBJ_NoAsset_222 && g_objectTable[OBJ_NoAsset_222].mobj != NULL) {
			maxBoundsExtent =
				g_modelTypeTable[g_objectTable[objectIdx].mobj->sourceObjectType].maxBoundsExtent;
		} else {
			maxBoundsExtent = g_modelTypeTable[(uint16_t)objectType].maxBoundsExtent;
		}
		g_curModelMaxExtent = maxBoundsExtent;
		switch (g_objectTable[objectIdx].genusId) {
			case GENUS_Fighter:
			case GENUS_Transport:
			case GENUS_Utility:
			case GENUS_Freighter:
			case GENUS_Starship:
			case GENUS_Platform:
			case GENUS_SatelliteBuoy:
			case GENUS_LargeScenery:
			case GENUS_Container:
			case GENUS_PilotDroid:
			case GENUS_WeaponEmplacement:
				g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
				if (g_curCraft->objectKind == 7) {
					break;
				}
				cullFlags =
					FlightView_CullObjectSphereToViewport(objectIdx, maxBoundsExtent, &projectedRadius);
				g_renderFlags = cullFlags;
				if (cullFlags != VIEWPORT_CULL_NONE) {
					RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ, cullFlags, projectedRadius);
				}
				break;
			case GENUS_PlayerProjectile:
			case GENUS_NpcProjectile:
			case GENUS_Debris:
			case GENUS_Rubble:
				cullFlags =
					FlightView_CullObjectSphereToViewport(objectIdx, 2 * maxBoundsExtent, &projectedRadius);
				g_renderFlags = cullFlags;
				if (g_objectTable[objectIdx].objectType == OBJ_LaserImperialDS) {
					cullFlags = (ViewportCullFlags)RENDER_SCENE_CLIP_FORCE_FULL;
					projectedRadius = 100;
					g_renderFlags = cullFlags;
				}
				if (cullFlags != VIEWPORT_CULL_NONE) {
					int sortDepth;

					sortDepth = viewZ;
					if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) {
						sortDepth = 0x7fffffff;
					}
					RenderList_QueueObject(objectIdx, sortDepth, viewX, viewY, viewZ, cullFlags,
										   projectedRadius);
				}
				break;
			case GENUS_Explosion:
				cullFlags =
					FlightView_CullObjectSphereToViewport(objectIdx, maxBoundsExtent, &projectedRadius);
				g_renderFlags = cullFlags;
				if (cullFlags != VIEWPORT_CULL_NONE && viewZ > 0) {
					RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ, cullFlags, projectedRadius);
				}
				break;
			case GENUS_SalvageJunk:
				cullFlags =
					FlightView_CullObjectSphereToViewport(objectIdx, maxBoundsExtent, &projectedRadius);
				g_renderFlags = cullFlags;
				if (cullFlags != VIEWPORT_CULL_NONE) {
					RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ, cullFlags, projectedRadius);
				}
				break;
			default:
				break;
		}
	}

	if (!g_filmPlaybackMode || g_filmOverlayActive != 1 || savedMapCameraState == 0) {
		int regionIndex;
		uint32_t backdropIdx;

		regionIndex = g_players[g_localPlayer].regionIndex;
		for (backdropIdx = 0; backdropIdx < (uint32_t)g_backdropCountByRegion[regionIndex]; ++backdropIdx) {
			WorldRectRecord* backdrop;

			backdrop = &g_backdropRecordsByRegion[regionIndex][backdropIdx];
			if (backdrop->modelType >= OBJ_BackdropTextureGroup9001_Sprite1100_521 &&
				backdrop->modelType <= OBJ_BackdropTextureGroup9010_Sprite2000 &&
				backdrop->viewDirQ20.z > 0 &&
				FlightView_IsLensFlareSourceVisible(
					backdrop->worldDirQ20.x + g_players[g_localPlayer].viewState.savedTargetX,
					backdrop->worldDirQ20.y + g_players[g_localPlayer].viewState.savedTargetY,
					backdrop->worldDirQ20.z + g_players[g_localPlayer].viewState.savedTargetZ)) {
				viewX = backdrop->viewDirQ20.x;
				viewY = backdrop->viewDirQ20.y;
				viewZ = backdrop->viewDirQ20.z;
				LensFlare_QueueSource(-1);
			}
		}
	}

	for (objIdx = (uint16_t)g_objScanStart; (uint16_t)objIdx < g_regionStaticObjectSlotEnd; ++objIdx) {
		ObjectRecord* obj;
		ViewportCullFlags cullFlags;

		obj = &g_objectTable[(uint16_t)objIdx];
		if (obj->objectType == OBJ_None) {
			continue;
		}
		if (g_useHardware3D) {
			g_objRenderState[(uint16_t)objIdx].drawnThisFrame = 0;
		}

		g_curModelMaxExtent = g_modelTypeTable[(uint16_t)obj->objectType].maxBoundsExtent;
		if (!FlightView_IsStaticRenderGenus(obj->genusId)) {
			continue;
		}

		cullFlags = FlightView_CullWorldSphereToViewport(obj->world_x, obj->world_y, obj->world_z,
														 g_curModelMaxExtent, &projectedRadius);
		g_renderFlags = cullFlags;
		if (cullFlags != VIEWPORT_CULL_NONE) {
			RenderList_QueueObject((uint16_t)objIdx, viewZ, viewX, viewY, viewZ, cullFlags, projectedRadius);
		}
	}
	for (objIdx = (uint16_t)g_localTransientSlotStart; (uint16_t)objIdx < (uint32_t)g_localTransientSlotEnd;
		 ++objIdx) {
		ObjectRecord* obj;
		ViewportCullFlags cullFlags;

		obj = &g_objectTable[(uint16_t)objIdx];
		if (obj->objectType == OBJ_None) {
			continue;
		}
		if (obj->genusId == GENUS_Debris && g_players[g_localPlayer].viewState.externalCameraActive) {
			continue;
		}

		g_curModelMaxExtent = g_modelTypeTable[(uint16_t)obj->objectType].maxBoundsExtent;
		cullFlags =
			FlightView_CullObjectSphereToViewport((uint16_t)objIdx, g_curModelMaxExtent, &projectedRadius);
		g_renderFlags = cullFlags;
		if (cullFlags != VIEWPORT_CULL_NONE && viewZ > 0) {
			RenderList_QueueObject((uint16_t)objIdx, viewZ, viewX, viewY, viewZ, cullFlags, projectedRadius);
		}
	}

	RenderList_SortDepthAscending();
	for (entry = g_renderListHead; entry != NULL; entry = entry->next) {
		ObjectRecord* obj;

		objectIdx = (uint16_t)entry->objectIdx;
		obj = &g_objectTable[objectIdx];
		if (objectIdx >= (uint16_t)g_regionMainObjectSlotStart &&
			objectIdx < (uint16_t)g_regionMainObjectSlotEnd) {
			FlightView_QueueHardwareGlowMarksIfNeeded(objectIdx, entry);

			switch (obj->genusId) {
				case GENUS_Fighter:
				case GENUS_Transport:
				case GENUS_Utility:
				case GENUS_Freighter:
				case GENUS_Starship:
				case GENUS_Platform:
				case GENUS_SatelliteBuoy:
				case GENUS_LargeScenery:
				case GENUS_Container:
				case GENUS_PilotDroid:
				case GENUS_WeaponEmplacement: {
					uint16_t savedModel;

					savedModel = 0xffffu;
					g_curCraft = obj->mobj->pCraft;
					FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, obj);

					if (objectIdx == (uint16_t)g_players[g_localPlayer].viewState.cameraFocusObjIdx &&
						!g_players[g_localPlayer].viewState.externalCameraActive && !g_replayViewMode) {
						if (obj->objectType == OBJ_None) {
							continue;
						}
						savedModel = g_loadedModels.byObjectType[(uint16_t)obj->objectType];
						g_curModelMaxExtent = g_modelTypeTable[(uint16_t)obj->objectType].maxBoundsExtent;
						g_camRelWorldX = obj->world_x;
						g_camRelWorldY = obj->world_y;
						g_camRelWorldZ = obj->world_z;
						g_cockpitViewActive = 1;
						if (g_players[g_localPlayer].currentSeatIdx == 0) {
							g_loadedModels.byObjectType[(uint16_t)obj->objectType] = g_cockpitModel;
						} else {
							ModelIndex modelIndex;
							uint16_t turretModelType;

							modelIndex = (ModelIndex)GetModelIndexFromType(obj->objectType);
							turretModelType =
								g_modelDefs[(uint16_t)modelIndex]
									.turretModelIndex[g_players[g_localPlayer].currentSeatIdx - 1];
							if (turretModelType != 0) {
								g_loadedModels.byObjectType[(uint16_t)obj->objectType] =
									g_loadedModels.byObjectType[turretModelType];
							} else {
								OutputDebugStringA("Can't find cockpit model\n");
							}
						}
						if (g_useHardware3D) {
							GlowMark_QueueCraftDamageSurfaceEffects(objectIdx);
							g_objRenderState[objectIdx].drawnThisFrame = 1;
							if (g_objRenderState[objectIdx].pendingGlowMarks != NULL) {
								GlowMark_ProcessPendingRequests(objectIdx);
							}
						}
					} else if (objectIdx == (uint16_t)g_players[g_localPlayer].objectIndex &&
							   g_players[g_localPlayer].viewState.externalCameraActive) {
						g_drawingOwnCraft = 1;
						savedModel = g_loadedModels.byObjectType[(uint16_t)obj->objectType];
						if (g_exteriorModelLoaded) {
							g_loadedModels.byObjectType[(uint16_t)obj->objectType] = g_exteriorModel;
						}
						viewX = entry->viewX;
						viewY = entry->viewY;
						viewZ = entry->viewZ;
						if (g_useHardware3D) {
							GlowMark_QueueCraftDamageSurfaceEffects(objectIdx);
							g_objRenderState[objectIdx].drawnThisFrame = 1;
							if (g_objRenderState[objectIdx].pendingGlowMarks != NULL) {
								GlowMark_ProcessPendingRequests(objectIdx);
							}
						}
					} else {
						g_cockpitViewActive = 0;
						g_drawingOwnCraft = 0;
						viewX = entry->viewX;
						viewY = entry->viewY;
						viewZ = entry->viewZ;
					}

					g_renderFlags = entry->cullFlags;
					{
						int lightIdx;

						for (lightIdx = 0; lightIdx < g_dirLightCount; ++lightIdx) {
							int dot_Q15;

							lightWorldX = g_directionalLights[lightIdx].worldDirX_Q15;
							lightWorldY = g_directionalLights[lightIdx].worldDirY_Q15;
							lightWorldZ = g_directionalLights[lightIdx].worldDirZ_Q15;
							dot_Q15 = g_curMatR0_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR0_Y * lightWorldY +
																  g_curMatR0_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirX = (float)dot_Q15 * flt_5A9F54;

							dot_Q15 = g_curMatR2_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR2_Y * lightWorldY +
																  g_curMatR2_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirY = (float)dot_Q15 * flt_5A9F54;

							dot_Q15 = g_curMatR1_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR1_Y * lightWorldY +
																  g_curMatR1_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirZ = (float)dot_Q15 * flt_5A9F54;
						}
					}
					FlightView_BuildPointLightsIfLarge(entry, obj);
					Damage_QueueCraftBillboards(objectIdx);
					RenderScene_DrawObjectModel(obj);
					g_objectPointLightCount = 0;

					if (savedModel != 0xffffu) {
						g_loadedModels.byObjectType[(uint16_t)obj->objectType] = savedModel;
					}
					g_cockpitViewActive = 0;
					g_drawingOwnCraft = 0;
					break;
				}
				case GENUS_PlayerProjectile:
				case GENUS_NpcProjectile:
					viewX = entry->viewX;
					viewY = entry->viewY;
					viewZ = entry->viewZ;
					g_renderFlags = RENDER_SCENE_CLIP_FORCE_FULL;
					FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, obj);
					RenderBillboard_DrawRollAlignedObjectModel(objectIdx);
					break;
				case GENUS_Debris:
					viewX = entry->viewX;
					viewY = entry->viewY;
					viewZ = entry->viewZ;
					FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, obj);
					g_renderFlags = RENDER_SCENE_CLIP_FORCE_FULL;
					SceneBillboard_QueueObjectTextured(objectIdx);
					break;
				case GENUS_Rubble:
					g_bindMeshTextures = 1;
					viewX = entry->viewX;
					viewY = entry->viewY;
					viewZ = entry->viewZ;
					g_renderFlags = RENDER_SCENE_CLIP_FORCE_FULL;
					FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, obj);
					if (g_useHardware3D && g_objRenderState[objectIdx].particleEffects != NULL) {
						Particle_AppendObjectEffectPointLights(objectIdx);
					}
					{
						int lightIdx;

						for (lightIdx = 0; lightIdx < g_dirLightCount; ++lightIdx) {
							int dot_Q15;

							lightWorldX = g_directionalLights[lightIdx].worldDirX_Q15;
							lightWorldY = g_directionalLights[lightIdx].worldDirY_Q15;
							lightWorldZ = g_directionalLights[lightIdx].worldDirZ_Q15;
							dot_Q15 = g_curMatR0_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR0_Y * lightWorldY +
																  g_curMatR0_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirX = (float)dot_Q15 * flt_5A9F54;

							dot_Q15 = g_curMatR2_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR2_Y * lightWorldY +
																  g_curMatR2_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirY = (float)dot_Q15 * flt_5A9F54;

							dot_Q15 = g_curMatR1_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR1_Y * lightWorldY +
																  g_curMatR1_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirZ = (float)dot_Q15 * flt_5A9F54;
						}
					}
					FlightView_BuildPointLightsIfLarge(entry, obj);
					RenderScene_DrawObjectModel(obj);
					g_objectPointLightCount = 0;
					g_bindMeshTextures = 0;
					break;
				case GENUS_Explosion: {
					unsigned int flareFrame;

					flareFrame = (unsigned int)g_modelTypeTable[(uint16_t)obj->objectType].frameCount - 4u -
								 (unsigned int)obj->typeSpecificByte[0];
					viewX = entry->viewX;
					viewY = entry->viewY;
					viewZ = entry->viewZ;
					g_renderFlags = entry->cullFlags;
					FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, obj);
					if (obj->objectType > OBJ_ExplosionTextureGroup2000 &&
						obj->objectType <= OBJ_ExplosionTextureGroup2006 && flareFrame <= 5u &&
						viewZ < 0x4000 &&
						FlightView_IsLensFlareSourceVisible(obj->world_x, obj->world_y, obj->world_z)) {
						LensFlare_QueueSource(flareSpriteOrColor[flareFrame]);
					}
					SceneBillboard_QueueObjectTextured(objectIdx);
					break;
				}
				case GENUS_SalvageJunk:
					FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, obj);
					g_cockpitViewActive = 0;
					g_drawingOwnCraft = 0;
					viewX = entry->viewX;
					viewY = entry->viewY;
					viewZ = entry->viewZ;
					g_renderFlags = entry->cullFlags;
					{
						int lightIdx;

						for (lightIdx = 0; lightIdx < g_dirLightCount; ++lightIdx) {
							int dot_Q15;

							lightWorldX = g_directionalLights[lightIdx].worldDirX_Q15;
							lightWorldY = g_directionalLights[lightIdx].worldDirY_Q15;
							lightWorldZ = g_directionalLights[lightIdx].worldDirZ_Q15;
							dot_Q15 = g_curMatR0_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR0_Y * lightWorldY +
																  g_curMatR0_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirX = (float)dot_Q15 * flt_5A9F54;

							dot_Q15 = g_curMatR2_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR2_Y * lightWorldY +
																  g_curMatR2_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirY = (float)dot_Q15 * flt_5A9F54;

							dot_Q15 = g_curMatR1_X;
							dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(dot_Q15 * lightWorldX +
																  g_curMatR1_Y * lightWorldY +
																  g_curMatR1_Z * lightWorldZ);
							g_directionalLights[lightIdx].localDirZ = (float)dot_Q15 * flt_5A9F54;
						}
					}
					FlightView_BuildPointLightsIfLarge(entry, obj);
					Damage_QueueCraftBillboards(objectIdx);
					RenderScene_DrawObjectModel(obj);
					g_objectPointLightCount = 0;
					g_cockpitViewActive = 0;
					g_drawingOwnCraft = 0;
					break;
				default:
					break;
			}
		} else if (objectIdx >= (uint16_t)g_localTransientSlotStart &&
				   objectIdx < (uint16_t)g_localTransientSlotEnd) {
			viewX = entry->viewX;
			viewY = entry->viewY;
			viewZ = entry->viewZ;
			g_renderFlags = entry->cullFlags;
			FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, 0, obj);
			SceneBillboard_QueueObjectTextured(objectIdx);
		} else {
			if (FlightView_IsStaticRenderGenus(obj->genusId)) {
				viewX = entry->viewX;
				viewY = entry->viewY;
				viewZ = entry->viewZ;
				g_renderFlags = entry->cullFlags;
				FVIEW_SetObjectTransform(obj->roll, obj->pitch, obj->yaw, obj->angleD, NULL);
				if (g_useHardware3D && g_objRenderState[objectIdx].particleEffects != NULL) {
					Particle_AppendObjectEffectPointLights(objectIdx);
				}
				{
					int lightIdx;

					for (lightIdx = 0; lightIdx < g_dirLightCount; ++lightIdx) {
						int dot_Q15;

						lightWorldX = g_directionalLights[lightIdx].worldDirX_Q15;
						lightWorldY = g_directionalLights[lightIdx].worldDirY_Q15;
						lightWorldZ = g_directionalLights[lightIdx].worldDirZ_Q15;
						dot_Q15 = g_curMatR0_X;
						dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(
							dot_Q15 * lightWorldX + g_curMatR0_Y * lightWorldY + g_curMatR0_Z * lightWorldZ);
						g_directionalLights[lightIdx].localDirX = (float)dot_Q15 * flt_5A9F54;

						dot_Q15 = g_curMatR2_X;
						dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(
							dot_Q15 * lightWorldX + g_curMatR2_Y * lightWorldY + g_curMatR2_Z * lightWorldZ);
						g_directionalLights[lightIdx].localDirY = (float)dot_Q15 * flt_5A9F54;

						dot_Q15 = g_curMatR1_X;
						dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(
							dot_Q15 * lightWorldX + g_curMatR1_Y * lightWorldY + g_curMatR1_Z * lightWorldZ);
						g_directionalLights[lightIdx].localDirZ = (float)dot_Q15 * flt_5A9F54;
					}
				}
				if (entry->projectedRadius > g_flightVpProjScaleX) {
					int lightIdx;

					for (lightIdx = 0; lightIdx < g_dirLightCount; ++lightIdx) {
						int dot_Q15;

						lightWorldX = g_directionalLights[lightIdx].worldDirX_Q15;
						lightWorldY = g_directionalLights[lightIdx].worldDirY_Q15;
						lightWorldZ = g_directionalLights[lightIdx].worldDirZ_Q15;
						dot_Q15 = g_curMatR0_X;
						dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(
							dot_Q15 * lightWorldX + g_curMatR0_Y * lightWorldY + g_curMatR0_Z * lightWorldZ);
						g_directionalLights[lightIdx].localDirX = (float)dot_Q15 * flt_5A9F54;

						dot_Q15 = g_curMatR2_X;
						dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(
							dot_Q15 * lightWorldX + g_curMatR2_Y * lightWorldY + g_curMatR2_Z * lightWorldZ);
						g_directionalLights[lightIdx].localDirY = (float)dot_Q15 * flt_5A9F54;

						dot_Q15 = g_curMatR1_X;
						dot_Q15 = Xwa_SaturateWrappedQ30ToQ15(
							dot_Q15 * lightWorldX + g_curMatR1_Y * lightWorldY + g_curMatR1_Z * lightWorldZ);
						g_directionalLights[lightIdx].localDirZ = (float)dot_Q15 * flt_5A9F54;
					}
					FlightLight_BuildObjectPointLights(obj);
				}
				RenderNonCraftSceneObject(objectIdx);
				g_objectPointLightCount = 0;
			}
		}
	}

	g_renderFlags = RENDER_SCENE_CLIP_FORCE_FULL;
	GlowMark_ClearPendingRequests();
	if (g_useHardware3D) {
		RenderScene_FlushGeometry();
		EngineGlow_RenderSceneGlows();
	}
	g_drawSceneEffects = 1;
	RenderScene_DrawVisibleFaces();
	g_drawSceneEffects = 0;
	if (!g_useHardware3D) {
		SceneBillboard_RenderQueuedTextured(1);
		Targeting_DrawSceneObjectBoxes();
	}

	g_flightHudUpdateElapsedTicks = 0;
	g_unusedFlightViewRenderHudWord = 0;
	g_inputTimestamp = (int)Time_GetFrameDelta() + g_inputTimestamp;
	g_flightHudUpdateElapsedTicks = g_inputTimestamp;
	if (g_useHardware3D) {
		RenderScene_FlushGeometry();
	}
	Hud_UpdateHUD();
	g_unusedFlightRenderColorByte = 0;
	{
		uint8_t colorEscape;

		g_inputTimestamp = (int)Time_GetFrameDelta() + g_inputTimestamp;
		colorEscape = g_flightColorEscapeBypassChar;
		g_flightHudUpdateElapsedTicks = g_inputTimestamp - g_flightHudUpdateElapsedTicks;
		g_unusedFlightRenderColorByte = colorEscape;
	}
	Hud_BlitSoftwareHudTextPanes();
	if (g_useHardware3D) {
		RenderScene_Initialize(1);
		FlightText_FlushQueue();
		RenderScene_DrawVisibleFaces();
	}

restore_film_camera:
	if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
		g_players[g_localPlayer].mapCameraState = savedMapCameraState;
		memcpy(&g_players[g_localPlayer].viewState, &g_savedPlayerViewStateForPlaybackCamera,
			   sizeof(g_players[g_localPlayer].viewState));
	}
	return (char)g_filmPlaybackMode;
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x4F2140
void FlightView_RenderFrame(void) {
	int shouldRender;
	int playerIdx;

	Math_SetFpuSinglePrecisionMode();
	Mission_SetActiveRegionObjectRanges(g_players[g_localPlayer].regionIndex);

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		if (g_players[playerIdx].connectedFlag == 1 && playerIdx != g_localPlayer) {
			FlightView_UpdatePlayerCamera(playerIdx);
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);

	if (g_reticleDirty) {
		Hud_SetupReticle();
		if (g_reticleDirty) {
			Hud_SetupReticle();
		}
	}

	FlightView_UpdatePlaybackCamera(g_localPlayer);
	shouldRender = 0;
	if (!g_filmPlaybackMode || (uint8_t)g_pauseState <= 2u) {
		shouldRender = 1;
	} else {
		++g_pauseState;
		if ((uint8_t)g_pauseState > 9u) {
			shouldRender = 1;
			g_pauseState = 3;
		}
		if (g_players[g_localPlayer].hyperspacePhase) {
			shouldRender = 1;
		}
	}

	if (shouldRender) {
#ifdef XWA_MODERN
		XwaSnapshotHud_BeginClassicFrame();
#endif
		FlightView_Render();
		if (g_useHardware3D) {
			RenderScene_End3D();
		}
		if (g_flightConfPowerVr && g_useHardware3D) {
			Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
			RenderScene_DrawVisibleFaces();
			RenderScene_End3D();
		}
		if (g_flightFpsOverlayMode) {
			Hud_DrawFpsOverlay();
		}
#ifdef XWA_MODERN
		XwaSnapshotHud_EndClassicFrame();
#endif
		FlightDisplay_Flip();
		if (g_useHardware3D) {
			RenderScene_ClearFrameBuffers();
		} else if (!g_flightExitRequest) {
			Hud_RenderHud();
			FlightDisplay_BlitRenderSurface();
		}
	}

	Math_SetFpuSinglePrecisionMode();
	FlightInput_Read(g_localPlayer);
	FlightInput_ScaleAxesForFlight();
}

// FUNCTION: XWA 0x4F2070
void FlightView_RenderStartupFrame(void) {
	int playerIdx;

	Mission_SetActiveRegionObjectRanges(g_players[g_localPlayer].regionIndex);
	RenderList_Reset();
#ifdef XWA_MODERN
	XwaSnapshotHud_BeginClassicFrame();
#endif

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		if (g_players[playerIdx].connectedFlag && playerIdx != g_localPlayer) {
			FlightView_UpdatePlayerCamera(playerIdx);
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		if (g_players[playerIdx].connectedFlag && playerIdx != g_localPlayer) {
			FlightView_UpdatePlayerCamera(playerIdx);
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);

	Hud_SetupReticle();
	if (g_useHardware3D) {
		RenderScene_End3D();
		FlightDisplay_Flip();
		RenderScene_ClearFrameBuffers();
	} else {
		Hud_ClearFlightSurface();
		Hud_EnableHudDrawElements();
		Hud_RenderHud();
		FlightDisplay_BlitRenderSurface();
	}
#ifdef XWA_MODERN
	XwaSnapshotHud_EndClassicFrame();
#endif
}

// FUNCTION: XWA 0x43FFB0
void FVIEW_calcrotatemove(Q16Angle angleA, Q16Angle angleB, ObjectRecord* objRecord) {
	MobileObject* mobj;
	int angleC000MinusA;
	int16_t cosNegB;
	int16_t cosC000MinusA;
	int16_t sinNegB;
	int16_t sinC000MinusA;
	int moveX;
	int moveY;
	int moveZ;

	angleC000MinusA = 0xc000 - angleA;
	cosNegB = trig2_getsignedcos(-angleB);
	cosC000MinusA = trig2_getsignedcos(angleC000MinusA);
	sinNegB = trig2_getsignedsin(-angleB);
	sinC000MinusA = trig2_getsignedsin(angleC000MinusA);

	g_curMatR0_X = cosNegB;
	g_curMatR0_Y = sinNegB;
	g_curMatR0_Z = 0;
	g_curMatR2_X = Xwa_Q15MulReuseFirstSlot(-sinNegB, cosC000MinusA);
	g_curMatR2_Y = Xwa_Q15MulReuseFirstSlot(cosNegB, cosC000MinusA);
	g_curMatR2_Z = sinC000MinusA;
	g_curMatR1_X = -Xwa_Q15MulReuseFirstSlot(sinNegB, sinC000MinusA);
	g_curMatR1_Y = -Xwa_Q15MulReuseFirstSlot(-cosNegB, sinC000MinusA);
	g_curMatR1_Z = -cosC000MinusA;

	moveX = -g_curMatR2_X;
	moveY = -g_curMatR2_Y;
	moveZ = -g_curMatR2_Z;

	g_fviewMoveX_Q15 = moveX;
	g_fviewMoveY_Q15 = moveY;
	g_fviewMoveZ_Q15 = moveZ;

	if (objRecord != NULL) {
		mobj = objRecord->mobj;
		if (mobj->state == 0 && (mobj->motionFlags & 1) != 0) {
			moveX = -moveX;
			moveY = -moveY;
			moveZ = -moveZ;

			g_fviewMoveX_Q15 = moveX;
			g_fviewMoveY_Q15 = moveY;
			g_fviewMoveZ_Q15 = moveZ;
		}

		objRecord->mobj->moveX = (int16_t)moveX;
		objRecord->mobj->moveY = (int16_t)g_fviewMoveY_Q15;
		objRecord->mobj->moveZ = (int16_t)g_fviewMoveZ_Q15;
		objRecord->mobj->moveVectorDirty = 0;
	}
}

// FUNCTION: XWA 0x440E40
void FVIEW_transformaxes(int axisX_Q15, int axisY_Q15, int axisZ_Q15, int16_t angleQ16) {
	int angle;
	int16_t cosAngle;
	int16_t sinAngle;
	int16_t oneMinusCos;
	int m00;
	int m01;
	int m02;
	int m10;
	int m11;
	int m12;
	int m20;
	int m21;
	int m22;
	int oldX;
	int oldY;
	int oldZ;
	int newX;
	int newY;
	int newZ;

	angle = angleQ16;
	if ((int16_t)angle == 0) {
		return;
	}

	cosAngle = trig2_getsignedcos(angle);
	sinAngle = trig2_getsignedsin(angle);

	if (cosAngle >= 0) {
		oneMinusCos = 0x7fff - cosAngle;
		m00 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + oneMinusCos * ((axisX_Q15 * axisX_Q15) >> 15));
		m01 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15Mul(axisZ_Q15, sinAngle) * 32768) +
										  oneMinusCos * ((axisY_Q15 * axisX_Q15) >> 15));
		m02 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15Mul(axisY_Q15, sinAngle)) +
										  oneMinusCos * ((axisZ_Q15 * axisX_Q15) >> 15));
		m10 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15Mul(axisZ_Q15, sinAngle)) +
										  oneMinusCos * Xwa_Q15Mul(axisY_Q15, axisX_Q15));
		m11 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + oneMinusCos * ((axisY_Q15 * axisY_Q15) >> 15));
		m12 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15Mul(axisX_Q15, sinAngle) * 32768) +
										  oneMinusCos * Xwa_Q15Mul(axisZ_Q15, axisY_Q15));
		m20 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15Mul(axisY_Q15, sinAngle) * 32768) +
										  oneMinusCos * Xwa_Q15Mul(axisZ_Q15, axisX_Q15));
		m21 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15Mul(axisX_Q15, sinAngle)) +
										  oneMinusCos * Xwa_Q15Mul(axisZ_Q15, axisY_Q15));
		m22 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + oneMinusCos * ((axisZ_Q15 * axisZ_Q15) >> 15));
	} else {
		oneMinusCos = -cosAngle;
		m00 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + axisX_Q15 * axisX_Q15 +
										  oneMinusCos * ((axisX_Q15 * axisX_Q15) >> 15));
		m01 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15Mul(axisZ_Q15, sinAngle) * 32768) + axisY_Q15 * axisX_Q15 +
										  oneMinusCos * Xwa_Q15Mul(axisY_Q15, axisX_Q15));
		m02 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15Mul(axisY_Q15, sinAngle)) + axisZ_Q15 * axisX_Q15 +
										  oneMinusCos * ((axisZ_Q15 * axisX_Q15) >> 15));
		m10 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15Mul(axisZ_Q15, sinAngle)) + axisY_Q15 * axisX_Q15 +
										  oneMinusCos * Xwa_Q15Mul(axisY_Q15, axisX_Q15));
		m11 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + axisY_Q15 * axisY_Q15 +
										  oneMinusCos * Xwa_Q15Mul(axisY_Q15, axisY_Q15));
		m12 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15Mul(axisX_Q15, sinAngle) * 32768) + axisZ_Q15 * axisY_Q15 +
										  oneMinusCos * Xwa_Q15Mul(axisZ_Q15, axisY_Q15));
		m20 = Xwa_SaturateWrappedQ30ToQ15((Xwa_Q15Mul(axisY_Q15, sinAngle) * 32768) + axisZ_Q15 * axisX_Q15 +
										  oneMinusCos * Xwa_Q15Mul(axisZ_Q15, axisX_Q15));
		m21 = Xwa_SaturateWrappedQ30ToQ15((-32768 * Xwa_Q15Mul(axisX_Q15, sinAngle)) + axisZ_Q15 * axisY_Q15 +
										  oneMinusCos * Xwa_Q15Mul(axisZ_Q15, axisY_Q15));
		m22 = Xwa_SaturateWrappedQ30ToQ15((cosAngle * 32768) + axisZ_Q15 * axisZ_Q15 +
										  oneMinusCos * Xwa_Q15Mul(axisZ_Q15, axisZ_Q15));
	}

	oldX = g_curMatR0_X;
	oldY = g_curMatR0_Y;
	oldZ = g_curMatR0_Z;
	newX = Xwa_WrappedMulAdd3Q15(m20, oldZ, m10, oldY, m00, oldX);
	newY = Xwa_WrappedMulAdd3Q15(m21, oldZ, m11, oldY, m01, oldX);
	newZ = Xwa_WrappedMulAdd3Q15(m22, oldZ, m12, oldY, m02, oldX);
	g_curMatR0_X = newX;
	g_curMatR0_Y = newY;
	g_curMatR0_Z = newZ;

	oldX = g_curMatR1_X;
	oldY = g_curMatR1_Y;
	oldZ = g_curMatR1_Z;
	newX = Xwa_WrappedMulAdd3Q15(m20, oldZ, m10, oldY, m00, oldX);
	newY = Xwa_WrappedMulAdd3Q15(m21, oldZ, m11, oldY, m01, oldX);
	newZ = Xwa_WrappedMulAdd3Q15(m22, oldZ, m12, oldY, m02, oldX);
	g_curMatR1_X = newX;
	g_curMatR1_Y = newY;
	g_curMatR1_Z = newZ;

	oldX = g_curMatR2_X;
	oldY = g_curMatR2_Y;
	oldZ = g_curMatR2_Z;
	newX = Xwa_WrappedMulAdd3Q15(m20, oldZ, m10, oldY, m00, oldX);
	newY = Xwa_WrappedMulAdd3Q15(m21, oldZ, m11, oldY, m01, oldX);
	newZ = Xwa_WrappedMulAdd3Q15(m22, oldZ, m12, oldY, m02, oldX);
	g_curMatR2_X = newX;
	g_curMatR2_Y = newY;
	g_curMatR2_Z = newZ;
}

// FUNCTION: XWA 0x4A1850
void FlightView_RotateFilmOverlayFreeCameraByInput(int pitchDeltaQ16, int negYawDeltaQ16, int playerIdx) {
	Q16Angle pitchQ16;
	int yawRawQ16;
	int yawNegQ16;
	int16_t cosYaw;
	int16_t sinYaw;
	int16_t cosPitch;
	int16_t sinPitch;
	int16_t cosPitchSinYaw;
	int16_t sinPitchSinYaw;
	int16_t negSinYaw;
	int16_t cosPitchCosYaw;
	int16_t sinPitchCosYaw;
	int16_t negSinPitch;
	int zeroCoeff;
	int oldX;
	int oldY;
	int oldZ;
	int newX;
	int newY;
	int newZ;

	(void)playerIdx;

	FVIEW_BuildCameraOrientNoTurret(g_filmOverlayViewState.viewRoll,
									(int16_t)g_filmOverlayViewState.viewPitch,
									(int16_t)g_filmOverlayViewState.viewYaw, 0, 0, 0, NULL, -1);

	g_curMatR2_X = -g_fviewFwdX_Q15;
	g_curMatR2_Y = -g_fviewFwdY_Q15;
	g_curMatR2_Z = -g_fviewFwdZ_Q15;
	g_curMatR1_X = g_fviewUpX_Q15;
	g_curMatR1_Y = g_fviewUpY_Q15;
	g_curMatR1_Z = g_fviewUpZ_Q15;
	g_curMatR0_X = g_fviewSideX_Q15;
	g_curMatR0_Y = g_fviewSideY_Q15;
	g_curMatR0_Z = g_fviewSideZ_Q15;

	FVIEW_transformaxes(g_fviewSideX_Q15, g_fviewSideY_Q15, g_fviewSideZ_Q15, pitchDeltaQ16);
	FVIEW_transformaxes(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, negYawDeltaQ16);

	pitchQ16 = trig2_w_arcsin(-g_curMatR2_Z);
	yawRawQ16 = trig2_arctan(g_curMatR2_X, -g_curMatR2_Y);
	yawNegQ16 = -yawRawQ16;

	g_filmOverlayViewState.viewYaw = (uint16_t)(int16_t)yawNegQ16;
	g_filmOverlayViewState.viewPitch = pitchQ16;

	cosYaw = trig2_getsignedcos((int16_t)yawNegQ16);
	sinYaw = trig2_getsignedsin((int16_t)yawNegQ16);
	cosPitch = trig2_getsignedcos((int16_t)pitchQ16);
	sinPitch = trig2_getsignedsin((int16_t)pitchQ16);
	cosPitchSinYaw = (int16_t)Xwa_Q15Mul(cosPitch, sinYaw);
	sinPitchSinYaw = (int16_t)Xwa_Q15Mul(sinPitch, sinYaw);
	negSinYaw = (int16_t)-sinYaw;
	cosPitchCosYaw = (int16_t)Xwa_Q15Mul(cosPitch, cosYaw);
	sinPitchCosYaw = (int16_t)Xwa_Q15Mul(sinPitch, cosYaw);
	negSinPitch = (int16_t)-sinPitch;
	zeroCoeff = (int16_t)((uint16_t)cosPitch - (uint16_t)cosPitch);

	oldX = g_curMatR0_X;
	oldY = g_curMatR0_Y;
	oldZ = g_curMatR0_Z;
	newX = Xwa_WrappedMulAdd3Q15(cosYaw, oldX, negSinYaw, oldY, zeroCoeff, oldZ);
	newY = Xwa_WrappedMulAdd3Q15(cosPitchSinYaw, oldX, cosPitchCosYaw, oldY, negSinPitch, oldZ);
	newZ = Xwa_WrappedMulAdd3Q15(sinPitchSinYaw, oldX, sinPitchCosYaw, oldY, cosPitch, oldZ);
	g_curMatR0_X = (int16_t)newX;
	g_curMatR0_Y = (int16_t)newY;
	g_curMatR0_Z = (int16_t)newZ;

	oldX = g_curMatR1_X;
	oldY = g_curMatR1_Y;
	oldZ = g_curMatR1_Z;
	newX = Xwa_WrappedMulAdd3Q15(cosYaw, oldX, negSinYaw, oldY, zeroCoeff, oldZ);
	newY = Xwa_WrappedMulAdd3Q15(cosPitchSinYaw, oldX, cosPitchCosYaw, oldY, negSinPitch, oldZ);
	newZ = Xwa_WrappedMulAdd3Q15(sinPitchSinYaw, oldX, sinPitchCosYaw, oldY, cosPitch, oldZ);
	g_curMatR1_X = (int16_t)newX;
	g_curMatR1_Y = (int16_t)newY;
	g_curMatR1_Z = (int16_t)newZ;

	oldX = g_curMatR2_X;
	oldY = g_curMatR2_Y;
	oldZ = g_curMatR2_Z;
	newX = Xwa_WrappedMulAdd3Q15(cosYaw, oldX, negSinYaw, oldY, zeroCoeff, oldZ);
	newY = Xwa_WrappedMulAdd3Q15(cosPitchSinYaw, oldX, cosPitchCosYaw, oldY, negSinPitch, oldZ);
	newZ = Xwa_WrappedMulAdd3Q15(sinPitchSinYaw, oldX, sinPitchCosYaw, oldY, cosPitch, oldZ);
	g_curMatR2_X = (int16_t)newX;
	g_curMatR2_Y = (int16_t)newY;
	g_curMatR2_Z = (int16_t)newZ;

	g_filmOverlayViewState.viewRoll = (uint16_t)(int16_t)-(int16_t)trig2_arctan(g_curMatR0_Y, g_curMatR0_X);
}

#ifdef XWA_MODERN
static int g_gimbalLockFixEnabled = 1;

int FlightDebug_GimbalLockFixEnabled(void) { return g_gimbalLockFixEnabled; }

void FlightDebug_SetGimbalLockFixEnabled(int enabled) {
	enabled = enabled != 0;
	if (enabled == g_gimbalLockFixEnabled) {
		return;
	}
	g_gimbalLockFixEnabled = enabled;
	Aeron_LogInfo("xwa.input.orientation", "ORIENTFIX state=%s", enabled ? "on" : "off");
}
#endif

// FUNCTION: XWA 0x5042F0
void USER_calcdeltapitch(int16_t pitchQ16, int16_t negYawQ16, uint16_t shipObjIdx) {
#ifdef XWA_MODERN
	if (g_gimbalLockFixEnabled) {
		XwaOrientationAngles current;
		XwaOrientationAngles updated;

		current.yaw = g_objectTable[shipObjIdx].yaw;
		current.pitch = g_objectTable[shipObjIdx].pitch;
		current.roll = g_objectTable[shipObjIdx].roll;
		updated = XwaOrientation_ApplyPitchYaw(current, pitchQ16, negYawQ16);

		g_objectTable[shipObjIdx].yaw = updated.yaw;
		g_objectTable[shipObjIdx].pitch = updated.pitch;
		g_objectTable[shipObjIdx].roll = updated.roll;
		g_objectTable[shipObjIdx].angleD = 0;
		return;
	}
#endif
	int yawRawQ16;
	int yawNegQ16;
	int newPitchQ16;
	int16_t cosYaw;
	int16_t sinYaw;
	int16_t cosPitch;
	int16_t sinPitch;
	int16_t cosPitchSinYaw;
	int16_t sinPitchSinYaw;
	int16_t negSinYaw;
	int16_t cosPitchCosYaw;
	int16_t sinPitchCosYaw;
	int16_t negSinPitch;
	int oldX;
	int oldY;
	int oldZ;
	Vec3i newRow;
	int rollQ16;

	if (g_objectTable[shipObjIdx].mobj->orientMatrixDirty) {
		FVIEW_calcrotatemove(g_objectTable[shipObjIdx].pitch, g_objectTable[shipObjIdx].yaw,
							 &g_objectTable[shipObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[shipObjIdx].roll, g_objectTable[shipObjIdx].angleD,
							   &g_objectTable[shipObjIdx]);
	}

	g_curMatR2_X = -g_objectTable[shipObjIdx].mobj->cachedFwdX;
	g_curMatR2_Y = -g_objectTable[shipObjIdx].mobj->cachedFwdY;
	g_curMatR2_Z = -g_objectTable[shipObjIdx].mobj->cachedFwdZ;
	g_curMatR1_X = g_objectTable[shipObjIdx].mobj->cachedUpX;
	g_curMatR1_Y = g_objectTable[shipObjIdx].mobj->cachedUpY;
	g_curMatR1_Z = g_objectTable[shipObjIdx].mobj->cachedUpZ;
	g_curMatR0_X = g_objectTable[shipObjIdx].mobj->cachedSideX;
	g_curMatR0_Y = g_objectTable[shipObjIdx].mobj->cachedSideY;
	g_curMatR0_Z = g_objectTable[shipObjIdx].mobj->cachedSideZ;

	FVIEW_transformaxes(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, pitchQ16);
	FVIEW_transformaxes(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, negYawQ16);

	newPitchQ16 = trig2_w_arcsin(-g_curMatR2_Z);
	g_objectTable[shipObjIdx].pitch = (uint16_t)newPitchQ16;
	yawRawQ16 = trig2_arctan(g_curMatR2_X, -g_curMatR2_Y);
	yawNegQ16 = -yawRawQ16;

	cosYaw = trig2_getsignedcos((int16_t)yawNegQ16);
	sinYaw = trig2_getsignedsin((int16_t)yawNegQ16);
	cosPitch = trig2_getsignedcos((int16_t)newPitchQ16);
	sinPitch = trig2_getsignedsin((int16_t)newPitchQ16);
	cosPitchSinYaw = (int16_t)Xwa_Q15Mul(cosPitch, sinYaw);
	sinPitchSinYaw = (int16_t)Xwa_Q15Mul(sinPitch, sinYaw);
	negSinYaw = (int16_t)-sinYaw;
	cosPitchCosYaw = (int16_t)Xwa_Q15Mul(cosPitch, cosYaw);
	sinPitchCosYaw = (int16_t)Xwa_Q15Mul(sinPitch, cosYaw);
	negSinPitch = (int16_t)-sinPitch;

	oldX = g_curMatR0_X;
	oldY = g_curMatR0_Y;
	oldZ = g_curMatR0_Z;
	newRow.x = Xwa_WrappedMulAdd3Q15(cosYaw, oldX, negSinYaw, oldY, 0, oldZ);
	newRow.y = Xwa_WrappedMulAdd3Q15(cosPitchSinYaw, oldX, cosPitchCosYaw, oldY, negSinPitch, oldZ);
	newRow.z = Xwa_WrappedMulAdd3Q15(sinPitchSinYaw, oldX, sinPitchCosYaw, oldY, cosPitch, oldZ);
	g_curMatR0_X = (int16_t)newRow.x;
	g_curMatR0_Y = (int16_t)newRow.y;
	g_curMatR0_Z = (int16_t)newRow.z;

	oldX = g_curMatR1_X;
	oldY = g_curMatR1_Y;
	oldZ = g_curMatR1_Z;
	newRow.x = Xwa_WrappedMulAdd3Q15(cosYaw, oldX, negSinYaw, oldY, 0, oldZ);
	newRow.y = Xwa_WrappedMulAdd3Q15(cosPitchSinYaw, oldX, cosPitchCosYaw, oldY, negSinPitch, oldZ);
	newRow.z = Xwa_WrappedMulAdd3Q15(sinPitchSinYaw, oldX, sinPitchCosYaw, oldY, cosPitch, oldZ);
	g_curMatR1_X = (int16_t)newRow.x;
	g_curMatR1_Y = (int16_t)newRow.y;
	g_curMatR1_Z = (int16_t)newRow.z;

	oldX = g_curMatR2_X;
	oldY = g_curMatR2_Y;
	oldZ = g_curMatR2_Z;
	newRow.x = Xwa_WrappedMulAdd3Q15(cosYaw, oldX, negSinYaw, oldY, 0, oldZ);
	newRow.y = Xwa_WrappedMulAdd3Q15(cosPitchSinYaw, oldX, cosPitchCosYaw, oldY, negSinPitch, oldZ);
	newRow.z = Xwa_WrappedMulAdd3Q15(sinPitchSinYaw, oldX, sinPitchCosYaw, oldY, cosPitch, oldZ);
	g_curMatR2_X = (int16_t)newRow.x;
	g_curMatR2_Y = (int16_t)newRow.y;
	g_curMatR2_Z = (int16_t)newRow.z;

	rollQ16 = -trig2_arctan(g_curMatR0_Y, g_curMatR0_X);
	g_objectTable[shipObjIdx].yaw = (uint16_t)yawNegQ16;
	g_objectTable[shipObjIdx].roll = (uint16_t)rollQ16;
}

static __inline void FlightView_RecomposeViewRow(int oldX, int oldY, int oldZ, int coeffX0, int coeffY0,
												 int coeffZ0, int coeffX1, int coeffY1, int coeffZ1,
												 int coeffX2, int coeffY2, int coeffZ2, int* newX, int* newY,
												 int* newZ) {
	*newX = Xwa_SaturateWrappedQ30ToQ15(
		(int32_t)((uint32_t)(coeffX0 * oldX) + (uint32_t)(coeffY0 * oldY) + (uint32_t)(coeffZ0 * oldZ)));
	*newY = Xwa_SaturateWrappedQ30ToQ15(
		(int32_t)((uint32_t)(coeffX1 * oldX) + (uint32_t)(coeffY1 * oldY) + (uint32_t)(coeffZ1 * oldZ)));
	*newZ = Xwa_SaturateWrappedQ30ToQ15(
		(int32_t)((uint32_t)(coeffX2 * oldX) + (uint32_t)(coeffY2 * oldY) + (uint32_t)(coeffZ2 * oldZ)));
}

// FUNCTION: XWA 0x4A12E0
void FlightView_RotateViewByInput(int pitchDeltaQ16, int negYawDeltaQ16, int playerIdx) {
	int pitchQ16;
	int yawRawQ16;
	int yawNegQ16;
	int cosYaw;
	int sinYaw;
	int cosPitch;
	int sinPitch;
	int cosPitchSinYaw;
	int sinPitchSinYaw;
	int negSinYaw;
	int cosPitchCosYaw;
	int sinPitchCosYaw;
	int negSinPitch;
	int zeroCoeff;
	int newX;
	int newY;
	int newZ;
	int rollQ16;

	FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
							(int16_t)g_players[playerIdx].viewState.viewPitch,
							(int16_t)g_players[playerIdx].viewState.viewYaw, 0, 0, 0, NULL, -1);

	g_curMatR2_X = -g_fviewFwdX_Q15;
	g_curMatR2_Y = -g_fviewFwdY_Q15;
	g_curMatR2_Z = -g_fviewFwdZ_Q15;
	g_curMatR1_X = g_fviewUpX_Q15;
	g_curMatR1_Y = g_fviewUpY_Q15;
	g_curMatR1_Z = g_fviewUpZ_Q15;
	g_curMatR0_X = g_fviewSideX_Q15;
	g_curMatR0_Y = g_fviewSideY_Q15;
	g_curMatR0_Z = g_fviewSideZ_Q15;

	FVIEW_transformaxes(g_fviewSideX_Q15, g_fviewSideY_Q15, g_fviewSideZ_Q15, pitchDeltaQ16);
	if ((g_flightKeyMods & 0xEu) != 2u) {
		FVIEW_transformaxes(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, negYawDeltaQ16);
	}

	pitchQ16 = trig2_w_arcsin(-g_curMatR2_Z);
	yawRawQ16 = trig2_arctan(g_curMatR2_X, -g_curMatR2_Y);
	yawNegQ16 = -yawRawQ16;

	g_players[playerIdx].viewState.viewYaw = (uint16_t)yawNegQ16;
	g_players[playerIdx].viewState.viewPitch = pitchQ16;

	cosYaw = trig2_getsignedcos(yawNegQ16);
	sinYaw = trig2_getsignedsin(yawNegQ16);
	cosPitch = trig2_getsignedcos(pitchQ16);
	sinPitch = trig2_getsignedsin(pitchQ16);
	cosPitchSinYaw = Xwa_Q15Mul((int16_t)cosPitch, (int16_t)sinYaw);
	sinPitchSinYaw = Xwa_Q15Mul((int16_t)sinPitch, (int16_t)sinYaw);
	negSinYaw = -sinYaw;
	cosPitchCosYaw = Xwa_Q15Mul((int16_t)cosPitch, (int16_t)cosYaw);
	sinPitchCosYaw = Xwa_Q15Mul((int16_t)sinPitch, (int16_t)cosYaw);
	negSinPitch = -sinPitch;
	zeroCoeff = (int16_t)((uint16_t)cosPitch - (uint16_t)cosPitch);

	FlightView_RecomposeViewRow(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, (int16_t)cosYaw, (int16_t)negSinYaw,
								zeroCoeff, (int16_t)cosPitchSinYaw, (int16_t)cosPitchCosYaw,
								(int16_t)negSinPitch, (int16_t)sinPitchSinYaw, (int16_t)sinPitchCosYaw,
								(int16_t)cosPitch, &newX, &newY, &newZ);
	g_curMatR0_X = (int16_t)newX;
	g_curMatR0_Y = (int16_t)newY;
	g_curMatR0_Z = (int16_t)newZ;

	FlightView_RecomposeViewRow(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, (int16_t)cosYaw, (int16_t)negSinYaw,
								zeroCoeff, (int16_t)cosPitchSinYaw, (int16_t)cosPitchCosYaw,
								(int16_t)negSinPitch, (int16_t)sinPitchSinYaw, (int16_t)sinPitchCosYaw,
								(int16_t)cosPitch, &newX, &newY, &newZ);
	g_curMatR1_X = (int16_t)newX;
	g_curMatR1_Y = (int16_t)newY;
	g_curMatR1_Z = (int16_t)newZ;

	FlightView_RecomposeViewRow(g_curMatR2_X, g_curMatR2_Y, g_curMatR2_Z, (int16_t)cosYaw, (int16_t)negSinYaw,
								zeroCoeff, (int16_t)cosPitchSinYaw, (int16_t)cosPitchCosYaw,
								(int16_t)negSinPitch, (int16_t)sinPitchSinYaw, (int16_t)sinPitchCosYaw,
								(int16_t)cosPitch, &newX, &newY, &newZ);
	g_curMatR2_X = (int16_t)newX;
	g_curMatR2_Y = (int16_t)newY;
	g_curMatR2_Z = (int16_t)newZ;

	rollQ16 = -trig2_arctan(g_curMatR0_Y, g_curMatR0_X);
	g_players[playerIdx].viewState.viewRoll = (uint16_t)(int16_t)rollQ16;
	if ((g_flightKeyMods & 0xEu) == 2u) {
		g_players[playerIdx].viewState.viewRoll = (uint16_t)(int16_t)(rollQ16 + negYawDeltaQ16);
	}
}

// FUNCTION: XWA 0x440140
void FVIEW_calcrotateorient(Q16Angle angleC, Q16Angle angleD, ObjectRecord* objRecord) {
	MobileObject* mobj;
	int16_t spinAngleQ16;

	FVIEW_transformaxes(g_curMatR1_X, g_curMatR1_Y, g_curMatR1_Z, (int16_t)angleD);
	FVIEW_transformaxes(g_curMatR2_X, g_curMatR2_Y, g_curMatR2_Z, (int16_t)angleC);

	if (objRecord != NULL) {
		mobj = objRecord->mobj;
		if (mobj != NULL) {
			spinAngleQ16 = mobj->spinAngleQ16;
			if (spinAngleQ16 != 0) {
				FVIEW_transformaxes((int)(mobj->spinAxisX * 32767.0f), (int)(mobj->spinAxisY * 32767.0f),
									(int)(mobj->spinAxisZ * 32767.0f), spinAngleQ16);
			}
		}
	}

	g_fviewFwdY_Q15 = -g_curMatR2_Y;
	g_fviewFwdZ_Q15 = -g_curMatR2_Z;
	g_fviewSideX_Q15 = g_curMatR0_X;
	g_fviewSideY_Q15 = g_curMatR0_Y;
	g_fviewSideZ_Q15 = g_curMatR0_Z;
	g_fviewUpX_Q15 = g_curMatR1_X;
	g_fviewFwdX_Q15 = -g_curMatR2_X;
	g_fviewUpY_Q15 = g_curMatR1_Y;
	g_fviewUpZ_Q15 = g_curMatR1_Z;

	if (objRecord != NULL) {
		objRecord->mobj->cachedFwdX = (int16_t)-g_curMatR2_X;
		objRecord->mobj->cachedFwdY = (int16_t)g_fviewFwdY_Q15;
		objRecord->mobj->cachedFwdZ = (int16_t)g_fviewFwdZ_Q15;
		objRecord->mobj->cachedSideX = (int16_t)g_fviewSideX_Q15;
		objRecord->mobj->cachedSideY = (int16_t)g_fviewSideY_Q15;
		objRecord->mobj->cachedSideZ = (int16_t)g_fviewSideZ_Q15;
		objRecord->mobj->cachedUpX = (int16_t)g_fviewUpX_Q15;
		objRecord->mobj->cachedUpY = (int16_t)g_fviewUpY_Q15;
		objRecord->mobj->cachedUpZ = (int16_t)g_fviewUpZ_Q15;
		objRecord->mobj->orientMatrixDirty = 0;
	}
}

// FUNCTION: XWA 0x4A2DD0
int pai_calcrotatedpoint(ObjectRecord* objRecord, int16_t sideArg, int16_t upArg, int16_t fwdArg) {
	MobileObject* mobj;

	mobj = objRecord->mobj;
	if (mobj->orientMatrixDirty) {
		FVIEW_calcrotatemove(objRecord->pitch, objRecord->yaw, objRecord);
		FVIEW_calcrotateorient(objRecord->roll, objRecord->angleD, objRecord);
	}

	g_rotatedX = Xwa_Q15Mul(mobj->cachedSideX, sideArg);
	g_rotatedX += Xwa_Q15Mul(mobj->cachedUpX, upArg);
	g_rotatedX += Xwa_Q15Mul(mobj->cachedFwdX, fwdArg);
	g_rotatedY = Xwa_Q15Mul(mobj->cachedSideY, sideArg);
	g_rotatedY += Xwa_Q15Mul(mobj->cachedUpY, upArg);
	g_rotatedY += Xwa_Q15Mul(mobj->cachedFwdY, fwdArg);
	g_rotatedZ = Xwa_Q15Mul(mobj->cachedSideZ, sideArg);
	g_rotatedZ += Xwa_Q15Mul(mobj->cachedUpZ, upArg);
	g_rotatedZ += Xwa_Q15Mul(mobj->cachedFwdZ, fwdArg);
	return g_rotatedZ;
}

// FUNCTION: XWA 0x4A2FB0
int pai_RotateLocalVectorToWorldScratch(ObjectRecord* objRecord, int localSide, int localUp, int localFwd) {
	MobileObject* mobj;

	mobj = objRecord->mobj;
	if (mobj->orientMatrixDirty) {
		FVIEW_calcrotatemove(objRecord->pitch, objRecord->yaw, objRecord);
		FVIEW_calcrotateorient(objRecord->roll, objRecord->angleD, objRecord);
	}

	g_rotatedX = Xwa_Q15Mul(mobj->cachedSideX, localSide);
	g_rotatedX += Xwa_Q15Mul(mobj->cachedUpX, localUp);
	g_rotatedX += Xwa_Q15Mul(mobj->cachedFwdX, localFwd);
	g_rotatedY = Xwa_Q15Mul(mobj->cachedSideY, localSide);
	g_rotatedY += Xwa_Q15Mul(mobj->cachedUpY, localUp);
	g_rotatedY += Xwa_Q15Mul(mobj->cachedFwdY, localFwd);
	g_rotatedZ = Xwa_Q15Mul(mobj->cachedSideZ, localSide);
	g_rotatedZ += Xwa_Q15Mul(mobj->cachedUpZ, localUp);
	g_rotatedZ += Xwa_Q15Mul(mobj->cachedFwdZ, localFwd);
	return g_rotatedZ;
}

// FUNCTION: XWA 0x4A3190
void pai_RotateLocalVectorToWorldScratchMaybeStatic(ObjectRecord* objRecord, int localSide, int localUp,
													int localFwd) {
	if (objRecord->mobj) {
		if (objRecord->mobj->orientMatrixDirty) {
			FVIEW_calcrotatemove(objRecord->pitch, objRecord->yaw, objRecord);
			FVIEW_calcrotateorient(objRecord->roll, objRecord->angleD, objRecord);
		}

		g_rotatedX = Xwa_Q15MulReuseFirstSlot(localSide, objRecord->mobj->cachedSideX);
		g_rotatedX += Xwa_Q15MulReuseFirstSlot(localUp, objRecord->mobj->cachedUpX);
		g_rotatedX += Xwa_Q15MulReuseFirstSlot(localFwd, objRecord->mobj->cachedFwdX);
		g_rotatedY = Xwa_Q15MulReuseFirstSlot(localSide, objRecord->mobj->cachedSideY);
		g_rotatedY += Xwa_Q15MulReuseFirstSlot(localUp, objRecord->mobj->cachedUpY);
		g_rotatedY += Xwa_Q15MulReuseFirstSlot(localFwd, objRecord->mobj->cachedFwdY);
		g_rotatedZ = Xwa_Q15MulReuseFirstSlot(localSide, objRecord->mobj->cachedSideZ);
		g_rotatedZ += Xwa_Q15MulReuseFirstSlot(localUp, objRecord->mobj->cachedUpZ);
		g_rotatedZ += Xwa_Q15MulReuseFirstSlot(localFwd, objRecord->mobj->cachedFwdZ);
	} else {
		FVIEW_calcrotatemove(objRecord->pitch, objRecord->yaw, NULL);
		FVIEW_calcrotateorient(objRecord->roll, objRecord->angleD, NULL);

		g_rotatedX = Xwa_Q15MulReuseFirstSlot(localSide, g_fviewSideX_Q15);
		g_rotatedX += Xwa_Q15MulReuseFirstSlot(localUp, g_fviewUpX_Q15);
		g_rotatedX += Xwa_Q15MulReuseFirstSlot(localFwd, g_fviewFwdX_Q15);
		g_rotatedY = Xwa_Q15MulReuseFirstSlot(localSide, g_fviewSideY_Q15);
		g_rotatedY += Xwa_Q15MulReuseFirstSlot(localUp, g_fviewUpY_Q15);
		g_rotatedY += Xwa_Q15MulReuseFirstSlot(localFwd, g_fviewFwdY_Q15);
		g_rotatedZ = Xwa_Q15MulReuseFirstSlot(localSide, g_fviewSideZ_Q15);
		g_rotatedZ += Xwa_Q15MulReuseFirstSlot(localUp, g_fviewUpZ_Q15);
		g_rotatedZ += Xwa_Q15MulReuseFirstSlot(localFwd, g_fviewFwdZ_Q15);
	}
}

// FUNCTION: XWA 0x4A34F0
void pai_RotateVectorByExplicitAnglesScratch(int localSide, int localUp, int localFwd, Q16Angle yaw,
											 Q16Angle pitch, Q16Angle roll, Q16Angle angleD) {
	FVIEW_calcrotatemove((int16_t)pitch, (int16_t)yaw, NULL);
	FVIEW_calcrotateorient(roll, angleD, NULL);

	g_rotatedX = Xwa_Q15Mul(g_fviewSideX_Q15, localSide) + Xwa_Q15Mul(g_fviewUpX_Q15, localUp) +
				 Xwa_Q15Mul(g_fviewFwdX_Q15, localFwd);
	g_rotatedY = Xwa_Q15Mul(g_fviewSideY_Q15, localSide) + Xwa_Q15Mul(g_fviewUpY_Q15, localUp) +
				 Xwa_Q15Mul(g_fviewFwdY_Q15, localFwd);
	g_rotatedZ = Xwa_Q15Mul(g_fviewSideZ_Q15, localSide) + Xwa_Q15Mul(g_fviewUpZ_Q15, localUp) +
				 Xwa_Q15Mul(g_fviewFwdZ_Q15, localFwd);
}

// FUNCTION: XWA 0x43F8E0
int FVIEW_BuildCameraOrient(int16_t rollQ16, int16_t pitchQ16, int16_t yawQ16, int angle25Q16,
							int16_t extraPitchQ16, int16_t extraYawQ16, ObjectRecord* objRecord,
							int playerIdx) {
	CraftData* craft;
	MobileObject* playerMobj;
	ModelIndex modelIndex;
	int cameraAxisX;
	int cameraAxisY;
	int cameraAxisZ;
	int savedAxisX;
	int savedAxisY;
	int savedAxisZ;
	int seatIdx;
	int16_t mountAngleA;
	int16_t mountAngleB;
	int16_t aimAngleA;
	int16_t aimAngleB;
	int16_t turretMatR2Y;
	int16_t turretMatR2Z;
	int16_t turretMatR0X;
	int16_t turretMatR0Y;
	int16_t turretMatR0Z;
	int16_t turretMatR1X;
	int16_t turretMatR1Y;
	int16_t turretMatR1Z;
	int playerObjectIdx;
	float viewMtx00;
	float viewMtx01;
	float viewMtx02;
	float viewMtx10;
	float viewMtx11;
	float viewMtx12;
#ifdef XWA_MODERN
	double camShadow[9];
	double shadowSavedAxis[3];
	double shadowCamAxis[3];
#endif

	FVIEW_calcrotatemove(pitchQ16, yawQ16, objRecord);
	FVIEW_calcrotateorient((Q16Angle)rollQ16, (Q16Angle)angle25Q16, objRecord);
#ifdef XWA_MODERN
	FVIEW_ShadowRotMove(camShadow, pitchQ16, yawQ16);
	FVIEW_ShadowRotOrient(camShadow, (Q16Angle)rollQ16, (Q16Angle)angle25Q16, objRecord);
#endif

	g_curMatR2_X = -g_curMatR2_X;
	g_curMatR2_Y = -g_curMatR2_Y;
	g_curMatR2_Z = -g_curMatR2_Z;
	g_curMatR1_X = -g_curMatR1_X;
	g_curMatR1_Y = -g_curMatR1_Y;
	g_curMatR1_Z = -g_curMatR1_Z;
#ifdef XWA_MODERN
	camShadow[3] = -camShadow[3];
	camShadow[4] = -camShadow[4];
	camShadow[5] = -camShadow[5];
	camShadow[6] = -camShadow[6];
	camShadow[7] = -camShadow[7];
	camShadow[8] = -camShadow[8];
#endif

	if (playerIdx >= 0 && g_players[playerIdx].currentSeatIdx != 0 &&
		g_players[playerIdx].mapCameraState == 0) {
		playerObjectIdx = g_players[playerIdx].objectIndex;
		modelIndex = GetModelIndexFromType(g_objectTable[playerObjectIdx].objectType);

		if (modelIndex != 0xffffu) {
			savedAxisX = g_curMatR1_X;
			savedAxisY = g_curMatR1_Y;
			playerMobj = g_objectTable[g_players[playerIdx].objectIndex].mobj;
			craft = playerMobj->pCraft;
			savedAxisZ = g_curMatR1_Z;

			mountAngleB = g_modelDefs[modelIndex].turretMountAngleB[g_players[playerIdx].currentSeatIdx - 1];
			mountAngleA = g_modelDefs[modelIndex].turretMountAngleA[g_players[playerIdx].currentSeatIdx - 1];
#ifdef XWA_MODERN
			shadowSavedAxis[0] = camShadow[3];
			shadowSavedAxis[1] = camShadow[4];
			shadowSavedAxis[2] = camShadow[5];
			FVIEW_ShadowRotateAxes(camShadow, camShadow[0], camShadow[1], camShadow[2], mountAngleA);
			FVIEW_ShadowRotateAxes(camShadow, shadowSavedAxis[0], shadowSavedAxis[1], shadowSavedAxis[2],
								   mountAngleB);
#endif
			FVIEW_transformaxes(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, mountAngleA);
			FVIEW_transformaxes(savedAxisX, savedAxisY, savedAxisZ, mountAngleB);

			seatIdx = g_players[playerIdx].currentSeatIdx;
			aimAngleA = craft->turretAim.aimAngleA[seatIdx - 1];
			savedAxisX = g_curMatR1_X;
			savedAxisY = g_curMatR1_Y;
			savedAxisZ = g_curMatR1_Z;
#ifdef XWA_MODERN
			shadowSavedAxis[0] = camShadow[3];
			shadowSavedAxis[1] = camShadow[4];
			shadowSavedAxis[2] = camShadow[5];
			FVIEW_ShadowRotateAxes(camShadow, camShadow[0], camShadow[1], camShadow[2], aimAngleA);
#endif
			FVIEW_transformaxes(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, aimAngleA);
			aimAngleB = craft->turretAim.aimAngleB[seatIdx - 1];
#ifdef XWA_MODERN
			FVIEW_ShadowRotateAxes(camShadow, shadowSavedAxis[0], shadowSavedAxis[1], shadowSavedAxis[2],
								   aimAngleB);
#endif
			FVIEW_transformaxes(savedAxisX, savedAxisY, savedAxisZ, aimAngleB);

			turretMatR2Y = (int16_t)g_curMatR2_Y;
			turretMatR2Z = (int16_t)g_curMatR2_Z;
			g_players[playerIdx].turretCamMat[0] = (int16_t)g_curMatR2_X;
			turretMatR0X = (int16_t)g_curMatR0_X;
			g_players[playerIdx].turretCamMat[1] = turretMatR2Y;
			turretMatR0Y = (int16_t)g_curMatR0_Y;
			g_players[playerIdx].turretCamMat[2] = turretMatR2Z;
			turretMatR0Z = (int16_t)g_curMatR0_Z;
			g_players[playerIdx].turretCamMat[3] = turretMatR0X;
			turretMatR1X = (int16_t)g_curMatR1_X;
			g_players[playerIdx].turretCamMat[4] = turretMatR0Y;
			turretMatR1Y = (int16_t)g_curMatR1_Y;
			g_players[playerIdx].turretCamMat[5] = turretMatR0Z;
			turretMatR1Z = (int16_t)g_curMatR1_Z;
			g_players[playerIdx].turretCamMat[6] = turretMatR1X;
			g_players[playerIdx].turretCamMat[7] = turretMatR1Y;
			g_players[playerIdx].turretCamMat[8] = turretMatR1Z;
		}
	}

	cameraAxisX = g_curMatR1_X;
	cameraAxisY = g_curMatR1_Y;
	cameraAxisZ = g_curMatR1_Z;
#ifdef XWA_MODERN
	shadowCamAxis[0] = camShadow[3];
	shadowCamAxis[1] = camShadow[4];
	shadowCamAxis[2] = camShadow[5];
	FVIEW_ShadowRotateAxes(camShadow, camShadow[0], camShadow[1], camShadow[2], extraPitchQ16);
	FVIEW_ShadowRotateAxes(camShadow, shadowCamAxis[0], shadowCamAxis[1], shadowCamAxis[2], extraYawQ16);
#endif
	FVIEW_transformaxes(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, extraPitchQ16);
	FVIEW_transformaxes(cameraAxisX, cameraAxisY, cameraAxisZ, extraYawQ16);

	viewMtx00 = (float)g_curMatR0_X;
	viewMtx01 = (float)g_curMatR0_Y;
	viewMtx02 = (float)g_curMatR0_Z;
	viewMtx10 = (float)g_curMatR1_X;
	viewMtx11 = (float)g_curMatR1_Y;
	viewMtx12 = (float)g_curMatR1_Z;

	g_camMatR0_X = g_curMatR0_X;
	g_camMatR0_Y = g_curMatR0_Y;
	g_viewMtx00 = viewMtx00 * flt_5A94B8;
	g_camMatR0_Z = g_curMatR0_Z;
	g_camMatR1_X = g_curMatR1_X;
	g_camMatR1_Y = g_curMatR1_Y;
	g_viewMtx01 = viewMtx01 * flt_5A94B8;
	g_viewMtx02 = viewMtx02 * flt_5A94B8;
	g_camMatR1_Z = g_curMatR1_Z;
	g_camMatR2_X = g_curMatR2_X;
	g_viewMtx10 = viewMtx10 * flt_5A94B8;
	g_viewMtx11 = viewMtx11 * flt_5A94B8;
	g_viewMtx12 = viewMtx12 * flt_5A94B8;
	g_viewMtx20 = (float)g_curMatR2_X * flt_5A94B8;
	g_viewMtx21 = (float)g_curMatR2_Y * flt_5A94B8;
	g_viewMtx22 = (float)g_curMatR2_Z * flt_5A94B8;
	g_camMatR2_Y = g_curMatR2_Y;
	g_camMatR2_Z = g_curMatR2_Z;
#ifdef XWA_MODERN
	FVIEW_StoreCamShadow(camShadow);
#endif
	return g_curMatR2_Z;
}

// FUNCTION: XWA 0x43FC90
void FVIEW_BuildCameraOrientNoTurret(int16_t rollQ16, int16_t pitchQ16, int16_t yawQ16, int angle25Q16,
									 int16_t extraPitchQ16, int16_t extraYawQ16, ObjectRecord* objRecord,
									 int playerIdx) {
	int cameraAxisX;
	int cameraAxisY;
	int cameraAxisZ;
	float viewMtx00;
	float viewMtx01;
	float viewMtx02;
	float viewMtx10;
	float viewMtx11;
	float viewMtx12;
#ifdef XWA_MODERN
	double camShadow[9];
	double shadowCamAxis[3];
#endif

	(void)playerIdx;
	FVIEW_calcrotatemove(pitchQ16, yawQ16, objRecord);
	FVIEW_calcrotateorient((Q16Angle)rollQ16, (Q16Angle)angle25Q16, objRecord);
#ifdef XWA_MODERN
	FVIEW_ShadowRotMove(camShadow, pitchQ16, yawQ16);
	FVIEW_ShadowRotOrient(camShadow, (Q16Angle)rollQ16, (Q16Angle)angle25Q16, objRecord);
	camShadow[3] = -camShadow[3];
	camShadow[4] = -camShadow[4];
	camShadow[5] = -camShadow[5];
	camShadow[6] = -camShadow[6];
	camShadow[7] = -camShadow[7];
	camShadow[8] = -camShadow[8];
	shadowCamAxis[0] = camShadow[3];
	shadowCamAxis[1] = camShadow[4];
	shadowCamAxis[2] = camShadow[5];
	FVIEW_ShadowRotateAxes(camShadow, camShadow[0], camShadow[1], camShadow[2], extraPitchQ16);
	FVIEW_ShadowRotateAxes(camShadow, shadowCamAxis[0], shadowCamAxis[1], shadowCamAxis[2], extraYawQ16);
#endif

	g_curMatR2_X = -g_curMatR2_X;
	g_curMatR2_Y = -g_curMatR2_Y;
	g_curMatR2_Z = -g_curMatR2_Z;
	g_curMatR1_X = -g_curMatR1_X;
	g_curMatR1_Y = -g_curMatR1_Y;
	g_curMatR1_Z = -g_curMatR1_Z;

	cameraAxisX = g_curMatR1_X;
	cameraAxisY = g_curMatR1_Y;
	cameraAxisZ = g_curMatR1_Z;

	FVIEW_transformaxes(g_curMatR0_X, g_curMatR0_Y, g_curMatR0_Z, extraPitchQ16);
	FVIEW_transformaxes(cameraAxisX, cameraAxisY, cameraAxisZ, extraYawQ16);

	viewMtx00 = (float)g_curMatR0_X;
	viewMtx01 = (float)g_curMatR0_Y;
	viewMtx02 = (float)g_curMatR0_Z;
	viewMtx10 = (float)g_curMatR1_X;
	viewMtx11 = (float)g_curMatR1_Y;
	viewMtx12 = (float)g_curMatR1_Z;

	g_camMatR0_X = g_curMatR0_X;
	g_camMatR0_Y = g_curMatR0_Y;
	g_camMatR0_Z = g_curMatR0_Z;
	g_camMatR1_X = g_curMatR1_X;
	g_viewMtx00 = viewMtx00 * flt_5A94B8;
	g_viewMtx01 = viewMtx01 * flt_5A94B8;
	g_viewMtx02 = viewMtx02 * flt_5A94B8;
	g_viewMtx10 = viewMtx10 * flt_5A94B8;
	g_camMatR1_Y = g_curMatR1_Y;
	g_camMatR1_Z = g_curMatR1_Z;
	g_camMatR2_X = g_curMatR2_X;
	g_camMatR2_Y = g_curMatR2_Y;
	g_viewMtx11 = viewMtx11 * flt_5A94B8;
	g_camMatR2_Z = g_curMatR2_Z;
	g_viewMtx12 = viewMtx12 * flt_5A94B8;
	g_viewMtx20 = (float)g_curMatR2_X * flt_5A94B8;
	g_viewMtx21 = (float)g_curMatR2_Y * flt_5A94B8;
	g_viewMtx22 = (float)g_curMatR2_Z * flt_5A94B8;
#ifdef XWA_MODERN
	FVIEW_StoreCamShadow(camShadow);
#endif
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x440300
int FVIEW_ComputeObjectViewMatrix(void) {
	int value;

	value = g_curMatR0_X;
	value = Xwa_SaturateWrappedQ30ToQ15(*(&value) * g_camMatR0_X + g_curMatR0_Y * g_camMatR0_Y +
										g_curMatR0_Z * g_camMatR0_Z);
	g_objViewMatF_R0_X = (float)value * flt_5A94B8;
	g_objViewMat_R0_X = value;

	value = g_curMatR0_X;
	value = Xwa_SaturateWrappedQ30ToQ15(value * g_camMatR1_X + g_curMatR0_Y * g_camMatR1_Y +
										g_curMatR0_Z * g_camMatR1_Z);
	g_objViewMatF_R0_Y = (float)value * flt_5A94B8;
	g_objViewMat_R0_Y = value;

	value = g_curMatR0_X;
	value = Xwa_SaturateWrappedQ30ToQ15(value * g_camMatR2_X + g_curMatR0_Y * g_camMatR2_Y +
										g_curMatR0_Z * g_camMatR2_Z);
	g_objViewMatF_R0_Z = (float)value * flt_5A94B8;
	g_objViewMat_R0_Z = value;

	value = g_curMatR2_X;
	value = Xwa_SaturateWrappedQ30ToQ15(value * g_camMatR0_X + g_curMatR2_Y * g_camMatR0_Y +
										g_curMatR2_Z * g_camMatR0_Z);
	g_objViewMatF_R1_X = (float)value * flt_5A94B8;
	g_objViewMat_R1_X = value;

	value = g_curMatR2_X;
	value = Xwa_SaturateWrappedQ30ToQ15(value * g_camMatR1_X + g_curMatR2_Y * g_camMatR1_Y +
										g_curMatR2_Z * g_camMatR1_Z);
	g_objViewMatF_R1_Y = (float)value * flt_5A94B8;
	g_objViewMat_R1_Y = value;

	value = g_curMatR2_X;
	value = Xwa_SaturateWrappedQ30ToQ15(value * g_camMatR2_X + g_curMatR2_Y * g_camMatR2_Y +
										g_curMatR2_Z * g_camMatR2_Z);
	g_objViewMatF_R1_Z = (float)value * flt_5A94B8;
	g_objViewMat_R1_Z = value;

	value = g_curMatR1_X;
	value = Xwa_SaturateWrappedQ30ToQ15(value * g_camMatR0_X + g_curMatR1_Y * g_camMatR0_Y +
										g_curMatR1_Z * g_camMatR0_Z);
	g_objViewMatF_R2_X = (float)value * flt_5A94B8;
	g_objViewMat_R2_X = value;

	value = g_curMatR1_X;
	value = Xwa_SaturateWrappedQ30ToQ15(value * g_camMatR1_X + g_curMatR1_Y * g_camMatR1_Y +
										g_curMatR1_Z * g_camMatR1_Z);
	g_objViewMatF_R2_Y = (float)value * flt_5A94B8;
	g_objViewMat_R2_Y = value;

	value = g_curMatR1_X;
	value = Xwa_SaturateWrappedQ30ToQ15(value * g_camMatR2_X + g_curMatR1_Y * g_camMatR2_Y +
										g_curMatR1_Z * g_camMatR2_Z);
	g_objViewMatF_R2_Z = (float)value * flt_5A94B8;
	g_objViewMat_R2_Z = value;

#ifdef XWA_MODERN
	/* Geometry-wobble fix: when the double camera shadow's exact Q15 source
	 * tag still matches g_camMat, derive the float matrix from it instead of the
	 * truncated Q15 product, so the per-frame requantization of the camera
	 * basis cannot displace models far from their origin. Same (R0, R2, R1)
	 * row arrangement as the integer matrix above. Any other camera writer
	 * fails the match and keeps the original integer-derived floats. */
	if (FVIEW_CamShadowMatchesInts()) {
		g_objViewMatF_R0_X =
			(float)(((double)g_curMatR0_X * g_camShadow[0] + (double)g_curMatR0_Y * g_camShadow[1] +
					 (double)g_curMatR0_Z * g_camShadow[2]) *
					(1.0 / 32768.0));
		g_objViewMatF_R0_Y =
			(float)(((double)g_curMatR0_X * g_camShadow[3] + (double)g_curMatR0_Y * g_camShadow[4] +
					 (double)g_curMatR0_Z * g_camShadow[5]) *
					(1.0 / 32768.0));
		g_objViewMatF_R0_Z =
			(float)(((double)g_curMatR0_X * g_camShadow[6] + (double)g_curMatR0_Y * g_camShadow[7] +
					 (double)g_curMatR0_Z * g_camShadow[8]) *
					(1.0 / 32768.0));
		g_objViewMatF_R1_X =
			(float)(((double)g_curMatR2_X * g_camShadow[0] + (double)g_curMatR2_Y * g_camShadow[1] +
					 (double)g_curMatR2_Z * g_camShadow[2]) *
					(1.0 / 32768.0));
		g_objViewMatF_R1_Y =
			(float)(((double)g_curMatR2_X * g_camShadow[3] + (double)g_curMatR2_Y * g_camShadow[4] +
					 (double)g_curMatR2_Z * g_camShadow[5]) *
					(1.0 / 32768.0));
		g_objViewMatF_R1_Z =
			(float)(((double)g_curMatR2_X * g_camShadow[6] + (double)g_curMatR2_Y * g_camShadow[7] +
					 (double)g_curMatR2_Z * g_camShadow[8]) *
					(1.0 / 32768.0));
		g_objViewMatF_R2_X =
			(float)(((double)g_curMatR1_X * g_camShadow[0] + (double)g_curMatR1_Y * g_camShadow[1] +
					 (double)g_curMatR1_Z * g_camShadow[2]) *
					(1.0 / 32768.0));
		g_objViewMatF_R2_Y =
			(float)(((double)g_curMatR1_X * g_camShadow[3] + (double)g_curMatR1_Y * g_camShadow[4] +
					 (double)g_curMatR1_Z * g_camShadow[5]) *
					(1.0 / 32768.0));
		g_objViewMatF_R2_Z =
			(float)(((double)g_curMatR1_X * g_camShadow[6] + (double)g_curMatR1_Y * g_camShadow[7] +
					 (double)g_curMatR1_Z * g_camShadow[8]) *
					(1.0 / 32768.0));
	}
#endif
	return value;
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x43FE50
int FVIEW_SetObjectTransform(Q16Angle angleC, Q16Angle angleA, Q16Angle angleB, Q16Angle angleD,
							 ObjectRecord* objRecord) {
	MobileObject* mobj;

	if (objRecord == NULL) {
		FVIEW_calcrotatemove((int16_t)angleA, (int16_t)angleB, NULL);
		FVIEW_calcrotateorient(angleC, angleD, NULL);
		return FVIEW_ComputeObjectViewMatrix();
	}

	mobj = objRecord->mobj;
	if (mobj->orientMatrixDirty) {
		FVIEW_calcrotatemove((int16_t)angleA, (int16_t)angleB, objRecord);
		FVIEW_calcrotateorient(angleC, angleD, objRecord);
		return FVIEW_ComputeObjectViewMatrix();
	}

	g_fviewFwdX_Q15 = mobj->cachedFwdX;
	g_fviewFwdY_Q15 = objRecord->mobj->cachedFwdY;
	g_fviewFwdZ_Q15 = objRecord->mobj->cachedFwdZ;
	g_fviewSideX_Q15 = objRecord->mobj->cachedSideX;
	g_fviewSideY_Q15 = objRecord->mobj->cachedSideY;
	g_fviewSideZ_Q15 = objRecord->mobj->cachedSideZ;
	g_fviewUpX_Q15 = objRecord->mobj->cachedUpX;
	g_fviewUpY_Q15 = objRecord->mobj->cachedUpY;
	g_fviewUpZ_Q15 = objRecord->mobj->cachedUpZ;
	g_curMatR2_X = -g_fviewFwdX_Q15;
	g_curMatR2_Y = -g_fviewFwdY_Q15;
	g_curMatR2_Z = -g_fviewFwdZ_Q15;
	g_curMatR0_X = g_fviewSideX_Q15;
	g_curMatR0_Y = g_fviewSideY_Q15;
	g_curMatR0_Z = g_fviewSideZ_Q15;
	g_curMatR1_X = g_fviewUpX_Q15;
	g_curMatR1_Y = g_fviewUpY_Q15;
	g_curMatR1_Z = g_fviewUpZ_Q15;
	return FVIEW_ComputeObjectViewMatrix();
}
