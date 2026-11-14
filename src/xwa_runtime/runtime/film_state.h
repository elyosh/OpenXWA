#ifndef XWA_RUNTIME_FILM_STATE_H
#define XWA_RUNTIME_FILM_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The port's block in films. A film starts in the middle of a mission, so it
   saves the state of the game at that step, then the inputs of each step.
   The original's header and world state hold the original game's state. The
   port adds rules and timing that keep state of their own, and this block
   holds them. It comes at the end of the header, from film version 6 on.
   Films without it play as before.

   The block also holds the settings that change the simulation: the timing
   step, which the playback uses, and the mod and the model replacements,
   which the playback can only report when they differ.

   From version 7 on, the film also saves the object table's layout before
   the world state. A mod's object counts change it, and the world state can
   only be restored into a table with the same layout.

   Version 8 writes the world state's records in the original's byte layout.
   Versions 6 and 7 wrote them in another order and can't be played. */

/* The name of the mod folder that the game uses. */
void XwaFilmState_Configure(const char* modFolder);

/* Writes the block to the film being recorded. */
void XwaFilmState_Write(void);
/* Reads the block from the film being played and restores the state. Call
   it after the world state is restored. */
void XwaFilmState_Read(void);

/* Reads the object table layout of the film being played (version 8 or
   later) and makes the next mission use its object counts. Call it before
   the mission loads. */
void XwaFilmState_UseLayoutCounts(void);

/* Writes the object table's layout to the film being recorded. */
void XwaFilmState_WriteLayout(void);
/* Reads the object table layout of the film being played (version 8 or
   later) and checks that the current table matches it. Logs the reason and
   returns 0 when they differ. */
int XwaFilmState_ReadLayout(void);

#ifdef __cplusplus
}
#endif

#endif
