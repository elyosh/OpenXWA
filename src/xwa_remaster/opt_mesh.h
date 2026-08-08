#ifndef XWA_REMASTER_OPT_MESH_H
#define XWA_REMASTER_OPT_MESH_H

#include "aeron/vfs.h"
#include "aeron/scene/gltf_mesh.h"

#include <stdbool.h>
#include <stddef.h>

bool XwaRemasterOptMesh_Init(AeronVfs* vfs, char* error, size_t error_size);

bool XwaRemasterOptMesh_Build(AeronVfs* vfs, const char* basename,
							  float smooth_angle_degrees, float emissive_strength,
							  AeronGltfModel* out, char* error, size_t error_size);

#endif
