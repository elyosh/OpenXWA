#include "xwa_runtime/models/model_roles.h"

#include "aeron/asset/flight_model.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/util/memory.h"
#include "xwa_runtime/models/opt_synth.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

typedef enum ModelRoleKind {
	MODEL_ROLE_SINGLE, /* one component; warned about when missing or repeated */
	MODEL_ROLE_SET,    /* any number of components; optional */
} ModelRoleKind;

typedef struct ModelRoleInfo {
	const char* tag;
	ModelRoleKind kind;
	const char* const* keys; /* models that carry the role, NULL-terminated */
} ModelRoleInfo;

static const char* const s_hangarCraneKeys[] = { "hangarcrane", NULL };
static const char* const s_hangarDroidKeys[] = { "hangardroid", NULL };
static const char* const s_hangarDroid2Keys[] = { "hangardroid2", NULL };
static const char* const s_workDroid1Keys[] = { "workdroid1", NULL };
static const char* const s_pilotKeys[] = { "rebelpilot", "imperialpilot", NULL };
static const char* const s_accelRingKeys[] = { "accelring2", "accelring3", NULL };
static const char* const s_compactorKeys[] = { "compactor", NULL };
static const char* const s_centrifugeKeys[] = { "centrifuge", NULL };
static const char* const s_smeltingRoomKeys[] = { "smeltingroom", NULL };
static const char* const s_dsReactorCoreKeys[] = { "dsreactorcore", NULL };
static const char* const s_platformKeys[] = { "platform1", "platform2", "platform3",
											  "platform4", "platform5", NULL };

static const ModelRoleInfo s_roles[XWA_MODEL_ROLE_COUNT] = {
	[XWA_ROLE_PLATFORM_HIDDEN_BY_BEAM] = { "platform.hidden_by_beam", MODEL_ROLE_SET, s_platformKeys },
	[XWA_ROLE_PLATFORM_HIDDEN_BY_JAMMING_DECOY] = { "platform.hidden_by_jamming_decoy", MODEL_ROLE_SET,
													s_platformKeys },
	[XWA_ROLE_HANGARCRANE_ARM] = { "hangarcrane.arm", MODEL_ROLE_SINGLE, s_hangarCraneKeys },
	[XWA_ROLE_HANGARDROID_NOD] = { "hangardroid.nod", MODEL_ROLE_SINGLE, s_hangarDroidKeys },
	[XWA_ROLE_HANGARDROID2_SWING] = { "hangardroid2.swing", MODEL_ROLE_SINGLE, s_hangarDroid2Keys },
	[XWA_ROLE_HANGARDROID2_SPIN] = { "hangardroid2.spin", MODEL_ROLE_SINGLE, s_hangarDroid2Keys },
	[XWA_ROLE_WORKDROID1_SPIN] = { "workdroid1.spin", MODEL_ROLE_SINGLE, s_workDroid1Keys },
	[XWA_ROLE_WORKDROID1_TURN] = { "workdroid1.turn", MODEL_ROLE_SINGLE, s_workDroid1Keys },
	[XWA_ROLE_WORKDROID1_NOD_1] = { "workdroid1.nod.1", MODEL_ROLE_SINGLE, s_workDroid1Keys },
	[XWA_ROLE_WORKDROID1_NOD_2] = { "workdroid1.nod.2", MODEL_ROLE_SINGLE, s_workDroid1Keys },
	[XWA_ROLE_PILOT_LIMB_1] = { "pilot.limb.1", MODEL_ROLE_SINGLE, s_pilotKeys },
	[XWA_ROLE_PILOT_LIMB_2] = { "pilot.limb.2", MODEL_ROLE_SINGLE, s_pilotKeys },
	[XWA_ROLE_PILOT_LIMB_3] = { "pilot.limb.3", MODEL_ROLE_SINGLE, s_pilotKeys },
	[XWA_ROLE_PILOT_LIMB_4] = { "pilot.limb.4", MODEL_ROLE_SINGLE, s_pilotKeys },
	[XWA_ROLE_ACCELRING_CORE] = { "accelring.core", MODEL_ROLE_SINGLE, s_accelRingKeys },
	[XWA_ROLE_COMPACTOR_DOOR_1] = { "compactor.door.1", MODEL_ROLE_SINGLE, s_compactorKeys },
	[XWA_ROLE_COMPACTOR_DOOR_2] = { "compactor.door.2", MODEL_ROLE_SINGLE, s_compactorKeys },
	[XWA_ROLE_COMPACTOR_DOOR_3] = { "compactor.door.3", MODEL_ROLE_SINGLE, s_compactorKeys },
	[XWA_ROLE_COMPACTOR_DOOR_4] = { "compactor.door.4", MODEL_ROLE_SINGLE, s_compactorKeys },
	[XWA_ROLE_CENTRIFUGE_SPIN_1] = { "centrifuge.spin.1", MODEL_ROLE_SINGLE, s_centrifugeKeys },
	[XWA_ROLE_CENTRIFUGE_SPIN_2] = { "centrifuge.spin.2", MODEL_ROLE_SINGLE, s_centrifugeKeys },
	[XWA_ROLE_CENTRIFUGE_SPIN_3] = { "centrifuge.spin.3", MODEL_ROLE_SINGLE, s_centrifugeKeys },
	[XWA_ROLE_CENTRIFUGE_DOOR_1] = { "centrifuge.door.1", MODEL_ROLE_SINGLE, s_centrifugeKeys },
	[XWA_ROLE_CENTRIFUGE_DOOR_2] = { "centrifuge.door.2", MODEL_ROLE_SINGLE, s_centrifugeKeys },
	[XWA_ROLE_CENTRIFUGE_DOOR_3] = { "centrifuge.door.3", MODEL_ROLE_SINGLE, s_centrifugeKeys },
	[XWA_ROLE_SMELTINGROOM_TURRET_1] = { "smeltingroom.turret.1", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_SMELTINGROOM_TURRET_2] = { "smeltingroom.turret.2", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_SMELTINGROOM_TURRET_3] = { "smeltingroom.turret.3", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_SMELTINGROOM_TURRET_4] = { "smeltingroom.turret.4", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_SMELTINGROOM_TURRET_5] = { "smeltingroom.turret.5", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_SMELTINGROOM_TURRET_6] = { "smeltingroom.turret.6", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_SMELTINGROOM_POWER_1] = { "smeltingroom.power.1", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_SMELTINGROOM_POWER_2] = { "smeltingroom.power.2", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_SMELTINGROOM_TARGET] = { "smeltingroom.target", MODEL_ROLE_SINGLE, s_smeltingRoomKeys },
	[XWA_ROLE_DSREACTORCORE_REGENERATOR] = { "dsreactorcore.regenerator", MODEL_ROLE_SINGLE,
											 s_dsReactorCoreKeys },
	[XWA_ROLE_DSREACTORCORE_REACTOR] = { "dsreactorcore.reactor", MODEL_ROLE_SINGLE, s_dsReactorCoreKeys },
};

static int ModelRole_AppliesTo(const ModelRoleInfo* info, const char* key) {
	const char* const* cursor;

	for (cursor = info->keys; *cursor != NULL; ++cursor) {
		if (strcmp(*cursor, key) == 0) {
			return 1;
		}
	}
	return 0;
}

int XwaModelRole_Validate(const char* key, const struct AeronFlightModel* model, char* reason,
						  unsigned int reasonSize) {
	int role;

	for (role = 0; role < XWA_MODEL_ROLE_COUNT; ++role) {
		const ModelRoleInfo* info = &s_roles[role];
		uint32_t carriers;

		if (info->kind != MODEL_ROLE_SINGLE || !ModelRole_AppliesTo(info, key)) {
			continue;
		}
		/* The model still loads: a missing role falls back to the stock
		   component number, a repeated one resolves to its first carrier. Stock
		   numbers index the craft's per-component arrays, which every model
		   has, so the fallback is memory safe; it only animates or targets
		   whatever component sits at that number. */
		carriers = Aeron_FlightModelCountTag(model, info->tag);
		if (carriers != 1) {
			Aeron_LogWarn("xwa.models",
						  "replacement '%s' should tag exactly one component '%s' (found %u); %s", key,
						  info->tag, carriers,
						  carriers ? "the first one is used" : "the stock component number is used");
		}
	}
	(void)reason;
	(void)reasonSize;
	return 1;
}

/* Locks `objectType`'s loaded model; NULL when none is loaded. */
static const OptimizedPolyObject* ModelRole_LockModel(unsigned short objectType, MemoryHandle* handle) {
	*handle = objectType < OBJ_Count ? g_loadedModels.byObjectType[objectType] : 0;
	return *handle != 0 ? (const OptimizedPolyObject*)Memory_LockHandle(*handle) : NULL;
}

int XwaModelRole_IsReplacement(unsigned short objectType) {
	MemoryHandle handle;
	const OptimizedPolyObject* model = ModelRole_LockModel(objectType, &handle);
	const int replacement = OptSynth_IsSynthesized(model);

	if (model != NULL) {
		Memory_UnlockHandle(handle);
	}
	return replacement;
}

int XwaModelRole_Resolve(unsigned short objectType, XwaModelRole role, int stockComponent) {
	MemoryHandle handle;
	const OptimizedPolyObject* model;
	int component;

	if ((unsigned int)role >= XWA_MODEL_ROLE_COUNT) {
		return stockComponent;
	}
	model = ModelRole_LockModel(objectType, &handle);
	if (model == NULL) {
		return stockComponent;
	}
	component = stockComponent;
	if (OptSynth_IsSynthesized(model)) {
		component = OptSynth_FindModelComponentByTag(model, s_roles[role].tag);
		if (component < 0) {
			/* Validation warned once at load; this runs per frame. */
			component = stockComponent;
		}
	}
	Memory_UnlockHandle(handle);
	return component;
}

int XwaModelRole_Collect(unsigned short objectType, XwaModelRole role, int* components, int capacity) {
	if ((unsigned int)role >= XWA_MODEL_ROLE_COUNT) {
		return 0;
	}
	return XwaModelRole_CollectTag(objectType, s_roles[role].tag, components, capacity);
}

int XwaModelRole_CollectTag(unsigned short objectType, const char* tagName, int* components, int capacity) {
	MemoryHandle handle;
	const OptimizedPolyObject* model;
	const XwaOptSynthData* synth;
	int count;
	uint32_t mesh;

	model = ModelRole_LockModel(objectType, &handle);
	if (model == NULL) {
		return 0;
	}
	synth = OptSynth_GetModelData(model);
	count = 0;
	for (mesh = 0; synth != NULL && mesh < synth->meshCount; ++mesh) {
		uint32_t tag;

		for (tag = 0; tag < synth->meshes[mesh].tagCount; ++tag) {
			if (strcmp(synth->meshes[mesh].tags[tag].name, tagName) == 0) {
				if (count < capacity) {
					components[count] = (int)mesh;
				}
				++count;
				break;
			}
		}
	}
	Memory_UnlockHandle(handle);
	return count < capacity ? count : capacity;
}
