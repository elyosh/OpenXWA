#include "xwa_remaster/opt_mesh.h"

#include "aeron/aeron.h"
#include "aeron/asset/opt_model.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint64_t kRuntimeOptMaxBytes = 64u * 1024u * 1024u;

static void opt_mesh_error(char* error, size_t error_size, const char* message) {
	if (error && error_size)
		snprintf(error, error_size, "%s", message ? message : "OPT load failed");
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
	uint8_t* bytes = NULL;
	size_t size = 0;
	char path[512];
	if (!opt_read_model(vfs, basename, path, sizeof path, &bytes, &size)) {
		opt_mesh_error(error, error_size, "original OPT not found or unreadable");
		return false;
	}
	AeronOptModelError build_error = { 0 };
	const bool built = Aeron_OptModelBuildMemory(
			bytes, size, path,
			&(AeronOptModelBuildOptions) {
				.vertex_scale = 1.0f,
				.smooth_angle_degrees = smooth_angle_degrees,
				.emissive_strength = emissive_strength,
				.emissive = true,
			},
			out, &build_error);
	free(bytes);
	if (!built)
		opt_mesh_error(error, error_size,
				build_error.message[0] ? build_error.message : "OPT conversion failed");
	return built;
}
