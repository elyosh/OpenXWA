#ifndef XWA_RUNTIME_MODELS_MISSION_BACKDROPS_H
#define XWA_RUNTIME_MODELS_MISSION_BACKDROPS_H

/* Extra backdrop ids, for mods whose missions use ids past the stock table.
 *
 * A backdrop flight group has a backdrop id. The stock table gives ids 0 to
 * 103 an object type and flags. flight/flight.yaml can make ids 104 to 255
 * draw planets, each with its own image group:
 *
 *   version: 1
 *   backdrops:
 *     extra_planets:
 *       - { ids: [104, 255], image_groups: [6304, 6455] }
 *
 * Each entry has a range of ids and a range of image groups of the same
 * length. The first id uses the first group, the second id the second
 * group, and so on.
 *
 * Each listed id gets its own object type after the stock ones, at
 * OBJ_FirstExtraPlanet + id - 104. It is a copy of stock planet id 1, with
 * the id's image group.
 *
 * The game reads the file once, at startup. Without it, ids above 103 show
 * nothing, as in the stock game. */

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
