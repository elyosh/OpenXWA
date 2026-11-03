#ifndef XWA_APP_MOD_FORMAT_H
#define XWA_APP_MOD_FORMAT_H

/* The mod format version, shared by the game and the mod importer. A mod's
 * mod.yaml gives the version it was made for in `version`, and the game only
 * uses mods with this version. Every change to a mod file format raises it
 * by one. A mod made for another version can't be used until it is imported
 * again, or updated by its author. */
enum { XWA_MOD_FORMAT_VERSION = 6 };

#endif
