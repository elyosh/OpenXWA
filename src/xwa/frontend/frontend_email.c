#include "xwa/frontend/frontend_email.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/family_transport_room.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/util/memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x9F4B18
int g_frontendFamilyHasNewEmail;
// GLOBAL: XWA 0x9F4B10
int g_frontendEmailCount;
// GLOBAL: XWA 0x9F4B24
FrontendEmailEntry* g_frontendEmailEntries;
// GLOBAL: XWA 0x783B10
int g_frontendEmailSelectedRow;
// GLOBAL: XWA 0x7839CC
int g_frontendEmailScrollOffset;

static void FrontendEmail_StripTrailingLf(char* line) {
	size_t length;

	length = strlen(line);
	if (length != 0 && line[length - 1] == '\n') {
		line[length - 1] = '\0';
	}
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5638D0
static int FrontendEmail_CompareByFrom(const void* lhs, const void* rhs) {
	const FrontendEmailEntry* left;
	const FrontendEmailEntry* right;
	int result;

	left = (const FrontendEmailEntry*)lhs;
	right = (const FrontendEmailEntry*)rhs;
	result = strcmp(left->from, right->from);
	if (!result) {
		result = (unsigned int)left->emailIndex < (unsigned int)right->emailIndex ? 1 : -1;
	}

	if (g_pilotData.emailsSortCriterion & 1) {
		result = -result;
	}

	return result;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x563930
static int FrontendEmail_CompareByStatus(const void* lhs, const void* rhs) {
	const FrontendEmailEntry* left;
	const FrontendEmailEntry* right;
	int result;

	left = (const FrontendEmailEntry*)lhs;
	if (g_pilotData.emailsStatus[(unsigned int)left->emailIndex] == 2) {
		right = (const FrontendEmailEntry*)rhs;
		result = g_pilotData.emailsStatus[(unsigned int)right->emailIndex] != 2;
	} else {
		right = (const FrontendEmailEntry*)rhs;
		result = (g_pilotData.emailsStatus[(unsigned int)right->emailIndex] != 2) - 1;
	}

	if (!result) {
		result = (unsigned int)left->emailIndex < (unsigned int)right->emailIndex ? 1 : -1;
	}

	if (g_pilotData.emailsSortCriterion & 1) {
		result = -result;
	}

	return result;
}

// FUNCTION: XWA 0x563990
static int FrontendEmail_CompareBySubject(const void* lhs, const void* rhs) {
	const FrontendEmailEntry* left;
	const FrontendEmailEntry* right;
	int result;

	left = (const FrontendEmailEntry*)lhs;
	right = (const FrontendEmailEntry*)rhs;
	result = strcmp(left->subject, right->subject);
	if (!result) {
		result = (unsigned int)left->emailIndex < (unsigned int)right->emailIndex ? 1 : -1;
	}

	if (g_pilotData.emailsSortCriterion & 1) {
		result = -result;
	}

	return result;
}

// FUNCTION: XWA 0x5639F0
static int FrontendEmail_CompareByIndex(const void* lhs, const void* rhs) {
	int result;

	result = (unsigned int)((const FrontendEmailEntry*)lhs)->emailIndex <
					 (unsigned int)((const FrontendEmailEntry*)rhs)->emailIndex
				 ? 1
				 : -1;
	if (g_pilotData.emailsSortCriterion & 1) {
		result = -result;
	}

	return result;
}

// FUNCTION: XWA 0x563820
int FrontendEmail_SortEntries(void) {
	if (g_frontendEmailEntries == NULL) {
		return 0;
	}

	switch (g_pilotData.emailsSortCriterion) {
		case 0:
		case 1:
			qsort(g_frontendEmailEntries, (size_t)g_frontendEmailCount, sizeof(FrontendEmailEntry),
				  FrontendEmail_CompareByIndex);
			return 1;

		case 2:
		case 3:
			qsort(g_frontendEmailEntries, (size_t)g_frontendEmailCount, sizeof(FrontendEmailEntry),
				  FrontendEmail_CompareByFrom);
			return 1;

		case 4:
		case 5:
			qsort(g_frontendEmailEntries, (size_t)g_frontendEmailCount, sizeof(FrontendEmailEntry),
				  FrontendEmail_CompareByStatus);
			return 1;

		case 6:
		case 7:
			qsort(g_frontendEmailEntries, (size_t)g_frontendEmailCount, sizeof(FrontendEmailEntry),
				  FrontendEmail_CompareBySubject);
			return 1;

		default:
			return 1;
	}
}

// FUNCTION: XWA 0x5631A0
int FrontendEmail_LoadList(void) {
	XwaFile* stream;
	int count;
	char buffer[1024];

	if (g_frontendEmailEntries != NULL) {
		Mem_Free(g_frontendEmailEntries);
		g_frontendEmailEntries = NULL;
	}

	g_frontendEmailCount = 0;
	sprintf(g_frontendScratchBuffer, "%s\\email.txt", g_campaignDirNames[4]);
	stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "r");
	if (stream == NULL) {
		return 0;
	}

	count = 0;
	while (File_ReadLine(stream, buffer, sizeof(buffer))) {
		int missionId;

		if (buffer[0] == '/' && buffer[1] == '/') {
			continue;
		}

		missionId = atoi(buffer);
		if (g_frontendMissionLoaded && missionId > g_currentMissionId) {
			break;
		}

		do {
			if (!File_ReadLine(stream, buffer, sizeof(buffer))) {
				++count;
				g_frontendEmailCount = count;
				goto counted_entries;
			}
		} while (buffer[0] != '*');

		++count;
	}

	g_frontendEmailCount = count;

counted_entries:
	if (g_frontendEmailCount == 0) {
		File_Close(stream);
		return 0;
	}

	File_Seek(stream, 0, SEEK_SET);
	if (g_frontendEmailEntries != NULL) {
		Mem_Free(g_frontendEmailEntries);
		g_frontendEmailEntries = NULL;
	}

	g_frontendEmailEntries =
		(FrontendEmailEntry*)Mem_Alloc(sizeof(FrontendEmailEntry) * g_frontendEmailCount);
	if (g_frontendEmailEntries == NULL) {
		File_Close(stream);
		return 0;
	}

	memset(g_frontendEmailEntries, 0, sizeof(FrontendEmailEntry) * g_frontendEmailCount);

	count = 0;
	while (File_ReadLine(stream, buffer, sizeof(buffer))) {
		int missionId;

		if (buffer[0] == '/' && buffer[1] == '/') {
			continue;
		}

		missionId = atoi(buffer);
		if (g_frontendMissionLoaded && missionId > g_currentMissionId) {
			break;
		}

		g_frontendEmailEntries[count].field00 = missionId;
		while (File_ReadLine(stream, buffer, sizeof(buffer))) {
			if (buffer[0] == '*') {
				break;
			}

			if (buffer[0] == '/' && buffer[1] == '/') {
				continue;
			}

			FrontendEmail_StripTrailingLf(buffer);
			strncpy(g_frontendEmailEntries[count].from, Linez_ResolveString(buffer),
					sizeof(g_frontendEmailEntries[count].from) - 1);
			g_frontendEmailEntries[count].from[sizeof(g_frontendEmailEntries[count].from) - 1] = '\0';
			break;
		}

		while (File_ReadLine(stream, buffer, sizeof(buffer))) {
			if (buffer[0] == '*') {
				break;
			}

			if (buffer[0] == '/' && buffer[1] == '/') {
				continue;
			}

			FrontendEmail_StripTrailingLf(buffer);
			strncpy(g_frontendEmailEntries[count].subject, Linez_ResolveString(buffer),
					sizeof(g_frontendEmailEntries[count].subject) - 1);
			g_frontendEmailEntries[count].subject[sizeof(g_frontendEmailEntries[count].subject) - 1] = '\0';
			break;
		}

		g_frontendEmailEntries[count].body[0] = '\0';
		while (buffer[0] != '*' && File_ReadLine(stream, buffer, sizeof(buffer))) {
			char* resolvedLine;

			if (buffer[0] == '*') {
				break;
			}

			if (buffer[0] == '/' && buffer[1] == '/') {
				continue;
			}

			FrontendEmail_StripTrailingLf(buffer);
			if (g_frontendEmailEntries[count].body[0]) {
				strncat(g_frontendEmailEntries[count].body, "$",
						sizeof(g_frontendEmailEntries[count].body) -
							strlen(g_frontendEmailEntries[count].body) - 1);
			}

			resolvedLine = Linez_ResolveString(buffer);
			strncat(g_frontendEmailEntries[count].body, resolvedLine,
					sizeof(g_frontendEmailEntries[count].body) - strlen(g_frontendEmailEntries[count].body) -
						1);
		}

		g_frontendEmailEntries[count].emailIndex = count;
		++count;
		if (count >= g_frontendEmailCount || buffer[0] != '*') {
			break;
		}
	}

	File_Close(stream);
	FrontendEmail_SortEntries();
	return 0;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x560800
int FrontendEmail_DrawInbox(void) {
	int outX;
	int outY;
	int iconOffsetX;
	int iconOffsetY;
	int firstVisibleRow;
	int rowIndex;
	int entryIndex;
	FrontendRect rect;
	FrontendRect out;

	FrontendCursor_GetPos(&outX, &outY);
	if (g_frontendFamilyPageResetPending) {
		g_frontendEmailSelectedRow = 0;
		g_frontendEmailScrollOffset = 0;
		g_frontendFamilyPageResetPending = 0;
	}

	FrontendDraw_RectAssign(&rect, 70, 60, 570, 230);
	FrontendDraw_RectOutline(&rect, 0, 0, (short)g_colorSlateBlue);
	FrontendDraw_RectAssign(&rect, 70, 60, 210, 80);
	FrontendDraw_RectOutline(&rect, 0, 0, (short)g_colorSlateBlue);
	if (FrontendButton_DrawMenuButton(rect.left + 2, rect.top + 4, FrontendString_Get(STR_FAMILY_FROM), 12,
									  g_colorPaleBlue, 20, 0, "settingsound")) {
		g_pilotData.emailsSortCriterion = (unsigned char)((g_pilotData.emailsSortCriterion == 2) + 2);
		FrontendEmail_SortEntries();
	}

	FrontendDraw_RectAssign(&rect, 210, 60, 250, 80);
	FrontendDraw_RectOutline(&rect, 0, 0, (short)g_colorSlateBlue);
	FrontImage_GetResourceRect("emailnew", &out);
	iconOffsetX = (out.left + rect.right - rect.left - out.right) >> 1;
	FrontendDraw_RectOffsetXY(&out, iconOffsetX + rect.left,
							  rect.top + ((rect.bottom + out.top - rect.top - out.bottom) >> 1));
	if (FrontendButton_DrawSpriteWithHoverText(
			&out, "emailnew", "emailnew", (void*)FrontendString_Get(STR_FAMILY_MESSAGE_STATUS),
			(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 23, "settingsound")) {
		g_pilotData.emailsSortCriterion = (unsigned char)((g_pilotData.emailsSortCriterion == 4) + 4);
		FrontendEmail_SortEntries();
	}
	iconOffsetY = out.top - out.bottom + 14;

	FrontendDraw_RectAssign(&rect, 250, 60, 500, 80);
	FrontendDraw_RectOutline(&rect, 0, 0, (short)g_colorSlateBlue);
	if (FrontendButton_DrawMenuButton(rect.left + 2, rect.top + 4, FrontendString_Get(STR_FAMILY_SUBJECT), 12,
									  g_colorPaleBlue, 21, 0, "settingsound")) {
		g_pilotData.emailsSortCriterion = (unsigned char)((g_pilotData.emailsSortCriterion == 6) + 6);
		FrontendEmail_SortEntries();
	}

	FrontendDraw_RectAssign(&rect, 500, 60, 570, 80);
	FrontendDraw_RectOutline(&rect, 0, 0, (short)g_colorSlateBlue);
	if (FrontendButton_DrawMenuButton(rect.left + 2, rect.top + 4, FrontendString_Get(STR_FAMILY_MESSAGE_ID),
									  12, g_colorPaleBlue, 22, 0, "settingsound")) {
		g_pilotData.emailsSortCriterion = (unsigned char)(g_pilotData.emailsSortCriterion == 0);
		FrontendEmail_SortEntries();
	}

	FrontendDraw_RectAssign(&rect, 550, 80, 569, 230);
	if ((unsigned int)g_frontendEmailCount <= 10u) {
		FrontendDraw_RectAssign(&rect, 72, 80, 567, 94);
	} else {
		g_frontendEmailScrollOffset = FrontendScrollbar_Draw(&rect, g_frontendEmailScrollOffset,
															 g_frontendEmailCount, 0, 5, g_colorNavy, 5);
		FrontendDraw_RectAssign(&rect, 72, 80, 547, 94);
	}

	firstVisibleRow = g_frontendEmailScrollOffset;
	rowIndex = g_frontendEmailScrollOffset;
	if (g_frontendEmailScrollOffset < g_frontendEmailCount) {
		entryIndex = g_frontendEmailScrollOffset;
		while (rowIndex - firstVisibleRow < 10) {
			unsigned int color;

			if (rowIndex == g_frontendEmailSelectedRow) {
				color = (unsigned int)g_colorGreen;
			} else if (FrontendDraw_PointInRect(&rect, outX, outY)) {
				if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
					color = (unsigned int)g_colorRed;
					g_frontendEmailSelectedRow = rowIndex;
					FrontendSound_PlayUISound("settingsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
											  63);
				} else {
					color = (unsigned int)g_colorYellow;
				}
			} else {
				color = (unsigned int)g_colorSlateBlue;
				if (g_pilotData.emailsStatus[g_frontendEmailEntries[entryIndex].emailIndex] != 2) {
					color = (unsigned int)g_colorPaleBlue;
				}
			}

			FrontendDraw_RectCopy(&out, &rect);
			out.right = 208;
			FrontendDisplay_SetScreenClipRect640x480(&out);
			FrontendText_Draw(12, g_frontendEmailEntries[entryIndex].from, out.left, out.top, (int)color);

			out.left = 210;
			out.right = 250;
			FrontendDisplay_SetScreenClipRect640x480(&out);
			FrontendDraw_RectOffsetXY(&out, iconOffsetX, iconOffsetY >> 1);
			if ((unsigned int)g_pilotData.emailsStatus[g_frontendEmailEntries[entryIndex].emailIndex] < 2u) {
				FrontImage_DrawSprite("emailnew", out.left, out.top);
			} else {
				FrontImage_DrawSprite("emailread", out.left, out.top);
			}

			FrontendDraw_RectCopy(&out, &rect);
			out.left = 252;
			out.right = 498;
			FrontendDisplay_SetScreenClipRect640x480(&out);
			FrontendText_Draw(12, g_frontendEmailEntries[entryIndex].subject, out.left, out.top, (int)color);

			out.left = 502;
			out.right = rect.right;
			FrontendDisplay_SetScreenClipRect640x480(&out);
			sprintf(g_frontendScratchBuffer, "%d", g_frontendEmailEntries[entryIndex].emailIndex + 5068);
			FrontendText_Draw(12, g_frontendScratchBuffer, out.left, out.top, (int)color);

			FrontendDraw_RectOffsetXY(&rect, 0, 15);
			++rowIndex;
			++entryIndex;
			if (rowIndex >= g_frontendEmailCount) {
				break;
			}
		}
	}

	FrontendDisplay_ResetScreenClipRect();
	FrontendDraw_RectAssign(&rect, 70, 232, 570, 412);
	FrontendDraw_RectOutline(&rect, 0, 0, (short)g_colorSlateBlue);
	FrontendDraw_RectInsetXY(&rect, 2, 0);
	if (g_frontendEmailSelectedRow < g_frontendEmailCount) {
		FrontendText_DrawWrappedClipped(12, g_frontendEmailEntries[g_frontendEmailSelectedRow].body, &rect,
										g_colorLightBlue, 3, 0);
		g_pilotData.emailsStatus[g_frontendEmailEntries[g_frontendEmailSelectedRow].emailIndex] = 2;
	}

	return 1;
}
