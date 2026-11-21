#ifndef XWA_RUNTIME_AUDIO_SOUND_SLOTS_H
#define XWA_RUNTIME_AUDIO_SOUND_SLOTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* When all the sound slots are taken, Sound_PlayEffectNow gives a new sound
 * the slot of the playing sound with the lowest priority. That priority only
 * depends on the volume and distance at the start, and drops with time. Many
 * sounds belong to fast objects such as laser bolts, which are out of range
 * within a second. Their sounds then play muted, past their max distance, but
 * keep their slots, and the game cuts sounds that can still be heard
 * instead.
 *
 * Returns the slot of a playing one-shot positional sound that is past its
 * max distance, so silent: the one farthest past it. Returns -1 if there is
 * none, or in 2D mode. Loops are left alone: the code that starts a loop
 * stops it itself. So are the sounds of priority 125 or more. */
int XwaSoundSlots_FindInaudible(void);

#ifdef __cplusplus
}
#endif

#endif
