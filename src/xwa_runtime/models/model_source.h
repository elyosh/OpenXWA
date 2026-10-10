#ifndef XWA_RUNTIME_MODELS_MODEL_SOURCE_H
#define XWA_RUNTIME_MODELS_MODEL_SOURCE_H

/* Modern port: chooses where each flight model comes from. With replacements
 * enabled, a model whose key has a `remaster/models/<key>.glb` on the asset
 * root is built from that GLB for simulation and rendering instead of its
 * OPT. The replacement set is fixed by XwaModelSource_Init. A key is the
 * model file's basename without extension, lower-cased
 * ("FlightModels\\XWing.opt" -> "xwing").
 *
 * remaster/models/model_map.yaml (optional) names a different GLB for a key,
 * for model sets that rename stock models:
 *   version: 1
 *   models:
 *     tunaboat2: mediumtransport
 * Craft, exterior, cockpit and equipment keys are all mapped explicitly; a key
 * keeps its identity everywhere else (roles, simulation, render registry). */

#include "xwa/assets/file_io.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { XWA_MODEL_SOURCE_KEY_MAX = 64 };

typedef enum XwaModelSource {
	XWA_MODEL_SOURCE_OPT = 0,
	XWA_MODEL_SOURCE_GLB = 1,
	/* An exterior OPT whose base craft is replaced and has no exterior GLB:
	   the load reports "no model". */
	XWA_MODEL_SOURCE_SUPPRESSED = 2,
} XwaModelSource;

/* Called before XwaPort_Init; replacements require the HD renderer. */
void XwaModelSource_Configure(int replacementsEnabled);
/* Builds the replacement set once the VFS is ready. */
void XwaModelSource_Init(void);
/* Writes the asset-root path of `key`'s replacement GLB
   ("remaster/models/<glb key>.glb"), where the GLB key is the key's entry in
   remaster/models/model_map.yaml, or the key itself. Returns 0 when it does
   not fit. */
int XwaModelSource_GlbPath(const char* key, char* path, size_t pathSize);
/* `glbPath` (optional) receives the asset-root path of the GLB when the
   result is XWA_MODEL_SOURCE_GLB. */
XwaModelSource XwaModelSource_Resolve(const char* fileName, char* glbPath, size_t glbPathSize);
/* Replacement-aware form of the frontend's model-existence probes
   (File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb")): the same File_Open for
   OPT, NULL for SUPPRESSED, and an open stream on the GLB for GLB. Callers
   keep their original close (or leak) of the returned stream. */
XwaFile* XwaModelSource_OpenModelFile(const char* fileName);
int XwaModelSource_ReplacementsEnabled(void);
/* Makes the GLB `key` loadable by its own name, for settings that name models
   no list has (a mothership's hangar, a hangar object swap). Returns 1 when
   the key now resolves to a GLB. */
int XwaModelSource_AddKey(const char* key);
/* Writes the GLB key `objectType`'s model list row resolves to (the row's
   key through the model map) to `key` (XWA_MODEL_SOURCE_KEY_MAX bytes).
   Returns 0 when replacements are off or the type has no row. */
int XwaModelSource_TypeKey(unsigned int objectType, char* key);
/* Writes `fileName`'s key (basename without extension, lower-cased) to `key`
   (XWA_MODEL_SOURCE_KEY_MAX bytes). Returns 0 for an empty or overlong name. */
int XwaModelSource_NormalizeKey(const char* fileName, char* key);
/* `key` may also be a file name; it is normalized first. */
int XwaModelSource_IsCockpitClassKey(const char* key);
/* True when a Spacecraft0.LST row with this key is bound to a model type
   flagged MODEL_TYPE_FLAG_EXPANDED_TARGET_PROBE, whose loads read float
   hardpoints. */
int XwaModelSource_KeyRequiresFloatHardpoints(const char* key);

#ifdef __cplusplus
}
#endif

#endif
