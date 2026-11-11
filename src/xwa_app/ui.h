#ifndef XWA_APP_UI_H
#define XWA_APP_UI_H
#include "aeron/aeron.h"
#include "aeron/scene/font_atlas.h"
#include "aeron/scene/ui.h"
#include <stdbool.h>

/* The AeronUi context of the game's own windows, with the font that the
 * mod importer also uses (resources/assets/fonts/menu.{fnt,ktx2}). */
typedef struct XwaAppUi {
	AeronUiContext* context;
	AeronFontAtlas font;
} XwaAppUi;

bool XwaAppUi_Init(XwaAppUi* ui, char* error, size_t capacity);
void XwaAppUi_Shutdown(XwaAppUi* ui);
AeronUiContext* XwaAppUi_Context(XwaAppUi* ui);
#endif
