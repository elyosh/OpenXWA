#include "xwa_remaster/environment.h"

#include "aeron/log.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* XWA keeps four regions per mission. */
#define ENV_MAX_REGIONS 4
#define ENV_MAX_MISSIONS 128
#define ENV_MAX_REFERENCES (1 + ENV_MAX_MISSIONS * ENV_MAX_REGIONS)

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
	int32_t hangar_reference; /* -1 = none */
	EnvMission missions[ENV_MAX_MISSIONS];
	uint32_t mission_count;
	/* Last logged selection, to log changes only. */
	int logged;
	XwaEnvironmentSelection last;
	char last_mission[sizeof(((XwaSnapshot*)0)->mission_path)];
} s_env;

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
							  kind == AERON_ENV_KIND_FULL ? "full" : "lighting");
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

static int env_parse_xwa(const AeronConfigNode* xwa) {
	static const char* const fields[] = { "hangar", "missions" };
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
		Aeron_LogInfo("xwa.remaster", "environment manifest %s: %u packages, %u missions, hangar %s",
					  vfs_path, AeronEnvironmentDefinitionSet_Count(s_env.packages), s_env.mission_count,
					  s_env.hangar_reference >= 0 ? s_env.references[s_env.hangar_reference].package
												  : "(none)");
	}
	return ok;
}

int XwaRemasterEnvironment_Init(AeronVfs* vfs, const AeronConfigFile* config, const char* config_path) {
	XwaRemasterEnvironment_Shutdown();
	s_env.enabled = AeronConfigFile_GetBool(config, "environment_maps.enabled", 0);
	const char* manifest = AeronConfigFile_GetString(config, "environment_maps.manifest", "");
	if (!manifest[0] || strlen(manifest) >= sizeof s_env.manifest_path) {
		Aeron_LogError("xwa.remaster",
					   "%s: environment_maps.manifest must be non-empty and shorter than %zu bytes",
					   config_path, sizeof s_env.manifest_path);
		return 0;
	}
	snprintf(s_env.manifest_path, sizeof s_env.manifest_path, "%s", manifest);
	if (!s_env.enabled) {
		Aeron_LogInfo("xwa.remaster", "environment maps: disabled");
		return 1;
	}
	if (!env_load_manifest(vfs)) {
		XwaRemasterEnvironment_Shutdown();
		return 0;
	}
	return 1;
}

void XwaRemasterEnvironment_Shutdown(void) {
	AeronEnvironmentDefinitionSet_Destroy(s_env.packages);
	memset(&s_env, 0, sizeof s_env);
	s_env.hangar_reference = -1;
}

int XwaRemasterEnvironment_Enabled(void) { return s_env.enabled; }

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
		env_select(out, XWA_ENV_MODE_HANGAR,
				   s_env.hangar_reference >= 0 ? XWA_ENV_FALLBACK_NONE : XWA_ENV_FALLBACK_NO_HANGAR_ENTRY,
				   s_env.hangar_reference);
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
	env_select(out, XWA_ENV_MODE_AUTHORED, XWA_ENV_FALLBACK_NONE, reference);
}

void XwaRemasterEnvironment_Resolve(const XwaSnapshot* snap, XwaEnvironmentSelection* out) {
	if (!snap || !out) {
		return;
	}
	env_resolve(snap, out);
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
	}
	return "unknown";
}
