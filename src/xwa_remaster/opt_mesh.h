#ifndef XWA_REMASTER_OPT_MESH_H
#define XWA_REMASTER_OPT_MESH_H

#include "aeron/asset/flight_model.h"
#include "aeron/vfs.h"

#include <stdbool.h>
#include <stddef.h>

bool XwaRemasterOptMesh_Init(AeronVfs* vfs, char* error, size_t error_size);

/* Converts the game's OPT file `basename` to a flight model. Its self-lit
   texels glow at `emissive_strength`. When `projectile_strength` is above 0,
   the whole model glows at that strength instead, the way the original game
   draws projectiles. */
bool XwaRemasterOptMesh_Build(AeronVfs* vfs, const char* basename,
							  float smooth_angle_degrees, float emissive_strength,
							  float projectile_strength,
							  AeronFlightModel* out, char* error, size_t error_size);

#endif
