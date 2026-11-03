#ifndef XWA_RUNTIME_MODELS_PARTICLE_LIGHTS_H
#define XWA_RUNTIME_MODELS_PARTICLE_LIGHTS_H

/* Particle lights: the game gives every particle effect attached to an
 * object, except type 1, a light of intensity 125 and colour (1, 1, 0.75)
 * at its emitter, for the whole life of the effect
 * (Particle_AppendObjectEffectPointLights). The `particle_lights` section
 * of the mod's flight settings replaces these lights in the HD renderer, by
 * group, and is read once at startup. Effect 4 is `cockpit_sparks`, effect 2
 * `cockpit_smoke`, and the other effects are `craft_damage`.
 *
 * Each light is a Blender point light: P watts give P * 683 / (4 pi)
 * candela all around. A group that isn't in the section keeps the game's
 * light. The game's own code and the classic renderers keep the game's
 * lights, so the section only changes the display. */

#ifdef __cplusplus
extern "C" {
#endif

struct XwaSnapshot;

/* Reads the `particle_lights` section of flight/flight.yaml. */
void XwaParticleLights_Init(void);

/* Writes the groups' settings into the snapshot. */
void XwaParticleLights_Capture(struct XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
