#ifndef XWA_RUNTIME_AUDIO_MISSION_SOUNDS_H
#define XWA_RUNTIME_AUDIO_MISSION_SOUNDS_H

/* Sound replacements for one mission. A mission file can list sound files to
 * load instead of others during that mission:
 *
 *   sounds:
 *     - ['Wave\Sfx_HangarAmbient\FamilyHangar0.wav', 'Wave\Custom\Evacuate.wav']
 *
 * When the game loads a sound effect from the first path, it loads the second
 * file instead, under the same effect name. The first path is compared with
 * the path that the game builds. Upper and lower case count as the same, but
 * '/' and '\' don't. If two entries have the same first path, the first entry
 * is used. If the replacement file doesn't exist, the original file loads and
 * a line is logged. Replacement files can be in the mod folder's game/
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
