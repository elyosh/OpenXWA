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
/* Same lookup, also giving the XwaModelSource the mesh was built from:
 * XWA_MODEL_SOURCE_OPT for an in-memory OPT conversion, XWA_MODEL_SOURCE_GLB
 * for a replacement GLB. */
AeronSceneMesh* XwaRemasterShip_MeshForNameWithSource(const char* name, uint8_t* out_source);
float XwaRemasterShip_OptProjectileEmissiveStrength(void);
void XwaRemasterShip_Shutdown(void);
void XwaRemasterShip_Configure(float opt_smooth_angle_degrees, float opt_emissive_strength,
							   float opt_projectile_emissive_strength, float engine_emissive_strength);

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
int XwaRemasterShip_BuildDebrisMeshTable(const AeronSceneMesh* mesh, const XwaFlightObject* f,
										 AeronSceneMeshTable* out);

/* Mesh table for the cockpit model. Pilot cockpits inherit articulation
 * and component visibility from the anchor craft. Turret cockpits retain
 * that visibility and B-Wing root compensation, but replace per-root craft
 * rotation with the captured seat aim used by the classic cockpit walk. */
int XwaRemasterShip_BuildCockpitMeshTable(const AeronSceneMesh* mesh, const XwaCockpit* c,
										  const XwaFlightObject* anchor, AeronSceneMeshTable* out);
int XwaRemasterShip_BuildPreviousCockpitMeshTable(
	const AeronSceneMesh* mesh, const XwaCockpit* current_cockpit,
	const XwaFlightObject* current_anchor, const XwaCockpit* previous_cockpit,
	const XwaFlightObject* previous_anchor, AeronSceneMeshTable* out);

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
	/* Debug material override (debug inspector only): every PBR material
	 * shaded with this metallic and perceptual roughness. */
	float debug_material_override; /* 0/1 */
	float debug_metallic;
	float debug_roughness;
} XwaShipPbrTuning;

void XwaRemasterShip_GetPbrTuning(XwaShipPbrTuning* out);
void XwaRemasterShip_SetPbrTuning(const XwaShipPbrTuning* in);
/* Installs the required remaster/config.yaml profile and makes it the debug reset target. */
void XwaRemasterShip_ConfigurePbrTuning(const XwaShipPbrTuning* in);
void XwaRemasterShip_GetPbrTuningDefault(XwaShipPbrTuning* out);

/* Dark back-face fill behind hairline cracks (AeronScene_SetBackfaceFill).
 * On by default; the debug inspector toggles it for comparison and
 * profiling. */
int  XwaRemasterShip_BackfaceFill(void);
void XwaRemasterShip_SetBackfaceFill(int enabled);

/* Per-frame SSAO inputs for XwaRemasterShip_SetPbrEnv (mirrors TIE's
 * FlightPbrAoParams). NULL disables AO (intensity 0): the FS leaves
 * the ambient term untouched and the bound AO texture reads as 1. */
typedef struct XwaShipAoParams {
	float intensity;  /* 0 = off; else ambient-occlusion lerp weight */
	float power;      /* pow(ao, power) contrast — 1 = linear */
	float rt_w, rt_h; /* scene RT dimensions (screen-UV denominator) */
	float direct;     /* AO weight on direct diffuse (0 = ambient-only) */
} XwaShipAoParams;

/* One punctual-light source in the classic SOURCE law's terms (world
 * space; linear colour with the classic intensity premultiplied). The
 * scene evaluates it as a physical light (XwaRemasterShip_ScenePointLight). */
typedef struct XwaShipPointLight {
	float pos[3];
	float color[3]; /* linear colour x classic intensity */
} XwaShipPointLight;
typedef struct XwaShipPointLightTuning {
	float spec_weight; /* 0 disables the spec lobe */
} XwaShipPointLightTuning;

/* A classic-intensity source as a physical scene light: an inverse-square
 * sphere of XWA_SHIP_POINT_LIGHT_RADIUS_M, as bright as the classic law
 * (0.5 * intensity / d) at XWA_SHIP_POINT_LIGHT_MATCH_DISTANCE_M, so
 * brighter closer and dimmer farther. `intensity_scale` multiplies it;
 * `range_scale` multiplies the distance where it falls under 1/1000 of
 * a unit of illuminance. Returns zero for a light that emits nothing. */
#define XWA_SHIP_POINT_LIGHT_MATCH_DISTANCE_M 5.0f
#define XWA_SHIP_POINT_LIGHT_RADIUS_M 0.25f
int XwaRemasterShip_ScenePointLight(const XwaShipPointLight* light, float intensity_scale, float range_scale,
									AeronSceneLight* out);

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

/* Fill every lobe with the current process-wide linear ambient tuning. */
void XwaRemasterShip_GetAmbientCube(XwaShipAmbientCube* out);

/* Optional detailed diffuse environment (Lambert irradiance / pi, linear
 * HDR) indexed in the supplied world-space orthonormal basis; bound with
 * AeronScene_SetPbrEnvironment. */
typedef struct XwaShipEnvironmentMap {
	AeronTexture* texture;
	AeronSampler* sampler;
	float right[3];
	float up[3];
	float forward[3];
	float strength;
} XwaShipEnvironmentMap;

/* Classic engine-glow POINT LIGHTS (the modelIndex branch of
 * FlightLight_AppendScenePointLightForObject): every glow with raw
 * dims.x/y over 2000 emits a light at its (articulated) anchor —
 * intensity dim.z x engineScale x 300, color = core color (the classic
 * range, engineScale x 16384, only selected lights); engineScale is the power margin x output scale
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
 * The key light is lights[key_index] when key_index is in range, otherwise
 * SelectKeyDirectionalLight's choice. Directional light
 * colors are decoded to linear and scaled by their intensity. Punctual
 * records are submitted separately through AeronScene_AddLight;
 * `point_tuning` controls their evaluation. `ambient_cube` optionally
 * replaces the uniform ambient fill; `environment_map`, when given,
 * selects the scene's diffuse environment (otherwise the scene's selection,
 * such as an authored package, is left as is). */
void XwaRemasterShip_SetPbrEnv(AeronScene3D* scene, const XwaDirLight* lights, uint32_t light_count,
							   int32_t key_index, const float cam_rows[9], const float cam_pos[3],
							   const XwaShipAoParams* ao, const XwaShipPointLightTuning* point_tuning,
							   const XwaShipAmbientCube* ambient_cube,
							   const XwaShipEnvironmentMap* environment_map);

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
 * over the HD mesh's flight-model engine glows (position/axes/dims/colors
 * per emitter): the rect / view-aligned-round corner build, the
 * look-axis depth extrusion, the viewZ / projected-size culls, and
 * the classic fan colors (center = core, rim = outer, sampling `tex`
 * with UVs remapped into its sub-rect).
 *
 * `transform` is the instance's model->space matrix (fl_model_matrix
 * layout) with uniform scale `model_scale`; `table` applies rotary-
 * mesh articulation to emitters on rotated meshes (NULL = none);
 * `knockout_mask` (8 words; NULL = none) bit N (word N >> 5) skips
 * destroyed emitter N; `scale` is the
 * classic intensity scale (EngineGlowScale / the preview's fixed
 * ~0.6 flickered base; <= 0 skips everything). `crows`/`cam_pos`
 * convert space->view for the view-dependent parts (NULL/NULL =
 * identity camera, space == eye — preview PiP). */
void XwaRemasterShip_SubmitEngineGlows(AeronScene3D* scene, const AeronSceneMesh* mesh,
									   const float transform[16], float model_scale,
									   const AeronSceneMeshTable* table, const uint32_t* knockout_mask,
									   float scale, const float crows[9], const float cam_pos[3],
									   const XwaAssetRef* tex);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_SHIP_H */
