#ifndef XWA_RUNTIME_MODELS_MISSION_BACKDROPS_H
#define XWA_RUNTIME_MODELS_MISSION_BACKDROPS_H

/* Backdrop ids past the stock table, for mods whose mission files use them.
 *
 * A backdrop flight group gives a backdrop id, and the stock table gives ids 0
 * to 103 an object type and flags. flight/flight.yaml can give ids 104 to 255
 * an image group each, which they draw as a planet:
 *
 *   version: 1
 *   backdrops:
 *     extra_planets:
 *       - { ids: [104, 255], first_group: 6304 }
 *
 * An entry gives the ids from its first to its last, and the first id's image
 * group; each next id takes the next group. Each listed id gets its own
 * object type, after the stock ones (OBJ_FirstExtraPlanet + id - 104), which
 * is a copy of stock planet id 1 with the id's image group, and the flags of
 * stock planet id 1. The game reads the file once, at startup. Without it,
 * ids above 103 place nothing, as in the stock game. */

#include "xwa/assets/object_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The first and last extra backdrop ids. */
#define XWA_EXTRA_BACKDROP_FIRST_ID XWA_MISSION_BACKDROP_DESCRIPTOR_COUNT
#define XWA_EXTRA_BACKDROP_LAST_ID 255

/* Makes the extra object types empty rows, then reads flight/flight.yaml.
   Called once at startup, before the craft definitions are applied. */
void XwaMissionBackdrops_Init(void);

/* The descriptor of backdrop id `id`: the stock one for ids 0 to 103, the
   extra one for a listed id, or NULL for any other id. */
const ObjectTypeBackdropDescriptor* XwaMissionBackdrops_Descriptor(int id);

#ifdef __cplusplus
}
#endif

#endif
