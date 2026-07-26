#include "xwa/render/effects.h"

#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/time.h"
#ifdef XWA_MODERN
#include "xwa_runtime/timing/modern_flight_timing.h"
#endif

#ifndef XWA_MODERN
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* outputString);
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#else
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#define g_OutputDebugStringA OutputDebugStringA
#endif

/*
 * Transient visual-effects translation unit (original 0x4C6020-0x4CC8BC).
 *
 * Consolidates particles, object trails, render batches, and engine-glow
 * knockout marks. The render batches, object-trail emitters/points, and
 * engine-glow knockout marks share a dynamic "DATAPOOL" arena: each allocated
 * block is registered into g_renderBatchBlocks and freed en-masse by
 * RenderBatch_FreeDataPools. In the original binary the 8-byte block-node
 * registration is inlined into each allocator; RenderDataPool_RegisterAllocation
 * is therefore static __inline so the whole-TU compile folds it back in.
 *
 * Particles use static pre-allocated record/effect pools (BSS arrays).
 */

enum {
	PARTICLE_POOL100_BLOCK_COUNT = 50,
	PARTICLE_POOL100_RECORDS_PER_BLOCK = 100,
	PARTICLE_POOL50_BLOCK_COUNT = 15,
	PARTICLE_POOL50_RECORDS_PER_BLOCK = 50,
	PARTICLE_POOL25_BLOCK_COUNT = 30,
	PARTICLE_POOL25_RECORDS_PER_BLOCK = 25,
	PARTICLE_EFFECT_POOL_COUNT = 36,
	PARTICLE_EFFECT_TEMPLATE_COUNT = 13,
	PARTICLE_LEGACY_UPDATE_TICKS = 8,
};

enum {
	RENDER_BATCH_BLOCK_CAPACITY = 3,
};

enum {
	OBJECT_TRAIL_POINT_BLOCK_CAPACITY = 5,
	OBJECT_TRAIL_EMITTER_BLOCK_CAPACITY = 5,
	OBJECT_TRAIL_RENDER_FLAGS_NEAREST = 0x8e12,
	OBJECT_TRAIL_RENDER_FLAGS_BILINEAR = 0x8f92,
};

enum {
	ENGINE_GLOW_KNOCKOUT_BLOCK_CAPACITY = 5,
};

/* Private DATAPOOL block-node: tracks one arena allocation for en-masse free. */
typedef struct RenderBatchPoolBlock {
	void* allocation;
	struct RenderBatchPoolBlock* next;
} RenderBatchPoolBlock;

/* ----- Particle pools and globals ----- */

// GLOBAL: XWA 0x97B860
static ParticleRecord g_particleRecordPool100[PARTICLE_POOL100_BLOCK_COUNT]
											 [PARTICLE_POOL100_RECORDS_PER_BLOCK];
// GLOBAL: XWA 0x96C0A0
static ParticleRecord g_particleRecordPool50[PARTICLE_POOL50_BLOCK_COUNT][PARTICLE_POOL50_RECORDS_PER_BLOCK];
// GLOBAL: XWA 0x973C40
static ParticleRecord g_particleRecordPool25[PARTICLE_POOL25_BLOCK_COUNT][PARTICLE_POOL25_RECORDS_PER_BLOCK];
#ifdef XWA_MODERN
typedef struct ParticleModernPoint {
	int32_t base[3];
	float offset[3];
	uint8_t valid;
} ParticleModernPoint;

typedef struct ParticleModernState {
	uint32_t snapshotId;
	ParticleModernPoint point;
	uint16_t pendingEmissionTicks;
} ParticleModernState;

#define PARTICLE_MODERN_POINT_REBASE 1024.0f

static ParticleModernState g_particleRecordModern100[PARTICLE_POOL100_BLOCK_COUNT]
													[PARTICLE_POOL100_RECORDS_PER_BLOCK];
static ParticleModernState g_particleRecordModern50[PARTICLE_POOL50_BLOCK_COUNT]
												   [PARTICLE_POOL50_RECORDS_PER_BLOCK];
static ParticleModernState g_particleRecordModern25[PARTICLE_POOL25_BLOCK_COUNT]
												   [PARTICLE_POOL25_RECORDS_PER_BLOCK];
#endif
// GLOBAL: XWA 0x9AECC0
static ParticleAuxBlockRef g_particleAuxBlockPool100[PARTICLE_POOL100_BLOCK_COUNT];
// GLOBAL: XWA 0x973BC0
static ParticleAuxBlockRef g_particleAuxBlockPool50[PARTICLE_POOL50_BLOCK_COUNT];
// GLOBAL: XWA 0x97B760
static ParticleAuxBlockRef g_particleAuxBlockPool25[PARTICLE_POOL25_BLOCK_COUNT];
// GLOBAL: XWA 0x74A290
static ParticleEffect g_particleEffectPool[PARTICLE_EFFECT_POOL_COUNT];
#ifdef XWA_MODERN
static ParticleModernState g_particleEffectModern[PARTICLE_EFFECT_POOL_COUNT];
static uint32_t g_particleSnapshotNextId;

static uint32_t Particle_NextSnapshotId(void) {
	if (++g_particleSnapshotNextId == 0) {
		++g_particleSnapshotNextId;
	}
	return g_particleSnapshotNextId;
}

static ParticleModernState* Particle_ModernRecordState(const ParticleRecord* particle) {
	const uintptr_t address = (uintptr_t)particle;
	const uintptr_t base100 = (uintptr_t)&g_particleRecordPool100[0][0];
	const uintptr_t base50 = (uintptr_t)&g_particleRecordPool50[0][0];
	const uintptr_t base25 = (uintptr_t)&g_particleRecordPool25[0][0];
	if (address >= base100 && address < base100 + sizeof g_particleRecordPool100) {
		const uintptr_t offset = address - base100;
		return offset % sizeof(ParticleRecord) == 0
				   ? &g_particleRecordModern100[0][0] + offset / sizeof(ParticleRecord)
				   : NULL;
	}
	if (address >= base50 && address < base50 + sizeof g_particleRecordPool50) {
		const uintptr_t offset = address - base50;
		return offset % sizeof(ParticleRecord) == 0
				   ? &g_particleRecordModern50[0][0] + offset / sizeof(ParticleRecord)
				   : NULL;
	}
	if (address >= base25 && address < base25 + sizeof g_particleRecordPool25) {
		const uintptr_t offset = address - base25;
		return offset % sizeof(ParticleRecord) == 0
				   ? &g_particleRecordModern25[0][0] + offset / sizeof(ParticleRecord)
				   : NULL;
	}
	return NULL;
}

static ParticleModernState* Particle_ModernEffectState(const ParticleEffect* effect) {
	const uintptr_t address = (uintptr_t)effect;
	const uintptr_t base = (uintptr_t)g_particleEffectPool;
	if (address < base || address >= base + sizeof g_particleEffectPool ||
		(address - base) % sizeof(ParticleEffect) != 0) {
		return NULL;
	}
	return &g_particleEffectModern[(address - base) / sizeof(ParticleEffect)];
}

static void Particle_ModernNormalizePoint(ParticleModernPoint* point) {
	for (int i = 0; i < 3; i++) {
		if (point->offset[i] > PARTICLE_MODERN_POINT_REBASE ||
			point->offset[i] < -PARTICLE_MODERN_POINT_REBASE) {
			const int64_t shift = (int64_t)point->offset[i];
			const int64_t base = (int64_t)point->base[i] + shift;
			if (base >= INT32_MIN && base <= INT32_MAX) {
				point->base[i] = (int32_t)base;
				point->offset[i] -= (float)shift;
			}
		}
	}
}

static void Particle_ModernSetPoint(ParticleModernPoint* point, const int32_t base[3],
									const float offset[3]) {
	for (int i = 0; i < 3; i++) {
		point->base[i] = base[i];
		point->offset[i] = offset[i];
	}
	point->valid = 1;
	Particle_ModernNormalizePoint(point);
}

static void Particle_ModernSetFloatPoint(ParticleModernPoint* point, float x, float y, float z) {
	const float values[3] = { x, y, z };
	for (int i = 0; i < 3; i++) {
		const double value = (double)values[i];
		if (value >= (double)INT32_MIN && value <= (double)INT32_MAX) {
			const int64_t whole = (int64_t)value;
			point->base[i] = (int32_t)whole;
			point->offset[i] = (float)(value - (double)whole);
		} else {
			point->base[i] = 0;
			point->offset[i] = values[i];
		}
	}
	point->valid = 1;
}

static void Particle_ModernAddPoint(ParticleModernPoint* point, const float delta[3]) {
	if (!point->valid)
		return;
	for (int i = 0; i < 3; i++)
		point->offset[i] += delta[i];
	Particle_ModernNormalizePoint(point);
}

static void Particle_ModernAdvanceRecord(ParticleRecord* particle, float scale) {
	ParticleModernState* state = Particle_ModernRecordState(particle);
	if (!state || !state->point.valid)
		return;
	const float delta[3] = { scale * particle->vel.x, scale * particle->vel.y, scale * particle->vel.z };
	Particle_ModernAddPoint(&state->point, delta);
}

static void Particle_ModernSetSpawnPoint(ParticleRecord* particle, const ParticleEffect* effect,
										 const float spawnOffset[3]) {
	ParticleModernState* particleState = Particle_ModernRecordState(particle);
	ParticleModernState* effectState = Particle_ModernEffectState(effect);
	if (!particleState)
		return;
	if (effectState && effectState->point.valid) {
		Particle_ModernSetPoint(&particleState->point, effectState->point.base, effectState->point.offset);
		Particle_ModernAddPoint(&particleState->point, spawnOffset);
	} else {
		Particle_ModernSetFloatPoint(&particleState->point, particle->world.x, particle->world.y,
									 particle->world.z);
	}
}
#endif
// GLOBAL: XWA 0x749B30
ParticleEffectTemplate g_particleEffectTemplates[PARTICLE_EFFECT_TEMPLATE_COUNT];
// GLOBAL: XWA 0x5A9CC4
const float g_particleUnitFloat = 1.0f;
// GLOBAL: XWA 0x5A9CCC
const float g_particleDeltaTickScale = 0.0042372881f;
// GLOBAL: XWA 0x5A9CD0
const float g_particleVelocityOutputScale = 30.0f;
// GLOBAL: XWA 0x5A9CD8
const float g_particleHalfFloat = 0.5f;
// GLOBAL: XWA 0x5A9D10
const float g_particleFiveFloat = 5.0f;
// GLOBAL: XWA 0x5A9DB0
const float g_particleTenFloat = 10.0f;
// GLOBAL: XWA 0x5A9DC4
const float g_particleRandU16ToUnitFloatScale = 0.00001525902f;
// GLOBAL: XWA 0x5A9DC8
const float g_particleColorByteScale = 255.0f;
// GLOBAL: XWA 0x5A9DCC
const float g_particleQ15MatrixToFloatScale = 0.000030518509f;
// GLOBAL: XWA 0x5A9DD0
const float g_particleTauRadians = 6.2831845f;
// GLOBAL: XWA 0x5A9DF4
const float g_particleThreeFloat = 3.0f;
// GLOBAL: XWA 0x5A9CDC
const float g_particleZeroFloat = 0.0f;
// GLOBAL: XWA 0x74A288
ParticleEffect* g_worldParticleEffects;
// GLOBAL: XWA 0x74A28C
float g_particleDeltaScale;
// GLOBAL: XWA 0x74C214
ParticleEffect* g_particleEffectFreeList;
// GLOBAL: XWA 0x74C218
int g_particleEffectActiveCount;
// GLOBAL: XWA 0x74C24C
int g_particleRecordAllocAttemptCount;
// GLOBAL: XWA 0x97B850
ParticleAuxBlockRef* g_particleAuxBlockFreeList100;
// GLOBAL: XWA 0x96C080
ParticleAuxBlockRef* g_particleAuxBlockFreeList50;
// GLOBAL: XWA 0x97B74C
ParticleAuxBlockRef* g_particleAuxBlockFreeList25;

/* ----- Render-batch DATAPOOL globals ----- */

// GLOBAL: XWA 0x74C210
RenderBatch* g_renderBatchFreeList;
// GLOBAL: XWA 0x74C240
int g_renderBatchPoolSize;
// GLOBAL: XWA 0x74C244
int g_renderBatchInUse;
// GLOBAL: XWA 0x74C248
RenderBatchPoolBlock* g_renderBatchBlocks;

/* ----- Engine-glow knockout-mark DATAPOOL globals ----- */

// GLOBAL: XWA 0x74C21C
EngineGlowKnockoutMark* g_engineGlowKnockoutFreeList;
// GLOBAL: XWA 0x74C224
int g_engineGlowKnockoutActiveCount;
// GLOBAL: XWA 0x74C220
int g_engineGlowKnockoutPoolCapacity;

/* ----- Object-trail DATAPOOL globals ----- */

// GLOBAL: XWA 0x74C228
ObjectTrailEmitter* g_objectTrailFreeEmitters;
// GLOBAL: XWA 0x74C234
ObjectTrailPoint* g_objectTrailFreePoints;
// GLOBAL: XWA 0x74C23C
int g_objectTrailActivePointCount;
// GLOBAL: XWA 0x74C230
int g_objectTrailActiveEmitterCount;
// GLOBAL: XWA 0x74C238
int g_objectTrailPointPoolCount;
// GLOBAL: XWA 0x74C22C
int g_objectTrailEmitterPoolCount;

/*
 * Register a DATAPOOL arena block for en-masse free. Declared static __inline so
 * the whole-TU compile inlines it into the four allocators (RenderBatch_Alloc,
 * ObjectTrail_AllocEmitter, ObjectTrail_AllocPoint, EngineGlow_AllocKnockoutMark),
 * matching the original binary which inlines this registration.
 */
static __inline void RenderDataPool_RegisterAllocation(void* allocation) {
	RenderBatchPoolBlock* block;

	block = (RenderBatchPoolBlock*)Memory_AllocTagged(RENDER_DATAPOOL_TAG, sizeof(RenderBatchPoolBlock));
	block->allocation = allocation;
	block->next = g_renderBatchBlocks;
	g_renderBatchBlocks = block;
}

// FUNCTION: XWA 0x4C74A0
double Particle_RandSignedUnitFloat(void) {
	uint16_t value;
	float unit;

	value = (uint16_t)GameRand2();
	unit = (float)((double)value * g_particleRandU16ToUnitFloatScale);
	return unit + unit - g_particleUnitFloat;
}

// FUNCTION: XWA 0x4C74D0
double Particle_RandUnitFloat(void) {
	return (double)(uint16_t)GameRand2() * g_particleRandU16ToUnitFloatScale;
}

static __inline float Particle_SignedUnitFromRand(uint16_t value) {
	double unit;

	unit = (double)value * g_particleRandU16ToUnitFloatScale;
	return (float)(unit + unit - g_particleUnitFloat);
}

static __inline float Particle_RandomSignedUnit(void) {
	return Particle_SignedUnitFromRand((uint16_t)GameRand2());
}

static __inline void Particle_InitEffectColorRamp(ParticleEffect* effect) {
	if (!effect->def->randomizeColorEndpoints) {
		effect->colorStartA =
			effect->def->colorRandomA * Particle_RandomSignedUnit() + effect->def->colorStartA;
		effect->colorStartR =
			effect->def->colorRandomR * Particle_RandomSignedUnit() + effect->def->colorStartR;
		effect->colorStartG =
			effect->def->colorRandomG * Particle_RandomSignedUnit() + effect->def->colorStartG;
		effect->colorStartB =
			effect->def->colorRandomB * Particle_RandomSignedUnit() + effect->def->colorStartB;
		effect->colorDeltaA = (effect->def->colorEndA - effect->colorStartA) * effect->def->colorDeltaScale;
		effect->colorDeltaR = (effect->def->colorEndR - effect->colorStartR) * effect->def->colorDeltaScale;
		effect->colorDeltaG = (effect->def->colorEndG - effect->colorStartG) * effect->def->colorDeltaScale;
		effect->colorDeltaB = (effect->def->colorEndB - effect->colorStartB) * effect->def->colorDeltaScale;
	} else {
		float endA;
		float endR;
		float endG;
		float endB;

		endA = effect->def->colorRandomA * Particle_RandomSignedUnit() + effect->def->colorEndA;
		endR = effect->def->colorRandomR * Particle_RandomSignedUnit() + effect->def->colorEndR;
		endG = effect->def->colorRandomG * Particle_RandomSignedUnit() + effect->def->colorEndG;
		endB = effect->def->colorRandomB * Particle_RandomSignedUnit() + effect->def->colorEndB;

		effect->colorStartA = effect->def->colorStartA;
		effect->colorStartR = effect->def->colorStartR;
		effect->colorStartG = effect->def->colorStartG;
		effect->colorStartB = effect->def->colorStartB;
		effect->colorDeltaA = (endA - effect->colorStartA) * effect->def->colorDeltaScale;
		effect->colorDeltaR = (endR - effect->colorStartR) * effect->def->colorDeltaScale;
		effect->colorDeltaG = (endG - effect->colorStartG) * effect->def->colorDeltaScale;
		effect->colorDeltaB = (endB - effect->colorStartB) * effect->def->colorDeltaScale;
	}
}

static __inline void
Particle_SetEffectTemplate(int templateIdx, float velocityAttractionScale, float velocityDamping,
						   float colorDeltaScale, float speedBase, float speedRandom, float colorStartA,
						   float colorStartR, float colorStartG, float colorStartB, float colorRandomA,
						   float colorRandomR, float colorRandomG, float colorRandomB, float colorEndA,
						   float colorEndR, float colorEndG, float colorEndB, float particleSizeBase,
						   float particleSizeGrowth, float particleSizeRandom, int particleLifetimeTicks,
						   int initialAgeRandomTicks, int randomizeColorEndpoints, uint16_t textureModelType,
						   uint16_t textureFrame, float billboardScaleMultiplier, uint32_t renderFlags,
						   uint32_t bilinearRenderFlags, int clearField64TexLevel) {
	ParticleEffectTemplate* def;

	def = &g_particleEffectTemplates[templateIdx];
	def->field00 = 1.0f;
	def->velocityAttractionScale = velocityAttractionScale;
	def->velocityDamping = velocityDamping;
	def->colorDeltaScale = colorDeltaScale;
	def->speedBase = speedBase;
	def->speedRandom = speedRandom;
	def->colorStartA = colorStartA;
	def->colorStartR = colorStartR;
	def->colorStartG = colorStartG;
	def->colorStartB = colorStartB;
	def->colorRandomA = colorRandomA;
	def->colorRandomR = colorRandomR;
	def->colorRandomG = colorRandomG;
	def->colorRandomB = colorRandomB;
	def->colorEndA = colorEndA;
	def->colorEndR = colorEndR;
	def->colorEndG = colorEndG;
	def->colorEndB = colorEndB;
	def->particleSizeBase = particleSizeBase;
	def->particleSizeGrowth = particleSizeGrowth;
	def->particleSizeRandom = particleSizeRandom;
	def->particleLifetimeTicks = particleLifetimeTicks;
	def->initialAgeRandomTicks = initialAgeRandomTicks;
	def->randomizeColorEndpoints = randomizeColorEndpoints;
	def->textureModelType = textureModelType;

	FeDiskIo_SelectTextureFrame(textureModelType, textureFrame, 256);
	def->billboardScale = (uint16_t)(int64_t)((double)g_projScaleDiv512 * (double)billboardScaleMultiplier);
	def->staticTexLevel = g_modelTypeTable[textureModelType].curTexLevel;
	def->field64TexLevel = clearField64TexLevel ? NULL : g_modelTypeTable[textureModelType].curTexLevel;
	def->renderFlags = g_bilinearEnabled ? bilinearRenderFlags : renderFlags;
}

static void Particle_SetTemplateGapFloat(int templateIdx, size_t offset, float value) {
	memcpy(&g_particleEffectTemplates[templateIdx].gap70[offset], &value, sizeof(value));
}

// FUNCTION: XWA 0x4C6030
void Particle_InitEffectTemplates(void) {
	uint16_t randomTextureModelType;

	g_worldParticleEffects = NULL;
	Particle_ResetPools();

	Particle_SetEffectTemplate(3, 1.0f, 0.012f, 0.0042372881f, 30.0f, 15.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
							   0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.94999999f, 0.0f, 0.30000001f, 236,
							   39, 0, 233, 2, 512.0f, 36370, 36754, 0);
	Particle_SetEffectTemplate(10, 1.0f, 0.039999999f, 0.012820513f, 25.0f, 7.0f, 1.0f, 1.0f, 1.0f, 1.0f,
							   0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.05f, 0.0038135592f, 0.0f, 78,
							   0, 0, 275, 1, 1024.0f, 36370, 36754, 0);
	Particle_SetEffectTemplate(11, 1.0f, 0.039999999f, 0.0063694268f, 40.0f, 5.0f, 0.75f, 0.75f, 0.75f, 0.75f,
							   0.0f, 0.25f, 0.25f, 0.25f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.0006355932f, 0.0f,
							   157, 0, 0, 277, 1, 1664.0f, 36368, 36752, 0);
	Particle_SetEffectTemplate(2, 1.0f, 0.039999999f, 0.0025445293f, 0.5f, 0.029999999f, 1.0f, 0.80000001f,
							   0.80000001f, 0.80000001f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f,
							   0.001f, 0.0000025423728f, 0.0f, 393, 275, 0, 277, 1, 1664.0f, 36368, 36752, 0);
	Particle_SetEffectTemplate(0, 1.0f, 0.0049999999f, 0.0014124294f, 0.85000002f, 0.15000001f, 1.0f, 1.0f,
							   1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0049999999f,
							   0.0f, 0.0f, 708, 236, 0, 547, 0, 512.0f, 36370, 36754, 0);
	Particle_SetEffectTemplate(1, 1.0f, 0.0f, 0.0063694268f, 30.0f, 20.0f, 0.80000001f, 0.89999998f,
							   0.60000002f, 0.2f, 0.15000001f, 0.1f, 0.25f, 0.15000001f, 0.0f, 0.2f, 0.2f,
							   0.2f, 0.085000001f, 0.00050847459f, 0.0f, 157, 0, 0, 237, 1, 512.0f, 36370,
							   36754, 0);
	Particle_SetTemplateGapFloat(1, 0, -0.1f);
	Particle_SetTemplateGapFloat(1, 4, -0.2f);
	Particle_SetTemplateGapFloat(1, 8, -0.30000001f);
	Particle_SetTemplateGapFloat(1, 12, -0.02f);
	Particle_SetEffectTemplate(5, 1.0f, 0.30000001f, 0.0025445293f, 80.0f, 40.0f, 1.0f, 1.0f, 1.0f, 1.0f,
							   0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.4f, 0.0f, 0.60000002f, 393,
							   39, 0, 544, 1, 512.0f, 36370, 36754, 0);
	Particle_SetEffectTemplate(9, 1.0f, 0.02f, 0.012820513f, 12.5f, 3.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
							   0.0f, 0.0f, 0.0f, 0.75f, 0.75f, 0.75f, 4.5f, -0.031779662f, 0.0f, 78, 15, 0,
							   275, 1, 512.0f, 36370, 36754, 0);
	Particle_SetEffectTemplate(4, 1.0f, 0.0024999999f, 0.0015898251f, 40.0f, 20.0f, 1.0f, 1.0f, 0.69999999f,
							   0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.80000001f, 0.0f, 0.0f, 0.0f, 0.075000003f,
							   0.0f, 0.0f, 629, 157, 0, 267, 5, 2560.0f, 36370, 36754, 0);
	Particle_SetEffectTemplate(7, 1.0f, 0.02f, 0.012820513f, 12.5f, 3.5f, 1.0f, 1.0f, 0.69999999f, 0.2f, 0.0f,
							   0.0f, 0.0f, 0.0f, 0.1f, 0.5f, 0.0f, 0.0f, 0.25f, -0.0025423728f, 0.0f, 78, 15,
							   0, 237, 1, 512.0f, 36370, 36754, 0);
	Particle_SetEffectTemplate(12, 1.0f, 0.1f, 0.012820513f, 20.0f, 5.0f, 1.0f, 0.2f, 0.80000001f, 1.0f, 0.2f,
							   0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.050000001f, 0.0f, 0.0f, 78, 15, 0,
							   277, 1, 1664.0f, 36370, 36754, 1);

	randomTextureModelType = (uint16_t)((GameRand2() & 3u) + 233u);
	Particle_SetEffectTemplate(6, 1.0f, 0.0089999996f, 0.0042372881f, 30.0f, 10.0f, 1.0f, 1.0f, 1.0f, 1.0f,
							   0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 0.0f, 1.0f, 236, 157, 0,
							   randomTextureModelType, 1, 1024.0f, 3602, 3986, 0);
	Particle_SetEffectTemplate(8, 1.0f, 0.0024999999f, 0.0042372881f, 10.0f, 4.0f, 0.80000001f, 0.89999998f,
							   0.0f, 0.89999998f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.2f,
							   0.0000095338992f, 0.0f, 236, 78, 0, 277, 1, 2664.0f, 36370, 36754, 0);
}

// FUNCTION: XWA 0x4C8AB0
void Particle_InitEffectType0(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	unsigned int maxParticles;
	uint16_t textureModelType;
	uint16_t frameCount;
	uint16_t randValue;

	effect->def = &g_particleEffectTemplates[3];
	effect->auxBufferKind = 2;
	effect->effectType = 0;
	effect->particles = NULL;

	randValue = GameRand2();
	maxParticles = (unsigned int)(12 - (int)((double)Particle_SignedUnitFromRand(randValue) * -5.0));
	if (maxParticles > 25u) {
		maxParticles = 25u;
	} else {
		randValue = GameRand2();
		maxParticles = (unsigned int)(12 - (int)((double)Particle_SignedUnitFromRand(randValue) * -5.0));
	}
	effect->maxParticles = (int)maxParticles;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->yawBaseRad = 0.0f;
	effect->lifetimeTicks = 243;
	effect->emitUntilTicks = 15;
	effect->ageTicks = 0;
	effect->yawRandomRad = 6.2831845f;
	effect->spawnBatchCount = (float)(unsigned int)effect->maxParticles;
	effect->pitchBaseRad = 0.0f;
	effect->pitchRandomRad = 6.2831845f;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	textureModelType = (uint16_t)(OBJ_DebrisTextureGroup4000 + (GameRand2() & 3u));
	def = effect->def;
	effect->textureModelType = textureModelType;
	frameCount = g_modelTypeTable[textureModelType].frameCount;
	effect->textureFrameCount = frameCount;
	effect->textureAnimRate = (float)frameCount * 0.5f;

	if (!def->randomizeColorEndpoints) {
		randValue = GameRand2();
		effect->colorStartA =
			effect->def->colorRandomA * Particle_SignedUnitFromRand(randValue) + effect->def->colorStartA;
		randValue = GameRand2();
		effect->colorStartR =
			effect->def->colorRandomR * Particle_SignedUnitFromRand(randValue) + effect->def->colorStartR;
		randValue = GameRand2();
		effect->colorStartG =
			effect->def->colorRandomG * Particle_SignedUnitFromRand(randValue) + effect->def->colorStartG;
		randValue = GameRand2();
		effect->colorStartB =
			effect->def->colorRandomB * Particle_SignedUnitFromRand(randValue) + effect->def->colorStartB;
		effect->colorDeltaA = (effect->def->colorEndA - effect->colorStartA) * effect->def->colorDeltaScale;
		effect->colorDeltaR = (effect->def->colorEndR - effect->colorStartR) * effect->def->colorDeltaScale;
		effect->colorDeltaG = (effect->def->colorEndG - effect->colorStartG) * effect->def->colorDeltaScale;
		effect->colorDeltaB = (effect->def->colorEndB - effect->colorStartB) * effect->def->colorDeltaScale;
	} else {
		float endA;
		float endR;
		float endG;
		float endB;

		randValue = GameRand2();
		endA = effect->def->colorRandomA * Particle_SignedUnitFromRand(randValue) + effect->def->colorEndA;
		randValue = GameRand2();
		endR = effect->def->colorRandomR * Particle_SignedUnitFromRand(randValue) + effect->def->colorEndR;
		randValue = GameRand2();
		endG = effect->def->colorRandomG * Particle_SignedUnitFromRand(randValue) + effect->def->colorEndG;
		randValue = GameRand2();
		endB = effect->def->colorRandomB * Particle_SignedUnitFromRand(randValue) + effect->def->colorEndB;

		effect->colorStartA = effect->def->colorStartA;
		effect->colorStartR = effect->def->colorStartR;
		effect->colorStartG = effect->def->colorStartG;
		effect->colorStartB = effect->def->colorStartB;
		effect->colorDeltaA = (endA - effect->colorStartA) * effect->def->colorDeltaScale;
		effect->colorDeltaR = (endR - effect->colorStartR) * effect->def->colorDeltaScale;
		effect->colorDeltaG = (endG - effect->colorStartG) * effect->def->colorDeltaScale;
		effect->colorDeltaB = (endB - effect->colorStartB) * effect->def->colorDeltaScale;
	}

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
	effect->sourceVelocityScale = 1.0f;
}

// FUNCTION: XWA 0x4C9250
void Particle_InitEffectType1(ParticleEffect* effect) {
	unsigned int maxParticles;

	effect->def = &g_particleEffectTemplates[11];
	effect->auxBufferKind = 0;
	effect->effectType = 1;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->yawBaseRad = 0.0f;
	effect->lifetimeTicks = -1;
	effect->emitUntilTicks = -1;
	effect->ageTicks = 0;
	effect->yawRandomRad = 0.52359873f;
	effect->pitchBaseRad = 0.0f;
	effect->pitchRandomRad = 0.52359873f;
	effect->spawnBatchCount = 2.0f;
	effect->spawnRemainder = 0.0f;
	maxParticles = 2u * (unsigned int)effect->def->particleLifetimeTicks;
	if (maxParticles > 100u) {
		maxParticles = 100u;
	}
	effect->maxParticles = (int)maxParticles;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	effect->textureFrameCount = 0;
	effect->textureAnimRate = 1.0f;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
	effect->sourceVelocityScale = 1.0f;
}

// FUNCTION: XWA 0x4C9560
void Particle_InitEffectType2(ParticleEffect* effect) {
	unsigned int maxParticles;

	effect->def = &g_particleEffectTemplates[2];
	effect->auxBufferKind = 0;
	effect->effectType = 2;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->yawBaseRad = 0.0f;
	effect->lifetimeTicks = 472;
	effect->emitUntilTicks = 78;
	effect->ageTicks = 0;
	effect->yawRandomRad = 0.43633226f;
	effect->pitchBaseRad = 0.0f;
	effect->pitchRandomRad = 0.43633226f;
	effect->spawnBatchCount = 4.0f;
	effect->spawnRemainder = 0.0f;
	maxParticles = 4u * (unsigned int)effect->def->particleLifetimeTicks;
	if (maxParticles > 100u) {
		maxParticles = 100u;
	}
	effect->maxParticles = (int)maxParticles;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0125f;
	effect->useAttachedTransform = 0;
	effect->textureFrameCount = 0;
	effect->textureAnimRate = 1.0f;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
	effect->sourceVelocityScale = 1.0f;
}

// FUNCTION: XWA 0x4C8E60
void Particle_InitEffectType3(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	unsigned int maxParticles;
	uint16_t frameCount;

	effect->def = &g_particleEffectTemplates[10];
	effect->auxBufferKind = 2;
	effect->effectType = 3;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->emitUntilTicks = -1;
	effect->ageTicks = 0;
	effect->yawBaseRad = (float)(Particle_RandomSignedUnit() * 90.0 * 0.017453291);
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->lifetimeTicks = -1;
	effect->yawRandomRad = (float)((Particle_RandomSignedUnit() * 30.0 * 0.017453291 - -45.0) * 0.017453291);
	effect->pitchBaseRad = (float)(Particle_RandomSignedUnit() * 90.0 * 0.017453291);
	effect->pitchRandomRad =
		(float)((Particle_RandomSignedUnit() * 30.0 * 0.017453291 - -45.0) * 0.017453291);

	def = effect->def;
	effect->spawnBatchCount = 2.0f;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	maxParticles = 2u * (unsigned int)def->particleLifetimeTicks;
	if (maxParticles > 25u) {
		maxParticles = 25u;
	}
	effect->maxParticles = (int)maxParticles;
	effect->textureModelType = 275;
	frameCount = g_modelTypeTable[275].frameCount;
	effect->textureFrameCount = frameCount;
	effect->textureAnimRate = (float)frameCount;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
	effect->sourceVelocityScale = 1.0f;
}

// FUNCTION: XWA 0x4C9870
void Particle_InitEffectType4(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	unsigned int maxParticles;
	uint16_t frameCount;
	uint16_t randValue;

	effect->def = &g_particleEffectTemplates[0];
	effect->auxBufferKind = 0;
	effect->effectType = 4;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->lifetimeTicks = 747;
	effect->emitUntilTicks = 47;
	effect->ageTicks = 0;
	effect->yawBaseRad = 0.0f;

	randValue = GameRand2();
	effect->pitchBaseRad = 0.0f;
	effect->yawRandomRad = (float)((20.0 - (double)randValue * 0.00001525902 * -10.0) * 0.017453291);

	randValue = GameRand2();
	effect->spawnBatchCount = 15.0f;
	effect->pitchRandomRad = (float)((20.0 - (double)randValue * 0.00001525902 * -10.0) * 0.017453291);

	maxParticles = (unsigned int)(int)((double)effect->emitUntilTicks * 15.0);
	if (maxParticles > 100u) {
		maxParticles = 100u;
	}

	def = effect->def;
	effect->maxParticles = (int)maxParticles;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = -20.0f;
	effect->useAttachedTransform = 0;
	effect->textureModelType = def->textureModelType;
	frameCount = g_modelTypeTable[effect->textureModelType].frameCount;
	effect->textureFrameCount = frameCount;
	effect->textureAnimRate = (float)frameCount;

	Particle_InitEffectColorRamp(effect);

	effect->sourceVelocityScale = 0.0f;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->randomDiscEnabled = 0;
	effect->randomDiscRadius = 1.0f;
	effect->stretchedBillboard = 1;
	effect->scale = 1.0f;
}

// FUNCTION: XWA 0x4C9C00
void Particle_InitEffectType5(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	unsigned int maxParticles;
	uint16_t randValue;
	int lifetimeTicks;

	effect->def = &g_particleEffectTemplates[1];
	effect->auxBufferKind = 0;
	effect->effectType = 5;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;

	randValue = GameRand2();
	lifetimeTicks =
		(int)(236 *
			  (70 - (int)(((double)randValue * 0.00001525902 + (double)randValue * 0.00001525902 - 1.0) *
						  -20.0))) /
		30;
	effect->lifetimeTicks = lifetimeTicks;
	effect->yawBaseRad = 0.0f;
	effect->emitUntilTicks = lifetimeTicks - effect->def->particleLifetimeTicks;
	effect->ageTicks = 0;

	randValue = GameRand2();
	effect->pitchBaseRad = 0.0f;
	effect->yawRandomRad =
		(float)(0.17453291 -
				((double)randValue * 0.00001525902 + (double)randValue * 0.00001525902 - 1.0) * -0.052359872);

	randValue = GameRand2();
	def = effect->def;
	effect->spawnBatchCount = 5.0f;
	effect->pitchRandomRad =
		(float)(0.17453291 -
				((double)randValue * 0.00001525902 + (double)randValue * 0.00001525902 - 1.0) * -0.052359872);
	maxParticles = (unsigned int)(int)((double)def->particleLifetimeTicks * 5.0);
	if (maxParticles > 100u) {
		maxParticles = 100u;
	}
	effect->maxParticles = (int)maxParticles;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	effect->textureFrameCount = 0;
	effect->textureAnimRate = 1.0f;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 1;
	effect->randomDiscRadius = 25.0f;
	effect->stretchedBillboard = 0;
	effect->scale = 1.0f;
	effect->sourceVelocityScale = 1.0f;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->fieldD8 = effect->def->speedRandom * Particle_RandomSignedUnit() + effect->def->speedBase;
	effect->fieldDC = (float)((double)(uint16_t)GameRand2() * 0.00001525902 * (double)effect->fieldD8 * 0.75);
}

// FUNCTION: XWA 0x4CA030
void Particle_InitEffectType6(ParticleEffect* effect) {
	float particleLimit;
	int emitUntilTicks;
	unsigned int maxParticles;
	uint16_t frameCount;

	effect->def = &g_particleEffectTemplates[5];
	effect->auxBufferKind = 0;
	effect->effectType = 6;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->lifetimeTicks = 432;

	{
		ParticleEffectTemplate* def = effect->def;

		effect->emitUntilTicks = 432 - def->particleLifetimeTicks;
		effect->ageTicks = 0;
	}
	effect->yawBaseRad = 0.0f;
	emitUntilTicks = effect->emitUntilTicks;
	particleLimit = (float)emitUntilTicks;
	effect->yawRandomRad = 6.2831845f;
	effect->pitchBaseRad = 0.0f;
	effect->pitchRandomRad = 6.2831845f;
	effect->spawnBatchCount = 10.0f;
	particleLimit *= g_particleVelocityOutputScale;
	particleLimit *= g_particleDeltaTickScale;
	particleLimit *= g_particleTenFloat;
	maxParticles = (unsigned int)(int)particleLimit;
	if (maxParticles > 100u) {
		maxParticles = 100u;
	}
	effect->maxParticles = (int)maxParticles;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	effect->textureModelType = 544;
	frameCount = g_modelTypeTable[544].frameCount;
	effect->textureFrameCount = frameCount;
	effect->textureAnimRate = (float)frameCount;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->sourceVelocityScale = 1.0f;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
}

// FUNCTION: XWA 0x4CAA20
void Particle_InitEffectType7(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	int emitUntilTicks;
	unsigned int maxParticles;
	uint16_t frameCount;

	effect->def = &g_particleEffectTemplates[9];
	effect->auxBufferKind = 1;
	effect->effectType = 7;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->lifetimeTicks = 629;

	def = effect->def;
	effect->ageTicks = 0;
	effect->emitUntilTicks = 629 - def->particleLifetimeTicks;
	effect->yawBaseRad = 0.0f;
	emitUntilTicks = effect->emitUntilTicks;
	effect->yawRandomRad = 6.2831845f;
	effect->pitchBaseRad = 0.0f;
	effect->pitchRandomRad = 6.2831845f;
	effect->spawnBatchCount = 3.0f;
	maxParticles = (unsigned int)(int)((float)emitUntilTicks * g_particleThreeFloat);
	if (maxParticles > 50u) {
		maxParticles = 50u;
	}
	effect->maxParticles = (int)maxParticles;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	effect->textureModelType = 275;
	frameCount = g_modelTypeTable[275].frameCount;
	effect->textureFrameCount = frameCount;
	effect->textureAnimRate = (float)frameCount;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->sourceVelocityScale = 0.75f;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
}

// FUNCTION: XWA 0x4CAD60
void Particle_InitEffectType8(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	int lifetimeTicks;
	unsigned int maxParticles;
	uint16_t frameCount;

	effect->def = &g_particleEffectTemplates[4];
	effect->auxBufferKind = 2;
	effect->effectType = 8;
	effect->particles = NULL;
	maxParticles = (unsigned int)(int)((double)(uint16_t)GameRand2() * g_particleRandU16ToUnitFloatScale);
	++maxParticles;
	if (maxParticles > 25u) {
		maxParticles = 25u;
	} else {
		maxParticles = (unsigned int)(int)((double)(uint16_t)GameRand2() * g_particleRandU16ToUnitFloatScale);
		++maxParticles;
	}
	effect->maxParticles = (int)maxParticles;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->yawBaseRad = 0.0f;

	def = effect->def;
	lifetimeTicks = def->particleLifetimeTicks + 31;
	effect->emitUntilTicks = 31;
	effect->ageTicks = 0;
	effect->lifetimeTicks = lifetimeTicks;
	effect->spawnBatchCount = (float)maxParticles;
	effect->yawRandomRad = 6.2831845f;
	effect->pitchBaseRad = 0.0f;
	effect->pitchRandomRad = 6.2831845f;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	effect->textureModelType = 275;
	frameCount = g_modelTypeTable[275].frameCount;
	effect->textureFrameCount = frameCount;
	effect->textureAnimRate = (float)frameCount;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
	effect->sourceVelocityScale = 1.0f;
	effect->particleSpawnCallback = Particle_SpawnChildEffectOnRecord;
}

// FUNCTION: XWA 0x4CA380
void Particle_InitEffectType10(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	int lifetimeTicks;
	int emitUntilTicks;
	unsigned int maxParticles;

	effect->def = &g_particleEffectTemplates[12];
	effect->auxBufferKind = 0;
	effect->effectType = 10;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;

	def = effect->def;
	lifetimeTicks = def->particleLifetimeTicks + 15;
	effect->lifetimeTicks = lifetimeTicks;
	effect->ageTicks = 0;
	effect->yawBaseRad = 0.0f;
	effect->emitUntilTicks = lifetimeTicks - def->particleLifetimeTicks;
	emitUntilTicks = effect->emitUntilTicks;
	effect->yawRandomRad = 6.2831845f;
	effect->pitchBaseRad = 0.0f;
	effect->pitchRandomRad = 6.2831845f;
	effect->spawnBatchCount = 20.0f;
	maxParticles = (unsigned int)(int)((double)emitUntilTicks * 20.0);
	if (maxParticles > 100u) {
		maxParticles = 100u;
	}
	effect->maxParticles = (int)maxParticles;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	effect->textureFrameCount = 0;
	effect->textureAnimRate = 1.0f;

	Particle_InitEffectColorRamp(effect);

	effect->sourceVelocityScale = 0.0f;
	effect->randomDiscEnabled = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->randomDiscRadius = 1.0f;
	effect->stretchedBillboard = 1;
	effect->scale = 1.0f;
}

// FUNCTION: XWA 0x4CA6B0
void Particle_InitEffectType11(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	int maxParticles;
	uint16_t textureModelType;
	uint16_t frameCount;
	uint16_t randValue;

	effect->def = &g_particleEffectTemplates[6];
	effect->auxBufferKind = 2;
	effect->effectType = 11;
	effect->particles = NULL;
	maxParticles = (GameRand2() & 2) + 3;
	if (maxParticles > 25) {
		maxParticles = 25;
	} else {
		maxParticles = (GameRand2() & 2) + 3;
	}
	effect->maxParticles = maxParticles;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = Particle_SpawnTransientObjectFromRecord;
	effect->yawRandomRad = 1.5707961f;
	effect->pitchRandomRad = 1.5707961f;
	effect->yawBaseRad = 0.0f;
	effect->lifetimeTicks = 243;
	effect->emitUntilTicks = 15;
	effect->ageTicks = 0;
	effect->pitchBaseRad = 0.0f;
	effect->spawnBatchCount = (float)((unsigned int)effect->maxParticles / 15u);
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	textureModelType = (uint16_t)((GameRand2() & 3u) + 233u);
	def = effect->def;
	effect->textureModelType = textureModelType;
	frameCount = g_modelTypeTable[textureModelType].frameCount;
	effect->textureFrameCount = frameCount;
	effect->textureAnimRate = (float)frameCount * g_particleHalfFloat;

	if (!def->randomizeColorEndpoints) {
		randValue = GameRand2();
		effect->colorStartA =
			effect->def->colorRandomA * Particle_SignedUnitFromRand(randValue) + effect->def->colorStartA;
		randValue = GameRand2();
		effect->colorStartR =
			effect->def->colorRandomR * Particle_SignedUnitFromRand(randValue) + effect->def->colorStartR;
		randValue = GameRand2();
		effect->colorStartG =
			effect->def->colorRandomG * Particle_SignedUnitFromRand(randValue) + effect->def->colorStartG;
		randValue = GameRand2();
		effect->colorStartB =
			effect->def->colorRandomB * Particle_SignedUnitFromRand(randValue) + effect->def->colorStartB;
		effect->colorDeltaA = (effect->def->colorEndA - effect->colorStartA) * effect->def->colorDeltaScale;
		effect->colorDeltaR = (effect->def->colorEndR - effect->colorStartR) * effect->def->colorDeltaScale;
		effect->colorDeltaG = (effect->def->colorEndG - effect->colorStartG) * effect->def->colorDeltaScale;
		effect->colorDeltaB = (effect->def->colorEndB - effect->colorStartB) * effect->def->colorDeltaScale;
	} else {
		float endA;
		float endR;
		float endG;
		float endB;

		randValue = GameRand2();
		endA = effect->def->colorRandomA * Particle_SignedUnitFromRand(randValue) + effect->def->colorEndA;
		randValue = GameRand2();
		endR = effect->def->colorRandomR * Particle_SignedUnitFromRand(randValue) + effect->def->colorEndR;
		randValue = GameRand2();
		endG = effect->def->colorRandomG * Particle_SignedUnitFromRand(randValue) + effect->def->colorEndG;
		randValue = GameRand2();
		endB = effect->def->colorRandomB * Particle_SignedUnitFromRand(randValue) + effect->def->colorEndB;

		effect->colorStartA = effect->def->colorStartA;
		effect->colorStartR = effect->def->colorStartR;
		effect->colorStartG = effect->def->colorStartG;
		effect->colorStartB = effect->def->colorStartB;
		effect->colorDeltaA = (endA - effect->colorStartA) * effect->def->colorDeltaScale;
		effect->colorDeltaR = (endR - effect->colorStartR) * effect->def->colorDeltaScale;
		effect->colorDeltaG = (endG - effect->colorStartG) * effect->def->colorDeltaScale;
		effect->colorDeltaB = (endB - effect->colorStartB) * effect->def->colorDeltaScale;
	}

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
	effect->sourceVelocityScale = 1.0f;
}

// FUNCTION: XWA 0x4CB0E0
void Particle_InitEffectType9(ParticleEffect* effect) {
	ParticleEffectTemplate* def;
	int emitUntilTicks;
	unsigned int maxParticles;

	effect->def = &g_particleEffectTemplates[7];
	effect->auxBufferKind = 1;
	effect->effectType = 9;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->lifetimeTicks = 472;

	def = effect->def;
	effect->ageTicks = 0;
	effect->yawBaseRad = 0.0f;
	effect->emitUntilTicks = 472 - def->particleLifetimeTicks;
	emitUntilTicks = effect->emitUntilTicks;
	effect->yawRandomRad = 6.2831845f;
	effect->pitchBaseRad = 0.0f;
	effect->pitchRandomRad = 6.2831845f;
	effect->spawnBatchCount = 5.0f;
	maxParticles = (unsigned int)(int)((double)emitUntilTicks * 5.0);
	if (maxParticles > 50u) {
		maxParticles = 50u;
	}
	effect->useAttachedTransform = 1;
	effect->maxParticles = (int)maxParticles;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->textureFrameCount = 0;
	effect->textureAnimRate = 1.0f;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->sourceVelocityScale = 0.5f;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
}

// FUNCTION: XWA 0x4CB410
void Particle_InitEffectType12(ParticleEffect* effect) {
	effect->def = &g_particleEffectTemplates[8];
	effect->auxBufferKind = 1;
	effect->effectType = 12;
	effect->particles = NULL;
	effect->particleCount = 0;
	effect->parentParticle = NULL;
	effect->particleSpawnCallback = NULL;
	effect->particleFreeCallback = NULL;
	effect->lifetimeTicks = -1;
	effect->emitUntilTicks = -1;
	effect->ageTicks = 0;
	effect->yawRandomRad = 6.2831845f;
	effect->pitchRandomRad = 6.2831845f;
	effect->yawBaseRad = 0.0f;
	effect->pitchBaseRad = 0.0f;
	effect->spawnBatchCount = 1.0f;
	effect->maxParticles = 50;
	effect->spawnRemainder = 0.0f;
	effect->accelX = 0.0f;
	effect->accelY = 0.0f;
	effect->accelZ = 0.0f;
	effect->useAttachedTransform = 1;
	effect->textureFrameCount = 0;
	effect->textureAnimRate = 1.0f;

	Particle_InitEffectColorRamp(effect);

	effect->randomDiscEnabled = 0;
	effect->stretchedBillboard = 0;
	effect->next = NULL;
	effect->fieldD0 = 0;
	effect->fieldD4 = 0;
	effect->sourceVelocityScale = 0.80000001f;
	effect->randomDiscRadius = 1.0f;
	effect->scale = 1.0f;
}

// FUNCTION: XWA 0x4C8140
void Particle_SpawnTransientObjectFromRecord(ParticleRecord* particle) {
	(void)particle;

	/* TODO: Reimplement Particle_SpawnTransientObjectFromRecord @ 0x4C8140. */
}

// FUNCTION: XWA 0x4C8300
void Particle_SpawnChildEffectOnRecord(ParticleRecord* particle, ParticleEffect* effect) {
	ParticleEffect* childEffect;

	(void)effect;
	childEffect = Particle_AllocEffect(9);
	if (childEffect) {
		ParticleEffect* oldChildEffects;

		oldChildEffects = particle->childEffects;
		childEffect->next = oldChildEffects;
		childEffect->ageTicks = 0;

		childEffect->localOffset.z = 0.0f;
		childEffect->localOffset.y = 0.0f;
		childEffect->localOffset.x = 0.0f;
		childEffect->velocity.z = 0.0f;
		childEffect->velocity.y = 0.0f;
		childEffect->velocity.x = 0.0f;

		childEffect->lastUpdateTime = g_gameTime;
		childEffect->objectIdx = (uint16_t)-1;
		childEffect->parentParticle = particle;
		GameRand2();
		particle->childEffects = childEffect;
	}
}

// FUNCTION: XWA 0x4C8070
ParticleEffect* Particle_AttachEffectToObject(int effectType, uint16_t objectIdx, const Vec3f* localOffset,
											  const Vec3f* direction) {
	ParticleEffect* effect;

	if (!g_particleEffectsEnabled && effectType != 10) {
		return NULL;
	}

	effect = Particle_AllocEffect(effectType);
	if (effect) {
		effect->objectIdx = objectIdx;
		effect->next = g_objRenderState[objectIdx].particleEffects;
		effect->ageTicks = 0;

		if (localOffset) {
			effect->localOffset = *localOffset;
		} else {
			effect->localOffset.z = 0.0f;
			effect->localOffset.y = 0.0f;
			effect->localOffset.x = 0.0f;
		}

		if (direction) {
			effect->pitchBaseRad = (float)-atan2(direction->x, direction->y);
			effect->yawBaseRad = (float)asin(direction->z);
		}

		effect->lastUpdateTime = g_gameTime;
		g_objRenderState[objectIdx].particleEffects = effect;
	}

	return effect;
}

// FUNCTION: XWA 0x4C8370
ParticleEffect* Particle_CreateWorldEffect(int effectType, const Vec3f* worldPos,
										   const Vec3f* worldVelocity) {
	ParticleEffect* effect;

	if (!g_particleEffectsEnabled && effectType != 10) {
		return NULL;
	}

	effect = Particle_AllocEffect(effectType);
	if (effect) {
		effect->objectIdx = (uint16_t)-1;
		effect->next = g_worldParticleEffects;
		effect->ageTicks = 0;
		effect->world = *worldPos;
		effect->localOffset = *worldPos;
		effect->velocity = *worldVelocity;
		effect->lastUpdateTime = g_gameTime;
		g_worldParticleEffects = effect;
#ifdef XWA_MODERN
		ParticleModernState* state = Particle_ModernEffectState(effect);
		if (state)
			Particle_ModernSetFloatPoint(&state->point, worldPos->x, worldPos->y, worldPos->z);
#endif
	}

	return effect;
}

// FUNCTION: XWA 0x4C74F0
void Particle_UpdateEffect(ParticleEffect* effect) {
	ParticleRecord* particle;
	ParticleRecord* prevParticle;
	ParticleEffectTemplate* def;
#ifdef XWA_MODERN
	int modernHighRate;
	int modernEmissionBatchPeriods;
#endif

	prevParticle = NULL;
	particle = effect->particles;
	def = effect->def;
	g_particleDeltaScale = (float)(uint16_t)g_elapsedTicks * g_particleDeltaTickScale;
#ifdef XWA_MODERN
	modernHighRate = XwaModernFlightTiming_IsHighRate();
	modernEmissionBatchPeriods = 1;
#endif

	while (particle) {
#ifdef XWA_MODERN
		int oldAgeTicks = (int)particle->ageTicks;
#endif
		particle->ageTicks = (int16_t)(particle->ageTicks + (uint16_t)g_elapsedTicks);
		if ((int)particle->ageTicks < def->particleLifetimeTicks) {
			float age;
			int ageTicks;

			ageTicks = (int)particle->ageTicks;
#ifdef XWA_MODERN
			Particle_ModernAdvanceRecord(particle, g_particleDeltaScale);
#endif
			particle->world.x += g_particleDeltaScale * particle->vel.x;
			particle->world.y += g_particleDeltaScale * particle->vel.y;
			particle->world.z += g_particleDeltaScale * particle->vel.z;
			particle->vel.x +=
				(effect->accelX * def->velocityAttractionScale - particle->vel.x * def->velocityDamping) *
				g_particleDeltaScale;
			particle->vel.y +=
				(effect->accelY * def->velocityAttractionScale - def->velocityDamping * particle->vel.y) *
				g_particleDeltaScale;
			particle->vel.z +=
				(effect->accelZ * def->velocityAttractionScale - def->velocityDamping * particle->vel.z) *
				g_particleDeltaScale;

			age = (float)ageTicks;
			particle->argbColor =
				(((uint16_t)(int)((effect->colorDeltaA * age + effect->colorStartA) *
								  g_particleColorByteScale) *
					  256u +
				  (uint16_t)(int)((effect->colorDeltaR * age + effect->colorStartR) *
								  g_particleColorByteScale)) *
					 256u +
				 (uint16_t)(int)((effect->colorDeltaG * age + effect->colorStartG) *
								 g_particleColorByteScale)) *
					256u +
				(uint16_t)(int)((effect->colorDeltaB * age + effect->colorStartB) * g_particleColorByteScale);
			prevParticle = particle;

#ifdef XWA_MODERN
			if (modernHighRate) {
				float elapsed;
				float growthAge;

				/* The recovered update adds growth * newAge once per legacy update.
				 * Integrate the quadratic extension of that recurrence so substeps
				 * remain smooth and land on the same value every eight ticks. */
				elapsed = (float)(uint16_t)g_elapsedTicks;
				growthAge = elapsed *
							((float)(2 * oldAgeTicks) + elapsed + (float)PARTICLE_LEGACY_UPDATE_TICKS) *
							(1.0f / (2.0f * (float)PARTICLE_LEGACY_UPDATE_TICKS));
				particle->size += def->particleSizeGrowth * growthAge;
			} else
#endif
			{
				particle->size += def->particleSizeGrowth * age;
			}

			if (particle->childEffects) {
				ParticleEffect* prevChild;
				ParticleEffect* childEffect;

				prevChild = NULL;
				childEffect = particle->childEffects;
				while (childEffect) {
					int childAge;
					int childLifetime;
					ParticleEffect* nextChild;

					if ((uint16_t)g_elapsedTicks != 0) {
						childEffect->ageTicks += (uint16_t)g_elapsedTicks;
					}
					childAge = childEffect->ageTicks;
					childEffect->lastUpdateTime = g_gameTime;
					childLifetime = childEffect->lifetimeTicks;
					if (childAge < childLifetime ||
						(childLifetime == -1 && childEffect->freeParticles != NULL)) {
						Particle_UpdateEffect(childEffect);
						prevChild = childEffect;
						childEffect = childEffect->next;
					} else {
						nextChild = childEffect->next;
						if (childEffect == particle->childEffects) {
							particle->childEffects = nextChild;
						}
						if (prevChild) {
							prevChild->next = nextChild;
						}
						if ((childEffect->particleSpawnCallback || childEffect->particleFreeCallback) &&
							childEffect->particles) {
							ParticleRecord* childParticle;

							childParticle = childEffect->particles;
							while (childParticle) {
								ParticleEffect* grandChild;

								grandChild = childParticle->childEffects;
								while (grandChild) {
									ParticleEffect* nextGrandChild = grandChild->next;

									Particle_FreeEffectWithChildren(grandChild);
									grandChild = nextGrandChild;
								}

								if (childEffect->particleFreeCallback) {
									childEffect->particleFreeCallback(childParticle);
								}
								childParticle = childParticle->next;
							}
						}

						Particle_FreeEffect(childEffect);
						childEffect = nextChild;
					}
				}
			}

			particle = particle->next;
		} else {
			ParticleRecord* nextParticle;
			ParticleEffect* childEffect;

			if (effect->particleFreeCallback) {
				effect->particleFreeCallback(particle);
			}

			nextParticle = particle->next;
			if (prevParticle) {
				prevParticle->next = nextParticle;
			}
			if (particle == effect->particles) {
				effect->particles = nextParticle;
			}

			particle->next = effect->freeParticles;
			effect->freeParticles = particle;
			--effect->particleCount;

			childEffect = particle->childEffects;
			if (childEffect) {
				while (childEffect) {
					ParticleEffect* nextChild = childEffect->next;

					if ((childEffect->particleSpawnCallback || childEffect->particleFreeCallback) &&
						childEffect->particles) {
						ParticleRecord* childParticle;

						childParticle = childEffect->particles;
						do {
							if (childParticle->childEffects) {
								Particle_FreeEffectListWithChildren(childParticle->childEffects);
							}
							if (childEffect->particleFreeCallback) {
								childEffect->particleFreeCallback(childParticle);
							}
							childParticle = childParticle->next;
						} while (childParticle);
					}

					Particle_FreeEffect(childEffect);
					childEffect = nextChild;
				}
				particle->childEffects = NULL;
			}
			particle = nextParticle;
		}
	}

#ifdef XWA_MODERN
	if (modernHighRate) {
		ParticleModernState* modernState;
		uint16_t pendingBefore;
		uint32_t pendingTotal;
		int elapsedTicks;
		int firstBoundaryAge;
		int duePeriods;

		/* Particle templates express spawnBatchCount per legacy update, not
		 * per elapsed tick. Retain a per-effect phase so records still advance
		 * every high-rate frame while emission and its RNG/callback side effects
		 * occur once per original eight-tick period. */
		modernState = Particle_ModernEffectState(effect);
		pendingBefore = modernState ? modernState->pendingEmissionTicks : 0;
		elapsedTicks = (int)(uint16_t)g_elapsedTicks;
		pendingTotal = (uint32_t)pendingBefore + (uint32_t)elapsedTicks;
		duePeriods = (int)(pendingTotal / PARTICLE_LEGACY_UPDATE_TICKS);
		if (modernState) {
			modernState->pendingEmissionTicks = (uint16_t)(pendingTotal % PARTICLE_LEGACY_UPDATE_TICKS);
		}

		modernEmissionBatchPeriods = 0;
		firstBoundaryAge =
			effect->ageTicks - elapsedTicks + (PARTICLE_LEGACY_UPDATE_TICKS - (int)pendingBefore);
		for (int period = 0; period < duePeriods; ++period) {
			int boundaryAge = firstBoundaryAge + period * PARTICLE_LEGACY_UPDATE_TICKS;
			if (effect->emitUntilTicks != -1 && boundaryAge >= effect->emitUntilTicks) {
				break;
			}
			++modernEmissionBatchPeriods;
		}
	}

	if ((!modernHighRate && (effect->ageTicks < effect->emitUntilTicks || effect->emitUntilTicks == -1)) ||
		(modernHighRate && modernEmissionBatchPeriods != 0)) {
#else
	if (effect->ageTicks < effect->emitUntilTicks || effect->emitUntilTicks == -1) {
#endif
		Matrix3x3 mat;
		Vec3f sourceVelocity;
		int spawnCount;
		float spawnStep;
#ifdef XWA_MODERN
		float spawnDeltaScale;
		uint16_t spawnElapsedTicks;

		spawnElapsedTicks = modernHighRate ? PARTICLE_LEGACY_UPDATE_TICKS : (uint16_t)g_elapsedTicks;
		spawnDeltaScale = (float)spawnElapsedTicks * g_particleDeltaTickScale;
#endif

		if (effect->useAttachedTransform) {
			if (effect->parentParticle) {
				ParticleRecord* parent;

				parent = effect->parentParticle;
#ifdef XWA_MODERN
				sourceVelocity.x = parent->vel.x * spawnDeltaScale * effect->sourceVelocityScale;
				sourceVelocity.y = parent->vel.y * spawnDeltaScale * effect->sourceVelocityScale;
				sourceVelocity.z = parent->vel.z * spawnDeltaScale * effect->sourceVelocityScale;
#else
				sourceVelocity.x = parent->vel.x * g_particleDeltaScale * effect->sourceVelocityScale;
				sourceVelocity.y = parent->vel.y * g_particleDeltaScale * effect->sourceVelocityScale;
				sourceVelocity.z = parent->vel.z * g_particleDeltaScale * effect->sourceVelocityScale;
#endif
				effect->world = parent->world;
#ifdef XWA_MODERN
				ParticleModernState* effectState = Particle_ModernEffectState(effect);
				ParticleModernState* parentState = Particle_ModernRecordState(parent);
				if (effectState) {
					if (parentState && parentState->point.valid)
						effectState->point = parentState->point;
					else
						Particle_ModernSetFloatPoint(&effectState->point, effect->world.x, effect->world.y,
													 effect->world.z);
				}
#endif
			} else if (effect->objectIdx == UINT16_MAX) {
				sourceVelocity.x = effect->velocity.x * effect->sourceVelocityScale;
				sourceVelocity.y = effect->velocity.y * effect->sourceVelocityScale;
				sourceVelocity.z = effect->velocity.z * effect->sourceVelocityScale;
				effect->world = effect->localOffset;
			} else {
				MobileObject** mobjRef;

				mobjRef = &g_objectTable[effect->objectIdx].mobj;

				mat.m[0] = (float)(*mobjRef)->cachedSideX * g_particleQ15MatrixToFloatScale;
				mat.m[1] = (float)(*mobjRef)->cachedSideY * g_particleQ15MatrixToFloatScale;
				mat.m[2] = (float)(*mobjRef)->cachedSideZ * g_particleQ15MatrixToFloatScale;
				mat.m[3] = -((float)(*mobjRef)->cachedFwdX * g_particleQ15MatrixToFloatScale);
				mat.m[4] = -((float)(*mobjRef)->cachedFwdY * g_particleQ15MatrixToFloatScale);
				mat.m[5] = -((float)(*mobjRef)->cachedFwdZ * g_particleQ15MatrixToFloatScale);
				mat.m[6] = (float)(*mobjRef)->cachedUpX * g_particleQ15MatrixToFloatScale;
				mat.m[7] = (float)(*mobjRef)->cachedUpY * g_particleQ15MatrixToFloatScale;
				mat.m[8] = (float)(*mobjRef)->cachedUpZ * g_particleQ15MatrixToFloatScale;

				effect->world = effect->localOffset;
				Math3D_RotateVec3(&effect->world, &mat);
#ifdef XWA_MODERN
				ParticleModernState* effectState = Particle_ModernEffectState(effect);
				if (effectState) {
					const int32_t base[3] = { g_objectTable[effect->objectIdx].world_x,
											  g_objectTable[effect->objectIdx].world_y,
											  g_objectTable[effect->objectIdx].world_z };
					const float offset[3] = { effect->world.x, effect->world.y, effect->world.z };
					Particle_ModernSetPoint(&effectState->point, base, offset);
				}
#endif
				effect->world.x += (float)g_objectTable[effect->objectIdx].world_x;
				effect->world.y += (float)g_objectTable[effect->objectIdx].world_y;
				effect->world.z += (float)g_objectTable[effect->objectIdx].world_z;

				if (effect->sourceVelocityScale != g_particleZeroFloat) {
					MobileObject* mobj;

					mobj = g_objectTable[effect->objectIdx].mobj;
					if (mobj->speed != 0) {
						int moveScale;
						float baseVelX;
						float baseVelY;
						float baseVelZ;
#ifdef XWA_MODERN
						moveScale = (int)spawnElapsedTicks * ((4660 * (int)mobj->speed + 128) >> 8) / 236;
#else
						moveScale =
							(int)(uint16_t)g_elapsedTicks * ((4660 * (int)mobj->speed + 128) >> 8) / 236;
#endif
						if (mobj->velocityOverrideActive) {
							baseVelX = (float)Xwa_Q15Mul(moveScale, mobj->velocityOverrideDirX);
							baseVelY = (float)Xwa_Q15Mul(
								moveScale, g_objectTable[effect->objectIdx].mobj->velocityOverrideDirY);
							baseVelZ = (float)Xwa_Q15Mul(
								moveScale, g_objectTable[effect->objectIdx].mobj->velocityOverrideDirZ);
						} else {
							baseVelX = (float)Xwa_Q15Mul(moveScale, mobj->moveX);
							baseVelY =
								(float)Xwa_Q15Mul(moveScale, g_objectTable[effect->objectIdx].mobj->moveY);
							baseVelZ =
								(float)Xwa_Q15Mul(moveScale, g_objectTable[effect->objectIdx].mobj->moveZ);
						}
						sourceVelocity.x = baseVelX * effect->sourceVelocityScale;
						sourceVelocity.y = baseVelY * effect->sourceVelocityScale;
						sourceVelocity.z = baseVelZ * effect->sourceVelocityScale;
					} else {
						sourceVelocity.z = 0.0f;
						sourceVelocity.y = 0.0f;
						sourceVelocity.x = 0.0f;
					}
				} else {
					sourceVelocity.z = 0.0f;
					sourceVelocity.y = 0.0f;
					sourceVelocity.x = 0.0f;
				}
			}
		} else {
			effect->world.z = 0.0f;
			effect->world.y = 0.0f;
			effect->world.x = 0.0f;
			sourceVelocity.z = 0.0f;
			sourceVelocity.y = 0.0f;
			sourceVelocity.x = 0.0f;
		}

#ifdef XWA_MODERN
		for (int emissionBatch = 0; emissionBatch < modernEmissionBatchPeriods; ++emissionBatch) {
#endif
			spawnCount = (int)effect->spawnBatchCount;
			spawnStep = g_particleUnitFloat / effect->spawnBatchCount;
			while (spawnCount != 0) {
				ParticleRecord* newParticle;
				float speed;
				float yaw;
				float pitch;
				float cosYaw;
				float phase;
				int64_t phaseSpawnCount;

				if ((unsigned int)effect->particleCount >= (unsigned int)effect->maxParticles) {
					break;
				}

				newParticle = Particle_AllocRecord(effect);
				if (newParticle) {
					speed = def->speedRandom * Particle_RandomSignedUnit() + def->speedBase;
					yaw = effect->yawRandomRad * Particle_RandomSignedUnit() + effect->yawBaseRad;
					pitch = effect->pitchRandomRad * Particle_RandomSignedUnit() + effect->pitchBaseRad;
					cosYaw = (float)cos(yaw);
					newParticle->vel.x = (float)-(sin(pitch) * cosYaw);
					newParticle->vel.y = (float)(cos(pitch) * cosYaw);
					newParticle->vel.z = (float)sin(yaw);

					if (effect->objectIdx != UINT16_MAX && effect->useAttachedTransform) {
						Math3D_RotateVec3(&newParticle->vel, &mat);
					}

					newParticle->vel.x = speed * newParticle->vel.x + sourceVelocity.x;
					newParticle->vel.y = speed * newParticle->vel.y + sourceVelocity.y;
					newParticle->vel.z = speed * newParticle->vel.z + sourceVelocity.z;

					phaseSpawnCount = (unsigned int)spawnCount;
					phase = (float)phaseSpawnCount * spawnStep;
#ifdef XWA_MODERN
					float preciseSpawnOffset[3];
					preciseSpawnOffset[2] = newParticle->vel.z - phase * sourceVelocity.z;
#endif
					if (!effect->randomDiscEnabled) {
						newParticle->world.x =
							newParticle->vel.x + effect->world.x - phase * sourceVelocity.x;
						newParticle->world.y =
							effect->world.y + newParticle->vel.y - phase * sourceVelocity.y;
#ifdef XWA_MODERN
						preciseSpawnOffset[0] = newParticle->vel.x - phase * sourceVelocity.x;
						preciseSpawnOffset[1] = newParticle->vel.y - phase * sourceVelocity.y;
#endif
					} else {
						float angle;

						angle = Particle_RandomSignedUnit() * g_particleTauRadians;
						newParticle->world.x = (float)-sin(angle);
						newParticle->world.y = (float)cos(angle);
						newParticle->world.x *= effect->randomDiscRadius;
						newParticle->world.x = newParticle->vel.x + effect->world.x -
											   phase * sourceVelocity.x + newParticle->world.x;
						newParticle->world.y *= effect->randomDiscRadius;
						newParticle->world.y = effect->world.y + newParticle->vel.y -
											   phase * sourceVelocity.y + newParticle->world.y;
						newParticle->world.z *= effect->randomDiscRadius;
#ifdef XWA_MODERN
						preciseSpawnOffset[0] = newParticle->world.x - effect->world.x;
						preciseSpawnOffset[1] = newParticle->world.y - effect->world.y;
#endif
					}
					newParticle->world.z = effect->world.z + newParticle->vel.z - phase * sourceVelocity.z;
#ifdef XWA_MODERN
					if (effect->useAttachedTransform)
						Particle_ModernSetSpawnPoint(newParticle, effect, preciseSpawnOffset);
#endif

					newParticle->vel.x = newParticle->vel.x * g_particleVelocityOutputScale;
					newParticle->vel.y = newParticle->vel.y * g_particleVelocityOutputScale;
					newParticle->vel.z = newParticle->vel.z * g_particleVelocityOutputScale;
					if (def->initialAgeRandomTicks) {
						newParticle->ageTicks = (int16_t)((uint16_t)GameRand2() % def->initialAgeRandomTicks);
					} else {
						newParticle->ageTicks = 0;
					}
					newParticle->argbColor = (uint32_t)(uintptr_t)&effect->colorStartA;
					newParticle->size =
						def->particleSizeRandom * Particle_RandomSignedUnit() + def->particleSizeBase;

					if (effect->particleSpawnCallback) {
						effect->particleSpawnCallback(newParticle, effect);
					}
					--spawnCount;
				} else {
					OutputDebugStringA("badparticle\n");
					spawnCount = 0;
				}
			}

			effect->spawnRemainder += (float)(unsigned int)spawnCount;
#ifdef XWA_MODERN
		}
#endif
	}
}

// FUNCTION: XWA 0x4C8410
void Particle_UpdateWorldEffects(void) {
	ParticleEffect* effect;
	ParticleEffect* prevEffect;

	effect = g_worldParticleEffects;
	prevEffect = NULL;
	while (effect != NULL) {
		ParticleEffect* nextEffect;

		if ((uint16_t)g_elapsedTicks != 0) {
			effect->ageTicks += (uint16_t)g_elapsedTicks;
		}
		effect->lastUpdateTime = g_gameTime;
		if (effect->ageTicks < effect->lifetimeTicks || effect->lifetimeTicks == -1) {
			Particle_UpdateEffect(effect);
			if (effect->particles != NULL) {
				Particle_DrawEffectBillboards(effect);
			}
			prevEffect = effect;
			effect = effect->next;
			continue;
		}

		nextEffect = effect->next;
		if (effect == g_worldParticleEffects) {
			g_worldParticleEffects = nextEffect;
		}
		if (prevEffect != NULL) {
			prevEffect->next = nextEffect;
		}

		if (effect->particleSpawnCallback || effect->particleFreeCallback) {
			ParticleRecord* particle;

			for (particle = effect->particles; particle != NULL; particle = particle->next) {
				ParticleEffect* childEffect;

				childEffect = particle->childEffects;
				while (childEffect != NULL) {
					ParticleEffect* nextChild;

					nextChild = childEffect->next;
					Particle_FreeEffectWithChildren(childEffect);
					childEffect = nextChild;
				}

				if (effect->particleFreeCallback) {
					effect->particleFreeCallback(particle);
				}
			}
		}

		Particle_FreeEffect(effect);
		effect = nextEffect;
	}
}

// FUNCTION: XWA 0x4C7F30
void Particle_DrawEffectBillboards(ParticleEffect* effect) {
	ParticleRecord* particle;

	if (effect->particleSpawnCallback) {
		for (particle = effect->particles; particle != NULL; particle = particle->next) {
			ParticleEffect* childEffect;

			childEffect = particle->childEffects;
			if (childEffect != NULL && childEffect->particles != NULL) {
				if (!childEffect->stretchedBillboard) {
					RenderBillboard_DrawSpriteFacingCamera(childEffect, childEffect->def->renderFlags);
				} else {
					RenderBillboard_DrawStretched(childEffect, childEffect->def->renderFlags);
				}
			}
		}
	}

	if (effect->useAttachedTransform) {
		if (!effect->stretchedBillboard) {
			RenderBillboard_DrawSpriteFacingCamera(effect, effect->def->renderFlags);
		} else {
			RenderBillboard_DrawStretched(effect, effect->def->renderFlags);
		}
		return;
	}

	if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
		if (effect->objectIdx == g_players[g_localPlayer].objectIndex) {
			return;
		}
		if (!effect->stretchedBillboard) {
			RenderBillboard_DrawOriented(effect, effect->def->renderFlags);
		} else {
			RenderBillboard_DrawOrientedStretched(effect, effect->def->renderFlags);
		}
		return;
	}

	if (effect->objectIdx != g_players[g_localPlayer].objectIndex ||
		!g_players[g_localPlayer].viewState.externalCameraActive) {
		if (!effect->stretchedBillboard) {
			RenderBillboard_DrawOriented(effect, effect->def->renderFlags);
		} else {
			RenderBillboard_DrawOrientedStretched(effect, effect->def->renderFlags);
		}
	}
}

// FUNCTION: XWA 0x4C8820
void Particle_DrawObjectEffectsForCrt(uint16_t objectIdx) {
	ParticleEffect* effect;

	for (effect = g_objRenderState[objectIdx].particleEffects; effect != NULL; effect = effect->next) {
		Particle_DrawEffectBillboards(effect);
	}
}

// FUNCTION: XWA 0x4C8850
void Particle_UpdateObjectEffectsForCrt(uint16_t objectIdx) {
	ParticleEffect* prevEffect;
	ParticleEffect* effect;

	prevEffect = NULL;
	effect = g_objRenderState[objectIdx].particleEffects;
	while (effect != NULL) {
		ParticleEffect* nextEffect;

		if ((uint16_t)g_elapsedTicks != 0) {
			effect->ageTicks += (uint16_t)g_elapsedTicks;
		}
		effect->lastUpdateTime = g_gameTime;
		if (effect->ageTicks < effect->lifetimeTicks || effect->lifetimeTicks == -1) {
			Particle_UpdateEffect(effect);
			Particle_DrawEffectBillboards(effect);
			prevEffect = effect;
			effect = effect->next;
			continue;
		}

		nextEffect = effect->next;
		if (effect == g_objRenderState[objectIdx].particleEffects) {
			g_objRenderState[objectIdx].particleEffects = nextEffect;
		}
		if (prevEffect != NULL) {
			prevEffect->next = nextEffect;
		}

		if (effect->particleSpawnCallback || effect->particleFreeCallback) {
			ParticleRecord* particle;

			for (particle = effect->particles; particle != NULL; particle = particle->next) {
				ParticleEffect* childEffect;

				childEffect = particle->childEffects;
				while (childEffect != NULL) {
					ParticleEffect* nextChild;

					nextChild = childEffect->next;
					Particle_FreeEffectWithChildren(childEffect);
					childEffect = nextChild;
				}

				if (effect->particleFreeCallback) {
					effect->particleFreeCallback(particle);
				}
			}
		}

		Particle_FreeEffect(effect);
		effect = nextEffect;
	}
}

// FUNCTION: XWA 0x4C8500
void Particle_FreeAllEffects(void) {
	ParticleEffect* effect;
	uint32_t objectIdx;

	effect = g_worldParticleEffects;
	if (effect) {
		do {
			ParticleEffect* nextEffect;

			nextEffect = effect->next;
			if (effect == g_worldParticleEffects) {
				g_worldParticleEffects = nextEffect;
			}

			if (effect->particleSpawnCallback || effect->particleFreeCallback) {
				ParticleRecord* particle;

				particle = effect->particles;
				if (particle) {
					do {
						ParticleEffect* childEffect;

						childEffect = particle->childEffects;
						if (childEffect) {
							do {
								ParticleEffect* nextChild;

								nextChild = childEffect->next;
								Particle_FreeEffectWithChildren(childEffect);
								childEffect = nextChild;
							} while (childEffect);
						}

						if (effect->particleFreeCallback) {
							effect->particleFreeCallback(particle);
						}
						particle = particle->next;
					} while (particle);
				}
			}

			Particle_FreeEffect(effect);
			effect = nextEffect;
		} while (effect);
	}

	objectIdx = g_activeRegionObjectSlotStart;
	while ((uint16_t)objectIdx < g_regionStaticObjectSlotEnd) {
		ParticleEffect* objectEffect;

		objectEffect = g_objRenderState[(uint16_t)objectIdx].particleEffects;
		if (objectEffect) {
			do {
				ParticleEffect* nextEffect;

				nextEffect = objectEffect->next;
				if ((uintptr_t)objectEffect->particleSpawnCallback |
					(uintptr_t)objectEffect->particleFreeCallback) {
					ParticleRecord* particle;

					particle = objectEffect->particles;
					if (particle) {
						do {
							if (particle->childEffects) {
								Particle_FreeEffectListWithChildren(particle->childEffects);
							}

							if (objectEffect->particleFreeCallback) {
								objectEffect->particleFreeCallback(particle);
							}
							particle = particle->next;
						} while (particle);
					}
				}

				Particle_FreeEffect(objectEffect);
				objectEffect = nextEffect;
			} while (objectEffect);
		}
		g_objRenderState[(uint16_t)objectIdx].particleEffects = NULL;
		++objectIdx;
	}
}

// FUNCTION: XWA 0x4C8630
void Particle_UpdateObjectEffects(void) {
	uint32_t objectIdx;

	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_regionStaticObjectSlotEnd; ++objectIdx) {
		ParticleEffect* effect;

		effect = g_objRenderState[objectIdx].particleEffects;
		if (g_objectTable[objectIdx].objectType != OBJ_None) {
			ParticleEffect* prevEffect;

			if (effect != NULL) {
				prevEffect = NULL;
				while (effect != NULL) {
					ParticleEffect* nextEffect;

					if ((uint16_t)g_elapsedTicks != 0) {
						effect->ageTicks += (uint16_t)g_elapsedTicks;
					}
					effect->lastUpdateTime = g_gameTime;
					if (effect->ageTicks < effect->lifetimeTicks || effect->lifetimeTicks == -1) {
						if (g_objRenderState[objectIdx].drawnThisFrame) {
							Particle_UpdateEffect(effect);
							Particle_DrawEffectBillboards(effect);
						}
						prevEffect = effect;
						effect = effect->next;
						continue;
					}

					nextEffect = effect->next;
					if (effect == g_objRenderState[objectIdx].particleEffects) {
						g_objRenderState[objectIdx].particleEffects = nextEffect;
					}
					if (prevEffect != NULL) {
						prevEffect->next = nextEffect;
					}

					if (effect->particleSpawnCallback || effect->particleFreeCallback) {
						ParticleRecord* particle;

						for (particle = effect->particles; particle != NULL; particle = particle->next) {
							ParticleEffect* childEffect;

							childEffect = particle->childEffects;
							while (childEffect != NULL) {
								ParticleEffect* nextChild;

								nextChild = childEffect->next;
								Particle_FreeEffectWithChildren(childEffect);
								childEffect = nextChild;
							}

							if (effect->particleFreeCallback) {
								effect->particleFreeCallback(particle);
							}
						}
					}

					Particle_FreeEffect(effect);
					effect = nextEffect;
				}
			}
		} else if (effect != NULL) {
			ParticleEffect* objectEffect;

			objectEffect = g_objRenderState[(uint16_t)objectIdx].particleEffects;
			while (objectEffect != NULL) {
				ParticleEffect* nextEffect;

				nextEffect = objectEffect->next;
				if (objectEffect->particleSpawnCallback || objectEffect->particleFreeCallback) {
					ParticleRecord* particle;

					for (particle = objectEffect->particles; particle != NULL; particle = particle->next) {
						if (particle->childEffects != NULL) {
							Particle_FreeEffectListWithChildren(particle->childEffects);
						}
						if (objectEffect->particleFreeCallback) {
							objectEffect->particleFreeCallback(particle);
						}
					}
				}
				Particle_FreeEffect(objectEffect);
				objectEffect = nextEffect;
			}
			g_objRenderState[(uint16_t)objectIdx].particleEffects = NULL;
		}
	}
}

// FUNCTION: XWA 0x4CC290
void Particle_ResetPools(void) {
	int idx;

	/* Rebuild the 100-record aux block free list, linking each block to the
	   next and pointing it at its slice of the record pool. The final block is
	   linked to NULL after the loop. */
	g_particleAuxBlockFreeList100 = g_particleAuxBlockPool100;
	for (idx = 0; idx < PARTICLE_POOL100_BLOCK_COUNT - 1; ++idx) {
		g_particleAuxBlockPool100[idx].next = &g_particleAuxBlockPool100[idx + 1];
		g_particleAuxBlockPool100[idx].records = g_particleRecordPool100[idx];
	}
	g_particleAuxBlockPool100[PARTICLE_POOL100_BLOCK_COUNT - 1].records =
		g_particleRecordPool100[PARTICLE_POOL100_BLOCK_COUNT - 1];
	g_particleAuxBlockPool100[PARTICLE_POOL100_BLOCK_COUNT - 1].next = NULL;

	g_particleAuxBlockFreeList50 = g_particleAuxBlockPool50;
	for (idx = 0; idx < PARTICLE_POOL50_BLOCK_COUNT - 1; ++idx) {
		g_particleAuxBlockPool50[idx].next = &g_particleAuxBlockPool50[idx + 1];
		g_particleAuxBlockPool50[idx].records = g_particleRecordPool50[idx];
	}
	g_particleAuxBlockPool50[PARTICLE_POOL50_BLOCK_COUNT - 1].next = NULL;
	g_particleAuxBlockPool50[PARTICLE_POOL50_BLOCK_COUNT - 1].records =
		g_particleRecordPool50[PARTICLE_POOL50_BLOCK_COUNT - 1];

	g_particleAuxBlockFreeList25 = g_particleAuxBlockPool25;
	for (idx = 0; idx < PARTICLE_POOL25_BLOCK_COUNT - 1; ++idx) {
		g_particleAuxBlockPool25[idx].next = &g_particleAuxBlockPool25[idx + 1];
		g_particleAuxBlockPool25[idx].records = g_particleRecordPool25[idx];
	}
	g_particleAuxBlockPool25[PARTICLE_POOL25_BLOCK_COUNT - 1].next = NULL;
	g_particleAuxBlockPool25[PARTICLE_POOL25_BLOCK_COUNT - 1].records =
		g_particleRecordPool25[PARTICLE_POOL25_BLOCK_COUNT - 1];

	g_particleEffectActiveCount = 0;
	for (idx = 0; idx < PARTICLE_EFFECT_POOL_COUNT - 1; ++idx) {
		g_particleEffectPool[idx].next = &g_particleEffectPool[idx + 1];
		g_particleEffectPool[idx].def = NULL;
		g_particleEffectPool[idx].particleBuffer = NULL;
		g_particleEffectPool[idx].auxBuffer = NULL;
	}
	g_particleEffectPool[PARTICLE_EFFECT_POOL_COUNT - 1].next = NULL;
	g_particleEffectPool[PARTICLE_EFFECT_POOL_COUNT - 1].def = NULL;
	g_particleEffectPool[PARTICLE_EFFECT_POOL_COUNT - 1].particleBuffer = NULL;
	g_particleEffectPool[PARTICLE_EFFECT_POOL_COUNT - 1].auxBuffer = NULL;
	g_particleEffectFreeList = g_particleEffectPool;
}

static __inline ParticleEffect* Particle_TakeNextLink(ParticleEffect** link) {
	ParticleEffect* next;

	next = *link;
	*link = NULL;
	return next;
}

// FUNCTION: XWA 0x4CC3A0
ParticleEffect* Particle_AllocEffect(int effectType) {
	ParticleEffect* effect;
	ParticleRecord* records;
	ParticleAuxBlockRef* auxBuffer;
	int lastRecordIdx;

	effect = g_particleEffectFreeList;
	if (effect) {
#ifdef XWA_MODERN
		ParticleModernState* modernState = &g_particleEffectModern[effect - g_particleEffectPool];
		modernState->snapshotId = Particle_NextSnapshotId();
		modernState->point.valid = 0;
		modernState->pendingEmissionTicks = 0;
#endif
		g_particleEffectFreeList = Particle_TakeNextLink(&effect->next);
		++g_particleEffectActiveCount;

		switch (effectType) {
			case 0:
				Particle_InitEffectType0(effect);
				break;
			case 3:
				Particle_InitEffectType3(effect);
				break;
			case 1:
				Particle_InitEffectType1(effect);
				break;
			case 2:
				Particle_InitEffectType2(effect);
				break;
			case 4:
				Particle_InitEffectType4(effect);
				break;
			case 5:
				Particle_InitEffectType5(effect);
				break;
			case 6:
				Particle_InitEffectType6(effect);
				break;
			case 7:
				Particle_InitEffectType7(effect);
				break;
			case 8:
				Particle_InitEffectType8(effect);
				break;
			case 9:
				Particle_InitEffectType9(effect);
				break;
			case 10:
				Particle_InitEffectType10(effect);
				break;
			case 11:
				Particle_InitEffectType11(effect);
				break;
			case 12:
				Particle_InitEffectType12(effect);
				break;
			default:
				break;
		}

		records = NULL;
		switch (effect->auxBufferKind) {
			case 0:
				if (g_particleAuxBlockFreeList100) {
					records = g_particleAuxBlockFreeList100->records;
					effect->auxBuffer = g_particleAuxBlockFreeList100;
					g_particleAuxBlockFreeList100 = g_particleAuxBlockFreeList100->next;
				}
				break;
			case 1:
				if (g_particleAuxBlockFreeList50) {
					records = g_particleAuxBlockFreeList50->records;
					effect->auxBuffer = g_particleAuxBlockFreeList50;
					g_particleAuxBlockFreeList50 = g_particleAuxBlockFreeList50->next;
				}
				break;
			case 2:
				if (g_particleAuxBlockFreeList25) {
					records = g_particleAuxBlockFreeList25->records;
					effect->auxBuffer = g_particleAuxBlockFreeList25;
					g_particleAuxBlockFreeList25 = g_particleAuxBlockFreeList25->next;
				}
				break;
			default:
				OutputDebugStringA("BADBADMORE\n");
				break;
		}

		auxBuffer = effect->auxBuffer;
		effect->particleBuffer = records;
		if (auxBuffer) {
			effect->freeParticles = records;
			lastRecordIdx = 0;
			if (effect->maxParticles - 1 > 0) {
				int recordIdx;
				int remainingLinks;

				remainingLinks = effect->maxParticles - 1;
				recordIdx = 0;
				lastRecordIdx = remainingLinks;
				do {
					effect->freeParticles[recordIdx].childEffects = NULL;
					effect->freeParticles[recordIdx].next = &effect->freeParticles[recordIdx + 1];
					++recordIdx;
					--remainingLinks;
				} while (remainingLinks);
			}

			effect->freeParticles[lastRecordIdx].next = NULL;
			effect->freeParticles[lastRecordIdx].childEffects = NULL;
			effect->particles = NULL;

			return effect;
		}
	}

	return NULL;
}

// FUNCTION: XWA 0x4CC560
void Particle_FreeEffect(ParticleEffect* effect) {
	if (effect->auxBuffer) {
		switch (effect->auxBufferKind) {
			case 0:
				effect->auxBuffer->next = g_particleAuxBlockFreeList100;
				g_particleAuxBlockFreeList100 = effect->auxBuffer;
				break;

			case 1:
				effect->auxBuffer->next = g_particleAuxBlockFreeList50;
				g_particleAuxBlockFreeList50 = effect->auxBuffer;
				break;

			case 2:
				effect->auxBuffer->next = g_particleAuxBlockFreeList25;
				g_particleAuxBlockFreeList25 = effect->auxBuffer;
				break;

			default:
				g_OutputDebugStringA("BADBADBAD\n");
				break;
		}

		effect->particleBuffer = NULL;
		effect->auxBuffer = NULL;
	}

	effect->next = g_particleEffectFreeList;
	g_particleEffectFreeList = effect;
	--g_particleEffectActiveCount;
}

// FUNCTION: XWA 0x4C89F0
void Particle_FreeEffectWithChildren(ParticleEffect* effect) {
	ParticleRecord* particle;

	if (effect->particleSpawnCallback || effect->particleFreeCallback) {
		for (particle = effect->particles; particle; particle = particle->next) {
			ParticleEffect* childEffect = particle->childEffects;

			while (childEffect) {
				ParticleEffect* nextChild = childEffect->next;

				Particle_FreeEffectWithChildren(childEffect);
				childEffect = nextChild;
			}

			if (effect->particleFreeCallback) {
				effect->particleFreeCallback(particle);
			}
		}
	}

	Particle_FreeEffect(effect);
}

// FUNCTION: XWA 0x4C8A50
void Particle_FreeEffectListWithChildren(ParticleEffect* effectList) {
	ParticleEffect* effect = effectList;
	ParticleRecord* particle;

	while (effect) {
		ParticleEffect* nextEffect = effect->next;

		if (effect->particleSpawnCallback || effect->particleFreeCallback) {
			for (particle = effect->particles; particle; particle = particle->next) {
				if (particle->childEffects) {
					Particle_FreeEffectListWithChildren(particle->childEffects);
				}

				if (effect->particleFreeCallback) {
					effect->particleFreeCallback(particle);
				}
			}
		}

		Particle_FreeEffect(effect);
		effect = nextEffect;
	}
}

// FUNCTION: XWA 0x4C8960
void Particle_FreeObjectEffects(uint16_t objIdx) {
	ParticleEffect* effect;

	effect = g_objRenderState[objIdx].particleEffects;
	while (effect) {
		ParticleEffect* nextEffect;

		nextEffect = effect->next;
		if (effect->particleSpawnCallback || effect->particleFreeCallback) {
			ParticleRecord* particle;

			particle = effect->particles;
			while (particle) {
				if (particle->childEffects) {
					Particle_FreeEffectListWithChildren(particle->childEffects);
				}

				if (effect->particleFreeCallback) {
					effect->particleFreeCallback(particle);
				}

				particle = particle->next;
			}
		}

		Particle_FreeEffect(effect);
		effect = nextEffect;
	}

	g_objRenderState[objIdx].particleEffects = NULL;
}

// FUNCTION: XWA 0x4CB710
void Particle_AppendObjectEffectPointLights(uint16_t objectIdx) {
	int outLightCount;
	ParticleEffect* effect;

	outLightCount = g_objectPointLightCount;
	effect = g_objRenderState[objectIdx].particleEffects;
	while (effect) {
		PointLight* pointLight;

		if (outLightCount >= XWA_OBJECT_POINT_LIGHT_COUNT) {
			break;
		}

		if (effect->effectType != 1) {
			pointLight = &g_objectPointLights[outLightCount];
			pointLight->fixed.x = (int)effect->localOffset.x;
			pointLight->fixed.y = (int)effect->localOffset.y;
			pointLight->fixed.z = (int)effect->localOffset.z;
			pointLight->intensity = 125.0f;
			pointLight->colorR = 1.0f;
			pointLight->colorG = 1.0f;
			pointLight->colorB = 0.75f;
			++outLightCount;
		}

		effect = effect->next;
	}

	g_objectPointLightCount = outLightCount;
}

// FUNCTION: XWA 0x4CC5F0
ParticleRecord* Particle_AllocRecord(ParticleEffect* effect) {
	ParticleRecord* particle = NULL;

	++g_particleRecordAllocAttemptCount;
	if ((unsigned int)effect->particleCount < (unsigned int)effect->maxParticles) {
		ParticleRecord* freeParticle = effect->freeParticles;

		if (freeParticle) {
			ParticleRecord* nextFreeParticle;
			ParticleRecord* oldParticleHead;

			particle = freeParticle;
#ifdef XWA_MODERN
			ParticleModernState* modernState = Particle_ModernRecordState(particle);
			if (modernState) {
				modernState->snapshotId = Particle_NextSnapshotId();
				modernState->point.valid = 0;
			}
#endif
			nextFreeParticle = particle->next;
			effect->freeParticles = nextFreeParticle;
			oldParticleHead = effect->particles;
			effect->particles = particle;
			particle->next = oldParticleHead;
			effect->particleCount = effect->particleCount + 1;
			particle->childEffects = NULL;
		}
	}

	return particle;
}

#ifdef XWA_MODERN
uint32_t Particle_SnapshotEffectId(const ParticleEffect* effect) {
	const ParticleModernState* state = Particle_ModernEffectState(effect);
	return state ? state->snapshotId : 0;
}

uint32_t Particle_SnapshotRecordId(const ParticleRecord* particle) {
	const ParticleModernState* state = Particle_ModernRecordState(particle);
	return state ? state->snapshotId : 0;
}

static int Particle_SnapshotPoint(const ParticleModernState* state, int32_t base[3], float offset[3]) {
	if (!state || !state->point.valid)
		return 0;
	for (int i = 0; i < 3; i++) {
		base[i] = state->point.base[i];
		offset[i] = state->point.offset[i];
	}
	return 1;
}

int Particle_SnapshotEffectPoint(const ParticleEffect* effect, int32_t base[3], float offset[3]) {
	return Particle_SnapshotPoint(Particle_ModernEffectState(effect), base, offset);
}

int Particle_SnapshotRecordPoint(const ParticleRecord* particle, int32_t base[3], float offset[3]) {
	return Particle_SnapshotPoint(Particle_ModernRecordState(particle), base, offset);
}

void Particle_SetWorldEffectPreciseOrigin(ParticleEffect* effect, const int32_t world[3]) {
	ParticleModernState* state = Particle_ModernEffectState(effect);
	if (!state || !world)
		return;
	const float offset[3] = { 0.0f, 0.0f, 0.0f };
	Particle_ModernSetPoint(&state->point, world, offset);
}
#endif

// FUNCTION: XWA 0x4CC720
ObjectTrailPoint* ObjectTrail_AllocPoint(void) {
	ObjectTrailPoint* point;
	ObjectTrailPoint** nextLink;

	point = g_objectTrailFreePoints;
	if (point == NULL) {
		int i;

		g_objectTrailFreePoints = (ObjectTrailPoint*)Memory_AllocTagged(
			RENDER_DATAPOOL_TAG, sizeof(ObjectTrailPoint) * OBJECT_TRAIL_POINT_BLOCK_CAPACITY);
		RenderDataPool_RegisterAllocation(g_objectTrailFreePoints);
		g_objectTrailPointPoolCount += OBJECT_TRAIL_POINT_BLOCK_CAPACITY;

		for (i = 0; i < OBJECT_TRAIL_POINT_BLOCK_CAPACITY; ++i) {
			g_objectTrailFreePoints[i].next = &g_objectTrailFreePoints[i + 1];
		}
		g_objectTrailFreePoints[OBJECT_TRAIL_POINT_BLOCK_CAPACITY - 1].next = NULL;
		point = g_objectTrailFreePoints;
	}

	nextLink = &g_objectTrailFreePoints->next;
	point = g_objectTrailFreePoints;
	g_objectTrailFreePoints = *nextLink;
	*nextLink = NULL;
	++g_objectTrailActivePointCount;
	return point;
}

// FUNCTION: XWA 0x4CC7C0
void ObjectTrail_FreePoint(ObjectTrailPoint* point) {
	point->next = g_objectTrailFreePoints;
	g_objectTrailFreePoints = point;
	--g_objectTrailActivePointCount;
}

// FUNCTION: XWA 0x4CC640
ObjectTrailEmitter* ObjectTrail_AllocEmitter(void) {
	ObjectTrailEmitter* emitter;
	ObjectTrailEmitter** nextLink;

	emitter = g_objectTrailFreeEmitters;
	if (emitter == NULL) {
		int i;

		g_objectTrailFreeEmitters = (ObjectTrailEmitter*)Memory_AllocTagged(
			RENDER_DATAPOOL_TAG, sizeof(ObjectTrailEmitter) * OBJECT_TRAIL_EMITTER_BLOCK_CAPACITY);
		RenderDataPool_RegisterAllocation(g_objectTrailFreeEmitters);
		g_objectTrailEmitterPoolCount += OBJECT_TRAIL_EMITTER_BLOCK_CAPACITY;

		for (i = 0; i < OBJECT_TRAIL_EMITTER_BLOCK_CAPACITY; ++i) {
			g_objectTrailFreeEmitters[i].next = &g_objectTrailFreeEmitters[i + 1];
			g_objectTrailFreeEmitters[i].pointHead = NULL;
		}
		g_objectTrailFreeEmitters[OBJECT_TRAIL_EMITTER_BLOCK_CAPACITY - 1].next = NULL;
		emitter = g_objectTrailFreeEmitters;
	}

	nextLink = &g_objectTrailFreeEmitters->next;
	emitter = g_objectTrailFreeEmitters;
	g_objectTrailFreeEmitters = *nextLink;
	*nextLink = NULL;
	++g_objectTrailActiveEmitterCount;
	return emitter;
}

// FUNCTION: XWA 0x4CC6F0
ObjectTrailEmitter* ObjectTrail_FreeEmitter(ObjectTrailEmitter* emitter) {
	emitter->next = g_objectTrailFreeEmitters;
	g_objectTrailFreeEmitters = emitter;
	--g_objectTrailActiveEmitterCount;
	emitter->pointHead = NULL;
	return emitter;
}

// FUNCTION: XWA 0x4CBC60
void ObjectTrail_InitTorpedoEmitter(ObjectTrailEmitter* trail) {
	float texVRate;

	trail->animFrameCount = 0;
	trail->animRateScale = 1.0f;
	trail->lifetimeTicks = 125;
	trail->ageRate = 0.0080000004f;
	trail->alphaFadeStart = 0.2f;
	trail->alphaFadeRate = 1.25f;
	trail->startAlphaBias = -0.75f;
	trail->sourceModelExtent = (uint16_t)g_modelTypeTable[OBJ_WarheadTorpedo].maxBoundsExtent;
	trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_NEAREST;
	if (g_bilinearEnabled) {
		trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_BILINEAR;
	}

	trail->textureModelType = (uint32_t)OBJ_TrailTextureGroup21000_Sprite000;
	trail->forwardOffset = -300;
	trail->argbColor = (unsigned int)-1;
	trail->ribbonWidth = 35.0f;
	FeDiskIo_SelectTextureFrame((uint16_t)trail->textureModelType, 1u, 256);
	texVRate = g_particleFiveFloat / (float)trail->lifetimeTicks;
	trail->curTexLevel = g_modelTypeTable[trail->textureModelType].curTexLevel;
	trail->animTicksAccum = 0;
	trail->texVRate = texVRate;
}

// FUNCTION: XWA 0x4CBD10
void ObjectTrail_InitMagPulseEmitter(ObjectTrailEmitter* trail) {
	float texVRate;

	trail->animFrameCount = 0;
	trail->animRateScale = 1.0f;
	trail->lifetimeTicks = 125;
	trail->ageRate = 0.0080000004f;
	trail->alphaFadeStart = 0.2f;
	trail->alphaFadeRate = 1.25f;
	trail->startAlphaBias = -0.5f;
	trail->sourceModelExtent = (uint16_t)g_modelTypeTable[OBJ_WarheadTorpedo].maxBoundsExtent;
	trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_NEAREST;
	if (g_bilinearEnabled) {
		trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_BILINEAR;
	}

	trail->textureModelType = (uint32_t)OBJ_TrailTextureGroup21005;
	trail->forwardOffset = -300;
	trail->argbColor = (unsigned int)-1;
	trail->ribbonWidth = 36.0f;
	FeDiskIo_SelectTextureFrame((uint16_t)trail->textureModelType, 1u, 256);
	texVRate = g_particleFiveFloat / (float)trail->lifetimeTicks;
	trail->curTexLevel = g_modelTypeTable[trail->textureModelType].curTexLevel;
	trail->animTicksAccum = 0;
	trail->texVRate = texVRate;
}

// FUNCTION: XWA 0x4CBDC0
void ObjectTrail_InitIonPulseEmitter(ObjectTrailEmitter* trail) {
	float texVRate;

	trail->animFrameCount = 0;
	trail->animRateScale = 1.0f;
	trail->lifetimeTicks = 100;
	trail->ageRate = 0.0099999998f;
	trail->alphaFadeStart = 0.2f;
	trail->alphaFadeRate = 1.25f;
	trail->startAlphaBias = -0.5f;
	trail->sourceModelExtent = (uint16_t)g_modelTypeTable[OBJ_WarheadTorpedo].maxBoundsExtent;
	trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_NEAREST;
	if (g_bilinearEnabled) {
		trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_BILINEAR;
	}

	trail->textureModelType = (uint32_t)OBJ_TrailTextureGroup21010;
	trail->forwardOffset = -300;
	trail->argbColor = (unsigned int)-1;
	trail->ribbonWidth = 35.0f;
	FeDiskIo_SelectTextureFrame((uint16_t)trail->textureModelType, 1u, 256);
	texVRate = g_particleFiveFloat / (float)trail->lifetimeTicks;
	trail->curTexLevel = g_modelTypeTable[trail->textureModelType].curTexLevel;
	trail->animTicksAccum = 0;
	trail->texVRate = texVRate;
}

// FUNCTION: XWA 0x4CBE70
void ObjectTrail_InitMissileEmitter(ObjectTrailEmitter* trail) {
	float texVRate;

	trail->lifetimeTicks = 75;
	trail->ageRate = 0.013333334f;
	trail->alphaFadeStart = 0.2f;
	trail->alphaFadeRate = 1.25f;
	trail->startAlphaBias = 0.0f;
	trail->sourceModelExtent = (uint16_t)g_modelTypeTable[OBJ_WarheadMissile].maxBoundsExtent;
	trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_NEAREST;
	if (g_bilinearEnabled) {
		trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_BILINEAR;
	}

	trail->textureModelType = (uint32_t)OBJ_TrailTextureGroup21020;
	trail->forwardOffset = -300;
	trail->argbColor = (unsigned int)-1;
	trail->ribbonWidth = 35.0f;
	FeDiskIo_SelectTextureFrame((uint16_t)trail->textureModelType, 1u, 256);
	texVRate = g_particleUnitFloat / (float)trail->lifetimeTicks;
	trail->curTexLevel = g_modelTypeTable[trail->textureModelType].curTexLevel;
	trail->animFrameCount = 0;
	trail->animRateScale = 1.0f;
	trail->animTicksAccum = 0;
	trail->texVRate = texVRate;
}

// FUNCTION: XWA 0x4CBF20
void ObjectTrail_InitRocketEmitter(ObjectTrailEmitter* trail) {
	float texVRate;

	trail->lifetimeTicks = 175;
	trail->ageRate = 0.0057142857f;
	trail->alphaFadeStart = 0.2f;
	trail->alphaFadeRate = 1.25f;
	trail->startAlphaBias = 0.75f;
	trail->sourceModelExtent = (uint16_t)g_modelTypeTable[OBJ_WarheadMissile].maxBoundsExtent;
	trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_NEAREST;
	if (g_bilinearEnabled) {
		trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_BILINEAR;
	}

	trail->textureModelType = (uint32_t)OBJ_TrailTextureGroup21020;
	trail->forwardOffset = -300;
	trail->argbColor = (unsigned int)-1;
	trail->ribbonWidth = 100.0f;
	FeDiskIo_SelectTextureFrame((uint16_t)trail->textureModelType, 1u, 256);
	texVRate = g_particleUnitFloat / (float)trail->lifetimeTicks;
	trail->curTexLevel = g_modelTypeTable[trail->textureModelType].curTexLevel;
	trail->animFrameCount = 0;
	trail->animRateScale = 1.0f;
	trail->animTicksAccum = 0;
	trail->texVRate = texVRate;
}

// FUNCTION: XWA 0x4CBFD0
void ObjectTrail_InitAdvancedTorpedoEmitter(ObjectTrailEmitter* trail) {
	float texVRate;

	trail->lifetimeTicks = 125;
	trail->ageRate = 0.0080000004f;
	trail->alphaFadeStart = 0.2f;
	trail->alphaFadeRate = 1.25f;
	trail->startAlphaBias = 1.0f;
	trail->sourceModelExtent = (uint16_t)g_modelTypeTable[OBJ_WarheadAdvancedTorpedo].maxBoundsExtent;
	trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_NEAREST;
	if (g_bilinearEnabled) {
		trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_BILINEAR;
	}

	trail->textureModelType = (uint32_t)OBJ_TrailTextureGroup21015;
	trail->forwardOffset = -300;
	trail->argbColor = (unsigned int)-1;
	trail->ribbonWidth = 35.0f;
	FeDiskIo_SelectTextureFrame((uint16_t)trail->textureModelType, 1u, 256);
	texVRate = g_particleUnitFloat / (float)trail->lifetimeTicks;
	trail->curTexLevel = g_modelTypeTable[trail->textureModelType].curTexLevel;
	trail->animFrameCount = 0;
	trail->animRateScale = 1.0f;
	trail->animTicksAccum = 0;
	trail->texVRate = texVRate;
}

// FUNCTION: XWA 0x4CC080
void ObjectTrail_InitAdvancedMissileEmitter(ObjectTrailEmitter* trail) {
	float texVRate;

	trail->lifetimeTicks = 75;
	trail->ageRate = 0.013333334f;
	trail->alphaFadeStart = 0.2f;
	trail->alphaFadeRate = 1.25f;
	trail->startAlphaBias = 1.0f;
	trail->sourceModelExtent = (uint16_t)g_modelTypeTable[OBJ_WarheadAdvancedMissile].maxBoundsExtent;
	trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_NEAREST;
	if (g_bilinearEnabled) {
		trail->renderFlags = OBJECT_TRAIL_RENDER_FLAGS_BILINEAR;
	}

	trail->textureModelType = (uint32_t)OBJ_TrailTextureGroup21025;
	trail->forwardOffset = -300;
	trail->argbColor = (unsigned int)-1;
	trail->ribbonWidth = 35.0f;
	FeDiskIo_SelectTextureFrame((uint16_t)trail->textureModelType, 1u, 256);
	texVRate = g_particleUnitFloat / (float)trail->lifetimeTicks;
	trail->curTexLevel = g_modelTypeTable[trail->textureModelType].curTexLevel;
	trail->animFrameCount = 0;
	trail->animRateScale = 1.0f;
	trail->animTicksAccum = 0;
	trail->texVRate = texVRate;
}

// FUNCTION: XWA 0x4CB8F0
ObjectTrailEmitter* ObjectTrail_CreateEmitter(uint16_t objectIdx, uint16_t objectType) {
	ObjectTrailEmitter* trail;

	trail = NULL;
	if (!g_useHardware3D || !g_trailsEnabled) {
		return NULL;
	}

	switch (objectType) {
		case OBJ_WarheadTorpedo:
			trail = ObjectTrail_AllocEmitter();
			trail->trailKind = 0;
			ObjectTrail_InitTorpedoEmitter(trail);
			break;

		case OBJ_WarheadMissile:
			trail = ObjectTrail_AllocEmitter();
			trail->trailKind = 1;
			ObjectTrail_InitMissileEmitter(trail);
			break;

		case OBJ_WarheadAdvancedTorpedo:
			trail = ObjectTrail_AllocEmitter();
			trail->trailKind = 3;
			ObjectTrail_InitAdvancedTorpedoEmitter(trail);
			break;

		case OBJ_WarheadAdvancedMissile:
			trail = ObjectTrail_AllocEmitter();
			trail->trailKind = 2;
			ObjectTrail_InitAdvancedMissileEmitter(trail);
			break;

		case OBJ_WarheadMagPulse:
			trail = ObjectTrail_AllocEmitter();
			trail->trailKind = 4;
			ObjectTrail_InitMagPulseEmitter(trail);
			break;

		case OBJ_WarheadIonPulse:
			trail = ObjectTrail_AllocEmitter();
			trail->trailKind = 5;
			ObjectTrail_InitIonPulseEmitter(trail);
			break;

		case OBJ_WarheadRocket:
			trail = ObjectTrail_AllocEmitter();
			trail->trailKind = 7;
			ObjectTrail_InitRocketEmitter(trail);
			break;

		default:
			break;
	}

	if (trail != NULL) {
		trail->objectIndex = objectIdx;
		trail->lastUpdateTime = g_gameTime;
		trail->pointCount = 0;
		trail->next = g_objRenderState[objectIdx].trailHead;
		g_objRenderState[objectIdx].trailHead = trail;
	}

	return trail;
}

// FUNCTION: XWA 0x4CBBF0
void ObjectTrail_FreeEmittersForObject(uint16_t objectIdx) {
	ObjectTrailEmitter* emitter;

	emitter = g_objRenderState[objectIdx].trailHead;
	while (emitter != NULL) {
		ObjectTrailEmitter* nextEmitter;
		ObjectTrailPoint* point;

		point = emitter->pointHead;
		nextEmitter = emitter->next;
		while (point != NULL) {
			ObjectTrailPoint* nextPoint;

			nextPoint = point->next;
			ObjectTrail_FreePoint(point);
			point = nextPoint;
		}

		emitter->pointCount = 0;
		emitter->pointHead = NULL;
		ObjectTrail_FreeEmitter(emitter);
		emitter = nextEmitter;
	}

	g_objRenderState[objectIdx].trailHead = NULL;
}

// FUNCTION: XWA 0x4CBA40
void ObjectTrail_Update(ObjectTrailEmitter* trail) {
	MobileObject* mobj;
	ObjectTrailPoint* point;
	ObjectTrailPoint* oldHead;
	float ageDelta;
	int offsetX;
	int offsetY;
	int offsetZ;

	mobj = g_objectTable[trail->objectIndex].mobj;
	point = ObjectTrail_AllocPoint();
	++trail->pointCount;

	offsetX = Xwa_Q15Mul(mobj->moveX, trail->forwardOffset);
	offsetY = Xwa_Q15Mul(mobj->moveY, trail->forwardOffset);
	offsetZ = Xwa_Q15Mul(mobj->moveZ, trail->forwardOffset);

	point->world.x = (float)(offsetX + g_objectTable[trail->objectIndex].world_x);
	point->world.y = (float)(offsetY + g_objectTable[trail->objectIndex].world_y);
	point->world.z = (float)(offsetZ + g_objectTable[trail->objectIndex].world_z);
#ifdef XWA_MODERN
	point->preciseWorld[0] = offsetX + g_objectTable[trail->objectIndex].world_x;
	point->preciseWorld[1] = offsetY + g_objectTable[trail->objectIndex].world_y;
	point->preciseWorld[2] = offsetZ + g_objectTable[trail->objectIndex].world_z;
#endif
	point->spawnTime = g_gameTime;
	point->ageFade = 0.0f;

	oldHead = trail->pointHead;
	trail->pointHead = point;
	trail->animTicksAccum += (uint16_t)g_elapsedTicks;
	point->next = oldHead;
	point->texV = 0.0f;

	ageDelta = (float)(g_gameTime - trail->lastUpdateTime) * trail->ageRate;
	trail->lastUpdateTime = g_gameTime;

	point = oldHead;
	while (point != NULL) {
		int spawnTime;

		spawnTime = point->spawnTime;
		point->ageFade += ageDelta;
		point->texV = (float)(g_gameTime - spawnTime) * trail->texVRate;

		if (g_gameTime - spawnTime > trail->lifetimeTicks) {
			ObjectTrailPoint** tailLink;

			tailLink = &point->next;
			point = point->next;
			*tailLink = NULL;
			if (point == NULL) {
				return;
			}

			do {
				ObjectTrailPoint* next;

				next = point->next;
				++trail->pointCount;
				ObjectTrail_FreePoint(point);
				point = next;
			} while (point != NULL);
		} else {
			point = point->next;
		}
	}
}

// FUNCTION: XWA 0x4CB7B0
void ObjectTrail_RenderObjectTrails(void) {
	uint32_t objectIdx;

	objectIdx = g_activeRegionObjectSlotStart;
	if (objectIdx < g_regionStaticObjectSlotEnd) {
		do {
			ObjectTrailEmitter* trail;

			trail = g_objRenderState[objectIdx].trailHead;
			if (trail != NULL) {
				uint16_t objectType;

				objectType = g_objectTable[objectIdx].objectType;
				if (objectType != 0 && objectType >= OBJ_LaserRebel && objectType <= OBJ_NoAsset_307) {
					trail = g_objRenderState[objectIdx].trailHead;
					while (trail != NULL) {
						ObjectTrail_Update(trail);
						RenderBillboard_DrawObjectTrail(trail);
						trail = trail->next;
					}
				} else {
					// Inlined ObjectTrail_FreeEmittersForObject((uint16_t)objectIdx).
					unsigned int freeIdx;
					ObjectTrailEmitter* emitter;

					freeIdx = (uint16_t)objectIdx;
					emitter = g_objRenderState[freeIdx].trailHead;
					while (emitter != NULL) {
						ObjectTrailPoint* point;
						ObjectTrailEmitter* nextEmitter;

						point = emitter->pointHead;
						nextEmitter = emitter->next;
						while (point != NULL) {
							ObjectTrailPoint* nextPoint;

							nextPoint = point->next;
							ObjectTrail_FreePoint(point);
							point = nextPoint;
						}

						emitter->pointCount = 0;
						emitter->pointHead = NULL;
						ObjectTrail_FreeEmitter(emitter);
						emitter = nextEmitter;
					}

					g_objRenderState[freeIdx].trailHead = NULL;
				}
			}

			++objectIdx;
		} while (objectIdx < g_regionStaticObjectSlotEnd);
	}
}

// FUNCTION: XWA 0x4CB8C0
void ObjectTrail_DrawEmittersForObject(uint16_t objectIdx) {
	ObjectTrailEmitter* trail;

	trail = g_objRenderState[objectIdx].trailHead;
	while (trail != NULL) {
		RenderBillboard_DrawObjectTrail(trail);
		trail = trail->next;
	}
}

// FUNCTION: XWA 0x4CC7E0
RenderBatch* RenderBatch_Alloc(void) {
	RenderBatch* batch;
	RenderBatch** nextLink;

	batch = g_renderBatchFreeList;
	if (batch == NULL) {
		int i;

		g_renderBatchFreeList = (RenderBatch*)Memory_AllocTagged(
			RENDER_DATAPOOL_TAG, sizeof(RenderBatch) * RENDER_BATCH_BLOCK_CAPACITY);
		RenderDataPool_RegisterAllocation(g_renderBatchFreeList);
		g_renderBatchPoolSize += RENDER_BATCH_BLOCK_CAPACITY;

		for (i = 0; i < RENDER_BATCH_BLOCK_CAPACITY; ++i) {
			g_renderBatchFreeList[i].next = &g_renderBatchFreeList[i + 1];
		}
		g_renderBatchFreeList[RENDER_BATCH_BLOCK_CAPACITY - 1].next = NULL;
		batch = g_renderBatchFreeList;
	}

	nextLink = &g_renderBatchFreeList->next;
	batch = g_renderBatchFreeList;
	g_renderBatchFreeList = *nextLink;
	*nextLink = NULL;
	++g_renderBatchInUse;
	batch->vertexCount = 0;
	batch->triCount = 0;
	return batch;
}

// FUNCTION: XWA 0x4CC8A0
void RenderBatch_Free(RenderBatch* batch) {
	batch->next = g_renderBatchFreeList;
	g_renderBatchFreeList = batch;
	--g_renderBatchInUse;
}

// FUNCTION: XWA 0x4CC130
void RenderBatch_FreeDataPools(void) {
	while (g_renderBatchBlocks != NULL) {
		RenderBatchPoolBlock* next;

		next = g_renderBatchBlocks->next;
		Memory_FreeTagged(RENDER_DATAPOOL_TAG, g_renderBatchBlocks->allocation);
		g_renderBatchBlocks->allocation = NULL;
		Memory_FreeTagged(RENDER_DATAPOOL_TAG, g_renderBatchBlocks);
		g_renderBatchBlocks = next;
	}

	g_engineGlowKnockoutFreeList = NULL;
	g_objectTrailFreeEmitters = NULL;
	g_objectTrailFreePoints = NULL;
	g_renderBatchFreeList = NULL;
}

void RenderBatch_FreeDataPoolsThunk(void) { RenderBatch_FreeDataPools(); }

// FUNCTION: XWA 0x4CC1A0
EngineGlowKnockoutMark* EngineGlow_AllocKnockoutMark(void) {
	EngineGlowKnockoutMark* mark;
	EngineGlowKnockoutMark** nextLink;

	mark = g_engineGlowKnockoutFreeList;
	if (mark == NULL) {
		int i;

		g_engineGlowKnockoutFreeList = (EngineGlowKnockoutMark*)Memory_AllocTagged(
			RENDER_DATAPOOL_TAG, sizeof(EngineGlowKnockoutMark) * ENGINE_GLOW_KNOCKOUT_BLOCK_CAPACITY);
		RenderDataPool_RegisterAllocation(g_engineGlowKnockoutFreeList);
		g_engineGlowKnockoutPoolCapacity += ENGINE_GLOW_KNOCKOUT_BLOCK_CAPACITY;

		for (i = 0; i < ENGINE_GLOW_KNOCKOUT_BLOCK_CAPACITY; ++i) {
			g_engineGlowKnockoutFreeList[i].next = &g_engineGlowKnockoutFreeList[i + 1];
		}
		g_engineGlowKnockoutFreeList[ENGINE_GLOW_KNOCKOUT_BLOCK_CAPACITY - 1].next = NULL;
		mark = g_engineGlowKnockoutFreeList;
	}

	nextLink = &g_engineGlowKnockoutFreeList->next;
	mark = g_engineGlowKnockoutFreeList;
	g_engineGlowKnockoutFreeList = *nextLink;
	mark->blastMark = NULL;
	*nextLink = NULL;
	++g_engineGlowKnockoutActiveCount;
	return mark;
}

// FUNCTION: XWA 0x4CC240
void EngineGlow_FreeKnockoutMark(EngineGlowKnockoutMark* mark) {
	mark->next = g_engineGlowKnockoutFreeList;
	g_engineGlowKnockoutFreeList = mark;
	--g_engineGlowKnockoutActiveCount;
}

// FUNCTION: XWA 0x4CC260
void EngineGlow_FreeKnockoutMarkList(EngineGlowKnockoutMark* markList) {
	EngineGlowKnockoutMark* mark;

	mark = markList;
	while (mark != NULL) {
		EngineGlowKnockoutMark* next;

		next = mark->next;
		mark->next = g_engineGlowKnockoutFreeList;
		g_engineGlowKnockoutFreeList = mark;
		--g_engineGlowKnockoutActiveCount;
		mark = next;
	}
}
