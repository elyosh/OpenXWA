#include "xwa_remaster/skins.h"

#include "aeron/log.h"
#include "xwa_runtime/models/model_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

struct XwaRemasterSkinModel {
	char key[XWA_SNAP_OPT_NAME_MAX];
	char glb[XWA_SNAP_SKIN_NAME_MAX]; /* GLB name the mission selections use */
	uint32_t variant_count;
	uint32_t asset_variant_count;
	char** variant_names;
	uint32_t skin_markings_count;
	uint32_t* skin_markings;
};

/* Skinned meshes, for the miss log. */
#define SKINS_MAX_MESHES 256
typedef struct SkinsMesh {
	const AeronSceneMesh* mesh;
	char key[XWA_SNAP_OPT_NAME_MAX];
	uint32_t markings_count;
	int miss_logged;
} SkinsMesh;
static SkinsMesh s_meshes[SKINS_MAX_MESHES];

static char* skins_copy(const char* s) {
	const size_t n = strlen(s) + 1;
	char* copy = (char*)malloc(n);
	if (copy) {
		memcpy(copy, s, n);
	}
	return copy;
}

void XwaRemasterSkins_Free(XwaRemasterSkinModel* skin) {
	if (!skin) {
		return;
	}
	for (uint32_t v = 0; skin->variant_names && v < skin->variant_count; v++) {
		free(skin->variant_names[v]);
	}
	free(skin->variant_names);
	free(skin->skin_markings);
	free(skin);
}

XwaRemasterSkinModel* XwaRemasterSkins_Capture(const AeronGltfModel* model, const char* key) {
	if (!model || model->group_count == 0 || model->variant_count == 0) {
		return NULL;
	}
	XwaRemasterSkinModel* skin = (XwaRemasterSkinModel*)calloc(1, sizeof *skin);
	if (!skin) {
		return NULL;
	}
	snprintf(skin->key, sizeof skin->key, "%s", key);
	/* The GLB name: remaster/models/<glb>.glb. */
	char path[512];
	if (XwaModelSource_GlbPath(key, path, sizeof path)) {
		const char* base = strrchr(path, '/');
		snprintf(skin->glb, sizeof skin->glb, "%s", base ? base + 1 : path);
		char* dot = strrchr(skin->glb, '.');
		if (dot) {
			*dot = '\0';
		}
	}
	skin->variant_count = model->variant_count;
	skin->asset_variant_count = model->asset_variant_count;
	skin->variant_names = (char**)calloc(model->variant_count, sizeof *skin->variant_names);
	skin->skin_markings_count = model->skin_markings_count;
	skin->skin_markings = (uint32_t*)calloc(model->skin_markings_count ? model->skin_markings_count : 1,
											sizeof *skin->skin_markings);
	if (!skin->variant_names || !skin->skin_markings) {
		XwaRemasterSkins_Free(skin);
		return NULL;
	}
	for (uint32_t v = 0; v < model->variant_count; v++) {
		if (model->variant_names[v] && !(skin->variant_names[v] = skins_copy(model->variant_names[v]))) {
			XwaRemasterSkins_Free(skin);
			return NULL;
		}
	}
	memcpy(skin->skin_markings, model->skin_markings,
		   model->skin_markings_count * sizeof *skin->skin_markings);
	return skin;
}

static uint32_t skins_variant(const XwaRemasterSkinModel* skin, const char* name) {
	for (uint32_t v = 0; v < skin->variant_count; v++) {
		if (skin->variant_names[v] && strcmp(skin->variant_names[v], name) == 0) {
			return v;
		}
	}
	return AERON_GLTF_NO_VARIANT;
}

uint32_t XwaRemasterSkins_MarkingsVariants(const XwaRemasterSkinModel* skin, const XwaSnapshot* snapshot,
										   uint32_t variants[XWA_REMASTER_MAX_MARKINGS]) {
	uint8_t used[XWA_REMASTER_MAX_MARKINGS] = { 0 };
	int highest = -1;
	for (uint32_t i = 0; i < snapshot->skin_use_count; i++) {
		const XwaSkinUse* use = &snapshot->skin_uses[i];
		if (strcmp(use->key, skin->key) == 0) {
			used[use->markings] = 1;
			highest = use->markings > highest ? use->markings : highest;
		}
	}
	/* A model no flight group or preview uses (hangar craft, for example)
	   shows markings 0, which objects start with. */
	if (highest < 0) {
		used[0] = 1;
		highest = 0;
	}
	for (int m = 0; m <= highest; m++) {
		const uint32_t assets = skin->asset_variant_count;
		variants[m] = (uint32_t)m < assets ? (uint32_t)m : assets - 1u;
		if (!used[m]) {
			continue;
		}
		const uint32_t fallback =
			(uint32_t)m < skin->skin_markings_count ? skin->skin_markings[m] : AERON_GLTF_NO_VARIANT;
		uint32_t variant = fallback;
		for (uint32_t i = 0; i < snapshot->skin_selection_count; i++) {
			const XwaSkinSelection* selection = &snapshot->skin_selections[i];
			if (selection->markings != m || strcmp(selection->model, skin->glb) != 0) {
				continue;
			}
			variant = skins_variant(skin, selection->variant);
			if (variant == AERON_GLTF_NO_VARIANT) {
				Aeron_LogWarn("xwa.remaster",
							  "skins: '%s' has no variant '%s' (markings %d); using its default", skin->glb,
							  selection->variant, m);
				variant = fallback;
			}
			break;
		}
		if (variant != AERON_GLTF_NO_VARIANT) {
			variants[m] = variant;
		}
	}
	return (uint32_t)(highest + 1);
}

uint64_t XwaRemasterSkins_MarkingsVariantsHash(const uint32_t* variants, uint32_t count) {
	uint64_t hash = 14695981039346656037ull ^ count;
	for (uint32_t i = 0; i < count; i++) {
		hash = (hash ^ variants[i]) * 1099511628211ull;
	}
	return hash;
}

void XwaRemasterSkins_NoteMesh(const AeronSceneMesh* mesh, const char* key, uint32_t markings_count) {
	for (uint32_t i = 0; i < SKINS_MAX_MESHES; i++) {
		if (!s_meshes[i].mesh) {
			s_meshes[i].mesh = mesh;
			snprintf(s_meshes[i].key, sizeof s_meshes[i].key, "%s", key);
			s_meshes[i].markings_count = markings_count;
			s_meshes[i].miss_logged = 0;
			return;
		}
	}
}

void XwaRemasterSkins_ForgetMesh(const AeronSceneMesh* mesh) {
	for (uint32_t i = 0; mesh && i < SKINS_MAX_MESHES; i++) {
		if (s_meshes[i].mesh == mesh) {
			memset(&s_meshes[i], 0, sizeof s_meshes[i]);
		}
	}
}

void XwaRemasterSkins_CheckMarkings(const AeronSceneMesh* mesh, uint32_t markings) {
	for (uint32_t i = 0; mesh && i < SKINS_MAX_MESHES; i++) {
		SkinsMesh* entry = &s_meshes[i];
		if (entry->mesh == mesh) {
			if (markings >= entry->markings_count && !entry->miss_logged) {
				entry->miss_logged = 1;
				Aeron_LogWarn("xwa.remaster", "skins: '%s' shown with markings %u but loaded for 0..%u",
							  entry->key, markings, entry->markings_count - 1u);
			}
			return;
		}
	}
}
