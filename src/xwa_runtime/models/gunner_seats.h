#ifndef XWA_RUNTIME_MODELS_GUNNER_SEATS_H
#define XWA_RUNTIME_MODELS_GUNNER_SEATS_H

/* The gunner seats of a model, from the `gunner_seats` group of its model
 * settings file:
 *
 *   gunner_seats:
 *     forward_fire: false
 *     seats:
 *       - {x: 0, y: 150, z: 65, mount_angle_a: 0, mount_angle_b: 0,
 *          aim_limit_a: 900, aim_limit_b: 32767, cockpit: ywingbgunner, inverted: false}
 *
 * In the stock game, only the Corellian transport, the Millennium Falcon and
 * the Family transport have gunner seats, and their craft records hold them.
 * With the group, any craft can have seats, and its list replaces the
 * record's seats. `seats: []` means no seat. A craft has at most 2 seats.
 *
 * A seat has a position (x, y, z) and two mount angles and two aim limits,
 * as in the craft record (the `turrets` entries of an object type file).
 * `cockpit` is the model of the seat's cockpit: an object type with
 * `gunner_cockpit: true` that loads it, or the stock gunner cockpit
 * (corelliantransportgunner). `inverted` draws the cockpit upside down, as
 * the stock game does for its second seat. All keys but `cockpit` are
 * optional and default to 0 or false.
 *
 * `forward_fire: false` stops the turret laser groups from also firing
 * forward when the pilot fires. It defaults to true, as in the stock game.
 *
 * The seats change the simulation. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

#define XWA_GUNNER_SEAT_MAX 2

typedef enum XwaGunnerSeatField {
	XWA_GUNNER_SEAT_X,
	XWA_GUNNER_SEAT_Y,
	XWA_GUNNER_SEAT_Z,
	XWA_GUNNER_SEAT_MOUNT_ANGLE_A,
	XWA_GUNNER_SEAT_MOUNT_ANGLE_B,
	XWA_GUNNER_SEAT_AIM_LIMIT_A,
	XWA_GUNNER_SEAT_AIM_LIMIT_B,
	XWA_GUNNER_SEAT_COCKPIT,
	XWA_GUNNER_SEAT_INVERTED,
	XWA_GUNNER_SEAT_FIELDS
} XwaGunnerSeatField;

/* The hardpoints that a gunner cockpit can have, for
   XwaGunnerSeats_CockpitHasHardpoint. */
typedef enum XwaGunnerHardpoint {
	XWA_GUNNER_HARDPOINT_LASER,
	XWA_GUNNER_HARDPOINT_ION,
	XWA_GUNNER_HARDPOINT_WARHEAD
} XwaGunnerHardpoint;

/* Drops the previous mission's seats. Called by the model settings reader. */
void XwaGunnerSeats_ClearModelSettings(void);
/* Reads the `gunner_seats` group of `objectType`'s model settings file, or
   does nothing when `group` is NULL. `path` is the file, for the log.
   Called by the model settings reader. */
void XwaGunnerSeats_ReadModelSettings(unsigned int objectType, const char* path,
									  const struct AeronConfigNode* group);

/* Returns the number of seats of a craft of `objectType`, or -1 when its
   model has no `gunner_seats`, so the stock code applies. */
int XwaGunnerSeats_Count(unsigned int objectType);
/* Returns `field` of seat `seat` (from 0) of a craft of `objectType`, or
   `stock` when the model has no `gunner_seats` or no such seat. The cockpit
   is an object type, and `inverted` is 1 or 0. */
int XwaGunnerSeats_Value(unsigned int objectType, int seat, XwaGunnerSeatField field, int stock);
/* Returns 0 when the turret laser groups of a craft of `objectType` don't
   fire forward with the pilot's shots. */
int XwaGunnerSeats_ForwardFire(unsigned int objectType);

/* Whether the loaded model of gunner cockpit `cockpitType` has a hardpoint
   of `kind`. */
int XwaGunnerSeats_CockpitHasHardpoint(unsigned int cockpitType, XwaGunnerHardpoint kind);
/* Writes the model file name of `objectType` ("<model>.opt") when it is a
   gunner cockpit that an object type file adds, and returns 1. */
int XwaGunnerSeats_CockpitFileName(unsigned int objectType, char* fileName, size_t fileNameSize);

#ifdef __cplusplus
}
#endif

#endif
