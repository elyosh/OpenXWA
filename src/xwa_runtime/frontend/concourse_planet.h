#ifndef XWA_RUNTIME_FRONTEND_CONCOURSE_PLANET_H
#define XWA_RUNTIME_FRONTEND_CONCOURSE_PLANET_H

/* A mission's planet in the concourse window, from the `concourse` section
 * of its mission file (mission_file.h). Every key is optional:
 *
 *   concourse:
 *     front_planet: { x: 319, y: 78, image: 15010 }
 *
 * `image` is the image group to show, such as 15010, instead of the planet
 * that the game picks. `x` and `y` are the planet's centre in 640x480 units,
 * instead of its random position. The mission is the concourse's current
 * mission: the entry of the mission list with g_currentMissionId. Its file
 * is read again when the mission changes. */

#ifdef __cplusplus
extern "C" {
#endif

/* The image group that the current mission sets, or -1. */
int XwaConcoursePlanet_Image(void);

/* Sets each axis of the centre `*x`, `*y` that the current mission sets. */
void XwaConcoursePlanet_Center(int* x, int* y);

#ifdef __cplusplus
}
#endif

#endif
