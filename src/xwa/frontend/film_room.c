#include "xwa/frontend/film_room.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/family_transport_room.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_dialog.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_escape.h"
#include "xwa/frontend/frontend_flight.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/movie/movie.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// GLOBAL: XWA 0x5FFB90
int g_filmFeatureEnabled;
// GLOBAL: XWA 0x783FE8
int g_filmRoomListScrollOffset;
// GLOBAL: XWA 0x783FEC
FilmRoomPendingAction g_filmRoomPendingAction;
// GLOBAL: XWA 0x783FF0
int g_filmRoomSelectedFilmIndex;
// GLOBAL: XWA 0x783FF4
FrontFilenameList* g_filmRoomFilmList;

extern int g_currentCdDisk;

// FUNCTION: XWA 0x573640
int FilmRoom_UpdateRightBarAnimation(void) {
	FrontendRect rightBarRect;
	char rightBarName[32] = "rightbar2";
	int animState;

	rightBarName[8] = (char)(g_frontendRightBarPanelIndex + '0');
	FrontImage_GetResourceRect(rightBarName, &rightBarRect);

	animState = g_frontendRightBarAnimState;
	if (!animState) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (FrontImage_GetSpriteFrame(rightBarName) == 9) {
			g_frontendRightBarAnimState = 1;
			return 1;
		}
	} else if (animState == 1) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		return 1;
	} else if (animState == 2) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			g_frontendRightBarAnimState = 3;
			return 1;
		}
	} else if (animState == 4) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 1;
			if (g_gameConfig.sfxDatapadEnabled) {
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}
			FrontendCursor_SetPos(g_frontendSidebarButtonRects[9].left + 10,
								  g_frontendSidebarButtonRects[9].top + 10);
		}
	}

	return 1;
}

// FUNCTION: XWA 0x573870
int FilmRoom_DrawFilmList(int frameCounter) {
	FrontFilenameList* filmList;
	FrontFilenameListNode* node;
	int filmIndex;
	int visibleRows;
	int activated;
	int nextFilmIndex;
	int mouseY;
	int mouseX;
	FrontendRect rect;

	(void)frameCounter;

	FrontendDraw_RectAssign(&rect, 72, 90, 480, 345);
	if (g_filmRoomFilmList->count > 17) {
		FrontendDraw_RectAssign(&rect, 551, 90, 570, 345);
		g_filmRoomListScrollOffset = FrontendScrollbar_Draw(&rect, g_filmRoomListScrollOffset,
															g_filmRoomFilmList->count, 0, 5, g_colorNavy, 8);
	}

	FrontendCursor_GetPos(&mouseX, &mouseY);
	FrontendDraw_RectAssign(&rect, 80, 90, 550, 104);

	filmList = g_filmRoomFilmList;
	node = filmList->head;
	filmIndex = 0;
	visibleRows = 0;
	activated = 0;
	nextFilmIndex = 0;

	if (filmIndex < filmList->count) {
		do {
			if (filmIndex < g_filmRoomListScrollOffset) {
				node = node->next;
			} else {
				uint16_t rowColor;

				rowColor = (uint16_t)g_colorPaleBlue;
				if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
					rowColor = (uint16_t)g_colorYellow;
					if (FrontendMouse_GetLeftDblClick() || FrontendMouse_GetRightDblClick()) {
						if (g_filmRoomSelectedFilmIndex != filmIndex) {
							rowColor = (uint16_t)g_colorRed;
							if (g_gameConfig.sfxDatapadEnabled) {
								FrontendSound_PlayUISound("jewelsound", 1, 0, 255,
														  12 * g_gameConfig.sfxDatapadVolume, 63);
							}
							g_filmRoomSelectedFilmIndex = filmIndex;
						}
						activated = 1;
					} else if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
						if (g_filmRoomSelectedFilmIndex != filmIndex) {
							rowColor = (uint16_t)g_colorRed;
							if (g_gameConfig.sfxDatapadEnabled) {
								FrontendSound_PlayUISound("jewelsound", 1, 0, 255,
														  12 * g_gameConfig.sfxDatapadVolume, 63);
							}
							g_filmRoomSelectedFilmIndex = filmIndex;
						}
					}
				}

				if (g_filmRoomSelectedFilmIndex == filmIndex) {
					rowColor = (uint16_t)g_colorGreen2;
				}

				strcpy(g_frontendScratchBuffer, node->fileName);
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 4] = '\0';
				FrontendText_DrawAlignedInRect(12, g_frontendScratchBuffer, &rect, 0, 1, rowColor);
				FrontendDraw_RectOffsetXY(&rect, 0, 15);

				node = node->next;
				if (++visibleRows >= 17) {
					break;
				}

				filmList = g_filmRoomFilmList;
				filmIndex = nextFilmIndex;
			}
			nextFilmIndex = ++filmIndex;
		} while (filmIndex < filmList->count);
	}

	return activated;
}

// FUNCTION: XWA 0x573AE0
int FilmRoom_DrawLoadDeleteButtons(void) {
	FrontendRect buttonRect;
	FrontendRect clippedRect;
	int mouseX;
	int mouseY;
	unsigned int textColor;
	int pressed;
	FrontFilenameListNode* node;
	int filmCount;
	int filmIndex;

	FrontendCursor_GetPos(&mouseX, &mouseY);

	FrontendDraw_RectAssign(&buttonRect, 220, 351, 319, 425);
	textColor = (unsigned int)g_colorPaleBlue;
	FrontendDraw_RectCopy(&clippedRect, &buttonRect);
	FrontendDraw_RectClipToBounds(&clippedRect);
	if (FrontendDraw_PointInRect(&clippedRect, mouseX, mouseY)) {
		textColor = (unsigned int)g_colorYellow;
	}

	pressed = FrontendButton_DrawSpriteHitTest(&buttonRect, "loadsetting", "loadsetting", NULL, 10,
											   g_colorLightBlue, 20, "settingsound");
	FrontendDraw_RectAssign(&buttonRect, 220, 351, 319, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(STR_GAME_LOAD), &buttonRect, textColor);
	if (pressed) {
		return 1;
	}

	FrontendDraw_RectAssign(&buttonRect, 320, 351, 419, 425);
	textColor = (unsigned int)g_colorPaleBlue;
	FrontendDraw_RectCopy(&clippedRect, &buttonRect);
	FrontendDraw_RectClipToBounds(&clippedRect);
	if (FrontendDraw_PointInRect(&clippedRect, mouseX, mouseY)) {
		textColor = (unsigned int)g_colorYellow;
	}

	pressed = FrontendButton_DrawSpriteHitTest(&buttonRect, "deletesetting", "deletesettingd", NULL, 10,
											   g_colorLightBlue, 22, "settingsound");
	FrontendDraw_RectAssign(&buttonRect, 320, 351, 419, 363);
	FrontendText_DrawCentered(10, FrontendString_Get(STR_GAME_DELETE), &buttonRect, textColor);
	if (pressed) {
		if (FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_FILM_CONFIRM_DELETE1),
											 FrontendString_Get(STR_FILM_CONFIRM_DELETE2),
											 FrontendString_Get(STR_FILM_CONFIRM_DELETE3),
											 FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL))) {
			node = g_filmRoomFilmList->head;
			g_frontendScratchBuffer[0] = '\0';
			filmCount = g_filmRoomFilmList->count;
			filmIndex = 0;
			while (filmIndex < filmCount) {
				if (filmIndex == g_filmRoomSelectedFilmIndex) {
					sprintf(g_frontendScratchBuffer, "film\\%s", node->fileName);
					break;
				}
				node = node->next;
				++filmIndex;
			}

			if (g_frontendScratchBuffer[0]) {
				File_Remove(AERON_VFS_ROOT_USER, g_frontendScratchBuffer);
				g_filmRoomSelectedFilmIndex = 0;
				if (g_filmRoomFilmList) {
					FrontendFileList_Free(g_filmRoomFilmList);
					g_filmRoomFilmList = NULL;
				}

				g_filmRoomFilmList = FrontendFileList_BuildSorted(AERON_VFS_ROOT_USER, "film\\*.flm");
				if (!g_filmRoomFilmList->count) {
					g_frontendRightBarAnimState = 4;
					if (g_gameConfig.sfxDatapadEnabled) {
						FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
												  63);
					}
				}
			}
		}
	}

	return 0;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x573020
int FilmRoom_Update(int frameCounter) {
	FrontendRect rect;
#ifdef XWA_MODERN
	static int entryMoviePending;

	if (frameCounter == 0 || entryMoviePending) {
		if (!entryMoviePending && !g_skipFrontendEntryMovie) {
			if (Movie_Play("pod", 1)) {
				entryMoviePending = 1;
				return 0;
			}
		}

		entryMoviePending = 0;
#else
	if (frameCounter == 0) {
		if (!g_skipFrontendEntryMovie) {
			Movie_Play("pod", 1);
		}
#endif
		g_skipFrontendEntryMovie = 0;
		g_filmRoomPendingAction = FILM_ROOM_ACTION_NONE;
		g_frontendLeftBarAnimState = 3;
		g_frontendRightBarAnimState = 0;
		g_filmRoomListScrollOffset = 0;

		if (g_filmRoomFilmList != NULL) {
			FrontendFileList_Free(g_filmRoomFilmList);
			g_filmRoomFilmList = NULL;
		}

		g_filmRoomFilmList = FrontendFileList_BuildSorted(AERON_VFS_ROOT_USER, "film\\*.flm");
		if (g_filmRoomFilmList->count == 0) {
			g_frontendRightBarPanelIndex = 1;
			FrontImage_SetSpriteFrame("rightbar1", 0);
		} else {
			g_frontendRightBarPanelIndex = 2;
			FrontImage_SetSpriteFrame("rightbar2", 0);
		}

		FrontendText_SetGlyphGradientBg(g_colorNearBlack);
		FrontImage_RegisterResourceDefault("frontres\\combat\\multiplayer.bmp", "background");
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontImage_DrawSprite("settingbar", 57, 348);
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontImage_DrawSprite("settingbar", 57, 348);
		FrontendDisplay_UnlockOffscreenSurface(1);
		if (g_gameConfig.sfxDatapadEnabled) {
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}
	}

	if (g_filmRoomPendingAction != FILM_ROOM_ACTION_NONE) {
		FilmRoom_UpdateRightBarAnimation();
		if (g_frontendLeftBarAnimState != 3 || g_frontendRightBarAnimState != 3) {
			return 0;
		}

		if (g_filmRoomPendingAction != FILM_ROOM_ACTION_EXIT_CONCOURSE) {
			FrontFilenameListNode* node;
			int filmCount;
			int filmIndex;

			if (g_filmRoomPendingAction != FILM_ROOM_ACTION_PLAY_SELECTED_FILM) {
				return 0;
			}

			node = g_filmRoomFilmList->head;
			g_filmFilePath[0] = '\0';
			filmCount = g_filmRoomFilmList->count;
			filmIndex = 0;
			while (filmIndex < filmCount) {
				if (filmIndex == g_filmRoomSelectedFilmIndex) {
					sprintf(g_filmFilePath, "film\\%s", node->fileName);
					break;
				}
				node = node->next;
				++filmIndex;
			}

			if (g_filmFilePath[0]) {
				XwaFile* filmFile;

				filmFile = File_Open(AERON_VFS_ROOT_USER, g_filmFilePath, "rb");
				if (filmFile != NULL) {
					int requiredDisk;
					int continuePlayback;

					File_ReadWord(filmFile, &frameCounter);
					requiredDisk = (uint16_t)frameCounter;
					File_Close(filmFile);

					continuePlayback = 1;
					if (requiredDisk < 2 && requiredDisk != g_currentCdDisk) {
						do {
							continuePlayback = 0;
							if (File_CheckGameCdPresent(requiredDisk)) {
								g_currentCdDisk = requiredDisk;
								continuePlayback = 1;
							} else {
								if (requiredDisk != 0) {
									continuePlayback = FrontendDialog_ShowConfirmDialog(
										FrontendString_Get(STR_FAILED_TO_DETECT2_1),
										FrontendString_Get(STR_FAILED_TO_DETECT2_2), NULL,
										FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL));
								} else {
									continuePlayback = FrontendDialog_ShowConfirmDialog(
										FrontendString_Get(STR_FAILED_TO_DETECT1_1),
										FrontendString_Get(STR_FAILED_TO_DETECT1_2), NULL,
										FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL));
								}
							}
						} while (continuePlayback && requiredDisk != g_currentCdDisk);
					}

					if (continuePlayback) {
						strcpy(g_mpRoster[0].name, g_pilotData.name);
						g_mpRoster[0].playerId = 1;
						g_mpRoster[0].rating = g_pilotData.pilotRating;
						g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_SINGLEPLAYER;
						g_pilotData.campaignMode = 0;
						g_unusedFrontendMissionLaunchPrepared = 1;
						FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
						return 0;
					}
				}

				g_filmRoomPendingAction = FILM_ROOM_ACTION_NONE;
				g_frontendRightBarAnimState = 0;
				if (g_gameConfig.sfxDatapadEnabled) {
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				}
				return 0;
			}
		}

		g_filmFilePath[0] = '\0';
		FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
		return 0;
	}

	FrontendDraw_RectAssign(&rect, 65, 60, 575, 75);
	FrontendText_DrawCentered(12, FrontendString_Get(STR_FILM_ROOM), &rect, g_colorLightBlue);

	if (g_filmRoomFilmList != NULL && g_filmRoomFilmList->count > 0 && FilmRoom_DrawFilmList(frameCounter) &&
		g_filmRoomFilmList->count > 0) {
		if (g_gameConfig.sfxDatapadEnabled) {
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}
		g_frontendRightBarAnimState = 2;
		g_filmRoomPendingAction = FILM_ROOM_ACTION_PLAY_SELECTED_FILM;
	}

	FilmRoom_UpdateRightBarAnimation();
	if (g_frontendRightBarAnimState == 1) {
		if (g_frontendRightBarPanelIndex == 2) {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[8]);
		} else {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
		}

		if (FrontendButton_DrawSpriteWithHoverText(&rect, "back", "back", (void*)FrontendString_Get(STR_BACK),
												   g_colorPaleBlue, g_colorLightBlue, 240, "jewelsound")) {
			g_frontendRightBarAnimState = 2;
			if (g_gameConfig.sfxDatapadEnabled) {
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}
			g_filmRoomPendingAction = FILM_ROOM_ACTION_EXIT_CONCOURSE;
		}

		if (g_frontendRightBarPanelIndex == 2) {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
			if (FrontendButton_DrawSpriteWithHoverText(
					&rect, "connect", "connect", (void*)FrontendString_Get(STR_PLAY_FILM), g_colorPaleBlue,
					g_colorLightBlue, 241, "jewelsound")) {
				g_frontendRightBarAnimState = 2;
				if (g_gameConfig.sfxDatapadEnabled) {
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				}
				g_filmRoomPendingAction = FILM_ROOM_ACTION_PLAY_SELECTED_FILM;
			}
		}
	}

	if (FilmRoom_DrawLoadDeleteButtons() && g_filmRoomFilmList->count > 0) {
		if (g_gameConfig.sfxDatapadEnabled) {
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}
		g_frontendRightBarAnimState = 2;
		g_filmRoomPendingAction = FILM_ROOM_ACTION_PLAY_SELECTED_FILM;
	}

	return Frontend_HandleEscapeQuit(1) == 1;
}

// FUNCTION: XWA 0x572FE0
int FilmRoom_Exit(int frameCounter) {
	(void)frameCounter;

	FrontImage_FreeResourceByName("background");
	Frontend_ResetScrollableControls();
	if (g_filmRoomFilmList != NULL) {
		FrontendFileList_Free(g_filmRoomFilmList);
		g_filmRoomFilmList = NULL;
	}

	return 0;
}
