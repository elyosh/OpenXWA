#ifndef XWA_RUNTIME_FILM_STATE_H
#define XWA_RUNTIME_FILM_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The port's block in films. A film starts in the middle of a mission, so it
   saves the state of the game at that step, then the inputs of each step.
   The original's header and world state hold the original game's state. The
   port adds rules and timing that keep state of their own, which this block
   holds. The port writes films as version 6, with this block at the end of
   the header. Older films don't have it and play as before.

   The block also holds the settings that change the simulation: the timing
   step, which the playback uses, and the mod and the model replacements,
   which the playback can only report when they differ. */

/* The name of the mod folder that the game uses. */
void XwaFilmState_Configure(const char* modFolder);

/* Writes the block to the film being recorded. */
void XwaFilmState_Write(void);
/* Reads the block from the film being played and restores the state. Call
   it after the world state is restored. */
void XwaFilmState_Read(void);

#ifdef __cplusplus
}
#endif

#endif
