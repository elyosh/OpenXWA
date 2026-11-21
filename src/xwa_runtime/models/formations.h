#ifndef XWA_RUNTIME_MODELS_FORMATIONS_H
#define XWA_RUNTIME_MODELS_FORMATIONS_H

/* The formations of a mod.
 *
 * The `formations` section of flight/flight.yaml replaces formations of the
 * game's tables (g_formPosX, g_formPosY, g_formPosZ and g_formationDivisor).
 * Each key is a formation number, from 0 to 33. `slots` gives the side,
 * forward and up values of the six slots, and `divisor` divides them. The
 * game multiplies each value by the model's size on that axis and by the
 * flight group's spacing; the mod documentation gives the details.
 *
 * The tables are read in three places: the spawn position of each craft
 * (Mission_InitFlightGroupObjectSlot), formation flying
 * (paiman_calcformation), and the place where a craft that drops off a
 * flight group releases each of its craft (Mission_ResolveFormationSlotWorldLoc,
 * which doesn't use the divisor). The section changes all three, as a change
 * to the tables of the executable would. */

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the section and writes it into the tables. Called once at startup. */
void XwaFormations_Init(void);

#ifdef __cplusplus
}
#endif

#endif
