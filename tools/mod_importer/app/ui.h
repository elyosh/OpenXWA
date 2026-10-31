#ifndef XWA_MOD_IMPORTER_UI_H
#define XWA_MOD_IMPORTER_UI_H

/*
 * The window of the OpenXWA Mod Importer. The Import tab takes the
 * installation, the original game and the mod folder, runs the import on its
 * own thread, and shows its progress, its log and a summary. The Mods tab
 * lists the mods and chooses the one that OpenXWA uses.
 */

#include <stddef.h>

typedef struct ImporterUi ImporterUi;

typedef enum ImporterUiFrame {
	IMPORTER_UI_CONTINUE,
	IMPORTER_UI_QUIT,
	IMPORTER_UI_FAILED,
} ImporterUiFrame;

/* Loads the font and OpenXWA's configuration. Aeron must be running. */
ImporterUi* ImporterUi_Create(char* error, size_t error_capacity);
/* Stops a running import and waits for it. */
void ImporterUi_Destroy(ImporterUi* app);

/* Draws one frame of the window, between Aeron_BeginFrame and
 * Aeron_Present. */
ImporterUiFrame ImporterUi_Frame(ImporterUi* app, float dt_seconds);

#endif /* XWA_MOD_IMPORTER_UI_H */
