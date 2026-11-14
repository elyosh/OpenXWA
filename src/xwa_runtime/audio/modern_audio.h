#ifndef XWA_RUNTIME_AUDIO_MODERN_AUDIO_H
#define XWA_RUNTIME_AUDIO_MODERN_AUDIO_H

/* Doppler for the modern build.
 *
 * The game passes DirectSound a velocity of speed × direction, in its own
 * speed units. Flight moves a craft by (4660·speed + 128) >> 8 units every
 * 236 ticks, and 236 ticks are 0.944 s, so one speed unit is
 * 4660 / 256 / 0.944 = 19.28 units per second. Doppler compares velocities
 * with the speed of sound, so they must be in units per second. The stock
 * game turns doppler off, so its velocities never mattered. */

#ifdef __cplusplus
extern "C" {
#endif

/* A velocity in the game's speed units, in units per second. */
static inline float XwaModernAudio_UnitsPerSecond(float speed) { return speed * (4660.0f / 256.0f / 0.944f); }

/* Turns doppler on, or off as in the stock game. */
void XwaModernAudio_SetDoppler(int enabled);
/* Sets the doppler factor of the current 3D listener from the last
   XwaModernAudio_SetDoppler. Sound_Init_Sound_Engine calls it after setting
   the stock factor of 0. */
void XwaModernAudio_ApplyDoppler(void);

#ifdef __cplusplus
}
#endif

#endif
