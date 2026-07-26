#include "xwa_remaster/opt_mesh.h"

#include "aeron/aeron.h"
#include "gltf_cook.h"
#include "opt2gltf.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint64_t kRuntimeOptMaxBytes = 64u * 1024u * 1024u;

typedef struct OptCookContext {
	OptGltfDocument* document;
	AeronGltfModel* model;
	const char* label;
} OptCookContext;

static void opt_mesh_error(char* error, size_t error_size, const char* message) {
	if (error && error_size) snprintf(error, error_size, "%s", message ? message : "OPT load failed");
}

static bool opt_image_provider(void* context, const cgltf_image* image,
							   AeronGltfCookImageView* out_view) {
	OptCookContext* cook = (OptCookContext*)context;
	OptGltfImageView image_view;
	if (!OptGltf_ImageView(cook->document, image, &image_view)) return false;
	out_view->rgba = image_view.rgba;
	out_view->width = (int)image_view.width;
	out_view->height = (int)image_view.height;
	return true;
}

static bool opt_model_consumer(void* context, const cgltf_data* cooked_data) {
	OptCookContext* cook = (OptCookContext*)context;
	return Aeron_GltfMeshBuildData(cooked_data, cook->label, cook->model);
}

static int opt_read_model(AeronVfs* vfs, const char* basename, char* resolved,
						  size_t resolved_size, uint8_t** out_bytes, size_t* out_size) {
	char path[512];
	snprintf(path, sizeof path, "FLIGHTMODELS/%s.OPT", basename);
	if (AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, path, (size_t)kRuntimeOptMaxBytes, out_bytes,
					 out_size)) {
		snprintf(resolved, resolved_size, "%s", path);
		return 1;
	}
	char uppercase[256];
	size_t i = 0;
	for (; basename[i] && i + 1 < sizeof uppercase; ++i) {
		const char c = basename[i];
		uppercase[i] = c >= 'a' && c <= 'z' ? (char)(c - ('a' - 'A')) : c;
	}
	uppercase[i] = '\0';
	snprintf(path, sizeof path, "FLIGHTMODELS/%s.OPT", uppercase);
	if (!AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, path, (size_t)kRuntimeOptMaxBytes, out_bytes,
					  out_size)) return 0;
	snprintf(resolved, resolved_size, "%s", path);
	return 1;
}

bool XwaRemasterOptMesh_Build(AeronVfs* vfs, const char* basename,
							  float smooth_angle_degrees, float emissive_strength,
							  AeronGltfModel* out, char* error, size_t error_size) {
	if (out) memset(out, 0, sizeof *out);
	if (error && error_size) error[0] = '\0';
	if (!vfs || !basename || !basename[0] || !out || !isfinite(emissive_strength) ||
		emissive_strength < 0.0f) {
		opt_mesh_error(error, error_size, "invalid OPT build arguments");
		return false;
	}

	uint8_t* file_bytes = NULL;
	size_t file_size = 0;
	char path[512];
	if (!opt_read_model(vfs, basename, path, sizeof path, &file_bytes, &file_size)) {
		opt_mesh_error(error, error_size, "original OPT not found or unreadable");
		return false;
	}

	opt_error_t opt_error = {{0}};
	opt_file_t* opt = opt_load_memory(file_bytes, file_size, &opt_error);
	free(file_bytes);
	if (!opt) {
		opt_mesh_error(error, error_size, opt_error.msg);
		return false;
	}

	const OptGltfBuildOptions build_options = {
		.vertex_scale = 1.0f,
		.smooth_angle_degrees = smooth_angle_degrees,
		.repair_normals = true,
		.emissive = true,
	};
	OptGltfDocument* document = NULL;
	if (!OptGltf_BuildMemory(opt, basename, &build_options, &document, &opt_error)) {
		opt_mesh_error(error, error_size, opt_error.msg);
		opt_free(opt);
		return false;
	}
	opt_free(opt);

	AeronGltfCookOptions cook_options;
	aeron_gltf_cook_default_options(&cook_options);
	cook_options.encoding = AERON_GLTF_COOK_ENCODING_RGBA8;
	cook_options.zstd_supercompress = false;
	cook_options.verbose = false;
	OptCookContext context = {
		.document = document,
		.model = out,
		.label = path,
	};
	const bool succeeded = aeron_gltf_cook_data(
		OptGltf_Data(document), path, opt_image_provider, &context,
		opt_model_consumer, &context, &cook_options);
	OptGltf_Free(document);
	if (!succeeded) {
		Aeron_GltfMeshFree(out);
		opt_mesh_error(error, error_size, "in-memory OPT conversion/cook failed");
		return false;
	}
	for (uint32_t i = 0; i < out->material_count; ++i) {
		AeronGltfMaterial* material = &out->materials[i];
		const float* emissive_rect = material->uv_xform[AERON_GLTF_CHANNEL_EMISSIVE];
		if (emissive_rect[2] > 0.0f && emissive_rect[3] > 0.0f) {
			material->emissive_strength *= emissive_strength;
		}
	}
	return true;
}
