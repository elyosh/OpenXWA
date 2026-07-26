#ifndef XWA_FLIGHT_FEDISKIO_H
#define XWA_FLIGHT_FEDISKIO_H

#include "xwa/assets/file_io.h"       /* XwaFile */
#include "xwa/assets/sprite_texture.h" /* TexLevel */
#include "xwa/util/memory.h"           /* MemoryHandle */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ModelFloatHardpoint;

/*
 * fediskio: the flight-engine resource / disk-I/O translation unit, the XWA
 * descendant of TIE's FEDISKIO.C (generic resource loading + pilot-record file
 * I/O). All members are defined in src/xwa/flight/fediskio.c. The functions
 * keep their original-binary descriptive suffixes under the unified FeDiskIo_
 * prefix.
 */

/* Owned resource handles + craft-load state (XWA 0x63CF38..0x63CF68). */
extern MemoryHandle g_visibleObjectsHandle;
extern MemoryHandle g_flightLog1BufferHandle;
extern MemoryHandle g_flightTinyFontHandle;
extern MemoryHandle g_flightMicroFontHandle;
extern MemoryHandle g_flightSmallFontHandle;
extern uint8_t      g_cockpitUsesTieHitEffectPlanes;
extern char         g_exteriorModelLoaded;
extern char         Buffer[256];

/* Pilot-record post-mission merge (TIE updatepilotrecord). */
int16_t FeDiskIo_CommitFlightResults(void);

/* Global buffer / handle lifecycle. */
void FeDiskIo_InitGlobalBuffers(void);
void FeDiskIo_FreeGlobalBuffers(void);
void FeDiskIo_FreeModelResources(void);

/* Buffered file-stream + fatal-error/retry plumbing. */
#ifndef XWA_MODERN
int      File_OpenGlobalStream(const char* fileName, const char* mode, int promptOnFail, int locationMode);
#endif
int16_t  FeDiskIo_CloseGlobalStream(int16_t removeFileOnError);
uint16_t FeDiskIo_ReadAllBytesOrFatal(const char* fileName, void* dst);
int      FeDiskIo_ShowFatalErrorMessageAndWaitKey(const char* message);
int16_t  FeDiskIo_ShowRetryFailPrompt(void);
size_t   FeDiskIo_ReadWithRetryPrompt(void* dst, size_t elemSize, size_t elemCount, XwaFile* stream);
void     FeDiskIo_FatalError(uint16_t errorCode);

/* Craft model / resource loading. */
char*    FeDiskIo_GetCraftModelName(unsigned int craftType);
void     FeDiskIo_LoadResources(void);
void     FeDiskIo_InitResources(void);
void     FeDiskIo_LoadCockpitModel(void);
void     FeDiskIo_LoadExteriorModel(void);
char     FeDiskIo_LoadFlightSfxBanks(void);
int      FeDiskIo_BuildModelDef(uint16_t modelEntryIdx, uint16_t loadedModelSlot);

/* Model textures. */
int16_t FeDiskIo_LoadTexturesForType(uint16_t modelType);
void    FeDiskIo_FreeTexturesForType(uint16_t modelType);
int16_t FeDiskIo_SelectTextureFrame(uint16_t modelType, uint16_t frame, int screenSize);
void    FeDiskIo_UploadTexLevelToStd3D(TexLevel* level);

/* Engine-glow emitter lists. */
void FeDiskIo_LoadCockpitGlowEmitters(unsigned int modelSlot);
void FeDiskIo_LoadExteriorGlowEmitters(unsigned int modelSlot);

/* Model-mesh accessors. */
uint8_t*                    FeDiskIo_GetMeshVertexComponentMap(int modelType, int meshIndex);
struct ModelFloatHardpoint* FeDiskIo_GetMeshFloatHardpoint(int modelType, uint8_t hardpointIndex);

#ifdef __cplusplus
}
#endif

#endif
