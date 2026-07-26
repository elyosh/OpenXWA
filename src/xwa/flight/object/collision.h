#ifndef XWA_FLIGHT_OBJECT_COLLISION_H
#define XWA_FLIGHT_OBJECT_COLLISION_H

#include "xwa/assets/model_mesh.h"
#include "xwa/math/vec3f.h"
#include "xwa/flight/object/object.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern float g_collidePlaneEpsilonPos;
extern float g_collidePlaneEpsilonNeg;
extern float g_collideSweepHitBackoff;
extern float g_collideZeroFloat;
extern float g_engineWashFullIntensity;
extern float g_engineOutputScaleMax;
extern int g_collideSweepAllowUnownedTargets;
extern float g_collideRicochetDamageScale;
extern float g_collidePolygonEdgeCrossScratch;
extern float g_collideLooseSweepEdgeThreshold;
extern int g_collisionStagedModelProbe;
extern uint8_t g_applyingProximityDamage;
extern int g_collideUpdateCollisionObjLink;
extern uint8_t g_craftImpactBounceEnabled;

extern Vec3f g_collisionImpactEffectCenter;
extern Vec3f g_collisionImpactEffectNormal;
extern Vec3f g_collideSweepWalkerStart;
extern Vec3f g_collideSweepWalkerEnd;
extern Vec3f g_collideSweepLocalStart;
extern Vec3f g_collideSweepLocalEnd;
extern int g_collisionProbeWorldX;
extern int g_collisionProbeWorldY;
extern int g_collisionProbeWorldZ;
extern int g_collisionSegmentStartWorldX;
extern int g_collisionSegmentStartWorldY;
extern int g_collisionSegmentStartWorldZ;
extern int g_collisionSweepStartX;
extern int g_collisionSweepStartY;
extern int g_collisionSweepStartZ;
extern int g_collisionSweepEndX;
extern int g_collisionSweepEndY;
extern int g_collisionSweepEndZ;
extern int g_collisionHitOffsetX;
extern int g_collisionHitOffsetY;
extern int g_collisionHitOffsetZ;
extern int g_savedCollisionSegmentStartWorldX;
extern int g_savedCollisionSegmentStartWorldY;
extern int g_savedCollisionSegmentStartWorldZ;
extern int g_savedCollisionProbeWorldX;
extern int g_savedCollisionProbeWorldY;
extern int g_savedCollisionProbeWorldZ;
extern int g_savedCollisionSweepStartX;
extern int g_savedCollisionSweepStartY;
extern int g_savedCollisionSweepStartZ;
extern int g_savedCollisionSweepEndX;
extern int g_savedCollisionSweepEndY;
extern int g_savedCollisionSweepEndZ;
extern int g_savedCollisionHitOffsetX;
extern int g_savedCollisionHitOffsetY;
extern int g_savedCollisionHitOffsetZ;
extern int g_collisionScratchBackupSegmentStartWorldX;
extern int g_collisionScratchBackupSegmentStartWorldY;
extern int g_collisionScratchBackupSegmentStartWorldZ;
extern int g_collisionScratchBackupSweepEndZ;
extern int g_collisionScratchBackupSweepEndX;
extern int g_collisionScratchBackupSweepEndY;
extern int g_collisionScratchBackupHitOffsetX;
extern int g_collisionScratchBackupProbeWorldX;
extern int g_collisionScratchBackupProbeWorldY;
extern int g_collisionScratchBackupProbeWorldZ;
extern int g_collisionScratchBackupHitOffsetZ;
extern int g_collisionScratchBackupHitOffsetY;
extern int g_collisionScratchBackupSweepStartX;
extern int g_collisionScratchBackupSweepStartY;
extern int g_collisionScratchBackupSweepStartZ;
extern int g_collideFaceGroupChildSelector;
extern int g_collideSweepHitMeshOrdinal;
extern OptRotationScale* g_collideCurrentRotScaleData;
extern int g_collideSweepCurrentMeshOrdinal;
extern float g_collideCurrentMeshRotationAngle;
extern int g_collideSweepSkipSsdMeshOrdinal;
extern float g_collideSweepHitFraction;
extern int g_collideSweepRejectNearStartHits;
extern OptNode* g_collideCurrentMeshVertsNode;
extern int g_collideSweepAuxHardpointIdx;
extern int g_collideSweepAuxHardpointWorldOffsetX;
extern int g_collideSweepAuxHardpointWorldOffsetY;
extern int g_collideSweepAuxHardpointWorldOffsetZ;
extern const int g_laserConvergenceDistanceByLevel_BiasedBase[4];

unsigned int collide_roughdistance3du(unsigned int abs_dx, unsigned int abs_dy, unsigned int abs_dz);
int collide_roughdistance3d(int dx, int dy, int dz);
int collide_IntersectSegmentWithFacePlane(const float* faceNormal, const float* faceVertex,
										  const float* segmentStart, const float* segmentEnd, float* outT);
int collide_PointInFacePolygon(const float* faceNormal, const float* vertexBase, const int* faceIndices,
							   const float* point);
uint16_t collide_checkboxcollision(int radius);
uint16_t static_laserstaticcollide(unsigned int sourceObjIdx, unsigned int staticObjIdx);
uint16_t collide_lasercraftcollide(unsigned int attackerObjIdx, unsigned int targetObjIdx);
uint16_t collide_targetinrange(int16_t shooterObjIdx, int16_t targetObjIdx, int16_t hardpointIndex);
uint16_t collide_craftstarshipcollision(uint16_t craftObjIdx, int lookaheadFrames);
void collide_applyCraftImpactBounce(unsigned int sourceObjIdx, unsigned int impactObjIdx);
void collide_applySurfaceRicochet(unsigned int objectObjIdx, unsigned int hitAgainstObjIdx);
int collide_GetMobileObjectProximitySpeedQ12(unsigned int objIdx);
void collide_InsertMobileObjectProximityCandidate(MobileObjectProximityList* list, unsigned int ownerObjIdx,
												  unsigned int candidateObjIdx);
void collide_RefreshMobileObjectProximityCandidates(MobileObjectProximityList* list,
													unsigned int ownerObjIdx);
void collide_ResetObjectProximityForSlot(uint16_t objIdx);
void collide_ResetNeighborProximityLists(uint16_t objIdx);
int collide_GetSweptHitRadius(unsigned int attackerObjIdx, unsigned int targetObjIdx, int* allowSimpleBoxHit);
int collide_GetHyperRegionDesignationForPlayer(unsigned int playerObjIdx, unsigned int regionMarkerObjIdx,
											   int designationSlot);
void collide_TransformHitIntoObjectLocalFrame(unsigned int objIdx, int* outDirX, int* outDirY, int* outDirZ,
											  int* outLocalX, int* outLocalY, int* outLocalZ);
void collide_TransformSweepToModelLocal(ObjectRecord* targetObj, int* worldEnd, int* worldStart);
int collide_TestSweepAgainstOptNode(OptimizedPolyObject* model, OptNode* node);
int collide_TestSweepAgainstOptNodeLoose(OptimizedPolyObject* model, OptNode* node);
void collide_TestSweepAgainstModelMeshes(unsigned int sourceObjIdx, unsigned int targetObjIdx,
										 OptimizedPolyObject* model);
int collide_CheckLocalSweepAgainstObjectModel(unsigned int sourceObjIdx, unsigned int targetObjIdx,
											  float* localSegmentEnd, float* localSegmentStart,
											  int skipRootNodeIndex, char useExactFaceHit);
int collide_CheckSweptModelCollision(unsigned int sourceObjIdx, uint16_t targetObjIdx);
void collide_ApplyHostileProximityWeaponDisruption(int ownerObjIdx, int hostileObjIdx);
char collide_damagecraft(unsigned int targetObjIdx, unsigned int componentId, unsigned int damageSourceObjIdx,
						 unsigned int shieldSide, unsigned int damageDirection);
void collide_laserhitcraft(unsigned int projectileObjIdx, unsigned int targetObjIdx,
						   unsigned int hitComponentId);
int collide_ApplyProximityDamageFalloff(unsigned int sourceObjIdx, unsigned int maxDamage,
										unsigned int radius, int excludedObjIdx);
int collide_ApplyDefaultProximityDamage(unsigned int sourceObjIdx, unsigned int maxDamage,
										int excludedObjIdx);
void static_laserhitstatic(unsigned int sourceObjIdx, unsigned int staticObjIdx, unsigned int hitComponentId);
void collide_ApplyEngineWashDamage(int victimObjIdx, int sourceObjIdx);
void collide_ConvertObjectToExplosion(unsigned int objIdx, ObjectTypeId explosionObjectType,
									  char playSfxAndFeedback);
void collide_collisions(void);

#ifdef __cplusplus
}
#endif

#endif
