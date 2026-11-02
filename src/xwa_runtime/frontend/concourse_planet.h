#ifndef XWA_RUNTIME_FRONTEND_CONCOURSE_PLANET_H
#define XWA_RUNTIME_FRONTEND_CONCOURSE_PLANET_H

/* The planet in the concourse window for a mission. The mission file sets
 * it in its `concourse` section (mission_file.h). All keys are optional:
 *
 *   concourse:
 *     front_planet: { position: [1276, 234], image: 15010 }
 *
 * - `image`: the image group to show, such as 15010. Without it, the game
 *   picks the planet.
 * - `position`: the centre of the planet, in design pixels
 *   (frontend/frontend.yaml `design`). Without it, the position is random.
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
