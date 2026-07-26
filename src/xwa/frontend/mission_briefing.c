#include "xwa/frontend/mission_briefing.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif
#include "xwa/flight/fediskio.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/flight_model.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/briefing_script.h"
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
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_mission_list.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/frontend_wave_stream.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/model_preview.h"
#include "xwa/frontend/tech_library.h"
#include "xwa/net/net.h"
#include "xwa/util/memory.h"
#include "xwa/util/time.h"

#include "xwa/util/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	BRIEFING_DEATH_STAR_OBJECT_TYPE = 227,
	BRIEFING_PREVIEW_SCRATCH_PITCH = 1280,
	BRIEFING_PREVIEW_SCRATCH_HEIGHT = 480,
	BRIEFING_TRANSPARENT_WORD = 8224,
};

// GLOBAL: XWA 0x9EB808
void* g_briefingSelectionImageBuffer;
// GLOBAL: XWA 0x9EB80C
void* g_briefingPreviewImageBuffer;
// GLOBAL: XWA 0x9EB876
FrontendRect g_briefingMapSourceRect;
// GLOBAL: XWA 0x783BDC
short g_briefingMapMajorGridColor;
// GLOBAL: XWA 0x783C00
short g_briefingMapMinorGridColor;
// GLOBAL: XWA 0x783BD4
int clipBottomAdjust;
// GLOBAL: XWA 0x783B88
int Seed;
// GLOBAL: XWA 0x783B94
int g_briefingSelectionRevealComplete;
// GLOBAL: XWA 0x783B98
int g_briefingModelLightDx;
// GLOBAL: XWA 0x783BD8
int g_briefingModelLightDy;
// GLOBAL: XWA 0x783BF4
int g_briefingModelLightDz;
// GLOBAL: XWA 0x783BE0
int g_briefingDsBriefAnimTick;
// GLOBAL: XWA 0x783BEC
int g_briefingSelectionStatsRevealDone;
// GLOBAL: XWA 0x783BF0
int g_briefingSelectionPreviewBaseY;
// GLOBAL: XWA 0x783C04
int g_briefingSelectionRevealRow;
// GLOBAL: XWA 0x783C1C
int g_briefingMapRegionTransitionFrame;
// GLOBAL: XWA 0x783C18
int g_missionBriefingTextViewActive;
// GLOBAL: XWA 0x783BE4
int g_missionBriefingPendingExitAction;
// GLOBAL: XWA 0x783C10
int g_missionBriefingLaunchSent;
// GLOBAL: XWA 0x9F60C0
int g_missionBriefingReadyPlayerCount;
// GLOBAL: XWA 0x9F60A0
int g_missionBriefingReadyPlayerIds[8];
// GLOBAL: XWA 0x9EB810
int g_frontendMissionOpcode99Count;
// GLOBAL: XWA 0x783B8C
int g_unusedBriefingSelectionImageLoadState;
// GLOBAL: XWA 0x783BE8
int g_missionBriefingLaunchCountdownMs;
// GLOBAL: XWA 0x783B90
int g_missionBriefingTickNowMs;
// GLOBAL: XWA 0x783BF8
int g_missionBriefingLastTickMs;
// GLOBAL: XWA 0x783C14
int g_missionBriefingLastCountdownSecondSent;
// GLOBAL: XWA 0x783BD0
float yawDeg;
// GLOBAL: XWA 0x783BFC
float rollDeg;
// GLOBAL: XWA 0x783B9C
float angleDeg;
// GLOBAL: XWA 0x783C0C
float g_briefingModelDefaultPitchDeg;
// GLOBAL: XWA 0x5AB8F4
const float g_briefingYawStep = -5.0f;
// GLOBAL: XWA 0x5AB8F8
const double g_briefingFullTurnDegrees = 360.0;
// GLOBAL: XWA 0x5AB900
const double g_briefingSizeMetersThreshold = 1000.0;
// GLOBAL: XWA 0x5AB908
const double g_briefingKilometersScale = 0.001;
// GLOBAL: XWA 0x783BA0
CraftTechStats stats;
extern int g_currentCdDisk;

static __inline int BriefingMap_GetSelectedObjectType(void) {
	return (uint8_t)g_briefingMapCurrentRegionIcons[g_briefingMapPrimaryHighlightIconIndex].objectType;
}

static __inline void BriefingMap_SetPreviewOrientation(int objectType) {
	int category;

	category = g_shipList[g_shipTypeToShipListIndex[objectType]].category;
	if (category != 7 && category != 8 && category != 9 && category != 10) {
		ModelPreview_SetObjectEulerDegrees((float)g_briefingModelDefaultPitchDeg, (float)yawDeg,
										   (float)rollDeg);
	} else {
		ModelPreview_SetObjectEulerDegrees(90.0f, (float)yawDeg, (float)rollDeg);
	}
	ModelPreview_SetObjectAngleDDegrees((float)angleDeg);
}

static __inline void BriefingMap_DrawDatapadBeams(FrontendRect* rect, int y, int count) {
	int i;
	int width;
	int16_t x;

	if (count <= 0) {
		return;
	}

	width = (int16_t)(rect->right - rect->left - 9);
	for (i = 0; i < count; ++i) {
		x = (int16_t)(rand() % width + rect->left + 5);
		if (g_pilotData.campaignMode) {
			if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				FrontendDraw_LineAntialiased(x, (int16_t)y, 616, 338, g_textShadeRamps[3][rand() % 6]);
			} else {
				FrontendDraw_LineAntialiased(x, (int16_t)y, 320, 450, g_textShadeRamps[3][rand() % 6]);
			}
		} else {
			FrontendDraw_LineAntialiased(x, (int16_t)y, 358, 443, g_textShadeRamps[3][rand() % 6]);
		}
	}
}

static __inline void BriefingMap_DrawDatapadBeamsFromX(FrontendRect* rect, int x, int count) {
	int i;
	int height;
	int16_t y;

	if (count <= 0) {
		return;
	}

	height = (int16_t)(rect->bottom - rect->top - 9);
	for (i = 0; i < count; ++i) {
		y = (int16_t)(rand() % height + rect->top + 5);
		if (g_pilotData.campaignMode) {
			if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				FrontendDraw_LineAntialiased((int16_t)x, y, 616, 338, g_textShadeRamps[3][rand() % 6]);
			} else {
				FrontendDraw_LineAntialiased((int16_t)x, y, 320, 450, g_textShadeRamps[3][rand() % 6]);
			}
		} else {
			FrontendDraw_LineAntialiased((int16_t)x, y, 358, 443, g_textShadeRamps[3][rand() % 6]);
		}
	}
}

static __inline void BriefingMap_ClearPreviewScratchRect(void* buffer, const FrontendRect* rect) {
	int row;
	int rowCount;
	unsigned char* pixels;
	int byteCount;

	rowCount = (int16_t)(rect->bottom - rect->top + 1);
	pixels = (unsigned char*)buffer + BRIEFING_PREVIEW_SCRATCH_PITCH * rect->top + 2 * rect->left;
	byteCount = (int16_t)(2 * (rect->right - rect->left) + 2);
	for (row = 0; row < rowCount; ++row) {
		memset(pixels, 0x20, (size_t)byteCount);
		pixels += BRIEFING_PREVIEW_SCRATCH_PITCH;
	}
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x567BD0
int16_t BriefingMap_MouseInputStub(FrontendRect* viewportRect, FrontendRect* clipRect, int leftDown,
								   int rightDown, int mouseX, int mouseY) {
	(void)viewportRect;
	(void)clipRect;
	(void)leftDown;
	(void)rightDown;
	(void)mouseX;
	(void)mouseY;

	return 1;
}

// FUNCTION: XWA 0x56AEB0
int MissionBriefing_HandleFlyPacketStub(void) { return 1; }

// FUNCTION: XWA 0x5667A0
void BriefingText_FreeAllocatedBuffers(void) {
	char** text;
	int remaining;

	text = g_frontendBriefingContent.mapLabelTexts;
	for (remaining = FRONTEND_BRIEFING_MAP_LABEL_COUNT; remaining != 0; --remaining) {
		if (*text != NULL) {
			Mem_Free(*text);
			*text = NULL;
		}
		++text;
	}

	text = g_frontendBriefingContent.textBlocks;
	for (remaining = FRONTEND_BRIEFING_TEXT_BLOCK_COUNT; remaining != 0; --remaining) {
		if (*text != NULL) {
			Mem_Free(*text);
			*text = NULL;
		}
		++text;
	}
}

// FUNCTION: XWA 0x566530
int16_t BriefingText_FreeAllocatedBuffersExit(void) {
	BriefingText_FreeAllocatedBuffers();
	return 1;
}

// FUNCTION: XWA 0x567960
void BriefingMap_ProjectPointToViewport(FrontendRect* viewport, int16_t mapX, int16_t mapY, int16_t* outX,
										int16_t* outY) {
	*outX = (int16_t)(g_briefingMapScale.x * (mapX - g_briefingMapCenter.x) / 256);
	*outX = (int16_t)(*outX + viewport->left + ((viewport->right - viewport->left) >> 1));

	*outY = (int16_t)(g_briefingMapScale.y * (mapY - g_briefingMapCenter.y) / 256);
	*outY = (int16_t)(*outY + viewport->top + ((viewport->bottom - viewport->top) >> 1));
}

// FUNCTION: XWA 0x5679F0
int16_t BriefingMap_StepS16TowardTarget(int16_t current, int16_t target, int step) {
	if (current > target) {
		current -= step;
		if (current < target) {
			current = target;
		}
	}
	if (current < target) {
		current += step;
		if (current > target) {
			current = target;
		}
	}
	return current;
}

// FUNCTION: XWA 0x567A20
void BriefingMap_AnimateViewState(void) {
	int16_t scaleDelta;
	int step;
	int centerScaleStep;
	int16_t centerDelta;
	int16_t scaledCenterDelta;
	int16_t currentX;
	int16_t targetX;
	int i;

	targetX = g_briefingMapTargetScale.x;
	currentX = g_briefingMapScale.x;
	scaleDelta = (int16_t)abs(currentX - targetX);
	if (scaleDelta < (int16_t)abs(g_briefingMapScale.y - g_briefingMapTargetScale.y)) {
		scaleDelta = (int16_t)abs(g_briefingMapScale.y - g_briefingMapTargetScale.y);
	}
	step = 2;
	if (scaleDelta >= 12) {
		step = 8;
	}
	if (currentX < 10) {
		step = 1;
	}
	g_briefingMapScale.x = BriefingMap_StepS16TowardTarget(currentX, targetX, step);
	g_briefingMapScale.y =
		BriefingMap_StepS16TowardTarget(g_briefingMapScale.y, g_briefingMapTargetScale.y, step);

	if (g_briefingMapScale.x) {
		centerScaleStep = 256 / g_briefingMapScale.x + 1;
	} else {
		centerScaleStep = 1;
	}

	currentX = g_briefingMapCenter.x;
	targetX = g_briefingMapTargetCenter.x;
	centerDelta = (int16_t)abs(currentX - targetX);
	if (centerDelta < (int16_t)abs(g_briefingMapCenter.y - g_briefingMapTargetCenter.y)) {
		centerDelta = (int16_t)abs(g_briefingMapCenter.y - g_briefingMapTargetCenter.y);
	}
	scaledCenterDelta = (int16_t)(centerDelta / (int16_t)centerScaleStep);
	centerScaleStep = (int)((unsigned int)centerScaleStep << 1);
	if (scaledCenterDelta >= 16) {
		centerScaleStep = (int)((unsigned int)centerScaleStep << 1);
	}
	g_briefingMapCenter.x = BriefingMap_StepS16TowardTarget(currentX, targetX, centerScaleStep);
	g_briefingMapCenter.y =
		BriefingMap_StepS16TowardTarget(g_briefingMapCenter.y, g_briefingMapTargetCenter.y, centerScaleStep);

	{
		int16_t* active = g_briefingMapFgMarkers.active;
		int16_t* age = g_briefingMapFgMarkers.age;

		for (i = 8; i != 0; --i, ++active, ++age) {
			if (*active) {
				++*age;
			}
		}
	}
	{
		int16_t* active = g_briefingMapLabels.active;
		int16_t* age = g_briefingMapLabels.age;

		for (i = 8; i != 0; --i, ++active, ++age) {
			if (*active) {
				++*age;
			}
		}
	}
}

// FUNCTION: XWA 0x5665D0
void BriefingMap_UpdatePlaybackAnimation(void) {
	if (g_briefingPlaybackActive) {
		if (!g_briefingMapRegionTransitionState) {
			BriefingMap_AnimateViewState();
			BriefingScript_AdvanceOrResetAtEnd();
		}
	}
}

// FUNCTION: XWA 0x569660
void BriefingMap_DrawGrid(FrontendRect* viewportRect) {
	FrontendRect dst;
	int centerTileY;
	int centerTileX;
	int16_t startY;
	int16_t startX;
	int16_t y;
	int16_t x;
	int xCounter;
	int yCounter;
	int16_t drawMinorQuarterLines;

	FrontendDraw_RectCopy(&dst, viewportRect);

	{
		int16_t centerX = g_briefingMapCenter.x;
		int centerXValue = centerX;

		centerTileX = centerXValue / 256;
		if (centerX > 0 && (centerX & 0xff) != 0) {
			++centerTileX;
		}
		{
			int16_t scaleX = g_briefingMapScale.x;

			startX = ((dst.right - dst.left) >> 1) + dst.left + ((scaleX * ((uint8_t)(-centerXValue))) >> 8);
			while ((int16_t)startX > dst.left) {
				startX = (int16_t)(startX - scaleX);
				--centerTileX;
			}
		}
	}

	{
		int16_t centerY = g_briefingMapCenter.y;
		int centerYValue = centerY;

		centerTileY = centerYValue / 256;
		if (centerY > 0 && (centerY & 0xff) != 0) {
			++centerTileY;
		}
		{
			int16_t scaleY = g_briefingMapScale.y;

			startY = ((dst.bottom - dst.top) >> 1) + dst.top + ((scaleY * ((uint8_t)(-centerYValue))) >> 8);
			while ((int16_t)startY > dst.top) {
				startY = (int16_t)(startY - scaleY);
				--centerTileY;
			}
		}
	}

	x = startX;
	xCounter = centerTileX;
	y = startY;
	yCounter = centerTileY;
	if (g_briefingMapScale.x >= 16) {
		drawMinorQuarterLines = g_briefingMapScale.x >= 32;
		while ((int16_t)x < dst.right) {
			if ((int16_t)(xCounter & 3) != 0 && ((int16_t)(xCounter & 3) == 2 || drawMinorQuarterLines)) {
				FrontendDraw_VerticalLineClipped(dst.top, dst.bottom, (int16_t)x,
												 g_briefingMapMinorGridColor);
			}
			x = (int16_t)(x + g_briefingMapScale.x);
			++xCounter;
		}
		while ((int16_t)y < dst.bottom) {
			if ((int16_t)(yCounter & 3) != 0 && ((int16_t)(yCounter & 3) == 2 || drawMinorQuarterLines)) {
				FrontendDraw_HorizontalLineClipped(dst.left, dst.right, (int16_t)y,
												   g_briefingMapMinorGridColor);
			}
			y = (int16_t)(y + g_briefingMapScale.y);
			++yCounter;
		}

		x = startX;
		xCounter = centerTileX;
		y = startY;
		yCounter = centerTileY;
	}

	while ((int16_t)x < dst.right) {
		if ((xCounter & 3) == 0) {
			FrontendDraw_VerticalLineClipped(dst.top, dst.bottom, (int16_t)x, g_briefingMapMajorGridColor);
		}
		x = (int16_t)(x + g_briefingMapScale.x);
		++xCounter;
	}
	while ((int16_t)y < dst.bottom) {
		if ((yCounter & 3) == 0) {
			FrontendDraw_HorizontalLineClipped(dst.left, dst.right, (int16_t)y, g_briefingMapMajorGridColor);
		}
		y = (int16_t)(y + g_briefingMapScale.y);
		++yCounter;
	}
}

// FUNCTION: XWA 0x569F00
void BriefingMap_DrawRevealedLabel(const char* text, int colorRampGroup, int16_t x, int16_t y,
								   int16_t revealCount, int shadeGroup) {
	char str[64];

	(void)colorRampGroup;

	if (revealCount < 0) {
		return;
	}

	{
		int16_t textLen;

		textLen = (int16_t)strlen(text);
		strcpy(str, text);
		shadeGroup *= 8;

		if (revealCount < textLen + 2) {
			FrontendRect rect;
			int revealChars;
			int16_t visibleChars;
			int16_t textWidth;
			uint16_t shadeGroupBase;
			uint16_t shadeIndex;
			int shadeEnd;

			revealChars = (uint16_t)revealCount;
			if ((int16_t)revealChars <= textLen) {
				str[revealCount] = '\0';
				visibleChars = revealChars;
			} else {
				visibleChars = textLen;
			}
			if ((uint16_t)revealChars > 3u) {
				revealChars = 3;
			}

			textWidth = FrontendText_MeasureWidth(str, 10);
			shadeIndex = (uint16_t)(shadeGroup + 2 * (3 - revealChars));
			shadeGroupBase = (uint16_t)shadeGroup;
			shadeEnd = shadeGroupBase + 6;
			while ((int16_t)shadeIndex <= shadeEnd) {
				if ((int16_t)visibleChars <= 0) {
					break;
				}
				str[(int16_t)visibleChars] = '\0';
				FrontendText_Draw(10, str, x, y, g_textShadeRamps[0][(int16_t)shadeIndex]);
				++shadeIndex;
				--visibleChars;
			}

			FrontendDraw_RectAssign(&rect, textWidth + x + 2, y, textWidth + x + 8, y + 6);
			if ((int16_t)revealCount < textLen) {
				FrontendDraw_Rect(&rect, 0, 0, g_textShadeRamps[0][shadeGroupBase + 7], 1);
			}
		} else {
			unsigned short shadeIndex;

			strcpy(str, text);
			if (revealCount < textLen + 4) {
				shadeIndex = (unsigned short)(textLen - (uint16_t)revealCount + shadeGroup + 9);
			} else {
				shadeIndex = (unsigned short)(shadeGroup + 6);
			}
			FrontendText_Draw(10, str, x, y, g_textShadeRamps[0][shadeIndex]);
		}
	}
}

// FUNCTION: XWA 0x569ED0
void BriefingMap_DrawRevealedLabelIfActive(const char* text, int colorRampGroup, int16_t x, int16_t y,
										   int revealCount, int shadeGroup) {
	if ((int16_t)revealCount >= 0) {
		BriefingMap_DrawRevealedLabel(text, colorRampGroup, x, y, 2 * revealCount, shadeGroup);
	}
}

// FUNCTION: XWA 0x56A0D0
void BriefingMap_DrawCraftIconHighlight(FrontendRect* viewportRect, int unused, int16_t briefingIconIndex,
										int highlightPhase) {
	int iconIndex;
	int16_t objectType;
	int colorBase;
	int16_t screenX;
	int16_t screenY;
	int16_t mapX;
	int16_t mapY;
	int shipListIndex;
	int iconWidth;
	int iconHeight;
	FrontendRect rect;

	(void)unused;

	iconIndex = (int16_t)briefingIconIndex;
	mapX = g_briefingMapCurrentRegionIcons[iconIndex].mapX;
	mapY = g_briefingMapCurrentRegionIcons[iconIndex].mapY;
	objectType = (uint8_t)g_briefingMapCurrentRegionIcons[iconIndex].objectType;
	switch (g_briefingMapCurrentRegionIcons[iconIndex].iconClass) {
		case 0:
			colorBase = 0;
			break;
		case 1:
		case 4:
			colorBase = 8;
			break;
		case 2:
			colorBase = 24;
			break;
		case 3:
			colorBase = 16;
			break;
		case 5:
			colorBase = 32;
			break;
		default:
			colorBase = 0;
			break;
	}

	if (objectType < 0) {
		return;
	}

	BriefingMap_ProjectPointToViewport(viewportRect, mapX, mapY, &screenX, &screenY);
	shipListIndex = g_shipTypeToShipListIndex[objectType];
	iconWidth = g_shipList[shipListIndex].iconRect.right - g_shipList[shipListIndex].iconRect.left + 1;
	iconHeight = g_shipList[shipListIndex].iconRect.bottom - g_shipList[shipListIndex].iconRect.top + 1;
	if ((g_briefingMapCurrentRegionIcons[iconIndex].drawFlags & 1) == 0) {
		screenX = (int16_t)(screenX - (iconWidth >> 1));
		screenY = (int16_t)(screenY - (iconHeight >> 1));
		FrontendDraw_RectAssign(&rect, screenX, screenY, iconWidth + screenX - 1, iconHeight + screenY - 1);
	} else {
		screenX = (int16_t)(screenX - (iconHeight >> 1));
		screenY = (int16_t)(screenY - (iconWidth >> 1));
		FrontendDraw_RectAssign(&rect, screenX, screenY, iconHeight + screenX - 1, iconWidth + screenY - 1);
	}

	if ((int16_t)highlightPhase < 12) {
		int count;
		int shadeIndex;
		int offset;

		if ((int16_t)highlightPhase < 4) {
			shadeIndex = colorBase - 2 * highlightPhase + 7;
			offset = 16;
			count = highlightPhase + 1;
		} else if ((int16_t)highlightPhase < 8) {
			shadeIndex = colorBase + 1;
			offset = 2 * (11 - highlightPhase);
			count = 4;
		} else {
			shadeIndex = colorBase + 1;
			offset = 2 * (11 - highlightPhase);
			count = 12 - highlightPhase;
		}

		if ((int16_t)highlightPhase >= 8) {
			int inset;

			inset = 11 - (int16_t)highlightPhase;
			FrontendDraw_RectInsetXY(&rect, inset, inset);
			FrontendDraw_Rect(&rect, 0, 0, g_textShadeRamps[0][(uint16_t)colorBase + 2], 1);
			FrontendDraw_RectOutline(&rect, 0, 0,
									 g_textShadeRamps[0][(uint16_t)colorBase + (int16_t)highlightPhase - 6]);
		}

		if ((int16_t)count > 0) {
			int repeatCount;

			repeatCount = (int16_t)count;
			do {
				int* tintColor;

				tintColor = &g_textShadeRamps[0][(uint16_t)shadeIndex];
				FrontImage_DrawSpriteRectTintedBlendMode(
					"lgreyicon", &g_shipList[shipListIndex].iconRect, screenX - (int16_t)offset,
					screenY - (int16_t)offset, *tintColor,
					g_briefingMapCurrentRegionIcons[iconIndex].drawFlags);
				FrontImage_DrawSpriteRectTintedBlendMode(
					"lgreyicon", &g_shipList[shipListIndex].iconRect, screenX + (int16_t)offset,
					screenY - (int16_t)offset, *tintColor,
					g_briefingMapCurrentRegionIcons[iconIndex].drawFlags);
				FrontImage_DrawSpriteRectTintedBlendMode(
					"lgreyicon", &g_shipList[shipListIndex].iconRect, screenX - (int16_t)offset,
					screenY + (int16_t)offset, *tintColor,
					g_briefingMapCurrentRegionIcons[iconIndex].drawFlags);
				FrontImage_DrawSpriteRectTintedBlendMode(
					"lgreyicon", &g_shipList[shipListIndex].iconRect, screenX + (int16_t)offset,
					screenY + (int16_t)offset, *tintColor,
					g_briefingMapCurrentRegionIcons[iconIndex].drawFlags);
				shadeIndex += 2;
				offset -= 2;
			} while (--repeatCount != 0);
		}
	} else {
		FrontendDraw_RectInsetXY(&rect, -2, -2);
		FrontendDraw_Rect(&rect, 0, 0, g_textShadeRamps[0][(uint16_t)colorBase + 2], 1);
		FrontendDraw_RectOutline(&rect, 0, 0, g_textShadeRamps[0][(uint16_t)colorBase + 6]);
	}
}

// FUNCTION: XWA 0x569900
void BriefingMap_DrawOverlays(FrontendRect* viewportRect, int unused) {
	FrontendRect dst;
	int i;
	int16_t outX;
	int16_t outY;
	char text[40];
	int mapIconResourceIndex;
	int remaining;

	FrontendDraw_RectCopy(&dst, viewportRect);
	if (g_briefingMapPrimaryHighlightActive) {
		BriefingMap_DrawCraftIconHighlight(viewportRect, unused,
										   (uint16_t)g_briefingMapPrimaryHighlightIconIndex,
										   (uint16_t)g_briefingMapPrimaryHighlightPhase);
	}

	{
		i = 0;
		remaining = 8;
		do {
			if (g_briefingMapFgMarkers.active[i]) {
				int16_t iconIndex;

				iconIndex = g_briefingMapFgMarkers.iconIdx[i];
				BriefingMap_ProjectPointToViewport(&dst, g_briefingMapCurrentRegionIcons[iconIndex].mapX,
												   g_briefingMapCurrentRegionIcons[iconIndex].mapY, &outX,
												   &outY);
				BriefingMap_DrawCraftIconHighlight(viewportRect, unused, iconIndex,
												   (uint16_t)g_briefingMapFgMarkers.age[i]);
			}
			++i;
		} while (--remaining);
	}

#ifdef XWA_MODERN
	mapIconResourceIndex = 0;
#endif

	{
		int remaining;

		i = 0;
		remaining = 8;
		do {
			if (g_briefingMapLabels.active[i]) {
				char* cursor;
				int charIndex;
				int16_t textIndex;

				textIndex = g_briefingMapLabels.textIdx[i];
				BriefingMap_ProjectPointToViewport(&dst, g_briefingMapLabels.x[i], g_briefingMapLabels.y[i],
												   &outX, &outY);
				strcpy(text, g_frontendBriefingContent.mapLabelTexts[textIndex]);
				charIndex = 0;
				if (text[0]) {
					cursor = text;
					do {
						if (*cursor == '[') {
							*cursor = 2;
						}
						if (*cursor == ']') {
							*cursor = 1;
						}
						++charIndex;
						cursor = &text[(int16_t)charIndex];
					} while (*cursor);
				}
				BriefingMap_DrawRevealedLabelIfActive(text, 1, outX, outY,
													  (uint16_t)g_briefingMapLabels.age[i],
													  (uint16_t)g_briefingMapLabels.style[i]);
			}
			++i;
		} while (--remaining);
	}

	{
		int remaining;

		i = 0;
		remaining = FRONTEND_BRIEFING_MAP_ICON_COUNT;
		do {
			int iconIndex;
			int16_t objectType;

			iconIndex = i;
			objectType = (uint8_t)g_briefingMapCurrentRegionIcons[iconIndex].objectType;
			if (objectType) {
				int shipListIndex;
				int iconWidth;
				int iconHeight;
				int16_t mapX;
				int16_t mapY;

				mapX = g_briefingMapCurrentRegionIcons[iconIndex].mapX;
				mapY = g_briefingMapCurrentRegionIcons[iconIndex].mapY;

				switch (g_briefingMapCurrentRegionIcons[iconIndex].iconClass) {
					case 0:
						mapIconResourceIndex = 0;
						break;
					case 2:
						mapIconResourceIndex = 2;
						break;
					case 3:
						mapIconResourceIndex = 3;
						break;
					case 1:
					case 4:
						mapIconResourceIndex = 1;
						break;
					case 5:
						mapIconResourceIndex = 4;
						break;
					default:
						break;
				}

				shipListIndex = g_shipTypeToShipListIndex[objectType];
				iconWidth =
					g_shipList[shipListIndex].iconRect.right - g_shipList[shipListIndex].iconRect.left + 1;
				iconHeight =
					g_shipList[shipListIndex].iconRect.bottom - g_shipList[shipListIndex].iconRect.top + 1;
				BriefingMap_ProjectPointToViewport(&dst, mapX, mapY, &outX, &outY);
				if ((g_briefingMapCurrentRegionIcons[iconIndex].drawFlags & 1) == 0) {
					outX = (int16_t)(outX - (iconWidth >> 1));
					outY = (int16_t)(outY - (iconHeight >> 1));
				} else {
					outX = (int16_t)(outX - (iconHeight >> 1));
					outY = (int16_t)(outY - (iconWidth >> 1));
				}
				sprintf(g_frontendScratchBuffer, "lmapicon%d", (int16_t)mapIconResourceIndex);
				FrontImage_DrawSpriteRectBlendMode(g_frontendScratchBuffer,
												   &g_shipList[shipListIndex].iconRect, outX, outY,
												   g_briefingMapCurrentRegionIcons[iconIndex].drawFlags);
			}
			++i;
		} while (--remaining);
	}
}

// FUNCTION: XWA 0x56BD20
int MissionBriefing_PlayNarrationSequenceStart(void) {
	int missionPrefix;
	int battleNumber;
	int missionNumber;
	char separator;
	char waveId[12];

	sscanf(g_missionList[g_selectedMissionListIndex].fileName, "%d%c%d%c%d", &missionPrefix, &separator,
		   &battleNumber, &separator, &missionNumber);
	if ((unsigned int)battleNumber > 99u || (unsigned int)missionNumber > 99u) {
		return 0;
	}
	sprintf(waveId, "%2d%2d%2d", battleNumber, missionNumber, 1);
	if (waveId[0] == ' ') {
		waveId[0] = '0';
	}
	if (waveId[2] == ' ') {
		waveId[2] = '0';
	}
	if (waveId[4] == ' ') {
		waveId[4] = '0';
	}
	sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\S%s.wav", battleNumber, missionNumber, waveId);
	strcpy(g_pendingVoiceWav, g_frontendScratchBuffer);
	return FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
}

// FUNCTION: XWA 0x56BE20
int MissionBriefing_PlayTextBlockVoice(int textBlockIndex) {
	int missionPrefix;
	int battleNumber;
	int missionNumber;
	char separator;
	char waveId[12];

	sscanf(g_missionList[g_selectedMissionListIndex].fileName, "%d%c%d%c%d", &missionPrefix, &separator,
		   &battleNumber, &separator, &missionNumber);
	sprintf(waveId, "%2d%2d%2d", battleNumber, missionNumber, textBlockIndex + 1);
	if (waveId[0] == ' ') {
		waveId[0] = '0';
	}
	if (waveId[2] == ' ') {
		waveId[2] = '0';
	}
	if (waveId[4] == ' ') {
		waveId[4] = '0';
	}
	sprintf(g_frontendScratchBuffer, "wave\\frontend\\B%dM%d\\B%s.wav", battleNumber, missionNumber, waveId);
	return FrontendWaveStream_PlayWaveFile(g_frontendScratchBuffer, 0, 0);
}

// FUNCTION: XWA 0x56AEC0
int MissionBriefing_DrawTextPage(void) {
	int lineCount;
	FrontendRect rect;

	FrontendDraw_RectAssign(&rect, 65, 90, 575, 106);
	if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
		FrontendText_DrawCenteredReveal(12, FrontendString_Get(STR_FAMILY_MISSION_OVERVIEW), &rect,
										g_colorLightBlue, clipBottomAdjust);
	} else {
		FrontendText_DrawCenteredReveal(12, FrontendString_Get(STR_FAMILY_COMMANDER_BRIEFING), &rect,
										g_colorLightBlue, clipBottomAdjust);
	}

	FrontendDraw_RectAssign(&rect, 70, 111, 570, 383);
	lineCount = FrontendText_DrawWrappedClipped(12, g_briefingText, &rect, 0xffff, 4, 4096) + 1;
	if (lineCount > 17) {
		FrontendDraw_RectAssign(&rect, 70, 111, 550, 383);
		lineCount = FrontendText_DrawWrappedClipped(12, g_briefingText, &rect, 0xffff, 4, 4096) + 1;
		FrontendDraw_RectAssign(&rect, 551, 111, 570, 383);
		g_frontendFirstVisibleLine =
			FrontendScrollbar_Draw(&rect, g_frontendFirstVisibleLine, lineCount, 0, 5, g_colorNavy, 9);
		FrontendDraw_RectAssign(&rect, 70, 111, 550, 383);
	} else {
		FrontendDraw_RectAssign(&rect, 70, 111, 570, 383);
	}
	FrontendText_DrawWrappedClippedEx(12, g_briefingText, &rect, g_colorLightBlue, 4,
									  g_frontendFirstVisibleLine, clipBottomAdjust);
	++clipBottomAdjust;
	return 1;
}

// FUNCTION: XWA 0x56B060
int BriefingMap_DrawCraftCharacteristicsPanel(FrontendRect* rect, int revealStep) {
	FrontendRect recta;
	FrontendRect dst;
	int specIndex;
	unsigned short color;
	int lineCount;
	double displayedSizeMeters;
	const char* unitText;
	const char* crewText;

	FrontendDraw_RectCopy(&recta, rect);
	specIndex = stats.craftType - 1;
	if (specIndex < 0) {
		specIndex = 0;
	}
	if (revealStep < 3) {
		color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
	} else {
		color = (unsigned short)g_textShadeRamps[0][6];
	}
	sprintf(g_frontendScratchBuffer, "%s", g_shipList[g_shipTypeToShipListIndex[stats.craftType]].name);
	FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
	FrontendDraw_RectOffsetXY(&recta, 0, 24);

	if (stats.craftType != 227) {
		if (g_briefingCraftStatsRevealEnabled > 0) {
			revealStep -= 3;
			if (revealStep < 0) {
				revealStep = 0;
			}
			if (revealStep < 3) {
				color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
			} else {
				color = (unsigned short)g_colorYellow;
			}
			FrontendText_DrawAlignedInRect(10, FrontendString_Get(STR_SPECIAL_CHARACTERISTICS), &recta, 0, 1,
										   color);
			FrontendDraw_RectOffsetXY(&recta, 0, 12);
			revealStep -= 3;
			if (revealStep < 0) {
				revealStep = 0;
			}
			if (revealStep < 3) {
				color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
			} else {
				color = (unsigned short)g_textShadeRamps[0][6];
			}
			FrontendDraw_RectCopy(&dst, &recta);
			dst.right = dst.left + 200;
			dst.left += 20;
			dst.bottom = dst.top + 120;
			lineCount = FrontendText_DrawWrapped(
				10, g_frontendBriefingContent.mapLabelTexts[g_briefingCraftStatsRevealEnabled + 127], &dst,
				color, 3, 0, 0, -1);
			FrontendDraw_RectOffsetXY(&recta, 0, 12 * (lineCount + 2));
		}

		if (!stats.genusId) {
			revealStep -= 3;
			if (revealStep < 0) {
				revealStep = 0;
			}
			if (revealStep < 3) {
				color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
			} else {
				color = (unsigned short)g_textShadeRamps[0][6];
			}
			if (revealStep < 3) {
				sprintf(g_frontendScratchBuffer, "%s %d %s", FrontendString_Get(STR_SPEED), stats.speedRating,
						FrontendString_Get(STR_MGLT));
			} else {
				sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_SPEED), 1,
						stats.speedRating, FrontendString_Get(STR_MGLT));
			}
			FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
			FrontendDraw_RectOffsetXY(&recta, 0, 12);

			revealStep -= 3;
			if (revealStep < 0) {
				revealStep = 0;
			}
			if (revealStep < 3) {
				color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
			} else {
				color = (unsigned short)g_textShadeRamps[0][6];
			}
			if (revealStep < 3) {
				sprintf(g_frontendScratchBuffer, "%s %d %s", FrontendString_Get(STR_ACCELERATION),
						stats.accelerationRating, FrontendString_Get(STR_MGLT_PER_SECOND));
			} else {
				sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_ACCELERATION), 1,
						stats.accelerationRating, FrontendString_Get(STR_MGLT_PER_SECOND));
			}
			FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
			FrontendDraw_RectOffsetXY(&recta, 0, 12);

			revealStep -= 3;
			if (revealStep < 0) {
				revealStep = 0;
			}
			if (revealStep < 3) {
				color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
			} else {
				color = (unsigned short)g_textShadeRamps[0][6];
			}
			if (revealStep < 3) {
				sprintf(g_frontendScratchBuffer, "%s %d %s", FrontendString_Get(STR_MANUEVERABILITY_RATING),
						stats.maneuverRating, FrontendString_Get(STR_DPF));
			} else {
				sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4,
						FrontendString_Get(STR_MANUEVERABILITY_RATING), 1, stats.maneuverRating,
						FrontendString_Get(STR_DPF));
			}
			FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
			FrontendDraw_RectOffsetXY(&recta, 0, 12);

			revealStep -= 3;
			if (revealStep < 0) {
				revealStep = 0;
			}
			if (revealStep < 3) {
				color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
			} else {
				color = (unsigned short)g_textShadeRamps[0][6];
			}
			if (revealStep < 3) {
				sprintf(g_frontendScratchBuffer, "%s ", FrontendString_Get(STR_LASERS));
			} else {
				sprintf(g_frontendScratchBuffer, "%c%s %c", 4, FrontendString_Get(STR_LASERS), 1);
			}
			if (stats.laserCount) {
				strcat(g_frontendScratchBuffer,
					   FrontendString_Get((UIString)(stats.laserCount + STR_ION_CANNONS)));
				strcat(g_frontendScratchBuffer, " ");
				strcat(g_frontendScratchBuffer, FrontendString_Get(STR_TURBOLASERS));
				if (stats.ionCount) {
					strcat(g_frontendScratchBuffer, " ");
					strcat(g_frontendScratchBuffer, FrontendString_Get(STR_AND));
					strcat(g_frontendScratchBuffer, " ");
				}
			}
			if (stats.ionCount) {
				strcat(g_frontendScratchBuffer,
					   FrontendString_Get((UIString)(stats.ionCount + STR_ION_CANNONS)));
				strcat(g_frontendScratchBuffer, " ");
				strcat(g_frontendScratchBuffer, FrontendString_Get(STR_ION_CANNONS));
			}
			FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
			FrontendDraw_RectOffsetXY(&recta, 0, 12);

			revealStep -= 3;
			if (revealStep < 0) {
				revealStep = 0;
			}
			if (revealStep < 3) {
				color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
			} else {
				color = (unsigned short)g_textShadeRamps[0][6];
			}
			if (revealStep < 3) {
				sprintf(g_frontendScratchBuffer, "%s %d", FrontendString_Get(STR_WARHEAD_CAPACITY_RATING),
						stats.warheadRating);
			} else {
				sprintf(g_frontendScratchBuffer, "%c%s %c%d", 4,
						FrontendString_Get(STR_WARHEAD_CAPACITY_RATING), 1, stats.warheadRating);
			}
			FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
			FrontendDraw_RectOffsetXY(&recta, 0, 12);
		} else {
			if (stats.genusId != 8 && stats.genusId != 9) {
				revealStep -= 3;
				if (revealStep < 0) {
					revealStep = 0;
				}
				if (revealStep < 3) {
					color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
				} else {
					color = (unsigned short)g_textShadeRamps[0][6];
				}
				displayedSizeMeters = (double)ModelPreview_GetDisplayedSizeMeters();
				if (revealStep < 3) {
					if (displayedSizeMeters >= g_briefingSizeMetersThreshold) {
						unitText = FrontendString_Get(STR_KM);
						sprintf(g_frontendScratchBuffer, "%s %.1f %s", FrontendString_Get(STR_SIZE),
								displayedSizeMeters * g_briefingKilometersScale, unitText);
					} else {
						unitText = FrontendString_Get(STR_METERS);
						sprintf(g_frontendScratchBuffer, "%s %.1f %s", FrontendString_Get(STR_SIZE),
								displayedSizeMeters, unitText);
					}
				} else {
					if (displayedSizeMeters >= g_briefingSizeMetersThreshold) {
						unitText = FrontendString_Get(STR_KM);
						sprintf(g_frontendScratchBuffer, "%c%s %c%.1f %s", 4, FrontendString_Get(STR_SIZE), 1,
								displayedSizeMeters * g_briefingKilometersScale, unitText);
					} else {
						unitText = FrontendString_Get(STR_METERS);
						sprintf(g_frontendScratchBuffer, "%c%s %c%.1f %s", 4, FrontendString_Get(STR_SIZE), 1,
								displayedSizeMeters, unitText);
					}
				}
				FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
				FrontendDraw_RectOffsetXY(&recta, 0, 12);

				revealStep -= 3;
				if (revealStep < 0) {
					revealStep = 0;
				}
				if (revealStep < 3) {
					color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
				} else {
					color = (unsigned short)g_textShadeRamps[0][6];
				}
				if (revealStep < 3) {
					crewText = g_techLibrarySpecTextTable[specIndex].crew;
					sprintf(g_frontendScratchBuffer, "%s %s", FrontendString_Get(STR_CREW), crewText);
				} else {
					crewText = g_techLibrarySpecTextTable[specIndex].crew;
					sprintf(g_frontendScratchBuffer, "%c%s %c%s", 4, FrontendString_Get(STR_CREW), 1,
							crewText);
				}
				FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
				FrontendDraw_RectOffsetXY(&recta, 0, 12);
			}
		}

		revealStep -= 3;
		if (revealStep < 0) {
			revealStep = 0;
		}
		if (revealStep < 3) {
			color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
		} else {
			color = (unsigned short)g_textShadeRamps[0][6];
		}
		if (revealStep < 3) {
			sprintf(g_frontendScratchBuffer, "%s %d %s", FrontendString_Get(STR_SHIELD_RATING),
					stats.shieldRating, FrontendString_Get(STR_SBD));
		} else {
			sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_SHIELD_RATING), 1,
					stats.shieldRating, FrontendString_Get(STR_SBD));
		}
		FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
		FrontendDraw_RectOffsetXY(&recta, 0, 12);

		revealStep -= 3;
		if (revealStep < 0) {
			revealStep = 0;
		}
		if (revealStep < 3) {
			color = (unsigned short)g_textShadeRamps[0][2 * revealStep];
		} else {
			color = (unsigned short)g_textShadeRamps[0][6];
		}
		if (revealStep < 3) {
			sprintf(g_frontendScratchBuffer, "%s %d %s", FrontendString_Get(STR_HULL_RATING),
					stats.hullRating, FrontendString_Get(STR_RU));
		} else {
			sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_HULL_RATING), 1,
					stats.hullRating, FrontendString_Get(STR_RU));
		}
		FrontendText_DrawAlignedInRect(10, g_frontendScratchBuffer, &recta, 0, 1, color);
		FrontendDraw_RectOffsetXY(&recta, 0, 12);
	}
	return 1;
}

// FUNCTION: XWA 0x567BE0
int16_t BriefingMap_DrawViewportAndSelection(FrontendRect* viewportRect, FrontendRect* clipRect, int unused) {
	FrontendRect rect;
	int16_t outY;
	int16_t outX;
	unsigned char* drawRow;
	FrontendRect viewport;
	int row;
	unsigned char* selectionRow;
	int revealWidth;
	int revealHeight;
	int16_t solidRevealWidth;
	int16_t solidRevealHeight;
	FrontendRect clipBounds;
	FrontendRect textRect;
	FrontendRect titleRect;
	FrontendRect previewClip;
	FrontendRect outRect;
	int transitionStep;
	int count;
	int col;
	unsigned char* previewRow;
	const char* pageLabel;
	char* craftModelName;
	XwaFile* stream;
	unsigned short sourcePixel;

	FrontendDraw_RectCopy(&titleRect, &g_briefingMapSourceRect);
	FrontendDraw_RectOffsetXY(&titleRect, viewportRect->left, viewportRect->top);
	titleRect.bottom = titleRect.top + 12;

	FrontendDraw_RectCopy(&textRect, &g_briefingMapSourceRect);
	FrontendDraw_RectOffsetXY(&textRect, viewportRect->left, viewportRect->top);
	textRect.top = textRect.bottom - 41;
	if (g_briefingTextSlotActive[1]) {
		FrontendText_DrawFormattedWrappedText(
			&textRect,
			(const unsigned char*)g_frontendBriefingContent.textBlocks[g_briefingTextSlotBlockIdx[1]], 0);
		if (g_briefingTextSlotBlockIdx[1] != g_briefingLastNarratedTextBlockIdx) {
			++g_briefingTextPageNumber;
			g_briefingLastNarratedTextBlockIdx = g_briefingTextSlotBlockIdx[1];
			MissionBriefing_PlayTextBlockVoice(g_briefingTextSlotBlockIdx[1]);
		}
	}

	FrontendDraw_RectCopy(&viewport, &g_briefingMapSourceRect);
	FrontendDraw_RectOffsetXY(&viewport, viewportRect->left, viewportRect->top);
	viewport.bottom -= 42;
	FrontendDraw_RectCopy(&clipBounds, clipRect);
	FrontendDisplay_SetScreenClipRect640x480(&viewport);
	FrontendDraw_RectClipToBounds(&clipBounds);
	FrontendDisplay_SetScreenClipRect640x480(&clipBounds);

	if (g_briefingMapRegionTransitionState == 1) {
		srand(1u);
		++g_briefingMapRegionTransitionState;
		g_briefingMapRegionTransitionFrame = 0;
		FrontendSound_PlayUISound("message4", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
	} else if (g_briefingMapRegionTransitionState == 2) {
		FrontendDraw_RectCopy(&rect, &viewport);
		rect.bottom = 480;
		rect.right = 640;
		FrontendDisplay_SetScreenClipRect640x480(&rect);
		FrontendDraw_RectCopy(&rect, &viewport);
		outY = (int16_t)(rect.bottom - 12 * g_briefingMapRegionTransitionFrame);
		if (outY <= rect.top) {
			outY = (int16_t)rect.top;
			rect.bottom = (int16_t)rect.top;
			FrontendDisplay_SetScreenClipRect640x480(&rect);
			++g_briefingMapRegionTransitionState;
			g_briefingMapRegionTransitionFrame = 0;
		} else {
			rect.bottom = outY;
			FrontendDraw_Line(rect.left + 5, outY, rect.right - 5, outY, g_textShadeRamps[3][7]);
			BriefingMap_DrawDatapadBeams(&rect, outY, rand() % 5 + 7);
			if (g_briefingPlaybackActive) {
				++g_briefingMapRegionTransitionFrame;
			}
			FrontendDisplay_SetScreenClipRect640x480(&rect);
		}
	} else if (g_briefingMapRegionTransitionState == 3) {
		FrontendDraw_RectCopy(&rect, &viewport);
		transitionStep = g_briefingMapRegionTransitionFrame / 3;
		if (transitionStep > 15) {
			++g_briefingMapRegionTransitionState;
			g_briefingMapRegionTransitionFrame = 0;
			memcpy(g_briefingMapCurrentRegionIcons,
				   g_briefingMapRegionIconSnapshots[g_briefingMapCurrentRegionIdx],
				   sizeof(g_briefingMapCurrentRegionIcons));
			for (count = 0; count < 8; ++count) {
				if (g_briefingMapLabels.active[count]) {
					g_briefingMapLabels.active[count] = 0;
				}
			}
			g_briefingMapLabelsChanged = 1;
			for (count = 0; count < 8; ++count) {
				if (g_briefingMapFgMarkers.active[count]) {
					g_briefingMapFgMarkers.active[count] = 0;
				}
			}
			g_briefingMapFgMarkersChanged = 1;
			BriefingScript_AdvanceOrResetAtEnd();
			FrontendSound_PlayUISound("message4", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		} else {
			if (transitionStep > 7) {
				FrontendText_DrawCentered(
					15, g_frontendMission->header.regions[g_briefingMapCurrentRegionIdx].name, &rect,
					g_textShadeRamps[3][7 - transitionStep]);
			} else {
				FrontendText_DrawCentered(
					15, g_frontendMission->header.regions[g_briefingMapCurrentRegionIdx].name, &rect,
					g_textShadeRamps[2][transitionStep]);
			}
			if (g_briefingPlaybackActive) {
				++g_briefingMapRegionTransitionFrame;
			}
		}
		FrontendDraw_RectAssign(&rect, 0, 0, 1, 1);
		FrontendDisplay_SetScreenClipRect640x480(&rect);
	} else if (g_briefingMapRegionTransitionState == 4) {
		FrontendDraw_RectCopy(&rect, &viewport);
		rect.bottom = 480;
		rect.right = 640;
		FrontendDisplay_SetScreenClipRect640x480(&rect);
		FrontendDraw_RectCopy(&rect, &viewport);
		outY = (int16_t)(rect.top + 12 * g_briefingMapRegionTransitionFrame);
		if (outY >= rect.bottom) {
			outY = (int16_t)rect.bottom;
			rect.bottom = outY;
			FrontendDisplay_SetScreenClipRect640x480(&rect);
			g_briefingMapRegionTransitionState = 0;
		} else {
			rect.bottom = outY;
			FrontendDraw_Line(rect.left + 5, outY, rect.right - 5, outY, g_textShadeRamps[3][7]);
			BriefingMap_DrawDatapadBeams(&rect, outY, rand() % 5 + 7);
			if (g_briefingPlaybackActive) {
				++g_briefingMapRegionTransitionFrame;
			}
			FrontendDisplay_SetScreenClipRect640x480(&rect);
		}
	}

	if (!g_briefingMapPrimaryHighlightActive || g_briefingMapPrimaryHighlightPhase < 17) {
		BriefingMap_DrawGrid(&viewport);
		BriefingMap_DrawOverlays(&viewport, (int)(intptr_t)&clipBounds);
		if (g_briefingMapPrimaryHighlightPhase < 12) {
			pageLabel = FrontendString_Get(STR_PAGE_NUMBER);
			sprintf(g_frontendScratchBuffer, "%s %d", pageLabel, g_briefingTextPageNumber);
			FrontendText_Draw(10, g_frontendScratchBuffer, viewport.right - 60, 80, g_colorLightBlue);
		}
	}

	if (g_briefingMapPrimaryHighlightPhase > 11) {
		if (g_briefingMapPrimaryHighlightPhase == 17) {
			FrontendDraw_RectCopy(&rect, &viewport);
		} else {
			int16_t leftDistance;
			int16_t rightDistance;
			int16_t topDistance;
			int16_t bottomDistance;
			int remainingPhase;

			FrontendDraw_RectCopy(&rect, &viewport);
			BriefingMap_ProjectPointToViewport(
				&viewport, g_briefingMapCurrentRegionIcons[g_briefingMapPrimaryHighlightIconIndex].mapX,
				g_briefingMapCurrentRegionIcons[g_briefingMapPrimaryHighlightIconIndex].mapY, &outX, &outY);
			if (outX < 0) {
				outX = 0;
			}
			if (outX >= 640) {
				outX = 639;
			}
			if (outY < 0) {
				outY = 0;
			}
			if (outY > 480) {
				outY = 479;
			}
			leftDistance = (int16_t)(outX - rect.left);
			if (leftDistance < 1) {
				leftDistance = 1;
			}
			rightDistance = (int16_t)(rect.right - outX);
			if (rightDistance < 1) {
				rightDistance = 1;
			}
			topDistance = (int16_t)(outY - rect.top);
			if (topDistance < 1) {
				topDistance = 1;
			}
			bottomDistance = (int16_t)(rect.bottom - outY);
			if (bottomDistance < 1) {
				bottomDistance = 1;
			}
			remainingPhase = 17 - g_briefingMapPrimaryHighlightPhase;
			rect.left = remainingPhase * leftDistance / 5 + rect.left;
			rect.right += remainingPhase * rightDistance / -5;
			rect.top = remainingPhase * topDistance / 5 + rect.top;
			rect.bottom += remainingPhase * bottomDistance / -5;
			if (rect.bottom < rect.top) {
				rect.bottom = rect.top + 1;
			}
			if (rect.right < rect.left) {
				rect.right = rect.left + 1;
			}
			FrontendDraw_Rect(&rect, 0, 0, 0, 1);
			FrontendDraw_Rect(&rect, 0, 0, g_briefingMapMajorGridColor, 0);
		}

		if (g_briefingSelectionRevealComplete) {
			if (g_briefingPlaybackActive) {
				if (BriefingMap_GetSelectedObjectType() == BRIEFING_DEATH_STAR_OBJECT_TYPE) {
					if (!(g_briefingDsBriefAnimTick % 2)) {
						FrontImage_AdvanceSpriteFrame("dsbrief", 1);
					}
					++g_briefingDsBriefAnimTick;
				} else {
					yawDeg -= g_briefingYawStep;
					if (yawDeg >= g_briefingFullTurnDegrees) {
						yawDeg = 0.0;
					}
				}
			}
			BriefingMap_SetPreviewOrientation(BriefingMap_GetSelectedObjectType());
			ModelPreview_SetObjectWorldPosition(
				0, g_briefingSelectionPreviewBaseY + 800 * (17 - g_briefingMapPrimaryHighlightPhase), 0);
			if (g_briefingMapPrimaryHighlightPhase == 17) {
				if (BriefingMap_GetSelectedObjectType() == BRIEFING_DEATH_STAR_OBJECT_TYPE) {
					FrontImage_DrawSprite("dsbrief", 188, 88);
				} else {
					ModelPreview_SetObjectWorldPosition(0, g_briefingSelectionPreviewBaseY - 600, 0);
					ModelPreview_SetWhiteDirectionalLight(g_briefingModelLightDx, g_briefingModelLightDy,
														  g_briefingModelLightDz);
					ModelPreview_RenderViewport(rect.left, rect.top, rect.right - rect.left + 1,
												rect.bottom - rect.top + 1, NULL, 0, 0);
				}
			} else if (BriefingMap_GetSelectedObjectType() != BRIEFING_DEATH_STAR_OBJECT_TYPE) {
#ifdef XWA_MODERN
				DebugPrintf(NULL);
#else
				DebugPrintf();
#endif
				rect.left = rect.top + rect.right - rect.bottom - 1;
				ModelPreview_SetWhiteDirectionalLight(g_briefingModelLightDx, g_briefingModelLightDy,
													  g_briefingModelLightDz);
				ModelPreview_RenderViewport(rect.left, rect.top, rect.right - rect.left + 1,
											rect.bottom - rect.top + 1, NULL, 0, 0);
			}
		} else if (g_briefingMapPrimaryHighlightPhase < 17 &&
				   BriefingMap_GetSelectedObjectType() != BRIEFING_DEATH_STAR_OBJECT_TYPE) {
			BriefingMap_SetPreviewOrientation(BriefingMap_GetSelectedObjectType());
			ModelPreview_SetObjectWorldPosition(
				0, g_briefingSelectionPreviewBaseY + 800 * (17 - g_briefingMapPrimaryHighlightPhase), 0);
			if (g_briefingMapPrimaryHighlightPhase == 17) {
				ModelPreview_SetObjectWorldPosition(0, g_briefingSelectionPreviewBaseY - 600, 0);
			}
#ifdef XWA_MODERN
			DebugPrintf(NULL);
#else
			DebugPrintf();
#endif
			ModelPreview_RenderWireframeViewport(rect.left, rect.top, rect.right - rect.left + 1,
												 rect.bottom - rect.top + 1, g_textShadeRamps[3][1], NULL, 0,
												 0);
		}

		if (g_briefingMapPrimaryHighlightPhase == 17) {
			if (!g_briefingSelectionRevealComplete) {
				srand((unsigned int)Seed);
				FrontendDraw_RectCopy(&rect, &viewport);
				outX = (int16_t)(rect.left + 16 * Seed);
				if (outX >= rect.right) {
					outX = (int16_t)rect.right;
					g_briefingSelectionRevealComplete = 1;
				} else {
					FrontendDraw_Line(outX, rect.top + 5, outX, rect.bottom - 5, g_textShadeRamps[3][7]);
				}
				outY = (int16_t)(rect.top + g_briefingSelectionRevealRow + 10 * g_briefingSelectionRevealRow);
				if (outY >= rect.bottom) {
					outY = (int16_t)rect.bottom;
				} else {
					FrontendDraw_Line(rect.left + 5, outY, rect.right - 5, outY, g_textShadeRamps[3][7]);
				}

				drawRow = &g_drawSurfacePtr[rect.top * FrontendDisplay_GetDrawSurfacePitch() + 2 * rect.left];
				previewRow = (unsigned char*)g_briefingPreviewImageBuffer +
							 BRIEFING_PREVIEW_SCRATCH_PITCH * rect.top + 2 * rect.left;
				selectionRow = (unsigned char*)g_briefingSelectionImageBuffer +
							   BRIEFING_PREVIEW_SCRATCH_PITCH * rect.top + 2 * rect.left;
				solidRevealWidth = (int16_t)(outX - rect.left + 1);
				solidRevealHeight = (int16_t)(outY - rect.top + 1);
				revealWidth = outX - rect.left + 1;
				revealHeight = outY - rect.top + 1;

				if (BriefingMap_GetSelectedObjectType() == BRIEFING_DEATH_STAR_OBJECT_TYPE) {
					FrontendDisplay_GetScreenClipRect(&outRect);
					FrontendDraw_RectCopy(&previewClip, viewportRect);
					previewClip.right = outX;
					previewClip.bottom = outY;
					FrontendDisplay_SetScreenClipRect640x480(&previewClip);
					FrontImage_DrawSprite("dsbrief", 188, 88);
					FrontendDisplay_SetScreenClipRect640x480(&outRect);
				} else if (g_gameConfig.use3dHardware[0]) {
					BriefingMap_SetPreviewOrientation(BriefingMap_GetSelectedObjectType());
					ModelPreview_SetObjectWorldPosition(0, g_briefingSelectionPreviewBaseY - 600, 0);
					ModelPreview_SetWhiteDirectionalLight(g_briefingModelLightDx, g_briefingModelLightDy,
														  g_briefingModelLightDz);
					ModelPreview_RenderViewport(
						rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, NULL,
						BRIEFING_PREVIEW_SCRATCH_PITCH, BRIEFING_PREVIEW_SCRATCH_HEIGHT);
				} else {
					for (row = 0; row < solidRevealHeight; ++row) {
						for (col = 0; col < solidRevealWidth; ++col) {
							sourcePixel = *(unsigned short*)&previewRow[2 * col];
							if (sourcePixel != BRIEFING_TRANSPARENT_WORD) {
								*(unsigned short*)&drawRow[2 * col] = sourcePixel;
							}
						}
						drawRow += FrontendDisplay_GetDrawSurfacePitch();
						previewRow += BRIEFING_PREVIEW_SCRATCH_PITCH;
					}
				}
#ifdef XWA_MODERN
				if (BriefingMap_GetSelectedObjectType() != BRIEFING_DEATH_STAR_OBJECT_TYPE) {
					/* Remaster: the loop below composites the SCRATCH
					 * wireframe over the solid model in the L-shaped
					 * unrevealed remainder. */
					XwaSnapshot_EmitSurfaceEventAux(XWA_SURFACE_EVENT_EXTERNAL_COMPOSITE_REVEAL, rect.left,
													rect.top, rect.right, rect.bottom, revealWidth,
													revealHeight);
				}
#endif

				drawRow = &g_drawSurfacePtr[2 * rect.left + rect.top * FrontendDisplay_GetDrawSurfacePitch()];
				selectionRow = (unsigned char*)g_briefingSelectionImageBuffer + 2 * rect.left +
							   BRIEFING_PREVIEW_SCRATCH_PITCH * rect.top;
				for (row = 0; row < rect.bottom - rect.top + 1; ++row) {
					for (col = 0; col < rect.right - rect.left + 1; ++col) {
						if (row < revealHeight && col < revealWidth) {
							switch ((int16_t)revealWidth - col) {
								case 1:
									*(unsigned short*)&drawRow[2 * col] = 0xffff;
									break;
								case 3:
									if (!(rand() % 2)) {
										*(unsigned short*)&drawRow[2 * col] =
											(unsigned short)g_textShadeRamps[3][6];
									}
									break;
								case 7:
									if (!(rand() % 4)) {
										*(unsigned short*)&drawRow[2 * col] =
											(unsigned short)g_textShadeRamps[3][4];
									}
									break;
								case 15:
									if (!(rand() % 8)) {
										*(unsigned short*)&drawRow[2 * col] =
											(unsigned short)g_textShadeRamps[3][2];
									}
									break;
								default:
									break;
							}
							switch ((int16_t)revealHeight - row) {
								case 1:
									*(unsigned short*)&drawRow[2 * col] = 0xffff;
									break;
								case 3:
									if (!(rand() % 2)) {
										*(unsigned short*)&drawRow[2 * col] =
											(unsigned short)g_textShadeRamps[3][6];
									}
									break;
								case 7:
									if (!(rand() % 4)) {
										*(unsigned short*)&drawRow[2 * col] =
											(unsigned short)g_textShadeRamps[3][4];
									}
									break;
								case 15:
									if (!(rand() % 8)) {
										*(unsigned short*)&drawRow[2 * col] =
											(unsigned short)g_textShadeRamps[3][2];
									}
									break;
								default:
									break;
							}
						} else if (BriefingMap_GetSelectedObjectType() != BRIEFING_DEATH_STAR_OBJECT_TYPE) {
							sourcePixel = *(unsigned short*)&selectionRow[2 * col];
							if (sourcePixel != BRIEFING_TRANSPARENT_WORD) {
								*(unsigned short*)&drawRow[2 * col] = sourcePixel;
							}
						}
					}
					drawRow += FrontendDisplay_GetDrawSurfacePitch();
					selectionRow += BRIEFING_PREVIEW_SCRATCH_PITCH;
				}
			}

			FrontendDraw_RectCopy(&rect, &viewport);
			FrontendDisplay_SetScreenClipRect640x480(&rect);
			if (!g_briefingSelectionRevealComplete) {
				rect.right = rect.left + 200;
				rect.bottom = rect.top + 14;
				BriefingMap_DrawCraftCharacteristicsPanel(&rect, 0);
				FrontendDraw_RectCopy(&rect, &viewport);
				outY = (int16_t)(rect.top + g_briefingSelectionRevealRow + 10 * g_briefingSelectionRevealRow);
				if (outY >= rect.bottom) {
					outY = (int16_t)rect.bottom;
					if (g_briefingPlaybackActive) {
						++g_briefingSelectionStatsRevealDone;
					}
				}
				outX = (int16_t)(rect.left + 16 * Seed);
				if (outX >= rect.right) {
					outX = (int16_t)rect.right;
					g_briefingSelectionRevealComplete = 1;
				}
				rect.bottom = outY;
				rect.right = outX;
			}
			FrontendDisplay_SetScreenClipRect640x480(&rect);
			rect.bottom = rect.top + 14;
			BriefingMap_DrawCraftCharacteristicsPanel(&rect, 500);
			if (!g_briefingSelectionRevealComplete) {
				FrontendDraw_RectCopy(&rect, &viewport);
				rect.bottom = 480;
				rect.right = 640;
				FrontendDisplay_SetScreenClipRect640x480(&rect);
				FrontendDraw_RectCopy(&rect, &viewport);
				outY = (int16_t)(rect.top + g_briefingSelectionRevealRow + 10 * g_briefingSelectionRevealRow);
				if (outY < rect.bottom) {
					BriefingMap_DrawDatapadBeams(&rect, outY, rand() % 5 + 7);
				}
				outX = (int16_t)(rect.left + 16 * Seed);
				if (outX < rect.right) {
					BriefingMap_DrawDatapadBeamsFromX(&rect, outX, rand() % 3 + 5);
				}
				if (g_briefingPlaybackActive) {
					++g_briefingSelectionRevealRow;
					++Seed;
				}
			}
		}
	}

	if (g_briefingPlaybackActive) {
		if (g_briefingMapPrimaryHighlightActive) {
			if (g_briefingMapPrimaryHighlightPhase < 17) {
				if (++g_briefingMapPrimaryHighlightPhase == 12) {
					g_briefingSelectionRevealComplete = 0;
					yawDeg = 225.0;
					if (BriefingMap_GetSelectedObjectType() == BRIEFING_DEATH_STAR_OBJECT_TYPE) {
						if (g_pilotData.campaignMode) {
							FrontImage_RegisterResourceDefault("frontres\\dsbrief\\dsbrief.flc", "dsbrief");
						} else {
							FrontImage_RegisterResourceDefault("frontres\\dsbrief\\dsbriefc.flc", "dsbrief");
						}
						stats.craftType = BRIEFING_DEATH_STAR_OBJECT_TYPE;
						g_briefingDsBriefAnimTick = 0;
						FrontImage_SetSpriteFrame("dsbrief", 0);
					} else {
						craftModelName =
							FeDiskIo_GetCraftModelName((unsigned int)BriefingMap_GetSelectedObjectType());
						if (craftModelName) {
							strcpy(g_frontendScratchBuffer, craftModelName);
							g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 4] = '\0';
							strcat(g_frontendScratchBuffer, "Exterior.opt");
							stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
							if (stream == NULL) {
								ModelPreview_LoadModel(craftModelName, BriefingMap_GetSelectedObjectType());
							} else {
								File_Close(stream);
								ModelPreview_LoadModel(g_frontendScratchBuffer,
													   BriefingMap_GetSelectedObjectType());
							}
						}
						memset(&stats, 0, sizeof(stats));
						stats.craftType = BriefingMap_GetSelectedObjectType();
						BuildCraftTechStats(&stats);
						ModelBounds_ComputeMaxMinExtentRatio(0);
						g_briefingSelectionPreviewBaseY = 600;
						ModelPreview_SetWhiteDirectionalLight(g_briefingModelLightDx, g_briefingModelLightDy,
															  g_briefingModelLightDz);
						if (BriefingMap_GetSelectedObjectType() == 127) {
							ModelPreview_SetNodeSwitchIndex(1);
						} else {
							ModelPreview_SetNodeSwitchIndex(0);
						}
					}
					FrontendSound_PlayUISound("message4", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				}

				if (g_briefingMapPrimaryHighlightPhase == 17) {
					g_briefingSelectionRevealRow = 0;
					g_briefingSelectionStatsRevealDone = 0;
					Seed = 0;
					FrontendDraw_RectCopy(&rect, &viewport);
					FrontendDisplay_SetScreenClipRect640x480(&rect);
					if (BriefingMap_GetSelectedObjectType() != BRIEFING_DEATH_STAR_OBJECT_TYPE) {
						ModelPreview_SetWhiteDirectionalLight(g_briefingModelLightDx, g_briefingModelLightDy,
															  g_briefingModelLightDz);
						BriefingMap_SetPreviewOrientation(BriefingMap_GetSelectedObjectType());
						ModelPreview_SetObjectWorldPosition(0, g_briefingSelectionPreviewBaseY - 600, 0);
#ifdef XWA_MODERN
						DebugPrintf(NULL);
#else
						DebugPrintf();
#endif
						if (g_gameConfig.use3dHardware[0]) {
							ModelPreview_SetWhiteDirectionalLight(
								g_briefingModelLightDx, g_briefingModelLightDy, g_briefingModelLightDz);
							ModelPreview_RenderViewport(
								rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1,
								NULL, BRIEFING_PREVIEW_SCRATCH_PITCH, BRIEFING_PREVIEW_SCRATCH_HEIGHT);
						} else {
							ModelPreview_SetWhiteDirectionalLight(
								g_briefingModelLightDx, g_briefingModelLightDy, g_briefingModelLightDz);
							ModelPreview_RenderViewport(
								rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1,
								g_briefingPreviewImageBuffer, BRIEFING_PREVIEW_SCRATCH_PITCH,
								BRIEFING_PREVIEW_SCRATCH_HEIGHT);
						}
					}

					FrontendDraw_RectCopy(&rect, &viewport);
					FrontendDisplay_SetScreenClipRect640x480(&rect);
					if (BriefingMap_GetSelectedObjectType() != BRIEFING_DEATH_STAR_OBJECT_TYPE) {
						ModelPreview_SetWhiteDirectionalLight(g_briefingModelLightDx, g_briefingModelLightDy,
															  g_briefingModelLightDz);
						BriefingMap_SetPreviewOrientation(BriefingMap_GetSelectedObjectType());
						BriefingMap_ClearPreviewScratchRect(g_briefingPreviewImageBuffer, &rect);
						ModelPreview_SetObjectWorldPosition(0, g_briefingSelectionPreviewBaseY - 600, 0);
						if (g_gameConfig.use3dHardware[0]) {
							ModelPreview_SetWhiteDirectionalLight(
								g_briefingModelLightDx, g_briefingModelLightDy, g_briefingModelLightDz);
							ModelPreview_RenderViewport(
								rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1,
								NULL, BRIEFING_PREVIEW_SCRATCH_PITCH, BRIEFING_PREVIEW_SCRATCH_HEIGHT);
						} else {
							ModelPreview_SetWhiteDirectionalLight(
								g_briefingModelLightDx, g_briefingModelLightDy, g_briefingModelLightDz);
							ModelPreview_RenderViewport(
								rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1,
								g_briefingPreviewImageBuffer, BRIEFING_PREVIEW_SCRATCH_PITCH,
								BRIEFING_PREVIEW_SCRATCH_HEIGHT);
						}
					}

					FrontendDraw_RectCopy(&rect, &viewport);
					BriefingMap_ClearPreviewScratchRect(g_briefingSelectionImageBuffer, &rect);
#ifdef XWA_MODERN
					/* Remaster: reset the external RT before the wireframe
					 * renders into it (classic scratch wipe above). */
					XwaSnapshot_EmitSurfaceEvent(XWA_SURFACE_EVENT_EXTERNAL_CLEAR, rect.left, rect.top,
												 rect.right, rect.bottom);
#endif
					BriefingMap_SetPreviewOrientation(BriefingMap_GetSelectedObjectType());
					ModelPreview_SetObjectWorldPosition(0, g_briefingSelectionPreviewBaseY - 600, 0);
#ifdef XWA_MODERN
					DebugPrintf(NULL);
#else
					DebugPrintf();
#endif
					ModelPreview_RenderWireframeViewport(
						rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1,
						g_textShadeRamps[3][1], g_briefingSelectionImageBuffer,
						BRIEFING_PREVIEW_SCRATCH_PITCH, BRIEFING_PREVIEW_SCRATCH_HEIGHT);
				}
			}
		} else if (g_briefingMapPrimaryHighlightPhase > 0 && --g_briefingMapPrimaryHighlightPhase == 11) {
			g_briefingMapPrimaryHighlightPhase = 0;
		}
	}

	(void)unused;
	return 1;
}

// FUNCTION: XWA 0x5665F0
int MissionBriefing_DrawMapViewport(FrontendRect* viewportRect, FrontendRect* clipRect, int highlightPhase) {
	FrontendRect viewport;
	FrontendRect clip;

	FrontendDraw_RectCopy(&viewport, viewportRect);
	FrontendDraw_RectCopy(&clip, clipRect);
	return BriefingMap_DrawViewportAndSelection(&viewport, &clip, highlightPhase);
}

// FUNCTION: XWA 0x56A490
int MissionBriefing_DrawPlaybackControls(void) {
	FrontendRect out;
	FrontendRect dst;
	FrontendRect leftBarRect;
	char rightBarName[32] = "rightbar3";
	char leftBarName[32];
	int outY;
	int outX;
	int localTeamOrSinglePlayer;
	int buttonStates[8];
	int result;

	rightBarName[8] = (char)(g_frontendRightBarPanelIndex + '0');
	localTeamOrSinglePlayer = FrontendNet_IsTeamLocalPlayer(g_pilotData.team) ||
							  g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER;
	FrontImage_GetResourceRect(rightBarName, &out);

	if (!g_frontendRightBarAnimState) {
		FrontImage_DrawSprite(rightBarName, out.left - out.right + 639, out.top - out.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (FrontImage_GetSpriteFrame(rightBarName) == 9) {
			g_frontendRightBarAnimState = 1;
		}
	} else if (g_frontendRightBarAnimState == 1) {
		FrontImage_DrawSprite(rightBarName, out.left - out.right + 639, out.top - out.bottom + 479);
	} else if (g_frontendRightBarAnimState == 2) {
		FrontImage_DrawSprite(rightBarName, out.left - out.right + 639, out.top - out.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			g_frontendRightBarAnimState = 3;
		}
	} else if (g_frontendRightBarAnimState == 4) {
		FrontImage_DrawSprite(rightBarName, out.left - out.right + 639, out.top - out.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 2;
		}
	} else if (g_frontendRightBarAnimState == 5) {
		FrontImage_DrawSprite(rightBarName, out.left - out.right + 639, out.top - out.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (!FrontImage_GetSpriteFrame(rightBarName)) {
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			g_frontendRightBarAnimState = 0;
			g_frontendRightBarPanelIndex = 1;
		}
	}

	if (!g_missionBriefingTextViewActive) {
		FrontImage_GetResourceRect("leftbar4", &leftBarRect);
		if (!g_frontendLeftBarAnimState) {
			FrontImage_DrawSprite("leftbar4", 0, leftBarRect.top - leftBarRect.bottom + 479);
			FrontImage_AdvanceSpriteFrame("leftbar4", 1);
			if (FrontImage_GetSpriteFrame("leftbar4") == 9) {
				g_frontendLeftBarAnimState = 1;
			}
			return 0;
		}

		if (g_frontendLeftBarAnimState == 1) {
			FrontImage_DrawSprite("leftbar4", 0, leftBarRect.top - leftBarRect.bottom + 479);
			FrontendDraw_RectCopy(&dst, &g_frontendSidebarButtonRects[3]);
		} else if (g_frontendLeftBarAnimState == 2) {
			FrontImage_DrawSprite("leftbar4", 0, leftBarRect.top - leftBarRect.bottom + 479);
			FrontImage_AdvanceSpriteFrame("leftbar4", 1);
			if (!FrontImage_GetSpriteFrame("leftbar4")) {
				g_frontendLeftBarAnimState = 3;
				return 0;
			}
			return 0;
		} else if (g_frontendLeftBarAnimState == 4) {
			FrontImage_DrawSprite("leftbar4", 0, leftBarRect.top - leftBarRect.bottom + 479);
			FrontImage_AdvanceSpriteFrame("leftbar4", 1);
			if (!FrontImage_GetSpriteFrame("leftbar4")) {
				g_missionBriefingTextViewActive = 1;
				clipBottomAdjust = 0;
				g_frontendFirstVisibleLine = 0;
				FrontendDisplay_LockOffscreenSurface();
				FrontImage_DrawSpriteOpaque("background", 0, 0);
				outX = 264;
				outY = 269;
				FrontendDisplay_UnlockOffscreenSurface(1);
				g_frontendLeftBarAnimState = 0;
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				MissionBriefing_PlayNarrationSequenceStart();
				return 0;
			}
			return 0;
		} else {
			return 0;
		}
	} else {
		if (localTeamOrSinglePlayer) {
			strcpy(leftBarName, "leftbar1");
		} else {
			strcpy(leftBarName, "leftbar1");
		}
		FrontImage_GetResourceRect(leftBarName, &leftBarRect);
		if (!g_frontendLeftBarAnimState) {
			FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
			FrontImage_AdvanceSpriteFrame(leftBarName, 1);
			if (FrontImage_GetSpriteFrame(leftBarName) == 9) {
				g_frontendLeftBarAnimState = 1;
			}
			return 0;
		}

		if (g_frontendLeftBarAnimState == 1) {
			FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
			if (localTeamOrSinglePlayer) {
				FrontendDraw_RectCopy(&dst, g_frontendSidebarButtonRects);
			} else {
				FrontendDraw_RectCopy(&dst, g_frontendSidebarButtonRects);
			}
			dst.right = dst.left + 24;
		} else if (g_frontendLeftBarAnimState == 2) {
			FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
			FrontImage_AdvanceSpriteFrame(leftBarName, 1);
			if (!FrontImage_GetSpriteFrame(leftBarName)) {
				g_frontendLeftBarAnimState = 3;
				return 0;
			}
			return 0;
		} else if (g_frontendLeftBarAnimState == 4) {
			FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
			FrontImage_AdvanceSpriteFrame(leftBarName, 1);
			if (FrontImage_GetSpriteFrame(leftBarName)) {
				return 0;
			}
			g_frontendLeftBarAnimState = 0;
			g_missionBriefingTextViewActive = 0;
			FrontendDisplay_LockOffscreenSurface();
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDraw_RectCopy(&dst, &g_briefingMapSourceRect);
			FrontendDraw_RectOffsetXY(&dst, 64, 81);
			dst.top = dst.bottom - 41;
			if (g_pilotData.campaignMode) {
				if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
					FrontImage_DrawSprite("briefbarfam", 60, 372);
				} else {
					FrontImage_DrawSprite("briefbartour", 60, 372);
				}
			} else {
				FrontImage_DrawSprite("briefbarcom", 60, 372);
			}
			FrontendDisplay_UnlockOffscreenSurface(1);
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			return 0;
		} else {
			return 0;
		}
	}

	buttonStates[g_missionBriefingTextViewActive + 5] = 2;
	if (!g_missionBriefingTextViewActive) {
		buttonStates[g_briefingPlaybackActive ^ 1] = 2;
	}

	FrontendCursor_GetPos(&outX, &outY);
	if (!g_missionBriefingTextViewActive) {
		if (g_briefingPlaybackActive) {
			if (FrontendDraw_PointInRect(&dst, outX, outY) &&
				(FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick())) {
				if (g_gameConfig.sfxDatapadEnabled) {
					FrontendSound_PlayUISound("jewelsound", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
											  63);
				}
				BriefingScript_AdvanceToNextVisibleLine();
				if (g_briefingTextSlotBlockIdx[1] == g_briefingLastNarratedTextBlockIdx) {
					g_briefingLastNarratedTextBlockIdx = 0xffff;
					g_briefingTextPageNumber = 0;
					BriefingScript_ResetState();
				}
			}
			FrontendButton_DrawCenteredTintedSpriteWithTooltip(
				&dst, "forward", FrontendString_Get(STR_FORWARD), (unsigned int)g_colorGreen);
		} else {
			result = FrontendButton_DrawSpriteWithHoverText(
				&dst, "forward", "forward", (void*)FrontendString_Get(STR_PLAY),
				(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 11, "jewelsound");
			if (result) {
				if (FrontendWaveStream_IsPaused()) {
					FrontendWaveStream_Resume();
				}
				g_briefingPlaybackActive = 1;
			}
		}

		FrontendDraw_RectCopy(&dst, &g_frontendSidebarButtonRects[2]);
		if (!g_briefingPlaybackActive) {
			FrontendButton_DrawCenteredTintedSpriteWithTooltip(&dst, "stop", FrontendString_Get(STR_STOP),
															   (unsigned int)g_colorGreen);
		} else {
			result = FrontendButton_DrawSpriteWithHoverText(
				&dst, "stop", "stop", (void*)FrontendString_Get(STR_STOP), (unsigned int)g_colorPaleBlue,
				(unsigned int)g_colorLightBlue, 12, "jewelsound");
			if (result) {
				FrontendWaveStream_Pause();
				g_briefingPlaybackActive = 0;
			}
		}

		FrontendDraw_RectCopy(&dst, &g_frontendSidebarButtonRects[1]);
		result = FrontendButton_DrawSpriteWithHoverText(
			&dst, "reverse", "reverse", (void*)FrontendString_Get(STR_REWIND), (unsigned int)g_colorPaleBlue,
			(unsigned int)g_colorLightBlue, 14, "jewelsound");
		if (result) {
			g_briefingLastNarratedTextBlockIdx = 0xffff;
			g_briefingTextPageNumber = 0;
			BriefingScript_ResetState();
		}
		FrontendDraw_RectCopy(&dst, g_frontendSidebarButtonRects);
	} else {
		FrontendDraw_RectCopy(&dst, g_frontendSidebarButtonRects);
	}
	if (g_missionBriefingTextViewActive == 1) {
		result = FrontendButton_DrawSpriteWithHoverText(
			&dst, "briefing", "briefing", (void*)FrontendString_Get(STR_VIEW_BRIEFING_MAP),
			(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 16, "jewelsound");
		if (result) {
			FrontendWaveStream_Shutdown();
			g_frontendLeftBarAnimState = 4;
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}
		return 0;
	}

	if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
		result = FrontendButton_DrawSpriteWithHoverText(
			&dst, "azrecord", "azrecord", (void*)FrontendString_Get(STR_FAMILY_MISSION_OVERVIEW),
			(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 17, "jewelsound");
	} else {
		result = FrontendButton_DrawSpriteWithHoverText(
			&dst, "todrecord", "todrecord", (void*)FrontendString_Get(STR_FAMILY_COMMANDER_BRIEFING),
			(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 17, "jewelsound");
	}
	if (result) {
		FrontendWaveStream_Shutdown();
		g_frontendLeftBarAnimState = 4;
		FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
	}
	return 0;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x566540
int16_t MissionBriefing_HandleMapMouseInput(FrontendRect* viewportRect, FrontendRect* clipRect,
											int suppressInput, int leftDown, int rightDown, int mouseX,
											int mouseY) {
	FrontendRect rect;
	FrontendRect dst;

	FrontendDraw_RectCopy(&dst, viewportRect);
	FrontendDraw_RectInsetXY(&dst, 1, 1);
	FrontendDraw_RectCopy(&rect, clipRect);
	FrontendDraw_RectInsetXY(&rect, 1, 1);
	if ((uint16_t)suppressInput) {
		return 0;
	}
	return BriefingMap_MouseInputStub(&dst, &rect, leftDown, rightDown, mouseX - 1, mouseY - 1);
}

// FUNCTION: XWA 0x564E90
int MissionBriefing_Update(int frameCounter) {
	int i;
	int packetType;
	int readyIndex;
	int previousUse3dHardware;
	int previous3dDevice;
	int previousBrightness;
	int slotIndex;
	const char* craftModelName;
	XwaFile* stream;
	FrontendRect rect;
	FrontendRect rc;
	FrontendRect src;
	int outX;
	int outY;

	if (!frameCounter) {
		int missionDirectoryId;

		missionDirectoryId = g_pilotData.missionDirectoryId;
		do {
			if (missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				g_skipFrontendEntryMovie = 1;
				break;
			}
			if (missionDirectoryId == MISSION_DIRECTORY_MELEE) {
				i = 0;
				while (i < g_teamCount) {
					if (g_teamFgCountScratch[i] > 1) {
						break;
					}
					++i;
				}
				if (i == g_teamCount) {
					g_skipFrontendEntryMovie = 1;
					break;
				}
			}
			g_pendingVoiceWav[0] = '\0';
			/* The modern asset VFS does not require the retail CD swap prompt. */
#ifndef XWA_MODERN
			if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
				!g_pilotData.campaignMode && missionDirectoryId == MISSION_DIRECTORY_TOUR) {
				int diskPresent;
				int requiredDisk;

				requiredDisk = (unsigned int)g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_TOUR] >=
							   (unsigned int)g_diskId;
				if (requiredDisk != g_currentCdDisk) {
					diskPresent = 0;
					do {
						if (File_CheckGameCdPresent(requiredDisk)) {
							g_currentCdDisk = requiredDisk;
							diskPresent = 1;
						}
						if (!diskPresent) {
							int result;

							if (requiredDisk) {
								result = FrontendDialog_ShowConfirmDialog(
									FrontendString_Get(STR_FAILED_TO_DETECT2_1),
									FrontendString_Get(STR_FAILED_TO_DETECT2_2), NULL,
									FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL));
							} else {
								result = FrontendDialog_ShowConfirmDialog(
									FrontendString_Get(STR_FAILED_TO_DETECT1_1),
									FrontendString_Get(STR_FAILED_TO_DETECT1_2), NULL,
									FrontendString_Get(STR_OKAY), FrontendString_Get(STR_CANCEL));
							}
							if (!result) {
								if (g_pilotData.campaignMode) {
									FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
								} else {
									g_skipFrontendEntryMovie = 1;
									FrontendScreen_SetCallbacks(MissionSetup_Update, MissionSetup_Exit);
								}
								return 0;
							}
							continue;
						}
					} while (!diskPresent);
				}
			}
#endif
		} while (0);
		if (g_frontendQuickStartLaunchFlag == 1) {
			FrontendMission_InitPlayerState();
			FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
			return 0;
		}

		if (g_pilotData.campaignMode) {
			if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				musicState = MUSIC_STATE_FRONTEND_1210;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1210);
				}
			} else {
				musicState = MUSIC_STATE_FRONTEND_1230;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1230);
				}
			}
		} else if (g_frontendMissionLoaded) {
			if ((unsigned int)g_currentMissionId < 7u) {
				musicState = MUSIC_STATE_FRONTEND_1210;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1210);
				}
			} else {
				musicState = MUSIC_STATE_FRONTEND_1240;
				if (g_gameConfig.datapadMusicEnabled) {
					Music_SetState(MUSIC_STATE_FRONTEND_1240);
				}
			}
		} else {
			musicState = MUSIC_STATE_FRONTEND_1240;
			if (g_gameConfig.datapadMusicEnabled) {
				Music_SetState(MUSIC_STATE_FRONTEND_1240);
			}
		}

		if (!FrontendDisplay_IsSecondaryDirectDrawActive()) {
			FrontendDisplay_SwitchDriver(0);
			FrontendColor_Init();
		}
		FrontendText_SetGlyphGradientBg(g_colorNearBlack);
		FrontendCursor_Show();
		ModelPreview_LoadTexture237();
		g_missionBriefingPendingExitAction = 0;
		g_frontendLeftBarAnimState = 0;
		FrontImage_SetSpriteFrame("leftbar4", 0);
		FrontImage_SetSpriteFrame("leftbar3", 0);
		FrontImage_SetSpriteFrame("leftbar1", 0);
		g_frontendRightBarAnimState = 0;
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			g_frontendRightBarPanelIndex = 2;
		} else {
			g_frontendRightBarPanelIndex = FrontendNet_IsTeamLocalPlayer(g_pilotData.team) ? 2 : 1;
		}
		FrontImage_SetSpriteFrame("rightbar1", 0);
		FrontImage_SetSpriteFrame("rightbar2", 0);
		FrontImage_SetSpriteFrame("rightbar3", 0);
		g_missionBriefingLaunchSent = 0;
		if (g_teamFgCountScratch[g_pilotData.team] > 1) {
			g_frontendChatTeamOnly = 1;
		}
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			g_frontendSinglePlayerFlightSessionActive = 1;
		}
		FrontendCursor_SetPos(
			g_frontendSidebarButtonRects[9].left +
				((g_frontendSidebarButtonRects[9].right - g_frontendSidebarButtonRects[9].left) >> 1),
			g_frontendSidebarButtonRects[9].top +
				((g_frontendSidebarButtonRects[9].bottom - g_frontendSidebarButtonRects[9].top) >> 1));
		g_briefingMapMajorGridColor = (short)FrontendDisplay_PackRGB(0x96, 0, 0);
		g_briefingMapMinorGridColor = (short)FrontendDisplay_PackRGB(0x50, 0, 0);
		g_frontendFirstVisibleLine = 0;
		g_missionBriefingReadyPlayerCount = 0;
		memset(g_missionBriefingReadyPlayerIds, 0, sizeof(g_missionBriefingReadyPlayerIds));
		g_frontendMissionOpcode99Count = 0;
		g_missionBriefingTextViewActive = 0;
		FrontendMission_LoadForBriefing();
		FrontImage_LoadResourceList((char*)"frontres\\mapicons\\mapicons.lst");
		if (g_briefingPreviewImageBuffer != NULL) {
			Mem_Free(g_briefingPreviewImageBuffer);
			g_briefingPreviewImageBuffer = NULL;
		}
		g_briefingPreviewImageBuffer = Mem_Alloc(0x96000u);
		if (g_briefingSelectionImageBuffer != NULL) {
			Mem_Free(g_briefingSelectionImageBuffer);
			g_briefingSelectionImageBuffer = NULL;
		}
		g_briefingSelectionImageBuffer = Mem_Alloc(0x96000u);
		g_unusedBriefingSelectionImageLoadState = 0;
		g_briefingModelDefaultPitchDeg = 110.0;
		yawDeg = 225.0;
		rollDeg = 0.0;
		g_briefingModelLightDx = 1;
		g_briefingModelLightDy = 1;
		g_briefingModelLightDz = 1;
		angleDeg = 0.0;
		TechLibrary_LoadSpecTextTable();
#ifdef XWA_MODERN
		DebugPrintf(NULL);
#else
		DebugPrintf();
#endif
		g_missionSetupDraggedPlayerId = 0;
		g_skipFrontendEntryMovie = 1;
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER &&
			g_pilotData.campaignMode) {
			if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				FrontImage_RegisterResourceDefault("frontres\\family\\markoholo.bmp", "background");
			} else {
				FrontImage_RegisterResourceDefault("frontres\\combat\\solo.bmp", "background");
			}
		} else {
			FrontImage_RegisterResourceDefault("frontres\\combat\\multiplayer.bmp", "background");
		}
		for (i = 0; i < 2; ++i) {
			if (i == 1) {
				FrontendDisplay_LockOffscreenSurface();
			}
			FrontImage_DrawSpriteOpaque("background", 0, 0);
			FrontendDraw_RectCopy(&rect, &g_briefingMapSourceRect);
			FrontendDraw_RectOffsetXY(&rect, 64, 81);
			rect.top = rect.bottom - 41;
			if (g_pilotData.campaignMode) {
				if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
					FrontImage_DrawSprite("briefbarfam", 60, 372);
				} else {
					FrontImage_DrawSprite("briefbartour", 60, 372);
				}
			} else {
				FrontImage_DrawSprite("briefbarcom", 60, 372);
			}
			if (i == 1) {
				FrontendDisplay_UnlockOffscreenSurface(1);
			}
		}
		FrontendText_ResetGlyphScratchBuffer(20);
		g_missionBriefingLaunchCountdownMs = 120000;
		g_missionBriefingTickNowMs = (int)GetTickCount();
		g_missionBriefingLastTickMs = g_missionBriefingTickNowMs;
		g_missionBriefingLastCountdownSecondSent = 120;
		if (g_briefingText != NULL) {
			Mem_Free(g_briefingText);
			g_briefingText = NULL;
		}
		g_briefingText = Mem_Alloc(0x1000u);
		MissionSetup_BuildBriefingText(g_briefingText);
		g_briefingMapRegionTransitionFrame = 0;
		g_briefingMapRegionTransitionState = 4;
		FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
	}

	if (FrontendDisplay_GetReactivatedFlag()) {
		if (g_briefingMapPrimaryHighlightActive && g_briefingMapPrimaryHighlightPhase <= 17 &&
			g_briefingMapPrimaryHighlightPhase >= 12 &&
			g_briefingMapCurrentRegionIcons[g_briefingMapPrimaryHighlightIconIndex].objectType !=
				(int8_t)0xe3) {
			craftModelName = FeDiskIo_GetCraftModelName((unsigned int)BriefingMap_GetSelectedObjectType());
			if (craftModelName != NULL) {
				strcpy(g_frontendScratchBuffer, craftModelName);
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 4] = '\0';
				strcat(g_frontendScratchBuffer, "Exterior.opt");
				stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
				if (stream == NULL) {
					ModelPreview_LoadModel(craftModelName, BriefingMap_GetSelectedObjectType());
				} else {
					File_Close(stream);
					ModelPreview_LoadModel(g_frontendScratchBuffer, BriefingMap_GetSelectedObjectType());
				}
				memset(&stats, 0, sizeof(stats));
				stats.craftType = BriefingMap_GetSelectedObjectType();
				BuildCraftTechStats(&stats);
			}
		}
	}

	if (g_gameConfig.use3dHardware[0] && g_briefingMapPrimaryHighlightActive) {
		if (g_briefingMapPrimaryHighlightPhase == 17) {
			uint16_t* row;

			/* NOTE(remaster): deliberately NOT emitted — this 0x2000 fill
			 * is the COLOR KEY for the dirty-rect keyed composite (the
			 * ship-inspect background shows through it), not a visible
			 * flash. Keyed texels must stay untouched in HD. */
			row = (uint16_t*)g_drawSurfacePtr;
			for (i = 0; i < 480; ++i) {
				uint32_t* pixels;
				int x;

				pixels = (uint32_t*)row;
				for (x = 0; x < 320; ++x) {
					pixels[x] = 0x20002000u;
				}
				row += (unsigned int)FrontendDisplay_GetDrawSurfacePitch() / 2;
			}
			FrontendDisplay_SetDirtyRectBlitEnabled(1);
		}
	} else if (g_briefingMapPrimaryHighlightPhase >= 16) {
		FrontendDisplay_RestoreBackBuffer();
	}

	if (g_missionBriefingPendingExitAction) {
		MissionBriefing_DrawPlaybackControls();
		if (g_frontendLeftBarAnimState == 3 && g_frontendRightBarAnimState == 3) {
			switch (g_missionBriefingPendingExitAction) {
				case 1:
					FrontendScreen_SetCallbacks(FrontendNet_JoinGameScreen,
												FrontendMissionList_FreeScreenResources);
					return 0;
				case 2:
					FrontendMission_InitPlayerState();
					FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
					return 0;
				case 3:
				case 4:
					FrontendScreen_SetCallbacks(MissionSetup_Update, MissionSetup_Exit);
					return 0;
				case 5:
					FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
					return 0;
				default:
					return 0;
			}
		}
		return 0;
	}

	FrontendDraw_RectAssign(&rect, 158, 52, 491, 68);
	FrontendDisplay_GetScreenClipRect(&src);
	FrontendDisplay_SetScreenClipRect640x480(&rect);
	sprintf(g_frontendScratchBuffer, "%s", g_missionList[g_selectedMissionListIndex].description);
	i = (int)strlen(g_frontendScratchBuffer) - 1;
	while (i > 0) {
		if (g_frontendScratchBuffer[i] == '(') {
			g_frontendScratchBuffer[i] = '\0';
			break;
		}
		--i;
	}
	FrontendText_DrawCentered(12, g_frontendScratchBuffer, &rect, g_colorLightBlue);
	FrontendDisplay_SetScreenClipRect640x480(&src);

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		while (1) {
			packetType = FrontendNet_ProcessNetworkPackets();
			if (!packetType) {
				break;
			}
			if (packetType == 'F') {
				Net_ShutdownDirectPlaySession();
				if (!Net_IsHost()) {
					FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_HOST_QUIT1),
													 FrontendString_Get(STR_HOST_QUIT2),
													 FrontendString_Get(STR_HOST_QUIT3), NULL, NULL);
				}
				g_frontendLeftBarAnimState = 2;
				g_frontendRightBarAnimState = 2;
				g_skipFrontendEntryMovie = 1;
				g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NET_CLIENT;
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				g_missionBriefingPendingExitAction = 1;
				break;
			}
			if (packetType == 'A') {
#ifdef XWA_MODERN
				DebugPrintf(NULL);
#else
				DebugPrintf();
#endif
			} else if (packetType != 'p') {
				if (packetType != 'S') {
					if (packetType == 'e') {
						if (g_frontendNetPacketArg0 < g_missionBriefingLaunchCountdownMs) {
							g_missionBriefingLaunchCountdownMs = g_frontendNetPacketArg0;
						}
					} else if (packetType == 'j') {
						readyIndex = 0;
						while (readyIndex < g_missionBriefingReadyPlayerCount &&
							   g_missionBriefingReadyPlayerIds[readyIndex] != g_frontendNetPacketArg0) {
							++readyIndex;
						}
						if (readyIndex < g_missionBriefingReadyPlayerCount) {
							--g_missionBriefingReadyPlayerCount;
						}
						if (readyIndex < 7) {
							for (i = readyIndex; i < 7; ++i) {
								g_missionBriefingReadyPlayerIds[i] = g_missionBriefingReadyPlayerIds[i + 1];
							}
						}
						g_missionBriefingReadyPlayerIds[7] = 0;
					} else if (packetType == 'k') {
						readyIndex = 0;
						while (readyIndex < g_missionBriefingReadyPlayerCount &&
							   g_missionBriefingReadyPlayerIds[readyIndex] != g_frontendNetPacketArg0) {
							++readyIndex;
						}
						if (readyIndex == g_missionBriefingReadyPlayerCount) {
							g_missionBriefingReadyPlayerIds[g_missionBriefingReadyPlayerCount] =
								g_frontendNetPacketArg0;
							++g_missionBriefingReadyPlayerCount;
						}
					} else if (packetType == 'u') {
						for (i = 0; i < 8; ++i) {
							if (g_mpRoster[i].playerId == g_frontendNetPacketSenderPlayerId) {
								g_mpRoster[i].rating = g_frontendNetPacketArg0;
								break;
							}
						}
					}
				} else {
					g_frontendLeftBarAnimState = 2;
					g_frontendRightBarAnimState = 2;
					g_skipFrontendEntryMovie = 1;
					FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
					g_missionBriefingPendingExitAction = 3;
					MissionBriefing_DrawPlaybackControls();
					return 0;
				}
			} else {
				MissionBriefing_HandleFlyPacketStub();
				g_frontendLeftBarAnimState = 2;
				g_frontendRightBarAnimState = 2;
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				g_missionBriefingPendingExitAction = 2;
				MissionBriefing_DrawPlaybackControls();
				return 0;
			}
		}
	}

	if (!g_missionBriefingTextViewActive) {
		FrontendDraw_RectAssign(&rect, 64, 81, 573, 419);
		FrontendDraw_RectAssign(&rc, 64, 81, 573, 419);
		FrontendDisplay_GetScreenClipRect(&src);
		FrontendDisplay_SetScreenClipRect640x480(&rc);
		FrontendCursor_GetPos(&outX, &outY);
		MissionBriefing_HandleMapMouseInput(&rect, &rc, 0, FrontendMouse_GetLeftDown(),
											FrontendMouse_GetRightDown(), outX, outY);
#ifdef XWA_MODERN
		BriefingMap_UpdatePlaybackAnimation();
#else
		BriefingMap_UpdatePlaybackAnimation(frameCounter);
#endif
		MissionBriefing_DrawMapViewport(&rect, &rc, 1);
		FrontendDisplay_SetScreenClipRect640x480(&src);
		if (!g_briefingMapPrimaryHighlightActive && !g_pilotData.campaignMode) {
			sprintf(g_frontendScratchBuffer, "%s %c%s", FrontendString_Get(STR_MISSION_BRIEFING), 4,
					g_frontendMission->teams[g_pilotData.team].name);
			FrontendText_Draw(12, g_frontendScratchBuffer, 66, 77, 0xffff);
		}
	} else {
		MissionBriefing_DrawTextPage();
#ifdef XWA_MODERN
		NetSession_StubReturnTrue();
#else
		NetSession_StubReturnTrue(frameCounter);
#endif
		if (g_frontendLeftBarAnimState == 1 && !FrontendWaveStream_IsPlaying() && g_pendingVoiceWav[0]) {
			g_pendingVoiceWav[strlen(g_pendingVoiceWav) - 5]++;
			if (!FrontendWaveStream_PlayWaveFile(g_pendingVoiceWav, 0, 0)) {
				g_pendingVoiceWav[0] = '\0';
			}
		}
	}

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && g_teamCount > 1) {
		FrontendDraw_RectAssign(&rect, 507, 452, 562, 464);
		Frontend_FormatSecondsToClockString((unsigned int)(g_missionBriefingLaunchCountdownMs / 1000));
		FrontendText_DrawCentered(12, g_frontendScratchBuffer, &rect, 0xffff);
	}
	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER && g_teamCount > 1) {
		g_missionBriefingTickNowMs = (int)GetTickCount();
		g_missionBriefingLaunchCountdownMs += g_missionBriefingLastTickMs - g_missionBriefingTickNowMs;
		if (Net_IsHost() &&
			g_missionBriefingLaunchCountdownMs / 1000 != g_missionBriefingLastCountdownSecondSent) {
			g_missionBriefingLastCountdownSecondSent = g_missionBriefingLaunchCountdownMs / 1000;
			g_frontendNetPacketScratch.packetType = 'e';
			*(int*)g_frontendNetPacketScratch.payload = g_missionBriefingLaunchCountdownMs;
			Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 8u);
		}
		if (g_missionBriefingLaunchCountdownMs < 0) {
			g_missionBriefingLaunchCountdownMs = 0;
			if (g_missionBriefingLaunchSent || !FrontendNet_IsTeamLocalPlayer(g_pilotData.team)) {
				return 0;
			}
			g_missionBriefingLaunchSent = 1;
			for (slotIndex = 0; slotIndex < 16; ++slotIndex) {
				if (g_combatSimSlots[slotIndex].ownerPlayerId &&
					g_frontendMission->flightGroups[g_combatSimSlots[slotIndex].fgIndex].team ==
						g_pilotData.team) {
					g_frontendNetPacketScratch.packetType = 'p';
					Net_SendPacketAndFlush(g_combatSimSlots[slotIndex].ownerPlayerId,
										   &g_frontendNetPacketScratch, 4u);
				}
			}
			return 0;
		}
		g_missionBriefingLastTickMs = g_missionBriefingTickNowMs;
	}

	MissionBriefing_DrawPlaybackControls();
	FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		if (NetSession_StubReturnTrue()) {
			if (g_frontendRightBarPanelIndex == 2) {
				if (g_frontendRightBarAnimState == 1) {
					int clicked;

					if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
						clicked = FrontendButton_DrawSpriteWithHoverText(
							&rect, "begin", "begin", (void*)FrontendString_Get(STR_MAP_TO_HANGAR),
							(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 7, "flysound");
					} else {
						clicked = FrontendButton_DrawSpriteWithHoverText(
							&rect, "begin", "begin", (void*)FrontendString_Get(STR_FLY),
							(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 7, "flysound");
					}
					if (clicked) {
						g_frontendLeftBarAnimState = 2;
						g_frontendRightBarAnimState = 2;
						FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
												  63);
						g_missionBriefingPendingExitAction = 2;
					}
				}
			} else if (g_frontendRightBarAnimState != 4) {
				g_frontendRightBarAnimState = 4;
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}
		} else if (g_frontendRightBarPanelIndex == 2 && g_frontendRightBarAnimState != 5) {
			g_frontendRightBarAnimState = 5;
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}
	} else if (FrontendNet_IsTeamLocalPlayer(g_pilotData.team)) {
		if (NetSession_StubReturnTrue()) {
			if (g_frontendRightBarPanelIndex == 2) {
				if (g_frontendRightBarAnimState == 1) {
					int clicked;

					if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
						clicked = FrontendButton_DrawSpriteWithHoverText(
							&rect, "begin", "begin", (void*)FrontendString_Get(STR_MAP_TO_HANGAR),
							(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 7, "flysound");
					} else {
						clicked = FrontendButton_DrawSpriteWithHoverText(
							&rect, "begin", "begin", (void*)FrontendString_Get(STR_FLY),
							(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 7, "flysound");
					}
					if (clicked) {
						for (slotIndex = 0; slotIndex < 16; ++slotIndex) {
							if (g_combatSimSlots[slotIndex].ownerPlayerId &&
								g_frontendMission->flightGroups[g_combatSimSlots[slotIndex].fgIndex].team ==
									g_pilotData.team) {
								g_frontendNetPacketScratch.packetType = 'p';
								Net_SendPacketAndFlush(g_combatSimSlots[slotIndex].ownerPlayerId,
													   &g_frontendNetPacketScratch, 4u);
							}
						}
					}
				}
			} else if (g_frontendRightBarAnimState != 4) {
				g_frontendRightBarAnimState = 4;
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
			}
		} else if (g_frontendRightBarPanelIndex == 2 && g_frontendRightBarAnimState != 5) {
			g_frontendRightBarAnimState = 5;
			FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
		}
	}

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[8]);
		if (g_frontendRightBarAnimState == 1) {
			int clicked;

			if (g_pilotData.campaignMode) {
				if (g_frontendMissionLoaded && (unsigned int)g_currentMissionId < 7u) {
					clicked = FrontendButton_DrawSpriteWithHoverText(
						&rect, "back", "back", (void*)FrontendString_Get(STR_BACK_TO_FAMILY_TRANSPORT),
						(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 8, "buttonsound");
				} else {
					clicked = FrontendButton_DrawSpriteWithHoverText(
						&rect, "back", "back", (void*)FrontendString_Get(STR_BACK_TO_CONCOURSE),
						(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 8, "buttonsound");
				}
			} else {
				clicked = FrontendButton_DrawSpriteWithHoverText(
					&rect, "back", "back", (void*)FrontendString_Get(STR_RETURN_SELECT_MISSION),
					(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 8, "buttonsound");
			}
			if (clicked) {
				g_skipFrontendEntryMovie = 1;
				g_frontendLeftBarAnimState = 2;
				g_frontendRightBarAnimState = 2;
				FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume, 63);
				g_missionBriefingPendingExitAction = (g_pilotData.campaignMode != 0) + 4;
			}
		}
	} else {
		if (g_frontendRightBarPanelIndex == 1) {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
		} else {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[8]);
		}
		if (g_frontendRightBarAnimState == 1) {
			if (Net_IsHost()) {
				if (FrontendButton_DrawSpriteWithHoverText(
						&rect, "back", "back", (void*)FrontendString_Get(STR_RETURN_SELECT_MISSION),
						(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 8, "buttonsound")) {
					if (FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_ARE_YOU_SURE_RESTART1),
														 FrontendString_Get(STR_ARE_YOU_SURE_RESTART2),
														 FrontendString_Get(STR_ARE_YOU_SURE_RESTART3),
														 FrontendString_Get(STR_OKAY),
														 FrontendString_Get(STR_CANCEL))) {
						g_frontendNetPacketScratch.packetType = 'S';
						Net_SendPacketAndFlush(0, &g_frontendNetPacketScratch, 4u);
					}
				}
			} else {
				if (FrontendButton_DrawSpriteWithHoverText(
						&rect, "back", "back", (void*)FrontendString_Get(STR_LEAVE),
						(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 8, "buttonsound")) {
					if (FrontendDialog_ShowConfirmDialog(FrontendString_Get(STR_GAME_IN_PROGRESS1),
														 FrontendString_Get(STR_GAME_IN_PROGRESS2),
														 FrontendString_Get(STR_GAME_IN_PROGRESS3),
														 FrontendString_Get(STR_OKAY),
														 FrontendString_Get(STR_CANCEL))) {
						g_skipFrontendEntryMovie = 1;
						g_frontendNetPacketScratch.packetType = 'G';
						Net_SendPacketAndFlush(Net_GetHostPlayerId(), &g_frontendNetPacketScratch, 4u);
						Net_ShutdownDirectPlaySession();
						g_frontendLeftBarAnimState = 2;
						g_frontendRightBarAnimState = 2;
						FrontendSound_PlayUISound("panelarm", 1, 0, 255, 12 * g_gameConfig.sfxDatapadVolume,
												  63);
						g_missionBriefingPendingExitAction = 1;
					}
				}
			}
		}
	}

	if (g_gameConfig.use3dHardware[0] && g_briefingMapPrimaryHighlightActive &&
		g_briefingMapPrimaryHighlightPhase == 17) {
		FrontendDisplay_SetDirtyRectBlitEnabled(1);
	}
	previousUse3dHardware = g_gameConfig.use3dHardware[0];
	previous3dDevice = g_gameConfig.threedDevice[0];
	previousBrightness = g_gameConfig.brightness[0];
	if (Frontend_HandleEscapeQuit(1) == 1) {
		return 1;
	}
	if (g_gameConfig.threedDevice[0] != previous3dDevice) {
		ModelPreview_FreeTexture237();
		ModelPreview_FreeResources();
		FrontendDisplay_SwitchDriver(0);
		FrontendColor_Init();
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("background", 0, 0);
		FrontendDraw_RectCopy(&rect, &g_briefingMapSourceRect);
		FrontendDraw_RectOffsetXY(&rect, 64, 81);
		rect.top = rect.bottom - 41;
		if (g_pilotData.campaignMode) {
			if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				FrontImage_DrawSprite("briefbarfam", 60, 372);
			} else {
				FrontImage_DrawSprite("briefbartour", 60, 372);
			}
		} else {
			FrontImage_DrawSprite("briefbarcom", 60, 372);
		}
		FrontendDisplay_UnlockOffscreenSurface(1);
		FrontendDisplay_BlitOffscreenToFront();
	}
	if (previousUse3dHardware != g_gameConfig.use3dHardware[0] ||
		previous3dDevice != g_gameConfig.threedDevice[0] ||
		previousBrightness != g_gameConfig.brightness[0]) {
		if (g_briefingMapPrimaryHighlightActive && g_briefingMapPrimaryHighlightPhase <= 17 &&
			g_briefingMapPrimaryHighlightPhase >= 12 &&
			g_briefingMapCurrentRegionIcons[g_briefingMapPrimaryHighlightIconIndex].objectType !=
				(int8_t)0xe3) {
			craftModelName = FeDiskIo_GetCraftModelName((unsigned int)BriefingMap_GetSelectedObjectType());
			if (craftModelName != NULL) {
				strcpy(g_frontendScratchBuffer, craftModelName);
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 4] = '\0';
				strcat(g_frontendScratchBuffer, "Exterior.opt");
				stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
				if (stream == NULL) {
					ModelPreview_LoadModel(craftModelName, BriefingMap_GetSelectedObjectType());
				} else {
					File_Close(stream);
					ModelPreview_LoadModel(g_frontendScratchBuffer, BriefingMap_GetSelectedObjectType());
				}
				memset(&stats, 0, sizeof(stats));
				stats.craftType = BriefingMap_GetSelectedObjectType();
				BuildCraftTechStats(&stats);
			}
		}
	}
	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		FrontendNet_UpdateAndDrawPanel(frameCounter);
	}
	return 0;
}

// FUNCTION: XWA 0x564DC0
int MissionBriefing_Exit(int frameCounter) {
	(void)frameCounter;

	BriefingText_FreeAllocatedBuffersExit();
	FrontImage_UnloadResourceList("frontres\\mapicons\\mapicons.lst");
	if (g_missionList != NULL) {
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}
	if (g_briefingText != NULL) {
		Mem_Free(g_briefingText);
		g_briefingText = NULL;
	}
	FrontImage_FreeResourceByName("background");
	FrontendWaveStream_Shutdown();
	if (g_techLibrarySpecTextTable != NULL) {
		Mem_Free(g_techLibrarySpecTextTable);
		g_techLibrarySpecTextTable = NULL;
	}
	if (g_briefingPreviewImageBuffer != NULL) {
		Mem_Free(g_briefingPreviewImageBuffer);
		g_briefingPreviewImageBuffer = NULL;
	}
	if (g_briefingSelectionImageBuffer != NULL) {
		Mem_Free(g_briefingSelectionImageBuffer);
		g_briefingSelectionImageBuffer = NULL;
	}
	ModelPreview_FreeResources();
	FrontImage_FreeResourceByName("dsbrief");
	Frontend_ResetScrollableControls();
	FrontendMouse_ClearInputGate();
	FrontendDisplay_RestorePrimaryDriver();
	FrontendColor_Init();
	return 0;
}
