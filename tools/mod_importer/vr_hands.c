/* The VR gloves of an XWAU installation.
 *
 * XWAU's Active Cockpit draws a glove where each VR controller is. A glove
 * is three OBJ files in Effects/ActiveCockpit, one for each pose of the
 * hand, and one image of a DAT file that Active_Cockpit.cfg names. This
 * converts them into the six models that OpenXWA draws
 * (flight/models/vr_hand_<side>_<pose>.glb), and writes the `vr_hands`
 * section of flight/flight.yaml, which says how the models sit on the hand.
 *
 * The OBJ files are in metres. The fingers point along -Z, the thumb is on
 * the +Y side, and the origin is near the tip of the index finger. The
 * models keep these coordinates. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cook_model.h"
#include "import_internal.h"
#include "ktx2_writer.h"
#include "tool_log.h"
#include "xwau_config.h"
#include "xwau_dat.h"

#define GLOVE_IMAGE_URI "glove.png"
/* Where the tip of the index finger of OpenXWA's own gloves is, in the space
 * of the grip pose. The converted gloves are placed with their fingertip at
 * the same point. */
static const double kTipInGrip[3] = { 0.0, -0.128, -0.010 };

typedef struct GloveVertex {
	float position[3];
	float normal[3];
	float uv[2];
} GloveVertex;

typedef struct GloveMesh {
	GloveVertex* vertices;
	size_t vertex_count;
	uint16_t* indices;
	size_t index_count;
	/* The vertex that is the furthest along the fingers (-Z). */
	float tip[3];
} GloveMesh;

typedef struct FloatList {
	float* items;
	size_t count, capacity;
} FloatList;

static bool list_add(FloatList* list, const float* values, size_t count) {
	if (list->count + count > list->capacity) {
		const size_t capacity = list->capacity ? list->capacity * 2 : 4096;
		float* items = realloc(list->items, capacity * sizeof *items);
		if (!items)
			return false;
		list->items = items;
		list->capacity = capacity;
	}
	memcpy(list->items + list->count, values, count * sizeof *values);
	list->count += count;
	return true;
}

static void mesh_free(GloveMesh* mesh) {
	free(mesh->vertices);
	free(mesh->indices);
	memset(mesh, 0, sizeof *mesh);
}

/* Adds the corner `v/vt/vn` of a face (indices from 1) and returns its
 * vertex, or -1 when the corner is invalid or the mesh is full. Corners
 * with the same three indices share a vertex. */
static int mesh_corner(GloveMesh* mesh, const FloatList* positions, const FloatList* uvs,
					   const FloatList* normals, int v, int vt, int vn, int (**seen)[3], size_t* capacity) {
	if (v < 1 || vt < 1 || vn < 1 || (size_t)v > positions->count / 3 || (size_t)vt > uvs->count / 2 ||
		(size_t)vn > normals->count / 3)
		return -1;
	for (size_t i = 0; i < mesh->vertex_count; ++i)
		if ((*seen)[i][0] == v && (*seen)[i][1] == vt && (*seen)[i][2] == vn)
			return (int)i;
	if (mesh->vertex_count >= 65535)
		return -1;
	if (mesh->vertex_count == *capacity) {
		const size_t grown = *capacity ? *capacity * 2 : 1024;
		GloveVertex* vertices = realloc(mesh->vertices, grown * sizeof *vertices);
		if (vertices)
			mesh->vertices = vertices;
		int (*corners)[3] = realloc(*seen, grown * sizeof *corners);
		if (corners)
			*seen = corners;
		if (!vertices || !corners)
			return -1;
		*capacity = grown;
	}
	GloveVertex* vertex = &mesh->vertices[mesh->vertex_count];
	memcpy(vertex->position, &positions->items[(v - 1) * 3], sizeof vertex->position);
	memcpy(vertex->normal, &normals->items[(vn - 1) * 3], sizeof vertex->normal);
	vertex->uv[0] = uvs->items[(vt - 1) * 2];
	/* An OBJ file's V goes up, and a glTF file's goes down. */
	vertex->uv[1] = 1.0f - uvs->items[(vt - 1) * 2 + 1];
	(*seen)[mesh->vertex_count][0] = v;
	(*seen)[mesh->vertex_count][1] = vt;
	(*seen)[mesh->vertex_count][2] = vn;
	return (int)mesh->vertex_count++;
}

/* Reads an OBJ file whose faces are triangles or quads with `v/vt/vn`
 * corners, as XWAU's gloves are. */
static bool mesh_read(const char* path, GloveMesh* mesh) {
	FILE* file = fopen(path, "rb");
	if (!file)
		return false;
	FloatList positions = { 0 }, uvs = { 0 }, normals = { 0 };
	int (*seen)[3] = NULL;
	size_t capacity = 0, index_capacity = 0;
	char line[512];
	bool ok = true;
	memset(mesh, 0, sizeof *mesh);
	while (ok && fgets(line, sizeof line, file)) {
		float values[3];
		if (line[0] == 'v' && line[1] == ' ') {
			ok = sscanf(line + 2, "%f %f %f", &values[0], &values[1], &values[2]) == 3 &&
				 list_add(&positions, values, 3);
		} else if (line[0] == 'v' && line[1] == 'n') {
			ok = sscanf(line + 2, "%f %f %f", &values[0], &values[1], &values[2]) == 3;
			const float length = sqrtf(values[0] * values[0] + values[1] * values[1] + values[2] * values[2]);
			for (int i = 0; ok && length > 0.0f && i < 3; ++i)
				values[i] /= length;
			ok = ok && list_add(&normals, values, 3);
		} else if (line[0] == 'v' && line[1] == 't') {
			ok = sscanf(line + 2, "%f %f", &values[0], &values[1]) == 2 && list_add(&uvs, values, 2);
		} else if (line[0] == 'f' && line[1] == ' ') {
			int v[4], vt[4], vn[4], corner[4];
			const int count = sscanf(line + 2, "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", &v[0], &vt[0], &vn[0],
									 &v[1], &vt[1], &vn[1], &v[2], &vt[2], &vn[2], &v[3], &vt[3], &vn[3]) /
							  3;
			ok = count >= 3;
			for (int i = 0; ok && i < count; ++i)
				ok = (corner[i] = mesh_corner(mesh, &positions, &uvs, &normals, v[i], vt[i], vn[i], &seen,
											  &capacity)) >= 0;
			/* A quad is two triangles: 0 1 2 and 0 2 3. */
			for (int i = 2; ok && i < count; ++i) {
				if (mesh->index_count + 3 > index_capacity) {
					index_capacity = index_capacity ? index_capacity * 2 : 4096;
					uint16_t* indices = realloc(mesh->indices, index_capacity * sizeof *indices);
					if (!indices) {
						ok = false;
						break;
					}
					mesh->indices = indices;
				}
				mesh->indices[mesh->index_count++] = (uint16_t)corner[0];
				mesh->indices[mesh->index_count++] = (uint16_t)corner[i - 1];
				mesh->indices[mesh->index_count++] = (uint16_t)corner[i];
			}
		}
	}
	fclose(file);
	free(positions.items);
	free(uvs.items);
	free(normals.items);
	free(seen);
	ok = ok && mesh->vertex_count > 0 && mesh->index_count > 0;
	if (!ok) {
		mesh_free(mesh);
		return false;
	}
	memcpy(mesh->tip, mesh->vertices[0].position, sizeof mesh->tip);
	for (size_t i = 1; i < mesh->vertex_count; ++i)
		if (mesh->vertices[i].position[2] < mesh->tip[2])
			memcpy(mesh->tip, mesh->vertices[i].position, sizeof mesh->tip);
	return true;
}

/* AeronGltfCookUriImageSource: every glove uses the one image. */
static bool glove_image(void* context, const char* uri, AeronGltfCookImageView* out) {
	const XwauDatImage* image = context;
	if (strcmp(uri, GLOVE_IMAGE_URI) != 0)
		return false;
	out->rgba = image->rgba;
	out->width = (int)image->width;
	out->height = (int)image->height;
	return true;
}

/* Cooks one glove into `glb`: a flight model with a root and one part. */
static bool glove_cook(const GloveMesh* mesh, const char* name, const XwauDatImage* image, const char* glb,
					   const AeronGltfCookOptions* options) {
	const size_t vertex_bytes = mesh->vertex_count * sizeof(float);
	const size_t index_bytes = (mesh->index_count * sizeof(uint16_t) + 3u) & ~(size_t)3u;
	const size_t size = vertex_bytes * 8 + index_bytes;
	uint8_t* bin = calloc(1, size);
	char json[4096];
	if (!bin)
		return false;
	float* positions = (float*)bin;
	float* normals = (float*)(bin + vertex_bytes * 3);
	float* uvs = (float*)(bin + vertex_bytes * 6);
	float low[3], high[3];
	memcpy(low, mesh->vertices[0].position, sizeof low);
	memcpy(high, mesh->vertices[0].position, sizeof high);
	for (size_t i = 0; i < mesh->vertex_count; ++i) {
		const GloveVertex* vertex = &mesh->vertices[i];
		memcpy(&positions[i * 3], vertex->position, sizeof vertex->position);
		memcpy(&normals[i * 3], vertex->normal, sizeof vertex->normal);
		memcpy(&uvs[i * 2], vertex->uv, sizeof vertex->uv);
		for (int axis = 0; axis < 3; ++axis) {
			low[axis] = fminf(low[axis], vertex->position[axis]);
			high[axis] = fmaxf(high[axis], vertex->position[axis]);
		}
	}
	memcpy(bin + vertex_bytes * 8, mesh->indices, mesh->index_count * sizeof(uint16_t));
	const int written = snprintf(
		json, sizeof json,
		"{\"asset\":{\"version\":\"2.0\",\"generator\":\"OpenXWA mod importer\"},"
		"\"extensionsUsed\":[\"AERON_flight_model\"],\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
		"\"nodes\":[{\"name\":\"%s\",\"children\":[1],"
		"\"extensions\":{\"AERON_flight_model\":{\"role\":\"model\"}}},"
		"{\"name\":\"glove\",\"mesh\":0,"
		"\"extensions\":{\"AERON_flight_model\":{\"role\":\"component\",\"meshType\":\"default\"}}}],"
		"\"meshes\":[{\"name\":\"glove\",\"primitives\":[{\"attributes\":"
		"{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0}]}],"
		"\"materials\":[{\"name\":\"glove\",\"pbrMetallicRoughness\":"
		"{\"baseColorTexture\":{\"index\":0},\"metallicFactor\":0,\"roughnessFactor\":0.8}}],"
		"\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987}],"
		"\"images\":[{\"name\":\"glove\",\"uri\":\"" GLOVE_IMAGE_URI "\",\"mimeType\":\"image/png\"}],"
		"\"textures\":[{\"source\":0,\"sampler\":0}],"
		"\"buffers\":[{\"byteLength\":%zu}],"
		"\"bufferViews\":["
		"{\"buffer\":0,\"byteOffset\":0,\"byteLength\":%zu,\"target\":34962},"
		"{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu,\"target\":34962},"
		"{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu,\"target\":34962},"
		"{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu,\"target\":34963}],"
		"\"accessors\":["
		"{\"bufferView\":0,\"componentType\":5126,\"count\":%zu,\"type\":\"VEC3\","
		"\"min\":[%.9g,%.9g,%.9g],\"max\":[%.9g,%.9g,%.9g]},"
		"{\"bufferView\":1,\"componentType\":5126,\"count\":%zu,\"type\":\"VEC3\"},"
		"{\"bufferView\":2,\"componentType\":5126,\"count\":%zu,\"type\":\"VEC2\"},"
		"{\"bufferView\":3,\"componentType\":5123,\"count\":%zu,\"type\":\"SCALAR\"}]}",
		name, size, vertex_bytes * 3, vertex_bytes * 3, vertex_bytes * 3, vertex_bytes * 6, vertex_bytes * 2,
		vertex_bytes * 8, index_bytes, mesh->vertex_count, (double)low[0], (double)low[1], (double)low[2],
		(double)high[0], (double)high[1], (double)high[2], mesh->vertex_count, mesh->vertex_count,
		mesh->index_count);
	bool cooked = false;
	if (written > 0 && (size_t)written < sizeof json) {
		const AeronGltfCookInput input = {
			.json = json,
			.json_size = (size_t)written,
			.label = glb,
			.bin = bin,
			.bin_size = size,
			.images = glove_image,
			.images_context = (void*)image,
		};
		cooked = aeron_gltf_cook_model_input(&input, glb, options, NULL);
	}
	free(bin);
	return cooked;
}

/* Reads a glove's image, which Active_Cockpit.cfg names as
 * <DAT file>-<group>-<image>. Without the key, it is XWAU's own. */
static bool glove_load_image(const char* source, const XwauLines* settings, const char* key,
							 XwauDatImage* image) {
	char value[512], path[IMPORT_PATH_CAP], error[256];
	int group = 1, id = 0;
	XwauLines_Value(settings, key, XWAU_RULES_CPP, value, sizeof value);
	if (!value[0])
		snprintf(value, sizeof value, "Effects\\ActiveCockpit\\Textures.dat-1-0");
	/* The group and the image are the last two numbers. */
	char* last = strrchr(value, '-');
	if (last) {
		*last = '\0';
		id = atoi(last + 1);
		char* first = strrchr(value, '-');
		if (first) {
			*first = '\0';
			group = atoi(first + 1);
		}
	}
	for (char* c = value; *c; ++c)
		if (*c == '\\')
			*c = '/';
	if (!Xwau_FindFile(source, value, path, sizeof path)) {
		ToolLog_Write(TOOL_LOG_INFO, "VR gloves: the image file %s is missing", value);
		return false;
	}
	if (!XwauDat_LoadImage(path, group, id, image, error, sizeof error)) {
		ToolLog_Write(TOOL_LOG_INFO, "VR gloves: %s", error);
		return false;
	}
	return true;
}

/* Converts one hand. Writes its `vr_hands` entry into `entry` and returns
 * true when the neutral pose, at least, was converted. */
static bool glove_convert_hand(ImportContext* ctx, const XwauLines* settings, int hand,
							   const AeronGltfCookOptions* options, char* entry, size_t entry_cap) {
	static const char* const sides[2] = { "left", "right" };
	static const char* const prefixes[2] = { "LGlove", "RGlove" };
	static const char* const poses[3][2] = { { "Neutral", "neutral" },
											 { "Point", "point" },
											 { "Grasp", "grasp" } };
	static const char* const keys[2] = { "left_glove_texture", "right_glove_texture" };
	const char* source = ctx->options->source;
	XwauDatImage image = { 0 };
	bool converted = false, has_tip = false;
	float tip[3] = { 0.0f, 0.0f, 0.0f };
	char rel[256], obj[IMPORT_PATH_CAP], glb[IMPORT_PATH_CAP], name[64];

	snprintf(rel, sizeof rel, "Effects/ActiveCockpit/%sNeutral.obj", prefixes[hand]);
	if (!Xwau_FindFile(source, rel, obj, sizeof obj) ||
		!glove_load_image(source, settings, keys[hand], &image))
		return false;
	for (int pose = 0; pose < 3 && !Import_ShouldStop(ctx); ++pose) {
		GloveMesh mesh;
		snprintf(rel, sizeof rel, "Effects/ActiveCockpit/%s%s.obj", prefixes[hand], poses[pose][0]);
		snprintf(name, sizeof name, "vr_hand_%s_%s", sides[hand], poses[pose][1]);
		if (!Xwau_FindFile(source, rel, obj, sizeof obj) ||
			!PathFormat(glb, sizeof glb, "%s/flight/models/%s.glb", ctx->options->mod, name))
			continue;
		if (!mesh_read(obj, &mesh)) {
			Import_ListFailed(ctx, "The VR glove %s couldn't be read.", rel);
			continue;
		}
		if (!glove_cook(&mesh, name, &image, glb, options)) {
			Import_ListFailed(ctx, "The VR glove %s couldn't be prepared for the game.", rel);
		} else {
			converted = converted || pose == 0;
			/* The pointing finger of the `point` model, or else of the
			   neutral one. */
			if (pose <= 1) {
				memcpy(tip, mesh.tip, sizeof tip);
				has_tip = true;
			}
		}
		mesh_free(&mesh);
	}
	XwauDat_FreeImage(&image);
	if (!converted || !has_tip)
		return false;
	/* The game turns the model by -90 degrees around X: the fingers (-Z)
	   then point along the grip's -Y, and the thumb side (+Y) along its -Z.
	   A point (x, y, z) of the model goes to (x, z, -y). The offset puts the
	   fingertip where the game's own gloves have theirs. */
	snprintf(entry, entry_cap,
			 "  %s: { model: vr_hand_%s, offset_m: [%.4f, %.4f, %.4f], rotation_deg: [-90, 0, 0], "
			 "tip_m: [%.4f, %.4f, %.4f] }\n",
			 sides[hand], sides[hand], kTipInGrip[0] - (double)tip[0], kTipInGrip[1] - (double)tip[2],
			 kTipInGrip[2] + (double)tip[1], (double)tip[0], (double)tip[1], (double)tip[2]);
	return true;
}

void ImportVrHands_Run(ImportContext* ctx) {
	const XwaModImportOptions* options = ctx->options;
	char settings_path[IMPORT_PATH_CAP], models[IMPORT_PATH_CAP], flight[IMPORT_PATH_CAP];
	char entries[2][256] = { "", "" };
	XwauLines settings = { 0 };
	AeronGltfCookOptions cook;

	if (!PathFormat(models, sizeof models, "%s/flight/models", options->mod) ||
		!PathFormat(flight, sizeof flight, "%s/flight/flight.yaml", options->mod) || !MakeDirs(models))
		return;
	if (Xwau_FindFile(options->source, "Active_Cockpit.cfg", settings_path, sizeof settings_path))
		XwauLines_Read(settings_path, "", XWAU_RULES_CPP, &settings);
	aeron_gltf_cook_default_options(&cook);
	if (options->zstd_level > 0)
		cook.zstd_level = options->zstd_level;
	const int zstd_level = ktx2_zstd_level();
	ktx2_set_zstd_level(cook.zstd_level);
	const bool left = glove_convert_hand(ctx, &settings, 0, &cook, entries[0], sizeof entries[0]);
	const bool right = glove_convert_hand(ctx, &settings, 1, &cook, entries[1], sizeof entries[1]);
	ktx2_set_zstd_level(zstd_level);
	XwauLines_Free(&settings);
	if (!left && !right)
		return;
	/* The flight settings are already written: the section goes at their
	   end. */
	FILE* file = fopen(flight, "ab");
	bool written = file && fprintf(file, "vr_hands:\n%s%s", entries[0], entries[1]) >= 0;
	written = file && fclose(file) == 0 && written;
	if (!written) {
		Import_ListFailed(ctx, "The VR gloves' settings couldn't be written.");
		return;
	}
	ToolLog_Write(TOOL_LOG_INFO, "VR gloves: imported the %s",
				  left && right ? "two gloves"
				  : left        ? "left glove"
								: "right glove");
}
