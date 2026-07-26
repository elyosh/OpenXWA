/*
 * XwaSnapshot color conversion — raw engine 16bpp color -> RGBA8.
 *
 * The HD renderer does not scrape the classic framebuffer or decode
 * classic assets through the engine's blitters. This module converts
 * paint-command and text-color arguments from 16bpp source values to
 * RGBA for the draw list.
 */

#include "xwa_runtime/snapshot/snapshot.h"

#include "aeron/log.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/opt_model.h"          /* g_loadedModels */
#include "xwa/assets/sprite_texture.h"     /* TexLevel */
#include "xwa/flight/flight_light.h"       /* point-light intensity tables */
#include "xwa/frontend/frontend_display.h" /* g_pixelFormat555 */
#include "xwa/render/renderer.h"           /* g_flightTextPalette */
#include "xwa/render/renderer_internal.h"  /* g_flightCharToColorLut */

#include <stdio.h>
#include <string.h>

/* Expand a raw 16bpp value to 8-bit RGB per the live 555/565 flag. */
static void color16_to_rgb(uint32_t c, uint8_t out[3]) {
	uint32_t r, g, b;
	if (g_pixelFormat555) {
		r = (c >> 10) & 0x1F;
		g = (c >> 5) & 0x1F;
		b = c & 0x1F;
		out[0] = (uint8_t)((r << 3) | (r >> 2));
		out[1] = (uint8_t)((g << 3) | (g >> 2));
		out[2] = (uint8_t)((b << 3) | (b >> 2));
	} else {
		r = (c >> 11) & 0x1F;
		g = (c >> 5) & 0x3F;
		b = c & 0x1F;
		out[0] = (uint8_t)((r << 3) | (r >> 2));
		out[1] = (uint8_t)((g << 2) | (g >> 4));
		out[2] = (uint8_t)((b << 3) | (b >> 2));
	}
}

void XwaSnapshotExport_ColorToRgba(uint32_t color, uint8_t out_rgba[4]) {
	color16_to_rgb(color, out_rgba);
	out_rgba[3] = 0xFF;
}

uint32_t XwaSnapshotExport_FlightPaletteColor(uint16_t color_index) {
	const uint16_t color = g_flightTextPalette[color_index & 0xffu];
	const uint32_t r = g_pixelFormat555 ? ((color >> 10) & 0x1fu) << 3 : ((color >> 11) & 0x1fu) << 3;
	const uint32_t g = g_pixelFormat555 ? ((color >> 5) & 0x1fu) << 3 : ((color >> 5) & 0x3fu) << 2;
	const uint32_t b = (color & 0x1fu) << 3;
	return 0xff000000u | (r << 16) | (g << 8) | b;
}

uint8_t XwaSnapshotExport_FlightColorCodePaletteIndex(uint8_t color_code) {
	return color_code >= 0x40u && color_code < 96u && color_code != g_flightColorEscapeBypassChar
			   ? g_flightCharToColorLut[color_code]
			   : color_code;
}

int XwaSnapshotExport_ComponentTargetGeometry(int object_type, int component, float out_local[3],
											  int* out_extent) {
	if (object_type <= 0 || object_type >= OBJ_Count || component < 0 || component >= XWA_SNAP_MAX_MESH_SLOTS)
		return 0;
	if (out_local) {
		/* OPT-native coordinates. The flight model matrix maps this to the
		 * same side/up/forward vector as the classic (x, z, -y) call. */
		out_local[0] = (float)ModelMesh_GetCenterX(object_type, component);
		out_local[1] = (float)ModelMesh_GetCenterY(object_type, component);
		out_local[2] = (float)ModelMesh_GetCenterZ(object_type, component);
	}
	if (out_extent)
		*out_extent = ModelMesh_GetComponentMaxExtent((uint16_t)object_type, (uint16_t)component) / 3;
	return 1;
}

int XwaSnapshotExport_ModelTextureBinding(int object_type, int* out_group, int* out_frames_mode,
										  int* out_sprite_id) {
	if (object_type <= 0 || object_type >= OBJ_Count) {
		return 0;
	}
	const ModelTypeInfo* info = &g_modelTypeTable[object_type];
	if (info->textureGroup == 0xffffu) {
		return 0;
	}
	if (out_group) {
		*out_group = info->textureGroup;
	}
	if (out_frames_mode) {
		*out_frames_mode = (info->flags & 0x100u) != 0;
	}
	if (out_sprite_id) {
		/* Single-sprite types keep the DAT id in the frameCount field
		 * (LoadTexturesForType's ConvertById path); group-frame types
		 * overwrite it with the live sprite count at load. */
		*out_sprite_id = info->frameCount;
	}
	return 1;
}

int XwaSnapshotExport_ModelTexFrame(int object_type, int frame, int* out_w, int* out_h, int* out_max_bounds) {
	if (object_type <= 0 || object_type >= OBJ_Count) {
		return 0;
	}
	const ModelTypeInfo* info = &g_modelTypeTable[object_type];
	if (info->texLevels == NULL || frame <= 0 || frame > info->frameCount) {
		return 0;
	}
	/* FeDiskIo_SelectTextureFrame's frame lookup, level 0 only (flag
	 * 0x20 = one level per frame, else four). */
	const int levelsPerFrame = (info->flags & 0x20u) != 0 ? 1 : 4;
	const TexLevel* level = &info->texLevels[(frame - 1) * levelsPerFrame];
	if (out_w) {
		*out_w = level->width;
	}
	if (out_h) {
		*out_h = level->height;
	}
	if (out_max_bounds) {
		*out_max_bounds = info->maxBoundsExtent;
	}
	return 1;
}

int XwaSnapshotExport_ModelFrameCount(int object_type) {
	if (object_type <= 0 || object_type >= OBJ_Count) {
		return 0;
	}
	return g_modelTypeTable[object_type].frameCount;
}

/* Classic point-light source laws — the per-type switch of
 * FlightLight_AppendScenePointLightForObject as a pure function of
 * the captured object fields. The x4 multiplier of the explosion
 * branch is applied here; the software-mode x0.4 is not (hardware
 * semantics). */
int XwaSnapshotExport_PointLightForObject(int object_type, int genus, int type_specific_0,
										  int type_specific_w, int instance_extent, int brightness_q8,
										  float out_color[3], float* out_intensity, int* out_cull_radius) {
	float intensity = 0.0f;
	float r = 1.0f, g = 1.0f, b = 1.0f;
	int cull = 1024;
	const int sub = type_specific_0;

	if (genus == GENUS_Explosion) {
		intensity = 16.0f;
		switch (object_type) {
			case OBJ_SparkTextureGroup3000:
				if (sub == 0 || sub > 12) {
					return 0;
				}
				intensity = (float)g_flightLightSparkIntensityBySubtype[sub];
				r = 1.0f;
				g = 0.40000001f;
				b = 0.1f;
				cull = 2048;
				break;
			case OBJ_SparkTextureGroup3001:
			case OBJ_SparkTextureGroup3002:
				if (sub == 0 || sub > 12) {
					return 0;
				}
				intensity = (float)g_flightLightSparkIntensityBySubtype[sub];
				r = 1.0f;
				g = 0.40000001f;
				b = 0.0f;
				cull = 2048;
				break;
			case OBJ_SparkTextureGroup3003:
				if (sub == 0 || sub > 13) {
					return 0;
				}
				intensity = sub >= 12 ? 10.0f : (float)g_flightLightSparkIntensityBySubtype[sub];
				r = 1.0f;
				g = 0.40000001f;
				b = 0.0f;
				cull = 2048;
				break;
			case OBJ_ExplosionTextureGroup2000:
			case OBJ_ExplosionTextureGroup2003:
				if (sub == 0 || sub > 12) {
					return 0;
				}
				intensity = (float)(((unsigned)instance_extent >> 5) +
									g_flightLightExplosionIntensityOffsetBySubtype[sub]);
				r = 1.0f;
				g = 0.40000001f;
				b = 0.0f;
				cull = 2 * instance_extent + 4096;
				break;
			case OBJ_ExplosionTextureGroup2001:
				if (sub == 0 || sub > 12) {
					return 0;
				}
				intensity = (float)(((unsigned)instance_extent >> 5) +
									g_flightLightExplosionIntensityOffsetBySubtype[sub]);
				r = 1.0f;
				g = 0.40000001f;
				b = 0.1f;
				cull = 2 * instance_extent + 4096;
				break;
			case OBJ_ExplosionTextureGroup2002:
				if (sub == 0 || sub > 12) {
					return 0;
				}
				intensity = (float)(((unsigned)instance_extent >> 5) +
									g_flightLightExplosionIntensityOffsetBySubtype[sub]);
				r = 0.89999998f;
				g = 0.40000001f;
				b = 0.2f;
				cull = 2 * instance_extent + 4096;
				break;
			case OBJ_ExplosionTextureGroup2004:
				if (sub == 0 || sub > 11) {
					return 0;
				}
				intensity = (float)(((unsigned)instance_extent >> 5) +
									g_explosionPointLightIntensityOffsetBySubtype[sub]);
				r = 0.80000001f;
				g = 0.30000001f;
				b = 0.0f;
				cull = 2 * instance_extent + 4096;
				break;
			case OBJ_ExplosionTextureGroup2005:
				if (sub == 0 || sub > 14) {
					return 0;
				}
				intensity = (float)(((unsigned)instance_extent >> 5) +
									g_explosionPointLightIntensityOffsetBySubtype[sub]);
				r = 0.80000001f;
				g = 0.34999999f;
				b = 0.2f;
				cull = 2 * instance_extent + 0x2000;
				break;
			case OBJ_ExplosionTextureGroup2006:
				if (sub == 0 || sub > 42) {
					return 0;
				}
				if (sub < 6) {
					intensity = (float)((double)((unsigned)instance_extent >> 5) + (double)sub * 50.0);
				} else if (sub <= 38) {
					intensity = (float)((double)((unsigned)instance_extent >> 5) + 400.0);
				} else {
					intensity = (float)((double)((unsigned)instance_extent >> 5) + (double)(43 - sub) * 50.0);
				}
				r = 0.89999998f;
				g = 0.40000001f;
				b = 0.0f;
				cull = 2 * instance_extent + 0x4000;
				break;
			case OBJ_ChaffTextureGroup5000:
				intensity = 5.0f;
				r = 0.2f;
				g = 0.2f;
				b = 1.0f;
				break;
			case OBJ_DeathStarIITextureGroup17002:
				intensity = (float)type_specific_w;
				cull = 0x100000;
				r = 0.60000002f;
				g = (float)((type_specific_w % 6 + 2) * 0.25 * 0.2);
				b = 0.0f;
				break;
			default:
				intensity = (float)(unsigned)(brightness_q8 - 256);
				break;
		}
		intensity *= 4.0f;
	} else {
		switch (object_type) {
			case OBJ_LaserRebel:
			case OBJ_LaserRebelTurbo:
			case OBJ_WarheadLaser1:
			case OBJ_WarheadLaser3:
			case OBJ_LaserRebelTurbo_301:
			case OBJ_LaserRebelTurbo_302:
				intensity = 200.0f;
				r = 1.0f;
				g = 0.2f;
				b = 0.0f;
				break;
			case OBJ_LaserImperial:
			case OBJ_LaserImperialTurbo:
			case OBJ_WarheadLaser2:
			case OBJ_LaserImperialTurbo_303:
			case OBJ_LaserImperialTurbo_304:
			case OBJ_LaserImperialTurbo_305:
				intensity = 200.0f;
				r = 0.0f;
				g = 1.0f;
				b = 0.2f;
				break;
			case OBJ_LaserImperialDS:
				intensity = 50000.0f;
				r = 0.0f;
				g = 1.0f;
				b = 0.2f;
				cull = 0x4000;
				break;
			case OBJ_LaserIon:
			case OBJ_LaserIonTurbo:
			case OBJ_WarheadIon:
				intensity = 200.0f;
				r = 0.2f;
				g = 0.2f;
				b = 1.0f;
				break;
			case OBJ_WarheadTorpedo:
			case OBJ_WarheadAdvancedTorpedo:
				intensity = 250.0f;
				r = 0.40000001f;
				g = 0.2f;
				b = 1.0f;
				break;
			case OBJ_WarheadMagPulse:
				intensity = 250.0f;
				r = 1.0f;
				g = 0.2f;
				b = 1.0f;
				break;
			case OBJ_WarheadIonPulse:
				intensity = 250.0f;
				r = 0.2f;
				g = 0.2f;
				b = 1.0f;
				break;
			case OBJ_WarheadMissile:
			case OBJ_WarheadAdvancedMissile:
			case OBJ_WarheadSpaceBomb:
			case OBJ_WarheadRocket:
				intensity = 250.0f;
				r = 1.0f;
				g = 0.40000001f;
				b = 0.2f;
				break;
			case OBJ_WarheadFlare:
				intensity = 200.0f;
				r = 1.0f;
				g = 0.40000001f;
				b = 1.0f;
				break;
			case OBJ_MilleniumFalcon2:
				intensity = 2048.0f;
				cull = 256;
				r = 0.5f;
				g = 0.5f;
				b = 0.5f;
				break;
			case OBJ_DSAccelChamber:
				intensity = (float)type_specific_w;
				if ((type_specific_w & 1) != 0) {
					cull = 0x8000;
					r = 0.2f;
					g = 0.0f;
					b = 0.80000001f;
				} else {
					cull = 1024;
					r = 0.80000001f;
					g = 0.0f;
					b = 0.40000001f;
				}
				break;
			case OBJ_DSReactorCylinder:
				intensity = (float)type_specific_w;
				cull = 0x20000;
				r = 0.30000001f;
				g = 0.5f;
				b = 0.89999998f;
				break;
			default:
				return 0;
		}
	}

	out_color[0] = r;
	out_color[1] = g;
	out_color[2] = b;
	*out_intensity = intensity;
	*out_cull_radius = cull;
	return 1;
}

int XwaSnapshotExport_ModelMeshTypes(int object_type, uint8_t out_types[XWA_SNAP_MAX_MESH_SLOTS]) {
	if (object_type <= 0 || object_type >= OBJ_Count) {
		return 0;
	}
	int count = ModelMesh_GetObjectTypeMeshCount(object_type);
	if (count <= 0) {
		return 0;
	}
	if (count > XWA_SNAP_MAX_MESH_SLOTS) {
		count = XWA_SNAP_MAX_MESH_SLOTS;
	}
	for (int i = 0; i < count; i++) {
		out_types[i] = (uint8_t)ModelMesh_GetObjectTypeMeshType(object_type, i);
	}
	return count;
}

/* Processed OPTs currently owned by the classic engine. Public handle slots
 * are reused, so freeing clears the entry before a later load can repopulate
 * it. The generation makes the committed snapshot set cheap to reconcile. */
typedef struct XwaOptHandleState {
	uint16_t public_handle;
	char name[XWA_SNAP_OPT_NAME_MAX];
	uint8_t active;
} XwaOptHandleState;

static XwaOptHandleState g_optHandles[XWA_SNAP_MAX_OPT_ASSETS];
static uint64_t g_optAssetGeneration;
static uint8_t g_textureAssets[XWA_SNAP_MAX_TEXTURE_ASSETS];
static uint64_t g_textureAssetGeneration;

void XwaSnapshot_NoteOptLoad(uint16_t public_handle, const char* file_name) {
	if (!public_handle || !file_name) {
		return;
	}
	XwaOptHandleState* state = NULL;
	for (uint32_t i = 0; i < XWA_SNAP_MAX_OPT_ASSETS; i++) {
		if (g_optHandles[i].active && g_optHandles[i].public_handle == public_handle) {
			state = &g_optHandles[i];
			break;
		}
		if (!state && !g_optHandles[i].active) {
			state = &g_optHandles[i];
		}
	}
	if (!state) {
		Aeron_Log("xwa.snapshot", "processed-OPT registry full; '%s' untracked", file_name);
		return;
	}
	/* Basename without directories or extension. */
	const char* base = file_name;
	for (const char* p = file_name; *p; p++) {
		if (*p == '\\' || *p == '/') {
			base = p + 1;
		}
	}
	size_t n = 0;
	while (base[n] && base[n] != '.' && n + 1 < sizeof state->name) {
		state->name[n] = base[n];
		n++;
	}
	state->name[n] = '\0';
	state->public_handle = public_handle;
	state->active = 1;
	g_optAssetGeneration++;
}

void XwaSnapshot_NoteOptFree(uint16_t public_handle) {
	for (uint32_t i = 0; i < XWA_SNAP_MAX_OPT_ASSETS; i++) {
		if (!g_optHandles[i].active || g_optHandles[i].public_handle != public_handle) {
			continue;
		}
		memset(&g_optHandles[i], 0, sizeof g_optHandles[i]);
		g_optAssetGeneration++;
		return;
	}
}

const char* XwaSnapshotExport_OptHandleName(uint16_t public_handle) {
	for (uint32_t i = 0; i < XWA_SNAP_MAX_OPT_ASSETS; i++) {
		if (g_optHandles[i].active && g_optHandles[i].public_handle == public_handle) {
			return g_optHandles[i].name[0] ? g_optHandles[i].name : NULL;
		}
	}
	return NULL;
}

void XwaSnapshotExport_CaptureOptAssets(XwaSnapshot* snapshot) {
	if (!snapshot) {
		return;
	}
	snapshot->opt_asset_generation = g_optAssetGeneration;
	snapshot->opt_asset_count = 0;
	for (uint32_t i = 0; i < XWA_SNAP_MAX_OPT_ASSETS; i++) {
		if (!g_optHandles[i].active || !g_optHandles[i].name[0]) {
			continue;
		}
		XwaOptAsset* asset = &snapshot->opt_assets[snapshot->opt_asset_count++];
		asset->public_handle = g_optHandles[i].public_handle;
		snprintf(asset->name, sizeof asset->name, "%s", g_optHandles[i].name);
	}
}

void XwaSnapshot_NoteTextureLoad(uint16_t model_type) {
	if (model_type == 0 || model_type >= XWA_SNAP_MAX_TEXTURE_ASSETS || g_textureAssets[model_type]) {
		return;
	}
	g_textureAssets[model_type] = 1;
	g_textureAssetGeneration++;
}

void XwaSnapshot_NoteTextureFree(uint16_t model_type) {
	if (model_type == 0 || model_type >= XWA_SNAP_MAX_TEXTURE_ASSETS || !g_textureAssets[model_type]) {
		return;
	}
	g_textureAssets[model_type] = 0;
	g_textureAssetGeneration++;
}

void XwaSnapshotExport_CaptureTextureAssets(XwaSnapshot* snapshot) {
	if (!snapshot) {
		return;
	}
	snapshot->texture_asset_generation = g_textureAssetGeneration;
	snapshot->texture_asset_count = 0;
	for (uint16_t model_type = 0; model_type < XWA_SNAP_MAX_TEXTURE_ASSETS; model_type++) {
		if (!g_textureAssets[model_type]) {
			continue;
		}
		snapshot->texture_assets[snapshot->texture_asset_count++].model_type = model_type;
	}
}

const char* XwaSnapshotExport_ModelName(int object_type) {
	if (object_type <= 0 || object_type >= XWA_LOADED_MODEL_COUNT) {
		return NULL;
	}
	/* Primary: the OPT the engine actually LOADED for this type — the
	 * identity the classic renderer draws (covers hangar props and
	 * every non-craft loadable). */
	const uint16_t handle = g_loadedModels.byObjectType[object_type];
	const char* loaded_name = XwaSnapshotExport_OptHandleName(handle);
	if (loaded_name) {
		return loaded_name;
	}
	/* Fallback: the craft model-def name. */
	if (object_type < OBJ_Count) {
		const ModelIndex idx = GetModelIndexFromType((uint16_t)object_type);
		if ((int)idx >= 0 && (int)idx < XWA_MODEL_DEF_COUNT) {
			const char* name = g_modelDefs[idx].name;
			if (name && name[0]) {
				return name;
			}
		}
	}
	return NULL;
}
