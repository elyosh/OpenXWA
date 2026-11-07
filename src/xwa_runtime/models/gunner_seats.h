#ifndef XWA_RUNTIME_MODELS_GUNNER_SEATS_H
#define XWA_RUNTIME_MODELS_GUNNER_SEATS_H

/* Reads a model's gunner seats from the `gunner_seats` group of its model
 * settings file. The mod documentation describes the keys.
 *
 * In the stock game, only the Corellian transport, the Millennium Falcon and
 * the family transport have gunner seats, and their craft records hold them.
 * With this group, any craft can have seats, and the list replaces the seats
 * in the craft record. A craft has at most 2 seats. The
 * `craft_record.turrets` entries of an object type file use the same keys,
 * without `inverted`, and with `object_type` instead of `cockpit`.
 *
 * A seat's position is in the same frame as the cockpit eye and the
 * hardpoints. At startup, the game gives each cockpit model that a seat uses
 * its own hidden object type.
 *
 * The seats change the simulation. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

#define XWA_GUNNER_SEAT_MAX 2

/* The values of a seat, stored like in the craft record: x is right, y is
   forward and z is up, in game units. Angles are 16-bit. A is the pitch and
   B is the yaw. */
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

/* Reads `position`, a list of three lengths in metres [right, forward, up],
   and stores them in `out` in game units. `what` names the key in log
   messages. If the list can't be used, logs a warning and returns 0. */
int XwaGunnerSeats_ReadPosition(const char* path, const char* what, const struct AeronConfigNode* position,
								int32_t* out);
/* Reads `position_m` and the four angles of the seat in `node`. Converts them
   to game units and 16-bit angles, and stores them in `value`, from
   XWA_GUNNER_SEAT_X to XWA_GUNNER_SEAT_AIM_LIMIT_B. A missing key gives 0.
   `what` identifies the seat in log messages. If a value can't be used,
   logs a warning and returns 0. */
int XwaGunnerSeats_ReadPlacement(const char* path, const char* what, const struct AeronConfigNode* node,
								 int32_t* value);

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
/* If `objectType` is a gunner cockpit that the game added for a seat,
   writes its model file name ("<model>.opt") and returns 1. Otherwise
   returns 0. */
int XwaGunnerSeats_CockpitFileName(unsigned int objectType, char* fileName, size_t fileNameSize);

#ifdef __cplusplus
}
#endif

#endif
