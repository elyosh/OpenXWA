#include "xwa_runtime/config/modern_mods_screen.h"

#include "xwa/assets/string_table.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa_runtime/config/modern_mods.h"
#include "xwa_runtime/config/modern_options_menu.h"

#include <stdint.h>
#include <stdio.h>

enum {
	XWA_MODS_NOTE_FONT = 10,
	XWA_MODS_NOTE_WIDTH = 560,
};

/* The next entry after `from` in the direction of `step`, with wrap. */
static size_t XwaModernModsScreen_Next(size_t from, int step) {
	const size_t count = XwaModernMods_Count();
	const size_t index = from < count ? from : 0;

	if (!count) {
		return from;
	}
	return step > 0 ? (index + 1) % count : (index + count - 1) % count;
}

/* Draws a centered note. A text too wide keeps its end, after "...", so a
 * long folder still shows its name. */
static void XwaModernModsScreen_DrawNote(int y, const char* text, int color) {
	char shortened[1100];
	FrontendRect rect;
	const char* shown = text;

	if (FrontendText_MeasureWidth(text, XWA_MODS_NOTE_FONT) > XWA_MODS_NOTE_WIDTH) {
		const char* tail = text;
		while (*tail) {
			snprintf(shortened, sizeof shortened, "...%s", ++tail);
			if (FrontendText_MeasureWidth(shortened, XWA_MODS_NOTE_FONT) <= XWA_MODS_NOTE_WIDTH) {
				break;
			}
		}
		shown = shortened;
	}
	FrontendDraw_RectAssign(&rect, 40, y, 600, y + 15);
	FrontendText_DrawCentered(XWA_MODS_NOTE_FONT, shown, &rect, color);
}

int XwaModernModsScreen_Update(int menu_center_x, int* cursor_row) {
	XwaModernOptionsMenu menu;
	const size_t chosen = XwaModernMods_Get();
	const XwaModernModEntry* entry = XwaModernMods_Entry(chosen);
	const int locked =
		XwaModernMods_LaunchOverride() || XwaModernModsScreen_Next(chosen, 1) == chosen || !entry;
	int step;
	int back;

	if (!cursor_row) {
		return 0;
	}
	XwaModernOptionsMenu_Begin(&menu, menu_center_x, 140, cursor_row, 2);
	XwaModernOptionsMenu_DrawTitle(&menu, "Mods");
	step = XwaModernOptionsMenu_DrawValue(&menu, "Mod", entry ? entry->name : "None", 160, locked);
	if (step) {
		XwaModernMods_Set(XwaModernModsScreen_Next(chosen, step));
	}

	entry = XwaModernMods_Entry(XwaModernMods_Get());
	XwaModernModsScreen_DrawNote(
		menu.y + 5, entry && entry->path[0] ? entry->path : "The original game, with no mod", g_colorGray);
	if (XwaModernMods_LaunchOverride()) {
		XwaModernModsScreen_DrawNote(menu.y + 22, "Set by the --mod option", g_colorGray);
	} else if (entry && entry->problem) {
		XwaModernModsScreen_DrawNote(menu.y + 22, entry->problem, g_colorRed);
	} else if (XwaModernMods_Get() != XwaModernMods_Running()) {
		XwaModernModsScreen_DrawNote(menu.y + 22, "Restart OpenXWA to use this mod.", g_colorGreen);
	}
	menu.y += 50;

	back = XwaModernOptionsMenu_DrawAction(&menu, FrontendString_Get(STR_BACK), 161, 0);
	back |= XwaModernOptionsMenu_TakeEscape(&menu);
	if (!back) {
		return 0;
	}
	XwaModernMods_Flush();
	return 1;
}
