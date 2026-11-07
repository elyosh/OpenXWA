#ifndef XWA_RUNTIME_FRONTEND_CONCOURSE_PLANET_H
#define XWA_RUNTIME_FRONTEND_CONCOURSE_PLANET_H

/* The planet in the concourse window for a mission, from the `concourse`
 * section of the mission file (mission_file.h): its image group and the
 * position of its centre, in design pixels. Without an image, the game picks
 * the planet. Without a position, the position is random.
 *
 * The mission is the concourse's current mission: the entry of the mission
 * list with g_currentMissionId. The game reads its file again when the
 * mission changes. */

#ifdef __cplusplus
extern "C" {
#endif

/* The image group that the current mission sets, or -1. */
int XwaConcoursePlanet_Image(void);

/* Sets the centre `*x`, `*y`, in 640x480 units, when the current mission
   sets it. */
void XwaConcoursePlanet_Center(int* x, int* y);

#ifdef __cplusplus
}
#endif

#endif
