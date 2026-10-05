#include "xwa_runtime/models/opt_synth.h"

#include "aeron/aeron.h"
#include "aeron/collision/hash.h"
#include "aeron/log.h"
#include "aeron/vfs.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/opt_model_native.h"
#include "xwa/util/memory.h"
#include "xwa_runtime/models/model_source.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. Compiled without
   floating-point contraction so derived simulation data is identical on
   every platform. */

enum {
	OPT_SYNTH_MAX_ENGINE_GLOWS = 16,
	OPT_SYNTH_MAX_GUNNER_HARDPOINTS = 8,
	OPT_SYNTH_HARDPOINT_GUNNER = 19,
	OPT_SYNTH_HARDPOINT_ACC_END = 24,
	OPT_SYNTH_VERTEX_SAMPLE_COUNT = 256,
};

/* Cockpit-class models are hit-tested by the loose FaceData walk: up to nine
   linear sweeps over every face per player hit, accepting near misses within
   about 50 / edge length OPT units of a face edge. Stock cockpits (the twelve
   *Cockpit.opt plus CorellianTransportGunner.opt) have at most 629 faces and
   a smallest per-component mean edge of 1.1 OPT units, so warn only beyond
   what the game shipped with. */
#define OPT_SYNTH_COCKPIT_MAX_FACES 5000u
#define OPT_SYNTH_COCKPIT_MIN_MEAN_EDGE 1.0

/* Tags that OpenXWA reads as a single component. */
static const char* const s_uniqueTags[] = { "crew.side", "crew.forward" };

static int OptSynth_Fail(const char* key, const char* reason) {
	Aeron_LogError("xwa.models", "replacement '%s' rejected: %s", key, reason);
	return 0;
}

static int OptSynth_IsLaserHardpoint(int32_t type) {
	return type == 0 || (type >= 1 && type <= 6) || (type >= 9 && type <= 11) || type == 18;
}

static int OptSynth_ValidateModel(const char* key, const AeronFlightModel* model) {
	char reason[160];
	uint32_t gunnerCount;
	uint32_t component;
	size_t tag;

	if (model->component_count == 0 || model->component_count > XWA_OPT_SYNTH_MAX_COMPONENTS) {
		snprintf(reason, sizeof reason, "%u components (1..%d allowed)", model->component_count,
				 XWA_OPT_SYNTH_MAX_COMPONENTS);
		return OptSynth_Fail(key, reason);
	}
	for (tag = 0; tag < sizeof s_uniqueTags / sizeof s_uniqueTags[0]; ++tag) {
		uint32_t carriers = 0;

		for (component = 0; component < model->component_count; ++component) {
			const AeronFlightComponent* item = &model->components[component];
			uint32_t index;

			for (index = 0; index < item->tag_count; ++index) {
				carriers += strcmp(item->tags[index].name, s_uniqueTags[tag]) == 0;
			}
		}
		if (carriers > 1) {
			snprintf(reason, sizeof reason, "tag '%s' is on %u components", s_uniqueTags[tag], carriers);
			return OptSynth_Fail(key, reason);
		}
	}
	if (model->engine_glow_count > OPT_SYNTH_MAX_ENGINE_GLOWS) {
		snprintf(reason, sizeof reason, "%u engine glows (at most %d)", model->engine_glow_count,
				 OPT_SYNTH_MAX_ENGINE_GLOWS);
		return OptSynth_Fail(key, reason);
	}
	gunnerCount = 0;
	for (component = 0; component < model->component_count; ++component) {
		const AeronFlightComponent* item = &model->components[component];
		uint32_t index;

		for (index = 0; index < item->hardpoint_count; ++index) {
			gunnerCount += item->hardpoints[index].type == OPT_SYNTH_HARDPOINT_GUNNER ||
						   item->hardpoints[index].type == OPT_SYNTH_HARDPOINT_ACC_END;
		}
	}
	if (gunnerCount > OPT_SYNTH_MAX_GUNNER_HARDPOINTS) {
		snprintf(reason, sizeof reason, "%u Gunner/AccEnd hardpoints (at most %d)", gunnerCount,
				 OPT_SYNTH_MAX_GUNNER_HARDPOINTS);
		return OptSynth_Fail(key, reason);
	}

	if (XwaModelSource_KeyRequiresFloatHardpoints(key)) {
		int hasLaser = 0;
		int hasMainHull = 0;

		for (component = 0; component < model->component_count; ++component) {
			const AeronFlightComponent* item = &model->components[component];
			uint32_t index;

			hasMainHull |= item->mesh_type == MESH_MainHull;
			for (index = 0; index < item->hardpoint_count; ++index) {
				hasLaser |= OptSynth_IsLaserHardpoint(item->hardpoints[index].type);
			}
		}
		if (!hasLaser) {
			return OptSynth_Fail(key, "its model type needs a laser hardpoint (type 0-6, 9-11 or 18)");
		}
		if (!hasMainHull) {
			return OptSynth_Fail(key, "its model type needs a MainHull component");
		}
	}
	return 1;
}

static int OptSynth_Finite(const float* values, size_t count) {
	size_t index;

	for (index = 0; index < count; ++index) {
		if (!isfinite(values[index])) {
			return 0;
		}
	}
	return 1;
}

/* Checks the derived data in OPT units, where large metre values could have
   overflowed. */
static int OptSynth_ValidateSimulation(const char* key, const AeronOptSimModel* simulation) {
	uint32_t meshIndex;

	for (meshIndex = 0; meshIndex < simulation->mesh_count; ++meshIndex) {
		const AeronOptSimMesh* mesh = &simulation->meshes[meshIndex];
		const AeronOptMeshDescriptor* descriptor = &mesh->descriptor;
		const AeronOptRotScale* rotScale = &mesh->rot_scale;
		char reason[96];
		uint32_t index;
		int finite;

		finite = OptSynth_Finite(&mesh->vertices[0][0], ((size_t)mesh->vertex_count + 2) * 3) &&
				 OptSynth_Finite(&mesh->normals[0][0], (size_t)mesh->vertex_count * 3) &&
				 OptSynth_Finite(descriptor->span, 3) && OptSynth_Finite(descriptor->center, 3) &&
				 OptSynth_Finite(descriptor->box_min, 3) && OptSynth_Finite(descriptor->box_max, 3) &&
				 OptSynth_Finite(descriptor->target, 3) && OptSynth_Finite(rotScale->pivot, 3) &&
				 OptSynth_Finite(rotScale->rotation_axis, 3) &&
				 OptSynth_Finite(rotScale->direction_axis, 3) && OptSynth_Finite(rotScale->up_axis, 3);
		for (index = 0; finite && index < mesh->face_count; ++index) {
			finite = OptSynth_Finite(mesh->faces[index].normal, 3);
		}
		for (index = 0; finite && index < mesh->hardpoint_count; ++index) {
			finite = OptSynth_Finite(mesh->hardpoints[index].position, 3);
		}
		for (index = 0; finite && index < mesh->glow_count; ++index) {
			const AeronOptEngineGlow* glow = &mesh->glows[index];

			finite = OptSynth_Finite(glow->dimensions, 3) && OptSynth_Finite(glow->position, 3) &&
					 OptSynth_Finite(glow->look, 3) && OptSynth_Finite(glow->up, 3) &&
					 OptSynth_Finite(glow->right, 3);
		}
		if (!finite) {
			snprintf(reason, sizeof reason, "component %u has a non-finite value", meshIndex);
			return OptSynth_Fail(key, reason);
		}
	}
	return 1;
}

/* Warns once when a cockpit-class model is denser than any stock cockpit:
   more faces makes the loose hit-spark sweeps slower, and shorter edges widen
   their near-miss tolerance. A collision mesh replaces the edge concern for
   its component. */
static void OptSynth_WarnDenseCockpit(const char* key, const AeronFlightModel* model) {
	uint32_t component;
	uint32_t faceTotal;
	double smallestMeanEdge;
	int smallestComponent;

	if (!XwaModelSource_IsCockpitClassKey(key)) {
		return;
	}
	faceTotal = 0;
	smallestMeanEdge = 0.0;
	smallestComponent = -1;
	for (component = 0; component < model->component_count; ++component) {
		const AeronFlightComponent* item = &model->components[component];
		const AeronFlightTopology* topology = &item->topology;
		double total;
		uint32_t face;

		faceTotal += topology->face_count;
		if (item->has_collision || topology->face_count == 0) {
			continue;
		}
		total = 0.0;
		for (face = 0; face < topology->face_count; ++face) {
			const uint32_t* indices = topology->faces[face].indices;
			int corner;

			for (corner = 0; corner < 3; ++corner) {
				const AeronFlightVec3* a = &topology->positions[indices[corner]];
				const AeronFlightVec3* b = &topology->positions[indices[(corner + 1) % 3]];
				const double dx = (double)a->x - (double)b->x;
				const double dy = (double)a->y - (double)b->y;
				const double dz = (double)a->z - (double)b->z;

				total += sqrt(dx * dx + dy * dy + dz * dz);
			}
		}
		total = total / (3.0 * topology->face_count) * AERON_OPT_SIM_UNITS_PER_METER;
		if (smallestComponent < 0 || total < smallestMeanEdge) {
			smallestMeanEdge = total;
			smallestComponent = (int)component;
		}
	}
	if (faceTotal > OPT_SYNTH_COCKPIT_MAX_FACES) {
		Aeron_LogWarn(
			"xwa.models",
			"replacement '%s': cockpit has %u faces (stock cockpits have at most 629); each player hit "
			"sweeps all of them up to nine times",
			key, faceTotal);
	}
	if (smallestComponent >= 0 && smallestMeanEdge < OPT_SYNTH_COCKPIT_MIN_MEAN_EDGE) {
		Aeron_LogWarn(
			"xwa.models",
			"replacement '%s': cockpit component %d has a mean edge of %.2f OPT units (stock cockpits "
			"stay above 1.1) and no collision mesh; hit sparks accept wide near misses",
			key, smallestComponent, smallestMeanEdge);
	}
}

static XwaOptSynthData* OptSynth_CreateData(const char* key, uint64_t glbHash, const AeronFlightModel* model,
											AeronOptSimModel* simulation, const uint32_t* stubOffsets) {
	XwaOptSynthData* synth;
	uint32_t tagTotal;
	uint32_t component;

	synth = (XwaOptSynthData*)calloc(1, sizeof *synth);
	if (synth == NULL) {
		return NULL;
	}
	tagTotal = 0;
	for (component = 0; component < model->component_count; ++component) {
		tagTotal += model->components[component].tag_count;
	}
	synth->meshes = (XwaOptSynthMesh*)calloc(simulation->mesh_count, sizeof *synth->meshes);
	synth->tagStorage = tagTotal != 0 ? (AeronFlightTag*)calloc(tagTotal, sizeof *synth->tagStorage) : NULL;
	if (synth->meshes == NULL || (tagTotal != 0 && synth->tagStorage == NULL)) {
		free(synth->meshes);
		free(synth->tagStorage);
		free(synth);
		return NULL;
	}
	snprintf(synth->key, sizeof synth->key, "%s", key);
	synth->glbHash = glbHash;
	synth->meshCount = simulation->mesh_count;
	synth->simulation = *simulation;
	memset(simulation, 0, sizeof *simulation);

	tagTotal = 0;
	for (component = 0; component < synth->meshCount; ++component) {
		const AeronFlightComponent* item = &model->components[component];
		const AeronOptSimMesh* source = &synth->simulation.meshes[component];
		XwaOptSynthMesh* mesh = &synth->meshes[component];

		mesh->stubFaceDataOffset = stubOffsets[component];
		mesh->faceCount = source->face_count;
		mesh->faces = source->faces;
		mesh->bvh = source->bvh;
		mesh->tagCount = item->tag_count;
		mesh->tags = item->tag_count != 0 ? &synth->tagStorage[tagTotal] : NULL;
		if (item->tag_count != 0) {
			memcpy(&synth->tagStorage[tagTotal], item->tags, item->tag_count * sizeof *item->tags);
		}
		tagTotal += item->tag_count;
	}
	return synth;
}

int OptSynth_BuildPayload(const char* key, uint8_t** outPayload, int* outPayloadSize,
						  XwaOptSynthData** outSynth) {
	char path[XWA_MODEL_SOURCE_KEY_MAX + 32];
	char error[256];
	uint8_t* glb;
	size_t glbSize;
	uint64_t glbHash;
	AeronFlightModel model;
	AeronOptSimModel simulation;
	AeronOptSimOptions simOptions;
	AeronOptImageOptions imageOptions;
	uint8_t* payload;
	int payloadSize;
	uint32_t* stubOffsets;
	XwaOptSynthData* synth;
	uint32_t vertexTotal;
	uint32_t faceTotal;
	uint32_t nodeTotal;
	int cooked;
	uint32_t meshIndex;

	*outPayload = NULL;
	*outPayloadSize = 0;
	*outSynth = NULL;

	snprintf(path, sizeof path, "remaster/models/%s.glb", key);
	glb = NULL;
	glbSize = 0;
	if (!AeronVfs_ReadAll(Aeron_GetVfs(), AERON_VFS_ROOT_ASSET, path, 0, &glb, &glbSize)) {
		return OptSynth_Fail(key, "cannot read the GLB");
	}
	glbHash = Aeron_Xxh64(glb, glbSize, 0);
	if (!Aeron_FlightModelBuildSemanticsMemory(glb, glbSize, path, &model)) {
		free(glb);
		return OptSynth_Fail(key, "invalid GLB or AERON_flight_model data");
	}
	free(glb);
	if (!OptSynth_ValidateModel(key, &model)) {
		Aeron_FlightModelFreeSemantics(&model);
		return 0;
	}

	simOptions.vertex_sample_count = OPT_SYNTH_VERTEX_SAMPLE_COUNT;
	simOptions.trailing_box = AERON_OPT_TRAILING_BOX_DESCRIPTOR;
	if (!Aeron_OptSimModelBuild(&model, &simOptions, &simulation, error, sizeof error)) {
		Aeron_FlightModelFreeSemantics(&model);
		return OptSynth_Fail(key, error);
	}
	if (!OptSynth_ValidateSimulation(key, &simulation)) {
		Aeron_OptSimModelFree(&simulation);
		Aeron_FlightModelFreeSemantics(&model);
		return 0;
	}

	stubOffsets = (uint32_t*)calloc(simulation.mesh_count, sizeof *stubOffsets);
	imageOptions.profile = AERON_OPT_IMAGE_PROFILE_V5;
	imageOptions.placeholder_texture = true;
	payload = NULL;
	payloadSize = 0;
	if (stubOffsets == NULL || !Aeron_OptSimModelWriteImage(&simulation, &imageOptions, &payload,
															&payloadSize, stubOffsets, error, sizeof error)) {
		free(stubOffsets);
		Aeron_OptSimModelFree(&simulation);
		Aeron_FlightModelFreeSemantics(&model);
		return OptSynth_Fail(key, stubOffsets == NULL ? "out of memory" : error);
	}

	vertexTotal = 0;
	faceTotal = 0;
	nodeTotal = 0;
	cooked = 1;
	for (meshIndex = 0; meshIndex < simulation.mesh_count; ++meshIndex) {
		const AeronOptSimMesh* mesh = &simulation.meshes[meshIndex];

		vertexTotal += mesh->vertex_count;
		faceTotal += mesh->face_count;
		nodeTotal += mesh->bvh != NULL ? mesh->bvh->node_count : 0;
		cooked &= mesh->bvh_cooked;
	}
	OptSynth_WarnDenseCockpit(key, &model);

	Aeron_OptSimModelTrimToFaces(&simulation);
	synth = OptSynth_CreateData(key, glbHash, &model, &simulation, stubOffsets);
	free(stubOffsets);
	Aeron_FlightModelFreeSemantics(&model);
	if (synth == NULL) {
		Aeron_OptSimModelFree(&simulation);
		free(payload);
		return OptSynth_Fail(key, "out of memory");
	}

	Aeron_LogInfo("xwa.models",
				  "replacement '%s': hash=%016llx components=%u vertices=%u faces=%u image=%d bytes "
				  "bvh_nodes=%u simulation=%s",
				  key, (unsigned long long)glbHash, synth->meshCount, vertexTotal, faceTotal, payloadSize,
				  nodeTotal, cooked ? "cooked" : "built at load");
	*outPayload = payload;
	*outPayloadSize = payloadSize;
	*outSynth = synth;
	return 1;
}

void OptSynth_Free(XwaOptSynthData* synth) {
	if (synth == NULL) {
		return;
	}
	Aeron_OptSimModelFree(&synth->simulation);
	free(synth->meshes);
	free(synth->tagStorage);
	free(synth);
}

const XwaOptSynthMesh* OptSynth_FindMeshByStubOffset(const XwaOptSynthData* synth,
													 uint32_t stubFaceDataOffset) {
	uint32_t low;
	uint32_t high;

	if (synth == NULL) {
		return NULL;
	}
	low = 0;
	high = synth->meshCount;
	while (low < high) {
		uint32_t middle = low + (high - low) / 2;

		if (synth->meshes[middle].stubFaceDataOffset < stubFaceDataOffset) {
			low = middle + 1;
		} else {
			high = middle;
		}
	}
	return low < synth->meshCount && synth->meshes[low].stubFaceDataOffset == stubFaceDataOffset
			   ? &synth->meshes[low]
			   : NULL;
}

int OptSynth_FindComponentByTag(const XwaOptSynthData* synth, const char* tag) {
	uint32_t mesh;

	if (synth == NULL || tag == NULL) {
		return -1;
	}
	for (mesh = 0; mesh < synth->meshCount; ++mesh) {
		uint32_t index;

		for (index = 0; index < synth->meshes[mesh].tagCount; ++index) {
			if (strcmp(synth->meshes[mesh].tags[index].name, tag) == 0) {
				return (int)mesh;
			}
		}
	}
	return -1;
}

/* ===== Native model integration ===== */

uint16_t OptSynth_LoadToHandle(const char* fileName, int* outVersion) {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	uint8_t* payload;
	int payloadSize;
	XwaOptSynthData* synth;
	uint16_t handle;
	NativeOptimizedPolyObject* native;

	if (outVersion != NULL) {
		*outVersion = 0;
	}

	if (!XwaModelSource_NormalizeKey(fileName, key)) {
		Aeron_LogError("xwa.models", "invalid replacement model name '%s'", fileName);
		return 0;
	}
	if (!OptSynth_BuildPayload(key, &payload, &payloadSize, &synth)) {
		return 0;
	}

	if (g_loadOptBufHandle != 0) {
		OptModel_FreeHandle(g_loadOptBufHandle);
	}

	handle = Memory_AllocHandle(0, sizeof(NativeOptimizedPolyObject));
	if (handle == 0) {
		Aeron_LogError("xwa.assets", "Failed to allocate native OPT handle for '%s'", fileName);
		OptSynth_Free(synth);
		free(payload);
		return 0;
	}

	native = (NativeOptimizedPolyObject*)Memory_LockHandle(handle);
	if (native == NULL) {
		Aeron_LogError("xwa.assets", "Failed to lock native OPT handle %u for '%s'", (unsigned)handle,
					   fileName);
		Memory_FreeHandle(0, handle);
		OptSynth_Free(synth);
		free(payload);
		return 0;
	}

	if (!OptModel_InitNativeModelFromBytes(native, payload, payloadSize)) {
		Aeron_LogError("xwa.assets", "Failed to parse synthesized OPT '%s' payload size %d", fileName,
					   payloadSize);
		OptModel_DestroyNativeModel(native);
		Memory_UnlockHandle(handle);
		Memory_FreeHandle(0, handle);
		OptSynth_Free(synth);
		return 0;
	}
	native->synth = synth;

	Memory_UnlockHandle(handle);
	if (outVersion != NULL) {
		*outVersion = 5;
	}
	g_loadOptBufHandle = handle;
	g_loadOptBufSize = payloadSize;
	return handle;
}

static int OptSynth_IsFaceDataNode(const OptNode* node) {
	return node->nodeType == OPT_FACEDATA || node->nodeType == OPT_FACEDATA_15 ||
		   node->nodeType == OPT_FACEDATA_16 || node->nodeType == OPT_FACEDATA_17;
}

static int OptSynth_FindStubFaceData(const OptNode* node, const OptNode** found) {
	int childIndex;

	if (node == NULL) {
		return 1;
	}
	if (OptSynth_IsFaceDataNode(node)) {
		if (*found != NULL && *found != node) {
			return 0;
		}
		*found = node;
	}
	for (childIndex = 0; childIndex < node->childCount; ++childIndex) {
		if (!OptSynth_FindStubFaceData(node->pChildren[childIndex], found)) {
			return 0;
		}
	}
	return 1;
}

int OptSynth_RekeyStubs(NativeOptimizedPolyObject* native) {
	XwaOptSynthData* synth;
	uint32_t meshIndex;
	int rootIndex;

	synth = native->synth;
	meshIndex = 0;
	for (rootIndex = 0; rootIndex < native->model.rootNodeCount; ++rootIndex) {
		const OptNode* root;
		const OptNode* stub;
		uint32_t offset;

		root = native->model.rootNodes[rootIndex];
		if (root->nodeType == OPT_TEXTURE || root->nodeType == OPT_TEXTURE_REF) {
			continue;
		}
		stub = NULL;
		if (meshIndex >= synth->meshCount || !OptSynth_FindStubFaceData(root, &stub) || stub == NULL ||
			(uint8_t*)stub->param2 < native->serializedData ||
			(uint8_t*)stub->param2 >= native->serializedData + native->serializedSize) {
			return 0;
		}
		offset = (uint32_t)((uint8_t*)stub->param2 - native->serializedData);
		if (meshIndex != 0 && offset <= synth->meshes[meshIndex - 1].stubFaceDataOffset) {
			return 0;
		}
		synth->meshes[meshIndex++].stubFaceDataOffset = offset;
	}
	return meshIndex == synth->meshCount;
}

int OptSynth_IsSynthesized(const OptimizedPolyObject* model) {
	return model != NULL && ((const NativeOptimizedPolyObject*)model)->synth != NULL;
}

const XwaOptSynthData* OptSynth_GetModelData(const OptimizedPolyObject* model) {
	return model != NULL ? ((const NativeOptimizedPolyObject*)model)->synth : NULL;
}

const XwaOptSynthMesh* OptSynth_FindMeshForNode(const OptimizedPolyObject* model,
												const OptNode* faceDataNode) {
	const NativeOptimizedPolyObject* native;

	native = (const NativeOptimizedPolyObject*)model;
	if (native == NULL || native->synth == NULL || faceDataNode == NULL ||
		(const uint8_t*)faceDataNode->param2 < native->serializedData ||
		(const uint8_t*)faceDataNode->param2 >= native->serializedData + native->serializedSize) {
		return NULL;
	}
	return OptSynth_FindMeshByStubOffset(
		native->synth, (uint32_t)((const uint8_t*)faceDataNode->param2 - native->serializedData));
}

int OptSynth_FindModelComponentByTag(const OptimizedPolyObject* model, const char* tag) {
	return OptSynth_FindComponentByTag(OptSynth_GetModelData(model), tag);
}
