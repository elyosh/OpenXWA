#include "xwa_remaster/opt_mesh.h"

#include "aeron/aeron.h"
#include "aeron/asset/opt_model.h"
#include "aeron/config_file.h"
#include "aeron/log.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint64_t kRuntimeOptMaxBytes = 64u * 1024u * 1024u;

#define OPT_ALPHA_OVERRIDE_MAX 256

typedef struct OptAlphaOverrideEntry {
	char model[512];
	char texture[64];
	AeronGltfAlphaMode mode;
	float cutoff;
} OptAlphaOverrideEntry;

static struct {
	int loaded;
	int valid;
	size_t count;
	OptAlphaOverrideEntry entries[OPT_ALPHA_OVERRIDE_MAX];
} s_alpha_overrides;

static void opt_mesh_error(char* error, size_t error_size, const char* message) {
	if (error && error_size)
		snprintf(error, error_size, "%s", message ? message : "OPT load failed");
}

static void opt_normalize_path(char* destination, size_t capacity,
							   const char* source) {
	if (!destination || capacity == 0) return;
	size_t out = 0;
	for (; source && *source && out + 1 < capacity; ++source) {
		char value = *source == '\\' ? '/' : *source;
		if (value >= 'a' && value <= 'z') value = (char)(value - ('a' - 'A'));
		destination[out++] = value;
	}
	destination[out] = '\0';
}

static int opt_parse_alpha_mode(const char* value, AeronGltfAlphaMode* mode) {
	if (!value || !mode) return 0;
	if (strcmp(value, "opaque") == 0) {
		*mode = AERON_GLTF_ALPHA_OPAQUE;
	} else if (strcmp(value, "mask") == 0) {
		*mode = AERON_GLTF_ALPHA_MASK;
	} else if (strcmp(value, "blend") == 0) {
		*mode = AERON_GLTF_ALPHA_BLEND;
	} else {
		return 0;
	}
	return 1;
}

static int opt_load_alpha_overrides(AeronVfs* vfs, char* error,
									size_t error_size) {
	if (s_alpha_overrides.loaded) {
		if (!s_alpha_overrides.valid)
			opt_mesh_error(error, error_size, "invalid OPT alpha override database");
		return s_alpha_overrides.valid;
	}
	s_alpha_overrides.loaded = 1;

	static const char* path = "remaster/opt_alpha_overrides.yaml";
	AeronConfigFile* config = NULL;
	if (!AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_RESOURCE, path, &config)) {
		opt_mesh_error(error, error_size, "OPT alpha override database is unavailable or invalid");
		return 0;
	}
	const AeronConfigNode* root = AeronConfigFile_Root(config);
	const AeronConfigNode* version = AeronConfigNode_MapGet(root, "version");
	const AeronConfigNode* materials = AeronConfigNode_MapGet(root, "materials");
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Type(version) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(version, 0) != 1 ||
		AeronConfigNode_Type(materials) != AERON_CONFIG_SEQUENCE ||
		AeronConfigNode_SequenceCount(materials) > OPT_ALPHA_OVERRIDE_MAX) {
		AeronConfigFile_Destroy(config);
		opt_mesh_error(error, error_size, "invalid OPT alpha override database schema");
		return 0;
	}

	const size_t count = AeronConfigNode_SequenceCount(materials);
	for (size_t index = 0; index < count; ++index) {
		const AeronConfigNode* node = AeronConfigNode_SequenceGet(materials, index);
		const AeronConfigNode* model_node = AeronConfigNode_MapGet(node, "model");
		const AeronConfigNode* texture_node = AeronConfigNode_MapGet(node, "texture");
		const AeronConfigNode* mode_node = AeronConfigNode_MapGet(node, "mode");
		const AeronConfigNode* cutoff_node = AeronConfigNode_MapGet(node, "alpha_cutoff");
		const char* model = AeronConfigNode_String(model_node, NULL);
		const char* texture = AeronConfigNode_String(texture_node, NULL);
		const char* mode_name = AeronConfigNode_String(mode_node, NULL);
		const AeronConfigNodeType cutoff_type = AeronConfigNode_Type(cutoff_node);
		OptAlphaOverrideEntry* entry = &s_alpha_overrides.entries[index];
		if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP || !model || !model[0] ||
			!texture || !texture[0] || !opt_parse_alpha_mode(mode_name, &entry->mode) ||
			(cutoff_type != AERON_CONFIG_NULL && cutoff_type != AERON_CONFIG_FLOAT &&
			 cutoff_type != AERON_CONFIG_INT)) {
			AeronConfigFile_Destroy(config);
			opt_mesh_error(error, error_size, "invalid OPT alpha override entry");
			return 0;
		}
		entry->cutoff = (float)AeronConfigNode_Float(cutoff_node, 0.5);
		if (!isfinite(entry->cutoff) || entry->cutoff < 0.0f ||
			entry->cutoff > 1.0f || strlen(model) >= sizeof entry->model ||
			strlen(texture) >= sizeof entry->texture) {
			AeronConfigFile_Destroy(config);
			opt_mesh_error(error, error_size, "invalid OPT alpha override value");
			return 0;
		}
		opt_normalize_path(entry->model, sizeof entry->model, model);
		opt_normalize_path(entry->texture, sizeof entry->texture, texture);
		for (size_t previous = 0; previous < index; ++previous) {
			const OptAlphaOverrideEntry* other = &s_alpha_overrides.entries[previous];
			if (strcmp(entry->model, other->model) == 0 &&
				strcmp(entry->texture, other->texture) == 0) {
				AeronConfigFile_Destroy(config);
				opt_mesh_error(error, error_size, "duplicate OPT alpha override");
				return 0;
			}
		}
	}
	s_alpha_overrides.count = count;
	s_alpha_overrides.valid = 1;
	AeronConfigFile_Destroy(config);
	return 1;
}

bool XwaRemasterOptMesh_Init(AeronVfs* vfs, char* error, size_t error_size) {
	if (error && error_size) error[0] = '\0';
	if (!vfs) {
		opt_mesh_error(error, error_size, "invalid OPT mesh initialization arguments");
		return false;
	}
	return opt_load_alpha_overrides(vfs, error, error_size) != 0;
}

static size_t opt_resolve_alpha_overrides(const char* model_path,
									  AeronOptAlphaOverride* output,
									  size_t capacity) {
	char normalized[512];
	opt_normalize_path(normalized, sizeof normalized, model_path);
	size_t count = 0;
	for (size_t index = 0; index < s_alpha_overrides.count; ++index) {
		const OptAlphaOverrideEntry* entry = &s_alpha_overrides.entries[index];
		if (strcmp(normalized, entry->model) != 0) continue;
		if (count >= capacity) break;
		output[count].texture_name = entry->texture;
		output[count].alpha_mode = entry->mode;
		output[count].alpha_cutoff = entry->cutoff;
		Aeron_LogDebug("xwa.remaster", "OPT alpha override: %s %s mode=%d cutoff=%.3g",
					   normalized, entry->texture, (int)entry->mode,
					   (double)entry->cutoff);
		count++;
	}
	return count;
}

static int opt_read_model(AeronVfs* vfs, const char* basename, char* resolved,
						  size_t resolved_size, uint8_t** out_bytes, size_t* out_size) {
	char path[512];
	snprintf(path, sizeof path, "FLIGHTMODELS/%s.OPT", basename);
	if (AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, path,
			(size_t)kRuntimeOptMaxBytes, out_bytes, out_size)) {
		snprintf(resolved, resolved_size, "%s", path);
		return 1;
	}
	char uppercase[256];
	size_t index = 0;
	for (; basename[index] && index + 1 < sizeof uppercase; ++index) {
		const char value = basename[index];
		uppercase[index] = value >= 'a' && value <= 'z'
				? (char)(value - ('a' - 'A')) : value;
	}
	uppercase[index] = '\0';
	snprintf(path, sizeof path, "FLIGHTMODELS/%s.OPT", uppercase);
	if (!AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, path,
			(size_t)kRuntimeOptMaxBytes, out_bytes, out_size))
		return 0;
	snprintf(resolved, resolved_size, "%s", path);
	return 1;
}

bool XwaRemasterOptMesh_Build(AeronVfs* vfs, const char* basename,
		float smooth_angle_degrees, float emissive_strength,
		AeronGltfModel* out, char* error, size_t error_size) {
	if (out) memset(out, 0, sizeof *out);
	if (error && error_size) error[0] = '\0';
	if (!vfs || !basename || !basename[0] || !out) {
		opt_mesh_error(error, error_size, "invalid OPT build arguments");
		return false;
	}
	if (!s_alpha_overrides.loaded || !s_alpha_overrides.valid) {
		opt_mesh_error(error, error_size, "OPT mesh asset policy is not initialized");
		return false;
	}
	uint8_t* bytes = NULL;
	size_t size = 0;
	char path[512];
	if (!opt_read_model(vfs, basename, path, sizeof path, &bytes, &size)) {
		opt_mesh_error(error, error_size, "original OPT not found or unreadable");
		return false;
	}
	AeronOptModelError build_error = { 0 };
	AeronOptAlphaOverride alpha_overrides[OPT_ALPHA_OVERRIDE_MAX];
	const size_t alpha_override_count = opt_resolve_alpha_overrides(
		path, alpha_overrides, OPT_ALPHA_OVERRIDE_MAX);
	const bool built = Aeron_OptModelBuildMemory(
			bytes, size, path,
			&(AeronOptModelBuildOptions) {
				.vertex_scale = 1.0f,
				.smooth_angle_degrees = smooth_angle_degrees,
				.emissive_strength = emissive_strength,
				.emissive = true,
				.alpha_overrides = alpha_overrides,
				.alpha_override_count = alpha_override_count,
			},
			out, &build_error);
	free(bytes);
	if (!built)
		opt_mesh_error(error, error_size,
				build_error.message[0] ? build_error.message : "OPT conversion failed");
	return built;
}
