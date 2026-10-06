#ifndef XWA_RUNTIME_MODELS_OPT_SYNTH_H
#define XWA_RUNTIME_MODELS_OPT_SYNTH_H

/* Modern port: builds an OPT payload image for a GLB replacement model
 * (remaster/models/<key>.glb). The image is what OptModel_LoadHandle would
 * have read from an OPT file, except that each mesh's LOD 0 FaceData is a
 * stub with no faces; the faces, their BVH and the component tags stay in
 * XwaOptSynthData, keyed by the stub's payload offset. */

#include "aeron/asset/flight_model.h"
#include "aeron/asset/opt_payload.h"
#include "aeron/asset/opt_sim_model.h"
#include "aeron/collision/face_bvh.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct NativeOptimizedPolyObject;

/* The modern CraftData carries XWA_CRAFT_MESH_SLOT_COUNT components and keeps
   the animated-texture frame and wreck flame state in its own field (the
   original's componentState[49]), so a model may use every component. */
enum { XWA_OPT_SYNTH_MAX_COMPONENTS = XWA_CRAFT_MESH_SLOT_COUNT };

typedef struct XwaOptSynthMesh {
	uint32_t stubFaceDataOffset; /* payload offset of the stub FaceData node */
	uint32_t faceCount;
	const AeronOptFace* faces; /* OPT frame and units */
	const AeronFaceBvh* bvh;   /* OPT units; NULL without faces */
	uint32_t tagCount;
	const AeronFlightTag* tags;
} XwaOptSynthMesh;

typedef struct XwaOptSynthData {
	char key[64];
	uint64_t glbHash; /* XXH64 (seed 0) of the GLB bytes */
	uint32_t meshCount;
	XwaOptSynthMesh* meshes;     /* in component order, ascending stub offsets */
	AeronOptSimModel simulation; /* owns faces and BVHs */
	AeronFlightTag* tagStorage;  /* owns every mesh's tags */
	/* The GLB is parsed once: after the simulation data is built, the parsed
	   model keeps what the HD renderer needs until it has built its mesh
	   (xwa_runtime/models/render_model.h). */
	int hasRenderModel;
	AeronFlightModel renderModel;
	struct XwaOptSynthData* nextLive; /* loaded replacements, for XwaRenderModel_Find */
} XwaOptSynthData;

void OptSynth_Free(XwaOptSynthData* synth);

/* OptModel_LoadFileToHandle for a GLB replacement: builds the payload for
   `fileName`'s key and loads it into a temporary native handle, setting the
   load-buffer globals as the file path does. `*outVersion` is 5. */
uint16_t OptSynth_LoadToHandle(const char* fileName, int* outVersion);
/* Texture processing deletes byte ranges ahead of the stubs, so the offsets
   the synthesizer recorded are stale once OptModel_LoadHandle has processed
   the model. Records each mesh's final stub payload offset (nothing moves
   bytes afterwards). Returns 0 if the processed layout no longer has one
   FaceData stub per mesh in ascending order. */
int OptSynth_RekeyStubs(struct NativeOptimizedPolyObject* native);

/* Per-model queries; all are safe on stock models. */
int OptSynth_IsSynthesized(const OptimizedPolyObject* model);
const XwaOptSynthData* OptSynth_GetModelData(const OptimizedPolyObject* model);
/* Side data for the stub FaceData node `faceDataNode`, or NULL. */
const XwaOptSynthMesh* OptSynth_FindMeshForNode(const OptimizedPolyObject* model,
												const OptNode* faceDataNode);
/* First component (mesh ordinal - 1) carrying `tag`, or -1. */
int OptSynth_FindModelComponentByTag(const OptimizedPolyObject* model, const char* tag);

#ifdef __cplusplus
}
#endif

#endif
