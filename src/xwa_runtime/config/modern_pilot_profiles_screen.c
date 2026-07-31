#include "xwa_runtime/config/modern_pilot_profiles_screen.h"

#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/string_table.h"
#include "xwa/config/game_config.h"
#include "xwa/config/pilot.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_dialog.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_file_list.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/util/memory.h"
#include "xwa/util/string.h"
#include "xwa_runtime/config/modern_options_menu.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum {
	XWA_PILOT_PROFILE_VISIBLE_ROWS = 10,
	XWA_PILOT_PROFILE_ACTION_COUNT = 4,
	XWA_PILOT_PROFILE_LIST_Y = 75,
	XWA_PILOT_PROFILE_ACTION_Y = 305,
};

typedef enum XwaPilotProfilePendingAction {
	XWA_PILOT_PROFILE_PENDING_NONE,
	XWA_PILOT_PROFILE_PENDING_NAME,
	XWA_PILOT_PROFILE_PENDING_CREATE,
	XWA_PILOT_PROFILE_PENDING_SWITCH,
	XWA_PILOT_PROFILE_PENDING_DELETE,
	XWA_PILOT_PROFILE_PENDING_NOTICE,
} XwaPilotProfilePendingAction;

typedef struct XwaPilotProfileEntry {
	const char* file_name;
	char name[14];
	int active;
	int duplicate;
} XwaPilotProfileEntry;

static struct {
	FrontFilenameList* file_list;
	XwaPilotProfileEntry* entries;
	int count;
	int selected_index;
	int scroll_offset;
	int initialized;
	int active_changed;
	int focus_selected_after_refresh;
	int pending_profile_index;
	XwaPilotProfilePendingAction pending_action;
	char create_name[14];
} g_pilotProfilesScreen;

static int XwaModernPilotProfiles_CanChangeActive(void) {
	return !g_configRestrictedOptionsModalActive &&
		   g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_NONE;
}

static void XwaModernPilotProfiles_FreeList(void) {
	if (g_pilotProfilesScreen.entries) {
		Mem_Free(g_pilotProfilesScreen.entries);
		g_pilotProfilesScreen.entries = NULL;
	}
	if (g_pilotProfilesScreen.file_list) {
		FrontendFileList_Free(g_pilotProfilesScreen.file_list);
		g_pilotProfilesScreen.file_list = NULL;
	}
	g_pilotProfilesScreen.count = 0;
}

static int XwaModernPilotProfiles_Read(const char* file_name, PilotData* out) {
	XwaFile* stream;
	int file_size;
	int read_ok;
	int close_ok;

	if (!file_name || !out) {
		return 0;
	}

	stream = File_Open(AERON_VFS_ROOT_USER, file_name, "rb");
	if (!stream) {
		return 0;
	}
	file_size = File_GetSize(stream);
	memset(out, 0, sizeof(*out));
	read_ok = file_size >= (int)sizeof(*out) && File_ReadCount(stream, out, sizeof(*out));
	close_ok = File_Close(stream) == 0;
	if (!read_ok || !close_ok || !out->name[0] || !memchr(out->name, '\0', sizeof(out->name))) {
		return 0;
	}
	return 1;
}

static int XwaModernPilotProfiles_Refresh(const char* preferred_name) {
	FrontFilenameListNode* node;
	PilotData pilot;
	int source_index;
	int selected_index;
	int active_found;
	int i;

	XwaModernPilotProfiles_FreeList();
	g_pilotProfilesScreen.file_list = FrontendFileList_BuildSorted(AERON_VFS_ROOT_USER, "*.plt");
	if (!g_pilotProfilesScreen.file_list) {
		Aeron_LogError("xwa.pilot", "Failed to enumerate pilot profiles");
		return 0;
	}
	if (g_pilotProfilesScreen.file_list->count > 0) {
		g_pilotProfilesScreen.entries = (XwaPilotProfileEntry*)Mem_Alloc(
			(size_t)g_pilotProfilesScreen.file_list->count * sizeof(*g_pilotProfilesScreen.entries));
		if (!g_pilotProfilesScreen.entries) {
			Aeron_LogError("xwa.pilot", "Failed to allocate pilot profile list");
			XwaModernPilotProfiles_FreeList();
			return 0;
		}
	}

	node = g_pilotProfilesScreen.file_list->head;
	for (source_index = 0; node && source_index < g_pilotProfilesScreen.file_list->count;
		 ++source_index, node = node->next) {
		XwaPilotProfileEntry* entry;

		if (!XwaModernPilotProfiles_Read(node->fileName, &pilot)) {
			Aeron_LogWarn("xwa.pilot", "Ignoring invalid pilot profile '%s'", node->fileName);
			continue;
		}
		entry = &g_pilotProfilesScreen.entries[g_pilotProfilesScreen.count++];
		entry->file_name = node->fileName;
		memcpy(entry->name, pilot.name, sizeof(entry->name));
		entry->name[sizeof(entry->name) - 1] = '\0';
		entry->active = 0;
		entry->duplicate = 0;
		for (i = 0; i < g_pilotProfilesScreen.count - 1; ++i) {
			if (!Xwa_Stricmp(g_pilotProfilesScreen.entries[i].name, entry->name)) {
				entry->duplicate = 1;
				break;
			}
		}
	}

	selected_index = -1;
	active_found = 0;
	for (i = 0; i < g_pilotProfilesScreen.count; ++i) {
		XwaPilotProfileEntry* entry = &g_pilotProfilesScreen.entries[i];

		if (!entry->duplicate && !active_found && !Xwa_Stricmp(entry->name, g_pilotData.name)) {
			entry->active = 1;
			active_found = 1;
		}
		if (selected_index < 0 && preferred_name && !entry->duplicate &&
			!Xwa_Stricmp(entry->name, preferred_name)) {
			selected_index = i;
		}
	}
	if (selected_index < 0) {
		for (i = 0; i < g_pilotProfilesScreen.count; ++i) {
			if (g_pilotProfilesScreen.entries[i].active) {
				selected_index = i;
				break;
			}
		}
	}
	g_pilotProfilesScreen.selected_index = selected_index;
	if (g_pilotProfilesScreen.scroll_offset >= g_pilotProfilesScreen.count) {
		g_pilotProfilesScreen.scroll_offset =
			g_pilotProfilesScreen.count > 0 ? g_pilotProfilesScreen.count - 1 : 0;
	}
	return 1;
}

static int XwaModernPilotProfiles_NameExists(const char* name) {
	int i;

	for (i = 0; i < g_pilotProfilesScreen.count; ++i) {
		if (!Xwa_Stricmp(g_pilotProfilesScreen.entries[i].name, name)) {
			return 1;
		}
	}
	return 0;
}

static const char* XwaModernPilotProfiles_ValidateName(const char* name) {
	size_t length;

	if (!name || !name[0]) {
		return "Enter a pilot name.";
	}
	length = strlen(name);
	if (length > 12) {
		return "Pilot names are limited to 12 characters.";
	}
	if (isspace((unsigned char)name[0]) || isspace((unsigned char)name[length - 1])) {
		return "A pilot name cannot begin or end with a space.";
	}
	if (strpbrk(name, "/\\:*?\"<>|$")) {
		return "The pilot name contains a filename character that is not allowed.";
	}
	if (XwaModernPilotProfiles_NameExists(name)) {
		return "A pilot with that name already exists.";
	}
	return NULL;
}

static void XwaModernPilotProfiles_ShowNotice(const char* line1, const char* line2) {
	if (FrontendDialog_BeginConfirmDialog(line1, line2, NULL, FrontendString_Get(STR_OKAY), NULL)) {
		g_pilotProfilesScreen.pending_action = XWA_PILOT_PROFILE_PENDING_NOTICE;
	}
}

static void XwaModernPilotProfiles_BeginSwitch(void) {
	XwaPilotProfileEntry* entry;
	char current_line[64];
	char new_line[64];

	if (g_pilotProfilesScreen.selected_index < 0 ||
		g_pilotProfilesScreen.selected_index >= g_pilotProfilesScreen.count) {
		return;
	}
	entry = &g_pilotProfilesScreen.entries[g_pilotProfilesScreen.selected_index];
	snprintf(current_line, sizeof(current_line), "Current: %s", g_pilotData.name);
	snprintf(new_line, sizeof(new_line), "New: %s", entry->name);
	if (FrontendDialog_BeginConfirmDialog("Switch to this pilot?", current_line, new_line,
										  FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL))) {
		g_pilotProfilesScreen.pending_profile_index = g_pilotProfilesScreen.selected_index;
		g_pilotProfilesScreen.pending_action = XWA_PILOT_PROFILE_PENDING_SWITCH;
	}
}

static void XwaModernPilotProfiles_BeginDelete(void) {
	XwaPilotProfileEntry* entry;
	char pilot_line[64];

	if (g_pilotProfilesScreen.selected_index < 0 ||
		g_pilotProfilesScreen.selected_index >= g_pilotProfilesScreen.count) {
		return;
	}
	entry = &g_pilotProfilesScreen.entries[g_pilotProfilesScreen.selected_index];
	snprintf(pilot_line, sizeof(pilot_line), "Pilot: %s", entry->name);
	if (FrontendDialog_BeginConfirmDialog("Delete this pilot?", pilot_line, "This cannot be undone.",
										  FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL))) {
		g_pilotProfilesScreen.pending_profile_index = g_pilotProfilesScreen.selected_index;
		g_pilotProfilesScreen.pending_action = XWA_PILOT_PROFILE_PENDING_DELETE;
	}
}

static void XwaModernPilotProfiles_BeginCreateConfirm(void) {
	char pilot_line[64];

	snprintf(pilot_line, sizeof(pilot_line), "Pilot: %s", g_pilotProfilesScreen.create_name);
	if (FrontendDialog_BeginConfirmDialog("Create and switch to this pilot?", pilot_line,
										  "The current pilot will be saved first.",
										  FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL))) {
		g_pilotProfilesScreen.pending_action = XWA_PILOT_PROFILE_PENDING_CREATE;
	}
}

static void XwaModernPilotProfiles_SwitchConfirmed(void) {
	XwaPilotProfileEntry* entry;
	PilotData candidate;
	const char* suffix;

	if (g_pilotProfilesScreen.pending_profile_index < 0 ||
		g_pilotProfilesScreen.pending_profile_index >= g_pilotProfilesScreen.count) {
		return;
	}
	if (!XwaModernPilotProfiles_CanChangeActive()) {
		XwaModernPilotProfiles_ShowNotice("Pilot switching is not available here.", NULL);
		return;
	}
	entry = &g_pilotProfilesScreen.entries[g_pilotProfilesScreen.pending_profile_index];
	if (!XwaModernPilotProfiles_Read(entry->file_name, &candidate) ||
		Xwa_Stricmp(entry->name, candidate.name)) {
		Aeron_LogError("xwa.pilot", "Pilot profile '%s' changed or could not be read", entry->file_name);
		XwaModernPilotProfiles_ShowNotice("The selected pilot could not be loaded.",
										  "The active pilot was not changed.");
		return;
	}
	if (!Pilot_Save(0)) {
		Aeron_LogError("xwa.pilot", "Failed to save active pilot '%s' before switching", g_pilotData.name);
		XwaModernPilotProfiles_ShowNotice("The current pilot could not be saved.",
										  "The active pilot was not changed.");
		return;
	}

	memcpy(&g_pilotData, &candidate, sizeof(g_pilotData));
	if (!g_pilotData.multiplayerGameName[0]) {
		suffix = FrontendString_Get(STR_SGAME);
		snprintf(g_pilotData.multiplayerGameName, sizeof(g_pilotData.multiplayerGameName), "%s%s",
				 g_pilotData.name, suffix);
	}
	if (!g_pilotData.multiplayerHostName[0]) {
		strncpy(g_pilotData.multiplayerHostName, g_pilotData.multiplayerGameName,
				sizeof(g_pilotData.multiplayerHostName));
		g_pilotData.multiplayerHostName[sizeof(g_pilotData.multiplayerHostName) - 1] = '\0';
	}
	Config_Write();
	g_pilotProfilesScreen.active_changed = 1;
	XwaModernPilotProfiles_Refresh(g_pilotData.name);
	g_pilotProfilesScreen.focus_selected_after_refresh = 1;
}

static void XwaModernPilotProfiles_CreateConfirmed(void) {
	PilotData previous;

	if (!XwaModernPilotProfiles_CanChangeActive()) {
		XwaModernPilotProfiles_ShowNotice("Pilot creation is not available here.", NULL);
		return;
	}
	if (!Pilot_Save(0)) {
		Aeron_LogError("xwa.pilot", "Failed to save active pilot '%s' before creation", g_pilotData.name);
		XwaModernPilotProfiles_ShowNotice("The current pilot could not be saved.",
										  "The new pilot was not created.");
		return;
	}
	memcpy(&previous, &g_pilotData, sizeof(previous));
	if (!Pilot_CreateNew(g_pilotProfilesScreen.create_name)) {
		memcpy(&g_pilotData, &previous, sizeof(g_pilotData));
		Aeron_LogError("xwa.pilot", "Failed to create pilot '%s'", g_pilotProfilesScreen.create_name);
		XwaModernPilotProfiles_ShowNotice("The new pilot could not be created.", NULL);
		return;
	}
	Config_Write();
	g_pilotProfilesScreen.active_changed = 1;
	XwaModernPilotProfiles_Refresh(g_pilotData.name);
	g_pilotProfilesScreen.focus_selected_after_refresh = 1;
}

static void XwaModernPilotProfiles_DeleteConfirmed(void) {
	XwaPilotProfileEntry* entry;

	if (g_pilotProfilesScreen.pending_profile_index < 0 ||
		g_pilotProfilesScreen.pending_profile_index >= g_pilotProfilesScreen.count) {
		return;
	}
	entry = &g_pilotProfilesScreen.entries[g_pilotProfilesScreen.pending_profile_index];
	if (entry->active) {
		return;
	}
	if (File_Remove(AERON_VFS_ROOT_USER, entry->file_name) != 0) {
		Aeron_LogError("xwa.pilot", "Failed to delete pilot profile '%s'", entry->file_name);
		XwaModernPilotProfiles_ShowNotice("The selected pilot could not be deleted.", NULL);
		return;
	}
	XwaModernPilotProfiles_Refresh(g_pilotData.name);
}

static int XwaModernPilotProfiles_HandlePending(void) {
	int confirmed;
	const char* validation_error;
	XwaPilotProfilePendingAction action;

	action = g_pilotProfilesScreen.pending_action;
	if (action == XWA_PILOT_PROFILE_PENDING_NONE) {
		return 0;
	}
	if (action == XWA_PILOT_PROFILE_PENDING_NAME) {
		if (!FrontendDialog_PromptForPilotName(g_pilotProfilesScreen.create_name)) {
			return 1;
		}
		g_pilotProfilesScreen.pending_action = XWA_PILOT_PROFILE_PENDING_NONE;
		if (!g_pilotProfilesScreen.create_name[0]) {
			return 0;
		}
		validation_error = XwaModernPilotProfiles_ValidateName(g_pilotProfilesScreen.create_name);
		if (validation_error) {
			XwaModernPilotProfiles_ShowNotice("The pilot name is not valid.", validation_error);
			return 1;
		}
		XwaModernPilotProfiles_BeginCreateConfirm();
		return 1;
	}
	if (!FrontendDialog_TakeConfirmDialogResult(&confirmed)) {
		return 1;
	}
	g_pilotProfilesScreen.pending_action = XWA_PILOT_PROFILE_PENDING_NONE;
	if (!confirmed || action == XWA_PILOT_PROFILE_PENDING_NOTICE) {
		return 0;
	}
	if (action == XWA_PILOT_PROFILE_PENDING_CREATE) {
		XwaModernPilotProfiles_CreateConfirmed();
	} else if (action == XWA_PILOT_PROFILE_PENDING_SWITCH) {
		XwaModernPilotProfiles_SwitchConfirmed();
	} else if (action == XWA_PILOT_PROFILE_PENDING_DELETE) {
		XwaModernPilotProfiles_DeleteConfirmed();
	}
	return g_pilotProfilesScreen.pending_action != XWA_PILOT_PROFILE_PENDING_NONE;
}

static void XwaModernPilotProfiles_FormatEntry(char* text, size_t text_size,
											   const XwaPilotProfileEntry* entry) {
	const char* active_text = entry->active ? " (Active)" : "";
	const char* duplicate_text = entry->duplicate ? " (Duplicate)" : "";

	snprintf(text, text_size, "%s%s%s", entry->name, active_text, duplicate_text);
}

int XwaModernPilotProfilesScreen_Update(int menu_center_x, int* cursor_row) {
	XwaModernOptionsMenu menu;
	FrontendRect rect;
	char row_text[96];
	char switch_text[64];
	char delete_text[64];
	const XwaPilotProfileEntry* selected_entry;
	int can_change_active;
	int list_top;
	int first_visible;
	int last_visible;
	int i;
	int selected_valid;
	int switch_disabled;
	int delete_disabled;
	int back;

	if (!cursor_row) {
		return 0;
	}
	if (!g_pilotProfilesScreen.initialized) {
		g_pilotProfilesScreen.initialized = 1;
		g_pilotProfilesScreen.selected_index = -1;
		g_pilotProfilesScreen.pending_profile_index = -1;
		g_pilotProfilesScreen.scroll_offset = 0;
		if (!XwaModernPilotProfiles_Refresh(g_pilotData.name)) {
			XwaModernPilotProfiles_ShowNotice("Pilot profiles could not be listed.", NULL);
			return 0;
		}
		/* Keep the initial focus consistent with the pilot named by the action buttons. */
		*cursor_row = g_pilotProfilesScreen.selected_index >= 0 ? g_pilotProfilesScreen.selected_index
																: g_pilotProfilesScreen.count + 1;
	}
	if (XwaModernPilotProfiles_HandlePending()) {
		return 0;
	}
	if (g_pilotProfilesScreen.focus_selected_after_refresh) {
		g_pilotProfilesScreen.focus_selected_after_refresh = 0;
		*cursor_row = g_pilotProfilesScreen.selected_index >= 0 ? g_pilotProfilesScreen.selected_index
																: g_pilotProfilesScreen.count + 1;
	}

	XwaModernOptionsMenu_Begin(&menu, menu_center_x, 55, cursor_row,
							   g_pilotProfilesScreen.count + XWA_PILOT_PROFILE_ACTION_COUNT);
	XwaModernOptionsMenu_DrawTitle(&menu, "Pilot Profiles");
	list_top = menu.y;
	if (*cursor_row >= 0 && *cursor_row < g_pilotProfilesScreen.count) {
		if (*cursor_row < g_pilotProfilesScreen.scroll_offset) {
			g_pilotProfilesScreen.scroll_offset = *cursor_row;
		} else if (*cursor_row >= g_pilotProfilesScreen.scroll_offset + XWA_PILOT_PROFILE_VISIBLE_ROWS) {
			g_pilotProfilesScreen.scroll_offset = *cursor_row - XWA_PILOT_PROFILE_VISIBLE_ROWS + 1;
		}
	}
	if (g_pilotProfilesScreen.count > XWA_PILOT_PROFILE_VISIBLE_ROWS) {
		FrontendDraw_RectAssign(&rect, 565, XWA_PILOT_PROFILE_LIST_Y, 584,
								XWA_PILOT_PROFILE_LIST_Y + XWA_PILOT_PROFILE_VISIBLE_ROWS * 20 - 1);
		g_pilotProfilesScreen.scroll_offset =
			FrontendScrollbar_Draw(&rect, g_pilotProfilesScreen.scroll_offset, g_pilotProfilesScreen.count, 0,
								   XWA_PILOT_PROFILE_VISIBLE_ROWS, g_colorNavy, 18);
	}
	first_visible = g_pilotProfilesScreen.scroll_offset;
	last_visible = first_visible + XWA_PILOT_PROFILE_VISIBLE_ROWS;
	if (last_visible > g_pilotProfilesScreen.count) {
		last_visible = g_pilotProfilesScreen.count;
	}
	menu.row = first_visible;
	menu.y = list_top;
	for (i = first_visible; i < last_visible; ++i) {
		XwaModernPilotProfiles_FormatEntry(row_text, sizeof(row_text), &g_pilotProfilesScreen.entries[i]);
		if (XwaModernOptionsMenu_DrawAction(&menu, row_text, 80 + i - first_visible, 0)) {
			g_pilotProfilesScreen.selected_index = i;
			*cursor_row = i;
		}
	}

	can_change_active = XwaModernPilotProfiles_CanChangeActive();
	if (!can_change_active) {
		FrontendDraw_RectAssign(&rect, 40, 280, 600, 295);
		FrontendText_DrawCentered(10, "Return to the concourse to switch pilots or create a new one.", &rect,
								  g_colorGray);
	}
	menu.row = g_pilotProfilesScreen.count;
	menu.y = XWA_PILOT_PROFILE_ACTION_Y;
	selected_valid = g_pilotProfilesScreen.selected_index >= 0 &&
					 g_pilotProfilesScreen.selected_index < g_pilotProfilesScreen.count;
	selected_entry =
		selected_valid ? &g_pilotProfilesScreen.entries[g_pilotProfilesScreen.selected_index] : NULL;
	if (selected_entry) {
		snprintf(switch_text, sizeof(switch_text), "Switch to %s", selected_entry->name);
		snprintf(delete_text, sizeof(delete_text), "Delete %s", selected_entry->name);
	} else {
		strcpy(switch_text, "Switch Pilot");
		strcpy(delete_text, "Delete Pilot");
	}
	switch_disabled =
		!can_change_active || !selected_entry || selected_entry->active || selected_entry->duplicate;
	if (XwaModernOptionsMenu_DrawAction(&menu, "Create and Switch to New Pilot", 101, !can_change_active)) {
		memset(g_pilotProfilesScreen.create_name, 0, sizeof(g_pilotProfilesScreen.create_name));
		g_pilotProfilesScreen.pending_action = XWA_PILOT_PROFILE_PENDING_NAME;
		FrontendDialog_PromptForPilotName(g_pilotProfilesScreen.create_name);
		return 0;
	}
	if (XwaModernOptionsMenu_DrawAction(&menu, switch_text, 100, switch_disabled)) {
		XwaModernPilotProfiles_BeginSwitch();
		return 0;
	}
	delete_disabled = !selected_entry || selected_entry->active;
	if (XwaModernOptionsMenu_DrawAction(&menu, delete_text, 102, delete_disabled)) {
		XwaModernPilotProfiles_BeginDelete();
		return 0;
	}
	back = XwaModernOptionsMenu_DrawAction(&menu, FrontendString_Get(STR_BACK), 103, 0);
	back |= XwaModernOptionsMenu_TakeEscape(&menu);
	if (!back) {
		return 0;
	}
	XwaModernPilotProfilesScreen_Leave();
	return 1;
}

void XwaModernPilotProfilesScreen_Leave(void) {
	XwaModernPilotProfiles_FreeList();
	g_pilotProfilesScreen.selected_index = -1;
	g_pilotProfilesScreen.scroll_offset = 0;
	g_pilotProfilesScreen.initialized = 0;
	g_pilotProfilesScreen.focus_selected_after_refresh = 0;
	g_pilotProfilesScreen.pending_profile_index = -1;
	g_pilotProfilesScreen.pending_action = XWA_PILOT_PROFILE_PENDING_NONE;
	memset(g_pilotProfilesScreen.create_name, 0, sizeof(g_pilotProfilesScreen.create_name));
}

int XwaModernPilotProfilesScreen_TakeActiveChanged(void) {
	int changed = g_pilotProfilesScreen.active_changed;

	g_pilotProfilesScreen.active_changed = 0;
	return changed;
}
