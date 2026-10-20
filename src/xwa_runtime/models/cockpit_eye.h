#ifndef XWA_RUNTIME_MODELS_COCKPIT_EYE_H
#define XWA_RUNTIME_MODELS_COCKPIT_EYE_H

/* Modern port: the pilot eye of GLB replacement models.
 *
 * XWA places the pilot eye at the model def's cockpitEyeX/Y/Z (+0x238), which
 * FlightView_ComputeSeatEyeOffset reads every frame. FeDiskIo_BuildModelDef sets Y
 * and Z from a Cockpit (type 31) hardpoint on the craft model and never X;
 * no stock OPT has one. For a GLB replacement the hardpoint places the whole
 * eye, so converted and authored models carry the eye that the mod gives
 * the craft (in its patched craft table or its CockpitPov files). */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Restores `modelEntryIdx`'s eye to its value before any model load. The
   original never restores it, so a craft that loads a model without a
   Cockpit hardpoint after one with it (a replacement falling back to its
   OPT) would keep the previous eye. Called before the hardpoint walk. */
void XwaCockpitEye_ResetModelDef(uint16_t modelEntryIdx);
/* Applies a Cockpit hardpoint's X, which the original ignores, when
   `objectType`'s loaded model is a GLB replacement. */
void XwaCockpitEye_ApplyHardpointX(uint16_t modelEntryIdx, unsigned short objectType, int x);

/* Debug inspector: the local player craft's eye (X, Y, Z as stored). Get
   returns 0 without a player craft. Set edits the model def in place until
   the model is rebuilt; Restore puts back the values seen before the first
   Set for that model. */
int XwaCockpitEye_GetPlayer(int* modelIndex, int eye[3]);
int XwaCockpitEye_SetPlayer(const int eye[3]);
int XwaCockpitEye_RestorePlayer(void);

#ifdef __cplusplus
}
#endif

#endif
