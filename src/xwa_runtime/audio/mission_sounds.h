#ifndef XWA_RUNTIME_AUDIO_MISSION_SOUNDS_H
#define XWA_RUNTIME_AUDIO_MISSION_SOUNDS_H

/* Sound replacements for one mission. A mission file can list sound files
 * to load instead of others during the mission:
 *
 *   sounds:
 *     - ['Wave/Sfx_HangarAmbient/FamilyHangar0.wav', 'Wave/Custom/Evacuate.wav']
 *
 * When the game loads a sound from the first path, it loads the second file
 * instead, under the same sound name. The game compares the first path with
 * the path that it builds. Case doesn't matter, and '/' and '\' are the
 * same. If two entries have the same first path, the game uses the first
 * one. If the replacement file doesn't exist, the game loads the original
 * file and logs a line. Replacement files can be in the mod's game/
 * folder. */

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the replacements of mission `missionPath` (for example
   "missions\1b0m7fw.tie"), replacing the previous mission's. */
void XwaMissionSounds_LoadMission(const char* missionPath);

/* Returns the file to load for sound file `fileName`: its replacement, or
   `fileName`. */
const char* XwaMissionSounds_Replace(const char* fileName);

#ifdef __cplusplus
}
#endif

#endif
