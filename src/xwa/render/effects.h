#ifndef XWA_RENDER_EFFECTS_H
#define XWA_RENDER_EFFECTS_H

#include <stdint.h>

#include "xwa/assets/sprite_texture.h" /* TexLevel */
#include "xwa/render/renderer.h"       /* RenderBatch, ObjectTrailEmitter/Point, EngineGlowKnockoutMark */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transient visual-effects translation unit (original TU 0x4C6020-0x4CC8BC).
 *
 * Owns three sibling transient-effect mechanisms:
 *   - Particles: static pre-allocated record/effect pools (large BSS arrays).
 *   - Object trails and render batches: dynamic "DATAPOOL" arena blocks tracked
 *     in a linked list and freed en-masse by RenderBatch_FreeDataPools.
 *   - Engine-glow knockout marks: dynamic "DATAPOOL" arena blocks.
 */

/* Particle subsystem: effect/record types, pools, and the effect API. */

typedef struct ParticleRecord         ParticleRecord;
/* ParticleEffect is forward-declared in renderer.h (included above); its full
 * definition lives below. Re-typedef'ing it here would trip
 * -Wtypedef-redefinition under the project's C standard. */
typedef struct ParticleAuxBlockRef    ParticleAuxBlockRef;
typedef struct ParticleEffectTemplate ParticleEffectTemplate;

typedef void (*ParticleSpawnCallback)(ParticleRecord* particle, ParticleEffect* effect);
typedef void (*ParticleRecordCallback)(ParticleRecord* particle);

typedef int ParticleAuxPoolKind;

#pragma pack(push, 1)
#define XWA_PARTICLE_PACKED_STRUCT
#if defined(__GNUC__) || defined(__clang__)
#undef XWA_PARTICLE_PACKED_STRUCT
#define XWA_PARTICLE_PACKED_STRUCT __attribute__((packed))
#endif

struct XWA_PARTICLE_PACKED_STRUCT ParticleRecord {
	Vec3f           world;
	uint32_t        argbColor;
	float           size;
	Vec3f           vel;
	int16_t         ageTicks;
	ParticleEffect* childEffects;
	ParticleRecord* next;
};

struct XWA_PARTICLE_PACKED_STRUCT ParticleAuxBlockRef {
	ParticleRecord*      records;
	ParticleAuxBlockRef* next;
};

struct XWA_PARTICLE_PACKED_STRUCT ParticleEffectTemplate {
	float     field00;
	float     velocityAttractionScale;
	float     velocityDamping;
	float     colorDeltaScale;
	float     speedBase;
	float     speedRandom;
	/* D3D diffuse-color channel order: A, R, G, B. */
	float     colorStartA;
	float     colorStartR;
	float     colorStartG;
	float     colorStartB;
	float     colorRandomA;
	float     colorRandomR;
	float     colorRandomG;
	float     colorRandomB;
	float     colorEndA;
	float     colorEndR;
	float     colorEndG;
	float     colorEndB;
	float     particleSizeBase;
	float     particleSizeGrowth;
	float     particleSizeRandom;
	int       particleLifetimeTicks;
	int       initialAgeRandomTicks;
	uint16_t  textureModelType;
	uint16_t  billboardScale;
	TexLevel* staticTexLevel;
	TexLevel* field64TexLevel;
	uint32_t  renderFlags;
	int       randomizeColorEndpoints;
	uint8_t   gap70[32];
};

struct XWA_PARTICLE_PACKED_STRUCT ParticleEffect {
	ParticleRecord*         freeParticles;
	ParticleRecord*         particleBuffer;
	ParticleRecord*         particles;
	int                     particleCount;
	int                     maxParticles;
	ParticleEffectTemplate* def;
	uint16_t                textureFrameCount;
	uint16_t                textureModelType;
	float                   textureAnimRate;
	int                     effectType;
	ParticleAuxPoolKind     auxBufferKind;
	ParticleAuxBlockRef*    auxBuffer;
	uint16_t                objectIdx;
	ParticleRecord*         parentParticle;
	ParticleSpawnCallback   particleSpawnCallback;
	ParticleRecordCallback  particleFreeCallback;
	uint16_t                useAttachedTransform;
	int                     lastUpdateTime;
	int                     ageTicks;
	int                     lifetimeTicks;
	int                     emitUntilTicks;
	float                   yawBaseRad;
	float                   yawRandomRad;
	float                   pitchBaseRad;
	float                   pitchRandomRad;
	float                   spawnBatchCount;
	float                   spawnRemainder;
	float                   accelX;
	float                   accelY;
	float                   accelZ;
	/* D3D diffuse-color channel order: A, R, G, B. */
	float                   colorStartA;
	float                   colorStartR;
	float                   colorStartG;
	float                   colorStartB;
	float                   colorDeltaA;
	float                   colorDeltaR;
	float                   colorDeltaG;
	float                   colorDeltaB;
	Vec3f                   world;
	Vec3f                   localOffset;
	Vec3f                   velocity;
	float                   sourceVelocityScale;
	uint8_t                 gapB8[4];
	int                     randomDiscEnabled;
	float                   randomDiscRadius;
	int                     stretchedBillboard;
	float                   scale;
	ParticleEffect*         next;
	int                     fieldD0;
	int                     fieldD4;
	float                   fieldD8;
	float                   fieldDC;
};

#pragma pack(pop)
#undef XWA_PARTICLE_PACKED_STRUCT

/* Particle globals (owned by effects.c). */
extern ParticleEffectTemplate g_particleEffectTemplates[13];
extern ParticleEffect*        g_worldParticleEffects;
extern ParticleEffect*        g_particleEffectFreeList;
extern int                    g_particleEffectActiveCount;
extern int                    g_particleRecordAllocAttemptCount;
extern float                  g_particleDeltaScale;
extern ParticleAuxBlockRef*   g_particleAuxBlockFreeList100;
extern ParticleAuxBlockRef*   g_particleAuxBlockFreeList50;
extern ParticleAuxBlockRef*   g_particleAuxBlockFreeList25;

/* Render-batch DATAPOOL globals (owned by effects.c). */
extern RenderBatch* g_renderBatchFreeList;
extern int          g_renderBatchPoolSize;
extern int          g_renderBatchInUse;

/* Engine-glow knockout-mark DATAPOOL globals (owned by effects.c). */
extern EngineGlowKnockoutMark* g_engineGlowKnockoutFreeList;
extern int                     g_engineGlowKnockoutActiveCount;
extern int                     g_engineGlowKnockoutPoolCapacity;

/* Object-trail DATAPOOL globals (owned by effects.c). */
extern ObjectTrailEmitter* g_objectTrailFreeEmitters;
extern ObjectTrailPoint*   g_objectTrailFreePoints;
extern int                 g_objectTrailActivePointCount;
extern int                 g_objectTrailActiveEmitterCount;
extern int                 g_objectTrailPointPoolCount;
extern int                 g_objectTrailEmitterPoolCount;

/* Particle API. */
void            Particle_UpdateWorldEffects(void);
void            Particle_UpdateEffect(ParticleEffect* effect);
void            Particle_DrawEffectBillboards(ParticleEffect* effect);
void            Particle_DrawObjectEffectsForCrt(uint16_t objectIdx);
void            Particle_UpdateObjectEffectsForCrt(uint16_t objectIdx);
void            Particle_FreeAllEffects(void);
void            Particle_UpdateObjectEffects(void);
void            Particle_FreeEffect(ParticleEffect* effect);
void            Particle_FreeEffectWithChildren(ParticleEffect* effect);
void            Particle_FreeEffectListWithChildren(ParticleEffect* effectList);
void            Particle_FreeObjectEffects(uint16_t objIdx);
void            Particle_AppendObjectEffectPointLights(uint16_t objectIdx);
void            Particle_ResetPools(void);
void            Particle_InitEffectTemplates(void);
ParticleEffect* Particle_AllocEffect(int effectType);
void            Particle_InitEffectType0(ParticleEffect* effect);
void            Particle_InitEffectType1(ParticleEffect* effect);
void            Particle_InitEffectType2(ParticleEffect* effect);
void            Particle_InitEffectType3(ParticleEffect* effect);
void            Particle_InitEffectType4(ParticleEffect* effect);
void            Particle_InitEffectType5(ParticleEffect* effect);
void            Particle_InitEffectType6(ParticleEffect* effect);
void            Particle_InitEffectType7(ParticleEffect* effect);
void            Particle_InitEffectType8(ParticleEffect* effect);
void            Particle_InitEffectType9(ParticleEffect* effect);
void            Particle_InitEffectType10(ParticleEffect* effect);
void            Particle_InitEffectType11(ParticleEffect* effect);
void            Particle_InitEffectType12(ParticleEffect* effect);
void            Particle_SpawnChildEffectOnRecord(ParticleRecord* particle, ParticleEffect* effect);
void            Particle_SpawnTransientObjectFromRecord(ParticleRecord* particle);
ParticleEffect* Particle_AttachEffectToObject(int effectType, uint16_t objectIdx, const Vec3f* localOffset,
											  const Vec3f* direction);
ParticleEffect* Particle_CreateWorldEffect(int effectType, const Vec3f* worldPos, const Vec3f* worldVelocity);
ParticleRecord* Particle_AllocRecord(ParticleEffect* effect);
double          Particle_RandUnitFloat(void);
double          Particle_RandSignedUnitFloat(void);

#ifdef XWA_MODERN
/* Snapshot-only allocation generations. Stable while an allocation is live;
 * never consumed by simulation/render branching and never exposes pointers. */
uint32_t Particle_SnapshotEffectId(const ParticleEffect* effect);
uint32_t Particle_SnapshotRecordId(const ParticleRecord* particle);
/* Modern render-only precise positions. These sidecars do not alter the
 * recovered particle layouts or feed back into simulation. */
int Particle_SnapshotEffectPoint(const ParticleEffect* effect, int32_t base[3], float offset[3]);
int Particle_SnapshotRecordPoint(const ParticleRecord* particle, int32_t base[3], float offset[3]);
void Particle_SetWorldEffectPreciseOrigin(ParticleEffect* effect, const int32_t world[3]);
#endif

/* Object-trail API. */
ObjectTrailPoint*   ObjectTrail_AllocPoint(void);
ObjectTrailEmitter* ObjectTrail_AllocEmitter(void);
void                ObjectTrail_FreePoint(ObjectTrailPoint* point);
ObjectTrailEmitter* ObjectTrail_FreeEmitter(ObjectTrailEmitter* emitter);
void                ObjectTrail_InitTorpedoEmitter(ObjectTrailEmitter* trail);
void                ObjectTrail_InitMagPulseEmitter(ObjectTrailEmitter* trail);
void                ObjectTrail_InitIonPulseEmitter(ObjectTrailEmitter* trail);
void                ObjectTrail_InitMissileEmitter(ObjectTrailEmitter* trail);
void                ObjectTrail_InitRocketEmitter(ObjectTrailEmitter* trail);
void                ObjectTrail_InitAdvancedTorpedoEmitter(ObjectTrailEmitter* trail);
void                ObjectTrail_InitAdvancedMissileEmitter(ObjectTrailEmitter* trail);
ObjectTrailEmitter* ObjectTrail_CreateEmitter(uint16_t objectIdx, uint16_t objectType);
void                ObjectTrail_FreeEmittersForObject(uint16_t objectIdx);
void                ObjectTrail_Update(ObjectTrailEmitter* trail);
void                ObjectTrail_RenderObjectTrails(void);
void                ObjectTrail_DrawEmittersForObject(uint16_t objectIdx);

/* Render-batch DATAPOOL API. */
RenderBatch* RenderBatch_Alloc(void);
void         RenderBatch_Free(RenderBatch* batch);
void         RenderBatch_FreeDataPools(void);
void         RenderBatch_FreeDataPoolsThunk(void);

/* Engine-glow knockout-mark DATAPOOL API. */
EngineGlowKnockoutMark* EngineGlow_AllocKnockoutMark(void);
void                    EngineGlow_FreeKnockoutMark(EngineGlowKnockoutMark* mark);
void                    EngineGlow_FreeKnockoutMarkList(EngineGlowKnockoutMark* markList);

#ifdef __cplusplus
}
#endif

#endif
