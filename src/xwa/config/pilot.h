#ifndef XWA_CONFIG_PILOT_H
#define XWA_CONFIG_PILOT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int Pilot_CreateNew(const char* pilotName);
int Pilot_LoadFile(const char* fileName);
int Pilot_FindAndLoadByName(const char* pilotName);
int Pilot_ParseCommandLine(const char* cmdLine);
int Pilot_Save(int toTempFile);

#ifdef __cplusplus
}
#endif

#endif
