#ifndef XWA_REMASTER_SHIP_H
#define XWA_REMASTER_SHIP_H

/*
 * Shared craft-rendering policy for the HD drivers — used by BOTH the
 * frontend model-preview PiP and the flight driver. Anything
 * about how an XWA ship is drawn beyond the raw mesh instance belongs
 * here, not in the per-driver files, including engine glows,
 * per-component articulation and visibility, and the PBR environment.
 */

#include "aeron/render.h"
#include "aeron/scene/billboard.h"
#include "aeron/scene/mesh.h"
#include "aeron/scene/scene3d.h"
#include "xwa_remaster/assets.h"
#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mirror the classic engine's live processed-OPT set published in the
 * snapshot. Reconciliation performs every disk read, conversion and GPU
 * upload before scene rendering; MeshForName is lookup-only. */
int XwaRemasterShip_AssetsNeedSync(const XwaSnapshot* snapshot);
typedef enum XwaRemasterShipSyncResult {
	XWA_REMASTER_SHIP_SYNC_FAILED = -1,
	XWA_REMASTER_SHIP_SYNC_MORE = 0,
	XWA_REMASTER_SHIP_SYNC_COMPLETE = 1
} XwaRemasterShipSyncResult;
XwaRemasterShipSyncResult XwaRemasterShip_SyncAssets(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot,
													 uint64_t byte_budget, uint32_t copy_budget);
/* Commit a successfully submitted synchronization batch. A complete target
 * generation is published atomically; an incomplete one remains staged for
 * the next host frame. */
void XwaRemasterShip_CommitSyncBatch(void);
AeronSceneMesh* XwaRemasterShip_MeshForName(const char* name);
/* Same lookup with source metadata; out_runtime_opt is 1 only for an
 * in-memory OPT conversion, never for an authored GLB. */
AeronSceneMesh* XwaRemasterShip_MeshForNameWithSource(const char* name, int* out_runtime_opt);
float XwaRemasterShip_OptProjectileEmissiveStrength(void);
void XwaRemasterShip_Shutdown(void);
void XwaRemasterShip_Configure(float opt_smooth_angle_degrees, float opt_emissive_strength,
							   float opt_projectile_emissive_strength, float engine_emissive_strength,
							   int force_opt_models);

/* Projectiles render as ordinary scene instances (roll-aligned by the
 * flight driver). Runtime-converted OPTs receive a genus-driven
 * base-color emission override; authored GLBs retain their materials. */

/* Build the per-instance mesh table (articulation + visibility lanes)
 * from the captured craft state: rotary meshes rotate about their
 * cooked OPT pivot/axis by the classic angle meshRotation[slot] *
 * 2pi/256 (RenderScene_DrawObjectModelHardware); blown-off components
 * (componentState[slot] != 0 — the classic node-walk skip) hide. B-Wings
 * additionally rotate every mesh root by the Bridge slot rotation before
 * applying the root-local transform.
 * Returns 1 when any slot rotates or hides (callers skip the table
 * bind otherwise). */
int XwaRemasterShip_BuildMeshTable(const AeronSceneMesh* mesh, const XwaFlightObject* f,
								   AeronSceneMeshTable* out);

/* Build the previous articulation table required for motion vectors when
 * rotary state changed between matched snapshots. Returns 0 when the current
 * table is also valid as history; otherwise returns 1 and initializes out,
 * including the identity-table case when rotation returned to zero. */
int XwaRemasterShip_BuildPreviousMeshTable(const AeronSceneMesh* mesh, const XwaFlightObject* current,
										   const XwaFlightObject* previous, AeronSceneMeshTable* out);

/* Mesh table for a DEBRIS CHUNK (OBJ_NoAsset_222 — the classic
 * RenderScene_DrawNoAssetSourceModel draws the SOURCE craft's model
 * with ONLY the detached component's mesh root, typeSpecificByte[0]>>1
 * in mesh-root ordinal order, no componentState gating): every slot
 * hidden except the component, spun by the object's spin state about
 * the component focus point (renderOffset). Returns 0 only on a bad
 * component index — callers must then skip the instance entirely
 * (never draw the whole source model). */
int XwaRemasterShip_BuildDebrisMeshTable(const XwaFlightObject* f, AeronSceneMeshTable* out);

/* Mesh table for the TURRET cockpit: classic rotates the model's
 * rotary meshes by the captured seat aim — gun/launcher types by
 * aim_angle_a, the beam type by -aim_angle_b (Q16, about each mesh's
 * own rotation node). Returns 1 when any slot rotates. */
int XwaRemasterShip_BuildCockpitMeshTable(const AeronSceneMesh* mesh, const XwaCockpit* c,
										  AeronSceneMeshTable* out);

/* Process-wide PBR tuning globals consumed by every lit
 * XwaRemasterShip_SetPbrEnv submit (stored in the combined PbrLightFS
 * environment block). The required remaster/config.yaml profile defines the HD
 * relight baseline. Mutated by the debug inspector; not persisted. */
typedef struct XwaShipPbrTuning {
	float light_intensity;    /* directional sun weight */
	float global_spec_mul;    /* Cook-Torrance specular multiplier */
	float light_wrap;         /* 0 = Lambert .. 1 = Half-Lambert */
	float spec_geom_adapt;    /* 0/1: blend spec normal toward the face */
	float debug_isolate_term; /* PbrIsolateMode (0 = off) */
	float ambient[3];         /* ambient cube fill (linear RGB) */
} XwaShipPbrTuning;

void XwaRemasterShip_GetPbrTuning(XwaShipPbrTuning* out);
void XwaRemasterShip_SetPbrTuning(const XwaShipPbrTuning* in);
/* Installs the required remaster/config.yaml profile and makes it the debug reset target. */
void XwaRemasterShip_ConfigurePbrTuning(const XwaShipPbrTuning* in);
void XwaRemasterShip_GetPbrTuningDefault(XwaShipPbrTuning* out);

/* Per-frame SSAO inputs for XwaRemasterShip_SetPbrEnv (mirrors TIE's
 * FlightPbrAoParams). NULL disables AO (intensity 0): the FS leaves
 * the ambient term untouched and the bound AO texture reads as 1. */
typedef struct XwaShipAoParams {
	float intensity;  /* 0 = off; else ambient-occlusion lerp weight */
	float power;      /* pow(ao, power) contrast — 1 = linear */
	float rt_w, rt_h; /* scene RT dimensions (screen-UV denominator) */
	float direct;     /* AO weight on direct diffuse (0 = ambient-only) */
} XwaShipAoParams;

/* One punctual-light candidate for the PBR env (world space, HDR
 * color with the source intensity premultiplied, range = attenuation
 * window in world units). Evaluated per fragment by the scene PBR FS
 * (windowed inverse-square, Lambert + Cook-Torrance spec). */
typedef struct XwaShipPointLight {
	float pos[3];
	float range;
	float color[3]; /* linear HDR */
} XwaShipPointLight;
typedef struct XwaShipPointLightTuning {
	float min_distance; /* near clamp on the 0.5/d law, world units */
	float spec_weight;  /* 0 disables the spec lobe */
	float diffuse_wrap; /* 0 = Lambert, 1 = half-Lambert */
	float contrib_cap;  /* per-light lit-color cap (classic saturation) */
} XwaShipPointLightTuning;

/* Optional per-frame linear ambient cube. NULL selects the process-wide
 * uniform ambient value from XwaShipPbrTuning. */
typedef struct XwaShipAmbientCube {
	float pos_x[3];
	float neg_x[3];
	float pos_y[3];
	float neg_y[3];
	float pos_z[3];
	float neg_z[3];
} XwaShipAmbientCube;

/* Classic engine-glow POINT LIGHTS (the modelIndex branch of
 * FlightLight_AppendScenePointLightForObject): every glow with raw
 * dims.x/y over 2000 emits a light at its (articulated) anchor —
 * intensity dim.z x engineScale x 300, range engineScale x 16384,
 * color = core color; engineScale is the power margin x output scale
 * (no throttle/flicker — those belong to the glow SIZE law). Color
 * carries the classic intensity premultiplied (linear HDR); the
 * caller applies its global scale knobs. Returns lights appended. */
uint32_t XwaRemasterShip_CollectEngineGlowPointLights(const AeronSceneMesh* mesh, const float transform[16],
													  const AeronSceneMeshTable* table,
													  const XwaFlightObject* f, XwaShipPointLight* out,
													  uint32_t max, uint32_t* dropped);

/* Select the semantic key directional: the brightest active sun
 * backdrop when present, otherwise the greatest linear luminance. */
const XwaDirLight* XwaRemasterShip_SelectKeyDirectionalLight(const XwaDirLight* lights, uint32_t light_count);

/* Queue the shared PBR fragment environment (PbrLightFS b1, including
 * lighting tuning) as a scene frame uniform, built from the snapshot's
 * captured WORLD-space dir_lights
 * channel. The FS lights in the space the instances were submitted
 * in, so the env must match that convention:
 *   - world-space instances (flight driver): cam_rows = NULL (light
 *     dirs stay world-space), cam_pos = world camera position;
 *   - eye-space instances (preview PiP, identity camera): cam_rows =
 *     the view's g_camMat world->eye rows (lights rotate into eye
 *     space), cam_pos = NULL (camera at the origin).
 * `ao` carries the per-frame SSAO inputs the FS folds into the
 * ambient (and optionally direct-diffuse) term; NULL = AO off.
 * The key light follows SelectKeyDirectionalLight. Directional light
 * colors are decoded to linear and scaled by their intensity. Punctual
 * records are submitted separately through AeronScene_AddLight;
 * `point_tuning` controls their evaluation. `ambient_cube` optionally
 * replaces the uniform ambient fill for scene-specific environments. */
void XwaRemasterShip_SetPbrEnv(AeronScene3D* scene, const XwaDirLight* lights, uint32_t light_count,
							   const float cam_rows[9], const float cam_pos[3], const XwaShipAoParams* ao,
							   const XwaShipPointLightTuning* point_tuning,
							   const XwaShipAmbientCube* ambient_cube);

/* Classic per-object engine-glow intensity scale — the float mirror
 * of EngineGlow_RenderObjectGlows' scale derivation from the captured
 * eg_* craft state: the power-margin formula (16 - redirects,
 * engineOutputScale, throttle, min 0.35) for normal craft, the
 * speed-boost formula for objectKind 5/6. Includes the classic ~6%
 * flicker (driver-side randomness — the classic consumed its render
 * RNG here, which read-only capture must not). Returns 0 when the
 * classic draws no glows (no craft / no engine output / subsystems
 * dead). */
float XwaRemasterShip_EngineGlowScale(const XwaFlightObject* f);

/* Derive + queue one model instance's engine glows as batched OVERLAY
 * scene billboards — the float mirror of EngineGlow_BuildProjectedQuad
 * over the cooked glb's engine-glow extras (position/axes/dims/colors
 * per emitter): the rect / view-aligned-round corner build, the
 * look-axis depth extrusion, the viewZ / projected-size culls, and
 * the classic fan colors (center = core, rim = outer, sampling `tex`
 * with UVs remapped into its sub-rect).
 *
 * `transform` is the instance's model->space matrix (fl_model_matrix
 * layout) with uniform scale `model_scale`; `table` applies rotary-
 * mesh articulation to emitters on rotated meshes (NULL = none);
 * `knockout_mask` bit N skips destroyed emitter N; `scale` is the
 * classic intensity scale (EngineGlowScale / the preview's fixed
 * ~0.6 flickered base; <= 0 skips everything). `crows`/`cam_pos`
 * convert space->view for the view-dependent parts (NULL/NULL =
 * identity camera, space == eye — preview PiP). */
void XwaRemasterShip_SubmitEngineGlows(AeronScene3D* scene, const AeronSceneMesh* mesh,
									   const float transform[16], float model_scale,
									   const AeronSceneMeshTable* table, uint32_t knockout_mask, float scale,
									   const float crows[9], const float cam_pos[3], const XwaAssetRef* tex);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_SHIP_H */
