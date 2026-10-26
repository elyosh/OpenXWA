#ifndef XWA_RUNTIME_MODELS_PARTICLE_LIGHTS_H
#define XWA_RUNTIME_MODELS_PARTICLE_LIGHTS_H

/* Particle lights: the game gives every particle effect attached to an
 * object, except type 1, a light of intensity 125 and colour (1, 1, 0.75)
 * at its emitter, for the whole life of the effect
 * (Particle_AppendObjectEffectPointLights). The `particle_lights` section
 * of flight/flight.yaml replaces these lights in the HD renderer, by
 * group, and is read once at startup:
 *
 *   particle_lights:
 *     cockpit_sparks:          # effect 4: sparks from a hit through the shields
 *       color: [1, 1, 0.522]   # linear
 *       power_w: 40
 *       fade: true
 *     cockpit_smoke: ...       # effect 2: smoke from a hit through the shields
 *     craft_damage: ...        # the other effects on objects
 *
 * Each light is a Blender point light: P watts give P * 683 / (4 pi)
 * candela all around, and 0 watts give no light. `power_w` is required.
 * Without `color`, the light has the game's colour. With `fade: true`, the
 * light dims to nothing over the effect's life; the game keeps it at full
 * strength until the effect ends. A group that isn't in the section keeps
 * the game's light. The game's own code and the classic renderers keep the
 * game's lights. */

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
