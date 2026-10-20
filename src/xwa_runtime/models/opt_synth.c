#include "xwa_runtime/models/opt_synth.h"

#include "aeron/aeron.h"
#include "aeron/log.h"
#include "aeron/vfs.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/opt_model_native.h"
#include "xwa/flight/fediskio.h"
#include "xwa/util/memory.h"
#include "xwa_runtime/models/model_roles.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/model_swaps.h"
#include "xwa_runtime/models/render_model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. Compiled without
   floating-point contraction so derived simulation data is identical on
   every platform. */

enum {
	OPT_SYNTH_MAX_ENGINE_GLOWS = XWA_CRAFT_ENGINE_EMITTER_COUNT,
	OPT_SYNTH_MAX_GUNNER_HARDPOINTS = 8,
	OPT_SYNTH_HARDPOINT_GUNNER = 19,
	OPT_SYNTH_HARDPOINT_ACC_END = 24,
	OPT_SYNTH_HARDPOINT_COCKPIT = 31,
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

/* Articulation names OpenXWA reads. A
   `cargo_clamps.` group may end with any carried model's encoded name, and
   `variant.<name>.` may prefix any group but `crew`. */
static const char* const s_articulationGroups[] = {
	"sfoils",       "landing_gear",      "hangar_doors",       "hatches",
	"warheads_bay", "cargo_clamps.open", "cargo_clamps.close", "crew"
};
static const char* const s_articulationBehaviors[] = { "swing", "random", "random_pause" };
static const char* const s_articulationSettings[] = {
	"step_ticks",         "sfoils.close_in_hyperspace", "sfoils.allow_fire_closed",
	"sfoils.park_closed", "hangar_doors.distance",      "crew.update_all"
};

static int OptSynth_IsKnownName(const char* name, const char* const* names, size_t count) {
	size_t index;

	for (index = 0; index < count; ++index) {
		if (strcmp(name, names[index]) == 0) {
			return 1;
		}
	}
	return 0;
}

static int OptSynth_Fail(const char* key, const char* reason) {
	Aeron_LogError("xwa.models", "replacement '%s' rejected: %s", key, reason);
	return 0;
}

static int OptSynth_IsLaserHardpoint(int32_t type) {
	return type == 0 || (type >= 1 && type <= 6) || (type >= 9 && type <= 11) || type == 18;
}

/* The key whose object type the checks below use: the key the mission swaps
   for `key`, else `key`. A swapped-in model, such as
   superstardestroyerrecentered, then gets the checks of the model it
   replaces. */
static const char* OptSynth_TypeKey(const char* key) {
	const char* source = XwaModelSwaps_SourceOf(key);

	return source != NULL ? source : key;
}

static int OptSynth_ValidateModel(const char* key, const AeronFlightModel* model) {
	char reason[160];
	uint32_t gunnerCount;
	uint32_t cockpitEyeCount;
	uint32_t component;

	if (model->component_count == 0 || model->component_count > XWA_OPT_SYNTH_MAX_COMPONENTS) {
		snprintf(reason, sizeof reason, "%u components (1..%d allowed)", model->component_count,
				 XWA_OPT_SYNTH_MAX_COMPONENTS);
		return OptSynth_Fail(key, reason);
	}
	if (model->engine_glow_count > OPT_SYNTH_MAX_ENGINE_GLOWS) {
		snprintf(reason, sizeof reason, "%u engine glows (at most %d)", model->engine_glow_count,
				 OPT_SYNTH_MAX_ENGINE_GLOWS);
		return OptSynth_Fail(key, reason);
	}
	gunnerCount = 0;
	cockpitEyeCount = 0;
	for (component = 0; component < model->component_count; ++component) {
		const AeronFlightComponent* item = &model->components[component];
		uint32_t index;

		for (index = 0; index < item->hardpoint_count; ++index) {
			gunnerCount += item->hardpoints[index].type == OPT_SYNTH_HARDPOINT_GUNNER ||
						   item->hardpoints[index].type == OPT_SYNTH_HARDPOINT_ACC_END;
			cockpitEyeCount += item->hardpoints[index].type == OPT_SYNTH_HARDPOINT_COCKPIT;
		}
	}
	if (gunnerCount > OPT_SYNTH_MAX_GUNNER_HARDPOINTS) {
		snprintf(reason, sizeof reason, "%u Gunner/AccEnd hardpoints (at most %d)", gunnerCount,
				 OPT_SYNTH_MAX_GUNNER_HARDPOINTS);
		return OptSynth_Fail(key, reason);
	}
	/* FeDiskIo_BuildModelDef takes the pilot eye from the craft model's Cockpit
	   hardpoint; with several, the last one in component order would win. */
	if (cockpitEyeCount > 1) {
		snprintf(reason, sizeof reason, "%u Cockpit (type 31) hardpoints (at most 1)", cockpitEyeCount);
		return OptSynth_Fail(key, reason);
	}
	if (cockpitEyeCount != 0 && XwaModelSource_IsCockpitClassKey(OptSynth_TypeKey(key))) {
		Aeron_LogWarn("xwa.models",
					  "replacement '%s': cockpit models ignore Cockpit (type 31) hardpoints; the pilot eye "
					  "belongs on the craft model",
					  key);
	}

	if (!XwaModelRole_Validate(OptSynth_TypeKey(key), model, reason, sizeof reason)) {
		return OptSynth_Fail(key, reason);
	}

	if (XwaModelSource_KeyRequiresFloatHardpoints(OptSynth_TypeKey(key))) {
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
		/* The laser hardpoints become the model def's float hardpoints. Without
		   any, the warhead launch point falls back to MainHull vertices, which
		   reads out of range when there is no MainHull. */
		if (!hasLaser) {
			return OptSynth_Fail(key, "its object type needs a laser hardpoint (type 0-6, 9-11 or 18)");
		}
		/* The original code runs without a MainHull: hardpoints go to
		   the nearest weapon-system mesh (root 0 when there is none), and only
		   MainHull-keyed effects are lost. */
		if (!hasMainHull) {
			Aeron_LogWarn(
				"xwa.models",
				"replacement '%s' has no MainHull component: no hull debris or forced hull "
				"explosions, and float hardpoints without a weapon-system mesh belong to component 0",
				key);
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
	uint32_t floatHardpoints = 0;

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
		/* Float hardpoints as FeDiskIo_BuildModelDef collects them: type 0 and
		   laser-group hardpoints, plus a mirrored copy of each at more than 256
		   OPT units off the centre line, up to 256 entries. */
		for (index = 0; index < mesh->hardpoint_count; ++index) {
			const int32_t type = mesh->hardpoints[index].type;
			const int x = (int)mesh->hardpoints[index].position[0];

			if (type != 0 && (type < 0 || type >= 40 || g_optHardpointWeaponGroupKindByType[type] != 1)) {
				continue;
			}
			if (floatHardpoints < 256) {
				++floatHardpoints;
			}
			if (floatHardpoints < 256 && (x > 256 || x < -256)) {
				++floatHardpoints;
			}
		}
	}
	/* The model def stores the count in a byte: 256 reads back as none, which
	   sends the warhead launch point to the MainHull-vertex fallback. */
	if (floatHardpoints >= 256 && XwaModelSource_KeyRequiresFloatHardpoints(OptSynth_TypeKey(key))) {
		return OptSynth_Fail(key, "256 or more float hardpoints (the model def holds at most 255)");
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

	if (!XwaModelSource_IsCockpitClassKey(OptSynth_TypeKey(key))) {
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

/* Warns once per load about articulation names OpenXWA does not read; they
   are ignored. */
static void OptSynth_WarnUnknownArticulations(const char* key, const AeronFlightModel* model) {
	uint32_t index;

	for (index = 0; index < model->articulation_count; ++index) {
		const AeronFlightArticulation* articulation = &model->articulations[index];
		const char* group = articulation->group;

		if (strncmp(group, "variant.", 8) == 0) {
			const char* end = strchr(group + 8, '.');

			group = end != NULL && end > group + 8 && strcmp(end + 1, "crew") != 0 ? end + 1 : "";
		}
		if (!OptSynth_IsKnownName(group, s_articulationGroups,
								  sizeof s_articulationGroups / sizeof s_articulationGroups[0]) &&
			strncmp(group, "cargo_clamps.", 13) != 0) {
			Aeron_LogWarn("xwa.models", "replacement '%s': component %u has unknown articulation group '%s'",
						  key, articulation->component, articulation->group);
		}
		if (articulation->behavior[0] != '\0' &&
			!OptSynth_IsKnownName(articulation->behavior, s_articulationBehaviors,
								  sizeof s_articulationBehaviors / sizeof s_articulationBehaviors[0])) {
			Aeron_LogWarn("xwa.models",
						  "replacement '%s': component %u has unknown articulation behavior '%s'", key,
						  articulation->component, articulation->behavior);
		}
	}
	for (index = 0; index < model->articulation_setting_count; ++index) {
		if (!OptSynth_IsKnownName(model->articulation_settings[index].name, s_articulationSettings,
								  sizeof s_articulationSettings / sizeof s_articulationSettings[0])) {
			Aeron_LogWarn("xwa.models", "replacement '%s': unknown articulation setting '%s'", key,
						  model->articulation_settings[index].name);
		}
	}
}

static XwaOptSynthData* OptSynth_CreateData(const char* key, const AeronFlightModel* model,
											AeronOptSimModel* simulation, const uint32_t* stubOffsets) {
	XwaOptSynthData* synth;
	uint32_t tagTotal;
	uint32_t component;
	const uint32_t articulationCount = model->articulation_count;
	const uint32_t settingCount = model->articulation_setting_count;
	const uint32_t setCount = model->component_set_count;

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
	synth->articulations =
		articulationCount != 0
			? (AeronFlightArticulation*)calloc(articulationCount, sizeof *synth->articulations)
			: NULL;
	synth->articulationSettings =
		settingCount != 0
			? (AeronFlightArticulationSetting*)calloc(settingCount, sizeof *synth->articulationSettings)
			: NULL;
	synth->componentSets =
		setCount != 0 ? (AeronFlightComponentSet*)calloc(setCount, sizeof *synth->componentSets) : NULL;
	if (synth->meshes == NULL || (tagTotal != 0 && synth->tagStorage == NULL) ||
		(articulationCount != 0 && synth->articulations == NULL) ||
		(settingCount != 0 && synth->articulationSettings == NULL) ||
		(setCount != 0 && synth->componentSets == NULL)) {
		free(synth->meshes);
		free(synth->tagStorage);
		free(synth->articulations);
		free(synth->articulationSettings);
		free(synth->componentSets);
		free(synth);
		return NULL;
	}
	if (setCount != 0) {
		memcpy(synth->componentSets, model->component_sets, setCount * sizeof *synth->componentSets);
	}
	synth->componentSetCount = setCount;
	if (articulationCount != 0) {
		memcpy(synth->articulations, model->articulations, articulationCount * sizeof *synth->articulations);
	}
	synth->articulationCount = articulationCount;
	if (settingCount != 0) {
		memcpy(synth->articulationSettings, model->articulation_settings,
			   settingCount * sizeof *synth->articulationSettings);
	}
	synth->articulationSettingCount = settingCount;
	snprintf(synth->key, sizeof synth->key, "%s", key);
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
		mesh->authoredOrder = source->authored_order;
		mesh->tagCount = item->tag_count;
		mesh->tags = item->tag_count != 0 ? &synth->tagStorage[tagTotal] : NULL;
		if (item->tag_count != 0) {
			memcpy(&synth->tagStorage[tagTotal], item->tags, item->tag_count * sizeof *item->tags);
		}
		tagTotal += item->tag_count;
	}
	return synth;
}

/* Builds the payload (the OPT bytes after the -version/size header) for
   `key`. On success the caller owns *outPayload (free()) and *outSynth
   (OptSynth_Free). Returns 0 and logs the reason on failure. */
static int OptSynth_BuildPayload(const char* key, uint8_t** outPayload, int* outPayloadSize,
								 XwaOptSynthData** outSynth) {
	char path[XWA_MODEL_SOURCE_KEY_MAX + 32];
	char error[256];
	uint8_t* glb;
	size_t glbSize;
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

	if (!XwaModelSource_GlbPath(key, path, sizeof path)) {
		return OptSynth_Fail(key, "GLB path too long");
	}
	glb = NULL;
	glbSize = 0;
	if (!AeronVfs_ReadAll(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, 0, &glb, &glbSize)) {
		return OptSynth_Fail(key, "cannot read the GLB");
	}
	/* The full build: the HD renderer takes the render half from this parse
	   (XwaRenderModel_Find) instead of reading the GLB again. */
	if (!Aeron_FlightModelBuildMemory(glb, glbSize, path, &model)) {
		free(glb);
		return OptSynth_Fail(key, "invalid GLB or AERON_flight_model data");
	}
	free(glb);
	if (!OptSynth_ValidateModel(key, &model)) {
		Aeron_FlightModelFree(&model);
		return 0;
	}

	simOptions.vertex_sample_count = OPT_SYNTH_VERTEX_SAMPLE_COUNT;
	simOptions.trailing_box = AERON_OPT_TRAILING_BOX_DESCRIPTOR;
	if (!Aeron_OptSimModelBuild(&model, &simOptions, &simulation, error, sizeof error)) {
		Aeron_FlightModelFree(&model);
		return OptSynth_Fail(key, error);
	}
	if (!OptSynth_ValidateSimulation(key, &simulation)) {
		Aeron_OptSimModelFree(&simulation);
		Aeron_FlightModelFree(&model);
		return 0;
	}
	/* Faces in BVH leaf order: collision queries return them ascending
	   without sorting. Exact ties between faces of one component then go to
	   the face earlier in leaf order. Failing the load on allocation failure
	   keeps that order the same on every machine. */
	if (!Aeron_OptSimModelReorderFacesToLeafOrder(&simulation)) {
		Aeron_OptSimModelFree(&simulation);
		Aeron_FlightModelFree(&model);
		return OptSynth_Fail(key, "out of memory");
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
		Aeron_FlightModelFree(&model);
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
		if (mesh->face_count != 0) {
			cooked &= mesh->bvh_cooked;
		}
	}
	OptSynth_WarnDenseCockpit(key, &model);
	OptSynth_WarnUnknownArticulations(key, &model);

	Aeron_OptSimModelTrimToFaces(&simulation);
	synth = OptSynth_CreateData(key, &model, &simulation, stubOffsets);
	free(stubOffsets);
	if (synth == NULL) {
		Aeron_FlightModelFree(&model);
		Aeron_OptSimModelFree(&simulation);
		free(payload);
		return OptSynth_Fail(key, "out of memory");
	}
	Aeron_FlightModelReleaseSimulationData(&model);
	synth->renderModel = model;
	synth->hasRenderModel = 1;

	Aeron_LogInfo("xwa.models",
				  "replacement '%s': components=%u vertices=%u faces=%u image=%d bytes "
				  "bvh_nodes=%u simulation=%s",
				  key, synth->meshCount, vertexTotal, faceTotal, payloadSize, nodeTotal,
				  cooked ? "cooked" : "built at load");
	*outPayload = payload;
	*outPayloadSize = payloadSize;
	*outSynth = synth;
	return 1;
}

/* Loaded replacements, most recent first; linked once a handle owns them. */
static XwaOptSynthData* s_liveSynths;

static void OptSynth_Unlink(XwaOptSynthData* synth) {
	XwaOptSynthData** link;

	for (link = &s_liveSynths; *link != NULL; link = &(*link)->nextLive) {
		if (*link == synth) {
			*link = synth->nextLive;
			synth->nextLive = NULL;
			return;
		}
	}
}

static void OptSynth_DropRenderModel(XwaOptSynthData* synth) {
	if (synth->hasRenderModel) {
		Aeron_FlightModelFree(&synth->renderModel);
		synth->hasRenderModel = 0;
	}
}

const AeronFlightModel* XwaRenderModel_Find(const char* key) {
	const XwaOptSynthData* synth;

	for (synth = s_liveSynths; synth != NULL && key != NULL; synth = synth->nextLive) {
		if (synth->hasRenderModel && strcmp(synth->key, key) == 0) {
			return &synth->renderModel;
		}
	}
	return NULL;
}

void XwaRenderModel_Release(const char* key) {
	XwaOptSynthData* synth;

	for (synth = s_liveSynths; synth != NULL && key != NULL; synth = synth->nextLive) {
		if (strcmp(synth->key, key) == 0) {
			OptSynth_DropRenderModel(synth);
		}
	}
}

void OptSynth_Free(XwaOptSynthData* synth) {
	if (synth == NULL) {
		return;
	}
	OptSynth_Unlink(synth);
	OptSynth_DropRenderModel(synth);
	Aeron_OptSimModelFree(&synth->simulation);
	free(synth->meshes);
	free(synth->tagStorage);
	free(synth->articulations);
	free(synth->articulationSettings);
	free(synth->componentSets);
	free(synth);
}

/* Mesh whose stub FaceData sits at `stubFaceDataOffset`, or NULL. */
static const XwaOptSynthMesh* OptSynth_FindMeshByStubOffset(const XwaOptSynthData* synth,
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

/* First component carrying `tag`, or -1. */
static int OptSynth_FindComponentByTag(const XwaOptSynthData* synth, const char* tag) {
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
	synth->nextLive = s_liveSynths;
	s_liveSynths = synth;

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

const AeronFlightComponentSet* OptSynth_FindComponentSet(const OptimizedPolyObject* model, const char* name) {
	const XwaOptSynthData* synth = OptSynth_GetModelData(model);
	uint32_t index;

	for (index = 0; synth != NULL && name != NULL && index < synth->componentSetCount; ++index) {
		if (strcmp(synth->componentSets[index].name, name) == 0) {
			return &synth->componentSets[index];
		}
	}
	return NULL;
}

int32_t OptSynth_GetArticulationSetting(const OptimizedPolyObject* model, const char* name,
										int32_t fallback) {
	const XwaOptSynthData* synth = OptSynth_GetModelData(model);
	uint32_t index;

	if (synth == NULL || name == NULL) {
		return fallback;
	}
	for (index = 0; index < synth->articulationSettingCount; ++index) {
		if (strcmp(synth->articulationSettings[index].name, name) == 0) {
			return synth->articulationSettings[index].value;
		}
	}
	return fallback;
}
