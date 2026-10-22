#include "xwa_remaster/environment.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa_remaster/debug_tools.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* XWA keeps four regions per mission. */
#define ENV_MAX_REGIONS 4
#define ENV_MAX_MISSIONS 128
#define ENV_MAX_REFERENCES (ENV_MAX_MISSIONS * ENV_MAX_REGIONS + 1) /* + xwa.default */
/* A region whose value is `none`. */
#define ENV_REFERENCE_NONE (-2)

typedef struct EnvMission {
	char key[XWA_ENV_MISSION_KEY_MAX];
	char source[XWA_ENV_MISSION_KEY_MAX];      /* manifest spelling, for diagnostics */
	int32_t region_reference[ENV_MAX_REGIONS]; /* index into references, -1 = no entry, ENV_REFERENCE_NONE */
} EnvMission;

typedef struct EnvState {
	int enabled;
	char manifest_path[256];
	AeronEnvironmentDefinitionSet* definitions;
	AeronEnvironmentReference references[ENV_MAX_REFERENCES];
	XwaEnvironmentBackdropRule backdrops[ENV_MAX_REFERENCES]; /* per reference */
	uint32_t reference_count;
	EnvMission missions[ENV_MAX_MISSIONS];
	uint32_t mission_count;
	/* xwa.default: index into references, -1 = none. */
	int32_t default_reference;
	/* Environment library and the prepared reference set: indices as passed to
	 * AeronEnvironmentLibrary_Prepare, -1 = not prepared. */
	AeronEnvironmentLibrary* library;
	char prepared_key[XWA_ENV_MISSION_KEY_MAX]; /* "" = no mission prepared */
	int32_t prepared_region[ENV_MAX_REGIONS];
	/* Last logged selection, to log changes only. */
	int logged;
	XwaEnvironmentSelection last;
	char last_mission[sizeof(((XwaSnapshot*)0)->mission_path)];
} EnvState;
static EnvState s_env;

/* Host options and the debug switch; survive Shutdown, which clears s_env. */
static struct {
	int authored;
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

static int env_list_has(const uint32_t* list, size_t count, uint32_t image) {
	for (size_t i = 0; i < count; i++) {
		if (list[i] == image) {
			return 1;
		}
	}
	return 0;
}

bool XwaRemasterEnvironment_KeepsBackdrops(const XwaEnvironmentSelection* selection) {
	return selection && selection->mode == XWA_ENV_MODE_AUTHORED && selection->prepared &&
		   selection->prepared->radiance && selection->backdrops &&
		   selection->backdrops->mode == XWA_ENV_BACKDROPS_KEEP;
}

bool XwaRemasterEnvironment_BackdropHidden(const XwaEnvironmentBackdropRule* rule, int group, int image) {
	if (!rule || rule->mode != XWA_ENV_BACKDROPS_KEEP || group < 0 || group > 0xFFFF || image < 0 ||
		image > 0xFFFF || rule->show_all) {
		return false;
	}
	const uint32_t key = (uint32_t)group << 16 | (uint32_t)image;
	if (env_list_has(rule->show, rule->show_count, key)) {
		return false;
	}
	return rule->hide_all || env_list_has(rule->hide, rule->hide_count, key);
}

/* Parses a "G-I" image: DAT group and image id, 0 to 65535 each. */
static int env_parse_image(const char* text, uint32_t* out) {
	char* end = NULL;
	if (text[0] < '0' || text[0] > '9') {
		return 0;
	}
	const long group = strtol(text, &end, 10);
	if (*end != '-' || end[1] < '0' || end[1] > '9') {
		return 0;
	}
	const long image = strtol(end + 1, &end, 10);
	if (*end || group > 0xFFFF || image > 0xFFFF) {
		return 0;
	}
	*out = (uint32_t)group << 16 | (uint32_t)image;
	return 1;
}

/* Reads `hide_backdrops` or `show_backdrops`: `all` or a sequence of "G-I"
 * images. */
static int env_read_backdrop_list(const AeronConfigNode* node, const char* what, const char* field,
								  bool* out_all, uint32_t* list, uint8_t* count) {
	if (AeronConfigNode_Type(node) == AERON_CONFIG_STRING &&
		strcmp(AeronConfigNode_String(node, ""), "all") == 0) {
		*out_all = true;
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE) {
		return env_node_error(node, "%s: '%s' must be 'all' or a list of \"group-image\" images", what,
							  field);
	}
	const size_t n = AeronConfigNode_SequenceCount(node);
	if (n > XWA_ENV_BACKDROP_LIST_MAX) {
		return env_node_error(node, "%s: '%s' lists more than %d images", what, field,
							  XWA_ENV_BACKDROP_LIST_MAX);
	}
	for (size_t i = 0; i < n; i++) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, i);
		const char* text =
			AeronConfigNode_Type(item) == AERON_CONFIG_STRING ? AeronConfigNode_String(item, "") : NULL;
		if (!text || !env_parse_image(text, &list[i])) {
			return env_node_error(
				item, "%s: '%s' has an image that is not \"group-image\" (for example \"6104-0\")", what,
				field);
		}
	}
	*count = (uint8_t)n;
	return 1;
}

/* Reads the backdrop fields of a reference map. */
static int env_read_backdrops(const AeronConfigNode* node, const char* what,
							  XwaEnvironmentBackdropRule* out) {
	const AeronConfigNode* mode = AeronConfigNode_MapGet(node, "backdrops");
	const AeronConfigNode* hide = AeronConfigNode_MapGet(node, "hide_backdrops");
	const AeronConfigNode* show = AeronConfigNode_MapGet(node, "show_backdrops");
	const AeronConfigNode* lights = AeronConfigNode_MapGet(node, "hidden_backdrop_lights");
	const AeronConfigNode* scale = AeronConfigNode_MapGet(node, "backdrop_light_scale");
	if (mode) {
		const char* value =
			AeronConfigNode_Type(mode) == AERON_CONFIG_STRING ? AeronConfigNode_String(mode, "") : "";
		if (strcmp(value, "keep") == 0) {
			out->mode = XWA_ENV_BACKDROPS_KEEP;
		} else if (strcmp(value, "replace") != 0) {
			return env_node_error(mode, "%s: 'backdrops' must be 'replace' or 'keep'", what);
		}
	}
	if (out->mode != XWA_ENV_BACKDROPS_KEEP) {
		static const char* const names[] = { "hide_backdrops", "show_backdrops", "hidden_backdrop_lights",
											 "backdrop_light_scale" };
		const AeronConfigNode* const nodes[] = { hide, show, lights, scale };
		for (int i = 0; i < 4; i++) {
			if (nodes[i]) {
				return env_node_error(nodes[i], "%s: '%s' needs 'backdrops: keep'", what, names[i]);
			}
		}
	}
	if (lights) {
		const char* value =
			AeronConfigNode_Type(lights) == AERON_CONFIG_STRING ? AeronConfigNode_String(lights, "") : "";
		if (strcmp(value, "remove") == 0) {
			out->remove_hidden_lights = true;
		} else if (strcmp(value, "keep") != 0) {
			return env_node_error(lights, "%s: 'hidden_backdrop_lights' must be 'keep' or 'remove'", what);
		}
	}
	if (scale) {
		const AeronConfigNodeType type = AeronConfigNode_Type(scale);
		const double value = AeronConfigNode_Float(scale, -1.0);
		if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) || !(value >= 0.0 && value <= 4.0)) {
			return env_node_error(scale, "%s: 'backdrop_light_scale' must be a number from 0 to 4", what);
		}
		out->light_scale = (float)value;
	}
	return (!hide || env_read_backdrop_list(hide, what, "hide_backdrops", &out->hide_all, out->hide,
											&out->hide_count)) &&
		   (!show || env_read_backdrop_list(show, what, "show_backdrops", &out->show_all, out->show,
											&out->show_count));
}

/* Adds a reference to an environment. A reference is either an environment
 * name or a map with `environment`, an optional `rotation_degrees` and the
 * optional backdrop fields. */
static int env_add_reference(const AeronConfigNode* node, const char* what, int32_t* out_index) {
	static const char* const fields[] = { "environment",         "rotation_degrees", "backdrops",
										  "hide_backdrops",      "show_backdrops",   "hidden_backdrop_lights",
										  "backdrop_light_scale" };
	const AeronConfigNode* name_node = node;
	const AeronConfigNode* rotation_node = NULL;
	XwaEnvironmentBackdropRule backdrops;
	memset(&backdrops, 0, sizeof backdrops);
	backdrops.light_scale = 1.0f;
	if (AeronConfigNode_Type(node) == AERON_CONFIG_MAP) {
		if (!env_check_fields(node, what, fields, sizeof fields / sizeof fields[0])) {
			return 0;
		}
		name_node = AeronConfigNode_MapGet(node, "environment");
		if (!name_node) {
			return env_node_error(node, "%s: missing required field 'environment'", what);
		}
		rotation_node = AeronConfigNode_MapGet(node, "rotation_degrees");
		if (!env_read_backdrops(node, what, &backdrops)) {
			return 0;
		}
	}
	if (AeronConfigNode_Type(name_node) != AERON_CONFIG_STRING) {
		return env_node_error(name_node, "%s: must name an environment", what);
	}
	const char* name = AeronConfigNode_String(name_node, "");
	const AeronEnvironmentDefinition* definition =
		AeronEnvironmentDefinitionSet_Find(s_env.definitions, name);
	if (!definition) {
		return env_node_error(name_node, "%s: unknown environment '%s'", what, name);
	}
	AeronEnvironmentReference* reference = &s_env.references[s_env.reference_count];
	memset(reference, 0, sizeof *reference);
	snprintf(reference->environment, sizeof reference->environment, "%s", name);
	if (rotation_node) {
		if (!env_read_rotation(rotation_node, what, reference->rotation_degrees)) {
			return 0;
		}
		reference->has_rotation = true;
	}
	s_env.backdrops[s_env.reference_count] = backdrops;
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
		/* A bare `none` means no environment. { environment: none } names an
		 * environment called `none`. */
		if (AeronConfigNode_Type(value) == AERON_CONFIG_STRING &&
			strcmp(AeronConfigNode_String(value, ""), "none") == 0) {
			mission->region_reference[region] = ENV_REFERENCE_NONE;
			continue;
		}
		if (!env_add_reference(value, region_what, &mission->region_reference[region])) {
			return 0;
		}
	}
	s_env.mission_count++;
	return 1;
}

static int env_parse_xwa(const AeronConfigNode* xwa) {
	static const char* const fields[] = { "missions", "default" };
	if (!xwa || AeronConfigNode_Type(xwa) != AERON_CONFIG_MAP) {
		return env_node_error(xwa, "manifest: 'xwa' must be a map");
	}
	if (!env_check_fields(xwa, "xwa", fields, sizeof fields / sizeof fields[0])) {
		return 0;
	}
	const AeronConfigNode* default_node = AeronConfigNode_MapGet(xwa, "default");
	if (default_node && AeronConfigNode_Type(default_node) == AERON_CONFIG_STRING &&
		strcmp(AeronConfigNode_String(default_node, ""), "none") == 0) {
		return env_node_error(default_node,
							  "xwa.default: 'none' is for regions; leave the default out instead");
	}
	if (default_node && !env_add_reference(default_node, "xwa.default", &s_env.default_reference)) {
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

/* Creates an empty set of definitions, for a missing manifest. */
static int env_empty_definitions(void) {
	AeronConfigFile* empty = NULL;
	AeronConfigError error;
	memset(&error, 0, sizeof error);
	const int ok = AeronConfigFile_CreateMap(AERON_VFS_ROOT_MOD, s_env.manifest_path, &empty, &error) &&
				   AeronEnvironment_ParseDefinitions(AeronConfigFile_Root(empty), &s_env.definitions, &error);
	AeronConfigFile_Destroy(empty);
	if (!ok) {
		Aeron_LogError("xwa.remaster", "environment maps: %s",
					   error.message[0] ? error.message : "out of memory");
	}
	return ok;
}

/* Returns 1 when the manifest loads or does not exist, 0 on error. */
static int env_load_manifest(AeronVfs* vfs) {
	static const char* const fields[] = { "version", "environments", "xwa" };
	const char* vfs_path = s_env.manifest_path;
	s_env.default_reference = -1;
	AeronConfigFile* manifest = NULL;
	AeronConfigError error;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_MOD, vfs_path, &manifest, &error)) {
		if (error.code == AERON_CONFIG_ERROR_NOT_FOUND) {
			/* The mod folder is optional. Without a manifest there are no
			 * authored environments: every region uses the classic path,
			 * and captured lighting still applies. */
			Aeron_LogInfo("xwa.remaster", "environment maps: no manifest at %s, no authored environments",
						  vfs_path);
			return env_empty_definitions();
		}
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
	if (ok && !AeronEnvironment_ParseDefinitions(AeronConfigNode_MapGet(root, "environments"),
												 &s_env.definitions, &error)) {
		Aeron_LogError("xwa.remaster", "%s:%d: %s", error.path[0] ? error.path : vfs_path, error.line,
					   error.message);
		ok = 0;
	}
	if (ok) {
		ok = env_parse_xwa(AeronConfigNode_MapGet(root, "xwa"));
	}
	AeronConfigFile_Destroy(manifest);
	if (ok) {
		Aeron_LogInfo("xwa.remaster", "environment manifest %s: %u environments, %u missions", vfs_path,
					  AeronEnvironmentDefinitionSet_Count(s_env.definitions), s_env.mission_count);
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
	for (int r = 0; r < ENV_MAX_REGIONS; r++) {
		s_env.prepared_region[r] = -1;
	}
}

void XwaRemasterEnvironment_Shutdown(void) {
	/* The library borrows the definitions. */
	AeronEnvironmentLibrary_Destroy(s_env.library);
	AeronEnvironmentDefinitionSet_Destroy(s_env.definitions);
	memset(&s_env, 0, sizeof s_env);
	env_clear_prepared();
}

int XwaRemasterEnvironment_Enabled(void) { return s_env.enabled; }

void XwaRemasterEnvironment_SetAuthoredEnvironments(int enabled) { s_env_options.authored = enabled != 0; }

int XwaRemasterEnvironment_AuthoredEnvironments(void) { return s_env_options.authored; }

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
	desc.definitions = s_env.definitions;
	desc.vfs = Aeron_GetVfs();
	desc.root = AERON_VFS_ROOT_MOD;
	desc.sun_max_radiance = s_env_options.sun_max_radiance;
	memcpy(desc.base_world_to_cube, env_base_world_to_cube, sizeof desc.base_world_to_cube);
	s_env.library = AeronEnvironmentLibrary_Create(&desc);
	if (!s_env.library) {
		Aeron_LogError("xwa.remaster",
					   "environment maps: environment library creation failed; classic path only");
		return 1;
	}
#ifdef AERON_DEBUG_UI
	AeronEnvironmentLibrary_RegisterDebugTool(s_env.library, XwaRemasterDebugTools_EnvironmentRows, NULL);
#else
	AeronEnvironmentLibrary_RegisterDebugTool(s_env.library, NULL, NULL);
#endif
	env_clear_prepared();
	return 1;
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
	return env_snapshot_key(snap, key, sizeof key) && strcmp(key, s_env.prepared_key) != 0;
}

/* Prepares the mission's regions when the mission key changed. With
 * `force`, prepares them in any case, and a snapshot without a mission key
 * prepares the last prepared mission again. */
static void env_prepare(AeronCommandBuffer* cmd, const XwaSnapshot* snap, int force) {
	char key[XWA_ENV_MISSION_KEY_MAX];
	const int has_key = env_snapshot_key(snap, key, sizeof key);
	if (!force && (!has_key || strcmp(key, s_env.prepared_key) == 0)) {
		return;
	}
	if (!has_key) {
		snprintf(key, sizeof key, "%s", s_env.prepared_key);
	}
	/* Prepare all regions, so a hyperspace jump to another region loads
	 * nothing. Regions without an entry share the default environment.
	 * Textures with the same path are uploaded once. */
	AeronEnvironmentReference references[ENV_MAX_REGIONS];
	uint32_t count = 0;
	int32_t regions[ENV_MAX_REGIONS] = { -1, -1, -1, -1 };
	int32_t default_index = -1;
	const EnvMission* mission = env_find_mission(key);
	for (int r = 0; r < ENV_MAX_REGIONS; r++) {
		if (mission && mission->region_reference[r] == ENV_REFERENCE_NONE) {
			continue;
		}
		if (mission && mission->region_reference[r] >= 0) {
			regions[r] = (int32_t)count;
			references[count++] = s_env.references[mission->region_reference[r]];
		} else if (s_env.default_reference >= 0) {
			if (default_index < 0) {
				default_index = (int32_t)count;
				references[count++] = s_env.references[s_env.default_reference];
			}
			regions[r] = default_index;
		}
	}
	/* Record the key even on failure: retrying every frame would not help. */
	env_clear_prepared();
	snprintf(s_env.prepared_key, sizeof s_env.prepared_key, "%s", key);
	if (!AeronEnvironmentLibrary_Prepare(s_env.library, cmd, references, count)) {
		Aeron_LogError("xwa.remaster", "environment maps: preparation failed for mission '%s'", key);
		return;
	}
	memcpy(s_env.prepared_region, regions, sizeof regions);
	Aeron_LogInfo("xwa.remaster", "environment maps: prepared mission '%s' (%u references)", key, count);
}

/* Reads the manifest again into a new state. If the manifest has errors,
 * keeps the loaded state and returns 0. */
static int env_reload_manifest(void) {
	static EnvState loaded;
	loaded = s_env;
	s_env.definitions = NULL;
	s_env.reference_count = 0;
	s_env.mission_count = 0;
	if (env_load_manifest(Aeron_GetVfs())) {
		return 1;
	}
	AeronEnvironmentDefinitionSet_Destroy(s_env.definitions);
	s_env = loaded;
	Aeron_LogError("xwa.remaster", "environment reload: the manifest has errors; keeping the loaded one");
	return 0;
}

void XwaRemasterEnvironment_Prepare(AeronCommandBuffer* cmd, const XwaSnapshot* snap) {
	if (!s_env.library || !cmd || !snap) {
		return;
	}
	if (!AeronEnvironmentLibrary_ReloadRequested(s_env.library)) {
		env_prepare(cmd, snap, 0);
		return;
	}
	/* Reload: also read the environment definitions again, so a new bake
	 * loads without a restart. The textures of the current references are
	 * read again, then everything is prepared from the new definitions,
	 * which can map other environments. */
	AeronEnvironmentDefinitionSet* previous = s_env.definitions;
	const int reread = env_reload_manifest();
	if (reread) {
		AeronEnvironmentLibrary_SetDefinitions(s_env.library, s_env.definitions);
	}
	AeronEnvironmentLibrary_ServiceReload(s_env.library, cmd);
	env_prepare(cmd, snap, reread);
	if (reread) {
		/* The prepared environments now point into the new set. */
		AeronEnvironmentDefinitionSet_Destroy(previous);
	}
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
	out->prepared = NULL;
	out->is_default = reference >= 0 && reference == s_env.default_reference;
	out->backdrops = reference >= 0 ? &s_env.backdrops[reference] : NULL;
}

/* The prepared environment at library index `prepared`, or NULL. */
static const AeronPreparedEnvironment* env_prepared(int32_t prepared) {
	return prepared >= 0 ? AeronEnvironmentLibrary_Find(s_env.library, (uint32_t)prepared) : NULL;
}

/* Selects the environment of the mission region, or the classic path with
 * the reason. A region without an entry uses the default environment when
 * `use_default` is set and there is one. */
static void env_resolve_region(const XwaSnapshot* snap, XwaEnvironmentSelection* out, int use_default) {
	const XwaFlightCamera* cam = &snap->flight_camera;
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
	const uint8_t region = cam->mission_region;
	if (region >= ENV_MAX_REGIONS) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_NO_REGION_ENTRY, -1);
		return;
	}
	int32_t reference = mission ? mission->region_reference[region] : -1;
	if (reference == ENV_REFERENCE_NONE) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_REGION_NONE, -1);
		return;
	}
	if (reference < 0 && use_default) {
		reference = s_env.default_reference;
	}
	if (reference < 0) {
		env_select(out, XWA_ENV_MODE_CLASSIC,
				   mission ? XWA_ENV_FALLBACK_NO_REGION_ENTRY : XWA_ENV_FALLBACK_NO_MISSION_ENTRY, -1);
		return;
	}
	const AeronPreparedEnvironment* prepared =
		strcmp(key, s_env.prepared_key) == 0 ? env_prepared(s_env.prepared_region[region]) : NULL;
	if (!prepared) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_NOT_READY, reference);
		return;
	}
	if (!s_env_options.authored) {
		env_select(out, XWA_ENV_MODE_CLASSIC, XWA_ENV_FALLBACK_AUTHORED_OFF, reference);
		return;
	}
	env_select(out, XWA_ENV_MODE_AUTHORED, XWA_ENV_FALLBACK_NONE, reference);
	out->prepared = prepared;
}

static void env_resolve(const XwaSnapshot* snap, XwaEnvironmentSelection* out) {
	const XwaFlightCamera* cam = &snap->flight_camera;
	out->region = cam->mission_region;
	if (cam->hyperspace_phase) {
		env_select(out, XWA_ENV_MODE_HYPERSPACE, XWA_ENV_FALLBACK_NONE, -1);
		return;
	}
	if (!s_env.enabled) {
		env_select(out, XWA_ENV_MODE_DISABLED, XWA_ENV_FALLBACK_NONE, -1);
		return;
	}
	/* A Death Star region uses its mapped environment like any other; without
	 * one it keeps the Death Star behaviour, with the reason. The default
	 * environment does not apply there. */
	if (cam->death_star_mode) {
		env_resolve_region(snap, out, 0);
		if (out->mode == XWA_ENV_MODE_CLASSIC) {
			out->mode = XWA_ENV_MODE_DEATH_STAR;
		}
		return;
	}
	env_resolve_region(snap, out, 1);
}

void XwaRemasterEnvironment_Resolve(const XwaSnapshot* snap, XwaEnvironmentSelection* out) {
	if (!snap || !out) {
		return;
	}
	env_resolve(snap, out);
	if (s_env.library) {
		int32_t active = -1;
		for (int r = 0; r < ENV_MAX_REGIONS && out->prepared && active < 0; r++) {
			if (out->prepared == env_prepared(s_env.prepared_region[r])) {
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
	Aeron_LogInfo("xwa.remaster", "environment: mission '%s' region %u -> %s%s%s%s%s%s%s", snap->mission_path,
				  (unsigned)out->region, XwaRemasterEnvironment_ModeName(out->mode),
				  out->reference ? " environment " : "", out->reference ? out->reference->environment : "",
				  out->is_default ? " (default)" : "",
				  out->backdrops && out->backdrops->mode == XWA_ENV_BACKDROPS_KEEP ? ", backdrops kept" : "",
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

const char* XwaRemasterEnvironment_ModeName(XwaEnvironmentMode mode) {
	switch (mode) {
		case XWA_ENV_MODE_DISABLED:
			return "disabled";
		case XWA_ENV_MODE_HYPERSPACE:
			return "hyperspace";
		case XWA_ENV_MODE_DEATH_STAR:
			return "death star";
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
		case XWA_ENV_FALLBACK_NOT_READY:
			return "environment not ready";
		case XWA_ENV_FALLBACK_AUTHORED_OFF:
			return "authored environments switch off";
		case XWA_ENV_FALLBACK_REGION_NONE:
			return "region set to none";
	}
	return "unknown";
}
