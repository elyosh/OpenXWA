#include "xwa_remaster/environment.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa_remaster/debug_tools.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* XWA keeps four regions per mission. */
#define ENV_MAX_REGIONS 4
#define ENV_MAX_MISSIONS 128
#define ENV_MAX_HANGARS 64
/* At most 64 models with probes: one bit each in a prepared set's mask. */
#define ENV_MAX_PROBES 64
#define ENV_MAX_REFERENCES (1 + ENV_MAX_HANGARS + ENV_MAX_PROBES + ENV_MAX_MISSIONS * ENV_MAX_REGIONS)

/* A model with its own package: a hangar's lighting (`xwa.hangars`) or its
 * probes (`xwa.probes`). */
typedef struct EnvModel {
	char key[XWA_SNAP_OPT_NAME_MAX]; /* model key, lower case */
	int32_t reference;
} EnvModel;

typedef struct EnvMission {
	char key[XWA_ENV_MISSION_KEY_MAX];
	char source[XWA_ENV_MISSION_KEY_MAX];      /* manifest spelling, for diagnostics */
	int32_t region_reference[ENV_MAX_REGIONS]; /* index into references, -1 = classic */
} EnvMission;

static struct {
	int enabled;
	char manifest_path[256];
	AeronEnvironmentDefinitionSet* packages;
	AeronEnvironmentReference references[ENV_MAX_REFERENCES];
	uint32_t reference_count;
	int32_t hangar_reference; /* xwa.hangar, -1 = none */
	EnvModel hangars[ENV_MAX_HANGARS];
	uint32_t hangar_count;
	EnvModel probes[ENV_MAX_PROBES];
	uint32_t probe_count;
	EnvMission missions[ENV_MAX_MISSIONS];
	uint32_t mission_count;
	/* Package library and the prepared reference set: indices as passed to
	 * AeronEnvironmentLibrary_Prepare, -1 = not prepared. */
	AeronEnvironmentLibrary* library;
	char prepared_key[XWA_ENV_MISSION_KEY_MAX]; /* "" = no mission prepared */
	int32_t prepared_hangar;
	int32_t prepared_hangar_reference; /* the reference prepared_hangar holds, -1 = none */
	int32_t prepared_region[ENV_MAX_REGIONS];
	/* The probes entries of the mission's models, by bit, and their library
	 * indices (-1 = not prepared). */
	uint64_t prepared_probes_mask;
	int32_t prepared_probes[ENV_MAX_PROBES];
	/* The probe sets placed this frame, for the debug rows. */
	XwaEnvironmentProbeSet frame_sets[AERON_SCENE_MAX_PROBE_SETS];
	uint32_t frame_set_count;
	/* Last logged selection, to log changes only. */
	int logged;
	XwaEnvironmentSelection last;
	char last_mission[sizeof(((XwaSnapshot*)0)->mission_path)];
} s_env;

/* Host options and the debug switch; survive Shutdown, which clears s_env. */
static struct {
	int authored;
	int show_probes;
	int enabled;
	float sun_max_radiance; /* 0 = Aeron's default */
	char manifest[sizeof s_env.manifest_path];
} s_env_options = { .authored = 1 };

void XwaRemasterEnvironment_Configure(int enabled, const char* manifest, float sun_max_radiance) {
	s_env_options.enabled = enabled != 0;
	s_env_options.sun_max_radiance = sun_max_radiance;
	snprintf(s_env_options.manifest, sizeof s_env_options.manifest, "%s", manifest ? manifest : "");
}

static int env_node_error(const AeronConfigNode* node, const char* format, ...) {
	char message[512];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof message, format, args);
	va_end(args);
	const char* path = node ? AeronConfigNode_SourcePath(node) : NULL;
	Aeron_LogError("xwa.remaster", "%s:%d: %s", path ? path : s_env.manifest_path,
				   node ? AeronConfigNode_Line(node) : 0, message);
	return 0;
}

static int env_check_fields(const AeronConfigNode* map, const char* what, const char* const* allowed,
							size_t count) {
	for (size_t i = 0; i < AeronConfigNode_MapCount(map); i++) {
		const char* key = AeronConfigNode_MapKeyAt(map, i);
		size_t k = 0;
		while (k < count && strcmp(key, allowed[k]) != 0) {
			k++;
		}
		if (k == count) {
			return env_node_error(AeronConfigNode_MapValueAt(map, i), "%s: unknown field '%s'", what, key);
		}
	}
	return 1;
}

int XwaRemasterEnvironment_NormalizeMissionKey(const char* path, char* out, size_t size) {
	if (!path || !out || size == 0) {
		return 0;
	}
	size_t length = 0;
	const char* p = path;
	while (*p) {
		/* One component: up to the next separator. */
		const char* start = p;
		while (*p && *p != '/' && *p != '\\') {
			p++;
		}
		const size_t component = (size_t)(p - start);
		if (*p) {
			p++;
		}
		if (component == 0 || (component == 1 && start[0] == '.')) {
			continue;
		}
		if (component == 2 && start[0] == '.' && start[1] == '.') {
			return 0;
		}
		if (length + (length > 0) + component >= size) {
			return 0;
		}
		if (length > 0) {
			out[length++] = '/';
		}
		for (size_t i = 0; i < component; i++) {
			const char c = start[i];
			out[length++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
		}
	}
	out[length] = '\0';
	return length > 0;
}

/* Parses three finite numbers into normalized degrees. */
static int env_read_rotation(const AeronConfigNode* node, const char* what, float out[3]) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != 3) {
		return env_node_error(node, "%s: 'rotation_degrees' must be a sequence of three numbers", what);
	}
	for (size_t i = 0; i < 3; i++) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);
		const AeronConfigNodeType type = AeronConfigNode_Type(item);
		const double value = AeronConfigNode_Float(item, 0.0);
		if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) || !isfinite(value)) {
			return env_node_error(node, "%s: 'rotation_degrees' must contain three finite numbers", what);
		}
		out[i] = AeronEnvironment_NormalizeDegrees((float)value);
	}
	return 1;
}

/* Adds a reference to a package of the required kind. A reference is either
 * a package name or a map with `package` and an optional `rotation_degrees`. */
static int env_add_reference(const AeronConfigNode* node, const char* what, AeronEnvironmentKind kind,
							 int32_t* out_index) {
	static const char* const fields[] = { "package", "rotation_degrees" };
	const AeronConfigNode* package_node = node;
	const AeronConfigNode* rotation_node = NULL;
	if (AeronConfigNode_Type(node) == AERON_CONFIG_MAP) {
		if (!env_check_fields(node, what, fields, sizeof fields / sizeof fields[0])) {
			return 0;
		}
		package_node = AeronConfigNode_MapGet(node, "package");
		if (!package_node) {
			return env_node_error(node, "%s: missing required field 'package'", what);
		}
		rotation_node = AeronConfigNode_MapGet(node, "rotation_degrees");
	}
	if (AeronConfigNode_Type(package_node) != AERON_CONFIG_STRING) {
		return env_node_error(package_node, "%s: must name a package", what);
	}
	const char* name = AeronConfigNode_String(package_node, "");
	const AeronEnvironmentDefinition* definition = AeronEnvironmentDefinitionSet_Find(s_env.packages, name);
	if (!definition) {
		return env_node_error(package_node, "%s: unknown package '%s'", what, name);
	}
	if (definition->kind != kind) {
		return env_node_error(package_node, "%s: package '%s' must be of kind '%s'", what, name,
							  kind == AERON_ENV_KIND_FULL       ? "full"
							  : kind == AERON_ENV_KIND_LIGHTING ? "lighting"
																: "probes");
	}
	AeronEnvironmentReference* reference = &s_env.references[s_env.reference_count];
	memset(reference, 0, sizeof *reference);
	snprintf(reference->package, sizeof reference->package, "%s", name);
	if (rotation_node) {
		if (!env_read_rotation(rotation_node, what, reference->rotation_degrees)) {
			return 0;
		}
		reference->has_rotation = true;
	}
	*out_index = (int32_t)s_env.reference_count++;
	return 1;
}

static int env_parse_mission(const char* source, const AeronConfigNode* node) {
	static const char* const fields[] = { "regions" };
	char what[192];
	snprintf(what, sizeof what, "xwa.missions '%s'", source);
	if (s_env.mission_count >= ENV_MAX_MISSIONS) {
		return env_node_error(node, "xwa.missions: at most %d missions are allowed", ENV_MAX_MISSIONS);
	}
	EnvMission* mission = &s_env.missions[s_env.mission_count];
	memset(mission, 0, sizeof *mission);
	for (int r = 0; r < ENV_MAX_REGIONS; r++) {
		mission->region_reference[r] = -1;
	}
	if (strlen(source) >= sizeof mission->source ||
		!XwaRemasterEnvironment_NormalizeMissionKey(source, mission->key, sizeof mission->key)) {
		return env_node_error(node, "%s: invalid mission path", what);
	}
	snprintf(mission->source, sizeof mission->source, "%s", source);
	for (uint32_t i = 0; i < s_env.mission_count; i++) {
		if (strcmp(s_env.missions[i].key, mission->key) == 0) {
			return env_node_error(node, "%s: duplicate of mission '%s'", what, s_env.missions[i].source);
		}
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		return env_node_error(node, "%s must be a map", what);
	}
	if (!env_check_fields(node, what, fields, sizeof fields / sizeof fields[0])) {
		return 0;
	}
	const AeronConfigNode* regions = AeronConfigNode_MapGet(node, "regions");
	if (!regions || AeronConfigNode_Type(regions) != AERON_CONFIG_MAP) {
		return env_node_error(regions ? regions : node, "%s: 'regions' must be a map", what);
	}
	for (size_t i = 0; i < AeronConfigNode_MapCount(regions); i++) {
		const char* key = AeronConfigNode_MapKeyAt(regions, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(regions, i);
		if (!(key[0] >= '0' && key[0] < '0' + ENV_MAX_REGIONS && key[1] == '\0')) {
			return env_node_error(value, "%s: region key '%s' must be 0 to %d", what, key,
								  ENV_MAX_REGIONS - 1);
		}
		const int region = key[0] - '0';
		if (mission->region_reference[region] >= 0) {
			return env_node_error(value, "%s: duplicate region %d", what, region);
		}
		char region_what[224];
		snprintf(region_what, sizeof region_what, "%s region %d", what, region);
		if (!env_add_reference(value, region_what, AERON_ENV_KIND_FULL, &mission->region_reference[region])) {
			return 0;
		}
	}
	s_env.mission_count++;
	return 1;
}

static void env_lower(char* out, size_t size, const char* text) {
	size_t i = 0;
	for (; text[i] && i + 1 < size; i++) {
		out[i] = (char)(text[i] >= 'A' && text[i] <= 'Z' ? text[i] - 'A' + 'a' : text[i]);
	}
	out[i] = '\0';
}

/* `xwa.hangars` or `xwa.probes`: model keys to packages of `kind`. */
static int env_parse_models(const AeronConfigNode* map, const char* field, AeronEnvironmentKind kind,
							EnvModel* models, uint32_t* count, uint32_t max) {
	if (AeronConfigNode_Type(map) != AERON_CONFIG_MAP) {
		return env_node_error(map, "xwa: '%s' must be a map", field);
	}
	for (size_t i = 0; i < AeronConfigNode_MapCount(map); i++) {
		const char* key = AeronConfigNode_MapKeyAt(map, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(map, i);
		if (*count >= max) {
			return env_node_error(value, "xwa.%s: at most %u models are allowed", field, max);
		}
		EnvModel* model = &models[*count];
		if (!key[0] || strlen(key) >= sizeof model->key) {
			return env_node_error(value, "xwa.%s: invalid model key '%s'", field, key);
		}
		env_lower(model->key, sizeof model->key, key);
		for (uint32_t m = 0; m < *count; m++) {
			if (strcmp(models[m].key, model->key) == 0) {
				return env_node_error(value, "xwa.%s: duplicate model '%s'", field, key);
			}
		}
		char what[96];
		snprintf(what, sizeof what, "xwa.%s '%s'", field, key);
		if (!env_add_reference(value, what, kind, &model->reference)) {
			return 0;
		}
		(*count)++;
	}
	return 1;
}

static int env_parse_xwa(const AeronConfigNode* xwa) {
	static const char* const fields[] = { "hangar", "hangars", "probes", "missions" };
	if (!xwa || AeronConfigNode_Type(xwa) != AERON_CONFIG_MAP) {
		return env_node_error(xwa, "manifest: 'xwa' must be a map");
	}
	if (!env_check_fields(xwa, "xwa", fields, sizeof fields / sizeof fields[0])) {
		return 0;
	}
	const AeronConfigNode* hangar = AeronConfigNode_MapGet(xwa, "hangar");
	if (hangar &&
		!env_add_reference(hangar, "xwa.hangar", AERON_ENV_KIND_LIGHTING, &s_env.hangar_reference)) {
		return 0;
	}
	const AeronConfigNode* hangars = AeronConfigNode_MapGet(xwa, "hangars");
	if (hangars && !env_parse_models(hangars, "hangars", AERON_ENV_KIND_LIGHTING, s_env.hangars,
									 &s_env.hangar_count, ENV_MAX_HANGARS)) {
		return 0;
	}
	const AeronConfigNode* probes = AeronConfigNode_MapGet(xwa, "probes");
	if (probes && !env_parse_models(probes, "probes", AERON_ENV_KIND_PROBES, s_env.probes, &s_env.probe_count,
									ENV_MAX_PROBES)) {
		return 0;
	}
	const AeronConfigNode* missions = AeronConfigNode_MapGet(xwa, "missions");
	if (!missions) {
		return 1;
	}
	if (AeronConfigNode_Type(missions) != AERON_CONFIG_MAP) {
		return env_node_error(missions, "xwa: 'missions' must be a map");
	}
	for (size_t i = 0; i < AeronConfigNode_MapCount(missions); i++) {
		if (!env_parse_mission(AeronConfigNode_MapKeyAt(missions, i),
							   AeronConfigNode_MapValueAt(missions, i))) {
			return 0;
		}
	}
	return 1;
}

static int env_load_manifest(AeronVfs* vfs) {
	static const char* const fields[] = { "version", "packages", "xwa" };
	char vfs_path[300];
	snprintf(vfs_path, sizeof vfs_path, "remaster/%s", s_env.manifest_path);
	AeronConfigFile* manifest = NULL;
	AeronConfigError error;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_ASSET, vfs_path, &manifest, &error)) {
		Aeron_LogError("xwa.remaster", "environment manifest %s:%d: %s", vfs_path, error.line,
					   error.message[0] ? error.message : "unavailable");
		return 0;
	}
	const AeronConfigNode* root = AeronConfigFile_Root(manifest);
	int ok =
		AeronConfigNode_Type(root) == AERON_CONFIG_MAP ? 1 : env_node_error(root, "manifest must be a map");
	if (ok) {
		ok = env_check_fields(root, "manifest", fields, sizeof fields / sizeof fields[0]);
	}
	if (ok) {
		const AeronConfigNode* version = AeronConfigNode_MapGet(root, "version");
		if (AeronConfigNode_Type(version) != AERON_CONFIG_INT || AeronConfigNode_Int(version, 0) != 1) {
			ok = env_node_error(version ? version : root, "manifest: 'version' must be 1");
		}
	}
	if (ok &&
		!AeronEnvironment_ParsePackages(AeronConfigNode_MapGet(root, "packages"), &s_env.packages, &error)) {
		Aeron_LogError("xwa.remaster", "%s:%d: %s", error.path[0] ? error.path : vfs_path, error.line,
					   error.message);
		ok = 0;
	}
	if (ok) {
		ok = env_parse_xwa(AeronConfigNode_MapGet(root, "xwa"));
	}
	AeronConfigFile_Destroy(manifest);
	if (ok) {
		Aeron_LogInfo(
			"xwa.remaster",
			"environment manifest %s: %u packages, %u missions, %u hangar models, %u models with probes, "
			"hangar %s",
			vfs_path, AeronEnvironmentDefinitionSet_Count(s_env.packages), s_env.mission_count,
			s_env.hangar_count, s_env.probe_count,
			s_env.hangar_reference >= 0 ? s_env.references[s_env.hangar_reference].package : "(none)");
	}
	return ok;
}

int XwaRemasterEnvironment_Init(AeronVfs* vfs) {
	XwaRemasterEnvironment_Shutdown();
	s_env.enabled = s_env_options.enabled;
	memcpy(s_env.manifest_path, s_env_options.manifest, sizeof s_env.manifest_path);
	if (!s_env.enabled) {
		Aeron_LogInfo("xwa.remaster", "environment maps: disabled");
		return 1;
	}
	if (!s_env.manifest_path[0]) {
		Aeron_LogError("xwa.remaster",
					   "environment_maps.manifest is required when environment maps are enabled");
		XwaRemasterEnvironment_Shutdown();
		return 0;
	}
	if (!env_load_manifest(vfs)) {
		XwaRemasterEnvironment_Shutdown();
		return 0;
	}
	return 1;
}

static void env_clear_prepared(void) {
	s_env.prepared_key[0] = '\0';
	s_env.prepared_hangar = -1;
	s_env.prepared_hangar_reference = -1;
	for (int r = 0; r < ENV_MAX_REGIONS; r++) {
		s_env.prepared_region[r] = -1;
	}
	s_env.prepared_probes_mask = 0;
	for (int p = 0; p < ENV_MAX_PROBES; p++) {
		s_env.prepared_probes[p] = -1;
	}
}

void XwaRemasterEnvironment_Shutdown(void) {
	/* The library borrows the definitions. */
	AeronEnvironmentLibrary_Destroy(s_env.library);
	AeronEnvironmentDefinitionSet_Destroy(s_env.packages);
	memset(&s_env, 0, sizeof s_env);
	s_env.hangar_reference = -1;
	env_clear_prepared();
}

int XwaRemasterEnvironment_Enabled(void) { return s_env.enabled; }

void XwaRemasterEnvironment_SetAuthoredEnvironments(int enabled) { s_env_options.authored = enabled != 0; }

int XwaRemasterEnvironment_AuthoredEnvironments(void) { return s_env_options.authored; }

void XwaRemasterEnvironment_SetShowProbes(int show) { s_env_options.show_probes = show != 0; }

int XwaRemasterEnvironment_ShowProbes(void) { return s_env_options.show_probes; }

/* XWA world X/Y horizontal, +Z up: cube +X = world +X, cube +Y = world +Z,
 * cube +Z = world -Y (the flight sky's existing rotation). */
static const float env_base_world_to_cube[9] = {
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
};

int XwaRemasterEnvironment_PrepareProcess(AeronCommandBuffer* cmd) {
	if (!cmd) {
		return 0;
	}
	if (!s_env.enabled || s_env.library) {
		return 1;
	}
	AeronEnvironmentLibraryDesc desc;
	memset(&desc, 0, sizeof desc);
	desc.definitions = s_env.packages;
	desc.vfs = Aeron_GetVfs();
	desc.root = AERON_VFS_ROOT_ASSET;
	desc.path_prefix = "remaster/";
	desc.sun_max_radiance = s_env_options.sun_max_radiance;
	memcpy(desc.base_world_to_cube, env_base_world_to_cube, sizeof desc.base_world_to_cube);
	s_env.library = AeronEnvironmentLibrary_Create(&desc);
	if (!s_env.library) {
		Aeron_LogError("xwa.remaster",
					   "environment maps: package library creation failed; classic path only");
		return 1;
	}
#ifdef AERON_DEBUG_UI
	AeronEnvironmentLibrary_RegisterDebugTool(s_env.library, XwaRemasterDebugTools_EnvironmentRows, NULL);
#else
	AeronEnvironmentLibrary_RegisterDebugTool(s_env.library, NULL, NULL);
#endif
	env_clear_prepared();
	if (s_env.hangar_reference >= 0 &&
		AeronEnvironmentLibrary_Prepare(s_env.library, cmd, &s_env.references[s_env.hangar_reference], 1)) {
		s_env.prepared_hangar = 0;
		s_env.prepared_hangar_reference = s_env.hangar_reference;
	}
	return 1;
}

/* The `xwa.hangars` reference of the loaded hangar model, or -1. The hangar
 * model is the launch reference object's. */
static int32_t env_model_hangar_reference(const XwaSnapshot* snap) {
	if (!snap->flight_camera.in_hangar || s_env.hangar_count == 0) {
		return -1;
	}
	for (uint32_t i = 0; i < snap->flight_object_count; i++) {
		const XwaFlightObject* object = &snap->flight_objects[i];
		if ((int32_t)object->slot != snap->flight_camera.hangar_launch_ref_obj_idx) {
			continue;
		}
		const char* name = XwaSnapshotExport_ModelName(object->object_type);
		char key[XWA_SNAP_OPT_NAME_MAX];
		env_lower(key, sizeof key, name ? name : "");
		for (uint32_t h = 0; h < s_env.hangar_count; h++) {
			if (strcmp(s_env.hangars[h].key, key) == 0) {
				return s_env.hangars[h].reference;
			}
		}
		return -1;
	}
	return -1;
}

/* The hangar reference to prepare and use: the loaded hangar model's own,
 * else xwa.hangar. Outside the hangar, the one already prepared. */
static int32_t env_hangar_reference(const XwaSnapshot* snap) {
	const int32_t model = env_model_hangar_reference(snap);
	if (model >= 0) {
		return model;
	}
	if (!snap->flight_camera.in_hangar && s_env.prepared_hangar_reference >= 0) {
		return s_env.prepared_hangar_reference;
	}
	return s_env.hangar_reference;
}

/* The `xwa.probes` entries of the snapshot's loaded models, by bit. */
static uint64_t env_probes_mask(const XwaSnapshot* snap) {
	uint64_t mask = 0;
	for (uint32_t i = 0; s_env.probe_count && i < snap->opt_asset_count && i < XWA_SNAP_MAX_OPT_ASSETS; i++) {
		char key[XWA_SNAP_OPT_NAME_MAX];
		env_lower(key, sizeof key, snap->opt_assets[i].name);
		for (uint32_t p = 0; p < s_env.probe_count; p++) {
			if (strcmp(s_env.probes[p].key, key) == 0) {
				mask |= (uint64_t)1 << p;
			}
		}
	}
	return mask;
}

static int env_snapshot_key(const XwaSnapshot* snap, char* key, size_t size) {
	return snap && snap->mission_path[0] &&
		   XwaRemasterEnvironment_NormalizeMissionKey(snap->mission_path, key, size);
}

static const EnvMission* env_find_mission(const char* key);

int XwaRemasterEnvironment_NeedsPrepare(const XwaSnapshot* snap) {
	if (!s_env.library) {
		return 0;
	}
	if (AeronEnvironmentLibrary_ReloadRequested(s_env.library)) {
		return 1;
	}
	char key[XWA_ENV_MISSION_KEY_MAX];
	return (env_snapshot_key(snap, key, sizeof key) && strcmp(key, s_env.prepared_key) != 0) ||
		   (snap && (env_hangar_reference(snap) != s_env.prepared_hangar_reference ||
					 env_probes_mask(snap) != s_env.prepared_probes_mask));
}

void XwaRemasterEnvironment_Prepare(AeronCommandBuffer* cmd, const XwaSnapshot* snap) {
	if (!s_env.library || !cmd) {
		return;
	}
	if (!snap) {
		return;
	}
	char key[XWA_ENV_MISSION_KEY_MAX];
	const int has_key = env_snapshot_key(snap, key, sizeof key);
	const int32_t hangar_reference = env_hangar_reference(snap);
	const uint64_t probes_mask = env_probes_mask(snap);
	if ((!has_key || strcmp(key, s_env.prepared_key) == 0) &&
		hangar_reference == s_env.prepared_hangar_reference && probes_mask == s_env.prepared_probes_mask) {
		AeronEnvironmentLibrary_ServiceReload(s_env.library, cmd);
		return;
	}
	if (!has_key) {
		/* Only the hangar or the models changed: keep the prepared mission's regions. */
		snprintf(key, sizeof key, "%s", s_env.prepared_key);
	}
	/* The hangar, every mapped region, so a hyperspace region change never
	 * loads anything, and the probes of the loaded models. Textures shared
	 * by path are uploaded once. */
	AeronEnvironmentReference references[1 + ENV_MAX_REGIONS + ENV_MAX_PROBES];
	uint32_t count = 0;
	int32_t hangar = -1;
	int32_t regions[ENV_MAX_REGIONS] = { -1, -1, -1, -1 };
	int32_t probes[ENV_MAX_PROBES];
	if (hangar_reference >= 0) {
		hangar = (int32_t)count;
		references[count++] = s_env.references[hangar_reference];
	}
	const EnvMission* mission = env_find_mission(key);
	for (int r = 0; mission && r < ENV_MAX_REGIONS; r++) {
		if (mission->region_reference[r] >= 0) {
			regions[r] = (int32_t)count;
			references[count++] = s_env.references[mission->region_reference[r]];
		}
	}
	for (uint32_t p = 0; p < ENV_MAX_PROBES; p++) {
		probes[p] = -1;
		if (probes_mask & ((uint64_t)1 << p)) {
			probes[p] = (int32_t)count;
			references[count++] = s_env.references[s_env.probes[p].reference];
		}
	}
	/* Record the key even on failure: retrying every frame would not help. */
	env_clear_prepared();
	snprintf(s_env.prepared_key, sizeof s_env.prepared_key, "%s", key);
	s_env.prepared_hangar_reference = hangar_reference;
	s_env.prepared_probes_mask = probes_mask;
	if (!AeronEnvironmentLibrary_Prepare(s_env.library, cmd, references, count)) {
		Aeron_LogError("xwa.remaster", "environment maps: preparation failed for mission '%s'", key);
		return;
	}
	s_env.prepared_hangar = hangar;
	memcpy(s_env.prepared_region, regions, sizeof regions);
	memcpy(s_env.prepared_probes, probes, sizeof probes);
	Aeron_LogInfo("xwa.remaster", "environment maps: prepared mission '%s' (%u references)", key, count);
}

static const EnvMission* env_find_mission(const char* key) {
	for (uint32_t i = 0; i < s_env.mission_count; i++) {
		if (strcmp(s_env.missions[i].key, key) == 0) {
			return &s_env.missions[i];
		}
	}
	return NULL;
}

static void env_select(XwaEnvironmentSelection* out, XwaEnvironmentMode mode, XwaEnvironmentFallback fallback,
					   int32_t reference) {
	out->mode = mode;
	out->fallback = fallback;
	out->reference = reference >= 0 ? &s_env.references[reference] : NULL;
	out->package = NULL;
}

/* The prepared package at library index `prepared`, or NULL. */
static const AeronEnvironmentPackage* env_package(int32_t prepared) {
	return prepared >= 0 ? AeronEnvironmentLibrary_Find(s_env.library, (uint32_t)prepared) : NULL;
}

static void env_resolve(const XwaSnapshot* snap, XwaEnvironmentSelection* out) {
	const XwaFlightCamera* cam = &snap->flight_camera;
	out->region = cam->region;
	if (cam->hyperspace_phase) {
		env_select(out, XWA_ENV_MODE_HYPERSPACE, XWA_ENV_FALLBACK_NONE, -1);
		return;
	}
	if (!s_env.enabled) {
		env_select(out, XWA_ENV_MODE_DISABLED, XWA_ENV_FALLBACK_NONE, -1);
		return;
	}
	if (cam->death_star_mode) {
		env_select(out, XWA_ENV_MODE_DEATH_STAR, XWA_ENV_FALLBACK_NONE, -1);
		return;
	}
	if (cam->in_hangar) {
		const int32_t reference = env_hangar_reference(snap);
		if (reference < 0) {
			env_select(out, XWA_ENV_MODE_HANGAR, XWA_ENV_FALLBACK_NO_HANGAR_ENTRY, -1);
			return;
		}
		const AeronEnvironmentPackage* package =
			reference == s_env.prepared_hangar_reference ? env_package(s_env.prepared_hangar) : NULL;
		if (!package) {
			env_select(out, XWA_ENV_MODE_HANGAR, XWA_ENV_FALLBACK_NOT_READY, reference);
			return;
		}
		if (!s_env_options.authored) {
			env_select(out, XWA_ENV_MODE_HANGAR, XWA_ENV_FALLBACK_AUTHORED_OFF, reference);
			return;
		}
		env_select(out, XWA_ENV_MODE_HANGAR, XWA_ENV_FALLBACK_NONE, reference);
		out->package = package;
		return;
	}
	if (!snap->mission_path[0]) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_NO_MISSION, -1);
		return;
	}
	char key[XWA_ENV_MISSION_KEY_MAX];
	if (!XwaRemasterEnvironment_NormalizeMissionKey(snap->mission_path, key, sizeof key)) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_INVALID_MISSION, -1);
		return;
	}
	const EnvMission* mission = env_find_mission(key);
	if (!mission) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_NO_MISSION_ENTRY, -1);
		return;
	}
	const int32_t reference = cam->region < ENV_MAX_REGIONS ? mission->region_reference[cam->region] : -1;
	if (reference < 0) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_NO_REGION_ENTRY, -1);
		return;
	}
	const AeronEnvironmentPackage* package =
		strcmp(key, s_env.prepared_key) == 0 ? env_package(s_env.prepared_region[cam->region]) : NULL;
	if (!package) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_NOT_READY, reference);
		return;
	}
	if (!s_env_options.authored) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_AUTHORED_OFF, reference);
		return;
	}
	env_select(out, XWA_ENV_MODE_AUTHORED, XWA_ENV_FALLBACK_NONE, reference);
	out->package = package;
}

void XwaRemasterEnvironment_Resolve(const XwaSnapshot* snap, XwaEnvironmentSelection* out) {
	if (!snap || !out) {
		return;
	}
	env_resolve(snap, out);
	s_env.frame_set_count = 0;
	if (s_env.library) {
		int32_t active = -1;
		if (out->package && out->package == env_package(s_env.prepared_hangar)) {
			active = s_env.prepared_hangar;
		}
		for (int r = 0; r < ENV_MAX_REGIONS && out->package && active < 0; r++) {
			if (out->package == env_package(s_env.prepared_region[r])) {
				active = s_env.prepared_region[r];
			}
		}
		AeronEnvironmentLibrary_SetActive(s_env.library, active);
	}
	if (s_env.logged && out->mode == s_env.last.mode && out->fallback == s_env.last.fallback &&
		out->reference == s_env.last.reference && out->region == s_env.last.region &&
		strcmp(snap->mission_path, s_env.last_mission) == 0) {
		return;
	}
	s_env.logged = 1;
	s_env.last = *out;
	snprintf(s_env.last_mission, sizeof s_env.last_mission, "%s", snap->mission_path);
	Aeron_LogInfo("xwa.remaster", "environment: mission '%s' region %u -> %s%s%s%s%s", snap->mission_path,
				  (unsigned)out->region, XwaRemasterEnvironment_ModeName(out->mode),
				  out->reference ? " package " : "", out->reference ? out->reference->package : "",
				  out->fallback != XWA_ENV_FALLBACK_NONE ? ", fallback: " : "",
				  out->fallback != XWA_ENV_FALLBACK_NONE ? XwaRemasterEnvironment_FallbackName(out->fallback)
														 : "");
}

int XwaRemasterEnvironment_LastSelection(XwaEnvironmentSelection* out, char* mission, size_t mission_size) {
	if (!s_env.logged) {
		return 0;
	}
	if (out) {
		*out = s_env.last;
	}
	if (mission && mission_size) {
		snprintf(mission, mission_size, "%s", s_env.last_mission);
	}
	return 1;
}

const char* XwaRemasterEnvironment_PreparedMissionKey(void) { return s_env.prepared_key; }

const AeronEnvironmentPackage* XwaRemasterEnvironment_ModelProbes(const char* model_name) {
	if (!s_env.library || !s_env.prepared_probes_mask || !s_env_options.authored || !model_name) {
		return NULL;
	}
	char key[XWA_SNAP_OPT_NAME_MAX];
	env_lower(key, sizeof key, model_name);
	for (uint32_t p = 0; p < s_env.probe_count; p++) {
		if (s_env.prepared_probes[p] >= 0 && strcmp(s_env.probes[p].key, key) == 0) {
			return env_package(s_env.prepared_probes[p]);
		}
	}
	return NULL;
}

void XwaRemasterEnvironment_NoteProbeSet(const char* model_name, const AeronEnvironmentPackage* package,
										 uint16_t slot) {
	if (s_env.frame_set_count >= AERON_SCENE_MAX_PROBE_SETS || !package) {
		return;
	}
	XwaEnvironmentProbeSet* set = &s_env.frame_sets[s_env.frame_set_count++];
	snprintf(set->model, sizeof set->model, "%s", model_name ? model_name : "");
	snprintf(set->package, sizeof set->package, "%s", package->definition ? package->definition->name : "");
	set->slot = slot;
}

uint32_t XwaRemasterEnvironment_FrameProbeSets(const XwaEnvironmentProbeSet** out) {
	if (out) {
		*out = s_env.frame_sets;
	}
	return s_env.frame_set_count;
}

const char* XwaRemasterEnvironment_ModeName(XwaEnvironmentMode mode) {
	switch (mode) {
		case XWA_ENV_MODE_DISABLED:
			return "disabled";
		case XWA_ENV_MODE_HYPERSPACE:
			return "hyperspace";
		case XWA_ENV_MODE_DEATH_STAR:
			return "death star";
		case XWA_ENV_MODE_HANGAR:
			return "hangar";
		case XWA_ENV_MODE_AUTHORED:
			return "authored";
		case XWA_ENV_MODE_CLASSIC:
			return "classic";
	}
	return "unknown";
}

const char* XwaRemasterEnvironment_FallbackName(XwaEnvironmentFallback fallback) {
	switch (fallback) {
		case XWA_ENV_FALLBACK_NONE:
			return "none";
		case XWA_ENV_FALLBACK_NO_MISSION:
			return "no mission path";
		case XWA_ENV_FALLBACK_INVALID_MISSION:
			return "invalid mission path";
		case XWA_ENV_FALLBACK_NO_MISSION_ENTRY:
			return "mission not mapped";
		case XWA_ENV_FALLBACK_NO_REGION_ENTRY:
			return "region not mapped";
		case XWA_ENV_FALLBACK_NO_HANGAR_ENTRY:
			return "no hangar package";
		case XWA_ENV_FALLBACK_NOT_READY:
			return "package not ready";
		case XWA_ENV_FALLBACK_AUTHORED_OFF:
			return "authored environments switch off";
	}
	return "unknown";
}
