#ifndef XWA_RUNTIME_MODELS_PARTICLE_LIGHTS_H
#define XWA_RUNTIME_MODELS_PARTICLE_LIGHTS_H

/* Particle lights: the game gives every particle effect attached to an
 * object, except type 1, a light of intensity 125 and colour (1, 1, 0.75)
 * at its emitter, for the whole life of the effect
 * (Particle_AppendObjectEffectPointLights). The HD renderer replaces these
 * lights by group: effect 4 is `cockpit_sparks`, effect 2 `cockpit_smoke`.
 * The other effects are `player_craft_damage` on the local player's craft
 * and `craft_damage` on every other object. The defaults are in
 * `point_lights.particle_lights` of resources/remaster/config.yaml, and
 * each key of the mod's `particle_lights` section replaces one. The classic
 * renderers keep the game's lights. */

#ifdef __cplusplus
extern "C" {
#endif

struct XwaSnapshot;

/* Returns the group called `name`, or -1. */
int XwaParticleLights_Find(const char* name);

/* Returns the name of `group`, or NULL. */
const char* XwaParticleLights_Name(int group);

/* Reads the `particle_lights` section of flight/flight.yaml. */
void XwaParticleLights_Init(void);

/* Writes the values that the mod sets into the snapshot's particle
   lights. */
void XwaParticleLights_Capture(struct XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
