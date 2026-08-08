#include "xwa/flight/object/collision.h"
#include "xwa/flight/hangar.h"

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/damage.h"
#include "xwa/flight/death_star.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_light.h"
#include "xwa/audio/fsfx.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/fixed.h"
#include "xwa/math/scalar.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/yard.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/random.h"
#include "xwa/util/time.h"

#include <math.h>
#include <string.h>

// GLOBAL: XWA 0x761E38
float g_collidePlaneEpsilonPos;
// GLOBAL: XWA 0x761DEC
float g_collidePlaneEpsilonNeg;
// GLOBAL: XWA 0x5A9EB4
float g_collideSweepHitBackoff = 0.1f;
// GLOBAL: XWA 0x5A9EB8
float g_collideZeroFloat = 0.0f;
// GLOBAL: XWA 0x5A9EC0
float g_collideQ15ToUnitScale = 0.000030517578f;
// GLOBAL: XWA 0x5A9ECC
float g_engineWashFullIntensity = 1.0f;
// GLOBAL: XWA 0x5A9ED0
float g_engineOutputScaleMax = 65535.0f;
// GLOBAL: XWA 0x5A935C
float g_collideRicochetQ15Scale = 0.000030518509f;
// GLOBAL: XWA 0x5A9360
float g_collideRicochetProjectionScale = 0.000061035156f;
// GLOBAL: XWA 0x5A9368
float g_collideRicochetMaximumDamage = 65535.0f;
// GLOBAL: XWA 0x5A939C
float g_proximityDamageFullScale = 1.0f;
// GLOBAL: XWA 0x5A9ED8
double g_engineWashMinimumIntensity = 0.01;
// GLOBAL: XWA 0x91B3C8
int g_collideSweepAllowUnownedTargets;
// GLOBAL: XWA 0x91B3CC
float g_collideRicochetDamageScale;
// GLOBAL: XWA 0x761E3C
float g_collidePolygonEdgeCrossScratch;
// GLOBAL: XWA 0x761DE8
float g_collideLooseSweepEdgeThreshold;
// GLOBAL: XWA 0x60E824
int g_collisionStagedModelProbe;
// GLOBAL: XWA 0x60E828
// Set while collide_damagecraft applies area/proximity (splash) damage, which
// changes source attribution and shield handling. Owned by the proximity path.
uint8_t g_applyingProximityDamage;
// GLOBAL: XWA 0x8BF384
int g_collideUpdateCollisionObjLink;
// GLOBAL: XWA 0x805415
// Enables the warhead-class projectile impact bounce on struck craft in
// collide_laserhitcraft. Cleared/set by the flight main loop.
uint8_t g_craftImpactBounceEnabled;
// GLOBAL: XWA 0x60E808
Vec3f g_collisionImpactEffectCenter;
// GLOBAL: XWA 0x60E818
Vec3f g_collisionImpactEffectNormal;
// GLOBAL: XWA 0x761DF0
Vec3f g_collideSweepWalkerStart;
// GLOBAL: XWA 0x761E00
Vec3f g_collideSweepWalkerEnd;
// GLOBAL: XWA 0x91B3D0
Vec3f g_collideSweepLocalStart;
// GLOBAL: XWA 0x91B3E0
Vec3f g_collideSweepLocalEnd;
// GLOBAL: XWA 0x7B33A8
int g_collisionProbeWorldX;
// GLOBAL: XWA 0x7B33AC
int g_collisionProbeWorldY;
// GLOBAL: XWA 0x7B33A0
int g_collisionProbeWorldZ;
// GLOBAL: XWA 0x8C1644
int g_collisionSegmentStartWorldX;
// GLOBAL: XWA 0x91AB64
int g_collisionSegmentStartWorldY;
// GLOBAL: XWA 0x91ACA4
int g_collisionSegmentStartWorldZ;
// GLOBAL: XWA 0x80B600
int g_collisionSweepStartX;
// GLOBAL: XWA 0x80DC60
int g_collisionSweepStartY;
// GLOBAL: XWA 0x7FFA44
int g_collisionSweepStartZ;
// GLOBAL: XWA 0x910784
int g_collisionSweepEndX;
// GLOBAL: XWA 0x910790
int g_collisionSweepEndY;
// GLOBAL: XWA 0x910794
int g_collisionSweepEndZ;
// GLOBAL: XWA 0x80AD2C
int g_collisionHitOffsetX;
// GLOBAL: XWA 0x8052B0
int g_collisionHitOffsetY;
// GLOBAL: XWA 0x7FBB64
int g_collisionHitOffsetZ;
// GLOBAL: XWA 0x761E0C
int g_savedCollisionSegmentStartWorldX;
// GLOBAL: XWA 0x761E10
int g_savedCollisionSegmentStartWorldY;
// GLOBAL: XWA 0x761E14
int g_savedCollisionSegmentStartWorldZ;
// GLOBAL: XWA 0x761E2C
int g_savedCollisionProbeWorldX;
// GLOBAL: XWA 0x761E30
int g_savedCollisionProbeWorldY;
// GLOBAL: XWA 0x761E34
int g_savedCollisionProbeWorldZ;
// GLOBAL: XWA 0x761E4C
int g_savedCollisionSweepStartX;
// GLOBAL: XWA 0x761E54
int g_savedCollisionSweepStartY;
// GLOBAL: XWA 0x761E60
int g_savedCollisionSweepStartZ;
// GLOBAL: XWA 0x761E20
int g_savedCollisionSweepEndX;
// GLOBAL: XWA 0x761E24
int g_savedCollisionSweepEndY;
// GLOBAL: XWA 0x761E1C
int g_savedCollisionSweepEndZ;
// GLOBAL: XWA 0x761E28
int g_savedCollisionHitOffsetX;
// GLOBAL: XWA 0x761E48
int g_savedCollisionHitOffsetY;
// GLOBAL: XWA 0x761E40
int g_savedCollisionHitOffsetZ;
// GLOBAL: XWA 0x770E90
int g_collisionScratchBackupSegmentStartWorldX;
// GLOBAL: XWA 0x770E94
int g_collisionScratchBackupSegmentStartWorldY;
// GLOBAL: XWA 0x770E98
int g_collisionScratchBackupSegmentStartWorldZ;
// GLOBAL: XWA 0x770E9C
int g_collisionScratchBackupSweepEndZ;
// GLOBAL: XWA 0x770EA0
int g_collisionScratchBackupSweepEndX;
// GLOBAL: XWA 0x770EA4
int g_collisionScratchBackupSweepEndY;
// GLOBAL: XWA 0x770EA8
int g_collisionScratchBackupHitOffsetX;
// GLOBAL: XWA 0x770EAC
int g_collisionScratchBackupProbeWorldX;
// GLOBAL: XWA 0x770EB0
int g_collisionScratchBackupProbeWorldY;
// GLOBAL: XWA 0x770EB4
int g_collisionScratchBackupProbeWorldZ;
// GLOBAL: XWA 0x770EB8
int g_collisionScratchBackupHitOffsetZ;
// GLOBAL: XWA 0x770EBC
int g_collisionScratchBackupHitOffsetY;
// GLOBAL: XWA 0x770EC0
int g_collisionScratchBackupSweepStartX;
// GLOBAL: XWA 0x770EC4
int g_collisionScratchBackupSweepStartY;
// GLOBAL: XWA 0x770EC8
int g_collisionScratchBackupSweepStartZ;
// GLOBAL: XWA 0x5FE730
int g_collideFaceGroupChildSelector = 1;
// GLOBAL: XWA 0x761E18
int g_collideSweepHitMeshOrdinal;
// GLOBAL: XWA 0x761E44
OptRotationScale* g_collideCurrentRotScaleData;
// GLOBAL: XWA 0x761E50
int g_collideSweepCurrentMeshOrdinal;
// GLOBAL: XWA 0x761E58
float g_collideCurrentMeshRotationAngle;
// GLOBAL: XWA 0x761E5C
int g_collideSweepSkipSsdMeshOrdinal;
// GLOBAL: XWA 0x761E64
float g_collideSweepHitFraction;
// GLOBAL: XWA 0x761E68
int g_collideSweepRejectNearStartHits;
// GLOBAL: XWA 0x761E6C
OptNode* g_collideCurrentMeshVertsNode;
// GLOBAL: XWA 0x91B3EC
int g_collideSweepAuxHardpointIdx;
// GLOBAL: XWA 0x91B3F0
int g_collideSweepAuxHardpointWorldOffsetX;
// GLOBAL: XWA 0x91B3F4
int g_collideSweepAuxHardpointWorldOffsetY;
// GLOBAL: XWA 0x91B3F8
int g_collideSweepAuxHardpointWorldOffsetZ;
// GLOBAL: XWA 0x5B66B4
const int g_laserConvergenceDistanceByLevel_BiasedBase[4] = {
	0x01330132,
	0x000027ae,
	0x0000770a,
	0x0000c666,
};

typedef struct CollisionTargetRangeScratch {
	int segmentStartWorldX;
	int segmentStartWorldY;
	int segmentStartWorldZ;
	int probeWorldX;
	int probeWorldY;
	int probeWorldZ;
	int sweepStartX;
	int sweepStartY;
	int sweepStartZ;
	int sweepEndX;
	int sweepEndY;
	int sweepEndZ;
	int hitOffsetX;
	int hitOffsetY;
	int hitOffsetZ;
} CollisionTargetRangeScratch;

static __inline int collide_DotQ15(int x, int y, int z, int basisX, int basisY, int basisZ) {
	return Xwa_Dot3Q15ReuseXSlot(x, y, z, basisX, basisY, basisZ);
}

static __inline int collide_MulWrap32(int a, int b) { return (int)((uint32_t)a * (uint32_t)b); }

static __inline int collide_ScaleBy1000Wrap32(int value) { return (int)((uint32_t)value * 1000u); }

static __inline int collide_DivideWrap32(int numerator, int denominator) { return numerator / denominator; }

static __inline int16_t collide_AddMoveComponentNoSignOverflow(int16_t base, int delta) {
	int sum;

	sum = (int)((uint16_t)base + (uint16_t)delta);
	if ((((uint16_t)((uint16_t)base ^ (uint16_t)sum) >> 8) & 0x80u) != 0) {
		return base;
	}
	return (int16_t)sum;
}

static __inline int collide_FoldYawDeltaToQuarterTurn(uint16_t yawDelta) {
	uint16_t magnitude;

	magnitude = yawDelta;
	if (magnitude >= 0x8000u) {
		magnitude = (uint16_t)-magnitude;
	}
	if (magnitude > 0x4000u) {
		magnitude = (uint16_t)(0x8000u - magnitude);
	}
	return (int)magnitude;
}

static __inline int collide_roughdistance3d_inline(int dx, int dy, int dz) {
	int absDx;
	int absDy;
	int absDz;

	absDx = dx;
	absDy = dy;
	absDz = dz;
	if (absDx < 0) {
		absDx = -absDx;
	}
	if (absDy < 0) {
		absDy = -absDy;
	}
	if (absDz < 0) {
		absDz = -absDz;
	}

	if (absDy < absDx && absDx > absDz) {
		return (absDz >> 2) + absDx + (absDy >> 2);
	}
	if (absDy <= absDx || absDy <= absDz) {
		return (absDx >> 2) + absDz + (absDy >> 2);
	}
	return (absDz >> 2) + absDy + (absDx >> 2);
}

// FUNCTION: XWA 0x412170
unsigned int collide_roughdistance3du(unsigned int abs_dx, unsigned int abs_dy, unsigned int abs_dz) {
	if (abs_dx > abs_dy && abs_dx > abs_dz) {
		return (abs_dz >> 2) + abs_dx + (abs_dy >> 2);
	}
	if (abs_dy <= abs_dx || abs_dy <= abs_dz) {
		return (abs_dx >> 2) + abs_dz + (abs_dy >> 2);
	}
	return (abs_dz >> 2) + abs_dy + (abs_dx >> 2);
}

// FUNCTION: XWA 0x4121C0
int collide_roughdistance3d(int dx, int dy, int dz) {
	int absDy;
	int absDz;
	int absDx;

	absDx = dx;
	if (absDx < 0) {
		absDx = -absDx;
	}
	absDy = dy;
	if (absDy < 0) {
		absDy = -absDy;
	}
	absDz = dz;
	if (absDz < 0) {
		absDz = -absDz;
	}

	if (absDy < absDx && absDx > absDz) {
		return absDx + (absDy >> 2) + (absDz >> 2);
	}
	if (absDy > absDx && absDy > absDz) {
		return absDy + (absDx >> 2) + (absDz >> 2);
	}
	return absDz + (absDx >> 2) + (absDy >> 2);
}

// FUNCTION: XWA 0x40CE30
uint16_t collide_checkboxcollision(int radius) {
	int sweepDeltaX;
	int sweepDeltaY;
	int sweepDeltaZ;
	int probeDeltaX;
	int probeDeltaY;
	int probeDeltaZ;
	int startRelX;
	int startRelY;
	int startRelZ;
	int deltaX;
	int deltaY;
	int deltaZ;
	int xNearNumerator;
	int xFarNumerator;
	int yNearNumerator;
	int yFarNumerator;
	int zNearNumerator;
	int zFarNumerator;
	int tEnter;
	int tExit;
	int tCandidate;
	int endRel;
	int normalEndX;
	int normalEndY;
	int normalEndZ;
	int normalProbeX;
	int normalProbeY;
	int normalProbeZ;
	uint16_t scaleQ15;
	float normalLen;

	probeDeltaY = g_collisionProbeWorldY;
	probeDeltaZ = g_collisionProbeWorldZ;
	sweepDeltaY = g_collisionSweepEndY;
	sweepDeltaX = g_collisionSweepEndX;
	probeDeltaX = g_collisionProbeWorldX;
	sweepDeltaZ = g_collisionSweepEndZ;
	probeDeltaZ -= g_collisionSegmentStartWorldZ;
	sweepDeltaY -= g_collisionSweepStartY;
	sweepDeltaX -= g_collisionSweepStartX;
	probeDeltaX -= g_collisionSegmentStartWorldX;
	probeDeltaY -= g_collisionSegmentStartWorldY;
	sweepDeltaZ -= g_collisionSweepStartZ;

	startRelX = g_collisionSweepStartX - g_collisionSegmentStartWorldX;
	if (startRelX > radius) {
		deltaX = sweepDeltaX - probeDeltaX;
		xFarNumerator = radius - startRelX;
		if (deltaX >= 0) {
			return 0;
		}
		if (xFarNumerator < deltaX) {
			return 0;
		}
		xNearNumerator = -(startRelX + radius);
	} else if (startRelX < -radius) {
		xNearNumerator = -(startRelX + radius);
		deltaX = sweepDeltaX - probeDeltaX;
		if (deltaX < 0) {
			return 0;
		}
		if (xNearNumerator >= deltaX) {
			return 0;
		}
		xFarNumerator = radius - startRelX;
	} else {
		xFarNumerator = radius - startRelX;
		xNearNumerator = -(startRelX + radius);
		deltaX = 0;
	}

	startRelY = g_collisionSweepStartY - g_collisionSegmentStartWorldY;
	if (startRelY > radius) {
		yFarNumerator = radius - startRelY;
		deltaY = sweepDeltaY - probeDeltaY;
		if (deltaY >= 0) {
			return 0;
		}
		if (yFarNumerator < deltaY) {
			return 0;
		}
		yNearNumerator = -(startRelY + radius);
	} else if (startRelY < -radius) {
		yNearNumerator = -(startRelY + radius);
		deltaY = sweepDeltaY - probeDeltaY;
		if (deltaY < 0) {
			return 0;
		}
		if (yNearNumerator >= deltaY) {
			return 0;
		}
		yFarNumerator = radius - startRelY;
	} else {
		deltaY = 0;
		yFarNumerator = radius - startRelY;
		yNearNumerator = -(startRelY + radius);
	}

	startRelZ = g_collisionSweepStartZ - g_collisionSegmentStartWorldZ;
	if (startRelZ > radius) {
		zFarNumerator = radius - startRelZ;
		deltaZ = sweepDeltaZ - probeDeltaZ;
		if (deltaZ >= 0) {
			return 0;
		}
		if (zFarNumerator < deltaZ) {
			return 0;
		}
		zNearNumerator = -(startRelZ + radius);
	} else if (startRelZ < -radius) {
		zNearNumerator = -(startRelZ + radius);
		deltaZ = sweepDeltaZ - probeDeltaZ;
		if (deltaZ < 0) {
			return 0;
		}
		if (zNearNumerator >= deltaZ) {
			return 0;
		}
		zFarNumerator = radius - startRelZ;
	} else {
		deltaZ = 0;
		zFarNumerator = radius - startRelZ;
		zNearNumerator = -(startRelZ + radius);
	}

#ifdef XWA_MODERN
	xFarNumerator = (int32_t)((uint32_t)xFarNumerator << 8);
	xNearNumerator = (int32_t)((uint32_t)xNearNumerator << 8);
	yNearNumerator = (int32_t)((uint32_t)yNearNumerator << 8);
	yFarNumerator = (int32_t)((uint32_t)yFarNumerator << 8);
	zFarNumerator = (int32_t)((uint32_t)zFarNumerator << 8);
	zNearNumerator = (int32_t)((uint32_t)zNearNumerator << 8);
#else
	xFarNumerator <<= 8;
	xNearNumerator <<= 8;
	yNearNumerator <<= 8;
	yFarNumerator <<= 8;
	zFarNumerator <<= 8;
	zNearNumerator <<= 8;
#endif

	if (deltaX == 0) {
		deltaX = sweepDeltaX - probeDeltaX;
		endRel = g_collisionSweepEndX - g_collisionProbeWorldX;
		if (endRel > radius) {
			tEnter = 0;
			tExit = xFarNumerator / deltaX;
		} else if (endRel < -radius) {
			tEnter = 0;
			tExit = xNearNumerator / deltaX;
		} else {
			tEnter = 0;
			tExit = 255;
		}
	} else {
		tEnter = xFarNumerator / deltaX;
		tExit = xNearNumerator / deltaX;
		if (deltaX >= 0) {
			tCandidate = tEnter;
			tEnter = tExit;
			tExit = tCandidate;
		}
	}

	if (deltaY == 0) {
		deltaY = sweepDeltaY - probeDeltaY;
		endRel = g_collisionSweepEndY - g_collisionProbeWorldY;
		if (endRel > radius) {
			tCandidate = yFarNumerator / deltaY;
		} else if (endRel < -radius) {
			tCandidate = yNearNumerator / deltaY;
		} else {
			tCandidate = 255;
		}
		if (tExit < 0) {
			return 0;
		}
		if (tEnter < 0) {
			tEnter = 0;
		}
		if (tCandidate < tEnter) {
			return 0;
		}
		if (tCandidate < tExit) {
			tExit = tCandidate;
		}
	} else {
		tCandidate = yFarNumerator / deltaY;
		if (deltaY >= 0) {
			if (tCandidate < tEnter) {
				return 0;
			}
			if (tCandidate < tExit) {
				tExit = tCandidate;
			}
			tCandidate = yNearNumerator / deltaY;
			if (tCandidate > tExit) {
				return 0;
			}
			if (tCandidate > tEnter) {
				tEnter = tCandidate;
			}
		} else {
			if (tCandidate > tExit) {
				return 0;
			}
			if (tCandidate > tEnter) {
				tEnter = tCandidate;
			}
			tCandidate = yNearNumerator / deltaY;
			if (tCandidate < tEnter) {
				return 0;
			}
			if (tCandidate < tExit) {
				tExit = tCandidate;
			}
		}
	}

	if (deltaZ == 0) {
		deltaZ = sweepDeltaZ - probeDeltaZ;
		endRel = g_collisionSweepEndZ - g_collisionProbeWorldZ;
		if (endRel > radius) {
			tCandidate = zFarNumerator / deltaZ;
		} else if (endRel < -radius) {
			tCandidate = zNearNumerator / deltaZ;
		} else {
			tCandidate = 255;
		}
		if (tExit < 0) {
			return 0;
		}
		if (tEnter < 0) {
			tEnter = 0;
		}
		if (tCandidate < tEnter) {
			return 0;
		}
	} else {
		tCandidate = zFarNumerator / deltaZ;
		if (deltaZ >= 0) {
			if (tCandidate < tEnter) {
				return 0;
			}
			if (tCandidate < tExit) {
				tExit = tCandidate;
			}
			tCandidate = zNearNumerator / deltaZ;
			if (tCandidate > tExit) {
				return 0;
			}
			if (tCandidate > tEnter) {
				tEnter = tCandidate;
			}
		} else {
			if (tCandidate > tExit) {
				return 0;
			}
			if (tCandidate > tEnter) {
				tEnter = tCandidate;
			}
			tCandidate = zNearNumerator / deltaZ;
			if (tCandidate < tEnter) {
				return 0;
			}
		}
	}

	if (tEnter > 255) {
		return 0;
	}

	g_collisionHitOffsetY = probeDeltaY;
	g_collisionHitOffsetX = probeDeltaX;
	g_collisionHitOffsetZ = probeDeltaZ;
	scaleQ15 = (uint16_t)tEnter << 7;
	g_collisionHitOffsetX = Xwa_Q15MulReuseFirstSlot(scaleQ15, g_collisionHitOffsetX);
	g_collisionHitOffsetY = Xwa_Q15MulReuseFirstSlot(scaleQ15, g_collisionHitOffsetY);
	g_collisionHitOffsetZ = Xwa_Q15MulReuseFirstSlot(scaleQ15, g_collisionHitOffsetZ);

	normalEndX = g_collisionSweepEndX;
	normalProbeX = g_collisionProbeWorldX;
	normalEndY = g_collisionSweepEndY;
	normalProbeZ = g_collisionProbeWorldZ;
	normalEndZ = g_collisionSweepEndZ;
	g_glowMarkWorldSegmentMode = 1;
	normalProbeY = g_collisionProbeWorldY;
	g_glowMarkScratchNormalVec->x = (float)((double)normalEndX - (double)normalProbeX);
	g_glowMarkScratchNormalVec->y = (float)((double)normalEndY - (double)normalProbeY);
	g_glowMarkScratchNormalVec->z = (float)((double)normalEndZ - (double)normalProbeZ);
	g_glowMarkPlaneScratch.center.x = g_glowMarkScratchNormalVec->x * 0.5f;
	g_glowMarkPlaneScratch.center.y = g_glowMarkScratchNormalVec->y * 0.5f;
	g_glowMarkPlaneScratch.center.z = g_glowMarkScratchNormalVec->z * 0.5f;

	normalLen = sqrt(g_glowMarkScratchNormalVec->y * g_glowMarkScratchNormalVec->y +
					 g_glowMarkScratchNormalVec->z * g_glowMarkScratchNormalVec->z +
					 g_glowMarkScratchNormalVec->x * g_glowMarkScratchNormalVec->x);
	g_glowMarkScratchNormalVec->x = (float)(g_glowMarkScratchNormalVec->x / normalLen);
	g_glowMarkScratchNormalVec->y = (float)(g_glowMarkScratchNormalVec->y / normalLen);
	g_glowMarkScratchNormalVec->z = (float)(g_glowMarkScratchNormalVec->z / normalLen);
	g_collideSweepAuxHardpointIdx = -1;
	return 0xffffu;
}

// FUNCTION: XWA 0x4E4210
uint16_t static_laserstaticcollide(unsigned int sourceObjIdx, unsigned int staticObjIdx) {
	unsigned int sourceSourceObjIdx;
	unsigned int staticGenusId;
	ObjectTypeId staticObjectType;
	int hitRadius;
	int broadRadius;
	int dx;
	int dy;
	int dz;
	unsigned int sourceToStaticDistance;
	int staticSweepAbs;

	sourceSourceObjIdx = (uint16_t)g_objectTable[sourceObjIdx].mobj->sourceObjIdx;
	if (sourceSourceObjIdx >= staticObjIdx && sourceSourceObjIdx != 0xffffu) {
		return 0;
	}

	staticGenusId = g_objectTable[staticObjIdx].genusId;
	if (staticGenusId == GENUS_Debris) {
		return 0;
	}
	if (staticGenusId != GENUS_Asteroid && sourceSourceObjIdx >= g_activeRegionObjectSlotStart &&
		sourceSourceObjIdx < g_activeRegionCraftObjectSlotEnd) {
		if (g_objectTable[sourceSourceObjIdx].playerOwnerIdx == -1 &&
			pai_GetEffectiveAIController(g_objectTable[sourceSourceObjIdx].mobj->pCraft)->targetObjIdx !=
				staticObjIdx) {
			return 0;
		}
	}

	staticObjectType = g_objectTable[staticObjIdx].objectType;
	Mission_ResolveObjectOrMissionPointWorldLoc(staticObjIdx, 0, 0, 0);
	g_collisionSweepStartX = worldlocx;
	g_collisionSweepEndX = worldlocx;
	g_collisionSweepStartY = worldlocy;
	g_collisionSweepEndY = worldlocy;
	g_collisionSweepStartZ = worldlocz;
	g_collisionSweepEndZ = worldlocz;

	hitRadius = g_modelTypeTable[staticObjectType].maxBoundsExtent +
				g_modelTypeTable[(uint16_t)g_objectTable[sourceObjIdx].objectType].maxBoundsExtent;
	broadRadius = hitRadius + 0x20000;

	dx = g_collisionProbeWorldX - worldlocx;
	if (dx < 0) {
		dx = -dx;
	}
	if (dx > broadRadius) {
		return 0;
	}
	dy = g_collisionProbeWorldY - worldlocy;
	if (dy < 0) {
		dy = -dy;
	}
	if (dy > broadRadius) {
		return 0;
	}
	dz = g_collisionProbeWorldZ - worldlocz;
	if (dz < 0) {
		dz = -dz;
	}
	if (dz > broadRadius) {
		return 0;
	}

	sourceToStaticDistance = collide_roughdistance3du((unsigned int)dx, (unsigned int)dy, (unsigned int)dz);
	if ((int)sourceToStaticDistance > broadRadius) {
		return 0;
	}

	dx = g_collisionProbeWorldX - g_collisionSegmentStartWorldX;
	if (dx < 0) {
		dx = -dx;
	}
	dy = g_collisionProbeWorldY - g_collisionSegmentStartWorldY;
	if (dy < 0) {
		dy = -dy;
	}
	dz = g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ;
	if (dz < 0) {
		dz = -dz;
	}

	staticSweepAbs = g_collisionSweepEndX - g_collisionSweepStartX;
	if (staticSweepAbs < 0) {
		staticSweepAbs = -staticSweepAbs;
	}
	dx += staticSweepAbs;
	staticSweepAbs = g_collisionSweepEndY - g_collisionSweepStartY;
	if (staticSweepAbs < 0) {
		staticSweepAbs = -staticSweepAbs;
	}
	dy += staticSweepAbs;
	staticSweepAbs = g_collisionSweepEndZ - g_collisionSweepStartZ;
	if (staticSweepAbs < 0) {
		staticSweepAbs = -staticSweepAbs;
	}
	dz += staticSweepAbs;

	if (sourceToStaticDistance > collide_roughdistance3du((unsigned int)(hitRadius + dx),
														  (unsigned int)(hitRadius + dy),
														  (unsigned int)(hitRadius + dz))) {
		return 0;
	}

	if (hitRadius >= 1095 || g_objectTable[staticObjIdx].objectType == OBJ_ContainerGem ||
		g_objectTable[staticObjIdx].genusId == GENUS_DeathStarTunnelSegment) {
		return (uint16_t)collide_CheckSweptModelCollision(sourceObjIdx, staticObjIdx);
	}
	hitRadius >>= 2;
	return collide_checkboxcollision(hitRadius + (hitRadius >> 1));
}

// FUNCTION: XWA 0x40C960
uint16_t collide_lasercraftcollide(unsigned int attackerObjIdx, unsigned int targetObjIdx) {
	unsigned int broadRadius;
	unsigned int targetBoundsExtent;
	unsigned int attackerBoundsExtent;
	unsigned int distanceX;
	unsigned int distanceY;
	unsigned int distanceZ;
	unsigned int probeDistance;
	int probeStartAbsX;
	int probeStartAbsY;
	int probeStartAbsZ;
	int probePathDistance;
	int sweepSpanX;
	int sweepSpanY;
	int sweepSpanZ;
	int hitRadius;
	unsigned int sweepEnvelope;
	int allowSimpleBoxHit;
	int probeWorldX;
	int probeWorldY;
	int probeWorldZ;

	targetBoundsExtent =
		(unsigned int)g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent;
	attackerBoundsExtent =
		(unsigned int)g_modelTypeTable[(uint16_t)g_objectTable[attackerObjIdx].objectType].maxBoundsExtent;
	broadRadius = targetBoundsExtent + attackerBoundsExtent + 0x20000u;
	g_approxDist = (int)broadRadius;

	probeWorldX = g_collisionProbeWorldX;
	distanceX = (unsigned int)(probeWorldX - g_collisionSweepEndX);
	if ((int)distanceX < 0) {
		distanceX = 0u - distanceX;
	}
	if (distanceX > broadRadius) {
		return 0;
	}
	distanceY = (unsigned int)(g_collisionProbeWorldY - g_collisionSweepEndY);
	if ((int)distanceY < 0) {
		distanceY = 0u - distanceY;
	}
	if (distanceY > broadRadius) {
		return 0;
	}
	probeWorldZ = g_collisionProbeWorldZ;
	distanceZ = (unsigned int)(probeWorldZ - g_collisionSweepEndZ);
	if ((int)distanceZ < 0) {
		distanceZ = 0u - distanceZ;
	}
	if (distanceZ > broadRadius) {
		return 0;
	}

	if (distanceX > distanceZ && distanceX > distanceY) {
		probeDistance = (distanceY >> 2) + distanceX + (distanceZ >> 2);
	} else if (distanceZ <= distanceX || distanceZ <= distanceY) {
		probeDistance = (distanceX >> 2) + distanceY + (distanceZ >> 2);
	} else {
		probeDistance = (distanceY >> 2) + distanceZ + (distanceX >> 2);
	}
	g_approxDist = (int)probeDistance;
	if (probeDistance > broadRadius) {
		return 0;
	}

	probeWorldX -= g_collisionSegmentStartWorldX;
	if (probeWorldX < 0) {
		probeWorldX = (int)(0u - (uint32_t)probeWorldX);
	}
	probeStartAbsX = probeWorldX;
	probeWorldY = g_collisionProbeWorldY;
	probeWorldY -= g_collisionSegmentStartWorldY;
	if (probeWorldY < 0) {
		probeWorldY = (int)(0u - (uint32_t)probeWorldY);
	}
	probeStartAbsY = probeWorldY;
	probeWorldZ -= g_collisionSegmentStartWorldZ;
	if (probeWorldZ < 0) {
		probeWorldZ = (int)(0u - (uint32_t)probeWorldZ);
	}
	probeStartAbsZ = probeWorldZ;
	probePathDistance = probeStartAbsX + probeStartAbsY + probeStartAbsZ;

	sweepSpanX = g_collisionSweepStartX - g_collisionSweepEndX;
	if (sweepSpanX < 0) {
		sweepSpanX = (int)(0u - (uint32_t)sweepSpanX);
	}
	probeStartAbsX += sweepSpanX;
	sweepSpanX = probeStartAbsX;
	sweepSpanY = g_collisionSweepStartY - g_collisionSweepEndY;
	if (sweepSpanY < 0) {
		sweepSpanY = (int)(0u - (uint32_t)sweepSpanY);
	}
	probeStartAbsY += sweepSpanY;
	sweepSpanY = probeStartAbsY;
	sweepSpanZ = g_collisionSweepStartZ - g_collisionSweepEndZ;
	if (sweepSpanZ < 0) {
		sweepSpanZ = (int)(0u - (uint32_t)sweepSpanZ);
	}
	probeStartAbsZ += sweepSpanZ;
	sweepSpanZ = probeStartAbsZ;

	hitRadius = collide_GetSweptHitRadius(attackerObjIdx, targetObjIdx, &allowSimpleBoxHit);
	sweepSpanX += hitRadius;
	sweepSpanY += hitRadius;
	sweepSpanZ += hitRadius;
	if ((unsigned int)sweepSpanY > (unsigned int)sweepSpanX &&
		(unsigned int)sweepSpanY > (unsigned int)sweepSpanZ) {
		sweepEnvelope =
			((unsigned int)sweepSpanZ >> 2) + (unsigned int)sweepSpanY + ((unsigned int)sweepSpanX >> 2);
	} else if ((unsigned int)sweepSpanX <= (unsigned int)sweepSpanY ||
			   (unsigned int)sweepSpanX <= (unsigned int)sweepSpanZ) {
		sweepEnvelope =
			((unsigned int)sweepSpanY >> 2) + (unsigned int)sweepSpanZ + ((unsigned int)sweepSpanX >> 2);
	} else {
		sweepEnvelope =
			((unsigned int)sweepSpanZ >> 2) + (unsigned int)sweepSpanX + ((unsigned int)sweepSpanY >> 2);
	}
	if (probeDistance > sweepEnvelope) {
		return 0;
	}

	if ((!allowSimpleBoxHit || hitRadius < 1095) &&
		g_objectTable[targetObjIdx].objectType != OBJ_ContainerGem) {
		ModelGenusId targetGenus;

		targetGenus = g_objectTable[targetObjIdx].genusId;
		if (targetGenus != GENUS_DeathStarTunnelSegment &&
			(targetGenus != GENUS_PilotDroid || !g_provingGroundsModeActive) &&
			targetGenus != GENUS_SalvageJunk) {
			goto simple_box_collision;
		}
	}

	if (g_collisionStagedModelProbe) {
		if (g_objectTable[targetObjIdx].mobj->speed < 40u) {
			unsigned int result;

			result = (unsigned int)collide_CheckSweptModelCollision(attackerObjIdx, targetObjIdx);
			g_collisionStagedModelProbe = 0;
			return result;
		}
		g_collisionStagedModelProbe = 0;
		goto simple_box_collision;
	}

	if (g_provingGroundsModeActive) {
		if (g_objectTable[targetObjIdx].objectType != OBJ_Compactor &&
			g_objectTable[targetObjIdx].objectType != OBJ_Centrifuge && probePathDistance == 0) {
			return 0;
		}
	} else if (probePathDistance == 0) {
		return 0;
	}

	{
		int savedUpdateCollisionObjLink;
		unsigned int result;

		savedUpdateCollisionObjLink = g_collideUpdateCollisionObjLink;
		g_collideUpdateCollisionObjLink = 1;
		result = (unsigned int)collide_CheckSweptModelCollision(attackerObjIdx, targetObjIdx);
		g_collideUpdateCollisionObjLink = savedUpdateCollisionObjLink;
		return result;
	}

simple_box_collision:
	if (g_missionHeader.body.missionType != XWA_MISSION_TYPE_DEATH_STAR ||
		g_objectTable[attackerObjIdx].objectType == OBJ_DSContainer ||
		g_objectTable[targetObjIdx].objectType == OBJ_DSContainer) {
		MobileObject* attackerMobj;
		int sourceObjectType;

		if (!g_provingGroundsModeActive) {
			hitRadius >>= 2;
		} else {
			attackerMobj = g_objectTable[attackerObjIdx].mobj;
			if (attackerMobj != NULL) {
				sourceObjectType = attackerMobj->sourceObjectType;
				if (sourceObjectType < OBJ_AccelRing2 ||
					(sourceObjectType > OBJ_SmeltingRoom && sourceObjectType != OBJ_AccelRing3)) {
					hitRadius >>= 2;
				}
			}
		}
	}
	return collide_checkboxcollision((int)(hitRadius + (hitRadius >> 1)));
}

// FUNCTION: XWA 0x40D410
uint16_t collide_targetinrange(int16_t shooterObjIdx, int16_t targetObjIdx, int16_t hardpointIndex) {
	static const float axisScale = 0.000030517578f;
	static const float angleScale = 0.000095873722f;

	uint16_t shooterIdx;
	uint16_t targetIdx;
	ObjectRecord* shooterObj;
	CraftData* shooterCraft;
	ModelIndex shooterModelIndex;
	int turretSeatIdx;
	uint16_t projectileType;
	uint16_t projectileSpeed;
	int lifetimeTicks;
	int projectileDistance;
	int muzzleWorldX;
	int muzzleWorldY;
	int muzzleWorldZ;
	int16_t localSide;
	int16_t localUp;
	int16_t localFwd;
	int16_t aimMoveX;
	int16_t aimMoveY;
	int16_t aimMoveZ;
	uint16_t result;
	CollisionTargetRangeScratch savedCollision;
	Matrix3x3 turretMatrix;
	float turretAxisAngle[4];
	Vec3f turretHardpoint;

	savedCollision.segmentStartWorldX = g_collisionSegmentStartWorldX;
	savedCollision.segmentStartWorldY = g_collisionSegmentStartWorldY;
	savedCollision.segmentStartWorldZ = g_collisionSegmentStartWorldZ;
	savedCollision.probeWorldX = g_collisionProbeWorldX;
	savedCollision.probeWorldY = g_collisionProbeWorldY;
	savedCollision.probeWorldZ = g_collisionProbeWorldZ;
	savedCollision.sweepStartX = g_collisionSweepStartX;
	savedCollision.sweepStartY = g_collisionSweepStartY;
	savedCollision.sweepStartZ = g_collisionSweepStartZ;
	savedCollision.sweepEndX = g_collisionSweepEndX;
	savedCollision.sweepEndY = g_collisionSweepEndY;
	savedCollision.sweepEndZ = g_collisionSweepEndZ;
	savedCollision.hitOffsetX = g_collisionHitOffsetX;
	savedCollision.hitOffsetY = g_collisionHitOffsetY;
	savedCollision.hitOffsetZ = g_collisionHitOffsetZ;

	shooterIdx = (uint16_t)shooterObjIdx;
	targetIdx = (uint16_t)targetObjIdx;
	if (g_players[g_localPlayer].objectIndex == shooterIdx) {
		turretSeatIdx = g_players[g_localPlayer].currentSeatIdx - 1;
	} else {
		turretSeatIdx = -1;
	}

	shooterObj = &g_objectTable[shooterIdx];
	shooterCraft = shooterObj->mobj->pCraft;
	shooterModelIndex = shooterCraft->modelIndex;
	projectileType =
		g_modelDefs[shooterModelIndex].laserGroupWeaponType[g_players[g_localPlayer].selectedWarhead];
	if ((int8_t)shooterCraft->warheadData[(uint16_t)hardpointIndex].laserCharge >= 64) {
		++projectileType;
	}

	projectileSpeed = g_projectileSpeedByType[projectileType - OBJ_LaserRebel];
	lifetimeTicks = 236u * (uint32_t)g_projectileLifetimeSecondsByType[projectileType - OBJ_LaserRebel];
	lifetimeTicks +=
		(uint16_t)MATH2_fraction(236u, g_projectileLifetimeFracQ16ByType[projectileType - OBJ_LaserRebel]);

	g_collisionSegmentStartWorldX = shooterObj->world_x;
	g_collisionSegmentStartWorldY = shooterObj->world_y;
	g_collisionSegmentStartWorldZ = shooterObj->world_z;

	if (turretSeatIdx == -1) {
		ModelWeaponHardpoint* hardpoint;

		hardpoint = &g_modelDefs[shooterModelIndex].weaponHardpoints[(uint16_t)hardpointIndex];
		localSide = hardpoint->x;
		localUp = hardpoint->z;
		localFwd = hardpoint->y;
	} else {
		uint16_t turretModelType;
		OptRotationScale* gunTurretRotScale;
		OptRotationScale* launcherRotScale;
		OptRotationScale* beamRotScale;
		uint16_t meshCount;
		uint16_t meshIdx;
		ModelIndex turretModelIndex;
		ModelDef* turretModelDef;

		gunTurretRotScale = NULL;
		launcherRotScale = NULL;
		beamRotScale = NULL;
		turretModelType = g_modelDefs[shooterModelIndex].turretModelIndex[turretSeatIdx];
		meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(turretModelType);
		for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
			MeshType meshType;

			meshType = ModelMesh_GetObjectTypeMeshType(turretModelType, meshIdx);
			switch (meshType) {
				case MESH_RotaryGunTurret:
					gunTurretRotScale = ModelMesh_GetRotScaleData(turretModelType, meshIdx);
					break;
				case MESH_RotaryLauncher:
					launcherRotScale = ModelMesh_GetRotScaleData(turretModelType, meshIdx);
					break;
				case MESH_RotaryBeamSystem:
					beamRotScale = ModelMesh_GetRotScaleData(turretModelType, meshIdx);
					break;
				default:
					break;
			}
		}

		turretModelIndex = GetModelIndexFromType((ObjectTypeId)turretModelType);
		turretModelDef = &g_modelDefs[turretModelIndex];
		localSide = turretModelDef->weaponHardpoints[0].x;
		localUp = turretModelDef->weaponHardpoints[0].z;
		localFwd = turretModelDef->weaponHardpoints[0].y;
		if (beamRotScale != NULL && launcherRotScale != NULL) {
			CraftData* turretCraft;

			turretCraft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
			turretHardpoint.x = (float)localSide - gunTurretRotScale->pivot.x;
			turretHardpoint.y = (float)-localFwd - gunTurretRotScale->pivot.y;
			turretHardpoint.z = (float)localUp - gunTurretRotScale->pivot.z;
			turretAxisAngle[0] = gunTurretRotScale->rotationAxis.x * axisScale;
			turretAxisAngle[1] = gunTurretRotScale->rotationAxis.y * axisScale;
			turretAxisAngle[2] = gunTurretRotScale->rotationAxis.z * axisScale;
			turretAxisAngle[3] =
				(float)(-(int16_t)turretCraft->turretAim.aimAngleA[turretSeatIdx] * angleScale);
			Math3D_BuildAxisAngleMatrix(&turretMatrix, turretAxisAngle);
			Math3D_RotateVec3(&turretHardpoint, &turretMatrix);

			turretHardpoint.x = turretHardpoint.x + gunTurretRotScale->pivot.x - beamRotScale->pivot.x;
			turretHardpoint.y = turretHardpoint.y + gunTurretRotScale->pivot.y - beamRotScale->pivot.y;
			turretHardpoint.z = turretHardpoint.z + gunTurretRotScale->pivot.z - beamRotScale->pivot.z;
			turretAxisAngle[0] = beamRotScale->rotationAxis.x * axisScale;
			turretAxisAngle[1] = beamRotScale->rotationAxis.y * axisScale;
			turretAxisAngle[2] = beamRotScale->rotationAxis.z * axisScale;
			turretAxisAngle[3] =
				(float)((int16_t)turretCraft->turretAim.aimAngleB[turretSeatIdx] * angleScale);
			Math3D_BuildAxisAngleMatrix(&turretMatrix, turretAxisAngle);
			Math3D_RotateVec3(&turretHardpoint, &turretMatrix);

			localSide = (int16_t)(int)(turretHardpoint.x + beamRotScale->pivot.x);
			localFwd = (int16_t)(int)-(turretHardpoint.y + beamRotScale->pivot.y);
			localUp = (int16_t)(int)(turretHardpoint.z + beamRotScale->pivot.z);
		}
	}

	pai_calcrotatedpoint(shooterObj, localSide, localUp, localFwd);
	if (shooterObj->objectType == OBJ_ImperialStarDestroyer2) {
		g_rotatedX <<= 1;
		g_rotatedY <<= 1;
		g_rotatedZ <<= 1;
	}
	g_collisionSegmentStartWorldX += g_rotatedX;
	g_collisionSegmentStartWorldY += g_rotatedY;
	g_collisionSegmentStartWorldZ += g_rotatedZ;
	muzzleWorldX = g_collisionSegmentStartWorldX;
	muzzleWorldY = g_collisionSegmentStartWorldY;
	muzzleWorldZ = g_collisionSegmentStartWorldZ;

	projectileDistance =
		lifetimeTicks * ((4660 * ((int)projectileSpeed + (int)shooterObj->mobj->speed) + 128) >> 8) / 236;
	if (shooterObj->mobj->moveVectorDirty && turretSeatIdx == -1) {
		FVIEW_calcrotatemove(shooterObj->pitch, shooterObj->yaw, shooterObj);
	}

	{
		uint8_t useConvergedAim;
		int convergeDistance;

		useConvergedAim = 0;
		if (shooterCraft->laserConvergeLevel != 0) {
			if (shooterCraft->laserConvergeLevel == 4) {
				if (targetIdx != 0xffffu) {
					ObjectRecord* targetObj;

					targetObj = &g_objectTable[targetIdx];
					if (targetObj->genusId == GENUS_Freighter || targetObj->genusId == GENUS_Starship) {
						Object_DirectionAndDistanceToMeshCenter(
							shooterObjIdx, targetObjIdx,
							(uint16_t)g_players[g_localPlayer].selectedTargetComponent);
					} else {
						pai_ObjectRefDirectionToObjectRef((uint16_t)targetObjIdx, shooterIdx);
					}
					convergeDistance = trig2_polardistance;
					if (convergeDistance < 4096) {
						convergeDistance = 4096;
					}
					if (convergeDistance > 0x10000) {
						convergeDistance = 0x10000;
					}
					useConvergedAim = 1;
					if (g_provingGroundsModeActive &&
						(targetObj->objectType == OBJ_AccelRing || targetObj->objectType == OBJ_R2D2)) {
						useConvergedAim = 0;
					}
				}
			} else {
				convergeDistance =
					g_laserConvergenceDistanceByLevel_BiasedBase[shooterCraft->laserConvergeLevel];
				useConvergedAim = 1;
			}
		}

		if (useConvergedAim) {
			int aimTargetX;
			int aimTargetY;
			int aimTargetZ;
			float deltaX;
			float deltaY;
			float deltaZ;
			float invLen;

			aimTargetX = shooterObj->world_x + Xwa_Q15Mul(convergeDistance, shooterObj->mobj->cachedFwdX);
			aimTargetY = shooterObj->world_y + Xwa_Q15Mul(convergeDistance, shooterObj->mobj->cachedFwdY);
			aimTargetZ = shooterObj->world_z + Xwa_Q15Mul(convergeDistance, shooterObj->mobj->cachedFwdZ);
			deltaX = (float)(aimTargetX - muzzleWorldX);
			deltaY = (float)(aimTargetY - muzzleWorldY);
			deltaZ = (float)(aimTargetZ - muzzleWorldZ);
			invLen = (float)(1.0 / sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ));
			aimMoveX = (int16_t)(int)(invLen * deltaX * 32768.0);
			aimMoveY = (int16_t)(int)(invLen * deltaY * 32768.0);
			aimMoveZ = (int16_t)(int)(invLen * deltaZ * 32768.0);
		} else {
			MobileObject* aimMobj;

			aimMobj = shooterObj->mobj;
			aimMoveX = aimMobj->moveX;
			aimMoveY = aimMobj->moveY;
			aimMoveZ = aimMobj->moveZ;
		}
	}

	if (turretSeatIdx == -1) {
		g_collisionProbeWorldX = g_collisionSegmentStartWorldX + Xwa_Q15Mul(projectileDistance, aimMoveX);
		g_collisionProbeWorldY = g_collisionSegmentStartWorldY + Xwa_Q15Mul(projectileDistance, aimMoveY);
		g_collisionProbeWorldZ = g_collisionSegmentStartWorldZ + Xwa_Q15Mul(projectileDistance, aimMoveZ);
	} else {
		g_collisionProbeWorldX = g_collisionSegmentStartWorldX +
								 Xwa_Q15Mul(projectileDistance, g_players[g_localPlayer].turretCamMat[0]);
		g_collisionProbeWorldY = g_collisionSegmentStartWorldY +
								 Xwa_Q15Mul(projectileDistance, g_players[g_localPlayer].turretCamMat[1]);
		g_collisionProbeWorldZ = g_collisionSegmentStartWorldZ +
								 Xwa_Q15Mul(projectileDistance, g_players[g_localPlayer].turretCamMat[2]);
	}

	if (g_objectTable[targetIdx].mobj != NULL) {
		ObjectRecord* targetObj;
		MobileObject* targetMobj;
		int targetDistance;

		targetObj = &g_objectTable[targetIdx];
		g_collisionSweepStartX = targetObj->world_x;
		g_collisionSweepStartY = targetObj->world_y;
		g_collisionSweepStartZ = targetObj->world_z;
		if ((uint16_t)g_players[g_localPlayer].selectedTargetComponent != 0) {
			pai_RotateLocalVectorToWorldScratch(
				&g_objectTable[targetIdx],
				ModelMesh_GetCenterX(g_objectTable[targetIdx].objectType,
									 (uint16_t)g_players[g_localPlayer].selectedTargetComponent),
				ModelMesh_GetCenterZ(g_objectTable[targetIdx].objectType,
									 (uint16_t)g_players[g_localPlayer].selectedTargetComponent),
				-ModelMesh_GetCenterY(g_objectTable[targetIdx].objectType,
									  (uint16_t)g_players[g_localPlayer].selectedTargetComponent));
			g_collisionSweepStartX += g_rotatedX;
			g_collisionSweepStartY += g_rotatedY;
			g_collisionSweepStartZ += g_rotatedZ;
		}

		targetMobj = targetObj->mobj;
		targetDistance = lifetimeTicks * ((4660 * (int)targetMobj->speed + 128) >> 8) / 236;
		if (targetMobj->moveVectorDirty) {
			FVIEW_calcrotatemove(targetObj->pitch, targetObj->yaw, targetObj);
		}
		g_collisionSweepEndX = g_collisionSweepStartX + Xwa_Q15Mul(targetDistance, targetObj->mobj->moveX);
		g_collisionSweepEndY = g_collisionSweepStartY + Xwa_Q15Mul(targetDistance, targetObj->mobj->moveY);
		g_collisionSweepEndZ = g_collisionSweepStartZ + Xwa_Q15Mul(targetDistance, targetObj->mobj->moveZ);
		g_collisionStagedModelProbe = 1;
		result = (uint16_t)collide_lasercraftcollide(shooterIdx, targetIdx);
	} else {
		g_collisionStagedModelProbe = 1;
		result = (uint16_t)static_laserstaticcollide(shooterIdx, targetIdx);
	}

	g_collisionSegmentStartWorldX = savedCollision.segmentStartWorldX;
	g_collisionSegmentStartWorldY = savedCollision.segmentStartWorldY;
	g_collisionSegmentStartWorldZ = savedCollision.segmentStartWorldZ;
	g_collisionProbeWorldX = savedCollision.probeWorldX;
	g_collisionProbeWorldY = savedCollision.probeWorldY;
	g_collisionProbeWorldZ = savedCollision.probeWorldZ;
	g_collisionSweepStartX = savedCollision.sweepStartX;
	g_collisionSweepStartY = savedCollision.sweepStartY;
	g_collisionSweepStartZ = savedCollision.sweepStartZ;
	g_collisionSweepEndX = savedCollision.sweepEndX;
	g_collisionSweepEndY = savedCollision.sweepEndY;
	g_collisionSweepEndZ = savedCollision.sweepEndZ;
	g_collisionHitOffsetX = savedCollision.hitOffsetX;
	g_collisionStagedModelProbe = 0;
	g_collisionHitOffsetY = savedCollision.hitOffsetY;
	g_collisionHitOffsetZ = savedCollision.hitOffsetZ;
	return result;
}

// FUNCTION: XWA 0x40DF00
uint16_t collide_craftstarshipcollision(uint16_t craftObjIdx, int lookaheadFrames) {
	uint16_t craftIdx;
	ObjectRecord* craftObj;
	MobileObject* craftMobj;
	int16_t simLookaheadScale;
	int speedScale;
	int moveScale;
	uint32_t scanObjIdx;
	MobileObjectProximityList* proximityList;
	uint16_t proximityIdx;

	craftIdx = (uint16_t)craftObjIdx;
	craftObj = &g_objectTable[craftIdx];
	simLookaheadScale = (int16_t)((int16_t)lookaheadFrames * (int16_t)g_simStepScale);

	g_collisionSegmentStartWorldX = craftObj->world_x;
	g_collisionSegmentStartWorldY = craftObj->world_y;
	g_collisionSegmentStartWorldZ = craftObj->world_z;

	craftMobj = craftObj->mobj;
	speedScale = ((int)craftMobj->speed * 4660 + 128) >> 8;
	moveScale = (int)(uint16_t)g_elapsedTicks * speedScale / 236;
	if (craftMobj->moveVectorDirty) {
		FVIEW_calcrotatemove(craftObj->pitch, craftObj->yaw, craftObj);
	}

	g_collisionProbeWorldX = g_collisionSegmentStartWorldX +
							 (int)simLookaheadScale * Xwa_Q15Mul((uint16_t)moveScale, craftObj->mobj->moveX);
	g_collisionProbeWorldY = g_collisionSegmentStartWorldY +
							 (int)simLookaheadScale * Xwa_Q15Mul((uint16_t)moveScale, craftObj->mobj->moveY);
	g_collisionProbeWorldZ = g_collisionSegmentStartWorldZ +
							 (int)simLookaheadScale * Xwa_Q15Mul((uint16_t)moveScale, craftObj->mobj->moveZ);

	scanObjIdx = g_activeRegionObjectSlotStart;
	while ((uint16_t)scanObjIdx < g_activeRegionCraftObjectSlotEnd) {
		uint16_t candidateIdx;
		ObjectRecord* candidateObj;

		candidateIdx = (uint16_t)scanObjIdx;
		candidateObj = &g_objectTable[candidateIdx];
		if (candidateObj->objectType != OBJ_None && candidateIdx != craftIdx &&
			(candidateObj->genusId == GENUS_Starship || candidateObj->genusId == GENUS_Platform ||
			 candidateObj->genusId == GENUS_Freighter || candidateObj->genusId == GENUS_Container ||
			 candidateObj->genusId == GENUS_Rubble)) {
			MobileObject* candidateMobj;
			uint16_t candidateMoveScale;

			g_collisionSweepStartX = candidateObj->world_x;
			g_collisionSweepStartY = candidateObj->world_y;
			g_collisionSweepStartZ = candidateObj->world_z;

			candidateMobj = candidateObj->mobj;
			candidateMoveScale = (uint16_t)MATH2_mphconvert(candidateMobj->speed, g_simStepScale);
			if (candidateMobj->moveVectorDirty) {
				FVIEW_calcrotatemove(candidateObj->pitch, candidateObj->yaw, candidateObj);
			}

			g_collisionSweepEndX =
				g_collisionSweepStartX +
				(int)simLookaheadScale * Xwa_Q15Mul(candidateMoveScale, candidateObj->mobj->moveX);
			g_collisionSweepEndY =
				g_collisionSweepStartY +
				(int)simLookaheadScale * Xwa_Q15Mul(candidateMoveScale, candidateObj->mobj->moveY);
			g_collisionSweepEndZ =
				g_collisionSweepStartZ +
				(int)simLookaheadScale * Xwa_Q15Mul(candidateMoveScale, candidateObj->mobj->moveZ);

			if ((uint16_t)collide_lasercraftcollide(craftIdx, candidateIdx) != 0) {
				return (uint16_t)scanObjIdx;
			}
		}

		++scanObjIdx;
	}

	proximityList = &g_objectTable[craftIdx].mobj->proximityList;
	proximityIdx = 0;
	while (proximityIdx < proximityList->count) {
		uint16_t staticObjIdx;

		staticObjIdx = proximityList->objIdx[proximityIdx];
		if ((unsigned int)staticObjIdx >= g_objScanStart &&
			staticObjIdx < (unsigned int)g_regionStaticObjectSlotEnd &&
			static_laserstaticcollide(craftIdx, staticObjIdx) != 0) {
			return staticObjIdx;
		}
		++proximityIdx;
	}

	return 0xffffu;
}

// FUNCTION: XWA 0x40C280
void collide_applyCraftImpactBounce(unsigned int sourceObjIdx, unsigned int impactObjIdx) {
	MobileObject* sourceMobj;
	MobileObject* impactMobj;
	int centrifugeImpact;
	int impactSpeed;
	uint16_t yawDelta;
	int yawMagnitude;
	int dx;
	int dy;
	int dz;
	int dxSq;
	int dySq;
	int dzSq;
	int distSq;
	int bounceX;
	int bounceY;
	int16_t sourceMoveX;
	int16_t sourceMoveY;
	Q16Angle sourceYawBefore;
	int sourceOffsetSpeed;
	int impulse;
	Q16Angle impactYawBefore;

	centrifugeImpact = 0;
	if (sourceObjIdx == 0xffffu || impactObjIdx == 0xffffu) {
		return;
	}
	if (g_objectTable[sourceObjIdx].genusId != GENUS_Fighter &&
		g_objectTable[sourceObjIdx].genusId != GENUS_Transport &&
		g_objectTable[sourceObjIdx].genusId != GENUS_PilotDroid &&
		g_objectTable[sourceObjIdx].genusId != GENUS_WeaponEmplacement &&
		g_objectTable[sourceObjIdx].genusId != GENUS_LargeScenery &&
		g_objectTable[sourceObjIdx].genusId != GENUS_Utility) {
		return;
	}

	if (g_objectTable[impactObjIdx].objectType == OBJ_Centrifuge) {
		centrifugeImpact = 1;
	}
	sourceMobj = g_objectTable[sourceObjIdx].mobj;
	sourceMobj->pCraft->aiFlight.impactObjIdx = (uint16_t)impactObjIdx;

	impactSpeed = (int)(uint16_t)sourceMobj->speed;
	yawDelta = (uint16_t)(g_objectTable[impactObjIdx].yaw - g_objectTable[sourceObjIdx].yaw);
	yawMagnitude = yawDelta;
	if ((uint16_t)yawMagnitude >= 0x8000u) {
		yawMagnitude = -yawMagnitude;
	}
	if ((uint16_t)yawMagnitude > 0x4000u) {
		yawMagnitude = 0x8000 - yawMagnitude;
	}
	if (impactObjIdx >= g_activeRegionObjectSlotStart && impactObjIdx < g_activeRegionCraftObjectSlotEnd) {
		if ((uint16_t)yawMagnitude < 0x4000u) {
			impactSpeed -=
				trig2_cosinewordmult((int)(uint16_t)g_objectTable[impactObjIdx].mobj->speed, yawMagnitude);
		} else {
			impactSpeed +=
				trig2_cosinewordmult((int)(uint16_t)g_objectTable[impactObjIdx].mobj->speed, yawMagnitude);
		}
	}
	if ((int16_t)impactSpeed < 0) {
		impactSpeed = -impactSpeed;
	}
	if ((((uint16_t)g_objectTable[sourceObjIdx].objectType >= (uint16_t)OBJ_RebelPilot &&
		  (uint16_t)g_objectTable[sourceObjIdx].objectType <= (uint16_t)OBJ_R2D2) ||
		 ((uint16_t)g_objectTable[impactObjIdx].objectType >= (uint16_t)OBJ_RebelPilot &&
		  (uint16_t)g_objectTable[impactObjIdx].objectType <= (uint16_t)OBJ_R2D2)) &&
		(int16_t)impactSpeed > 50) {
		impactSpeed = 50;
	}
	if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR && (int16_t)impactSpeed > 50) {
		impactSpeed = 50;
	}

	if (sourceMobj != NULL && (impactMobj = g_objectTable[impactObjIdx].mobj) != NULL) {
		dx = sourceMobj->prevWorldX - impactMobj->prevWorldX;
		dy = sourceMobj->prevWorldY - impactMobj->prevWorldY;
		dz = sourceMobj->prevWorldZ - impactMobj->prevWorldZ;
	} else {
		dx = g_objectTable[sourceObjIdx].world_x - g_objectTable[impactObjIdx].world_x;
		dy = g_objectTable[sourceObjIdx].world_y - g_objectTable[impactObjIdx].world_y;
		dz = g_objectTable[sourceObjIdx].world_z - g_objectTable[impactObjIdx].world_z;
	}

	dzSq = dz * dz;
	dySq = dy * dy;
	distSq = (int)((uint32_t)(dx * dx) + (uint32_t)dySq + (uint32_t)dzSq);
	dxSq = dx * dx;
	if (distSq > 50) {
		int signedImpactSpeed;

		signedImpactSpeed = (int)(int16_t)impactSpeed;
		dx *= 1000 * signedImpactSpeed;
		bounceX = dx / distSq;
		dy *= 1000 * signedImpactSpeed;
		bounceY = dy / distSq;
	} else {
		bounceX = 0;
		bounceY = 100;
	}

	if (sourceMobj->moveVectorDirty) {
		FVIEW_calcrotatemove(g_objectTable[sourceObjIdx].pitch, g_objectTable[sourceObjIdx].yaw,
							 &g_objectTable[sourceObjIdx]);
	}
	sourceMoveX = (int16_t)(sourceMobj->moveX + bounceX);
	if ((((uint16_t)(sourceMobj->moveX ^ sourceMoveX) >> 8) & 0x80u) != 0) {
		sourceMoveX = sourceMobj->moveX;
	}
	sourceMoveY = (int16_t)(sourceMobj->moveY + bounceY);
	if ((((uint16_t)(sourceMobj->moveY ^ sourceMoveY) >> 8) & 0x80u) != 0) {
		sourceMoveY = sourceMobj->moveY;
	}

	sourceYawBefore = g_objectTable[sourceObjIdx].yaw;
	g_objectTable[sourceObjIdx].yaw = trig2_arctan((int16_t)sourceMoveX, (int16_t)sourceMoveY);
	if (impactObjIdx >= g_projectileObjectSlotStart && impactObjIdx < g_projectileObjectSlotEnd) {
		sourceOffsetSpeed = (int)(int16_t)(uint16_t)(8u * g_objectTable[impactObjIdx].mobj->speed);
		if (sourceYawBefore > g_objectTable[sourceObjIdx].yaw) {
			sourceOffsetSpeed = -sourceOffsetSpeed;
		}
		g_objectTable[sourceObjIdx].yaw = (Q16Angle)(g_objectTable[sourceObjIdx].yaw + sourceOffsetSpeed);
	}

	if (g_provingGroundsModeActive) {
		if (g_objectTable[impactObjIdx].genusId == GENUS_LargeScenery ||
			g_objectTable[impactObjIdx].genusId == GENUS_SalvageJunk) {
			sourceOffsetSpeed = (int)(int16_t)(uint16_t)(8u * g_objectTable[impactObjIdx].mobj->speed);
		} else {
			sourceOffsetSpeed = (int)(int16_t)(uint16_t)(2u * g_objectTable[impactObjIdx].mobj->speed);
		}
		if (sourceYawBefore > g_objectTable[sourceObjIdx].yaw) {
			sourceOffsetSpeed = -sourceOffsetSpeed;
		}
		g_objectTable[sourceObjIdx].yaw = (Q16Angle)(g_objectTable[sourceObjIdx].yaw + sourceOffsetSpeed);
	}

	if (!centrifugeImpact && impactObjIdx >= g_activeRegionObjectSlotStart &&
		impactObjIdx < g_activeRegionCraftObjectSlotEnd) {
		g_objectTable[impactObjIdx].pitch = trig2_w_arcsin((int16_t)sourceMoveX);
		impulse = 100 * impactSpeed;
		if (g_provingGroundsModeActive || g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) {
			impulse = 50 * impactSpeed;
		}
		if ((uint16_t)impulse >= 0x8000u) {
			impulse = 0x7fff;
		}
		if (sourceYawBefore < g_objectTable[impactObjIdx].yaw) {
			impulse = -impulse;
		}
		g_objectTable[impactObjIdx].mobj->rollImpulseRate = (int16_t)impulse;
		FVIEW_calcrotatemove(g_objectTable[impactObjIdx].pitch, g_objectTable[impactObjIdx].yaw,
							 &g_objectTable[impactObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[impactObjIdx].roll, g_objectTable[impactObjIdx].angleD,
							   &g_objectTable[impactObjIdx]);
		if (g_objectTable[impactObjIdx].playerOwnerIdx == g_localPlayer) {
			ForceFeedback_PlayImpactEffect(90, impulse);
		}
	}

	impactYawBefore = sourceYawBefore;
	if (impactObjIdx >= g_activeRegionObjectSlotStart && impactObjIdx < g_activeRegionCraftObjectSlotEnd) {
		int16_t impactMoveX;
		int16_t impactMoveY;

		g_objectTable[impactObjIdx].mobj->pCraft->aiFlight.impactObjIdx = (uint16_t)sourceObjIdx;
		distSq = (int)((uint32_t)dxSq + (uint32_t)dySq + (uint32_t)dzSq);
		if (distSq > 50) {
			int signedImpactSpeed;

			signedImpactSpeed = (int)(int16_t)impactSpeed;
			bounceX = dx * (1000 * signedImpactSpeed) / distSq;
			bounceY = dy * (1000 * signedImpactSpeed) / distSq;
		} else {
			bounceX = 0;
			bounceY = 100;
		}

		if (g_objectTable[impactObjIdx].mobj->moveVectorDirty) {
			FVIEW_calcrotatemove(g_objectTable[impactObjIdx].pitch, g_objectTable[impactObjIdx].yaw,
								 &g_objectTable[impactObjIdx]);
		}
		impactMoveX = (int16_t)(g_objectTable[impactObjIdx].mobj->moveX + bounceX);
		if ((((uint16_t)(g_objectTable[impactObjIdx].mobj->moveX ^ impactMoveX) >> 8) & 0x80u) != 0) {
			impactMoveX = g_objectTable[impactObjIdx].mobj->moveX;
		}
		impactMoveY = (int16_t)(g_objectTable[impactObjIdx].mobj->moveY + bounceY);
		if ((((uint16_t)(g_objectTable[impactObjIdx].mobj->moveY ^ impactMoveY) >> 8) & 0x80u) != 0) {
			impactMoveY = g_objectTable[impactObjIdx].mobj->moveY;
		}
		impactYawBefore = g_objectTable[impactObjIdx].yaw;
		if (!centrifugeImpact) {
			g_objectTable[impactObjIdx].yaw = trig2_arctan((int16_t)impactMoveX, (int16_t)impactMoveY);
		}
		g_objectTable[impactObjIdx].pitch = trig2_w_arcsin((int16_t)impactMoveX);
	}

	impulse = 100 * impactSpeed;
	if (g_provingGroundsModeActive || g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) {
		impulse = 50 * impactSpeed;
	}
	if ((uint16_t)impulse >= 0x8000u) {
		impulse = 0x7fff;
	}
	if (impactYawBefore < g_objectTable[sourceObjIdx].yaw) {
		impulse = -impulse;
	}
	g_objectTable[sourceObjIdx].mobj->rollImpulseRate = (int16_t)impulse;
	if (g_objectTable[sourceObjIdx].playerOwnerIdx == g_localPlayer) {
		ForceFeedback_PlayImpactEffect(90, impulse);
	}
	FVIEW_calcrotatemove(g_objectTable[sourceObjIdx].pitch, g_objectTable[sourceObjIdx].yaw,
						 &g_objectTable[sourceObjIdx]);
	FVIEW_calcrotateorient(g_objectTable[sourceObjIdx].roll, g_objectTable[sourceObjIdx].angleD,
						   &g_objectTable[sourceObjIdx]);

	if (!centrifugeImpact && impactObjIdx >= g_activeRegionObjectSlotStart &&
		impactObjIdx < g_projectileObjectSlotEnd) {
		FVIEW_calcrotatemove(g_objectTable[impactObjIdx].pitch, g_objectTable[impactObjIdx].yaw,
							 &g_objectTable[impactObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[impactObjIdx].roll, g_objectTable[impactObjIdx].angleD,
							   &g_objectTable[impactObjIdx]);
	}

	fsfx_PlaySound((uint16_t)GameRand2() % 3 + 45, (uint16_t)sourceObjIdx,
				   (unsigned int)g_objectTable[sourceObjIdx].playerOwnerIdx);
	if (impactObjIdx >= g_activeRegionObjectSlotStart && impactObjIdx < g_activeRegionCraftObjectSlotEnd) {
		fsfx_PlaySound((uint16_t)GameRand2() % 3 + 45, (uint16_t)impactObjIdx,
					   (unsigned int)g_objectTable[sourceObjIdx].playerOwnerIdx);
	}
}

// FUNCTION: XWA 0x4DFC30
int collide_IntersectSegmentWithFacePlane(const float* faceNormal, const float* faceVertex,
										  const float* segmentStart, const float* segmentEnd, float* outT) {
	float endDistance;
	float startDistance;
	float t;

	endDistance =
		(segmentEnd[2] - faceVertex[2]) * faceNormal[2] +
		((segmentEnd[0] - faceVertex[0]) * faceNormal[0] + (segmentEnd[1] - faceVertex[1]) * faceNormal[1]);
	startDistance = (segmentStart[2] - faceVertex[2]) * faceNormal[2] +
					((segmentStart[0] - faceVertex[0]) * faceNormal[0] +
					 (segmentStart[1] - faceVertex[1]) * faceNormal[1]);

	if (startDistance < g_collidePlaneEpsilonPos && startDistance > g_collidePlaneEpsilonNeg) {
		startDistance = 0.0f;
	}
	if (endDistance < g_collidePlaneEpsilonPos && endDistance > g_collidePlaneEpsilonNeg) {
		endDistance = 0.0f;
	}

	if (startDistance == g_collideZeroFloat) {
		*outT = 0.0f;
		return 1;
	}
	if (endDistance == g_collideZeroFloat) {
		*outT = 1.0f;
		return 1;
	}

	if (startDistance < g_collideZeroFloat && endDistance > g_collideZeroFloat) {
		t = startDistance / (endDistance - startDistance);
		if ((*outT = t) >= g_collideZeroFloat) {
			return 1;
		}
		*outT = -t;
		return 1;
	}

	if (endDistance < g_collideZeroFloat && startDistance > g_collideZeroFloat) {
		t = startDistance / (startDistance - endDistance);
		if ((*outT = t) < g_collideZeroFloat) {
			*outT = -t;
		}
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4DFDE0
int collide_PointInFacePolygon(const float* faceNormal, const float* vertexBase, const int* faceIndices,
							   const float* point) {
	float absY;
	float absZ;
	float absX;
	float pointProjected[3];
	int axisU;
	int axisV;
	int vertex0Base;
	int vertex1Base;
	float vertex1U;
	float vertex1V;
	float vertex0U;
	float vertex0V;
	int firstEdgeNegative;
	const int* indices;
	int vertex2Base;
	float vertex2U;
	float vertex2V;
	int vertex3Index;

	memcpy(pointProjected, point, sizeof(pointProjected));
	absX = faceNormal[0];
	absY = faceNormal[1];
	absZ = faceNormal[2];
	if (absX < g_collideZeroFloat) {
		absX = -absX;
	}
	if (absY < g_collideZeroFloat) {
		absY = -absY;
	}
	if (absZ < g_collideZeroFloat) {
		absZ = -absZ;
	}

	if (absZ < absY || absZ < absX) {
		if (absY < absX || absY < absZ) {
			axisU = 1;
			axisV = 2;
		} else {
			axisU = 0;
			axisV = 2;
			pointProjected[1] = pointProjected[0];
		}
	} else {
		axisU = 0;
		axisV = 1;
		pointProjected[2] = pointProjected[1];
		pointProjected[1] = pointProjected[0];
	}

	vertex1Base = 3 * faceIndices[1];
	vertex0Base = 3 * faceIndices[0];
	vertex1U = vertexBase[axisU + vertex1Base];
	vertex1V = vertexBase[axisV + vertex1Base];
	vertex0U = vertexBase[axisU + vertex0Base];
	vertex0V = vertexBase[axisV + vertex0Base];
	g_collidePolygonEdgeCrossScratch = (pointProjected[1] - vertex0U) * (vertex1V - vertex0V) -
									   (pointProjected[2] - vertex0V) * (vertex1U - vertex0U);
	firstEdgeNegative = g_collidePolygonEdgeCrossScratch < g_collideZeroFloat;
	indices = faceIndices;

	vertex2Base = 3 * indices[2];
	vertex2U = vertexBase[axisU + vertex2Base];
	vertex2V = vertexBase[axisV + vertex2Base];
	g_collidePolygonEdgeCrossScratch = (pointProjected[1] - vertex1U) * (vertex2V - vertex1V) -
									   (pointProjected[2] - vertex1V) * (vertex2U - vertex1U);
	if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat && !firstEdgeNegative) {
		return 0;
	}
	if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
		return 0;
	}

	vertex3Index = indices[3];
	if (vertex3Index != -1) {
		int vertex3Base;
		float vertex3U;
		float vertex3V;

		vertex3Base = 3 * vertex3Index;
		vertex3U = vertexBase[axisU + vertex3Base];
		vertex3V = vertexBase[axisV + vertex3Base];
		g_collidePolygonEdgeCrossScratch = (pointProjected[1] - vertex2U) * (vertex3V - vertex2V) -
										   (pointProjected[2] - vertex2V) * (vertex3U - vertex2U);
		if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat && !firstEdgeNegative) {
			return 0;
		}
		if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
			return 0;
		}
		vertex2U = vertex3U;
		vertex2V = vertex3V;
	}

	vertex0Base = 3 * indices[0];
	vertex0U = vertexBase[axisU + vertex0Base];
	vertex0V = vertexBase[axisV + vertex0Base];
	g_collidePolygonEdgeCrossScratch = (pointProjected[1] - vertex2U) * (vertex0V - vertex2V) -
									   (pointProjected[2] - vertex2V) * (vertex0U - vertex2U);
	if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat && !firstEdgeNegative) {
		return 0;
	}
	if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
		return 0;
	}
	return 1;
}

// FUNCTION: XWA 0x4DF750
int collide_TestSweepAgainstOptNode(OptimizedPolyObject* model, OptNode* node) {
	OptNode* curNode;
	int childSelector;
	int faceIndex;
	Vec3f point;
	float axisAngle[4];
	float rotationMatrix[16];
	float hitT;
	const float* bounds;
	const int* faceIndices;
	const float* vertexBase;
	const Vec3f* faceNormal;
	Vec3f* normal;
	Vec3f* uAxisRef;
	const Vec3f* faceVertex;
	int childIndex;
	int result;

	curNode = OptModel_ResolveNodeRef(node, model);
	if (curNode == NULL) {
		return 0;
	}

	childSelector = 0;
	switch (curNode->nodeType) {
		case OPT_ROTSCALE:
			g_collideCurrentRotScaleData = (OptRotationScale*)curNode->param2;
			break;

		case OPT_FACEGROUP:
			childSelector = g_collideFaceGroupChildSelector;
			break;

		case OPT_MESHVERTS:
			g_collideCurrentMeshVertsNode = curNode;
			bounds = (const float*)curNode->param2 + 3 * ((int)curNode->param1 - 2);
			if (g_collideSweepWalkerStart.x < bounds[0]) {
				if (g_collideSweepWalkerEnd.x < bounds[0]) {
					return 1;
				}
			}
			if (g_collideSweepWalkerStart.y < bounds[1]) {
				if (g_collideSweepWalkerEnd.y < bounds[1]) {
					return 1;
				}
			}
			if (g_collideSweepWalkerStart.z < bounds[2]) {
				if (g_collideSweepWalkerEnd.z < bounds[2]) {
					return 1;
				}
			}

			bounds += 3;
			if (g_collideSweepWalkerStart.x > bounds[0]) {
				if (g_collideSweepWalkerEnd.x > bounds[0]) {
					return 1;
				}
			}
			if (g_collideSweepWalkerStart.y > bounds[1]) {
				if (g_collideSweepWalkerEnd.y > bounds[1]) {
					return 1;
				}
			}
			if (g_collideSweepWalkerStart.z > bounds[2]) {
				if (g_collideSweepWalkerEnd.z > bounds[2]) {
					return 1;
				}
			}
			break;

		case OPT_FACEDATA:
		case OPT_FACEDATA_15:
		case OPT_FACEDATA_16:
		case OPT_FACEDATA_17:
			faceIndices = (const int*)((const uint8_t*)curNode->param2 + 4);
			vertexBase = (const float*)g_collideCurrentMeshVertsNode->param2;
			faceNormal = (const Vec3f*)&faceIndices[16 * (int)curNode->param1];
			faceIndex = 0;
			if ((int)curNode->param1 <= 0) {
				break;
			}

			do {
				if (collide_IntersectSegmentWithFacePlane(&faceNormal->x, &vertexBase[3 * faceIndices[0]],
														  &g_collideSweepWalkerStart.x,
														  &g_collideSweepWalkerEnd.x, &hitT) &&
					(!g_collideSweepRejectNearStartHits || hitT >= g_collideSweepHitBackoff) &&
					hitT < g_collideSweepHitFraction) {
					point.x = (g_collideSweepWalkerEnd.x - g_collideSweepWalkerStart.x) * hitT +
							  g_collideSweepWalkerStart.x;
					point.y = (g_collideSweepWalkerEnd.y - g_collideSweepWalkerStart.y) * hitT +
							  g_collideSweepWalkerStart.y;
					point.z = (g_collideSweepWalkerEnd.z - g_collideSweepWalkerStart.z) * hitT +
							  g_collideSweepWalkerStart.z;

					if (collide_PointInFacePolygon(&faceNormal->x, vertexBase, faceIndices, &point.x)) {
						g_collideSweepHitMeshOrdinal = g_collideSweepCurrentMeshOrdinal;
						g_collideSweepHitFraction = hitT;
						g_glowMarkWorldSegmentMode = 0;

						if (g_collideCurrentMeshRotationAngle != g_collideZeroFloat) {
							float rotationAxisZ;

							normal = g_glowMarkScratchNormalVec;
							*normal = *faceNormal;
							faceVertex = (const Vec3f*)&vertexBase[3 * faceIndices[0]];
							uAxisRef = g_glowMarkScratchUAxisRefVec;
							*uAxisRef = *faceVertex;

							axisAngle[0] =
								g_collideCurrentRotScaleData->rotationAxis.x * g_collideQ15ToUnitScale;
							axisAngle[1] =
								g_collideCurrentRotScaleData->rotationAxis.y * g_collideQ15ToUnitScale;
							rotationAxisZ =
								g_collideCurrentRotScaleData->rotationAxis.z * g_collideQ15ToUnitScale;
							axisAngle[3] = g_collideCurrentMeshRotationAngle;
							axisAngle[2] = rotationAxisZ;
							Math3D_BuildAxisAngleMatrix((Matrix3x3*)rotationMatrix, axisAngle);
							Math3D_RotateVec3(g_glowMarkScratchNormalVec, (Matrix3x3*)rotationMatrix);
							Math3D_RotateVec3(g_glowMarkScratchUAxisRefVec, (Matrix3x3*)rotationMatrix);
							g_glowMarkPlaneScratch.center.x =
								(g_collideSweepLocalEnd.x - g_collideSweepLocalStart.x) * hitT +
								g_collideSweepLocalStart.x;
							g_glowMarkPlaneScratch.center.y =
								(g_collideSweepLocalEnd.y - g_collideSweepLocalStart.y) * hitT +
								g_collideSweepLocalStart.y;
							g_glowMarkPlaneScratch.center.z =
								(g_collideSweepLocalEnd.z - g_collideSweepLocalStart.z) * hitT +
								g_collideSweepLocalStart.z;
						} else {
							g_glowMarkPlaneScratch.center = point;
							normal = g_glowMarkScratchNormalVec;
							*normal = *faceNormal;
							faceVertex = (const Vec3f*)&vertexBase[3 * faceIndices[0]];
							uAxisRef = g_glowMarkScratchUAxisRefVec;
							*uAxisRef = *faceVertex;
						}
					}
				}

				++faceNormal;
				faceIndices += 16;
				++faceIndex;
			} while (faceIndex < (int)curNode->param1);
			break;

		default:
			break;
	}

	if (curNode->childCount == 0) {
		return 0;
	}
	if (childSelector != 0) {
		if (childSelector == -1) {
			return 0;
		}
		result = collide_TestSweepAgainstOptNode(model, curNode->pChildren[childSelector - 1]);
	} else {
		childIndex = 0;
		if (curNode->childCount <= 0) {
			return 0;
		}
		while (childIndex < curNode->childCount) {
			if (collide_TestSweepAgainstOptNode(model, curNode->pChildren[childIndex])) {
				return 1;
			}
			++childIndex;
		}
		return 0;
	}
	return result;
}

// FUNCTION: XWA 0x4E37A0
int collide_TestSweepAgainstOptNodeLoose(OptimizedPolyObject* model, OptNode* node) {
	int childSelector;
	int childIndex;

	node = OptModel_ResolveNodeRef(node, model);
	if (node == NULL) {
		return 0;
	}

	childSelector = 0;
	switch (node->nodeType) {
		case OPT_ROTSCALE:
			if (g_collideCurrentMeshRotationAngle != g_collideZeroFloat) {
				OptRotationScale* rotScale;
				float axisAngle[4];
				Matrix3x3 matrix;
				float rotationAxisZ;

				rotScale = (OptRotationScale*)node->param2;
				g_collideSweepWalkerStart.x -= rotScale->pivot.x;
				g_collideSweepWalkerStart.y -= rotScale->pivot.y;
				g_collideSweepWalkerStart.z -= rotScale->pivot.z;
				g_collideSweepWalkerEnd.x -= rotScale->pivot.x;
				g_collideSweepWalkerEnd.y -= rotScale->pivot.y;
				g_collideSweepWalkerEnd.z -= rotScale->pivot.z;

				axisAngle[0] = rotScale->rotationAxis.x * g_collideQ15ToUnitScale;
				axisAngle[1] = rotScale->rotationAxis.y * g_collideQ15ToUnitScale;
				rotationAxisZ = rotScale->rotationAxis.z * g_collideQ15ToUnitScale;
				axisAngle[3] = g_collideCurrentMeshRotationAngle;
				axisAngle[2] = rotationAxisZ;
				Math3D_BuildAxisAngleMatrix(&matrix, axisAngle);
				Math3D_RotateVec3(&g_collideSweepWalkerStart, &matrix);
				Math3D_RotateVec3(&g_collideSweepWalkerEnd, &matrix);

				g_collideSweepWalkerStart.x += rotScale->pivot.x;
				g_collideSweepWalkerStart.y += rotScale->pivot.y;
				g_collideSweepWalkerStart.z += rotScale->pivot.z;
				g_collideSweepWalkerEnd.x += rotScale->pivot.x;
				g_collideSweepWalkerEnd.y += rotScale->pivot.y;
				g_collideSweepWalkerEnd.z += rotScale->pivot.z;
			}
			break;

		case OPT_FACEGROUP:
			childSelector = 1;
			break;

		case OPT_MESHVERTS: {
			const float* bounds;
			const float* meshVerts;
			int meshParam1;

			g_collideCurrentMeshVertsNode = node;
			meshParam1 = (int)node->param1;
			meshVerts = (const float*)node->param2;
			bounds = meshVerts + 3 * (meshParam1 - 2);
			if (g_collideSweepWalkerStart.x < bounds[0] && g_collideSweepWalkerEnd.x < bounds[0]) {
				return 1;
			}
			if (g_collideSweepWalkerStart.y < bounds[1] && g_collideSweepWalkerEnd.y < bounds[1]) {
				return 1;
			}
			if (g_collideSweepWalkerStart.z < bounds[2] && g_collideSweepWalkerEnd.z < bounds[2]) {
				return 1;
			}

			bounds += 3;
			if (g_collideSweepWalkerStart.x > bounds[0] && g_collideSweepWalkerEnd.x > bounds[0]) {
				return 1;
			}
			if (g_collideSweepWalkerStart.y > bounds[1] && g_collideSweepWalkerEnd.y > bounds[1]) {
				return 1;
			}
			if (g_collideSweepWalkerStart.z > bounds[2] && g_collideSweepWalkerEnd.z > bounds[2]) {
				return 1;
			}
			break;
		}

		case OPT_FACEDATA:
		case OPT_FACEDATA_15:
		case OPT_FACEDATA_16:
		case OPT_FACEDATA_17: {
			const int* faceIndices;
			const float* vertexBase;
			const float* faceNormal;
			int faceIndex;

			faceIndices = (const int*)((const uint8_t*)node->param2 + 4);
			vertexBase = (const float*)g_collideCurrentMeshVertsNode->param2;
			faceNormal = (const float*)&faceIndices[16 * (int)node->param1];
			faceIndex = 0;
			if ((int)node->param1 <= 0) {
				break;
			}

			do {
				float hitT;

				if (collide_IntersectSegmentWithFacePlane(faceNormal, &vertexBase[3 * faceIndices[0]],
														  &g_collideSweepWalkerStart.x,
														  &g_collideSweepWalkerEnd.x, &hitT) &&
					(!g_collideSweepRejectNearStartHits || hitT >= g_collideSweepHitBackoff) &&
					hitT < g_collideSweepHitFraction) {
					float hitPoint[3];
					float absX;
					float absY;
					float absZ;
					int axisU;
					int axisV;
					float pointProjected[3];
					int vertex0Base;
					int vertex1Base;
					int vertex2Base;
					float vertex0U;
					float vertex0V;
					float vertex1U;
					float vertex1V;
					float vertex2U;
					float vertex2V;
					int firstEdgeNegative;
					char inside;

					hitPoint[0] = (g_collideSweepWalkerEnd.x - g_collideSweepWalkerStart.x) * hitT +
								  g_collideSweepWalkerStart.x;
					absX = faceNormal[0];
					hitPoint[1] = (g_collideSweepWalkerEnd.y - g_collideSweepWalkerStart.y) * hitT +
								  g_collideSweepWalkerStart.y;
					pointProjected[0] = hitPoint[0];
					hitPoint[2] = (g_collideSweepWalkerEnd.z - g_collideSweepWalkerStart.z) * hitT +
								  g_collideSweepWalkerStart.z;
					pointProjected[1] = hitPoint[1];
					pointProjected[2] = hitPoint[2];
					absY = faceNormal[1];
					absZ = faceNormal[2];

					if (absX < g_collideZeroFloat) {
						absX = -absX;
					}
					if (absY < g_collideZeroFloat) {
						absY = -absY;
					}
					if (absZ < g_collideZeroFloat) {
						absZ = -absZ;
					}
					if (absZ < absY || absZ < absX) {
						if (absY < absX || absY < absZ) {
							axisU = 1;
							axisV = 2;
						} else {
							axisU = 0;
							axisV = 2;
							pointProjected[1] = pointProjected[0];
						}
					} else {
						axisU = 0;
						axisV = 1;
						pointProjected[2] = pointProjected[1];
						pointProjected[1] = pointProjected[0];
					}

					vertex0Base = 3 * faceIndices[0];
					vertex1Base = 3 * faceIndices[1];
					vertex0U = vertexBase[vertex0Base + axisU];
					vertex0V = vertexBase[vertex0Base + axisV];
					vertex1U = vertexBase[vertex1Base + axisU];
					vertex1V = vertexBase[vertex1Base + axisV];
					g_collidePolygonEdgeCrossScratch =
						(pointProjected[1] - vertex0U) * (vertex1V - vertex0V) -
						(pointProjected[2] - vertex0V) * (vertex1U - vertex0U);
					firstEdgeNegative = g_collidePolygonEdgeCrossScratch < g_collideZeroFloat;

					vertex2Base = 3 * faceIndices[2];
					vertex2U = vertexBase[vertex2Base + axisU];
					vertex2V = vertexBase[vertex2Base + axisV];
					g_collidePolygonEdgeCrossScratch =
						(pointProjected[1] - vertex1U) * (vertex2V - vertex1V) -
						(pointProjected[2] - vertex1V) * (vertex2U - vertex1U);
					inside = g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat || firstEdgeNegative;
					if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
						inside = 0;
					}
					if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat) {
						g_collidePolygonEdgeCrossScratch = -g_collidePolygonEdgeCrossScratch;
					}

					if (faceIndices[3] != -1) {
						int vertex3Base;
						float vertex3U;
						float vertex3V;

						vertex3Base = 3 * faceIndices[3];
						vertex3U = vertexBase[vertex3Base + axisU];
						vertex3V = vertexBase[vertex3Base + axisV];
						g_collidePolygonEdgeCrossScratch =
							(pointProjected[1] - vertex2U) * (vertex3V - vertex2V) -
							(pointProjected[2] - vertex2V) * (vertex3U - vertex2U);
						if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat && !firstEdgeNegative) {
							inside = 0;
						}
						if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
							inside = 0;
						}
						if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat) {
							g_collidePolygonEdgeCrossScratch = -g_collidePolygonEdgeCrossScratch;
						}
						vertex2U = vertex3U;
						vertex2V = vertex3V;
					}

					vertex0Base = 3 * faceIndices[0];
					vertex0U = vertexBase[vertex0Base + axisU];
					vertex0V = vertexBase[vertex0Base + axisV];
					g_collidePolygonEdgeCrossScratch =
						(pointProjected[1] - vertex2U) * (vertex0V - vertex2V) -
						(pointProjected[2] - vertex2V) * (vertex0U - vertex2U);
					if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat && !firstEdgeNegative) {
						inside = 0;
					}
					if (g_collidePolygonEdgeCrossScratch >= g_collideZeroFloat && firstEdgeNegative) {
						inside = 0;
					}
					if (g_collidePolygonEdgeCrossScratch < g_collideZeroFloat) {
						g_collidePolygonEdgeCrossScratch = -g_collidePolygonEdgeCrossScratch;
					}

					if (inside) {
						Vec3f* normal;
						Vec3f* uAxisRef;
						const float* firstFaceVertex;

						g_collideSweepHitFraction = hitT;
						g_collideSweepHitMeshOrdinal = g_collideSweepCurrentMeshOrdinal;
						g_glowMarkPlaneScratch.center.x = hitPoint[0];
						g_glowMarkPlaneScratch.center.y = hitPoint[1];
						g_glowMarkPlaneScratch.center.z = hitPoint[2];
						normal = g_glowMarkScratchNormalVec;
						normal->x = faceNormal[0];
						normal->y = faceNormal[1];
						normal->z = faceNormal[2];
						uAxisRef = g_glowMarkScratchUAxisRefVec;
						firstFaceVertex = &vertexBase[vertex0Base];
						uAxisRef->x = firstFaceVertex[0];
						uAxisRef->y = firstFaceVertex[1];
						uAxisRef->z = firstFaceVertex[2];
						g_glowMarkWorldSegmentMode = 0;
					} else if (g_collidePolygonEdgeCrossScratch < g_collideLooseSweepEdgeThreshold) {
						const float* looseHitVertex;
						const float* firstFaceVertex;

						g_collideSweepHitFraction = hitT;
						g_collideSweepHitMeshOrdinal = g_collideSweepCurrentMeshOrdinal;
						looseHitVertex = &vertexBase[vertex1Base];
						g_glowMarkPlaneScratch.center.x = looseHitVertex[0];
						g_glowMarkPlaneScratch.center.y = looseHitVertex[1];
						g_glowMarkPlaneScratch.center.z = looseHitVertex[2];
						g_glowMarkScratchNormalVec->x = faceNormal[0];
						g_glowMarkScratchNormalVec->y = faceNormal[1];
						g_glowMarkScratchNormalVec->z = faceNormal[2];
						firstFaceVertex = &vertexBase[vertex0Base];
						g_glowMarkScratchUAxisRefVec->x = firstFaceVertex[0];
						g_glowMarkScratchUAxisRefVec->y = firstFaceVertex[1];
						g_glowMarkScratchUAxisRefVec->z = firstFaceVertex[2];
						g_glowMarkWorldSegmentMode = 0;
					}
				}

				faceNormal += 3;
				faceIndices += 16;
				++faceIndex;
			} while (faceIndex < (int)node->param1);
			break;
		}

		default:
			break;
	}

	if (node->childCount == 0) {
		return 0;
	}
	if (childSelector != 0) {
		int result;

		if (childSelector == -1) {
			return 0;
		}
		result = collide_TestSweepAgainstOptNodeLoose(model, node->pChildren[childSelector - 1]);
		return result;
	}

	childIndex = 0;
	while (childIndex < node->childCount) {
		if (collide_TestSweepAgainstOptNodeLoose(model, node->pChildren[childIndex])) {
			return 1;
		}
		++childIndex;
	}
	return 0;
}

// FUNCTION: XWA 0x4E3310
int collide_CheckLocalSweepAgainstObjectModel(unsigned int sourceObjIdx, unsigned int targetObjIdx,
											  float* localSegmentEnd, float* localSegmentStart,
											  int skipRootNodeIndex, char useExactFaceHit) {
	ObjectRecord* targetObj;
	MobileObject* targetMobj;
	MemoryHandle modelHandle;
	OptimizedPolyObject* model;
	int rootIndex;
	int meshOrdinal;

	targetObjIdx = (uint16_t)targetObjIdx;
	targetMobj = g_objectTable[targetObjIdx].mobj;
	if (targetMobj != NULL) {
		g_curCraft = targetMobj->pCraft;
	}

	g_collideSweepWalkerStart.x = localSegmentStart[0];
	g_collideSweepLocalStart.x = g_collideSweepWalkerStart.x;
	g_collideSweepWalkerStart.y = localSegmentStart[1];
	g_collideSweepLocalStart.y = g_collideSweepWalkerStart.y;
	g_collideSweepWalkerStart.z = localSegmentStart[2];
	g_collideSweepLocalStart.z = g_collideSweepWalkerStart.z;
	g_collideSweepWalkerEnd.x = localSegmentEnd[0];
	g_collideSweepLocalEnd.x = g_collideSweepWalkerEnd.x;
	g_collideSweepWalkerEnd.y = localSegmentEnd[1];
	g_collideSweepLocalEnd.y = g_collideSweepWalkerEnd.y;
	g_collideSweepWalkerEnd.z = localSegmentEnd[2];
	g_collideSweepLocalEnd.z = g_collideSweepWalkerEnd.z;

	modelHandle = g_loadedModels.byObjectType[(uint16_t)g_objectTable[targetObjIdx].objectType];
	if (modelHandle == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);
	sourceObjIdx = (uint16_t)sourceObjIdx;

	g_collidePlaneEpsilonPos = 0.000099999997f;
	g_collidePlaneEpsilonNeg = -0.000099999997f;
	g_collideSweepHitMeshOrdinal = 0;
	g_collideSweepHitFraction = 2.0f;
	g_collideSweepCurrentMeshOrdinal = 0;
	g_collideCurrentMeshVertsNode = NULL;

	meshOrdinal = 0;
	targetObj = &g_objectTable[targetObjIdx];
	for (rootIndex = 0; rootIndex < model->rootNodeCount; ++rootIndex) {
		OptNode* rootNode;
		OptNodeType nodeType;

		g_collideCurrentMeshRotationAngle = 0.0f;
		if (skipRootNodeIndex == rootIndex) {
			continue;
		}

		rootNode = model->rootNodes[rootIndex];
		nodeType = rootNode->nodeType;
		if (nodeType == OPT_TEXTURE || nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		++meshOrdinal;
		g_collideSweepCurrentMeshOrdinal = meshOrdinal;

		targetMobj = targetObj->mobj;
		if (targetMobj != NULL) {
			CraftData* craft;
			int meshIdx;
			unsigned int meshRotation;

			craft = targetMobj->pCraft;
			if (craft != NULL) {
				meshIdx = meshOrdinal - 1;
				if (craft->componentHp[meshIdx] == 0) {
					continue;
				}
				meshRotation = craft->meshRotation[meshIdx];
				if (meshRotation != 0) {
					if (!g_provingGroundsModeActive && g_objectTable[sourceObjIdx].playerOwnerIdx != -1) {
						continue;
					}
					g_collideCurrentMeshRotationAngle = meshRotation * 0.024543673f;
				}
			}
		}

		{
			MeshDescriptor* descriptor;
			int bound;

			descriptor = ModelMesh_GetObjectTypeMeshDescriptor(targetObj->objectType, meshOrdinal - 1);
			if (descriptor != NULL) {
				bound = (int)descriptor->boxMin.x;
				if (g_collideSweepWalkerEnd.x >= (float)bound ||
					g_collideSweepWalkerStart.x >= (float)bound) {
					bound = (int)descriptor->boxMin.y;
					if (g_collideSweepWalkerEnd.y >= (float)bound ||
						g_collideSweepWalkerStart.y >= (float)bound) {
						bound = (int)descriptor->boxMin.z;
						if (g_collideSweepWalkerEnd.z >= (float)bound ||
							g_collideSweepWalkerStart.z >= (float)bound) {
							bound = (int)descriptor->boxMax.x;
							if (g_collideSweepWalkerEnd.x <= (float)bound ||
								g_collideSweepWalkerStart.x <= (float)bound) {
								bound = (int)descriptor->boxMax.y;
								if (g_collideSweepWalkerEnd.y <= (float)bound ||
									g_collideSweepWalkerStart.y <= (float)bound) {
									bound = (int)descriptor->boxMax.z;
									if (g_collideSweepWalkerEnd.z <= (float)bound ||
										g_collideSweepWalkerStart.z <= (float)bound) {
										DebugPrintf((const char*)(uintptr_t)(uint16_t)targetObj->objectType,
													meshOrdinal - 1);
										if (useExactFaceHit) {
											collide_TestSweepAgainstOptNode(model, rootNode);
										} else {
											g_collideLooseSweepEdgeThreshold = 50.0f;
											collide_TestSweepAgainstOptNodeLoose(model, rootNode);
										}

										g_collideSweepWalkerStart = g_collideSweepLocalStart;
										g_collideSweepWalkerEnd = g_collideSweepLocalEnd;
									}
								}
							}
						}
					}
				}
			}
		}

		meshOrdinal = g_collideSweepCurrentMeshOrdinal;
	}

	if (g_collideSweepHitMeshOrdinal != 0) {
		g_collideSweepHitFraction -= g_collideSweepHitBackoff;
		if (g_collideSweepHitFraction < g_collideZeroFloat) {
			g_collideSweepHitFraction = 0.0f;
		}
		g_collisionHitOffsetX = (int)((double)(g_collisionProbeWorldX - g_collisionSegmentStartWorldX) *
									  g_collideSweepHitFraction);
		g_collisionHitOffsetY = (int)((double)(g_collisionProbeWorldY - g_collisionSegmentStartWorldY) *
									  g_collideSweepHitFraction);
		g_collisionHitOffsetZ = (int)((double)(g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ) *
									  g_collideSweepHitFraction);
	}

	Memory_UnlockHandle(modelHandle);
	return g_collideSweepHitMeshOrdinal;
}

// FUNCTION: XWA 0x4DE9B0
int collide_CheckSweptModelCollision(unsigned int sourceObjIdx, uint16_t targetObjIdx) {
	ObjectRecord* targetObj;
	MemoryHandle modelHandle;
	OptimizedPolyObject* model;
	unsigned int targetObjSlot;
	int auxHardpointIdx;
	int continueAuxSearch;
	int worldStart[3];
	int worldEnd[3];

	targetObjSlot = (uint16_t)targetObjIdx;
	targetObj = &g_objectTable[targetObjSlot];

	if (g_provingGroundsModeActive &&
		(uint16_t)g_objectTable[(uint16_t)sourceObjIdx].objectType >= (uint16_t)OBJ_ChuteMouth &&
		(uint16_t)g_objectTable[(uint16_t)sourceObjIdx].objectType <= (uint16_t)OBJ_ContainerGrandePG) {
		return 0;
	}

	if (targetObj->mobj != NULL) {
		g_curCraft = targetObj->mobj->pCraft;
	}

	modelHandle = g_loadedModels.byObjectType[(uint16_t)targetObj->objectType];
	if (modelHandle == 0) {
		return 0;
	}

	model = (OptimizedPolyObject*)Memory_LockHandle(modelHandle);
	OptModel_AdjustOptimizedPolyObjectPointers(model);

	g_collidePlaneEpsilonPos = 10.0f;
	g_collidePlaneEpsilonNeg = -10.0f;
	g_collideSweepAuxHardpointIdx = -1;
	auxHardpointIdx = -1;

	for (;;) {
		ObjectRecord* sourceObj;

		worldEnd[0] = g_collisionProbeWorldX;
		worldEnd[1] = g_collisionProbeWorldY;
		worldEnd[2] = g_collisionProbeWorldZ;
		worldStart[0] = g_collisionSegmentStartWorldX;
		worldStart[1] = g_collisionSegmentStartWorldY;
		worldStart[2] = g_collisionSegmentStartWorldZ;

		sourceObj = &g_objectTable[(uint16_t)sourceObjIdx];
		if (sourceObj->mobj != NULL && g_modelTypeTable[(uint16_t)sourceObj->objectType].modelIndex != -1 &&
			(((g_collideSweepAllowUnownedTargets & 1) != 0) || sourceObj->playerOwnerIdx != -1) &&
			!g_collisionStagedModelProbe) {
			ModelDef* modelDef;

			g_collideSweepAuxHardpointIdx = auxHardpointIdx + 1;
			modelDef = &g_modelDefs[(uint16_t)g_modelTypeTable[(uint16_t)sourceObj->objectType].modelIndex];
			if ((unsigned int)g_collideSweepAuxHardpointIdx >= modelDef->auxHardpointCount) {
				g_collideSweepAuxHardpointIdx = -1;
				continueAuxSearch = 0;
			} else {
				int auxHardpointIdxForVector;

				auxHardpointIdxForVector = g_collideSweepAuxHardpointIdx;
				pai_RotateLocalVectorToWorldScratch(sourceObj,
													modelDef->auxHardpoints[auxHardpointIdxForVector].x,
													modelDef->auxHardpoints[auxHardpointIdxForVector].z,
													modelDef->auxHardpoints[auxHardpointIdxForVector].y);
				worldEnd[0] += g_rotatedX;
				worldEnd[1] += g_rotatedY;
				worldEnd[2] += g_rotatedZ;
				worldStart[0] += g_rotatedX;
				worldStart[1] += g_rotatedY;
				worldStart[2] += g_rotatedZ;
				g_collideSweepAuxHardpointWorldOffsetX = g_rotatedX;
				g_collideSweepAuxHardpointWorldOffsetY = g_rotatedY;
				g_collideSweepAuxHardpointWorldOffsetZ = g_rotatedZ;
				continueAuxSearch = (unsigned int)g_collideSweepAuxHardpointIdx <
									(unsigned int)modelDef->auxHardpointCount - 1u;
			}
		} else {
			continueAuxSearch = 0;
		}

		collide_TransformSweepToModelLocal(targetObj, worldEnd, worldStart);
		g_collideSweepHitMeshOrdinal = 0;
		g_collideSweepHitFraction = 2.0f;
		g_collideSweepCurrentMeshOrdinal = 0;
		g_collideCurrentMeshVertsNode = NULL;
		collide_TestSweepAgainstModelMeshes((uint16_t)sourceObjIdx, targetObjSlot, model);
		if (g_collideSweepHitMeshOrdinal != 0) {
			break;
		}
		if (!continueAuxSearch) {
			break;
		}
		auxHardpointIdx = g_collideSweepAuxHardpointIdx;
	}

	if (g_collideSweepHitMeshOrdinal != 0) {
		g_collideSweepHitFraction -= g_collideSweepHitBackoff;
		if (g_collideSweepHitFraction < g_collideZeroFloat) {
			g_collideSweepHitFraction = 0.0f;
		}
		g_collisionHitOffsetX = (int)((double)(g_collisionProbeWorldX - g_collisionSegmentStartWorldX) *
									  g_collideSweepHitFraction);
		g_collisionHitOffsetY = (int)((double)(g_collisionProbeWorldY - g_collisionSegmentStartWorldY) *
									  g_collideSweepHitFraction);
		g_collisionHitOffsetZ = (int)((double)(g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ) *
									  g_collideSweepHitFraction);
	}

	Memory_UnlockHandle(modelHandle);
	return g_collideSweepHitMeshOrdinal;
}

// FUNCTION: XWA 0x4DF130
void collide_TestSweepAgainstModelMeshes(unsigned int sourceObjIdx, unsigned int targetObjIdx,
										 OptimizedPolyObject* model) {
	ObjectRecord* targetObj;
	int i;

	targetObj = &g_objectTable[targetObjIdx];
	g_collideFaceGroupChildSelector = 1;
	if ((g_modelTypeTable[targetObj->objectType].flags & MODEL_TYPE_FLAG_COLLISION_FACEGROUPS) != 0) {
		ObjectRecord* sourceObj;

		g_collideFaceGroupChildSelector = 2;
		sourceObj = &g_objectTable[sourceObjIdx];
		if (sourceObj->playerOwnerIdx != -1 || targetObj->playerOwnerIdx != -1 ||
			sourceObj->genusId == GENUS_PlayerProjectile) {
			g_collideFaceGroupChildSelector = 1;
		} else if (sourceObj->genusId == GENUS_NpcProjectile && sourceObj->mobj != NULL &&
				   sourceObj->mobj->pWarheadGuidance != NULL) {
			WarheadGuidanceState* guidance;

			guidance = sourceObj->mobj->pWarheadGuidance;
			if (guidance->sourcePlayerIdx != -1 || guidance->targetComponentIdx != 0xffffu) {
				g_collideFaceGroupChildSelector = 1;
			} else if (guidance->targetObjIdx != 0xffffu && guidance->targetObjIdx < 0x8000u) {
				ObjectRecord* guidanceTarget;

				guidanceTarget = &g_objectTable[guidance->targetObjIdx];
				if (guidanceTarget->objectSignature == guidance->targetSignature &&
					guidanceTarget->playerOwnerIdx != -1) {
					g_collideFaceGroupChildSelector = 1;
				}
			}
		}
	}

	for (i = 0; i < model->rootNodeCount; i++) {
		OptNode* node;

		g_collideCurrentMeshRotationAngle = 0.0f;
		node = model->rootNodes[i];
		if (node->nodeType == OPT_TEXTURE || node->nodeType == OPT_TEXTURE_REF) {
			continue;
		}

		++g_collideSweepCurrentMeshOrdinal;

		if (g_provingGroundsModeActive) {
			if (g_objectTable[sourceObjIdx].objectType == OBJ_LaserRebelTurbo_301 &&
				targetObj->objectType == OBJ_SmeltingRoom && g_collideSweepCurrentMeshOrdinal - 1 != 9) {
				continue;
			}

			{
				unsigned int targetObjectType;

				targetObjectType = (uint16_t)targetObj->objectType;
				switch (targetObjectType) {
					case OBJ_AccelRing2:
					case OBJ_AccelRing3:
						if (g_collisionStagedModelProbe && targetObj->mobj->pCraft->componentHp[4] != 0 &&
							g_collideSweepCurrentMeshOrdinal - 1 != 4) {
							continue;
						}
						break;

					default:
						break;
				}
			}
		}

		if (sourceObjIdx == targetObjIdx) {
			MeshType meshType;

			meshType =
				ModelMesh_GetObjectTypeMeshType(targetObj->objectType, g_collideSweepCurrentMeshOrdinal - 1);
			if (meshType == MESH_GunTurret || meshType == MESH_SmallGun || meshType == MESH_RotaryGunTurret) {
				continue;
			}
			if (targetObj->objectType == OBJ_SuperStarDestroyer &&
				g_collideSweepCurrentMeshOrdinal - 1 == g_collideSweepSkipSsdMeshOrdinal) {
				continue;
			}
		}

		{
			MeshDescriptor* descriptor;

			descriptor = ModelMesh_GetObjectTypeMeshDescriptor(targetObj->objectType,
															   g_collideSweepCurrentMeshOrdinal - 1);
			if (descriptor != NULL) {
				MobileObject* targetMobj;

				targetMobj = targetObj->mobj;
				if (targetMobj != NULL && targetMobj->pCraft != NULL) {
					CraftData* craft;
					unsigned int meshRotation;

					craft = targetMobj->pCraft;
					if (craft->componentHp[g_collideSweepCurrentMeshOrdinal - 1] == 0) {
						continue;
					}

					meshRotation = craft->meshRotation[g_collideSweepCurrentMeshOrdinal - 1];
					if (meshRotation != 0 && !g_provingGroundsModeActive &&
						g_objectTable[sourceObjIdx].playerOwnerIdx != -1) {
						continue;
					}

					g_collideCurrentMeshRotationAngle = meshRotation * 0.024543673f;
					if (meshRotation != 0) {
						OptRotationScale* rotScale;

						rotScale = ModelMesh_GetRotScaleData(targetObj->objectType,
															 g_collideSweepCurrentMeshOrdinal - 1);
						if (rotScale != NULL) {
							float axisAngle[4];
							Matrix3x3 matrix;

							g_collideSweepWalkerStart.x -= rotScale->pivot.x;
							g_collideSweepWalkerStart.y -= rotScale->pivot.y;
							g_collideSweepWalkerStart.z -= rotScale->pivot.z;
							g_collideSweepWalkerEnd.x -= rotScale->pivot.x;
							g_collideSweepWalkerEnd.y -= rotScale->pivot.y;
							g_collideSweepWalkerEnd.z -= rotScale->pivot.z;

							axisAngle[0] = rotScale->rotationAxis.x * g_collideQ15ToUnitScale;
							axisAngle[1] = rotScale->rotationAxis.y * g_collideQ15ToUnitScale;
							axisAngle[2] = rotScale->rotationAxis.z * g_collideQ15ToUnitScale;
							axisAngle[3] = g_collideCurrentMeshRotationAngle;
							Math3D_BuildAxisAngleMatrix(&matrix, axisAngle);
							Math3D_RotateVec3(&g_collideSweepWalkerStart, &matrix);
							Math3D_RotateVec3(&g_collideSweepWalkerEnd, &matrix);

							g_collideSweepWalkerStart.x += rotScale->pivot.x;
							g_collideSweepWalkerStart.y += rotScale->pivot.y;
							g_collideSweepWalkerStart.z += rotScale->pivot.z;
							g_collideSweepWalkerEnd.x += rotScale->pivot.x;
							g_collideSweepWalkerEnd.y += rotScale->pivot.y;
							g_collideSweepWalkerEnd.z += rotScale->pivot.z;
						}
					}
				}

				if ((g_collideSweepWalkerEnd.x >= descriptor->boxMin.x ||
					 g_collideSweepWalkerStart.x >= descriptor->boxMin.x) &&
					(g_collideSweepWalkerEnd.y >= descriptor->boxMin.y ||
					 g_collideSweepWalkerStart.y >= descriptor->boxMin.y) &&
					(g_collideSweepWalkerEnd.z >= descriptor->boxMin.z ||
					 g_collideSweepWalkerStart.z >= descriptor->boxMin.z) &&
					(g_collideSweepWalkerEnd.x <= descriptor->boxMax.x ||
					 g_collideSweepWalkerStart.x <= descriptor->boxMax.x) &&
					(g_collideSweepWalkerEnd.y <= descriptor->boxMax.y ||
					 g_collideSweepWalkerStart.y <= descriptor->boxMax.y) &&
					(g_collideSweepWalkerEnd.z <= descriptor->boxMax.z ||
					 g_collideSweepWalkerStart.z <= descriptor->boxMax.z)) {
					MobileObject* sourceMobj;

					DebugPrintf((const char*)(uintptr_t)(uint16_t)targetObj->objectType,
								g_collideSweepCurrentMeshOrdinal - 1);
					sourceMobj = g_objectTable[sourceObjIdx].mobj;
					if (g_collideUpdateCollisionObjLink) {
						if (sourceMobj != NULL && sourceObjIdx != targetObjIdx &&
							sourceMobj->collisionObjIdx != (int)targetObjIdx) {
							if (sourceMobj->collisionObjIdx != 0xffff) {
								int targetMaxExtent;
								int oldMaxExtent;

								targetMaxExtent = ModelBounds_GetMaxExtent(targetObj->objectType);
								oldMaxExtent = ModelBounds_GetMaxExtent(
									g_objectTable[sourceMobj->collisionObjIdx].objectType);
								if (targetMaxExtent < oldMaxExtent) {
									sourceMobj->collisionObjIdx = (int)targetObjIdx;
								}
							} else {
								sourceMobj->collisionObjIdx = (int)targetObjIdx;
							}
						}
					}
					collide_TestSweepAgainstOptNode(model, node);
					g_collideSweepWalkerStart = g_collideSweepLocalStart;
					g_collideSweepWalkerEnd = g_collideSweepLocalEnd;
				} else {
					DebugPrintf((const char*)(uintptr_t)(uint16_t)targetObj->objectType,
								g_collideSweepCurrentMeshOrdinal - 1);
					g_collideSweepWalkerStart = g_collideSweepLocalStart;
					g_collideSweepWalkerEnd = g_collideSweepLocalEnd;
				}
			}
		}
	}
}

// FUNCTION: XWA 0x40BFB0
int collide_GetMobileObjectProximitySpeedQ12(unsigned int objIdx) {
	MobileObject* mobj;
	int maxSpeed;
	ModelIndex modelIndex;

	mobj = g_objectTable[objIdx].mobj;
	if (mobj == NULL) {
		return 0;
	}

	switch (mobj->state) {
		case 0:
			modelIndex = GetModelIndexFromType(g_objectTable[objIdx].objectType);
			if ((uint16_t)modelIndex == 0xffffu) {
				return 0;
			}
			maxSpeed = g_modelDefs[(uint16_t)modelIndex].maxSpeed;
			if (maxSpeed < (int)g_objectTable[objIdx].mobj->speed) {
				maxSpeed = g_objectTable[objIdx].mobj->speed;
			}
			break;

		case 1:
			if (mobj->pWarheadGuidance != NULL) {
				return (int)mobj->pWarheadGuidance->minSpeed << 12;
			}
			return (int)mobj->speed << 12;

		case 6: {
			uint16_t objectType = g_objectTable[objIdx].objectType;

			if (objectType < OBJ_Junk01 || objectType > OBJ_MoltenBlock) {
				maxSpeed = 0;
				break;
			}

			modelIndex = GetModelIndexFromType(objectType);
			if ((uint16_t)modelIndex == 0xffffu) {
				return 0;
			}
			maxSpeed = g_modelDefs[(uint16_t)modelIndex].maxSpeed;
			mobj = g_objectTable[objIdx].mobj;
			if (maxSpeed < (int)mobj->speed) {
				maxSpeed = mobj->speed;
			}
			if (mobj->velocityOverrideActive) {
				maxSpeed += mobj->velocityOverrideSpeed;
			}
		} break;

		default:
			return 0;
	}

	return maxSpeed << 12;
}

// FUNCTION: XWA 0x411DE0
void collide_ConvertObjectToExplosion(unsigned int objIdx, ObjectTypeId explosionObjectType,
									  char playSfxAndFeedback) {
	float worldVelocity[3];
	float worldPos[3];

	if (g_useHardware3D && g_flightSideEffectsEnabled) {
		if (g_objRenderState[objIdx].particleEffects) {
			Particle_FreeObjectEffects((uint16_t)objIdx);
		}
		if (g_objRenderState[objIdx].trailHead) {
			ObjectTrail_FreeEmittersForObject((uint16_t)objIdx);
		}

		switch (g_objectTable[objIdx].genusId) {
			case GENUS_Fighter:
				worldPos[0] = (float)g_objectTable[objIdx].world_x;
				worldPos[1] = (float)g_objectTable[objIdx].world_y;
				worldPos[2] = (float)g_objectTable[objIdx].world_z;
				worldVelocity[0] = (float)Xwa_Q15MulReuseFirstSlot(g_objectTable[objIdx].mobj->moveX,
																   g_objectTable[objIdx].mobj->speed);
				worldVelocity[1] = (float)Xwa_Q15MulReuseFirstSlot(g_objectTable[objIdx].mobj->moveY,
																   g_objectTable[objIdx].mobj->speed);
				worldVelocity[2] = (float)Xwa_Q15MulReuseFirstSlot(g_objectTable[objIdx].mobj->moveZ,
																   g_objectTable[objIdx].mobj->speed);
				if (g_objRenderState[objIdx].drawnThisFrame) {
#ifdef XWA_MODERN
					ParticleEffect* effect =
						Particle_CreateWorldEffect(6, (const Vec3f*)worldPos, (const Vec3f*)worldVelocity);
					const int32_t preciseWorld[3] = { g_objectTable[objIdx].world_x,
													  g_objectTable[objIdx].world_y,
													  g_objectTable[objIdx].world_z };
					Particle_SetWorldEffectPreciseOrigin(effect, preciseWorld);
#else
					Particle_CreateWorldEffect(6, (const Vec3f*)worldPos, (const Vec3f*)worldVelocity);
#endif
				}
				break;

			case GENUS_Transport:
			case GENUS_Freighter:
			case GENUS_Starship:
			case GENUS_Platform:
			case GENUS_Container:
			case GENUS_Rubble:
				worldPos[0] = (float)g_objectTable[objIdx].world_x;
				worldPos[1] = (float)g_objectTable[objIdx].world_y;
				worldPos[2] = (float)g_objectTable[objIdx].world_z;
				worldVelocity[0] = (float)Xwa_Q15MulReuseFirstSlot(g_objectTable[objIdx].mobj->moveX,
																   g_objectTable[objIdx].mobj->speed);
				worldVelocity[1] = (float)Xwa_Q15MulReuseFirstSlot(g_objectTable[objIdx].mobj->moveY,
																   g_objectTable[objIdx].mobj->speed);
				(void)Xwa_Q15MulReuseFirstSlot(g_objectTable[objIdx].mobj->moveZ,
											   g_objectTable[objIdx].mobj->speed);
				break;
		}
	}

	g_objectTable[objIdx].mobj->instanceExtent =
		g_modelTypeTable[(uint16_t)g_objectTable[objIdx].objectType].maxBoundsExtent;
	g_objectTable[objIdx].objectType = explosionObjectType;
	g_objectTable[objIdx].genusId = GENUS_Explosion;
	g_objectTable[objIdx].mobj->state = 5;
	g_objectTable[objIdx].typeSpecificByte[0] = 1;
	g_objectTable[objIdx].mobj->sourceObjIdx = -1;
	g_objectTable[objIdx].mobj->framesAlive = 0;
	g_objectTable[objIdx].mobj->lifetimeTimer = 0;
	g_objectTable[objIdx].roll = 0;
	g_objectTable[objIdx].angleD = 0;
	g_objectTable[objIdx].mobj->rollImpulseRate = 0;
	g_objectTable[objIdx].mobj->spinRate = 0;
	g_objectTable[objIdx].mobj->spinRateFrac = 0;
	g_objectTable[objIdx].mobj->velocityOverrideActive = 0;
	if (g_objectTable[objIdx].mobj->pCraft) {
		g_objectTable[objIdx].mobj->pCraft->breakupYawRate = 0;
		g_objectTable[objIdx].mobj->pCraft->breakupPitchRate = 0;
	}
	g_objectTable[objIdx].mobj->spinAngleQ16 = 0;
	g_objectTable[objIdx].mobj->orientMatrixDirty = 1;

	if (playSfxAndFeedback) {
		fsfx_PlaySound(fsfx_PickRandomSmallExplosionSfx(), objIdx, (unsigned int)g_localPlayer);
		ForceFeedback_PlayProximityEffectForObject(1, objIdx);
	}
}

// FUNCTION: XWA 0x40BD20
void collide_InsertMobileObjectProximityCandidate(MobileObjectProximityList* list, unsigned int ownerObjIdx,
												  unsigned int candidateObjIdx) {
	ObjectRecord* owner;
	int score;
	int clearance;
	int distance;
	uint8_t count;
	int insertIndex;
	int i;
	int entryCount;

	owner = &g_objectTable[ownerObjIdx];
	distance = collide_roughdistance3d_inline(owner->world_x - g_objectTable[candidateObjIdx].world_x,
											  owner->world_y - g_objectTable[candidateObjIdx].world_y,
											  owner->world_z - g_objectTable[candidateObjIdx].world_z);

	clearance = g_modelTypeTable[(uint16_t)owner->objectType].maxBoundsExtent;
	if (distance >= clearance) {
		clearance = distance - clearance;
	} else {
		clearance = -1;
	}
	distance = g_modelTypeTable[(uint16_t)g_objectTable[candidateObjIdx].objectType].maxBoundsExtent;
	if (clearance >= distance) {
		clearance -= distance;
	} else {
		clearance = -1;
	}
	if (clearance < 0) {
		score = 0;
	} else {
		score = collide_GetMobileObjectProximitySpeedQ12(ownerObjIdx) +
				collide_GetMobileObjectProximitySpeedQ12(candidateObjIdx);
		clearance >>= 8;
		score >>= 8;
		if (score == 0) {
			return;
		}

		score = 13275 * clearance / score;
	}

	if (g_objectTable[candidateObjIdx].objectType == OBJ_TIEBomb &&
		ownerObjIdx == g_objectTable[candidateObjIdx].mobj->pCraft->aiController.targetObjIdx) {
		score -= 1000;
	}

	insertIndex = 0;
	entryCount = list->count;
	count = list->count;
	while (insertIndex < entryCount) {
		if (list->objIdx[insertIndex] == candidateObjIdx) {
			list->score[insertIndex] = score;

			if (insertIndex + 1 < list->count) {
				++insertIndex;
				while (list->score[insertIndex] < score) {
					list->score[insertIndex - 1] = list->score[insertIndex];
					list->objIdx[insertIndex - 1] = list->objIdx[insertIndex];
					list->score[insertIndex] = score;
					list->objIdx[insertIndex] = (uint16_t)candidateObjIdx;
					++insertIndex;
					if (insertIndex == list->count) {
						break;
					}
				}
				--insertIndex;
			}

			if (insertIndex > 0) {
				--insertIndex;
				while (list->score[insertIndex] > score) {
					list->score[insertIndex + 1] = list->score[insertIndex];
					list->objIdx[insertIndex + 1] = list->objIdx[insertIndex];
					list->score[insertIndex] = score;
					list->objIdx[insertIndex] = (uint16_t)candidateObjIdx;
					--insertIndex;
					if (insertIndex < 0) {
						break;
					}
				}
			}

			return;
		}
		if (score < list->score[insertIndex]) {
			break;
		}
		++insertIndex;
	}

	if (insertIndex == entryCount) {
		if (count == 16) {
			if (score < list->overflowScore) {
				list->overflowScore = score;
			}
			return;
		}

		list->score[insertIndex] = score;
		list->objIdx[insertIndex] = candidateObjIdx;
		list->count = (uint8_t)(list->count + 1);
		return;
	}

	if (entryCount == 16) {
		if (list->score[15] < list->overflowScore) {
			list->overflowScore = list->score[15];
		}
		--count;
		--entryCount;
		list->count = count;
	}

	for (i = entryCount; i > insertIndex; --i) {
		list->score[i] = list->score[i - 1];
		list->objIdx[i] = list->objIdx[i - 1];
	}

	list->score[insertIndex] = score;
	list->objIdx[insertIndex] = candidateObjIdx;
	list->count = (uint8_t)(list->count + 1);
}

// FUNCTION: XWA 0x4083B0
void collide_RefreshMobileObjectProximityCandidates(MobileObjectProximityList* list,
													unsigned int ownerObjIdx) {
	CraftData* ownerCraft;
	MobileObject* ownerMobj;
	ObjectRecord* ownerObj;
	uint16_t targetObjIdx;
	uint32_t scanObjIdx;
	uint16_t sourceObjIdx;
	int ownerGenus;

	ownerMobj = g_objectTable[ownerObjIdx].mobj;
	ownerCraft = ownerMobj->pCraft;
	ownerObj = &g_objectTable[ownerObjIdx];

	if (ownerObj->playerOwnerIdx != -1) {
		if (ownerCraft->workingSubsystems != 0) {
			unsigned int candidateObjIdx;

			scanObjIdx = g_activeRegionObjectSlotStart;
			candidateObjIdx = (uint16_t)scanObjIdx;
			while (candidateObjIdx < g_activeRegionCraftObjectSlotEnd) {
				if (g_objectTable[candidateObjIdx].objectType != OBJ_None && candidateObjIdx != ownerObjIdx &&
					g_objectTable[candidateObjIdx].genusId != GENUS_Explosion &&
					(!g_provingGroundsModeActive ||
					 !Yard_ShouldSuppressProximityPair(ownerObjIdx, candidateObjIdx)) &&
					g_objectTable[candidateObjIdx].flightGroupIdx != ownerObj->flightGroupIdx &&
					ownerCraft->carriedObjectIndex != (uint16_t)scanObjIdx &&
					ownerCraft->lastReleasedObjectIdx != (uint16_t)scanObjIdx &&
					ownerCraft->linkedPrevObjectIdx != (uint16_t)scanObjIdx &&
					ownerCraft->nextLinkObjectIdx != (uint16_t)scanObjIdx) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				}
				++scanObjIdx;
				candidateObjIdx = (uint16_t)scanObjIdx;
			}

			scanObjIdx = g_salvageJunkObjectSlotStart;
			candidateObjIdx = (uint16_t)scanObjIdx;
			while (candidateObjIdx < g_debrisObjectSlotStart) {

				if (g_objectTable[candidateObjIdx].objectType != OBJ_None) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				}
				++scanObjIdx;
				candidateObjIdx = (uint16_t)scanObjIdx;
			}
		}

		if (!g_provingGroundsModeActive) {
			unsigned int candidateObjIdx;

			scanObjIdx = g_objScanStart;
			candidateObjIdx = (uint16_t)scanObjIdx;
			for (; candidateObjIdx < g_regionStaticObjectSlotEnd;
				 ++scanObjIdx, candidateObjIdx = (uint16_t)scanObjIdx) {
				if (g_objectTable[candidateObjIdx].objectType != OBJ_None) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
				}
			}
		}
		return;
	}

	ownerGenus = ownerObj->genusId;
	sourceObjIdx = (uint16_t)ownerMobj->sourceObjIdx;
	switch (ownerGenus) {
		case GENUS_Freighter:
		case GENUS_Starship:
		case GENUS_Platform:
		case GENUS_Container:
		case GENUS_Rubble:
			if (ownerCraft->carrierObjIdx == 0xffffu &&
				pai_GetEffectiveAIController(ownerCraft)->maneuverMode != 30) {
				for (scanObjIdx = g_activeRegionObjectSlotStart;
					 (uint16_t)scanObjIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjIdx) {
					unsigned int candidateObjIdx;

					candidateObjIdx = (uint16_t)scanObjIdx;
					if (g_objectTable[candidateObjIdx].objectType == OBJ_None ||
						candidateObjIdx == ownerObjIdx) {
						continue;
					}

					if (g_objectTable[candidateObjIdx].playerOwnerIdx != -1) {
						if (g_objectTable[candidateObjIdx].mobj != NULL) {
							collide_InsertMobileObjectProximityCandidate(
								&g_objectTable[candidateObjIdx].mobj->proximityList, candidateObjIdx,
								ownerObjIdx);
						}
						continue;
					}

					{
						CraftData* candidateCraft = g_objectTable[candidateObjIdx].mobj->pCraft;
						if (((ownerCraft->linkedPrevObjectIdx == 0xffffu &&
							  ownerCraft->nextLinkObjectIdx == 0xffffu) ||
							 (candidateCraft->linkedPrevObjectIdx == 0xffffu &&
							  candidateCraft->nextLinkObjectIdx == 0xffffu)) &&
							g_objectTable[candidateObjIdx].genusId != GENUS_Explosion) {
							collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
							switch (g_objectTable[candidateObjIdx].genusId) {
								case GENUS_Freighter:
								case GENUS_Starship:
								case GENUS_Platform:
								case GENUS_LargeScenery:
								case GENUS_Container:
								case GENUS_WeaponEmplacement:
								case GENUS_Rubble: {
									MobileObject* reverseMobj;

									reverseMobj = g_objectTable[candidateObjIdx].mobj;
									if (reverseMobj != NULL) {
										CraftData* reverseCraft;

										reverseCraft = reverseMobj->pCraft;
										if (reverseCraft != NULL && reverseCraft->carrierObjIdx == 0xffffu &&
											pai_GetEffectiveAIController(reverseCraft)->maneuverMode != 30) {
											collide_InsertMobileObjectProximityCandidate(
												&g_objectTable[candidateObjIdx].mobj->proximityList,
												candidateObjIdx, ownerObjIdx);
										}
									}
									break;
								}
								default:
									break;
							}
						}
					}
				}
			}
			return;

		case GENUS_Fighter:
		case GENUS_Transport:
		case GENUS_Utility:
		case GENUS_PilotDroid:
		case GENUS_WeaponEmplacement:
			for (scanObjIdx = g_activeRegionObjectSlotStart;
				 (uint16_t)scanObjIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjIdx) {
				unsigned int candidateObjIdx;

				candidateObjIdx = (uint16_t)scanObjIdx;
				if (g_objectTable[candidateObjIdx].objectType == OBJ_None) {
					continue;
				}

				if (g_provingGroundsModeActive) {
					if (Yard_ShouldSuppressProximityPair(ownerObjIdx, candidateObjIdx)) {
						continue;
					}
				} else if (g_objectTable[candidateObjIdx].flightGroupIdx == ownerObj->flightGroupIdx) {
					continue;
				}

				if (g_objectTable[candidateObjIdx].playerOwnerIdx != -1 &&
					g_objectTable[candidateObjIdx].mobj != NULL) {
					collide_InsertMobileObjectProximityCandidate(
						&g_objectTable[candidateObjIdx].mobj->proximityList, candidateObjIdx, ownerObjIdx);
				}
				switch (g_objectTable[candidateObjIdx].genusId) {
					case GENUS_Freighter:
					case GENUS_Starship:
					case GENUS_Platform:
					case GENUS_LargeScenery:
					case GENUS_Container:
					case GENUS_WeaponEmplacement:
					case GENUS_Rubble:
						if (ownerObjIdx != candidateObjIdx) {
							MobileObject* candidateMobj;

							candidateMobj = g_objectTable[candidateObjIdx].mobj;
							if (candidateMobj != NULL) {
								CraftData* candidateCraft;

								candidateCraft = candidateMobj->pCraft;
								if (candidateCraft != NULL && candidateCraft->carrierObjIdx == 0xffffu &&
									pai_GetEffectiveAIController(candidateCraft)->maneuverMode != 30) {
									collide_InsertMobileObjectProximityCandidate(
										&g_objectTable[candidateObjIdx].mobj->proximityList, candidateObjIdx,
										ownerObjIdx);
								}
							}
						}
						break;
					default:
						break;
				}
			}

			if (!g_provingGroundsModeActive) {
				unsigned int candidateObjIdx;

				scanObjIdx = g_objScanStart;
				candidateObjIdx = (uint16_t)scanObjIdx;
				for (; candidateObjIdx < g_regionStaticObjectSlotEnd;
					 ++scanObjIdx, candidateObjIdx = (uint16_t)scanObjIdx) {
					uint16_t candidateType;
					ObjectRecord* candidateObj;

					candidateObj = &g_objectTable[candidateObjIdx];
					candidateType = (uint16_t)candidateObj->objectType;
					if (candidateType != OBJ_None && (candidateObj->genusId == GENUS_DeathStarTunnelSegment ||
													  ((uint16_t)candidateType >= OBJ_AsteroidHR1 &&
													   (uint16_t)candidateType <= OBJ_AsteroidHR6))) {
						collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
					}
				}
			} else if ((int16_t)ownerGenus == GENUS_PilotDroid) {
				for (scanObjIdx = g_salvageJunkObjectSlotStart;
					 (uint16_t)scanObjIdx < g_debrisObjectSlotStart; ++scanObjIdx) {
					ObjectRecord* candidateObj;
					unsigned int candidateObjIdx;

					candidateObjIdx = (uint16_t)scanObjIdx;
					candidateObj = &g_objectTable[candidateObjIdx];
					if (candidateObj->objectType != OBJ_None && ownerObjIdx != candidateObjIdx) {
						switch (candidateObj->objectType) {
							case OBJ_ChuteMouth:
							case OBJ_ChuteTunnel:
							case OBJ_SalvageRoom:
							case OBJ_SRTubeNOBend:
							case OBJ_SRTubeUP:
							case OBJ_SRTubeDown:
							case OBJ_SRTubeLH:
							case OBJ_SRTubeRH:
							case OBJ_ContainerGrandePG:
							case OBJ_Asteroid01:
							case OBJ_Asteroid02:
							case OBJ_JunkBlock:
							case OBJ_MoltenBlock:
								collide_InsertMobileObjectProximityCandidate(
									&candidateObj->mobj->proximityList, candidateObjIdx, ownerObjIdx);
								break;
							default:
								break;
						}
					}
				}
			}
			return;

		case GENUS_PlayerProjectile:
		case GENUS_NpcProjectile: {
			unsigned int candidateObjIdx;

			targetObjIdx = ownerMobj->pWarheadGuidance->targetObjIdx;
			scanObjIdx = g_activeRegionObjectSlotStart;
			candidateObjIdx = (uint16_t)scanObjIdx;
			for (; candidateObjIdx < g_debrisObjectSlotStart;
				 ++scanObjIdx, candidateObjIdx = (uint16_t)scanObjIdx) {
				uint16_t candidateType;
				ObjectRecord* candidateObj;
				int targetIsPlayerCraft;
				int sourceIsPlayerControlled;

				candidateObj = &g_objectTable[candidateObjIdx];
				candidateType = (uint16_t)candidateObj->objectType;
				if (candidateType == OBJ_None) {
					continue;
				}

				if (candidateObjIdx >= g_salvageJunkObjectSlotStart) {
					collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
					continue;
				}

#ifdef XWA_MODERN
				// Impact effects can remain in projectile slots after the projectile is converted
				// in place. The original lookup reads adjacent globals for those non-projectile
				// object types; reject them before indexing the bounded modern table.
				if (candidateObjIdx >= g_projectileObjectSlotStart &&
					((uint16_t)candidateType < OBJ_LaserRebel ||
					 (uint16_t)candidateType > OBJ_LaserImperialDS)) {
					continue;
				}
#endif

				if (scanObjIdx >= g_projectileObjectSlotStart &&
					(g_projectileWarheadClassByType[(uint16_t)candidateType - OBJ_LaserRebel] == 0 ||
					 candidateObjIdx == ownerObjIdx ||
					 candidateObj->mobj->sourceObjIdx == (int16_t)sourceObjIdx)) {
					continue;
				}

				if (g_provingGroundsModeActive && candidateObjIdx == sourceObjIdx &&
					g_objectTable[sourceObjIdx].objectType == OBJ_SmeltingRoom) {
					targetIsPlayerCraft = 1;
				} else {
					if (candidateObjIdx == sourceObjIdx || candidateObj->genusId == GENUS_Explosion ||
						candidateType == OBJ_NavBuoy3 || candidateType == OBJ_HyperBuoy) {
						continue;
					}

					targetIsPlayerCraft = 0;
					if (targetObjIdx < g_activeRegionCraftObjectSlotEnd &&
						targetObjIdx >= g_activeRegionObjectSlotStart &&
						g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
						targetIsPlayerCraft = 1;
					}
				}

				sourceIsPlayerControlled = 0;
				if (sourceObjIdx != 0xffffu) {
					if (g_objectTable[sourceObjIdx].playerOwnerIdx != -1) {
						sourceIsPlayerControlled = 1;
					} else {
						uint16_t playerIdx;

						for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
							if (g_players[playerIdx].objectIndex == sourceObjIdx) {
								sourceIsPlayerControlled = 1;
								break;
							}
						}
					}
				}
				if (!targetIsPlayerCraft && sourceObjIdx != 0xffffu && !sourceIsPlayerControlled &&
					targetObjIdx != candidateObjIdx) {
					continue;
				}

				collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
			}

			if (!g_provingGroundsModeActive) {
				for (scanObjIdx = g_objScanStart; (uint16_t)scanObjIdx < g_regionStaticObjectSlotEnd;
					 ++scanObjIdx) {
					if (g_objectTable[(uint16_t)scanObjIdx].objectType != OBJ_None) {
						collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, (uint16_t)scanObjIdx);
					}
				}
			}
			return;
		}

		case GENUS_SalvageJunk:
			if ((uint16_t)ownerObj->objectType >= OBJ_Junk01 &&
				(uint16_t)ownerObj->objectType <= OBJ_Junk10) {
				for (scanObjIdx = g_activeRegionObjectSlotStart;
					 (uint16_t)scanObjIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjIdx) {
					uint16_t candidateType;

					candidateType = (uint16_t)g_objectTable[(uint16_t)scanObjIdx].objectType;
					if (candidateType != (uint16_t)OBJ_None && candidateType == (uint16_t)OBJ_Compactor) {
						collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, (uint16_t)scanObjIdx);
						break;
					}
				}

				for (scanObjIdx = g_salvageJunkObjectSlotStart;
					 (uint16_t)scanObjIdx < g_debrisObjectSlotStart; ++scanObjIdx) {
					unsigned int candidateObjIdx;

					candidateObjIdx = (uint16_t)scanObjIdx;
					if (g_objectTable[candidateObjIdx].objectType != OBJ_None &&
						ownerObjIdx != candidateObjIdx) {
						switch (g_objectTable[candidateObjIdx].objectType) {
							case OBJ_ChuteMouth:
							case OBJ_ChuteTunnel:
							case OBJ_SalvageRoom:
							case OBJ_Junk01:
							case OBJ_Junk02:
							case OBJ_Junk03:
							case OBJ_Junk04:
							case OBJ_Junk05:
							case OBJ_Junk06:
							case OBJ_Junk07:
							case OBJ_Junk08:
							case OBJ_Junk09:
							case OBJ_Junk10:
							case OBJ_JunkBlock:
								collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx,
																			 candidateObjIdx);
								break;
							default:
								break;
						}
					}
				}
			}

			if (!g_provingGroundsModeActive) {
				unsigned int candidateObjIdx;

				scanObjIdx = g_objScanStart;
				candidateObjIdx = (uint16_t)scanObjIdx;
				for (; candidateObjIdx < g_regionStaticObjectSlotEnd;
					 ++scanObjIdx, candidateObjIdx = (uint16_t)scanObjIdx) {
					ObjectRecord* candidateObj;
					ObjectTypeId candidateType;

					candidateObj = &g_objectTable[candidateObjIdx];
					candidateType = candidateObj->objectType;
					if (candidateType != OBJ_None && (candidateObj->genusId == GENUS_DeathStarTunnelSegment ||
													  ((uint16_t)candidateType >= OBJ_AsteroidHR1 &&
													   (uint16_t)candidateType <= OBJ_AsteroidHR6))) {
						collide_InsertMobileObjectProximityCandidate(list, ownerObjIdx, candidateObjIdx);
					}
				}
			}
			return;

		default:
			return;
	}
}

// FUNCTION: XWA 0x40C0E0
void collide_ResetObjectProximityForSlot(uint16_t objIdx) {
	MobileObject* mobj;
	uint32_t scanObjIdx;

	mobj = g_objectTable[objIdx].mobj;
	if (mobj != NULL) {
		mobj->proximityList.overflowScore = 0;
		mobj->proximityList.count = 0;
		return;
	}

	for (scanObjIdx = g_activeRegionObjectSlotStart; scanObjIdx < g_activeRegionCraftObjectSlotEnd;
		 ++scanObjIdx) {
		ObjectRecord* scanObj = &g_objectTable[scanObjIdx];

		if (scanObj->objectType != 0 && scanObj->playerOwnerIdx != -1 && scanObj->mobj != NULL) {
			collide_InsertMobileObjectProximityCandidate(&scanObj->mobj->proximityList, (uint16_t)scanObjIdx,
														 objIdx);
		}
	}
}

// FUNCTION: XWA 0x40C180
void collide_ResetNeighborProximityLists(uint16_t objIdx) {
	MobileObject* mobj;
	int count;
	int i;

	mobj = g_objectTable[objIdx].mobj;
	if (mobj == NULL) {
		return;
	}

	count = mobj->proximityList.count;
	for (i = 0; i < count; ++i) {
		uint16_t neighborObjIdx = g_objectTable[objIdx].mobj->proximityList.objIdx[i];
		MobileObject* neighborMobj = g_objectTable[neighborObjIdx].mobj;
		uint32_t scanObjIdx;

		if (neighborMobj != NULL) {
			neighborMobj->proximityList.overflowScore = 0;
			g_objectTable[neighborObjIdx].mobj->proximityList.count = 0;
			continue;
		}

		for (scanObjIdx = g_activeRegionObjectSlotStart; scanObjIdx < g_activeRegionCraftObjectSlotEnd;
			 ++scanObjIdx) {
			ObjectRecord* scanObj = &g_objectTable[scanObjIdx];

			if (scanObj->objectType != 0 && scanObj->playerOwnerIdx != -1 && scanObj->mobj != NULL) {
				collide_InsertMobileObjectProximityCandidate(&scanObj->mobj->proximityList,
															 (uint16_t)scanObjIdx, neighborObjIdx);
			}
		}
	}
}

// FUNCTION: XWA 0x40CCA0
int collide_GetSweptHitRadius(unsigned int attackerObjIdx, unsigned int targetObjIdx,
							  int* allowSimpleBoxHit) {
	uint16_t targetType;
	int radius;
	ObjectRecord* objects;

	*allowSimpleBoxHit = 1;

	objects = g_objectTable;
	targetType = objects[targetObjIdx].objectType;
	radius = g_modelTypeTable[(uint16_t)targetType].maxBoundsExtent;

	if (objects[targetObjIdx].genusId == GENUS_NpcProjectile ||
		objects[targetObjIdx].genusId == GENUS_PlayerProjectile) {
		if (targetType == OBJ_WarheadSpaceBomb) {
			radius <<= 4;
			*allowSimpleBoxHit = 0;
		} else if (targetType != OBJ_WarheadFlare) {
			radius >>= 1;
		} else {
			*allowSimpleBoxHit = 0;
		}
		return radius;
	}

	if (objects[targetObjIdx].genusId != GENUS_Fighter) {
		return radius;
	}

	if (objects[attackerObjIdx].genusId != GENUS_NpcProjectile &&
		objects[attackerObjIdx].genusId != GENUS_PlayerProjectile) {
		return radius;
	}

	if (objects[targetObjIdx].mobj->pCraft->shieldRear + objects[targetObjIdx].mobj->pCraft->shieldFront !=
		0) {
		radius += radius >> 1;
		if (radius > 1095) {
			radius = 1094;
		}
	}

	if (!g_asyncFlag || objects[targetObjIdx].playerOwnerIdx == -1 ||
		objects[attackerObjIdx].genusId != GENUS_PlayerProjectile ||
#ifdef XWA_MODERN
		laser_GetProjectileWarheadClass(objects[attackerObjIdx].objectType) != 0) {
#else
		g_projectileWarheadClassByType[(uint16_t)objects[attackerObjIdx].objectType - OBJ_LaserRebel] != 0) {
#endif
		return radius;
	}

	switch (targetType) {
		case OBJ_AWing:
		case OBJ_TIEInterceptor:
		case OBJ_TIEAdvanced:
			radius *= 2;
			break;

		case OBJ_TIEFighter:
			radius += radius / 4 + radius / 2;
			break;

		case OBJ_YWing:
		case OBJ_BWing:
			break;

		default:
			radius += radius / 2;
			break;
	}

	*allowSimpleBoxHit = 0;
	return radius;
}

// FUNCTION: XWA 0x4E2E40
void collide_ApplyHostileProximityWeaponDisruption(int ownerObjIdx, int hostileObjIdx) {
	ObjectRecord* hostileObj;
	CraftData* hostileCraft;
	uint16_t hostileType;
	int16_t modelIndex;
	int deltaX;
	int deltaY;
	int targetBoundsExtent;
	int localUp;
	int withinDisruptionPoint;
	int disruptionRadius;
	int localSide;
	int localFwd;
	int attachSide;
	int attachFwd;
	int attachUp;
	int deltaZ;

	hostileObj = &g_objectTable[hostileObjIdx];
	withinDisruptionPoint = 0;
	targetBoundsExtent = g_modelTypeTable[(uint16_t)hostileObj->objectType].maxBoundsExtent;
	deltaX = g_objectTable[ownerObjIdx].world_x - hostileObj->world_x;
	deltaY = g_objectTable[ownerObjIdx].world_y - hostileObj->world_y;
	deltaZ = g_objectTable[ownerObjIdx].world_z - hostileObj->world_z;
	hostileObjIdx = collide_roughdistance3d(deltaX, deltaY, deltaZ);
	if (hostileObjIdx > 2 * targetBoundsExtent) {
		return;
	}

	if (hostileObj->mobj == NULL) {
		return;
	}

	hostileCraft = hostileObj->mobj->pCraft;
	if (hostileCraft == NULL) {
		return;
	}
	if (hostileCraft->workingSubsystems == 0) {
		return;
	}
	if (hostileCraft->modelIndex == 0xffffu) {
		return;
	}

	hostileType = hostileObj->objectType;
	if (hostileType == OBJ_ImperialResearchCenter) {
		return;
	}

	modelIndex = GetModelIndexFromType(hostileType);
	if ((uint16_t)modelIndex == 0xffffu) {
		return;
	}

	if (hostileObj->objectType == OBJ_Factory) {
		if (hostileObjIdx <= 4096) {
		} else {
			return;
		}
	} else {

		{
			MobileObject* hostileMobj;

			hostileMobj = hostileObj->mobj;
			if (hostileMobj->orientMatrixDirty) {
				FVIEW_calcrotatemove(hostileObj->pitch, hostileObj->yaw, hostileObj);
				FVIEW_calcrotateorient(hostileObj->roll, hostileObj->angleD, hostileObj);
			}
		}

		localSide = collide_DotQ15(deltaX, deltaY, deltaZ, hostileObj->mobj->cachedSideX,
								   hostileObj->mobj->cachedSideY, hostileObj->mobj->cachedSideZ);
		localFwd = collide_DotQ15(deltaX, deltaY, deltaZ, hostileObj->mobj->cachedFwdX,
								  hostileObj->mobj->cachedFwdY, hostileObj->mobj->cachedFwdZ);
		localUp = collide_DotQ15(deltaX, deltaY, deltaZ, hostileObj->mobj->cachedUpX,
								 hostileObj->mobj->cachedUpY, hostileObj->mobj->cachedUpZ);

		attachSide = g_modelDefs[(uint16_t)modelIndex].meshAttachData[5];
		attachUp = g_modelDefs[(uint16_t)modelIndex].meshAttachData[6];
		attachFwd = g_modelDefs[(uint16_t)modelIndex].meshAttachData[7];

		if (hostileObj->objectType == OBJ_ImperialStarDestroyer2 ||
			hostileObj->objectType == OBJ_Interdictor2 ||
			hostileObj->objectType == OBJ_VictoryStarDestroyer2) {
			if (localUp > 0) {
				return;
			}
		}

		if (hostileObj->objectType == OBJ_SuperStarDestroyer) {
			int upDelta;
			int absAttachUp;

			upDelta = localUp - attachUp;
			if (upDelta < 0) {
				upDelta = -upDelta;
			}
			absAttachUp = attachUp;
			if (absAttachUp < 0) {
				absAttachUp = -absAttachUp;
			}

			if (upDelta > absAttachUp - (absAttachUp >> 2)) {
				return;
			}
			if (collide_roughdistance3d(localSide - attachSide, localFwd - attachFwd, upDelta) >
				targetBoundsExtent >> 3) {
				return;
			}
		} else {
			if (hostileObj->objectType == OBJ_RepairYard) {
				int meshCount;
				int meshIdx;
				int distance;
				ObjectTypeId repairYardType;

				distance =
					collide_roughdistance3d(localSide - attachSide, localFwd - attachFwd, localUp - attachUp);
				disruptionRadius = targetBoundsExtent / 6;
				if (distance <= disruptionRadius) {
				} else {
					repairYardType = hostileObj->objectType;
					meshCount = ModelMesh_GetObjectTypeMeshCount(repairYardType);
					meshIdx = 0;
					if (meshCount <= 0) {
						return;
					}

					for (; meshIdx < meshCount; ++meshIdx) {
						if (ModelMesh_GetObjectTypeMeshType(repairYardType, meshIdx) == MESH_Hangar) {
							int centerX;
							int centerY;
							int centerZ;

							centerX = ModelMesh_GetCenterX(repairYardType, meshIdx);
							centerZ = ModelMesh_GetCenterZ(repairYardType, meshIdx);
							centerY = ModelMesh_GetCenterY(repairYardType, meshIdx);
							pai_RotateLocalVectorToWorldScratch(hostileObj, centerX, centerZ, -centerY);
							if (collide_roughdistance3d(localSide - g_rotatedX, localFwd - g_rotatedY,
														localUp - g_rotatedZ) < disruptionRadius) {
								withinDisruptionPoint = 1;
								break;
							}
						}
					}
				}
			} else if (g_modelDefs[(uint16_t)modelIndex].jammingPointCount != 0) {
				int pointIdx;
				int distance;

				distance =
					collide_roughdistance3d(localSide - attachSide, localFwd - attachFwd, localUp - attachUp);
				disruptionRadius = targetBoundsExtent / 6;
				if (distance < disruptionRadius) {
					withinDisruptionPoint = 1;
				}

				pointIdx = 0;
				while (pointIdx < g_modelDefs[(uint16_t)modelIndex].jammingPointCount) {
					if (withinDisruptionPoint) {
						break;
					}
					pai_RotateLocalVectorToWorldScratch(
						hostileObj, g_modelDefs[(uint16_t)modelIndex].jammingPoints[pointIdx].x,
						g_modelDefs[(uint16_t)modelIndex].jammingPoints[pointIdx].z,
						g_modelDefs[(uint16_t)modelIndex].jammingPoints[pointIdx].y);
					if (collide_roughdistance3d(localSide - g_rotatedX, localFwd - g_rotatedY,
												localUp - g_rotatedZ) < disruptionRadius) {
						withinDisruptionPoint = 1;
						break;
					}
					++pointIdx;
				}
			} else {
				int distance;

				distance =
					collide_roughdistance3d(localSide - attachSide, localFwd - attachFwd, localUp - attachUp);
				disruptionRadius = targetBoundsExtent / 6;
				if (distance <= disruptionRadius) {
					withinDisruptionPoint = 1;
				}
			}
		}
		if (!withinDisruptionPoint) {
			return;
		}
	}

	{
		MobileObject* ownerMobj;
		CraftData* ownerCraft;

		ownerMobj = g_objectTable[ownerObjIdx].mobj;
		if (ownerMobj != NULL) {
			ownerCraft = ownerMobj->pCraft;
			if (ownerCraft != NULL) {
				ownerCraft->beamEffectAccum[2] = 0x28000u;
				ownerCraft->chaffActiveTimer = 0;
			}
		}
	}
}

// FUNCTION: XWA 0x40BA80
void collide_TransformHitIntoObjectLocalFrame(unsigned int objIdx, int* outDirX, int* outDirY, int* outDirZ,
											  int* outLocalX, int* outLocalY, int* outLocalZ) {
	int normalX;
	int normalY;
	int normalZ;
	int localX;
	int localY;
	int localZ;
	int result;

	FVIEW_calcrotatemove(g_objectTable[objIdx].pitch, g_objectTable[objIdx].yaw, NULL);
	FVIEW_calcrotateorient(g_objectTable[objIdx].roll, g_objectTable[objIdx].angleD, NULL);

	normalX = (int)(g_collisionImpactEffectNormal.x * 32767.0);
	normalY = (int)-(g_collisionImpactEffectNormal.y * 32767.0);
	normalZ = (int)(g_collisionImpactEffectNormal.z * 32767.0);
	*outDirX = Xwa_Q15Mul(normalX, g_fviewSideX_Q15) + Xwa_Q15Mul(normalZ, g_fviewUpX_Q15) +
			   Xwa_Q15Mul(normalY, g_fviewFwdX_Q15);
	*outDirY = Xwa_Q15Mul(normalX, g_fviewSideY_Q15) + Xwa_Q15Mul(normalZ, g_fviewUpY_Q15) +
			   Xwa_Q15Mul(normalY, g_fviewFwdY_Q15);
	*outDirZ = Xwa_Q15Mul(normalX, g_fviewSideZ_Q15) + Xwa_Q15Mul(normalZ, g_fviewUpZ_Q15) +
			   Xwa_Q15Mul(normalY, g_fviewFwdZ_Q15);

	localX = (int)g_collisionImpactEffectCenter.x;
	localY = (int)-g_collisionImpactEffectCenter.y;
	localZ = (int)g_collisionImpactEffectCenter.z;
	*outLocalX = Xwa_Q15Mul(localX, g_fviewSideX_Q15) + Xwa_Q15Mul(localZ, g_fviewUpX_Q15) +
				 Xwa_Q15Mul(localY, g_fviewFwdX_Q15);
	*outLocalY = Xwa_Q15Mul(localX, g_fviewSideY_Q15) + Xwa_Q15Mul(localZ, g_fviewUpY_Q15) +
				 Xwa_Q15Mul(localY, g_fviewFwdY_Q15);
	result = Xwa_Q15Mul(localX, g_fviewSideZ_Q15);
	*outLocalZ = result + Xwa_Q15Mul(localZ, g_fviewUpZ_Q15) + Xwa_Q15Mul(localY, g_fviewFwdZ_Q15);
}

// FUNCTION: XWA 0x4DED00
void collide_TransformSweepToModelLocal(ObjectRecord* targetObj, int* worldEnd, int* worldStart) {
	MobileObject* mobj;
	int endDx;
	int endDy;
	int endDz;
	int startDx;
	int startDy;
	int startDz;
	int endFwd;
	int startFwd;
	struct {
		int endSide;
		int pad0;
		int endUp;
		int startSide;
		int startUp;
	} projected;

	endDx = worldEnd[0] - targetObj->world_x;
	startDx = worldStart[0] - targetObj->world_x;
	endDy = worldEnd[1] - targetObj->world_y;
	startDy = worldStart[1] - targetObj->world_y;
	endDz = worldEnd[2] - targetObj->world_z;
	startDz = worldStart[2] - targetObj->world_z;
	mobj = targetObj->mobj;

	if (mobj != NULL) {
		if (mobj->orientMatrixDirty) {
			FVIEW_calcrotatemove(targetObj->pitch, targetObj->yaw, targetObj);
			FVIEW_calcrotateorient(targetObj->roll, targetObj->angleD, targetObj);
		}

		projected.endSide =
			Xwa_Dot3Q15Inline(endDx, endDy, endDz, mobj->cachedSideX, mobj->cachedSideY, mobj->cachedSideZ);
		endFwd = Xwa_Dot3Q15Inline(endDx, endDy, endDz, mobj->cachedFwdX, mobj->cachedFwdY, mobj->cachedFwdZ);
		projected.endUp =
			Xwa_Dot3Q15Inline(endDx, endDy, endDz, mobj->cachedUpX, mobj->cachedUpY, mobj->cachedUpZ);
		projected.startSide = Xwa_Dot3Q15Inline(startDx, startDy, startDz, mobj->cachedSideX,
												mobj->cachedSideY, mobj->cachedSideZ);
		startFwd = Xwa_Dot3Q15Inline(startDx, startDy, startDz, mobj->cachedFwdX, mobj->cachedFwdY,
									 mobj->cachedFwdZ);
		projected.startUp =
			Xwa_Dot3Q15Inline(startDx, startDy, startDz, mobj->cachedUpX, mobj->cachedUpY, mobj->cachedUpZ);
	} else {
		FVIEW_calcrotatemove(targetObj->pitch, targetObj->yaw, NULL);
		FVIEW_calcrotateorient(targetObj->roll, targetObj->angleD, NULL);

		projected.endSide = g_fviewSideX_Q15;
		projected.endSide =
			Xwa_Dot3Q15Inline(endDx, endDy, endDz, projected.endSide, g_fviewSideY_Q15, g_fviewSideZ_Q15);
		endFwd = g_fviewFwdX_Q15;
		endFwd = Xwa_Dot3Q15Inline(endDx, endDy, endDz, endFwd, g_fviewFwdY_Q15, g_fviewFwdZ_Q15);
		projected.endUp = g_fviewUpX_Q15;
		projected.endUp =
			Xwa_Dot3Q15Inline(endDx, endDy, endDz, projected.endUp, g_fviewUpY_Q15, g_fviewUpZ_Q15);
		projected.startSide = g_fviewSideX_Q15;
		projected.startSide = Xwa_Dot3Q15Inline(startDx, startDy, startDz, projected.startSide,
												g_fviewSideY_Q15, g_fviewSideZ_Q15);
		startFwd = g_fviewFwdX_Q15;
		startFwd = Xwa_Dot3Q15Inline(startDx, startDy, startDz, startFwd, g_fviewFwdY_Q15, g_fviewFwdZ_Q15);
		projected.startUp = g_fviewUpX_Q15;
		projected.startUp =
			Xwa_Dot3Q15Inline(startDx, startDy, startDz, projected.startUp, g_fviewUpY_Q15, g_fviewUpZ_Q15);
	}

	g_collideSweepWalkerStart.x = (float)projected.startSide;
	g_collideSweepLocalStart.x = (float)projected.startSide;
	g_collideSweepWalkerStart.y = (float)-startFwd;
	g_collideSweepLocalStart.y = (float)-startFwd;
	g_collideSweepWalkerStart.z = (float)projected.startUp;
	g_collideSweepLocalStart.z = (float)projected.startUp;

	g_collideSweepWalkerEnd.x = (float)projected.endSide;
	g_collideSweepLocalEnd.x = (float)projected.endSide;
	g_collideSweepWalkerEnd.y = (float)-endFwd;
	g_collideSweepLocalEnd.y = (float)-endFwd;
	g_collideSweepWalkerEnd.z = (float)projected.endUp;
	g_collideSweepLocalEnd.z = (float)projected.endUp;
}

// FUNCTION: XWA 0x40B3D0
int collide_GetHyperRegionDesignationForPlayer(unsigned int playerObjIdx, unsigned int regionMarkerObjIdx,
											   int designationSlot) {
	uint16_t flightGroupIdx;
	int result;
	char designation;
	char enableDesignation;

	result = 0;
	flightGroupIdx = g_objectTable[regionMarkerObjIdx].flightGroupIdx;
	designation = (char)(&g_missionFlightGroups[flightGroupIdx].fg.designation1)[designationSlot];
	if ((designation >= 16 && designation <= 19) || designation == 21) {
		enableDesignation =
			(char)(&g_missionFlightGroups[flightGroupIdx].fg.enableDesignation)[designationSlot];
		switch (enableDesignation) {
			case 0:
				if (g_objectTable[playerObjIdx].mobj->team == 0) {
					result = designation;
				}
				break;
			case 1:
				if (g_objectTable[playerObjIdx].mobj->team == 1) {
					result = designation;
				}
				break;
			case 8:
				result = designation;
				break;
			case 13:
				if (g_objectTable[playerObjIdx].mobj->iff == 0) {
					result = designation;
				}
				break;
			case 14:
				if (g_objectTable[playerObjIdx].mobj->iff == 1) {
					result = designation;
				}
				break;
			default:
				break;
		}
	}

	return result;
}

// Destroy a craft whose hull has been depleted: credit the kill, rebind/notify any
// players bound to it, release carried/carrier and linked-craft references, record
// the outcome, play destruction audio/music, and convert the object into debris
// (tumbling breakup or a stationary explosion). May clear *pPlayGeneric when it
// emits a specific destruction cue.
static __inline void collide_HandleCraftDestruction(unsigned int targetObjIdx, CraftData* targetCraft,
													uint16_t damageKind, unsigned int damageSourceObjIdx,
													int creditPlayerIdx, uint16_t attackerSrcIdx,
													char* pPlayGeneric) {
	unsigned int killCreditCode;
	int playerIdxd;
	int i;

	if (damageSourceObjIdx != 0xFFFF) {
		MobileObject* km = g_objectTable[damageSourceObjIdx].mobj;
		if (km) {
			if (g_applyingProximityDamage != 1 || creditPlayerIdx == -1 ||
				g_players[creditPlayerIdx].objectIndex == 0xFFFF) {
				if (km->state) {
					uint16_t src = km->sourceObjIdx;
					if (src != 0xFFFF)
						Mission_CreditDestructionDamageContributors(src, (uint16_t)targetObjIdx);
				} else {
					Mission_CreditDestructionDamageContributors((uint16_t)damageSourceObjIdx,
																(uint16_t)targetObjIdx);
				}
			} else {
				Mission_CreditDestructionDamageContributors((uint16_t)g_players[creditPlayerIdx].objectIndex,
															(uint16_t)targetObjIdx);
			}
		}
		km = g_objectTable[damageSourceObjIdx].mobj;
		if (km && (uint16_t)km->sourceObjIdx == g_players[g_localPlayer].objectIndex &&
			Object_IsHostileToTeam((uint16_t)targetObjIdx, (uint16_t)g_players[g_localPlayer].playerIff)) {
			if (ModelMesh_HasFuselage(g_objectTable[targetObjIdx].objectType) ||
				g_objectTable[targetObjIdx].genusId == GENUS_Fighter) {
				if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN)
					Music_TriggerSequence(2125, g_objectTable[targetObjIdx].regionIdx, 0);
				else
					Music_TriggerSequence(2105, g_objectTable[targetObjIdx].regionIdx, 0);
			} else if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
				Music_TriggerSequence(2120, g_objectTable[targetObjIdx].regionIdx, 0);
			} else {
				Music_TriggerSequence(2100, g_objectTable[targetObjIdx].regionIdx, 0);
			}
			if (g_objectTable[targetObjIdx].genusId == GENUS_Fighter) {
				MobileObject* tm = g_objectTable[targetObjIdx].mobj;
				CraftData* tc;
				uint16_t aiTarget;
				if (tm && (tc = tm->pCraft) != NULL && g_objectTable[targetObjIdx].playerOwnerIdx == -1 &&
					(aiTarget = tc->aiController.targetObjIdx) != 0xFFFF &&
					aiTarget < g_regionObjectSlotEnd &&
					g_missionFlightGroups[g_objectTable[aiTarget].flightGroupIdx].fg.globalUnit ==
						g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.globalUnit)
					fsfx_speakorderack(g_localPlayer, aiTarget, 38, -1, 0xFFFF, 0xFFFF);
				else
					fsfx_speakorderack(g_localPlayer, -1, 38, -1, 0xFFFF, 0x8000);
			}
		}
	}
	killCreditCode = Mission_RecordPlayerCraftLoss((uint16_t)targetObjIdx, 0);

	playerIdxd = -1;
	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		if (g_players[i].connectedFlag && g_players[i].hasCheckpointFlag &&
			g_players[i].objectIndex == (int)targetObjIdx) {
			playerIdxd = i;
			break;
		}
	}

	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		if (g_players[i].boundObjectSignature == g_objectTable[targetObjIdx].objectSignature &&
			g_players[i].mapCameraState && g_players[i].connectedFlag == 1) {
			int savedMapCam;
			Mission_ProcessFlightGroupWaveCompletion(g_players[i].boundFlightGroupIdx);
			savedMapCam = g_players[i].mapCameraState;
			g_players[i].mapCameraState = 0;
			if (Player_BindToAvailableCraft(i, 0xFFFF, g_players[i].boundObjectSignature, 0)) {
				g_players[i].mapCameraState = savedMapCam;
			} else {
				Hud_RestorePlayerHudState(i);
				g_players[i].mapCameraState = 0;
				g_players[i].altViewObjectIdx = 0xFFFF;
				if (i == g_localPlayer)
					ForceFeedback_EnableEffects();
			}
		}
	}

	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
		Player_ReleaseCarriedObject(g_objectTable[targetObjIdx].playerOwnerIdx);
		Player_SaveCraftSettings(g_objectTable[targetObjIdx].playerOwnerIdx);
		if (g_provingGroundsModeActive)
			Yard_SavePlayerRecoveryState(targetObjIdx);
	}
	if (playerIdxd != -1)
		Player_SaveCraftSettings(playerIdxd);
	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
		Player_StartPostDestructionState(g_objectTable[targetObjIdx].playerOwnerIdx, attackerSrcIdx,
										 killCreditCode);
		if (playerIdxd != -1)
			Player_StartPostDestructionState(playerIdxd, attackerSrcIdx, killCreditCode);
		if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer || playerIdxd == g_localPlayer) {
			ForceFeedback_StopAllEffects();
			ForceFeedback_EnableEffects();
			ForceFeedback_PlayCraftDestructionEffect();
			Music_TriggerSequence(2195, g_objectTable[targetObjIdx].regionIdx, 0);
		}
	}
	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1)
		g_flightGlobalCountdownTimers[3] = 0; /* missionArrivalTriggerScanTimer */
	Mission_RecordCraftOutcome((uint16_t)targetObjIdx, g_objectTable[targetObjIdx].flightGroupIdx, 2);

	/* Release carried/carrier links that referenced the destroyed craft. */
	{
		unsigned int start = g_activeRegionObjectSlotStart;
		unsigned int end = g_activeRegionCraftObjectSlotEnd;
		unsigned int slot;
		for (slot = (uint16_t)start; (uint16_t)slot < end; ++slot) {
			ObjectRecord* o = &g_objectTable[(uint16_t)slot];
			if (o->objectType != OBJ_None) {
				CraftData* c = o->mobj->pCraft;
				if (c->carriedObjectIndex == (uint16_t)targetObjIdx) {
					c->carriedObjectIndex = 0xFFFF;
					c->aiFlight.motionScale = -1;
					end = g_activeRegionCraftObjectSlotEnd;
				}
			}
		}
		for (slot = (uint16_t)start; (uint16_t)slot < end; ++slot) {
			ObjectRecord* o = &g_objectTable[(uint16_t)slot];
			if (o->objectType != OBJ_None) {
				CraftData* c = o->mobj->pCraft;
				if (c->carrierObjIdx == (uint16_t)targetObjIdx) {
					c->carrierObjIdx = 0xFFFF;
					end = g_activeRegionCraftObjectSlotEnd;
				}
			}
		}
	}

	/* Walk to the tail of the linked-craft chain and unlink the whole group. */
	{
		uint16_t tail = (uint16_t)targetObjIdx;
		uint16_t next;
		CraftData* tailCraft;
		uint16_t prev;
		uint16_t* prevSlot;
		for (next = g_objectTable[(uint16_t)targetObjIdx].mobj->pCraft->nextLinkObjectIdx; next != 0xFFFF;
			 next = g_objectTable[next].mobj->pCraft->nextLinkObjectIdx)
			tail = next;
		tailCraft = g_objectTable[tail].mobj->pCraft;
		tailCraft->workingSubsystems = 0;
		prevSlot = &tailCraft->linkedPrevObjectIdx;
		for (prev = tailCraft->linkedPrevObjectIdx; prev != 0xFFFF; prev = tailCraft->linkedPrevObjectIdx) {
			CraftData* prevCraft;
			*prevSlot = 0xFFFF;
			prevCraft = g_objectTable[prev].mobj->pCraft;
			prevCraft->nextLinkObjectIdx = 0xFFFF;
			prevCraft->workingSubsystems = 0;
			g_objectTable[prev].mobj->speed = 0;
			g_objectTable[prev].mobj->speedRemainder = 0;
			tailCraft = prevCraft;
			prevSlot = &prevCraft->linkedPrevObjectIdx;
		}
	}

	msg_emitCraftMessage((uint16_t)targetObjIdx, targetCraft, 144);
	{
		uint8_t globalUnit =
			g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.globalUnit;
		if (globalUnit == g_missionFlightGroups[g_objectTable[targetObjIdx].flightGroupIdx].fg.globalUnit) {
			if (!fsfx_speakorderack(g_localPlayer, targetObjIdx, 8, -1, targetObjIdx, 0xC000))
				fsfx_speakorderack(g_localPlayer, -1, 26, -1, targetObjIdx, 0xC000);
		} else if (attackerSrcIdx != 0xFFFF && attackerSrcIdx >= g_activeRegionObjectSlotStart &&
				   attackerSrcIdx < g_activeRegionCraftObjectSlotEnd &&
				   attackerSrcIdx != g_players[g_localPlayer].objectIndex &&
				   globalUnit ==
					   g_missionFlightGroups[g_objectTable[attackerSrcIdx].flightGroupIdx].fg.globalUnit) {
			if (g_objectTable[targetObjIdx].genusId == GENUS_Fighter ||
				g_objectTable[targetObjIdx].genusId == GENUS_Transport)
				fsfx_speakorderack(g_localPlayer, attackerSrcIdx, 19, -1, targetObjIdx, 0xFFFF);
			else
				fsfx_speakorderack(g_localPlayer, attackerSrcIdx, 20, -1, targetObjIdx, 0xA000);
		}
	}
	if (Object_IsFriendlyToTeam((uint16_t)targetObjIdx, (uint16_t)g_players[g_localPlayer].playerIff) &&
		g_objectTable[targetObjIdx].playerOwnerIdx != g_localPlayer) {
		if (ModelMesh_HasFuselage((uint16_t)g_objectTable[targetObjIdx].objectType) ||
			!g_objectTable[targetObjIdx].genusId) {
			if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN)
				Music_TriggerSequence(2135, g_objectTable[targetObjIdx].regionIdx, 0);
			else
				Music_TriggerSequence(2115, g_objectTable[targetObjIdx].regionIdx, 0);
		} else if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
			Music_TriggerSequence(2130, g_objectTable[targetObjIdx].regionIdx, 0);
		} else {
			Music_TriggerSequence(2110, g_objectTable[targetObjIdx].regionIdx, 0);
		}
	}
	if (fsfx_SpeakTacticalOfficerEvent(4, 89, targetObjIdx, 0xFFFF) &&
		Object_IsFriendlyToTeam((uint16_t)targetObjIdx, (uint16_t)g_players[g_localPlayer].playerIff)) {
		int localObjIdx = g_players[g_localPlayer].objectIndex;
		if (localObjIdx != 0xFFFF) {
			int isRebelFighter;
			uint16_t lt = g_objectTable[localObjIdx].objectType;
			isRebelFighter =
				lt == OBJ_XWing || lt == OBJ_YWing || lt == OBJ_AWing || lt == OBJ_Z95 || lt == OBJ_BWing;
			if (isRebelFighter)
				fsfx_PlaySound(122, 0xFFFF, g_localPlayer);
			else
				fsfx_PlaySound(60, 0xFFFF, g_localPlayer);
		}
	}

	{
		uint16_t destroyedType = g_objectTable[targetObjIdx].objectType;
		ForceFeedback_PlayProximityEffectForObject(1, targetObjIdx);
		fsfx_PlaySound(fsfx_PickRandomSmallExplosionSfx(), targetObjIdx, g_localPlayer);

		if (!ModelMesh_HasFuselage((uint16_t)destroyedType) && g_objectTable[targetObjIdx].genusId &&
			g_objectTable[targetObjIdx].genusId != 9) {
			/* Debris breakup: random tumble + scheduled final explosion. */
			ModelIndex modelIndex = targetCraft->modelIndex;
			uint16_t yawMag = (GameRand() & 0x3FFF) + 0x2000;
			uint16_t* tumblePtr = &g_modelDefs[modelIndex].maxTumbleAngle;
			uint16_t pitchMag;
			int16_t yawRate;
			int16_t pitchRate;

			while (yawMag > *tumblePtr)
				yawMag >>= 1;
			GameRand();
			g_objectTable[targetObjIdx].mobj->rollImpulseRate = 0;
			for (pitchMag = (GameRand() & 0x3FFF) + 0x2000; pitchMag > *tumblePtr; pitchMag >>= 1)
				;
			yawRate = pitchMag >> 1;
			if ((uint16_t)GameRand() < 0x8000)
				yawRate = -yawRate;
			g_objectTable[targetObjIdx].mobj->pCraft->breakupYawRate = yawRate;
			{
				uint16_t mag = (GameRand() & 0x3FFF) + 0x2000;
				uint16_t lim = *tumblePtr;
				while (mag > lim)
					mag >>= 1;
				pitchRate = mag >> 1;
				if ((uint16_t)GameRand() < 0x8000)
					pitchRate = -pitchRate;
			}
			g_objectTable[targetObjIdx].mobj->pCraft->breakupPitchRate = pitchRate;
			targetCraft->objectKind = 3;

			if (damageSourceObjIdx != 0xFFFF &&
				g_objectTable[damageSourceObjIdx].objectType == OBJ_LaserImperialDS &&
				g_objectTable[targetObjIdx].mobj) {
				g_objectTable[targetObjIdx].mobj->lifetimeTimer = 2 * (uint16_t)g_elapsedTicks + 300;
			} else if (g_missionFlightGroups[g_objectTable[targetObjIdx].flightGroupIdx]
						   .fg.craftExplosionTime) {
				g_objectTable[targetObjIdx].mobj->lifetimeTimer =
					1180 * g_missionFlightGroups[g_objectTable[targetObjIdx].flightGroupIdx]
							   .fg.craftExplosionTime -
					1179;
			} else if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
				g_objectTable[targetObjIdx].mobj->lifetimeTimer = 708;
			} else {
				int frames;
				uint16_t type = g_objectTable[targetObjIdx].objectType;
				if (type == 61) {
					frames = (GameRand() & 3) + 2;
				} else if (type == 47 || type == 48) {
					frames = (GameRand() & 1) + 1;
				} else if (type == 203 || type == 205 || type == 204 || type == 207 || type == 206) {
					targetCraft->objectKind = 4;
					g_objectTable[targetObjIdx].mobj->lifetimeTimer = 1;
					Craft_DetachDamageableComponent((uint16_t)targetObjIdx, 1, 0xFFFF);
					fsfx_PlaySound(138, targetObjIdx, g_localPlayer);
					frames = -1; /* lifetime already set */
				} else {
					frames = (GameRand() & 7) + 5;
				}
				if (frames >= 0)
					g_objectTable[targetObjIdx].mobj->lifetimeTimer = 236 * frames;
			}

			if (g_objectTable[targetObjIdx].mobj->orientMatrixDirty ||
				g_objectTable[targetObjIdx].mobj->moveVectorDirty) {
				FVIEW_calcrotatemove(g_objectTable[targetObjIdx].pitch, g_objectTable[targetObjIdx].yaw,
									 &g_objectTable[targetObjIdx]);
				FVIEW_calcrotateorient(g_objectTable[targetObjIdx].roll, g_objectTable[targetObjIdx].angleD,
									   &g_objectTable[targetObjIdx]);
			}
			g_objectTable[targetObjIdx].mobj->velocityOverrideActive = 1;
			g_objectTable[targetObjIdx].mobj->velocityOverrideDuration = 0;
			g_objectTable[targetObjIdx].mobj->velocityOverrideSpeed = g_objectTable[targetObjIdx].mobj->speed;
			g_objectTable[targetObjIdx].mobj->velocityOverrideElapsed = 0;
			g_objectTable[targetObjIdx].mobj->velocityOverrideDirX =
				g_objectTable[targetObjIdx].mobj->cachedFwdX;
			g_objectTable[targetObjIdx].mobj->velocityOverrideDirY =
				g_objectTable[targetObjIdx].mobj->cachedFwdY;
			g_objectTable[targetObjIdx].mobj->velocityOverrideDirZ =
				g_objectTable[targetObjIdx].mobj->cachedFwdZ;
			return;
		}

		/* Fuselage / station: either a stationary explosion or a spinning breakup. */
		{
			uint16_t spd;
			if ((damageKind && g_modelTypeTable[damageKind].maxBoundsExtent > 1095 &&
				 !g_modelTypeTable[damageKind].familyId) ||
				damageKind == 78 || (spd = g_objectTable[targetObjIdx].mobj->speed) == 0 ||
				g_provingGroundsModeActive) {
				g_objectTable[targetObjIdx].mobj->pCraft->throttleSpeed = 0;
				g_objectTable[targetObjIdx].mobj->pCraft->engineOutputScale = 0;
				g_objectTable[targetObjIdx].mobj->lifetimeTimer = 1;
				fsfx_PlaySound(24, targetObjIdx, g_localPlayer);
				ForceFeedback_PlayProximityEffectForObject(0, targetObjIdx);
				*pPlayGeneric = 0;
				targetCraft->objectKind = 4;
				return;
			}
			if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
				if (spd < 0x32 || (uint16_t)GameRand() < 0x2000) {
					g_objectTable[targetObjIdx].mobj->pCraft->throttleSpeed = 0;
					g_objectTable[targetObjIdx].mobj->pCraft->engineOutputScale = 0;
					g_objectTable[targetObjIdx].mobj->lifetimeTimer = 1;
					fsfx_PlaySound(24, targetObjIdx, g_localPlayer);
					ForceFeedback_PlayProximityEffectForObject(0, targetObjIdx);
					*pPlayGeneric = 0;
					targetCraft->objectKind = 4;
					return;
				}
			}

			ModelMesh_GetObjectTypeMeshCount(destroyedType);
			g_objectTable[targetObjIdx].mobj->pCraft->throttleSpeed = 0;
			g_objectTable[targetObjIdx].mobj->pCraft->engineOutputScale = 0;
			{
				int spinMag = (GameRand() & 0x3FFF) + 0x2000;
				int spin = spinMag;
				if ((uint16_t)spinMag < 0x8000)
					spin = -spinMag;
				g_objectTable[targetObjIdx].mobj->rollImpulseRate = 0;
				g_objectTable[targetObjIdx].mobj->spinRate = (int16_t)spin;
				g_objectTable[targetObjIdx].mobj->spinRateFrac = 0;
				g_objectTable[targetObjIdx].mobj->renderOffsetX = 0;
				g_objectTable[targetObjIdx].mobj->renderOffsetY = 0;
				g_objectTable[targetObjIdx].mobj->renderOffsetZ = 0;
			}
			MobileObject_SetRandomSpinAxis(targetObjIdx);
			targetCraft->objectKind = 3;
			if (g_objectTable[targetObjIdx].playerOwnerIdx != -1)
				g_objectTable[targetObjIdx].mobj->lifetimeTimer = 708;
			else
				g_objectTable[targetObjIdx].mobj->lifetimeTimer = (uint16_t)GameRand() % 944 + 118;
			if (g_useHardware3D && g_flightSideEffectsEnabled &&
				g_objRenderState[targetObjIdx].drawnThisFrame) {
				Vec3f direction;
				direction.x = 0.0f;
				direction.y = 1.0f;
				direction.z = 0.0f;
				if ((uint16_t)GameRand2() <= 0x6000) {
					targetCraft->componentState[49] = 1;
				} else {
					Particle_AttachEffectToObject(1, (uint16_t)targetObjIdx, NULL, &direction);
					if (g_flightPlayerCount > 1 || g_filmRecording == 2 || g_filmPlaybackMode == 2)
						targetCraft->componentState[49] = 1;
				}
			} else {
				targetCraft->componentState[49] = 1;
			}
			{
				uint16_t meshCount =
					(uint16_t)ModelMesh_GetObjectTypeMeshCount(g_objectTable[targetObjIdx].objectType);
				uint16_t mesh;
				for (mesh = 0; mesh < meshCount; ++mesh)
					Craft_DetachDamageableComponent((uint16_t)targetObjIdx, 0, mesh);
			}
		}
	}
}

// FUNCTION: XWA 0x40F230
// Central craft damage routine. Applies damage from damageSourceObjIdx to
// targetObjIdx; componentId low-16 selects the hit mesh (0xFFFF means generic, no
// component/hull damage). For normal calls shieldSide selects front/rear shield and
// damageDirection drives force-feedback/SFX direction. Synthetic source ids reuse
// shieldSide as a raw damage amount: -2 (starship splash), -3 (raw), -4 (raw<<8),
// 0xFFFF (fixed collision). Returns nonzero when the caller should play generic
// impact feedback, 0 after this routine emitted the specific hit/destruction cue.
// The damage RNG is reseeded from the target AI's saved seed so results are
// deterministic per craft.
char collide_damagecraft(unsigned int targetObjIdx, unsigned int componentId, unsigned int damageSourceObjIdx,
						 unsigned int shieldSide, unsigned int damageDirection) {
	CraftData* targetCraft;
	AiController* effectiveAI;
	CraftData* savedCurCraft;
	int* shieldPtr;
	unsigned int damage;
	unsigned int damageAmount;
	char playGenericImpactFeedback = 1;
	int syntheticStarshipDamage;
	char missionStatus20DamageCap;
	uint16_t damageKind;
	uint16_t attackerSrcIdx;
	int creditPlayerIdx;
	int flightGroupIdx;
	uint8_t objectKind;

	if (g_flightSimSideEffectsSuppressed)
		return 1;
	syntheticStarshipDamage = 0;

	missionStatus20DamageCap = 0;
	flightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
	if (g_missionFlightGroups[flightGroupIdx].fg.status1 == 20 ||
		g_missionFlightGroups[flightGroupIdx].fg.status2 == 20 ||
		(damageSourceObjIdx >= g_activeRegionObjectSlotStart &&
		 damageSourceObjIdx < g_activeRegionCraftObjectSlotEnd &&
		 (g_missionFlightGroups[g_objectTable[damageSourceObjIdx].flightGroupIdx].fg.status1 == 20 ||
		  g_missionFlightGroups[g_objectTable[damageSourceObjIdx].flightGroupIdx].fg.status2 == 20))) {
		missionStatus20DamageCap = 1;
	}

	targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
	GameRand_SavePrimarySeed();
	effectiveAI = pai_GetEffectiveAIController(targetCraft);
	Math_SeedRandom(effectiveAI->savedRandSeed);

	if (g_applyingProximityDamage == 1) {
		damageKind = 264;
		damageAmount = (uint16_t)shieldSide;
		damage = damageAmount;
		shieldSide = 0;
	} else if (damageSourceObjIdx == 0xFFFFFFFEu) {
		damageSourceObjIdx = 0xFFFF;
		damageKind = 139;
		damageAmount = (uint16_t)shieldSide;
		damage = damageAmount;
		shieldSide = 0;
		syntheticStarshipDamage = 1;
	} else if (damageSourceObjIdx == 0xFFFFFFFDu) {
		damageSourceObjIdx = 0xFFFF;
		damageAmount = (uint16_t)shieldSide;
		damage = damageAmount;
		shieldSide = 0;
		damageKind = 0;
	} else if (damageSourceObjIdx == 0xFFFFFFFCu) {
		damageSourceObjIdx = 0xFFFF;
		damageAmount = (uint16_t)shieldSide << 8;
		damage = damageAmount;
		shieldSide = 0;
		damageKind = 0;
	} else if (damageSourceObjIdx == 0xFFFFu) {
		damageKind = 139;
		damageAmount = 0x20000;
		damage = damageAmount;
	} else {
		MobileObject* srcMobj = g_objectTable[damageSourceObjIdx].mobj;
		damageKind = (uint16_t)g_objectTable[damageSourceObjIdx].objectType;
		if (srcMobj) {
			CraftData* srcCraft;

			srcCraft = srcMobj->pCraft;
			if (!srcCraft) {
				damageAmount = srcMobj->damageAmount;
			} else {
				damageAmount = srcMobj->damageAmount;
				if (g_missionFormatVersion >= 14) {
					ModelGenusId tgtGenus = g_objectTable[targetObjIdx].genusId;
					if (tgtGenus == GENUS_Starship || tgtGenus == GENUS_Platform) {
						int modelIdx = GetModelIndexFromType((ObjectTypeId)damageKind);
						if (modelIdx != 0xFFFF) {
							unsigned int launcherCount = srcCraft->warheadLauncherCount;
							if (launcherCount != 0) {
								uint16_t* slotTypeIds = srcCraft->warheadSlotTypeIds;
								uint8_t sourceGenus = g_objectTable[damageSourceObjIdx].genusId;
								unsigned int launcher;
								for (launcher = 0; launcher < launcherCount; ++launcher) {
									uint16_t slotType = slotTypeIds[launcher];
									if (sourceGenus == GENUS_Starship &&
										g_objectTable[damageSourceObjIdx].objectType == OBJ_Dreadnaught2 &&
										g_objectTable[targetObjIdx].objectType == OBJ_SuperStarDestroyer &&
										srcCraft->objectKind != 3 && slotType == 293) {
										damageAmount += 4800000;
									} else {
										uint8_t firstSlot =
											g_modelDefs[modelIdx].warheadLauncherFirstSlot[launcher];
										uint8_t lastSlot =
											g_modelDefs[modelIdx].warheadLauncherLastSlot[launcher];
										damageAmount +=
											((srcCraft->warheadData[firstSlot].count +
											  srcCraft->warheadData[lastSlot].count) *
											 (unsigned int)
												 g_projectileDamageByType[slotType - OBJ_LaserRebel]) >>
											3;
									}
								}
							}
						}
					}
				}
				if (g_objectTable[damageSourceObjIdx].objectType == OBJ_SuperStarDestroyer) {
					damageAmount *= 16;
				}
			}
		} else {
			if ((unsigned int)g_modelTypeTable[damageKind].maxBoundsExtent >= 0x8000)
				damageAmount = 0x20000;
			else
				damageAmount = 4 * g_modelTypeTable[damageKind].maxBoundsExtent;
		}
		damage = damageAmount;
	}

	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
		PlayerViewState* viewState = &g_players[g_objectTable[targetObjIdx].playerOwnerIdx].viewState;
		if ((viewState->cameraPanDeltaZ & viewState->cameraPanDeltaX & viewState->cameraPanDeltaY) == 0) {
			if (syntheticStarshipDamage) {
				if (damageAmount > 0xA) {
					viewState->cameraPanDeltaX = GameRand() & 0x8007;
					viewState->cameraPanDeltaY = GameRand() & 0x8007;
					viewState->cameraPanDeltaZ = GameRand() & 0x8007;
				} else {
					viewState->cameraPanDeltaX = GameRand() & 0x8002;
					viewState->cameraPanDeltaY = GameRand() & 0x8002;
					viewState->cameraPanDeltaZ = GameRand() & 0x8002;
				}
				if (viewState->cameraPanDeltaX & 0x8000)
					viewState->cameraPanDeltaX = -(viewState->cameraPanDeltaX & 0x7FFF);
				if (viewState->cameraPanDeltaY & 0x8000)
					viewState->cameraPanDeltaY = -(viewState->cameraPanDeltaY & 0x7FFF);
				if (viewState->cameraPanDeltaZ & 0x8000)
					viewState->cameraPanDeltaZ = -(viewState->cameraPanDeltaZ & 0x7FFF);
			} else if (damageSourceObjIdx == 0xFFFF || g_objectTable[damageSourceObjIdx].mobj == NULL) {
				viewState->cameraPanDeltaX = GameRand() & 0x801F;
				if (viewState->cameraPanDeltaX & 0x8000)
					viewState->cameraPanDeltaX = -(viewState->cameraPanDeltaX & 0x7FFF);
				viewState->cameraPanDeltaY = GameRand() & 0x801F;
				if (viewState->cameraPanDeltaY & 0x8000)
					viewState->cameraPanDeltaY = -(viewState->cameraPanDeltaY & 0x7FFF);
				viewState->cameraPanDeltaZ = GameRand() & 0x801F;
				if (viewState->cameraPanDeltaZ & 0x8000)
					viewState->cameraPanDeltaZ = -(viewState->cameraPanDeltaZ & 0x7FFF);
			} else {
				int magnitude = (damage > 0x12C) ? 31 : 15;
				if (g_objectTable[damageSourceObjIdx].mobj->orientMatrixDirty != 0) {
					FVIEW_calcrotatemove(g_objectTable[damageSourceObjIdx].pitch,
										 g_objectTable[damageSourceObjIdx].yaw,
										 &g_objectTable[damageSourceObjIdx]);
					FVIEW_calcrotateorient(g_objectTable[damageSourceObjIdx].roll,
										   g_objectTable[damageSourceObjIdx].angleD,
										   &g_objectTable[damageSourceObjIdx]);
				}
				viewState->cameraPanDeltaX =
					-Xwa_Q15Mul(magnitude, g_objectTable[damageSourceObjIdx].mobj->cachedFwdX);
				viewState->cameraPanDeltaY =
					-Xwa_Q15Mul(magnitude, g_objectTable[damageSourceObjIdx].mobj->cachedFwdY);
				viewState->cameraPanDeltaZ =
					-Xwa_Q15Mul(magnitude, g_objectTable[damageSourceObjIdx].mobj->cachedFwdZ);
			}
		}
	}

	attackerSrcIdx = 0xFFFF;
	creditPlayerIdx = -1;
	{
		ModelGenusId tgtGenus = g_objectTable[targetObjIdx].genusId;

		if (tgtGenus == GENUS_Starship || tgtGenus == GENUS_Platform)
			damageAmount >>= 4;
		if (tgtGenus == GENUS_Freighter || tgtGenus == GENUS_Container)
			damageAmount >>= 2;
		targetCraft->damageReceivedTotal += damageAmount;
		damage = damageAmount;

		if (damageSourceObjIdx == 0xFFFF || g_objectTable[damageSourceObjIdx].mobj == NULL) {
			if (damageSourceObjIdx == 0xFFFF) {
				if (syntheticStarshipDamage)
					targetCraft->damageFromStarship += damageAmount;
			} else if (g_objectTable[damageSourceObjIdx].genusId == GENUS_Asteroid) {
				targetCraft->damageFromCollision += damageAmount;
			}
		} else {
			MobileObject* srcMobj = g_objectTable[damageSourceObjIdx].mobj;

			if (g_applyingProximityDamage == 1 && (damageSourceObjIdx < g_projectileObjectSlotStart ||
												   damageSourceObjIdx >= g_projectileObjectSlotEnd)) {
				uint16_t proximitySrcIdx = 0xFFFF;
				if (damageSourceObjIdx < g_activeRegionObjectSlotStart ||
					damageSourceObjIdx >= g_activeRegionCraftObjectSlotEnd) {
					if (damageSourceObjIdx >= g_explosionObjectSlotStart &&
						damageSourceObjIdx < g_explosionObjectSlotEnd)
						proximitySrcIdx = srcMobj->sourceObjIdx;
				} else {
					proximitySrcIdx = (uint16_t)damageSourceObjIdx;
				}
				if (proximitySrcIdx != 0xFFFF) {
					uint8_t bestFg = 0xFF;
					unsigned bestAmount = 0;
					CraftData* contributorCraft = g_objectTable[proximitySrcIdx].mobj->pCraft;
					int i;
					if (contributorCraft) {
						for (i = 0; i < 8; ++i) {
							if ((uint8_t)contributorCraft->damageFromFlightGroupIdx[i] != 0xFF &&
								(unsigned int)contributorCraft->damageFromFlightGroupAmount[i] > bestAmount) {
								bestAmount = contributorCraft->damageFromFlightGroupAmount[i];
								bestFg = (uint8_t)contributorCraft->damageFromFlightGroupIdx[i];
							}
						}
						if (bestFg != 0xFF) {
							int idx;
							for (idx = 0; idx < 8; ++idx) {
								if ((uint8_t)targetCraft->damageFromFlightGroupIdx[idx] == bestFg) {
									targetCraft->damageFromFlightGroupAmount[idx] += damageAmount;
									break;
								}
							}
							if (idx == 8) {
								for (idx = 0; idx < 8; ++idx) {
									if ((uint8_t)targetCraft->damageFromFlightGroupIdx[idx] == 0xFF) {
										targetCraft->damageFromFlightGroupAmount[idx] = damageAmount;
										targetCraft->damageFromFlightGroupIdx[idx] = (char)bestFg;
										break;
									}
								}
							}
							if (idx == 8) {
								for (idx = 0; idx < 8; ++idx) {
									if ((unsigned int)targetCraft->damageFromFlightGroupAmount[idx] <
										damageAmount) {
										targetCraft->damageFromFlightGroupAmount[idx] = damageAmount;
										targetCraft->damageFromFlightGroupIdx[idx] = (char)bestFg;
										break;
									}
								}
							}
						}
					}
					creditPlayerIdx = g_missionFlightGroups[bestFg].playerOwnerIdx;
				}
			} else if (g_applyingProximityDamage != 1 && !srcMobj->state) {
				attackerSrcIdx = (uint16_t)damageSourceObjIdx;
				creditPlayerIdx = g_objectTable[damageSourceObjIdx].playerOwnerIdx;
			} else {
				attackerSrcIdx = srcMobj->sourceObjIdx;
				if (attackerSrcIdx == 0xFFFF) {
					creditPlayerIdx = -1;
				} else {
					creditPlayerIdx = g_objectTable[attackerSrcIdx].playerOwnerIdx;
					if (srcMobj->state == 1 && srcMobj->pWarheadGuidance)
						creditPlayerIdx = srcMobj->pWarheadGuidance->sourcePlayerIdx;
				}
			}

			if (creditPlayerIdx == -1) {
				if (attackerSrcIdx != 0xFFFF) {
					if (g_objectTable[damageSourceObjIdx].mobj->state) {
						ObjectRecord* attrObj = &g_objectTable[attackerSrcIdx];
						if (attrObj->genusId) {
							if (attackerSrcIdx < g_objScanStart ||
								attackerSrcIdx >= g_regionStaticObjectSlotEnd)
								targetCraft->damageFromStarship += damageAmount;
							else
								targetCraft->damageFromMine += damageAmount;
						} else {
							targetCraft->damageFromAiSkill[g_missionFlightGroups[attrObj->flightGroupIdx]
															   .fg.groupAI] += damageAmount;
						}
					} else {
						targetCraft->damageFromCollision += damageAmount;
					}
				}
			} else {
				ObjectTypeId tt = g_objectTable[targetObjIdx].objectType;
				if (tt != OBJ_AccelRing2 && tt != OBJ_AccelRing3)
					targetCraft->damageFromPlayer[creditPlayerIdx] += damageAmount;
				if (!g_objectTable[damageSourceObjIdx].mobj->state)
					targetCraft->damageFromCollision += damageAmount;
			}
			if (attackerSrcIdx != 0xFFFF) {
				uint8_t fgIdx = g_objectTable[attackerSrcIdx].flightGroupIdx;
				int idx;
				for (idx = 0; idx < 8; ++idx) {
					if ((uint8_t)targetCraft->damageFromFlightGroupIdx[idx] == fgIdx) {
						targetCraft->damageFromFlightGroupAmount[idx] += damageAmount;
						break;
					}
				}
				if (idx == 8) {
					for (idx = 0; idx < 8; ++idx) {
						if ((uint8_t)targetCraft->damageFromFlightGroupIdx[idx] == 0xFF) {
							targetCraft->damageFromFlightGroupAmount[idx] = damageAmount;
							targetCraft->damageFromFlightGroupIdx[idx] = (char)fgIdx;
							break;
						}
					}
				}
				if (idx == 8) {
					for (idx = 0; idx < 8; ++idx) {
						if ((unsigned int)targetCraft->damageFromFlightGroupAmount[idx] < damageAmount) {
							targetCraft->damageFromFlightGroupAmount[idx] = damageAmount;
							targetCraft->damageFromFlightGroupIdx[idx] = (char)fgIdx;
							break;
						}
					}
				}
			}
		}
	}

	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1)
		targetCraft->damageReceivedByPlayerOwnedCraft += damageAmount;

	/* Pre-shield component/engine-glow damage for explosion meshes (skipped when a
	   shield is up on the highest difficulty). */
	if (!missionStatus20DamageCap && (uint16_t)componentId != 0xFFFF) {
		int shieldsUp = (targetCraft->shieldFront + targetCraft->shieldRear) != 0;
		if (shieldsUp) {
			if (g_flightDifficulty != 2) {
				ModelIndex modelIdx = GetModelIndexFromType(g_objectTable[targetObjIdx].objectType);
				if (modelIdx != (ModelIndex)0xFFFF && g_modelDefs[modelIdx].engineGlowCount) {
					savedCurCraft = g_curCraft;
					g_curCraft = targetCraft;
					damage = Craft_DamageNearestEngineEmitterForMesh((uint16_t)targetObjIdx,
																	 (int16_t)componentId, damageAmount);
					g_curCraft = savedCurCraft;
				}
				if (ModelMesh_HasExplosionType1(g_objectTable[targetObjIdx].objectType,
												(uint16_t)componentId - 1)) {
					savedCurCraft = g_curCraft;
					g_curCraft = targetCraft;
					damage =
						Craft_DamageComponent((uint16_t)targetObjIdx, componentId, damage, attackerSrcIdx);
					g_curCraft = savedCurCraft;
				}
			}
		} else {
			ModelIndex modelIdx = GetModelIndexFromType(g_objectTable[targetObjIdx].objectType);
			if (modelIdx != (ModelIndex)0xFFFF && g_modelDefs[modelIdx].engineGlowCount) {
				savedCurCraft = g_curCraft;
				g_curCraft = targetCraft;
				damage = Craft_DamageNearestEngineEmitterForMesh((uint16_t)targetObjIdx, (int16_t)componentId,
																 damageAmount);
				g_curCraft = savedCurCraft;
			}
			if (ModelMesh_HasExplosionType1(g_objectTable[targetObjIdx].objectType,
											(uint16_t)componentId - 1)) {
				savedCurCraft = g_curCraft;
				g_curCraft = targetCraft;
				damage = Craft_DamageComponent((uint16_t)targetObjIdx, componentId, damage, attackerSrcIdx);
				g_curCraft = savedCurCraft;
			}
		}

		/* Proving-ground race-gate (AccelRing) scoring on a beam-system mesh hit. */
		{
			uint16_t tgtType = g_objectTable[targetObjIdx].objectType;
			if (tgtType == OBJ_AccelRing2 || tgtType == OBJ_AccelRing3) {
				if ((uint16_t)ModelMesh_GetObjectTypeMeshType(g_objectTable[targetObjIdx].objectType,
															  (uint16_t)componentId - 1) == MESH_BeamSystem &&
					!targetCraft->meshRotation[(uint16_t)componentId + 49]) {
					++g_yardContext.playerChallengeStates[creditPlayerIdx].field_40;
					++targetCraft->damageFromPlayer[creditPlayerIdx];
					targetCraft->aiController.aiPlanState = 1416;
				}
			}
		}
	}

	if (g_objectTable[targetObjIdx].genusId == GENUS_LargeScenery) {
		uint16_t tt = (uint16_t)g_objectTable[targetObjIdx].objectType;
		if (tt < OBJ_Junk01 || tt > OBJ_MoltenBlock)
			damage = 0;
	}
	if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer && syntheticStarshipDamage == 1 &&
		!g_provingGroundsModeActive)
		ForceFeedback_PlayCriticalDamageEffect();
	if (damageSourceObjIdx != 0xFFFF && g_objectTable[damageSourceObjIdx].mobj &&
		g_objectTable[damageSourceObjIdx].genusId == GENUS_PlayerProjectile) {
		uint16_t tt = (uint16_t)g_objectTable[targetObjIdx].objectType;
		if ((tt < OBJ_Rubble01 || tt > OBJ_Rubble12) &&
			(uint16_t)g_objectTable[damageSourceObjIdx].mobj->sourceObjIdx ==
				g_players[g_localPlayer].objectIndex &&
			!Object_IsHostileToTeam((uint16_t)targetObjIdx, (uint16_t)g_players[g_localPlayer].playerIff)) {
			++g_fsfxLocalPlayerTargetedProjectileFriendlyHitCount;
			if (g_fsfxLocalPlayerTargetedProjectileFriendlyHitCount == 2)
				fsfx_SpeakTacticalOfficerEvent(5, 124, targetObjIdx, 0xFFFF);
			else if (g_fsfxLocalPlayerTargetedProjectileFriendlyHitCount == 12)
				fsfx_SpeakTacticalOfficerEvent(5, 125, targetObjIdx, 0xFFFF);
		}
	}

	shieldPtr = (&targetCraft->shieldFront) + (uint16_t)shieldSide;
	if (*shieldPtr <= (int)damage &&
		(damageKind != 264 || targetCraft->shieldFront + targetCraft->shieldRear < (int)damage)) {
		/* Shields depleted: the overflow continues into systems / hull. */
		if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer && !syntheticStarshipDamage)
			ForceFeedback_PlayLongDirectionalDamageEffect((uint16_t)damageDirection);
		if (g_gameConfig.voiceTacticalOfficerEnabled == 2 && g_objectTable[targetObjIdx].genusId &&
			*shieldPtr && !targetCraft->hullDamage)
			fsfx_SpeakTacticalOfficerEvent(4, 78, targetObjIdx, 0xFFFF);
		if (g_useHardware3D && g_players[g_localPlayer].objectIndex == (int)targetObjIdx &&
			g_players[g_localPlayer].cockpitVisible &&
			!g_players[g_localPlayer].viewState.externalCameraActive && g_cockpitSparkHardpointCount)
			GlowMark_SpawnLocalPlayerHitEffects();
		if (damageKind == 264) {
			int fr = targetCraft->shieldFront + targetCraft->shieldRear;
			targetCraft->shieldFront = 0;
			damage = damage - fr;
			targetCraft->shieldRear = 0;
		} else {
			int s = *shieldPtr;
			*shieldPtr = 0;
			damage = damage - s;
		}
		if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
			int other;
			int otherVal;
			if (!(uint16_t)shieldSide && !syntheticStarshipDamage && (uint16_t)GameRand() < 0x1000)
				fsfx_PlaySound(124, 0xFFFF, g_objectTable[targetObjIdx].playerOwnerIdx);
			other = (uint16_t)shieldSide ^ 1;
			otherVal = (&targetCraft->shieldFront)[other];
			if (otherVal) {
				(&targetCraft->shieldFront)[other] = otherVal - otherVal / 2;
				*shieldPtr += otherVal / 2;
				targetCraft->shieldDistribMode = 1;
			}
		}

		if (damage) {
			/* Penetration impact cue. */
			if (g_objectTable[targetObjIdx].playerOwnerIdx != g_localPlayer) {
				if (damageSourceObjIdx != 0xFFFF && g_objectTable[damageSourceObjIdx].mobj) {
					MobileObject* km = g_objectTable[damageSourceObjIdx].mobj;
					uint16_t saved = (uint16_t)km->sourceObjIdx;
					km->sourceObjIdx = -1;
					if (damageKind == 0x118) {
						fsfx_PlaySound(54, damageSourceObjIdx, g_localPlayer);
						playGenericImpactFeedback = 0;
					} else if (damageKind == 0x119 || damageKind == 0x129 || damageKind == 0x120) {
						fsfx_PlaySound(55, damageSourceObjIdx, g_localPlayer);
						playGenericImpactFeedback = 0;
					} else if (damageKind == 0x11C || damageKind == 0x11D || damageKind == 0x122) {
						fsfx_PlaySound((GameRand2() & 1) + 56, damageSourceObjIdx, g_localPlayer);
						playGenericImpactFeedback = 0;
					}
					g_objectTable[damageSourceObjIdx].mobj->sourceObjIdx = saved;
				} else {
					if (damageKind == 0x118) {
						fsfx_PlaySound(54, targetObjIdx, g_localPlayer);
						playGenericImpactFeedback = 0;
					} else if (damageKind == 0x119 || damageKind == 0x129 || damageKind == 0x120) {
						fsfx_PlaySound(55, targetObjIdx, g_localPlayer);
						playGenericImpactFeedback = 0;
					} else if (damageKind == 0x11C || damageKind == 0x11D || damageKind == 0x122) {
						fsfx_PlaySound((GameRand2() & 1) + 56, targetObjIdx, g_localPlayer);
						playGenericImpactFeedback = 0;
					}
				}
			}

			if (g_objectTable[targetObjIdx].genusId && g_objectTable[targetObjIdx].mobj->ejectionSpawnCount) {
				int lastHit = targetCraft->lastSystemHitTime;
				int now = g_gameTime;
				targetCraft->lastSystemHitTime = g_gameTime;
				if ((int)(1180 * damage * (now - lastHit)) > targetCraft->hullMax - targetCraft->hullDamage)
					targetCraft->systemHitFlag = 1;
			}
			if (g_provingGroundsModeActive && damageSourceObjIdx != 0xFFFF && g_flightPlayerCount > 1) {
				MobileObject* km = g_objectTable[damageSourceObjIdx].mobj;
				if (km->sourceObjIdx != -1) {
					uint16_t st = g_objectTable[(uint16_t)km->sourceObjIdx].objectType;
					if (st == OBJ_AccelRing2 || st == OBJ_AccelRing3) {
						int isHighest = 1;
						int i;
						for (i = 0; i < g_flightPlayerCount; ++i) {
							if (g_yardContext.playerChallengeStates[i].score >
								g_yardContext
									.playerChallengeStates[g_objectTable[targetObjIdx].playerOwnerIdx]
									.score) {
								isHighest = 0;
								break;
							}
						}
						if (isHighest) {
							MobileObject* tm = g_objectTable[targetObjIdx].mobj;
							if (tm->speed > 0x32)
								tm->speed -= (tm->speed - 50) / 2;
						}
					}
				}
			}

			if (damageKind == 284 || damageKind == 285 || damageKind == 290 || damageKind == 296) {
				/* Ion / subsystem damage: degrade systems instead of the hull. */
				int16_t systemStrengthRemaining;
				int i;
				if (!missionStatus20DamageCap && targetCraft->subsystemDamage < 0x3E8u) {
					if (damageKind == 284)
						targetCraft->subsystemDamage += 1;
					if (damageKind == 285)
						targetCraft->subsystemDamage += 2;
					if (damageKind == 290)
						targetCraft->subsystemDamage += 4;
					if (damageKind == 296)
						targetCraft->subsystemDamage += 30;
				}
				if (targetCraft->modelIndex == (ModelIndex)0xFFFF)
					systemStrengthRemaining = 100;
				else
					systemStrengthRemaining =
						g_modelDefs[targetCraft->modelIndex].systemStrength - targetCraft->subsystemDamage;
				if (targetCraft->workingSubsystems && systemStrengthRemaining <= 10) {
					if ((int)damage > 0) {
						int count = (damage + 199) / 0xC8;
						do {
							uint16_t working = targetCraft->workingSubsystems;
							uint16_t sysIdx = 0;
							while (sysIdx < 10 && (working & g_subsystemIdToFlag[sysIdx]) == 0)
								++sysIdx;
							if (sysIdx < 10)
								targetCraft->workingSubsystems =
									working & (g_subsystemIdToFlag[sysIdx] ^ 0x3FF);
							if (sysIdx < 10 && g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
								targetCraft->systemHealth[sysIdx] = 0;
								targetCraft->systemTimer[sysIdx] = g_subsystemRepairDuration[sysIdx];
							}
							--count;
						} while (count);
					}
					if (systemStrengthRemaining <= 0 && g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
						for (i = 0; i < 10; ++i) {
							uint16_t flag = g_subsystemIdToFlag[i];
							if (targetCraft->workingSubsystems & flag) {
								targetCraft->workingSubsystems &= (flag ^ 0x3FF);
								targetCraft->systemHealth[i] = 0;
								targetCraft->systemTimer[i] = g_subsystemRepairDuration[i];
							}
						}
						targetCraft->workingSubsystems = 0;
						if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer)
							msg_emitInFlightMessage(MSG_NOW_DISABLED, g_localPlayer);
					}
					if (!targetCraft->workingSubsystems) {
						targetCraft->shieldFront = 0;
						targetCraft->subsystemDamage = g_modelDefs[targetCraft->modelIndex].systemStrength;
						targetCraft->shieldRear = 0;
						msg_emitCraftMessage((uint16_t)targetObjIdx, targetCraft, 145);
						if (fsfx_SpeakTacticalOfficerEvent(4, 93, targetObjIdx, 0xFFFF) &&
							!Object_IsHostileToTeam((uint16_t)targetObjIdx,
													(uint16_t)g_players[g_localPlayer].playerIff)) {
							int localObjIdx = g_players[g_localPlayer].objectIndex;
							if (localObjIdx != 0xFFFF) {
								int isRebelFighter;
								uint16_t lt = g_objectTable[localObjIdx].objectType;
								isRebelFighter = lt == OBJ_XWing || lt == OBJ_YWing || lt == OBJ_AWing ||
												 lt == OBJ_Z95 || lt == OBJ_BWing;
								if (isRebelFighter)
									fsfx_PlaySound(122, 0xFFFF, g_localPlayer);
								else
									fsfx_PlaySound(60, 0xFFFF, g_localPlayer);
							}
						}
						if (g_objectTable[damageSourceObjIdx].mobj) {
							uint16_t srcOf = g_objectTable[damageSourceObjIdx].mobj->sourceObjIdx;
							int localO = g_players[g_localPlayer].objectIndex;
							if (localO != 0xFFFF && localO != srcOf && srcOf != 0xFFFF &&
								g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx]
										.fg.globalUnit ==
									g_missionFlightGroups[g_objectTable[srcOf].flightGroupIdx].fg.globalUnit)
								fsfx_speakorderack(g_localPlayer, srcOf, 21, -1, 0xFFFF, 0xFFFF);
						}
						if (g_objectTable[targetObjIdx].playerOwnerIdx == -1) {
							unsigned int slot;
							if (targetCraft->laserSlotCount) {
								for (slot = 0; slot < targetCraft->laserSlotCount; ++slot) {
									if (targetCraft->warheadData[slot].weaponType >= 4)
										targetCraft->warheadData[slot].turretTargetObjIdx = -1;
								}
							}
							for (slot = 0; slot < targetCraft->cannonClassCount; ++slot)
								targetCraft->laserLinkMode[slot] = 0;
						}
						{
							int fgIdx = g_objectTable[targetObjIdx].flightGroupIdx;
							++g_missionFgStats[fgIdx].outcomeCount[14];
							if (g_missionFlightGroups[fgIdx].fg.specialCargoCraft == targetCraft->waveNumber)
								g_missionFgStats[fgIdx].specialCargoOutcome[14] = 1;
						}
					}
				}
				if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer)
					fsfx_PlaySound((GameRand2() & 1) + 48, targetObjIdx, g_localPlayer);
			} else {
				/* Component then hull damage. */
				if (!missionStatus20DamageCap && (uint16_t)componentId != 0xFFFF) {
					ModelIndex modelIdx = GetModelIndexFromType(g_objectTable[targetObjIdx].objectType);
					if (modelIdx != (ModelIndex)0xFFFF && g_modelDefs[modelIdx].engineGlowCount) {
						savedCurCraft = g_curCraft;
						g_curCraft = targetCraft;
						damage = Craft_DamageNearestEngineEmitterForMesh((uint16_t)targetObjIdx,
																		 (int16_t)componentId, damage);
						g_curCraft = savedCurCraft;
					}
					if (ModelMesh_IsObjectTypeMeshDamageable(g_objectTable[targetObjIdx].objectType,
															 (uint16_t)componentId - 1)) {
						savedCurCraft = g_curCraft;
						g_curCraft = targetCraft;
						damage = Craft_DamageComponent((uint16_t)targetObjIdx, componentId, damage,
													   attackerSrcIdx);
						g_curCraft = savedCurCraft;
					}
				}

				{
					unsigned int hullDamageBefore = targetCraft->hullDamage;
					targetCraft->hullDamage = hullDamageBefore + damage;
					if (g_gameConfig.voiceTacticalOfficerEnabled == 2 &&
						g_objectTable[targetObjIdx].genusId) {
						unsigned int t95 = MATH2_longfraction(targetCraft->hullMax, 0xF333);
						if (hullDamageBefore >= t95 || (unsigned int)targetCraft->hullDamage < t95) {
							unsigned int t75 = MATH2_longfraction(targetCraft->hullMax, 0xC000);
							if (hullDamageBefore >= t75 || (unsigned int)targetCraft->hullDamage < t75) {
								unsigned int t25 = MATH2_longfraction(targetCraft->hullMax, 0x4000);
								if (hullDamageBefore < t25 && (unsigned int)targetCraft->hullDamage >= t25)
									fsfx_SpeakTacticalOfficerEvent(4, 82, targetObjIdx, 0xFFFF);
							} else {
								fsfx_SpeakTacticalOfficerEvent(4, 83, targetObjIdx, 0xFFFF);
							}
						} else {
							fsfx_SpeakTacticalOfficerEvent(4, 84, targetObjIdx, 0xFFFF);
						}
					}
					if (g_objectTable[targetObjIdx].genusId == GENUS_Fighter && !hullDamageBefore &&
						targetCraft->hullDamage)
						fsfx_speakorderack(g_localPlayer, targetObjIdx, 7, 0, targetObjIdx, 0x4000);
					if (g_objectTable[targetObjIdx].playerOwnerIdx != -1 && !syntheticStarshipDamage) {
						g_playerFlightTransientTimers[g_objectTable[targetObjIdx].playerOwnerIdx].field_04 +=
							59;
						if (!missionStatus20DamageCap && (uint16_t)GameRand() < 0x4000) {
							uint16_t working = targetCraft->workingSubsystems;
							int sysIdx = GameRand() & 7;
							sysIdx += GameRand() & 1;
							sysIdx += GameRand() & 1;
							if (working & g_subsystemIdToFlag[sysIdx]) {
								targetCraft->workingSubsystems =
									working & (g_subsystemIdToFlag[sysIdx] ^ 0x3FF);
								g_msgArgTable[0] = g_subsystemMessageArgById[sysIdx];
								g_msgArgTable[1] = 94;
								if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer &&
									!g_players[g_localPlayer].regionSessionId)
									msg_emitInFlightMessage(MSG_SYSTEMCOND, g_localPlayer);
								targetCraft->systemHealth[sysIdx] = 0;
								targetCraft->systemTimer[sysIdx] = g_subsystemRepairDuration[sysIdx];
							}
						}
					}
				}
			}

			/* Hull crossed the system-damage threshold: random HUD feature failure. */
			{
				int handledThresholdFeedback = 0;
				if (!missionStatus20DamageCap &&
					(unsigned int)targetCraft->hullDamage >=
						(unsigned int)targetCraft->systemDamageHullThreshold &&
					!syntheticStarshipDamage) {
					uint16_t mask = g_subsystemFailureHudMaskByRandomSlot[GameRand() & 0xF];
					if (!((g_provingGroundsModeActive && mask == 1) ||
						  (mask & targetCraft->installedHudFeatureMask) == 0)) {
						if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
							uint16_t tt = g_objectTable[targetObjIdx].objectType;
							fsfx_PlaySound((GameRand2() & 1) + 43, targetObjIdx,
										   g_objectTable[targetObjIdx].playerOwnerIdx);
							playGenericImpactFeedback = 0;
							if (tt == OBJ_XWing || tt == OBJ_YWing || tt == OBJ_AWing || tt == OBJ_Z95 ||
								tt == OBJ_BWing) {
								if (mask <= 8) {
									if (mask == 8 || mask == 4 || mask == 2)
										mask = 0xE;
								} else if (mask <= 0x100) {
									if (mask == 0x100 || mask == 0x80)
										mask = 0x180;
								} else if (mask <= 0x400) {
									if (mask == 0x400 || mask == 0x200)
										mask = 0xE00;
								} else if (mask == 0x800 || mask == 0x1000) {
									mask = 0xE00;
								}
							}
						}
						targetCraft->activeHudFeatureMask &= ~mask;
					}
					if (!targetCraft->objectKind && (int)targetObjIdx != g_players[g_localPlayer].objectIndex)
						fsfx_speakorderack(g_localPlayer, targetObjIdx, 7, 9, targetObjIdx, 0x8000);
					handledThresholdFeedback = 1;
				}
				if (!handledThresholdFeedback) {
					if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer &&
						!syntheticStarshipDamage) {
						fsfx_PlaySound(GameRand2() % 3 + 45, targetObjIdx, g_localPlayer);
						playGenericImpactFeedback = 0;
					}
					if ((int)targetObjIdx != g_players[g_localPlayer].objectIndex)
						fsfx_speakorderack(g_localPlayer, targetObjIdx, 3, -1, targetObjIdx, 0x4000);
				}
			}
		}
	} else {
		/* Shields absorb the hit. */
		if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer && !syntheticStarshipDamage)
			ForceFeedback_PlayShortDirectionalDamageEffect((uint16_t)damageDirection);
		if (damageKind == 264) {
			int shieldFront = targetCraft->shieldFront;
			int half = (int)damage >> 1;
			if (shieldFront < half) {
				targetCraft->shieldFront = 0;
				targetCraft->shieldRear -= damage - shieldFront;
			} else {
				int shieldRear = targetCraft->shieldRear;
				if (shieldRear < half) {
					targetCraft->shieldRear = 0;
					targetCraft->shieldFront = shieldFront - (damage - shieldRear);
				} else {
					targetCraft->shieldFront = shieldFront - half;
					targetCraft->shieldRear = shieldRear - half;
				}
			}
		} else {
			*shieldPtr -= damage;
			if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
				int* otherPtr;
				int otherVal;
				g_lastShieldDamageSide = (uint16_t)shieldSide;
				otherPtr = (&targetCraft->shieldFront) + ((uint16_t)shieldSide ^ 1);
				g_playerFlightTransientTimers[g_objectTable[targetObjIdx].playerOwnerIdx].field_02 = 59;
				otherVal = *otherPtr;
				if (*otherPtr > *shieldPtr) {
					int give = (otherVal - *shieldPtr) / 2;
					*otherPtr = otherVal - give;
					*shieldPtr += give;
					targetCraft->shieldDistribMode = 1;
				}
			}
			if (g_objectTable[targetObjIdx].playerOwnerIdx != g_localPlayer &&
				(damageKind == 280 || damageKind == 281 || damageKind == 297 || damageKind == 288 ||
				 damageKind == 284 || damageKind == 285 || damageKind == 290)) {
				if (damageSourceObjIdx != 0xFFFF) {
					MobileObject* km = g_objectTable[damageSourceObjIdx].mobj;
					uint16_t saved = (uint16_t)km->sourceObjIdx;
					km->sourceObjIdx = -1;
					fsfx_PlaySound((GameRand2() & 1) + 52, damageSourceObjIdx, g_localPlayer);
					playGenericImpactFeedback = 0;
					g_objectTable[damageSourceObjIdx].mobj->sourceObjIdx = saved;
				} else {
					fsfx_PlaySound((GameRand2() & 1) + 52, targetObjIdx, g_localPlayer);
					playGenericImpactFeedback = 0;
				}
			}
		}
		if ((int)targetObjIdx != g_players[g_localPlayer].objectIndex)
			fsfx_speakorderack(g_localPlayer, targetObjIdx, 3, -1, targetObjIdx, 0x2000);
	}

	/* Cap or finalize the hull, and destroy the craft if it has been killed. */
	if (missionStatus20DamageCap &&
		(unsigned int)targetCraft->hullDamage >= (unsigned int)targetCraft->hullMax)
		targetCraft->hullDamage = targetCraft->hullMax - 1;
	objectKind = targetCraft->objectKind;
	if (!g_flightSimSideEffectsSuppressed) {
		if ((!objectKind || objectKind == 5) &&
			(unsigned int)targetCraft->hullDamage >= (unsigned int)targetCraft->hullMax)
			collide_HandleCraftDestruction(targetObjIdx, targetCraft, damageKind, damageSourceObjIdx,
										   creditPlayerIdx, attackerSrcIdx, &playGenericImpactFeedback);
	} else if ((!objectKind || objectKind == 5) &&
			   (unsigned int)targetCraft->hullDamage >= (unsigned int)targetCraft->hullMax) {
		targetCraft->hullDamage = targetCraft->hullMax - 1;
	}

	effectiveAI->savedRandSeed = GameRand_GetPrimarySeed();
	GameRand_RestorePrimarySeed();
	return playGenericImpactFeedback;
}

// FUNCTION: XWA 0x40E260
// Process projectileObjIdx hitting craft targetObjIdx at hitComponentId. Records the
// "attacked by team" mission stat and a tactical-officer voice line on the first hit,
// applies the FG goal score for the first scoring hit, then evaluates friendly-fire
// warning gating that controls whether the attacker is recorded as the craft's latest
// attacker. It applies mag-pulse/ion/chaff special effects or normal damage through
// collide_damagecraft, and finally converts/replaces the projectile with an
// impact/explosion object plus hit SFX and force feedback. The directional damage
// scale is computed only for the local player's own craft.
//
void collide_laserhitcraft(unsigned int projectileObjIdx, unsigned int targetObjIdx,
						   unsigned int hitComponentId) {
	// hitRegistered mirrors the original [ebp+0Fh] scratch byte (byte 3 of the
	// targetObjIdx argument slot, which is 0 for any valid slot index). It selects
	// whether the trailing impact SFX/feedback plays.
#ifdef XWA_MODERN
	char hitRegistered = 0;
#endif
	int ionFlag = 0;
	int shieldTotal;

	if (targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
		uint16_t attackerTeam;
		unsigned int sourceRef;
		uint16_t attackerIdx;
		CraftData* targetCraft;
		uint8_t* atkFlag;
		int forwardPositive;
		int shieldSideFlag;
		int damageDirection;
		uint16_t projType;

		{
			ObjectTypeId targetType = g_objectTable[targetObjIdx].objectType;
			if (targetType >= OBJ_DSReactorCore &&
				(targetType <= OBJ_DSReactorCylinder || targetType == OBJ_DSFocusLens)) {
				DeathStar_HandleReactorHit(projectileObjIdx, targetObjIdx, hitComponentId - 1);
				return;
			}
		}

		{
			uint16_t srcRaw = (uint16_t)g_objectTable[projectileObjIdx].mobj->sourceObjIdx;
			if (srcRaw != 0xFFFF) {
				sourceRef = (uint16_t)srcRaw;
				attackerTeam = g_missionFlightGroups[g_objectTable[sourceRef].flightGroupIdx].fg.team;
			} else {
				sourceRef = (uint16_t)projectileObjIdx;
				attackerTeam = 1;
			}
		}

		// A SmeltingRoom source in Proving Grounds skips the self-hit early-out.
		if (!(g_provingGroundsModeActive && g_objectTable[sourceRef].objectType == OBJ_SmeltingRoom)) {
			if ((unsigned int)sourceRef == targetObjIdx) {
				return;
			}
		}

		attackerIdx = sourceRef;
		targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
		// attackedByTeam is a char[] but the original packs flags here (bit0 counted,
		// bit7 goal-scored, bits 4-6 friendly-fire warning count) and shifts unsigned.
		atkFlag = (uint8_t*)&targetCraft->attackedByTeam[attackerTeam];

		if (*atkFlag == 0) {
			uint8_t fgIdx = g_objectTable[targetObjIdx].flightGroupIdx;

			*atkFlag = 1;
			++g_missionFgStats[fgIdx].outcomeCount[4];
			if (g_missionFlightGroups[fgIdx].fg.specialCargoCraft == targetCraft->waveNumber) {
				g_missionFgStats[fgIdx].specialCargoOutcome[4] = 1;
			}

			// First hit of a friendly craft by the local player's team triggers a
			// tactical-officer voice line classified by weapon/attacker type.
			if (g_gameConfig.voiceTacticalOfficerEnabled == 2 &&
				g_missionFlightGroups[fgIdx].fg.team == g_players[g_localPlayer].playerIff &&
				g_objectTable[targetObjIdx].genusId != 0) {
				if (attackerIdx < g_activeRegionObjectSlotStart ||
					attackerIdx >= g_activeRegionCraftObjectSlotEnd) {
					fsfx_SpeakTacticalOfficerEvent(4, 86, targetObjIdx, 0xFFFFu);
				} else {
					ObjectTypeId attackerType = g_objectTable[projectileObjIdx].objectType;
					if (attackerType == OBJ_WarheadMissile || attackerType == OBJ_WarheadAdvancedMissile) {
						fsfx_SpeakTacticalOfficerEvent(4, 110, targetObjIdx, 0xFFFFu);
					} else if (attackerType == OBJ_WarheadTorpedo ||
							   attackerType == OBJ_WarheadAdvancedTorpedo) {
						fsfx_SpeakTacticalOfficerEvent(4, 111, targetObjIdx, 0xFFFFu);
					} else if (attackerType == OBJ_WarheadRocket) {
						fsfx_SpeakTacticalOfficerEvent(4, 112, targetObjIdx, 0xFFFFu);
					} else if (attackerType == OBJ_WarheadSpaceBomb) {
						fsfx_SpeakTacticalOfficerEvent(4, 113, targetObjIdx, 0xFFFFu);
					} else {
						ModelGenusId attackerGenus = g_objectTable[attackerIdx].genusId;
						if (attackerGenus == GENUS_Fighter) {
							fsfx_SpeakTacticalOfficerEvent(4, 87, targetObjIdx, 0xFFFFu);
						} else if (attackerGenus == GENUS_Starship) {
							fsfx_SpeakTacticalOfficerEvent(4, 88, targetObjIdx, 0xFFFFu);
						} else {
							fsfx_SpeakTacticalOfficerEvent(4, 86, targetObjIdx, 0xFFFFu);
						}
					}
				}
			}
		}

		// Award the FG goal score for the first scoring hit by a craft-owning player.
		{
			int attackerPlayerOwner = g_objectTable[attackerIdx].playerOwnerIdx;
			if (attackerPlayerOwner != -1) {
				if ((*atkFlag & 0x80) != 0x80) {
					uint8_t targetFgIdx = g_objectTable[targetObjIdx].flightGroupIdx;
					uint16_t specialCargoFlag =
						(g_missionFlightGroups[targetFgIdx].fg.specialCargoCraft == targetCraft->waveNumber);
					Mission_ApplyFlightGroupGoalScore(3, targetFgIdx, attackerPlayerOwner, 1u,
													  specialCargoFlag,
													  (uint16_t)g_players[attackerPlayerOwner].playerIff);
				}
				*atkFlag |= 0x80u;
			}
		}

		// Record the most recent attacker (drives retaliation AI). For a craft with no
		// recorded attacker yet, friendly fire below the warning threshold is not recorded;
		// a craft that already has an attacker re-records only when player-owned.
		if (targetCraft->lastAttackerObjIdx == 0xFFFF) {
			if (attackerIdx >= g_activeRegionObjectSlotStart &&
				attackerIdx < g_activeRegionCraftObjectSlotEnd) {
				ModelGenusId attackerGenus = g_objectTable[attackerIdx].genusId;

				if (attackerGenus != GENUS_Starship && attackerGenus != GENUS_Platform &&
					attackerGenus != GENUS_Freighter && attackerGenus != GENUS_Container) {
					uint8_t recordAttacker = 1;

					if (g_objectTable[attackerIdx].playerOwnerIdx != -1 &&
						!Team_IsHostileToTeam(g_objectTable[targetObjIdx].mobj->team,
											  g_objectTable[attackerIdx].mobj->team) &&
						g_objectTable[targetObjIdx].genusId != 0) {
						uint8_t warnCount = (uint8_t)((*atkFlag >> 4) & 7);

						if (warnCount < 5) {
							warnCount +=
#ifdef XWA_MODERN
								(laser_GetProjectileWarheadClass(
									 g_objectTable[projectileObjIdx].objectType) > 0)
#else
								(g_projectileWarheadClassByType[(uint16_t)g_objectTable[projectileObjIdx]
																	.objectType -
																OBJ_LaserRebel] != 0)
#endif
									? 4
									: 0;
							if (warnCount > 7) {
								warnCount = 7;
							}
							*atkFlag = (uint8_t)((*atkFlag & 0x8F) | (warnCount << 4));
						}
						if (warnCount < 7) {
							recordAttacker = 0;
						}
					}
					if (recordAttacker == 1) {
						targetCraft->lastAttackerObjIdx = sourceRef;
						targetCraft->lastHitTimestamp = (uint16_t)Mission_GameTimeToSeconds(
							g_missionElapsedClock.hours, g_missionElapsedClock.minutes,
							g_missionElapsedClock.seconds);
					}
				}
			}
		} else if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
			targetCraft->lastAttackerObjIdx = sourceRef;
			targetCraft->lastHitTimestamp = (uint16_t)Mission_GameTimeToSeconds(
				g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);
		}

		{
			ObjectRecord* targetObj = &g_objectTable[targetObjIdx];
			MobileObject* targetMobj = targetObj->mobj;
			int hitOffX, hitOffY, hitOffZ;
			int dotFwd;
			int targetPlayerOwner;

			targetCraft->aiFlight.threatObjIdx = sourceRef;
			++targetCraft->aiFlight.reactionTimer;

			if (targetMobj->orientMatrixDirty) {
				FVIEW_calcrotatemove(targetObj->pitch, targetObj->yaw, targetObj);
				FVIEW_calcrotateorient(targetObj->roll, targetObj->angleD, targetObj);
			}

			hitOffX = (int16_t)(g_collisionProbeWorldX - g_collisionSegmentStartWorldX);
			hitOffY = (int16_t)(g_collisionProbeWorldY - g_collisionSegmentStartWorldY);
			hitOffZ = (int16_t)(g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ);

			dotFwd = Xwa_Dot3Q15Wrapped(targetMobj->cachedFwdX, targetMobj->cachedFwdY,
										targetMobj->cachedFwdZ, hitOffX, hitOffY, hitOffZ);
			forwardPositive = ((int16_t)dotFwd >= 0);

			targetPlayerOwner = g_objectTable[targetObjIdx].playerOwnerIdx;
			if (targetPlayerOwner != -1) {
				shieldSideFlag = forwardPositive;
				if (targetPlayerOwner == g_localPlayer) {
					// Directional damage scale for the player's own craft, derived from the
					// hit position projected onto the craft side axis vs. its distance.
					int hx = (int16_t)(g_collisionProbeWorldX - g_collisionSegmentStartWorldX);
					int hy = (int16_t)(g_collisionProbeWorldY - g_collisionSegmentStartWorldY);
					int hz = (int16_t)(g_collisionProbeWorldZ - g_collisionSegmentStartWorldZ);
					int dotSide = Xwa_Dot3Q15Wrapped(targetMobj->cachedSideX, targetMobj->cachedSideY,
													 targetMobj->cachedSideZ, hx, hy, hz);
					uint32_t sq = (uint32_t)(hx * hx) + (uint32_t)(hy * hy) + (uint32_t)(hz * hz);
					int distHalf = (int32_t)sq >> 1;

					damageDirection = 180 * shieldSideFlag;
					if (dotSide >= 0) {
						if (dotSide * dotSide > distHalf) {
							damageDirection = 300 - 60 * shieldSideFlag;
						}
					} else if (dotSide * dotSide > distHalf) {
						damageDirection = 60 * (shieldSideFlag + 1);
					}
				} else {
					damageDirection = (int)projectileObjIdx;
				}
			} else {
				shieldSideFlag = 0;
				damageDirection = (int)projectileObjIdx;
			}

			// Warhead-class projectiles physically bounce the struck craft when enabled.
			if (g_craftImpactBounceEnabled) {
				if (targetCraft->objectKind == 0) {
#ifdef XWA_MODERN
					if (laser_GetProjectileWarheadClass(g_objectTable[projectileObjIdx].objectType) > 0)
#else
					if (g_projectileWarheadClassByType[(uint16_t)g_objectTable[projectileObjIdx].objectType -
													   OBJ_LaserRebel] != 0)
#endif
					{
						collide_applyCraftImpactBounce(targetObjIdx, projectileObjIdx);
					}
				}
			}

			projType = g_objectTable[projectileObjIdx].objectType;
			if (projType != OBJ_WarheadMagPulse) {
				int chaffHandled = 0;

				if (targetCraft->cmTypeId == 1 && targetCraft->chaffActiveTimer != 0 &&
#ifdef XWA_MODERN
					laser_GetProjectileWarheadClass(projType) > 0 &&
#else
					g_projectileWarheadClassByType[(uint16_t)projType - OBJ_LaserRebel] != 0 &&
#endif
					forwardPositive) {
					chaffHandled = 1;
					msg_emitInFlightMessage(MSG_CHAFF_SUCCESS, g_objectTable[targetObjIdx].playerOwnerIdx);
				}
				if (!chaffHandled) {
					if (g_provingGroundsModeActive && g_objectTable[targetObjIdx].playerOwnerIdx != -1 &&
						g_objectTable[attackerIdx].playerOwnerIdx != -1) {
						Yard_HandleR2D2CarrierLaserHit(projectileObjIdx, targetObjIdx);
#ifdef XWA_MODERN
						hitRegistered = 1;
#else
						((uint8_t*)&targetObjIdx)[3] = 1;
#endif
					} else {
#ifdef XWA_MODERN
						hitRegistered =
							collide_damagecraft(targetObjIdx, hitComponentId, projectileObjIdx,
												(unsigned int)shieldSideFlag, (unsigned int)damageDirection);
#else
						((uint8_t*)&targetObjIdx)[3] =
							collide_damagecraft(targetObjIdx, hitComponentId, projectileObjIdx,
												(unsigned int)shieldSideFlag, (unsigned int)damageDirection);
#endif
					}
				}
			} else {
				uint16_t oldInhibit = targetCraft->weaponFireInhibitTimer;

				if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
					uint16_t i;
					for (i = 0; i < targetCraft->laserSlotCount; ++i) {
						targetCraft->warheadData[i].laserCharge = 0;
					}
					if ((targetCraft->workingSubsystems & 0x10) != 0) {
						targetCraft->systemHealth[2] = 0;
						targetCraft->workingSubsystems &= 0x3EF;
						targetCraft->systemTimer[2] = g_subsystemRepairDuration[2];
						if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer &&
							!g_players[g_localPlayer].regionSessionId) {
							g_msgArgTable[0] = 99;
							g_msgArgTable[1] = 94;
							msg_emitInFlightMessage(MSG_SYSTEMCOND, g_localPlayer);
						}
					} else {
						msg_emitInFlightMessage(MSG_LASER_DRAINAGE,
												g_objectTable[targetObjIdx].playerOwnerIdx);
					}
				} else {
					ModelGenusId g = g_objectTable[targetObjIdx].genusId;
					if (g == GENUS_Fighter || g == GENUS_Transport || g == GENUS_WeaponEmplacement ||
						g == GENUS_PilotDroid || g == GENUS_Utility) {
						targetCraft->weaponFireInhibitTimer = (uint16_t)(oldInhibit + 3540);
					} else {
						targetCraft->weaponFireInhibitTimer = (uint16_t)(oldInhibit + 7080);
					}
				}
				if (oldInhibit > targetCraft->weaponFireInhibitTimer) {
					targetCraft->weaponFireInhibitTimer = 0xFFFF;
				}
#ifdef XWA_MODERN
				hitRegistered = 1;
#else
				((uint8_t*)&targetObjIdx)[3] = 1;
#endif
			}

			shieldTotal = targetCraft->shieldRear + targetCraft->shieldFront;
		}
	} else {
		shieldTotal = 0;
	}

	{
		ObjectTypeId projType = g_objectTable[projectileObjIdx].objectType;
		int effectClass; // 0 = warhead splash, 1 = laser spark, 2 = mag-pulse animation
		unsigned int effectSlot;

#ifdef XWA_MODERN
		if (laser_GetProjectileWarheadClass(projType) <= 0) {
#else
		if (g_projectileWarheadClassByType[(uint16_t)projType - OBJ_LaserRebel] == 0) {
#endif
			effectClass = 1;
			if (projType == OBJ_LaserIon || projType == OBJ_LaserIonTurbo || projType == OBJ_WarheadIon ||
				projType == OBJ_WarheadIonPulse) {
				ionFlag = 1;
			}
		} else {
			effectClass = (projType == OBJ_WarheadMagPulse) ? 2 : 0;
		}

		// Hull impact particle marks: only when shields are depleted, hardware 3D + side
		// effects are on, the projectile is not an ion weapon, and the target drew this frame.
		if (shieldTotal == 0 && g_useHardware3D && g_flightSideEffectsEnabled && !ionFlag &&
			g_objRenderState[targetObjIdx].drawnThisFrame) {
			if (g_glowMarkWorldSegmentMode) {
				// Place the mark at a model vertex nearest the hit in the target's local frame.
				ObjectRecord* obj = &g_objectTable[targetObjIdx];
				MobileObject* mobj = obj->mobj;
				int relX = g_collisionSegmentStartWorldX + g_collisionHitOffsetX - obj->world_x;
				int relY = g_collisionSegmentStartWorldY + (g_collisionHitOffsetY - obj->world_y);
				int relZ = g_collisionSegmentStartWorldZ + (g_collisionHitOffsetZ - obj->world_z);
				int localSide = Xwa_Dot3Q15Inline(mobj->cachedSideX, mobj->cachedSideY, mobj->cachedSideZ,
												  relX, relY, relZ);
				int localFwd =
					Xwa_Dot3Q15Inline(mobj->cachedFwdX, mobj->cachedFwdY, mobj->cachedFwdZ, relX, relY, relZ);
				int localUp =
					Xwa_Dot3Q15Inline(mobj->cachedUpX, mobj->cachedUpY, mobj->cachedUpZ, relX, relY, relZ);
				uint16_t mesh =
					ModelMesh_FindNearestByBounds((uint16_t)obj->objectType, localSide, -localFwd, localUp);
#ifdef XWA_MODERN
				Vec3f* pickedVertex = NULL;

				ModelMesh_PickRandomVertex((uint16_t)obj->objectType, mesh, &pickedVertex);
#else
				// The vertex replaces the component id after its final use.
				ModelMesh_PickRandomVertex((uint16_t)obj->objectType, mesh, (Vec3f**)&hitComponentId);
#endif
				{
					Vec3f dir = { 0.0f, 1.0f, 0.0f };

					if ((uint16_t)GameRand2() < 0x4000u) {
#ifdef XWA_MODERN
						Particle_AttachEffectToObject(7, targetObjIdx, pickedVertex, &dir);
#else
						Particle_AttachEffectToObject(7, targetObjIdx, (Vec3f*)hitComponentId, &dir);
#endif
					} else {
#ifdef XWA_MODERN
						Particle_AttachEffectToObject(0, targetObjIdx, pickedVertex, &dir);
#else
						Particle_AttachEffectToObject(0, targetObjIdx, (Vec3f*)hitComponentId, &dir);
#endif
					}
				}
			} else {
				uint16_t r = (uint16_t)GameRand2();
				if (g_objRenderState[targetObjIdx].drawnThisFrame) {
					if (r < 0x1000u) {
						Particle_AttachEffectToObject(8, targetObjIdx, &g_glowMarkPlaneScratch.center,
													  g_glowMarkScratchNormalVec);
					} else {
						if (r < 0xB000u) {
							if (effectClass) {
								ParticleEffect* e = Particle_AttachEffectToObject(
									0, targetObjIdx, &g_glowMarkPlaneScratch.center,
									g_glowMarkScratchNormalVec);
								if (e) {
									e->yawRandomRad = 1.5707961f;
									e->pitchRandomRad = 1.5707961f;
								}
							} else {
								Particle_AttachEffectToObject(11, targetObjIdx,
															  &g_glowMarkPlaneScratch.center,
															  g_glowMarkScratchNormalVec);
								Particle_AttachEffectToObject(11, targetObjIdx,
															  &g_glowMarkPlaneScratch.center,
															  g_glowMarkScratchNormalVec);
							}
						} else {
							if (g_objectTable[targetObjIdx].objectType == OBJ_SmeltingRoom) {
								if ((int)((uint16_t)GameRand2()) % 2 != 0) {
									Vec3f dir = { 1.0f, 0.0f, 0.0f };
									if ((int)((uint16_t)GameRand2()) % 2 != 0) {
										ParticleEffect* e = Particle_AttachEffectToObject(
											5, targetObjIdx, &g_glowMarkPlaneScratch.center,
											g_glowMarkScratchNormalVec);
										if (e) {
											int half = e->lifetimeTicks / 2;
											ParticleEffectTemplate* def = e->def;
											e->lifetimeTicks = half;
											if (half > def->particleLifetimeTicks) {
												e->emitUntilTicks = half - def->particleLifetimeTicks;
											} else {
												e->emitUntilTicks = 1;
											}
										}
									} else {
										ParticleEffect* e = Particle_AttachEffectToObject(
											1, targetObjIdx, &g_glowMarkPlaneScratch.center, &dir);
										if (e) {
											e->lifetimeTicks = 314;
											e->emitUntilTicks = 118;
										}
									}
								}
							} else {
								Particle_AttachEffectToObject(5, targetObjIdx, &g_glowMarkPlaneScratch.center,
															  g_glowMarkScratchNormalVec);
							}
						}
					}
				}
			}
		}

		if (effectClass != 0) {
			const ObjectTypeId sparkType = OBJ_SparkTextureGroup3000;

			g_objectTable[projectileObjIdx].objectType = OBJ_None;
			effectSlot = Object_AllocLocalTransientSlot();
			if (effectSlot != 0xFFFF) {
				if (g_useHardware3D && g_hitEffectsEnabled) {
					if (shieldTotal == 0 && effectClass == 1 && !ionFlag) {
						g_objectTable[effectSlot].objectType = sparkType;
					}
				} else if (effectClass == 1) {
					g_objectTable[effectSlot].objectType = sparkType;
				} else {
					g_objectTable[effectSlot].objectType = OBJ_AnimationTextureGroup2007;
				}
			}
		} else {
			// Warhead splash: the projectile becomes a random explosion sprite and deals
			// radial proximity damage scaled by its remaining damage value.
			ObjectTypeId explosionType;
			unsigned int damageAmount;
			unsigned int radius;

			explosionType = (ObjectTypeId)GameRand();
			explosionType = (ObjectTypeId)((explosionType & 3) + OBJ_ExplosionTextureGroup2002);
			g_objectTable[projectileObjIdx].objectType = explosionType;
			damageAmount = g_objectTable[projectileObjIdx].mobj->damageAmount;
			if (damageAmount > 10000u) {
				damageAmount = 10000;
			}
			radius = 2 * damageAmount / 3u;
			if (radius < 1u) {
				radius = 1;
			} else if (radius > 0x8000u) {
				radius = 0x8000;
			}
			collide_ApplyProximityDamageFalloff(projectileObjIdx, damageAmount, radius, (int)targetObjIdx);
			effectSlot = projectileObjIdx;
		}

		if (effectSlot != 0xFFFF) {
			g_objectTable[effectSlot].world_x = g_collisionSegmentStartWorldX + g_collisionHitOffsetX;
			g_objectTable[effectSlot].world_y = g_collisionSegmentStartWorldY + g_collisionHitOffsetY;
			g_objectTable[effectSlot].world_z = g_collisionSegmentStartWorldZ + g_collisionHitOffsetZ;
			g_objectTable[effectSlot].genusId = GENUS_Explosion;
			g_objectTable[effectSlot].mobj->state = 5;
			g_objectTable[effectSlot].typeSpecificByte[0] = 1;
			g_objectTable[effectSlot].mobj->framesAlive = 0;
			g_objectTable[effectSlot].mobj->lifetimeTimer = 0;
			g_objectTable[effectSlot].mobj->sourceObjIdx = -1;
			if (shieldTotal) {
				g_objectTable[effectSlot].mobj->instanceExtent =
					g_modelTypeTable[(uint16_t)g_objectTable[effectSlot].objectType].maxBoundsExtent >> 1;
			} else {
				g_objectTable[effectSlot].mobj->instanceExtent =
					g_modelTypeTable[(uint16_t)g_objectTable[effectSlot].objectType].maxBoundsExtent;
			}
			g_objectTable[effectSlot].mobj->speed = g_objectTable[targetObjIdx].mobj->speed;
			g_objectTable[effectSlot].pitch = g_objectTable[targetObjIdx].pitch;
			g_objectTable[effectSlot].yaw = g_objectTable[targetObjIdx].yaw;
			if (effectClass) {
				g_objectTable[effectSlot].roll = (Q16Angle)GameRand2();
			} else {
				g_objectTable[effectSlot].roll = (Q16Angle)GameRand();
			}
			g_objectTable[effectSlot].angleD = 0;
			g_objectTable[effectSlot].mobj->orientMatrixDirty = 1;
			g_objectTable[effectSlot].mobj->moveVectorDirty = 1;
		}

		// Trailing impact SFX / force feedback when a hit was registered.
#ifdef XWA_MODERN
		if (hitRegistered) {
#else
		if (((uint8_t*)&targetObjIdx)[3]) {
#endif
			if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer) {
				fsfx_PlaySound((int)((uint16_t)GameRand2() % 3) + 40, targetObjIdx, g_localPlayer);
				return;
			}
			if (effectSlot != 0xFFFF) {
				if (g_objectTable[effectSlot].objectType == OBJ_SparkTextureGroup3000 ||
					g_objectTable[effectSlot].objectType == OBJ_SparkTextureGroup3001) {
					fsfx_PlaySound(34, effectSlot, g_localPlayer);
					return;
				}
				ForceFeedback_PlayProximityEffectForObject(1, effectSlot);
				fsfx_PlaySound(fsfx_PickRandomSmallExplosionSfx(), effectSlot, g_localPlayer);
			}
		}
	}
}

// FUNCTION: XWA 0x412280
// Apply radial proximity (splash) damage from sourceObjIdx to nearby active-region
// objects, excluding excludedObjIdx. Source cargo volatility scales maxDamage/radius;
// damage falls off quadratically with distance (octagonal distance approximation);
// non-linked containers also receive a velocity impulse away from the blast. Returns
// the active-region craft slot end (loop bound), matching the original.
int collide_ApplyProximityDamageFalloff(unsigned int sourceObjIdx, unsigned int maxDamage,
										unsigned int radius, int excludedObjIdx) {
	int sourceX;
	int sourceY;
	int sourceZ;
	unsigned int idx;
	MobileObject* targetMobj;
	MobileObject* sourceMobj;
	unsigned int dmg;

	sourceMobj = g_objectTable[sourceObjIdx].mobj;
	sourceX = g_objectTable[sourceObjIdx].world_x;
	sourceY = g_objectTable[sourceObjIdx].world_y;
	sourceZ = g_objectTable[sourceObjIdx].world_z;

	if (sourceMobj) {
		CraftData* srcCraft = sourceMobj->pCraft;
		if (srcCraft) {
			uint8_t cargoIdx = srcCraft->cargoIndex;
			if (cargoIdx != 0xFF) {
				uint8_t volatility = g_missionHeader.body.globalCargos[cargoIdx].volatility;
				switch (volatility) {
					case 0:
						DebugPrintfChannel(0x1000, "Decreasing proximity damage due to Low volatility...\n");
						maxDamage /= 3u;
						radius /= 3u;
						break;
					case 2:
						DebugPrintfChannel(0x1000, "Increasing proximity damage due to High volatility...\n");
						maxDamage *= 2;
						radius *= 2;
						break;
					case 3:
						DebugPrintfChannel(0x1000,
										   "Increasing proximity damage due to \"Kaboom!\" volatility...\n");
						maxDamage *= 3;
						radius *= 3;
						break;
				}
			}
		}
	}

	for (idx = g_activeRegionObjectSlotStart; idx < g_activeRegionCraftObjectSlotEnd; ++idx) {
		if ((uint16_t)g_objectTable[idx].objectType && idx != (unsigned int)excludedObjIdx &&
			g_objectTable[idx].genusId != 13 && (uint16_t)g_objectTable[idx].objectType != 326) {
			int targetX;
			int targetY;
			int targetZ;

			targetMobj = g_objectTable[idx].mobj;
			if (targetMobj) {
				if ((targetMobj->framesAlive < 0xA && ((uint16_t)g_objectTable[idx].objectType == 47 ||
													   (uint16_t)g_objectTable[idx].objectType == 48)) ||
					((uint16_t)g_objectTable[idx].objectType >= 0x196u &&
					 (uint16_t)g_objectTable[idx].objectType <= 0x1A1u) ||
					(targetMobj->pCraft &&
					 (targetMobj->pCraft->objectKind == 3 || targetMobj->pCraft->objectKind == 4)))
					continue;

				targetX = targetMobj->prevWorldX;
				targetY = targetMobj->prevWorldY;
				targetZ = targetMobj->prevWorldZ;
			} else {
				Mission_ResolveObjectOrMissionPointWorldLoc(idx, 0, 0, 0);
				targetX = worldlocx;
				targetY = worldlocy;
				targetZ = worldlocz;
			}

			{
				int dz;
				int dy;
				int dx;
				int adx;
				int ady;
				int adz;
				unsigned int dist;

				dz = sourceZ - targetZ;
				dy = sourceY - targetY;
				dx = sourceX - targetX;
				adz = dz;
				adx = dx;
				if (adx < 0)
					adx = -adx;
				ady = dy;
				if (ady < 0)
					ady = -ady;
				if (adz < 0)
					adz = -adz;

				/* Octagonal distance: largest axis plus a quarter of the other two. */
				if (ady < adx && adx > adz) {
					dist = (adz >> 2) + adx + (ady >> 2);
				} else if ((ady <= adx) || (ady <= adz)) {
					dist = (ady >> 2) + adz + (adx >> 2);
				} else {
					dist = (adz >> 2) + ady + (adx >> 2);
				}

				if (g_objectTable[idx].genusId == 5) {
					unsigned int half =
						g_modelTypeTable[(uint16_t)g_objectTable[idx].objectType].maxBoundsExtent / 2;
					if (half > dist)
						dist = 0;
					else
						dist -= half;
				}

				if (dist < radius) {
					double ratio;
					double falloff;

					ratio = (double)dist / (double)radius;
					falloff = g_proximityDamageFullScale - ratio;
					dmg = (unsigned int)(int64_t)((double)maxDamage * (falloff * falloff));
					if (dmg > 0) {
						g_applyingProximityDamage = 1;
						collide_damagecraft(idx, 0xFFFFu, sourceObjIdx, dmg, 0);
						g_applyingProximityDamage = 0;

						if (g_objectTable[idx].genusId == 17 &&
							g_objectTable[idx].objectType != OBJ_ContainerSphere &&
							!g_objectTable[idx].mobj->pCraft->linkSequenceIndex) {
							uint16_t newVel = 128;
							if (32 * dmg <= 0xFFFF) {
								DebugPrintfChannel(0x1000, "Velocity adjusted by %d damage from %d.", dmg,
												   newVel);
								newVel = (uint16_t)MATH2_fraction(0x80u, (uint16_t)(32 * dmg));
							}
							DebugPrintfChannel(0x1000, "Final velocity set to %d.\n", newVel);
							if (newVel < targetMobj->velocityOverrideSpeed &&
								targetMobj->velocityOverrideActive) {
								DebugPrintfChannel(
									0x1000,
									"Old velocity (%d) overrides new velocity; new changes ignored.\n",
									targetMobj->velocityOverrideSpeed);
							} else {
								trig2_ctop(targetX, targetY, targetZ);
								if (trig2_polardistance > 0) {
									targetMobj->velocityOverrideActive = 1;
									targetMobj->velocityOverrideDirX = -65535 * dx / trig2_polardistance;
									targetMobj->velocityOverrideDirY = -65535 * dy / trig2_polardistance;
									targetMobj->velocityOverrideDirZ =
										(int16_t)(-65535 * dz / trig2_polardistance);
									targetMobj->velocityOverrideSpeed = newVel;
									targetMobj->velocityOverrideElapsed = 0;
									targetMobj->velocityOverrideDuration = 16;
								}
							}
						}
					}
				}
			}
		}
	}
	return g_activeRegionCraftObjectSlotEnd;
}

// FUNCTION: XWA 0x412220
// Apply proximity damage using the engine default radius derived from maxDamage:
// radius = clamp(maxDamage * 2/3, 1, 0x8000).
int collide_ApplyDefaultProximityDamage(unsigned int sourceObjIdx, unsigned int maxDamage,
										int excludedObjIdx) {
	unsigned int radius = 2 * maxDamage / 3;

	if (radius < 1) {
		radius += 1;
		return collide_ApplyProximityDamageFalloff(sourceObjIdx, maxDamage, radius, excludedObjIdx);
	}
	if (radius > 0x8000)
		radius = 0x8000;
	return collide_ApplyProximityDamageFalloff(sourceObjIdx, maxDamage, radius, excludedObjIdx);
}

// FUNCTION: XWA 0x4E4480
// Apply a projectile/object hit to a static or special object slot (XWA equivalent of
// TIE's static_laserhitstatic): update mission flight-group outcome stats, handle mine
// retaliation and DeathStar power nodes, select ion vs normal impact effects, apply
// warhead proximity damage, and spawn the impact effect object with SFX/force feedback.
void static_laserhitstatic(unsigned int sourceObjIdx, unsigned int staticObjIdx,
						   unsigned int hitComponentId) {
	uint16_t effectType;
	uint16_t genusId;
	unsigned int genus;

	genusId = g_objectTable[staticObjIdx].genusId;
	genus = genusId;
	if (genus != GENUS_Asteroid) {
		if (genus != GENUS_DeathStarTunnelSegment) {
			if (sourceObjIdx >= g_activeRegionObjectSlotStart &&
				sourceObjIdx < g_activeRegionCraftObjectSlotEnd) {
				uint16_t flightGroupIdx = g_objectTable[staticObjIdx].flightGroupIdx;
				++g_missionFgStats[flightGroupIdx].outcomeCount[2];
				effectType = OBJ_ExplosionTextureGroup2002;
				g_objectTable[staticObjIdx].objectType = OBJ_None;
				Mission_CreditDestructionDamageContributors((uint16_t)sourceObjIdx, (uint16_t)staticObjIdx);
			} else {
				uint16_t srcType = g_objectTable[sourceObjIdx].objectType;
				if (srcType != OBJ_LaserIon && srcType != OBJ_LaserIonTurbo) {
					uint16_t flightGroupIdx = g_objectTable[staticObjIdx].flightGroupIdx;
					++g_missionFgStats[flightGroupIdx].outcomeCount[2];
					effectType = OBJ_ExplosionTextureGroup2002;
					if (g_objectTable[staticObjIdx].objectType == OBJ_Mine3)
						laser_createprojectilefromstatic((uint16_t)staticObjIdx,
														 g_objectTable[sourceObjIdx].mobj->sourceObjIdx);
					g_objectTable[staticObjIdx].objectType = OBJ_None;
					Mission_CreditDestructionDamageContributors(
						g_objectTable[sourceObjIdx].mobj->sourceObjIdx, (uint16_t)staticObjIdx);
				} else {
					g_objectTable[staticObjIdx].typeSpecificWord = 0;
					++g_missionFgStats[g_objectTable[staticObjIdx].flightGroupIdx].outcomeCount[14];
					effectType = OBJ_SparkTextureGroup3001;
				}
			}
		} else {
			effectType = (ObjectTypeId)(uint16_t)DeathStar_HandlePowerNodeHit(sourceObjIdx, staticObjIdx,
																			  hitComponentId);
		}
	} else {
		uint16_t srcType;
		srcType = g_objectTable[sourceObjIdx].objectType;
		if (srcType == OBJ_LaserIon || srcType == OBJ_LaserIonTurbo) {
			effectType = OBJ_SparkTextureGroup3001;
		} else {
			effectType = OBJ_SparkTextureGroup3000;
		}
	}

#ifdef XWA_MODERN
	if (laser_GetProjectileWarheadClass(g_objectTable[sourceObjIdx].objectType) > 0) {
#else
	if (g_projectileWarheadClassByType[(uint16_t)g_objectTable[sourceObjIdx].objectType - OBJ_LaserRebel]) {
#endif
		effectType = OBJ_ExplosionTextureGroup2002;
		collide_ApplyDefaultProximityDamage(sourceObjIdx, g_objectTable[sourceObjIdx].mobj->damageAmount,
											(int)staticObjIdx);
	}

	if (effectType) {
		if (sourceObjIdx >= g_activeRegionObjectSlotStart &&
			sourceObjIdx < g_activeRegionCraftObjectSlotEnd) {
			sourceObjIdx = (uint16_t)Object_AllocSlotForGenus(GENUS_Explosion);
			if (sourceObjIdx == 0xFFFF)
				return;
			g_objectTable[sourceObjIdx].objectType = OBJ_LaserRebel;
		}

		g_objectTable[sourceObjIdx].world_x = g_collisionSegmentStartWorldX + g_collisionHitOffsetX;
		g_objectTable[sourceObjIdx].world_y = g_collisionSegmentStartWorldY + g_collisionHitOffsetY;
		g_objectTable[sourceObjIdx].world_z = g_collisionSegmentStartWorldZ + g_collisionHitOffsetZ;
		g_objectTable[sourceObjIdx].objectType = effectType;
		g_objectTable[sourceObjIdx].genusId = GENUS_Explosion;
		g_objectTable[sourceObjIdx].mobj->state = 5;
		g_objectTable[sourceObjIdx].typeSpecificByte[0] = 1;
		g_objectTable[sourceObjIdx].mobj->speed = 0;
		g_objectTable[sourceObjIdx].mobj->instanceExtent =
			g_modelTypeTable[(uint16_t)g_objectTable[sourceObjIdx].objectType].maxBoundsExtent;
		g_objectTable[sourceObjIdx].mobj->framesAlive = 0;
		g_objectTable[sourceObjIdx].mobj->lifetimeTimer = 0;
		g_objectTable[sourceObjIdx].mobj->sourceObjIdx = -1;
		g_objectTable[sourceObjIdx].pitch = 0;
		g_objectTable[sourceObjIdx].yaw = 0;
		g_objectTable[sourceObjIdx].roll = 0;
		g_objectTable[sourceObjIdx].angleD = 0;
		g_objectTable[sourceObjIdx].mobj->orientMatrixDirty = 1;
		g_objectTable[sourceObjIdx].mobj->moveVectorDirty = 1;
		if (genusId == GENUS_DeathStarTunnelSegment &&
			(uint16_t)effectType >= OBJ_ExplosionTextureGroup2000 &&
			(uint16_t)effectType <= OBJ_ExplosionTextureGroup2005)
			*(unsigned int*)&g_objectTable[sourceObjIdx].mobj->instanceExtent >>= 2;

		if (effectType == OBJ_SparkTextureGroup3000 || effectType == OBJ_SparkTextureGroup3001) {
			fsfx_PlaySound(34, sourceObjIdx, g_localPlayer);
			return;
		}
		fsfx_PlaySound(fsfx_PickRandomSmallExplosionSfx(), sourceObjIdx, g_localPlayer);
		ForceFeedback_PlayProximityEffectForObject(1, sourceObjIdx);
	}
}

// FUNCTION: XWA 0x4E28F0
// Apply engine-wash damage from sourceObjIdx to victimObjIdx when the victim lies
// inside one of the source's engine mesh plumes. Transforms the victim into the
// source's local axes, tests each MESH_Engine box with an expanding wake volume,
// scales intensity by the engine emitter's health, records the strongest player
// engine-wash source, halves damage 8x for unshielded victims, then applies it via
// the synthetic-starship-damage path of collide_damagecraft.
void collide_ApplyEngineWashDamage(int victimObjIdx, int sourceObjIdx) {
	ObjectRecord* sourceObj = &g_objectTable[sourceObjIdx];
	int sourceBoundsExtent;
	int localUp;
	int deltaX;
	int deltaY;
	int deltaZ;
	MobileObject* sm;
	int localSide, localForward;
	CraftData* sourceCraft;
	int meshIndex;
	int meshCount;

	{
		uint16_t sourceType = sourceObj->objectType;
		sourceBoundsExtent = g_modelTypeTable[sourceType].maxBoundsExtent;
	}
	deltaX = g_objectTable[victimObjIdx].world_x - sourceObj->world_x;
	deltaY = g_objectTable[victimObjIdx].world_y - sourceObj->world_y;
	deltaZ = g_objectTable[victimObjIdx].world_z - sourceObj->world_z;

	if (collide_roughdistance3d(deltaX, deltaY, deltaZ) > 3 * sourceBoundsExtent)
		return;
	sm = sourceObj->mobj;
	if (!sm)
		return;
	if (sm->orientMatrixDirty) {
		FVIEW_calcrotatemove(sourceObj->pitch, sourceObj->yaw, sourceObj);
		FVIEW_calcrotateorient(sourceObj->roll, sourceObj->angleD, sourceObj);
	}

	localSide = Xwa_Dot3Q15ReuseXSlot(deltaX, deltaY, deltaZ, sourceObj->mobj->cachedSideX,
									  sourceObj->mobj->cachedSideY, sourceObj->mobj->cachedSideZ);
	localForward = -Xwa_Dot3Q15Inline(deltaX, deltaY, deltaZ, sourceObj->mobj->cachedFwdX,
									  sourceObj->mobj->cachedFwdY, sourceObj->mobj->cachedFwdZ);
	if (localForward < -sourceBoundsExtent)
		return;
	localUp = Xwa_Dot3Q15ReuseXSlot(deltaX, deltaY, deltaZ, sourceObj->mobj->cachedUpX,
									sourceObj->mobj->cachedUpY, sourceObj->mobj->cachedUpZ);

	sourceCraft = sourceObj->mobj->pCraft;
	meshCount = ModelMesh_GetObjectTypeMeshCount(sourceObj->objectType);
	for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
		MeshDescriptor* desc = ModelMesh_GetObjectTypeMeshDescriptor(sourceObj->objectType, meshIndex);
		uint16_t objType;
		int xExtent, zExtent, engineMeshExtent;
		int washLength, depthIntoWash;
		double intensity;
		ModelIndex modelIdx;
		int washDamage, playerOwnerIdx;
		MobileObject* vm;

		if (!desc)
			continue;
		if (desc->meshType != MESH_Engine) {
			DebugPrintf((const char*)(uintptr_t)sourceObj->objectType, meshIndex);
			continue;
		}

		engineMeshExtent = (int)(desc->boxMax.y - desc->boxMin.y);
		objType = sourceObj->objectType;
		if (objType == OBJ_SuperStarDestroyer)
			engineMeshExtent >>= 6;
		xExtent = (int)(desc->boxMax.x - desc->boxMin.x);
		zExtent = (int)(desc->boxMax.z - desc->boxMin.z);
		if (objType == OBJ_SuperStarDestroyer && (float)localUp > desc->boxMax.z) {
			DebugPrintf((const char*)(uintptr_t)0x8C, meshIndex);
			continue;
		}
		if (objType == OBJ_CalamariCruiserNew && desc->center.z < g_collideZeroFloat &&
			(float)localUp > desc->boxMax.z) {
			DebugPrintf((const char*)(uintptr_t)0x87, meshIndex);
			continue;
		}

		if (xExtent > engineMeshExtent)
			engineMeshExtent = xExtent;
		if (zExtent > engineMeshExtent)
			engineMeshExtent = zExtent;
		washLength = 8 * engineMeshExtent;
		if (washLength > 2 * sourceBoundsExtent)
			washLength = 2 * sourceBoundsExtent;
		if (objType == OBJ_SuperStarDestroyer) {
			depthIntoWash = localForward + (washLength >> 8) - (int)desc->boxMax.y;
		} else {
			depthIntoWash = localForward - (int)desc->boxMin.y;
		}
		if (depthIntoWash <= 0 || depthIntoWash > washLength) {
			DebugPrintf((const char*)(uintptr_t)(uint16_t)objType, meshIndex);
			continue;
		}

		{
			int offSide = localSide - (int)desc->center.x;
			int offUp = localUp - (int)desc->center.z;
			int extentExpansion;
			xExtent += xExtent >> 1;
			extentExpansion = xExtent * depthIntoWash / washLength;
			xExtent += extentExpansion;
			zExtent += zExtent >> 1;
			extentExpansion = zExtent * depthIntoWash / washLength;
			zExtent += extentExpansion;
			if (offSide < 0)
				offSide = -offSide;
			if (offUp < 0)
				offUp = -offUp;
			if (offSide > xExtent || offUp > zExtent) {
				DebugPrintf((const char*)(uintptr_t)(uint16_t)objType, meshIndex);
				continue;
			}

			DebugPrintf((const char*)(uintptr_t)(uint16_t)objType, meshIndex);
			modelIdx = GetModelIndexFromType(sourceObj->objectType);
			intensity = g_engineWashFullIntensity;
			if (modelIdx != (ModelIndex)0xFFFF) {
				unsigned int g;
				for (g = 0; g < g_modelDefs[modelIdx].engineGlowCount; ++g) {
					if (g_modelDefs[modelIdx].engineGlowMeshIdx[g] == meshIndex) {
						intensity = (double)(sourceCraft->engineEmitterHealth[g] /
											 (int)g_modelDefs[modelIdx].componentMaxHp);
						break;
					}
				}
			}
			if (intensity < g_engineWashMinimumIntensity)
				continue;

			washDamage = (washLength >> 8) *
						 (100 * (washLength - depthIntoWash) / washLength *
						  (100 * (zExtent + xExtent - offSide - offUp) / (zExtent + xExtent)) / 100) /
						 100;
		}
		if (sourceObj->objectType == OBJ_SuperStarDestroyer) {
			washDamage >>= 4;
			washDamage += washDamage >> 1;
		}
		washDamage = (int)((double)washDamage * intensity);
		if (washDamage > 64)
			washDamage = 64;

		playerOwnerIdx = g_objectTable[victimObjIdx].playerOwnerIdx;
		if (playerOwnerIdx != -1 && washDamage > g_players[playerOwnerIdx].engineWashStrength) {
			g_players[playerOwnerIdx].engineWashSourceObjIdx = (ObjectIndex)sourceObjIdx;
			g_players[playerOwnerIdx].engineWashStrength = (uint16_t)washDamage;
		}

		vm = g_objectTable[victimObjIdx].mobj;
		if (vm) {
			CraftData* vc = vm->pCraft;
			if (vc && !(vc->shieldFront + vc->shieldRear))
				washDamage >>= 3;
		}
		if (washDamage < 1)
			washDamage = 1;
		collide_damagecraft(victimObjIdx, 0xFFFFu, 0xFFFFFFFEu, washDamage, 0);
	}
}

// FUNCTION: XWA 0x40B4F0
// Apply a ricochet/deflection to objectObjIdx after it hits hitAgainstObjIdx: reflect
// the object's velocity-override direction about the impact surface normal (in the
// target's local frame), reposition it just off the surface, apply synthetic collision
// damage scaled by speed/hull/incidence, and emit hit SFX plus an optional spark
// particle effect. The original's char return is discarded by every caller (vestigial).
void collide_applySurfaceRicochet(unsigned int objectObjIdx, unsigned int hitAgainstObjIdx) {
	MobileObject* mobj = g_objectTable[objectObjIdx].mobj;
	Vec3i outDir;
	int outLocalX, outLocalY, outLocalZ;
	float mx, my, mz, mag;
	int normX, normY, normZ;
	int dot, absDot;
	int proj;
	int reflX, reflY, reflZ;
	float rx;
	float ry, rz;
	float mag2;
	uint16_t clampedSpeed;
	int durationScale;
	int surfaceBackoff;
	uint16_t hitType;

	collide_TransformHitIntoObjectLocalFrame(hitAgainstObjIdx, &outDir.x, &outDir.y, &outDir.z, &outLocalX,
											 &outLocalY, &outLocalZ);

	mx = (float)((int)mobj->speed * mobj->moveX +
				 (int)mobj->velocityOverrideSpeed * mobj->velocityOverrideDirX);
	my = (float)((int)mobj->speed * mobj->moveY +
				 (int)mobj->velocityOverrideSpeed * mobj->velocityOverrideDirY);
	mz = (float)((int)mobj->speed * mobj->moveZ +
				 (int)mobj->velocityOverrideSpeed * mobj->velocityOverrideDirZ);
	mag = (float)sqrt(mx * mx + my * my + mz * mz) * g_collideRicochetQ15Scale;
	normZ = (int16_t)(int)(mz / mag);
	normY = (int16_t)(int)(my / mag);
	normX = (int16_t)(int)(mx / mag);

	dot = Xwa_Q15MulInline(-outDir.z, normZ) + Xwa_Q15MulInline(-outDir.y, normY) +
		  Xwa_Q15MulInline(-outDir.x, normX);
	absDot = (dot <= 0) ? -dot : dot;
	proj = (int)((float)absDot * mag * g_collideRicochetProjectionScale);

#ifdef XWA_MODERN
	reflX = (int)((uint32_t)collide_MulWrap32(proj, outDir.x) +
				  (uint32_t)collide_MulWrap32((int)mobj->velocityOverrideSpeed, mobj->velocityOverrideDirX));
	reflY = (int)((uint32_t)collide_MulWrap32(proj, outDir.y) +
				  (uint32_t)collide_MulWrap32((int)mobj->velocityOverrideSpeed, mobj->velocityOverrideDirY));
	reflZ = (int)((uint32_t)collide_MulWrap32(proj, outDir.z) +
				  (uint32_t)collide_MulWrap32((int)mobj->velocityOverrideSpeed, mobj->velocityOverrideDirZ));
#else
	reflX = proj * outDir.x + mobj->velocityOverrideSpeed * mobj->velocityOverrideDirX;
	reflY = proj * outDir.y + mobj->velocityOverrideSpeed * mobj->velocityOverrideDirY;
	reflZ = proj * outDir.z + mobj->velocityOverrideSpeed * mobj->velocityOverrideDirZ;
#endif
	rx = (float)reflX;
	ry = (float)reflY;
	rz = (float)reflZ;
	mag2 = (float)sqrt(rx * rx + ry * ry + rz * rz) * g_collideRicochetQ15Scale;

	mobj->velocityOverrideDirX = (int16_t)(int)(rx / mag2);
	mobj->velocityOverrideDirY = (int16_t)(int)(ry / mag2);
	mobj->velocityOverrideDirZ = (int16_t)(int)(rz / mag2);
	clampedSpeed = (uint16_t)(int)mag2;
	mobj->velocityOverrideSpeed = clampedSpeed;
	mobj->velocityOverrideElapsed = 0;
	mobj->velocityOverrideActive = 1;

	if (clampedSpeed > 1)
		durationScale = clampedSpeed;
	else
		durationScale = 1;
	if (durationScale >= 20)
		durationScale = 20;
	mobj->velocityOverrideDuration = (uint16_t)(1000 * durationScale);
	mobj->spinRate = -mobj->spinRate;
	surfaceBackoff = 20;

	g_objectTable[objectObjIdx].world_x =
		outLocalX + Xwa_Q15Mul(surfaceBackoff, outDir.x) + g_objectTable[hitAgainstObjIdx].world_x;
	g_objectTable[objectObjIdx].world_y =
		outLocalY + Xwa_Q15Mul(surfaceBackoff, outDir.y) + g_objectTable[hitAgainstObjIdx].world_y;
	g_objectTable[objectObjIdx].world_z =
		outLocalZ + Xwa_Q15Mul(surfaceBackoff, outDir.z) + g_objectTable[hitAgainstObjIdx].world_z;
	if (g_collideSweepAuxHardpointIdx != -1) {
		int worldX = g_objectTable[objectObjIdx].world_x;
		int worldY = g_objectTable[objectObjIdx].world_y;
		worldX -= g_collideSweepAuxHardpointWorldOffsetX;
		worldY -= g_collideSweepAuxHardpointWorldOffsetY;
		g_objectTable[objectObjIdx].world_x = worldX;
		g_objectTable[objectObjIdx].world_y = worldY;
		g_objectTable[objectObjIdx].world_z -= g_collideSweepAuxHardpointWorldOffsetZ;
	}

	hitType = g_objectTable[hitAgainstObjIdx].objectType;
	if (hitType >= OBJ_Junk01 && hitType <= OBJ_Junk10)
		g_objectTable[hitAgainstObjIdx].mobj->rollImpulseRate = 900;

	if (g_objectTable[objectObjIdx].genusId == GENUS_SalvageJunk)
		return;

	{
		CraftData* objCraft = g_objectTable[objectObjIdx].mobj->pCraft;
		unsigned int hullStrength;
		float dotScale = (float)dot * g_collideRicochetQ15Scale;
		double speedRatio;
		float dmg;
		hullStrength = (unsigned int)g_modelDefs[objCraft->modelIndex].hullStrength;
		speedRatio = (double)g_objectTable[objectObjIdx].mobj->speed /
					 (double)(uint16_t)objCraft->aiFlight.maxSpeedCache;
		dmg = speedRatio * (double)hullStrength * (double)(dotScale * dotScale) *
			  (double)(g_collideRicochetDamageScale * 1.5f);
		if (dmg >= g_collideRicochetMaximumDamage)
			dmg = g_collideRicochetMaximumDamage;
		collide_damagecraft(objectObjIdx, 0xFFFFu, 0xFFFFFFFDu, (unsigned int)(int)dmg, 0);
	}

	{
		MobileObject* om = g_objectTable[objectObjIdx].mobj;
		if (om) {
			CraftData* oc = om->pCraft;
			if (!oc)
				return;
			if (oc->objectKind >= 3) {
				if (oc->objectKind > 4) {
					if (oc->objectKind == 8) {
						oc->objectKind = 0;
						g_objectTable[objectObjIdx].mobj->speed /= 10;
						g_objectTable[objectObjIdx].mobj->velocityOverrideSpeed /= 10;
					}
				} else {
					oc->breakupYawRate = 0;
					oc->breakupPitchRate = 0;
					oc->objectKind = 4;
					g_objectTable[objectObjIdx].mobj->velocityOverrideActive = 0;
					g_objectTable[objectObjIdx].mobj->lifetimeTimer = 1;
				}
			}
		}
	}

	if (objectObjIdx == (unsigned int)g_players[g_localPlayer].objectIndex)
		fsfx_PlaySound((uint16_t)GameRand2() % 3 + 45, objectObjIdx,
					   g_objectTable[objectObjIdx].playerOwnerIdx);

	if (g_useHardware3D && (g_players[g_localPlayer].viewState.externalCameraActive ||
							objectObjIdx != (unsigned int)g_players[g_localPlayer].objectIndex ||
							(g_filmPlaybackMode != 0 && g_filmOverlayActive == 1))) {
		if (g_objectTable[objectObjIdx].mobj->speed > 0x32u) {
			ParticleEffect* fx =
				Particle_AttachEffectToObject(5, (uint16_t)hitAgainstObjIdx, &g_collisionImpactEffectCenter,
											  &g_collisionImpactEffectNormal);
			if (fx) {
				int halfLife = fx->lifetimeTicks / 2;
				fx->lifetimeTicks = halfLife;
				if (halfLife > fx->def->particleLifetimeTicks)
					fx->emitUntilTicks = halfLife - fx->def->particleLifetimeTicks;
				else
					fx->emitUntilTicks = 1;
			}
		}
	}
}

// Refresh a player's per-tick engine-wash state and reapply wash from nearby large craft.
// Mirrors the engine-wash pass at the top of collide_collisions' player branch.
static __inline void collide_RefreshPlayerEngineWash(unsigned int ownerObjIdx, int playerIdx) {
	unsigned int idx;

	g_players[playerIdx].engineWashSourceObjIdx = -1;
	g_players[playerIdx].engineWashStrength = 0;
	g_players[playerIdx].impactDamageCooldownTime = g_gameTime + 29;

	for (idx = g_activeRegionObjectSlotStart; idx < g_activeRegionCraftObjectSlotEnd; ++idx) {
		ObjectRecord* o = &g_objectTable[idx];
		CraftData* c;
		ModelGenusId g;
		if (o->objectType == OBJ_None) {
			continue;
		}
		c = o->mobj->pCraft;
		if (!c || !c->workingSubsystems || !c->engineOutputScale) {
			continue;
		}
		g = o->genusId;
		if (g >= GENUS_Freighter && g <= GENUS_Starship) {
			collide_ApplyEngineWashDamage(ownerObjIdx, idx);
		}
	}
}

// Find candidateObjIdx in the proximity list and shift the remaining entries down, dropping the count.
// Reproduces the in-place removal used at the invalid-candidate and dead-candidate sites.
static __inline void collide_RemoveProximityEntry(MobileObjectProximityList* list,
												  unsigned int candidateObjIdx) {
	int count = list->count;
	int pos;
	int i;

	for (pos = 0; pos < count; ++pos) {
		if (list->objIdx[pos] == candidateObjIdx) {
			break;
		}
	}
	if (pos == count) {
		return;
	}
	for (i = pos + 1; i < list->count; ++i) {
		list->objIdx[i - 1] = list->objIdx[i];
		list->score[i - 1] = list->score[i];
	}
	--list->count;
}

// Per-player target inspection: when the player's current target is an unidentified craft, mark it
// partially or fully inspected based on range, accumulate per-flight-group/team inspection stats and
// goal scoring, and emit the inspection messages/SFX. Assumes the collision probe globals already hold
// the owner object's world/previous-world position.
static __inline void collide_HandleTargetInspection(int playerIdx, unsigned int objIdx) {
	uint16_t targetIdx;
	CraftData* tcraft;
	uint8_t objectKind;
	unsigned int broadRadius;
	int fg;
	uint8_t maxLevel;
	int t;
	int specialCargoFlag;
	int goalScore;
	int inspectGoalMatch;

	targetIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
	if (targetIdx < (unsigned int)g_activeRegionObjectSlotStart ||
		targetIdx >= g_activeRegionCraftObjectSlotEnd) {
		return;
	}

	if (g_objectTable[targetIdx].objectType == OBJ_None ||
		g_objectTable[targetIdx].genusId == GENUS_Explosion) {
		return;
	}

	g_collisionSweepEndX = g_objectTable[targetIdx].world_x;
	g_collisionSweepEndY = g_objectTable[targetIdx].world_y;
	g_collisionSweepEndZ = g_objectTable[targetIdx].world_z;
	g_collisionSweepStartX = g_objectTable[targetIdx].mobj->prevWorldX;
	g_collisionSweepStartY = g_objectTable[targetIdx].mobj->prevWorldY;
	g_collisionSweepStartZ = g_objectTable[targetIdx].mobj->prevWorldZ;

	g_approxDist = collide_roughdistance3d_inline(g_collisionProbeWorldX - g_collisionSweepEndX,
												  g_collisionProbeWorldY - g_collisionSweepEndY,
												  g_collisionProbeWorldZ - g_collisionSweepEndZ);

	tcraft = g_objectTable[targetIdx].mobj->pCraft;
	objectKind = tcraft->objectKind;
	if (objectKind == 3 || objectKind == 4 ||
		(int8_t)tcraft->iffVisibility[g_objectTable[objIdx].mobj->team] >= 1) {
		return;
	}

	broadRadius =
		4u * (unsigned int)g_modelTypeTable[(uint16_t)g_objectTable[targetIdx].objectType].maxBoundsExtent;
	if (broadRadius < 0x2800u) {
		broadRadius = 10240;
	}
	if (broadRadius > 0xA000u) {
		broadRadius = 40960;
	}

	if ((unsigned int)g_approxDist < broadRadius) {
		// Close range: full inspection.
		maxLevel = 0;
		t = 10;
		do {
			if ((int8_t)tcraft->iffVisibility[10 - t] > (int)maxLevel) {
				maxLevel = tcraft->iffVisibility[10 - t];
			}
		} while (--t);
		++maxLevel;
		tcraft->iffVisibility[(uint16_t)g_players[playerIdx].playerIff] = maxLevel;

		fg = g_objectTable[targetIdx].flightGroupIdx;
		++g_missionFlightRuntimeState
			  .teamFgCounters[TEAM_FG_COUNTER_INSPECTED][(uint16_t)g_players[playerIdx].playerIff][fg];
		++g_missionFgStats[fg].outcomeCount[8];
		++g_missionFgStats[fg].teamInspected[(uint16_t)g_players[playerIdx].playerIff];
		++g_players[playerIdx].perMissionKills.field2;

		specialCargoFlag = 0;
		if (g_missionFlightGroups[fg].fg.specialCargoCraft == tcraft->waveNumber) {
			g_missionFgStats[fg].specialCargoOutcome[8] = 1;
			specialCargoFlag = 1;
			++g_missionFgStats[fg].teamSpecialCargoInspected[(uint16_t)g_players[playerIdx].playerIff];
			++g_players[playerIdx].perMissionKills.numSpecialInspected;
		}

		goalScore =
			Mission_ApplyFlightGroupGoalScore(5, (uint16_t)fg, playerIdx, maxLevel - 1, specialCargoFlag,
											  (uint16_t)g_players[playerIdx].playerIff);

		inspectGoalMatch = 0;
		if (fg < (int16_t)g_missionHeader.numFlightGroups) {
			XwaGoalFG* goals = g_missionFlightGroups[fg].fg.fgGoals;
			int gi;
			for (gi = 0; gi < 8; ++gi) {
				if (goals[gi].payload.enabledForTeam[(uint16_t)g_players[playerIdx].playerIff] &&
					(goals[gi].payload.condition == 5 ||
					 (goals[gi].payload.amount == 6 &&
					  (specialCargoFlag || !g_missionFgStats[fg].specialCargoOutcome[8]))) &&
					(goals[gi].payload.argument == 0 || goals[gi].payload.argument == 2)) {
					inspectGoalMatch = 1;
				}
			}
		}

		if (inspectGoalMatch && playerIdx == g_localPlayer) {
			if (specialCargoFlag) {
				msg_emitInFlightMessage(MSG_TARGET_INSPECT_SPC, g_localPlayer);
			} else {
				msg_emitInFlightMessage(MSG_TARGET_INSPECT_CMP, g_localPlayer);
			}
			g_playerFlightTransientTimers[g_localPlayer].targetDescriptionRefreshTimer = 944;
		}

		if ((!inspectGoalMatch && g_objectTable[targetIdx].genusId == 0 &&
			 (g_flightLocatePlayersEnabled || g_objectTable[targetIdx].playerOwnerIdx == -1)) ||
			playerIdx != g_localPlayer) {
			return;
		}

		msg_emitCraftMessage(targetIdx, tcraft, 150);
		fsfx_PlaySound(69, g_players[g_localPlayer].objectIndex, g_localPlayer);
		if (goalScore && g_flightPlayerCount > 1 &&
			(g_missionHeader.body.missionType == XWA_MISSION_TYPE_QUICK_START ||
			 g_missionHeader.body.missionType == XWA_MISSION_TYPE_SKIRMISH)) {
			g_msgArgTable[0] = (uint16_t)(maxLevel + 385);
			msg_emitInFlightMessage(MSG_INSPECT_PLACE, g_localPlayer);
		}
	} else {
		// Medium range: register a partial inspection.
		if ((unsigned int)g_approxDist >= 6u * broadRadius) {
			return;
		}
		if (tcraft->iffVisibility[(uint16_t)g_players[playerIdx].playerIff] == 0) {
			return;
		}
		tcraft->iffVisibility[(uint16_t)g_players[playerIdx].playerIff] = 0;

		fg = g_objectTable[targetIdx].flightGroupIdx;
		++g_missionFgStats[fg].outcomeCount[29];
		++g_missionFgStats[fg].teamPartiallyInspected[(uint16_t)g_players[playerIdx].playerIff];
		if (g_missionFlightGroups[fg].fg.specialCargoCraft == tcraft->waveNumber) {
			g_missionFgStats[fg].specialCargoOutcome[29] = 1;
			++g_missionFgStats[fg]
				  .teamSpecialCargoPartiallyInspected[(uint16_t)g_players[playerIdx].playerIff];
		}
		if (playerIdx != g_localPlayer) {
			return;
		}
		fsfx_PlaySound(69, g_players[g_localPlayer].objectIndex, g_localPlayer);
	}
}

// Offers the auto-land/tractor prompt when the player's craft drifts near a docking point of its
// departure (or alternate) mothership. *ioMirrorDist carries the running mirror-side distance used to
// decide which Calamari-cruiser docking offset is closer; it persists across objects exactly as the
// original v233 scratch does.
static __inline void collide_HandleDepartureDockPrompt(int playerIdx, unsigned int objIdx,
													   int* ioMirrorDist) {
	uint16_t slot;

	if (g_objectTable[objIdx].mobj->framesAlive < 15u || g_players[playerIdx].inputDisabledFlag) {
		return;
	}

	for (slot = 0; slot < 2; ++slot) {
		uint16_t mothershipFg;
		int mirrorDist;
		unsigned int idx;
		if (slot == 0) {
			uint8_t departMethod =
				g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.departMethod;
			if (departMethod == 0 || departMethod == 2) {
				continue;
			}
			mothershipFg = g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.departureMothership;
		} else {
			if (!g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.alternateMothershipUsed) {
				continue;
			}
			mothershipFg = g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.alternateMothership;
		}

		if (g_activeRegionObjectSlotStart >= g_activeRegionCraftObjectSlotEnd) {
			continue;
		}

		mirrorDist = *ioMirrorDist;
		for (idx = g_activeRegionObjectSlotStart; idx < g_activeRegionCraftObjectSlotEnd; ++idx) {
			ObjectRecord* cand = &g_objectTable[idx];
			CraftData* ccraft;
			unsigned int threshold;
			ObjectTypeId ot;
			int dist;
			if (cand->objectType == OBJ_None || cand->genusId == GENUS_Explosion ||
				cand->flightGroupIdx != mothershipFg) {
				continue;
			}

			g_collisionSweepEndX = cand->world_x;
			g_collisionSweepEndY = cand->world_y;
			g_collisionSweepEndZ = cand->world_z;
			g_collisionSweepStartX = cand->mobj->prevWorldX;
			g_collisionSweepStartY = cand->mobj->prevWorldY;
			g_collisionSweepStartZ = cand->mobj->prevWorldZ;

			ccraft = cand->mobj->pCraft;
			if (ccraft->objectKind) {
				continue;
			}

			threshold = 0x2000;
			if (cand->genusId == GENUS_Starship) {
				threshold =
					(g_missionFlightRuntimeState
						 .teamGoalStatus[(uint16_t)g_players[playerIdx].playerIff][TEAM_GOAL_PRIMARY] != 1)
						? 0x2000
						: 0x4000;
			}

			ot = cand->objectType;
			if (ot == OBJ_CalamariCruiserNew || ot == OBJ_CalamariWinged) {
				// Evaluate the mirrored (-X) docking offset first.
				pai_RotateLocalVectorToWorldScratch(cand, -g_modelDefs[ccraft->modelIndex].meshAttachData[5],
													g_modelDefs[ccraft->modelIndex].meshAttachData[6],
													g_modelDefs[ccraft->modelIndex].meshAttachData[7]);
				g_collisionSweepEndX += g_rotatedX;
				g_collisionSweepEndY += g_rotatedY;
				g_collisionSweepEndZ += g_rotatedZ;
				mirrorDist = collide_roughdistance3d_inline(g_collisionSweepEndX - g_collisionProbeWorldX,
															g_collisionSweepEndY - g_collisionProbeWorldY,
															g_collisionSweepEndZ - g_collisionProbeWorldZ);
				if ((unsigned int)mirrorDist < threshold) {
					g_hangarLaunchMirrorAttachOffsets = 1;
					if (!g_players[playerIdx].pendingActionId) {
						if (playerIdx == g_localPlayer) {
							msg_emitInFlightMessage(MSG_HANGAR_TRACTOR, g_localPlayer);
						}
						g_players[playerIdx].pendingActionId = 2;
						g_players[playerIdx].pendingActionParam = (int16_t)idx;
						g_players[playerIdx].pendingActionTimer = 1888;
					}
				}
				g_collisionSweepEndX -= g_rotatedX;
				g_collisionSweepEndY -= g_rotatedY;
				g_collisionSweepEndZ -= g_rotatedZ;
			}

			// Evaluate the primary (+X) docking offset.
			pai_RotateLocalVectorToWorldScratch(cand, g_modelDefs[ccraft->modelIndex].meshAttachData[5],
												g_modelDefs[ccraft->modelIndex].meshAttachData[6],
												g_modelDefs[ccraft->modelIndex].meshAttachData[7]);
			g_collisionSweepEndX += g_rotatedX;
			g_collisionSweepEndY += g_rotatedY;
			g_collisionSweepEndZ += g_rotatedZ;
			dist = collide_roughdistance3d_inline(g_collisionSweepEndX - g_collisionProbeWorldX,
												  g_collisionSweepEndY - g_collisionProbeWorldY,
												  g_collisionSweepEndZ - g_collisionProbeWorldZ);
			if ((unsigned int)dist < threshold) {
				if (dist < mirrorDist) {
					g_hangarLaunchMirrorAttachOffsets = 0;
				}
				if (!g_players[playerIdx].pendingActionId) {
					if (playerIdx == g_localPlayer) {
						msg_emitInFlightMessage(MSG_HANGAR_TRACTOR, g_localPlayer);
					}
					g_players[playerIdx].pendingActionId = 2;
					g_players[playerIdx].pendingActionParam = (int16_t)idx;
					g_players[playerIdx].pendingActionTimer = 1888;
				}
			}
		}
		*ioMirrorDist = mirrorDist;
	}
}

// Offers the "ask about hyperspace" prompt when the player's craft is near a hyperbuoy designating a
// reachable region and not already in or near hyperspace.
static __inline void collide_HandleHyperBuoyPrompt(int playerIdx, unsigned int objIdx) {
	unsigned int idx;

	if (g_players[playerIdx].hyperspacePhase != 0 ||
		g_players[playerIdx].hyperspaceRuntime.hyperBuoyPromptCooldown ||
		g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] == 1) {
		return;
	}

	for (idx = g_activeRegionObjectSlotStart; idx < g_activeRegionCraftObjectSlotEnd; ++idx) {
		ObjectRecord* o = &g_objectTable[idx];
		int region;
		if (o->objectType == OBJ_None || o->genusId != GENUS_SatelliteBuoy) {
			continue;
		}

		region = collide_GetHyperRegionDesignationForPlayer(objIdx, idx, 0);
		if (!region) {
			region = collide_GetHyperRegionDesignationForPlayer(objIdx, idx, 1);
			if (!region) {
				continue;
			}
		}

		pai_ObjectRefUpdateApproxRangeScore(objIdx, idx);
		if ((unsigned int)g_targetRangeScore < 0x6000 && !g_players[playerIdx].pendingActionId) {
			int regionIdx = region - 16;
			if (playerIdx == g_localPlayer) {
				msg_addMessagePtr(0, g_missionHeader.body.regions[regionIdx].name);
				msg_emitInFlightMessage(MSG_ASK_ABOUT_HYPER, g_localPlayer);
			}
			g_players[playerIdx].pendingActionId = 10;
			g_players[playerIdx].pendingActionParam = (int16_t)regionIdx;
			g_players[playerIdx].pendingActionTimer = 708;
		}
	}
}

// Mutual elastic-bump response between two small craft that touched without a hard ricochet. Pushes
// each craft's heading away from the contact, sets per-craft roll impulse and force feedback, records
// the opposing object in each craft's aiFlight.impactObjIdx, and plays the collision message/SFX. This
// is a distinct routine from collide_applyCraftImpactBounce: it uses current (not previous) world
// positions, quarter-speed scaling and 5x/100x roll tuning inside the Death Star, rather than the
// bounce routine's clamped speed / collide_ComputeRollImpulse handling.
static __inline void collide_ApplyCraftCraftBump(unsigned int srcObjIdx, unsigned int candObjIdx) {
	int srcSpeed;
	int candSpeed;
	int yawMagnitude;
	int16_t cosTerm;
	int v115;
	int relSpeed;
	int signedRelSpeed;
	int dx;
	int dy;
	int dz;
	int dxSq;
	int dySq;
	int dzSq;
	int distSq;
	int impX, impY;
	int16_t srcNewMoveX;
	int16_t srcNewMoveY;
	Q16Angle srcYawBefore;
	int candRoll;
	int srcRollMagnitude;
	int16_t candNewMoveX;
	int16_t candNewMoveY;
	Q16Angle candYawBefore;

	g_objectTable[srcObjIdx].mobj->pCraft->aiFlight.impactObjIdx = (uint16_t)candObjIdx;

	srcSpeed = (int16_t)g_objectTable[srcObjIdx].mobj->speed;
	candSpeed = (int16_t)g_objectTable[candObjIdx].mobj->speed;
	if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) {
		srcSpeed >>= 2;
		candSpeed >>= 2;
	}

	yawMagnitude = collide_FoldYawDeltaToQuarterTurn(
		(uint16_t)(g_objectTable[candObjIdx].yaw - g_objectTable[srcObjIdx].yaw));
	if (yawMagnitude < 0x4000) {
		cosTerm = trig2_cosinewordmult(candSpeed, yawMagnitude);
		v115 = srcSpeed - cosTerm;
	} else {
		cosTerm = trig2_cosinewordmult(candSpeed, yawMagnitude);
		v115 = srcSpeed + cosTerm;
	}
	relSpeed = ((int16_t)v115 < 0) ? -v115 : v115;
	signedRelSpeed = (int)(int16_t)relSpeed;

	dx = g_objectTable[srcObjIdx].world_x - g_objectTable[candObjIdx].world_x;
	dy = g_objectTable[srcObjIdx].world_y - g_objectTable[candObjIdx].world_y;
	dz = g_objectTable[srcObjIdx].world_z - g_objectTable[candObjIdx].world_z;
	dxSq = collide_MulWrap32(dx, dx);
	dySq = collide_MulWrap32(dy, dy);
	dzSq = collide_MulWrap32(dz, dz);
	distSq = (int)((uint32_t)dxSq + (uint32_t)dySq + (uint32_t)dzSq);

	if (distSq <= 50) {
		impX = 0;
		impY = 100;
	} else {
#ifdef XWA_MODERN
		dx = collide_ScaleBy1000Wrap32(collide_MulWrap32(dx, signedRelSpeed));
		dy = collide_ScaleBy1000Wrap32(collide_MulWrap32(dy, signedRelSpeed));
#else
		dx *= 1000 * signedRelSpeed;
		dy = 1000 * signedRelSpeed * dy;
#endif
		impX = dx / distSq;
		impY = dy / distSq;
	}

	// First half: deflect the source craft, and stamp the (transient) candidate pitch/roll + cache.
	if (g_objectTable[srcObjIdx].mobj->moveVectorDirty) {
		FVIEW_calcrotatemove(g_objectTable[srcObjIdx].pitch, g_objectTable[srcObjIdx].yaw,
							 &g_objectTable[srcObjIdx]);
	}
	srcNewMoveX = collide_AddMoveComponentNoSignOverflow(g_objectTable[srcObjIdx].mobj->moveX, impX);
	srcNewMoveY = collide_AddMoveComponentNoSignOverflow(g_objectTable[srcObjIdx].mobj->moveY, impY);

	srcYawBefore = g_objectTable[srcObjIdx].yaw;
	g_objectTable[srcObjIdx].yaw = trig2_arctan((int16_t)srcNewMoveX, (int16_t)srcNewMoveY);
	g_objectTable[candObjIdx].pitch = trig2_w_arcsin(srcNewMoveX);

	candRoll = 100 * relSpeed;
	srcRollMagnitude = 100 * relSpeed;
	if (g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) {
		candRoll = 5 * relSpeed;
	}
	if ((uint16_t)candRoll >= 0x8000) {
		candRoll = 0x7FFF;
	}
	if ((uint16_t)srcYawBefore < g_objectTable[candObjIdx].yaw) {
		candRoll = -candRoll;
	}
	g_objectTable[candObjIdx].mobj->rollImpulseRate = (int16_t)candRoll;
	if (g_objectTable[candObjIdx].playerOwnerIdx == g_localPlayer) {
		int16_t r = g_objectTable[candObjIdx].mobj->rollImpulseRate;
		if (r < 0) {
			ForceFeedback_PlayImpactEffect(270, -r);
		} else {
			ForceFeedback_PlayImpactEffect(90, r);
		}
	}
	FVIEW_calcrotatemove(g_objectTable[candObjIdx].pitch, g_objectTable[candObjIdx].yaw,
						 &g_objectTable[candObjIdx]);
	FVIEW_calcrotateorient(g_objectTable[candObjIdx].roll, g_objectTable[candObjIdx].angleD,
						   &g_objectTable[candObjIdx]);

	g_objectTable[candObjIdx].mobj->pCraft->aiFlight.impactObjIdx = (uint16_t)srcObjIdx;

	distSq = (int)((uint32_t)dxSq + (uint32_t)dySq + (uint32_t)dzSq);
	if (distSq <= 50) {
		impX = 0;
		impY = 100;
	} else {
#ifdef XWA_MODERN
		impX = collide_DivideWrap32(collide_ScaleBy1000Wrap32(collide_MulWrap32(dx, signedRelSpeed)), distSq);
		impY = collide_DivideWrap32(collide_ScaleBy1000Wrap32(collide_MulWrap32(dy, signedRelSpeed)), distSq);
#else
		impX = dx * (1000 * signedRelSpeed) / distSq;
		impY = dy * (1000 * signedRelSpeed) / distSq;
#endif
	}

	// Second half: deflect the candidate craft and apply roll impulse to the source craft.
	if (g_objectTable[candObjIdx].mobj->moveVectorDirty) {
		FVIEW_calcrotatemove(g_objectTable[candObjIdx].pitch, g_objectTable[candObjIdx].yaw,
							 &g_objectTable[candObjIdx]);
	}
	candNewMoveX = collide_AddMoveComponentNoSignOverflow(g_objectTable[candObjIdx].mobj->moveX, impX);
	candNewMoveY = collide_AddMoveComponentNoSignOverflow(g_objectTable[candObjIdx].mobj->moveY, impY);

	candYawBefore = g_objectTable[candObjIdx].yaw;
	g_objectTable[candObjIdx].yaw = trig2_arctan((int16_t)candNewMoveX, (int16_t)candNewMoveY);
	g_objectTable[candObjIdx].pitch = trig2_w_arcsin(candNewMoveX);
	g_objectTable[candObjIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[candObjIdx].mobj->moveVectorDirty = 1;

	if ((uint16_t)srcRollMagnitude >= 0x8000) {
		srcRollMagnitude = 0x7FFF;
	}
	if ((uint16_t)candYawBefore < g_objectTable[srcObjIdx].yaw) {
		srcRollMagnitude = -srcRollMagnitude;
	}
	g_objectTable[srcObjIdx].mobj->rollImpulseRate = (int16_t)srcRollMagnitude;
	if (g_objectTable[srcObjIdx].playerOwnerIdx == g_localPlayer) {
		int16_t r = g_objectTable[srcObjIdx].mobj->rollImpulseRate;
		if (r < 0) {
			ForceFeedback_PlayImpactEffect(270, -r);
		} else {
			ForceFeedback_PlayImpactEffect(90, r);
		}
	}
	FVIEW_calcrotatemove(g_objectTable[srcObjIdx].pitch, g_objectTable[srcObjIdx].yaw,
						 &g_objectTable[srcObjIdx]);
	FVIEW_calcrotateorient(g_objectTable[srcObjIdx].roll, g_objectTable[srcObjIdx].angleD,
						   &g_objectTable[srcObjIdx]);

	msg_emitInFlightMessage(MSG_COLLISION, g_objectTable[srcObjIdx].playerOwnerIdx);
	fsfx_PlaySound((GameRand2() & 1) + 50, (uint16_t)srcObjIdx,
				   (unsigned int)g_objectTable[srcObjIdx].playerOwnerIdx);
	fsfx_PlaySound((GameRand2() & 1) + 50, (uint16_t)candObjIdx,
				   (unsigned int)g_objectTable[candObjIdx].playerOwnerIdx);
}

// Copies the latest collision impact plane (center + normal) from the glow-mark scratch globals into
// the impact-effect globals consumed by ricochet/particle code.
static __inline void collide_CaptureImpactPlane(void) {
	g_collisionImpactEffectNormal.x = g_glowMarkScratchNormalVec->x;
	g_collisionImpactEffectNormal.y = g_glowMarkScratchNormalVec->y;
	g_collisionImpactEffectNormal.z = g_glowMarkScratchNormalVec->z;
	g_collisionImpactEffectCenter.x = g_glowMarkPlaneScratch.center.x;
	g_collisionImpactEffectCenter.y = g_glowMarkPlaneScratch.center.y;
	g_collisionImpactEffectCenter.z = g_glowMarkPlaneScratch.center.z;
}

// Returns nonzero if the projectile hit landed on one of the target model's engine-glow meshes.
static __inline int collide_HitMeshIsEngineGlow(ObjectTypeId targetType, uint16_t hitComponent) {
	ModelIndex modelIdx = GetModelIndexFromType(targetType);
	unsigned int i;
	if (modelIdx == (ModelIndex)0xFFFF) {
		return 0;
	}
	for (i = 0; i < g_modelDefs[modelIdx].engineGlowCount; ++i) {
		if ((uint16_t)g_modelDefs[modelIdx].engineGlowMeshIdx[i] == hitComponent) {
			return 1;
		}
	}
	return 0;
}

static __inline uint8_t collide_GetProjectileWarheadClass(ObjectTypeId projectileType) {
#ifdef XWA_MODERN
	return laser_GetProjectileWarheadClass(projectileType) > 0;
#else
	return g_projectileWarheadClassByType[(uint16_t)projectileType - OBJ_LaserRebel];
#endif
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x408DC0
// Per-region flight object interaction pass (XWA's expanded successor of TIE's collide_collisions).
// For every active object it handles player-facing inspection / departure-dock / hyperbuoy prompts,
// refreshes and ages the MobileObjectProximityList, then dispatches each nearby object pair through the
// swept collision tests, applying ricochets, craft-to-craft bumps, projectile hits, damage, explosions,
// glow marks, particles, SFX, and mission scoring side effects.
void collide_collisions(void) {
	unsigned int objIdx;
	unsigned int sourceObjIdx;
	int mirrorDist = 0xFFFF;

	g_collideUpdateCollisionObjLink = 1;

	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_debrisObjectSlotStart; ++objIdx) {
		ObjectRecord* obj;
		MobileObject* mobj;
		int playerOwnerIdx;
		MobileObject* sm;
		MobileObjectProximityList* list;
		int newOverflow;
		uint8_t count;
		int entryIdx;
		sourceObjIdx = objIdx;
		if (g_objectTable[sourceObjIdx].objectType == OBJ_None ||
			g_objectTable[sourceObjIdx].genusId == GENUS_Explosion) {
			continue;
		}

		mobj = g_objectTable[sourceObjIdx].mobj;
		if (mobj && (g_objectTable[sourceObjIdx].world_x != mobj->prevWorldX ||
					 g_objectTable[sourceObjIdx].world_y != mobj->prevWorldY ||
					 g_objectTable[sourceObjIdx].world_z != mobj->prevWorldZ)) {
			mobj->collisionObjIdx = 0xFFFF;
		}

		playerOwnerIdx = g_objectTable[sourceObjIdx].playerOwnerIdx;
		if (playerOwnerIdx != -1 && !g_provingGroundsModeActive) {
			if (g_players[playerOwnerIdx].hyperspacePhase == PLAYER_HYPERSPACE_OUTBOUND) {
				continue;
			}
			if (g_gameTime > g_players[playerOwnerIdx].impactDamageCooldownTime) {
				collide_RefreshPlayerEngineWash(sourceObjIdx, playerOwnerIdx);
			}

			g_collisionProbeWorldX = g_objectTable[sourceObjIdx].world_x;
			g_collisionProbeWorldY = g_objectTable[sourceObjIdx].world_y;
			g_collisionProbeWorldZ = g_objectTable[sourceObjIdx].world_z;
			g_collisionSegmentStartWorldX = g_objectTable[sourceObjIdx].mobj->prevWorldX;
			g_collisionSegmentStartWorldY = g_objectTable[sourceObjIdx].mobj->prevWorldY;
			g_collisionSegmentStartWorldZ = g_objectTable[sourceObjIdx].mobj->prevWorldZ;

			collide_HandleTargetInspection(playerOwnerIdx, sourceObjIdx);
			collide_HandleDepartureDockPrompt(playerOwnerIdx, sourceObjIdx, &mirrorDist);
			collide_HandleHyperBuoyPrompt(playerOwnerIdx, sourceObjIdx);
		}

		// LABEL_176: refresh and age this object's proximity candidate list.
		sm = g_objectTable[sourceObjIdx].mobj;
		if (!sm) {
			continue;
		}
		list = &sm->proximityList;
		newOverflow = list->overflowScore - (uint16_t)g_elapsedTicks;
		list->overflowScore = newOverflow;
		if (newOverflow <= 0) {
			list->overflowScore = 0x7FFF;
			collide_RefreshMobileObjectProximityCandidates(list, sourceObjIdx);
		}

		count = list->count;
		if (count == 0) {
			continue;
		}

		obj = &g_objectTable[sourceObjIdx];
		entryIdx = 0;
		while (1) {
			uint16_t entrySlot = (uint16_t)entryIdx;
			unsigned int candIdx = list->objIdx[entrySlot];
			ObjectRecord* cand = &g_objectTable[candIdx];
			ObjectTypeId candType = cand->objectType;
			ModelGenusId candGenus;
			MobileObject* candMobj;
			int srcOwner;
			int hitComponent;
			MobileObject* cm;
			int isRicochet;
			int doRicochet;
			int allowSimpleBoxHit;
			int hitRadius;
			ModelGenusId dg;
			ModelGenusId cg;
			unsigned int hitTarget;
			unsigned int attacker;
			ObjectTypeId srcType;
			MobileObject* am2;
			ObjectTypeId projType;
			CraftData* candCraft;
			int placeMark;
			ObjectMeshTextureLayerBlock* patch;

			candGenus = cand->genusId;
			if (candType == OBJ_None || candGenus == GENUS_Explosion || candType == OBJ_NavBuoy3 ||
				candType == OBJ_HyperBuoy) {
				collide_RemoveProximityEntry(list, candIdx);
				--entryIdx;
				goto advance;
			}

			// Skip freshly-spawned non-projectile candidates unless the owner is a projectile.
			if (!(g_provingGroundsModeActive || (candMobj = cand->mobj) == NULL ||
				  candMobj->framesAlive >= 3u || candGenus == GENUS_PlayerProjectile)) {
				if (candGenus != GENUS_NpcProjectile) {
					ModelGenusId sg = obj->genusId;
					if (sg != GENUS_PlayerProjectile && sg != GENUS_NpcProjectile) {
						goto advance;
					}
				}
			}

			list->score[entrySlot] -= (uint16_t)g_elapsedTicks;
			if (list->score[entrySlot] > 0) {
				goto advance;
			}

			if (!g_provingGroundsModeActive && obj->mobj->framesAlive < 3u) {
				ModelGenusId sg = obj->genusId;
				if (sg != GENUS_PlayerProjectile && sg != GENUS_NpcProjectile) {
					ModelGenusId cg = cand->genusId;
					if (cg != GENUS_PlayerProjectile && cg != GENUS_NpcProjectile) {
						goto reinsert;
					}
				}
			}

			srcOwner = obj->playerOwnerIdx;
			if (srcOwner != -1) {
				// Owner is a player-controlled craft.
				CraftData* srcCraft = obj->mobj->pCraft;
				if (candIdx == srcCraft->aiFlight.impactObjIdx) {
					goto reinsert;
				}
				if (candIdx == srcCraft->carriedObjectIndex || candIdx == srcCraft->lastReleasedObjectIdx ||
					candIdx == srcCraft->linkedPrevObjectIdx || candIdx == srcCraft->nextLinkObjectIdx) {
					goto advance;
				}
				if (g_players[srcOwner].inputDisabledFlag &&
					candIdx == pai_GetEffectiveAIController(srcCraft)->targetObjIdx) {
					goto advance;
				}

				g_collisionProbeWorldX = obj->world_x;
				g_collisionProbeWorldY = obj->world_y;
				g_collisionProbeWorldZ = obj->world_z;
				g_collisionSegmentStartWorldX = obj->mobj->prevWorldX;
				g_collisionSegmentStartWorldY = obj->mobj->prevWorldY;
				g_collisionSegmentStartWorldZ = obj->mobj->prevWorldZ;

				cm = cand->mobj;
				if (cm) {
					CraftData* candCraft = cm->pCraft;
					if (candCraft) {
						AiController* ctrl = pai_GetEffectiveAIController(candCraft);
						if (!strcmp(g_planTable[ctrl->currentPlanId].name, "boardtogivepln") &&
							ctrl->targetObjIdx == sourceObjIdx) {
							goto reinsert;
						}
					}

					g_collisionSweepEndX = cand->world_x;
					g_collisionSweepEndY = cand->world_y;
					g_collisionSweepEndZ = cand->world_z;
					g_collisionSweepStartX = cand->mobj->prevWorldX;
					g_collisionSweepStartY = cand->mobj->prevWorldY;
					g_collisionSweepStartZ = cand->mobj->prevWorldZ;

					hitComponent = (int)collide_lasercraftcollide(sourceObjIdx, candIdx);
					collide_CaptureImpactPlane();
					if ((uint16_t)hitComponent == 0) {
						goto finalize;
					}

					if (g_provingGroundsModeActive) {
						doRicochet = Yard_HandleChallengeObjectCollision(sourceObjIdx, candIdx, hitComponent);
						isRicochet = 1;
					} else {
						int allowSimpleBoxHit;
						int hitRadius = collide_GetSweptHitRadius(sourceObjIdx, candIdx, &allowSimpleBoxHit);
						isRicochet = 1;
						if ((!allowSimpleBoxHit || hitRadius < 1095) &&
							cand->objectType != OBJ_ContainerGem) {
							ModelGenusId cg = cand->genusId;
							if (cg != GENUS_DeathStarTunnelSegment &&
								(cg != GENUS_PilotDroid || !g_provingGroundsModeActive) &&
								cg != GENUS_SalvageJunk) {
								isRicochet = 0;
							}
						}
						doRicochet = isRicochet;
					}

					if (isRicochet) {
						if (doRicochet) {
							goto do_ricochet;
						}
					} else {
						ModelGenusId cg = cand->genusId;
						if (cg == GENUS_Fighter || cg == GENUS_Transport || cg == GENUS_PilotDroid ||
							cg == GENUS_WeaponEmplacement || cg == GENUS_Utility) {
							collide_ApplyCraftCraftBump(sourceObjIdx, candIdx);
						}
						if (g_flightCollisionsEnabled) {
							goto damage_both;
						}
						dg = cand->genusId;
						if (dg == GENUS_Starship || dg == GENUS_Freighter || dg == GENUS_Container ||
							dg == GENUS_Platform || dg == GENUS_Rubble) {
							goto damage_both;
						}
					}
					goto post;
				} else {
					// Candidate is a static object.
					int16_t hit = static_laserstaticcollide(sourceObjIdx, candIdx);
					collide_CaptureImpactPlane();
					if ((uint16_t)hit == 0) {
						goto finalize;
					}
					hitRadius = collide_GetSweptHitRadius(sourceObjIdx, candIdx, &allowSimpleBoxHit);
					isRicochet = 1;
					if ((!allowSimpleBoxHit || hitRadius < 1095) && cand->objectType != OBJ_ContainerGem) {
						cg = cand->genusId;
						if (cg != GENUS_DeathStarTunnelSegment &&
							(cg != GENUS_PilotDroid || !g_provingGroundsModeActive) &&
							cg != GENUS_SalvageJunk) {
							isRicochet = 0;
						}
					}
					if (isRicochet) {
						collide_applySurfaceRicochet(sourceObjIdx, candIdx);
						goto finalize;
					}
					collide_applyCraftImpactBounce(sourceObjIdx, candIdx);
					if (!g_flightCollisionsEnabled) {
						goto finalize;
					}
					if (g_missionFlightGroups[cand->flightGroupIdx].fg.status1 != 20 &&
						g_missionFlightGroups[cand->flightGroupIdx].fg.status2 != 20 &&
						g_missionFlightGroups[obj->flightGroupIdx].fg.status1 != 20 &&
						g_missionFlightGroups[obj->flightGroupIdx].fg.status2 != 20) {
						static_laserhitstatic(sourceObjIdx, candIdx, (uint16_t)hit - 1u);
					}
					if (!g_provingGroundsModeActive) {
						collide_damagecraft(sourceObjIdx, 0xFFFFu, candIdx, 0, 0);
						goto finalize;
					}
					goto post;
				}

			do_ricochet:
				collide_applySurfaceRicochet(sourceObjIdx, candIdx);
				goto finalize;

			damage_both:
				collide_damagecraft(candIdx, (unsigned int)hitComponent, sourceObjIdx, 0, 0);
				if (obj->mobj->orientMatrixDirty) {
					FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
					FVIEW_calcrotateorient(obj->roll, obj->angleD, obj);
				}
				{
					int rz = (int16_t)(g_collisionSweepEndZ - g_collisionSweepStartZ);
					int ry = (int16_t)(g_collisionSweepEndY - g_collisionSweepStartY);
					int rx = (int16_t)(g_collisionSweepEndX - g_collisionSweepStartX);
					MobileObject* smo = obj->mobj;
					int dot = rz * smo->cachedFwdZ + ry * smo->cachedFwdY + rx * smo->cachedFwdX;
					if (dot >= 0x40000000) {
						dot = 0x3FFFFFFF;
					}
					if (dot <= -1073741824) {
						dot = -1073676288;
					}
					collide_damagecraft(sourceObjIdx, 0xFFFFu, candIdx, (((dot >> 15) & 0x8000) != 0), 0);
				}
				goto finalize;
			} else {
				// Owner is not player-controlled: dispatch on its genus.
				switch (obj->genusId) {
					case GENUS_Fighter:
					case GENUS_Transport:
					case GENUS_Utility:
					case GENUS_PilotDroid:
					case GENUS_WeaponEmplacement:
						g_collisionProbeWorldX = obj->world_x;
						g_collisionProbeWorldY = obj->world_y;
						g_collisionProbeWorldZ = obj->world_z;
						g_collisionSegmentStartWorldX = obj->mobj->prevWorldX;
						g_collisionSegmentStartWorldY = obj->mobj->prevWorldY;
						g_collisionSegmentStartWorldZ = obj->mobj->prevWorldZ;
						if (candIdx >= (unsigned int)g_objScanStart &&
							candIdx < g_regionStaticObjectSlotEnd) {
							if (!static_laserstaticcollide(sourceObjIdx, candIdx)) {
								goto finalize;
							}
							if (!g_flightCollisionsEnabled) {
								collide_applyCraftImpactBounce(sourceObjIdx, candIdx);
								goto finalize;
							}
							collide_damagecraft(sourceObjIdx, 0xFFFFu, candIdx, 0, 0);
							goto finalize;
						}
						if (obj->genusId != GENUS_WeaponEmplacement) {
							goto post;
						}
						g_collisionSweepEndX = cand->world_x;
						g_collisionSweepEndY = cand->world_y;
						g_collisionSweepEndZ = cand->world_z;
						g_collisionSweepStartX = cand->mobj->prevWorldX;
						g_collisionSweepStartY = cand->mobj->prevWorldY;
						g_collisionSweepStartZ = cand->mobj->prevWorldZ;
						if ((uint16_t)collide_lasercraftcollide(sourceObjIdx, candIdx)) {
							collide_damagecraft(candIdx, 0xFFFFu, sourceObjIdx, 0, 0);
							collide_ConvertObjectToExplosion(sourceObjIdx,
															 (ObjectTypeId)((GameRand() & 3) + 266), 1);
							obj->mobj->speed = 0;
							obj->mobj->instanceExtent *= 4;
						}
						goto finalize;

					case GENUS_Freighter:
					case GENUS_Starship:
					case GENUS_Platform:
					case GENUS_LargeScenery:
					case GENUS_Container:
					case GENUS_Rubble:
					case GENUS_SalvageJunk: {
						CraftData* srcCraft = obj->mobj->pCraft;
						if (srcCraft) {
							AiController* ctrl = pai_GetEffectiveAIController(srcCraft);
							uint8_t maneuverMode = ctrl->maneuverMode;
							if (maneuverMode == 30) {
								entryIdx = 16;
								goto advance;
							}
							if (srcCraft->carrierObjIdx != 0xFFFF) {
								entryIdx = 16;
								goto advance;
							}
							if (srcCraft->carriedObjectIndex == candIdx ||
								(maneuverMode == 18 && ctrl->targetObjIdx == candIdx)) {
								collide_InsertMobileObjectProximityCandidate(list, (uint16_t)sourceObjIdx,
																			 candIdx);
								goto advance;
							}
						}

						cm = cand->mobj;
						if (cm) {
							CraftData* candCraft = cm->pCraft;
							if (candCraft) {
								AiController* ctrl = pai_GetEffectiveAIController(candCraft);
								const char* plan = g_planTable[ctrl->pendingPlanId].name;
								int leaderMatch;
								if (!strcmp(plan, "exithangarpln") || !strcmp(plan, "outofhyperspacepln") ||
									!strcmp(plan, "enterhangarpln")) {
									goto reinsert;
								}
								leaderMatch = 0;
								if (!strcmp(plan, "followhomeevadepln")) {
									AiController* lead = pai_GetEffectiveAIController(
										g_objectTable[candCraft->leader_obj_idx].mobj->pCraft);
									const char* leadPlan = g_planTable[lead->pendingPlanId].name;
									leaderMatch = !strcmp(leadPlan, "exithangarpln") ||
												  !strcmp(leadPlan, "enterhangarpln");
								}
								if (leaderMatch || candCraft->carrierObjIdx != 0xFFFF ||
									ctrl->maneuverMode == 18 || ctrl->maneuverMode == 21 ||
									ctrl->maneuverMode == 30) {
									collide_InsertMobileObjectProximityCandidate(list, (uint16_t)sourceObjIdx,
																				 candIdx);
									goto advance;
								}
							}

							// Junk sources swap so the junk is treated as the target being hit.
							srcType = obj->objectType;
							if ((uint16_t)srcType < OBJ_Junk01 || (uint16_t)srcType > OBJ_Junk10) {
								hitTarget = sourceObjIdx;
								attacker = candIdx;
							} else {
								hitTarget = candIdx;
								attacker = sourceObjIdx;
							}

							g_collisionProbeWorldX = g_objectTable[attacker].world_x;
							g_collisionProbeWorldY = g_objectTable[attacker].world_y;
							g_collisionProbeWorldZ = g_objectTable[attacker].world_z;
							g_collisionSegmentStartWorldX = g_objectTable[attacker].mobj->prevWorldX;
							g_collisionSegmentStartWorldY = g_objectTable[attacker].mobj->prevWorldY;
							g_collisionSegmentStartWorldZ = g_objectTable[attacker].mobj->prevWorldZ;
							g_collisionSweepEndX = g_objectTable[hitTarget].world_x;
							g_collisionSweepEndY = g_objectTable[hitTarget].world_y;
							g_collisionSweepEndZ = g_objectTable[hitTarget].world_z;
							g_collisionSweepStartX = g_objectTable[hitTarget].mobj->prevWorldX;
							g_collisionSweepStartY = g_objectTable[hitTarget].mobj->prevWorldY;
							g_collisionSweepStartZ = g_objectTable[hitTarget].mobj->prevWorldZ;

							hitComponent = (int)collide_lasercraftcollide(attacker, hitTarget);
							collide_CaptureImpactPlane();
							if ((uint16_t)hitComponent == 0) {
								goto finalize;
							}

							if (g_provingGroundsModeActive) {
								doRicochet =
									Yard_HandleChallengeObjectCollision(attacker, hitTarget, hitComponent);
								isRicochet = 1;
							} else {
								int allowSimpleBoxHit;
								int hitRadius =
									collide_GetSweptHitRadius(attacker, hitTarget, &allowSimpleBoxHit);
								isRicochet = 1;
								if ((!allowSimpleBoxHit || hitRadius < 1095) &&
									g_objectTable[hitTarget].objectType != OBJ_ContainerGem) {
									ModelGenusId cg = g_objectTable[hitTarget].genusId;
									if (cg != GENUS_DeathStarTunnelSegment &&
										(cg != GENUS_PilotDroid || !g_provingGroundsModeActive) &&
										cg != GENUS_SalvageJunk) {
										isRicochet = 0;
									}
								}
								doRicochet = isRicochet;
							}

							if (!isRicochet) {
								collide_damagecraft(hitTarget, (unsigned int)hitComponent, attacker, 0, 0);
								collide_damagecraft(attacker, 0xFFFFu, hitTarget, 0, 0);
								goto finalize;
							}
							if (doRicochet) {
								MobileObject* am = g_objectTable[attacker].mobj;
								uint16_t savedDuration = am->velocityOverrideDuration;
								uint16_t savedSpeed = am->velocityOverrideSpeed;
								uint16_t savedElapsed = am->velocityOverrideElapsed;
								collide_applySurfaceRicochet(attacker, hitTarget);
								if (!g_provingGroundsModeActive) {
									if (g_missionHeader.body.missionType != XWA_MISSION_TYPE_DEATH_STAR) {
										collide_damagecraft(hitTarget, (unsigned int)hitComponent, attacker,
															0, 0);
									}
									if (!g_provingGroundsModeActive) {
										goto finalize;
									}
								}
								am2 = g_objectTable[attacker].mobj;
								if (am2->velocityOverrideActive) {
									ObjectTypeId at = g_objectTable[attacker].objectType;
									if ((uint16_t)at >= OBJ_Junk01 && (uint16_t)at <= OBJ_Junk10) {
										am2->velocityOverrideDuration = savedDuration;
										am2->velocityOverrideSpeed = savedSpeed;
										am2->velocityOverrideElapsed = savedElapsed;
										goto finalize;
									}
								}
							}
						} else if (candIdx >= (unsigned int)g_objScanStart &&
								   candIdx < g_regionStaticObjectSlotEnd) {
							if (static_laserstaticcollide(sourceObjIdx, candIdx)) {
								collide_damagecraft(sourceObjIdx, 0xFFFFu, candIdx, 0, 0);
							}
							goto finalize;
						}
						goto post;
					}

					case GENUS_PlayerProjectile:
					case GENUS_NpcProjectile: {
						unsigned int shooter = sourceObjIdx;
						if (candIdx < g_activeRegionCraftObjectSlotEnd) {
							AiController* ctrl = pai_GetEffectiveAIController(cand->mobj->pCraft);
							if (!strcmp(g_planTable[ctrl->currentPlanId].name, "boardtogivepln") &&
								ctrl->maneuverMode == 18 && ctrl->maneuverPhase == 2 &&
								ctrl->targetObjIdx == sourceObjIdx) {
								collide_InsertMobileObjectProximityCandidate(list, (uint16_t)sourceObjIdx,
																			 candIdx);
								goto advance;
							}
						}

						g_collisionProbeWorldX = obj->world_x;
						g_collisionProbeWorldY = obj->world_y;
						g_collisionProbeWorldZ = obj->world_z;
						g_collisionSegmentStartWorldX = obj->mobj->prevWorldX;
						g_collisionSegmentStartWorldY = obj->mobj->prevWorldY;
						g_collisionSegmentStartWorldZ = obj->mobj->prevWorldZ;

						projType = obj->objectType;
						if (cand->mobj) {
							g_collisionSweepEndX = cand->world_x;
							g_collisionSweepEndY = cand->world_y;
							g_collisionSweepEndZ = cand->world_z;
							g_collisionSweepStartX = cand->mobj->prevWorldX;
							g_collisionSweepStartY = cand->mobj->prevWorldY;
							g_collisionSweepStartZ = cand->mobj->prevWorldZ;

							hitComponent = (int)collide_lasercraftcollide(shooter, candIdx);
							if ((uint16_t)hitComponent == 0) {
								goto finalize;
							}

							if (candIdx < g_projectileObjectSlotStart ||
								candIdx >= g_projectileObjectSlotEnd) {
								// Candidate is a craft.
								if (candIdx < g_activeRegionCraftObjectSlotEnd) {
									Mission_RecordProjectileHitStats(shooter, 0);
								}
								collide_laserhitcraft(shooter, candIdx, hitComponent);
							} else {
								// Projectile-versus-projectile.
								if (collide_GetProjectileWarheadClass(obj->objectType)) {
									if (!collide_GetProjectileWarheadClass(cand->objectType)) {
										Mission_RecordProjectileHitStats(candIdx, 1);
									} else if (obj->mobj->pWarheadGuidance->targetObjIdx == candIdx) {
										Mission_RecordProjectileHitStats(shooter, 1);
									} else if (cand->mobj->pWarheadGuidance->targetObjIdx == shooter) {
										Mission_RecordProjectileHitStats(candIdx, 1);
									}
									collide_ConvertObjectToExplosion(
										shooter, (ObjectTypeId)((GameRand() & 3) + 266), 1);
								} else {
									Mission_RecordProjectileHitStats(shooter, 1);
									if (obj->objectType == OBJ_LaserIon ||
										obj->objectType == OBJ_LaserIonTurbo ||
										obj->objectType == OBJ_WarheadIonPulse) {
										collide_ConvertObjectToExplosion(shooter, OBJ_SparkTextureGroup3001,
																		 1);
									} else {
										collide_ConvertObjectToExplosion(shooter, OBJ_SparkTextureGroup3000,
																		 1);
									}
								}
								collide_ConvertObjectToExplosion(candIdx,
																 (ObjectTypeId)((GameRand() & 3) + 266), 1);
							}

							// LABEL_418: glow-mark / particle effects.
							if (g_glowMarkWorldSegmentMode) {
								g_glowMarkSegmentEndWorld[0] = g_collisionSegmentStartWorldX;
								g_glowMarkSegmentEndWorld[1] = g_collisionSegmentStartWorldY;
								g_glowMarkSegmentEndWorld[2] = g_collisionSegmentStartWorldZ;
								if (g_collisionHitOffsetX || g_collisionHitOffsetY || g_collisionHitOffsetZ) {
									g_glowMarkSegmentStartWorld[0] =
										g_collisionSegmentStartWorldX + g_collisionHitOffsetX;
									g_glowMarkSegmentStartWorld[1] =
										g_collisionSegmentStartWorldY + g_collisionHitOffsetY;
									g_glowMarkSegmentStartWorld[2] =
										g_collisionSegmentStartWorldZ + g_collisionHitOffsetZ;
								} else {
									g_glowMarkSegmentStartWorld[0] = g_collisionProbeWorldX;
									g_glowMarkSegmentStartWorld[1] = g_collisionProbeWorldY;
									g_glowMarkSegmentStartWorld[2] = g_collisionProbeWorldZ;
								}
							}

							if (!g_useHardware3D || !g_flightSideEffectsEnabled) {
								goto finalize;
							}
							if ((uint16_t)projType == 295) {
								GlowMark_QueueRequest(candIdx, 295, 278, -1.0f, -1.0f);
								goto finalize;
							}
							candCraft = cand->mobj->pCraft;
							if (candCraft && (candCraft->shieldFront || candCraft->shieldRear)) {
								GlowMark_QueueRequest(candIdx, (int16_t)projType, 552, -1.0f, -1.0f);
								goto finalize;
							}
							if ((uint16_t)projType == 284 || (uint16_t)projType == 285 ||
								(uint16_t)projType == 290 || (uint16_t)projType == 296) {
								GlowMark_QueueRequest(candIdx, (int16_t)projType, 279, -1.0f, -1.0f);
								goto finalize;
							}

							if (!collide_GetProjectileWarheadClass(projType)) {
								if (!g_glowMarkWorldSegmentMode) {
									if ((uint16_t)GameRand2() <= 0xF000) {
										goto finalize;
									}
									placeMark = 1;
									patch = g_objRenderState[candIdx].glowMarkTail;
									while (patch && placeMark) {
										if (patch->currentFrame < 8) {
											placeMark = 0;
										} else {
											float ddx = patch->center.x - g_glowMarkPlaneScratch.center.x;
											float ddy = patch->center.y - g_glowMarkPlaneScratch.center.y;
											float ddz = patch->center.z - g_glowMarkPlaneScratch.center.z;
											if (ddz * ddz + (ddx * ddx + ddy * ddy) < 500000.0f) {
												placeMark = 0;
											}
										}
										patch = patch->prevActive;
									}
									if (!placeMark) {
										goto finalize;
									}
									if (cand->objectType != OBJ_SmeltingRoom) {
										placeMark = !collide_HitMeshIsEngineGlow(cand->objectType,
																				 (uint16_t)hitComponent);
										if (placeMark) {
											GlowMark_QueueRequest(candIdx, (int16_t)projType, 419, -1.0f,
																  -1.0f);
										}
										if (g_glowMarkWorldSegmentMode) {
											Particle_AttachEffectToObject(0, candIdx, NULL, NULL);
										} else {
											Particle_AttachEffectToObject(0, candIdx,
																		  &g_glowMarkPlaneScratch.center,
																		  g_glowMarkScratchNormalVec);
											Particle_AttachEffectToObject(11, candIdx,
																		  &g_glowMarkPlaneScratch.center,
																		  g_glowMarkScratchNormalVec);
										}
										goto finalize;
									}
								}
								goto post;
							}

							if ((uint8_t)GameRand2()) {
								placeMark =
									!collide_HitMeshIsEngineGlow(cand->objectType, (uint16_t)hitComponent);
								if (placeMark) {
									GlowMark_QueueRequest(candIdx, (int16_t)projType, 419, -1.0f, -1.0f);
								}
								if (g_glowMarkWorldSegmentMode) {
									Particle_AttachEffectToObject(0, candIdx, NULL, NULL);
								} else {
									Particle_AttachEffectToObject(11, candIdx, &g_glowMarkPlaneScratch.center,
																  g_glowMarkScratchNormalVec);
									Particle_AttachEffectToObject(11, candIdx, &g_glowMarkPlaneScratch.center,
																  g_glowMarkScratchNormalVec);
								}
							}
							goto finalize;
						} else {
							int16_t hit = static_laserstaticcollide(shooter, candIdx);
							if ((uint16_t)hit != 0) {
								static_laserhitstatic(shooter, candIdx, (uint16_t)hit - 1u);
								Mission_RecordProjectileHitStats(shooter, 1);
							}
							goto finalize;
						}
					}

					default:
						goto post;
				}
			}

		reinsert:
			collide_InsertMobileObjectProximityCandidate(list, (uint16_t)sourceObjIdx, candIdx);
			goto advance;

		finalize:
		post:
			if (g_objectTable[sourceObjIdx].objectType == OBJ_None ||
				g_objectTable[sourceObjIdx].genusId == GENUS_Explosion) {
				list->overflowScore = 0;
				list->count = 0;
				break;
			}
			if (cand->objectType == OBJ_None || cand->genusId == GENUS_Explosion) {
				collide_RemoveProximityEntry(list, candIdx);
				--entryIdx;
				goto advance;
			}
			goto advance;

		advance:
			count = list->count;
			++entryIdx;
			if ((uint16_t)entryIdx >= count) {
				break;
			}
		}
	}

	g_collideUpdateCollisionObjLink = 0;
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif
