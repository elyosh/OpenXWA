#ifndef XWA_ASSETS_OPT_SYNTH_H
#define XWA_ASSETS_OPT_SYNTH_H

/* Modern port: builds an OPT payload image for a GLB replacement model
 * (remaster/models/<key>.glb). The image is what OptModel_LoadHandle would
 * have read from an OPT file, except that each mesh's LOD 0 FaceData is a
 * stub with no faces; the faces, their BVH and the component tags stay in
 * XwaOptSynthData, keyed by the stub's payload offset. */

#include "aeron/asset/flight_model.h"
#include "aeron/asset/opt_payload.h"
#include "aeron/asset/opt_sim_model.h"
#include "aeron/collision/face_bvh.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CraftData.componentState[49] holds the animated-texture frame and wreck
   flame state, so a model may use components 0..48 only. */
enum { XWA_OPT_SYNTH_MAX_COMPONENTS = 49 };

typedef struct XwaOptSynthMesh {
	uint32_t stubFaceDataOffset; /* payload offset of the stub FaceData node */
	uint32_t faceCount;
	const AeronOptFace* faces; /* OPT frame and units */
	const AeronFaceBvh* bvh;   /* OPT units */
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
} XwaOptSynthData;

/* Builds the payload (the OPT bytes after the -version/size header) for
   `key`. On success the caller owns *outPayload (free()) and *outSynth
   (OptSynth_Free). Returns 0 and logs the reason on failure. */
int OptSynth_BuildPayload(const char* key, uint8_t** outPayload, int* outPayloadSize,
						  XwaOptSynthData** outSynth);
void OptSynth_Free(XwaOptSynthData* synth);

/* Mesh whose stub FaceData sits at `stubFaceDataOffset`, or NULL. */
const XwaOptSynthMesh* OptSynth_FindMeshByStubOffset(const XwaOptSynthData* synth,
													 uint32_t stubFaceDataOffset);
/* First component carrying `tag`, or -1. */
int OptSynth_FindComponentByTag(const XwaOptSynthData* synth, const char* tag);

#ifdef __cplusplus
}
#endif

#endif
