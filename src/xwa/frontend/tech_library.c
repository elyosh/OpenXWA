#include "xwa/frontend/tech_library.h"

#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/fediskio.h"
#include "xwa/frontend/concourse.h"
#include "xwa/frontend/family_transport_room.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_escape.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/model_preview.h"
#include "xwa/movie/movie.h"
#include "xwa/util/memory.h"
#include "xwa/xwa_options.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// GLOBAL: XWA 0x783FF8
int g_techLibrarySelectedShipListIdx;

// GLOBAL: XWA 0x783FFC
int g_techLibraryBaseGradientBg;

// GLOBAL: XWA 0x784000
int g_techLibraryLightX;

// GLOBAL: XWA 0x784004
float g_techLibraryPreviewAngleD;

// GLOBAL: XWA 0x784008
int g_techLibraryPreviewDragActive;

// GLOBAL: XWA 0x784010
CraftTechStats g_techLibraryCraftStats;

// GLOBAL: XWA 0x784040
float g_techLibraryPreviewYawDeg;

// GLOBAL: XWA 0x784044
int g_techLibraryHighlightGradientBg;

// GLOBAL: XWA 0x784048
int g_techLibraryLightY;

// GLOBAL: XWA 0x78404C
int g_techLibraryDescriptionScrollCounter;

// GLOBAL: XWA 0x784050
int g_techLibraryModelLoadCountdown;

// GLOBAL: XWA 0x784054
int g_techLibrarySpecPageAnimCounter;

// GLOBAL: XWA 0x784058
int g_techLibraryPreviewLastMouseY;

// GLOBAL: XWA 0x78405C
int g_techLibraryZoomDistanceScale;

// GLOBAL: XWA 0x784060
int g_techLibraryLightZ;

// GLOBAL: XWA 0x784064
float g_techLibraryPreviewRollDeg;

// GLOBAL: XWA 0x784068
int g_techLibraryExitTransitionActive;

// GLOBAL: XWA 0x78406C
float g_techLibraryPreviewPitchDeg;

// GLOBAL: XWA 0x784070
int g_techLibraryPreviewLastMouseX;

// GLOBAL: XWA 0x784074
int g_techLibraryRotationPaused;

// GLOBAL: XWA 0x9F4A14
TechLibrarySpecText* g_techLibrarySpecTextTable;

// GLOBAL: XWA 0x5A9E38
const double g_techLibraryRatingScale = 0.4444444444444444;

// GLOBAL: XWA 0x5A9E40
const double g_techLibraryNegativeRoundingOffset = -0.5;

// GLOBAL: XWA 0x5A9E48
const float g_techLibraryManeuverScale = 0.0052315756f;

// GLOBAL: XWA 0x5A9E50
const double g_techLibraryMetersScale = 1600.0;

// GLOBAL: XWA 0x5A9E58
const double g_techLibraryQ16Scale = 0.0000152587890625;

// GLOBAL: XWA 0x5AB918
const float g_techLibraryZeroDegrees = 0.0f;

// GLOBAL: XWA 0x5AB91C
const float g_techLibraryNegativeAutoRotateDegrees = -5.0f;

// GLOBAL: XWA 0x5AB920
const double g_techLibraryFullRotationDegrees = 360.0;

// GLOBAL: XWA 0x5AB928
const double g_techLibraryQuarterRotationDegrees = 90.0;

// GLOBAL: XWA 0x5AB930
const double g_techLibraryThreeQuarterRotationDegrees = 270.0;

// GLOBAL: XWA 0x5AB938
const double g_techLibraryNegativeFullRotationDegrees = -360.0;

#ifdef XWA_MODERN
static void TechLibrary_AppendBounded(char* dest, size_t destSize, const char* source) {
	size_t length;

	if (destSize == 0) {
		return;
	}
	length = strlen(dest);
	if (length + 1 >= destSize) {
		return;
	}
	strncat(dest, source, destSize - length - 1);
}

static void TechLibrary_FormatCrewText(char* dest, size_t destSize, const char* source) {
	char word[128];
	size_t wordLen;
	size_t i;

	if (destSize == 0) {
		return;
	}

	dest[0] = '\0';
	memset(word, 0, sizeof(word));
	wordLen = 0;
	for (i = 0; source[i] != '\0'; ++i) {
		char c = source[i];

		if (c == ' ') {
			TechLibrary_AppendBounded(dest, destSize, word);
			memset(word, 0, sizeof(word));
			word[0] = c;
			wordLen = 1;
		} else {
			if (c == ':') {
				if (wordLen + 3 < sizeof(word)) {
					memmove(&word[1], word, wordLen);
					word[0] = 4;
					word[wordLen + 1] = c;
					word[wordLen + 2] = 1;
					wordLen += 3;
				}
			} else if (wordLen + 1 < sizeof(word)) {
				word[wordLen] = c;
				++wordLen;
			}
		}
	}
	TechLibrary_AppendBounded(dest, destSize, word);
}
#endif

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x4DCEB0
int BuildCraftTechStats(CraftTechStats* stats) {
	unsigned int objectType;
	uint16_t modelIndex;
	int boundSizeShift;
	int boundSize;
	int shiftedBound;
	int genusId;
	int group;

	objectType = g_objectTypeTables.craftTypeToObjectType[stats->craftType];
	stats->genusId = g_modelTypeTable[objectType].genusId;

	if (objectType < OBJ_Count) {
		modelIndex = g_modelTypeTable[(uint16_t)objectType].modelIndex;
	} else {
		modelIndex = UINT16_MAX;
	}
	if (modelIndex == UINT16_MAX) {
		return 0;
	}

	stats->speedRating = (int)((double)g_modelDefs[modelIndex].maxSpeed * g_techLibraryRatingScale -
							   g_techLibraryNegativeRoundingOffset);
	stats->accelerationRating = (int)((double)g_modelDefs[modelIndex].accelRate * g_techLibraryRatingScale -
									  g_techLibraryNegativeRoundingOffset);
	stats->maneuverRating = (int)((double)((uint16_t)g_modelDefs[modelIndex].rollRate +
										   (uint16_t)g_modelDefs[modelIndex].pitchRate) *
									  g_techLibraryManeuverScale -
								  g_techLibraryNegativeRoundingOffset);
	if (g_modelDefs[modelIndex].hasShields) {
		stats->shieldRating = g_modelDefs[modelIndex].shieldStrength / 50;
	} else {
		stats->shieldRating = 0;
	}
	stats->hullRating = g_modelDefs[modelIndex].hullStrength / 105u;

	boundSizeShift = g_modelDefs[modelIndex].boundSizeShift;
	boundSize = (int16_t)g_modelDefs[modelIndex].boundSizeX << boundSizeShift;
	shiftedBound = (int16_t)g_modelDefs[modelIndex].boundSizeZ << boundSizeShift;
	if (shiftedBound > boundSize) {
		boundSize = shiftedBound;
	}
	shiftedBound = (int16_t)g_modelDefs[modelIndex].boundSizeY << boundSizeShift;
	if (shiftedBound > boundSize) {
		boundSize = shiftedBound;
	}

	{
		double displayedSize;

		displayedSize = (double)boundSize;
		displayedSize *= g_techLibraryMetersScale;
		displayedSize *= g_techLibraryQ16Scale;
		stats->sizeRating = (int)displayedSize;
		genusId = stats->genusId;
		if (genusId == 4 || genusId == 5) {
			stats->shieldRating *= 16;
			stats->hullRating *= 16;
		}
		if (genusId == 3 || genusId == 17) {
			stats->shieldRating *= 4;
			stats->hullRating *= 4;
		}
	}

	stats->laserCount = 0;
	stats->ionCount = 0;
	for (group = 0; group < 3; ++group) {
		if (g_modelDefs[modelIndex].laserGroupWeaponType[group] == OBJ_LaserRebel ||
			g_modelDefs[modelIndex].laserGroupWeaponType[group] == OBJ_LaserImperial) {
			stats->laserCount += g_modelDefs[modelIndex].laserGroupSlotCount[group];
		}
		if (g_modelDefs[modelIndex].laserGroupWeaponType[group] == OBJ_LaserIon) {
			stats->ionCount += g_modelDefs[modelIndex].laserGroupSlotCount[group];
		}
	}

	stats->warheadRating = 0;
	for (group = 0; group < 2; ++group) {
		if (g_modelDefs[modelIndex].warheadLauncherType[group]) {
			int launcherSlotCount;
			int launcherValue;

			launcherSlotCount = g_modelDefs[modelIndex].warheadLauncherSlotCount[group];
			launcherValue = g_modelDefs[modelIndex].warheadLauncherValue[group];
			stats->warheadRating += launcherSlotCount * launcherValue;
		}
	}

	switch (objectType) {
		case OBJ_TIEAdvanced:
			stats->laserCount = 4;
			stats->ionCount = 0;
			stats->warheadRating = 8;
			break;
		case OBJ_Twing:
			stats->laserCount = 2;
			stats->ionCount = 0;
			stats->warheadRating = 8;
			break;
		case OBJ_Z95:
			stats->laserCount = 2;
			stats->ionCount = 0;
			break;
		case OBJ_R41:
			stats->laserCount = 2;
			stats->ionCount = 2;
			break;
		default:
			break;
	}

	stats->combatRating = ComputeCraftCombatRating((ObjectTypeId)objectType);
	return 1;
}

// FUNCTION: XWA 0x4DCE50
char* GetCraftTypeModelLongName(int craftType) {
	unsigned int objectType;
	uint16_t modelIndex;
	char* nameLong;

	objectType = g_objectTypeTables.craftTypeToObjectType[craftType];
	if ((uint16_t)objectType < OBJ_Count) {
		modelIndex = (uint16_t)g_modelTypeTable[(uint16_t)objectType].modelIndex;
	} else {
		modelIndex = UINT16_MAX;
	}
	if (modelIndex != UINT16_MAX) {
		nameLong = g_modelDefs[modelIndex].nameLong;
		if (nameLong == NULL) {
			return "NA";
		}
		return nameLong;
	}
	return NULL;
}

#ifndef XWA_MODERN
#pragma function(memcpy)
#endif
// FUNCTION: XWA 0x577140
int TechLibrary_LoadSpecTextTable(void) {
#ifdef XWA_MODERN
	const char* path = "specdesc.txt";
	XwaFile* stream;
	char line[1024];
	int entryIndex;
	int fieldIndex;

	stream = File_Open(AERON_VFS_ROOT_ASSET, path, "r");
	if (stream == NULL) {
		Aeron_LogError("xwa.assets", "Failed to open tech library spec text table '%s'", path);
		return 0;
	}

	if (g_techLibrarySpecTextTable != NULL) {
		Mem_Free(g_techLibrarySpecTextTable);
		g_techLibrarySpecTextTable = NULL;
	}

	g_techLibrarySpecTextTable = (TechLibrarySpecText*)Mem_Alloc(sizeof(TechLibrarySpecText) * 256u);
	if (g_techLibrarySpecTextTable == NULL) {
		Aeron_LogError("xwa.assets", "Failed to allocate tech library spec text table '%s'", path);
		File_Close(stream);
		return 0;
	}
	memset(g_techLibrarySpecTextTable, 0, sizeof(TechLibrarySpecText) * 256u);

	for (entryIndex = 0; entryIndex < 256; ++entryIndex) {
		fieldIndex = 0;
		while (fieldIndex < 5) {
			char* resolved;
			size_t length;

			if (!File_ReadLine(stream, line, sizeof(line))) {
				Aeron_LogError("xwa.assets", "Short tech library spec text table '%s' at entry %d field %d", path,
						  entryIndex, fieldIndex);
				File_Close(stream);
				return 1;
			}
			line[255] = '\0';
			if (line[0] == '/' && line[1] == '/') {
				continue;
			}

			length = strlen(line);
			if (length > 0 && line[length - 1] == '\n') {
				line[length - 1] = '\0';
				--length;
			}
			if (length > 0 && line[length - 1] == '\r') {
				line[length - 1] = '\0';
			}

			resolved = Linez_ResolveString(line);
			switch (fieldIndex) {
				case 0:
					strncpy(g_techLibrarySpecTextTable[entryIndex].designation, resolved, 64u);
					break;
				case 1:
					strncpy(g_techLibrarySpecTextTable[entryIndex].manufacturer, resolved, 64u);
					break;
				case 2:
					strncpy(g_techLibrarySpecTextTable[entryIndex].inUseBy, resolved, 64u);
					break;
				case 3:
					strncpy(g_techLibrarySpecTextTable[entryIndex].description, resolved, 256u);
					break;
				case 4:
					TechLibrary_FormatCrewText(g_techLibrarySpecTextTable[entryIndex].crew,
											   sizeof(g_techLibrarySpecTextTable[entryIndex].crew), resolved);
					break;
				default:
					break;
			}
			++fieldIndex;
		}
	}

	return File_Close(stream);
#else
	XwaFile* stream;
	int entryIndex;
	int fieldIndex;
	char word[128];
	char crewText[512];

	stream = File_Open(AERON_VFS_ROOT_ASSET, "specdesc.txt", "r");
	if (stream == NULL) {
		return 0;
	}

	if (g_techLibrarySpecTextTable != NULL) {
		Mem_Free(g_techLibrarySpecTextTable);
		g_techLibrarySpecTextTable = NULL;
	}

	g_techLibrarySpecTextTable = (TechLibrarySpecText*)Mem_Alloc(sizeof(TechLibrarySpecText) * 256u);
	memset(g_techLibrarySpecTextTable, 0, sizeof(TechLibrarySpecText) * 256u);
	if (g_techLibrarySpecTextTable == NULL) {
		File_Close(stream);
		return 0;
	}

	for (entryIndex = 0; entryIndex < 256; ++entryIndex) {
		fieldIndex = 0;
		do {
			size_t length;

			do {
				if (fgets(g_frontendScratchBuffer, 1024, stream) == NULL) {
					File_Close(stream);
					return 1;
				}
				g_frontendScratchBuffer[255] = '\0';
			} while (g_frontendScratchBuffer[0] == '/' && g_frontendScratchBuffer[1] == '/');

			length = strlen(g_frontendScratchBuffer);
			if (g_frontendScratchBuffer[length - 1] == '\n') {
				g_frontendScratchBuffer[length - 1] = '\0';
			}

			switch (fieldIndex) {
				case 0:
					strncpy(g_techLibrarySpecTextTable[entryIndex].designation,
							Linez_ResolveString(g_frontendScratchBuffer), 64u);
					break;
				case 1:
					strncpy(g_techLibrarySpecTextTable[entryIndex].manufacturer,
							Linez_ResolveString(g_frontendScratchBuffer), 64u);
					break;
				case 2:
					strncpy(g_techLibrarySpecTextTable[entryIndex].inUseBy,
							Linez_ResolveString(g_frontendScratchBuffer), 64u);
					break;
				case 3:
					strncpy(g_techLibrarySpecTextTable[entryIndex].description,
							Linez_ResolveString(g_frontendScratchBuffer), 256u);
					break;
				case 4: {
					int wordLength;
					int characterIndex;

					strncpy(crewText, Linez_ResolveString(g_frontendScratchBuffer), 64u);
					g_techLibrarySpecTextTable[entryIndex].crew[0] = '\0';
					memset(word, 0, sizeof(word));
					wordLength = 0;
					for (characterIndex = 0; characterIndex < (int)strlen(crewText); ++characterIndex) {
						if (crewText[characterIndex] == ' ') {
							strcat(g_techLibrarySpecTextTable[entryIndex].crew, word);
							memset(word, 0, sizeof(word));
							word[0] = crewText[characterIndex];
							wordLength = 1;
						} else {
							if (crewText[characterIndex] == ':') {
								memcpy(&word[1], word, strlen(word));
								word[0] = 4;
								word[++wordLength] = crewText[characterIndex];
								word[++wordLength] = 1;
							} else {
								word[wordLength] = crewText[characterIndex];
							}
							++wordLength;
						}
					}
					strcat(g_techLibrarySpecTextTable[entryIndex].crew, word);
					break;
				}
				default:
					break;
			}
			++fieldIndex;
		} while (fieldIndex < 5);
	}

	return File_Close(stream);
#endif
}
#ifndef XWA_MODERN
#pragma intrinsic(memcpy)
#endif

// FUNCTION: XWA 0x575C90
int TechLibrary_UpdateModelControls(void) {
	int mouseX;
	int mouseY;
	FrontendRect rightBarRect;
	FrontendRect dirtyRect;
	FrontendRect leftBarRect;
	char leftBarName[32] = "leftbar1";
	char rightBarName[32] = "rightbar1";
	int buttonResult;

	leftBarName[7] = (char)(g_frontendLeftBarPanelIndex + '0');
	rightBarName[8] = (char)(g_frontendRightBarPanelIndex + '0');
	FrontImage_GetResourceRect(leftBarName, &leftBarRect);
	FrontImage_GetResourceRect(rightBarName, &rightBarRect);

	if (g_frontendLeftBarAnimState != 3) {
		FrontendDraw_RectCopy(&dirtyRect, &leftBarRect);
		FrontendDraw_RectOffsetXY(&dirtyRect, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontendDraw_AddDirtyRect(&dirtyRect);
	}
	if (g_frontendRightBarAnimState != 3) {
		FrontendDraw_RectCopy(&dirtyRect, &rightBarRect);
		FrontendDraw_RectOffsetXY(&dirtyRect, rightBarRect.left - rightBarRect.right + 639,
								  leftBarRect.top - leftBarRect.bottom + 479);
		FrontendDraw_AddDirtyRect(&dirtyRect);
	}

	if (!g_frontendRightBarAnimState) {
		FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
							  rightBarRect.top - rightBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(rightBarName, 1);
		if (FrontImage_GetSpriteFrame(rightBarName) == 9) {
			g_frontendRightBarAnimState = 1;
		}
	} else {
		if (g_frontendRightBarAnimState == 1) {
			FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
								  rightBarRect.top - rightBarRect.bottom + 479);
		} else if (g_frontendRightBarAnimState == 2) {
			FrontImage_DrawSprite(rightBarName, rightBarRect.left - rightBarRect.right + 639,
								  rightBarRect.top - rightBarRect.bottom + 479);
			FrontImage_AdvanceSpriteFrame(rightBarName, 1);
			if (!FrontImage_GetSpriteFrame(rightBarName)) {
				g_frontendRightBarAnimState = 3;
			}
		}
	}

	if (!g_frontendLeftBarAnimState) {
		FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		FrontImage_AdvanceSpriteFrame(leftBarName, 1);
		if (FrontImage_GetSpriteFrame(leftBarName) == 9) {
			g_frontendLeftBarAnimState = 1;
		}
	} else {
		if (g_frontendLeftBarAnimState == 1) {
			FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
		} else if (g_frontendLeftBarAnimState == 2) {
			FrontImage_DrawSprite(leftBarName, 0, leftBarRect.top - leftBarRect.bottom + 479);
			FrontImage_AdvanceSpriteFrame(leftBarName, 1);
			if (!FrontImage_GetSpriteFrame(leftBarName)) {
				g_frontendLeftBarAnimState = 3;
			}
		}
	}

	if (g_frontendLeftBarAnimState != 1) {
		return 1;
	}

	FrontendCursor_GetPos(&mouseX, &mouseY);

	FrontendDraw_RectCopy(&dirtyRect, &g_frontendSidebarButtonRects[4]);
	buttonResult = FrontendButton_DrawSpriteWithHoverText(
		&dirtyRect, (char*)"lighting", (char*)"lighting", (void*)FrontendString_Get(STR_CHANGE_LIGHTING),
		(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 13, (char*)"jewelsound");
	if (buttonResult == 1) {
		--g_techLibraryLightX;
		if (g_techLibraryLightX == -2) {
			g_techLibraryLightX = 1;
			--g_techLibraryLightY;
			if (g_techLibraryLightY == -2) {
				g_techLibraryLightY = 1;
				--g_techLibraryLightZ;
				if (g_techLibraryLightZ == -2) {
					g_techLibraryLightZ = 1;
				}
			}
		}
		ModelPreview_SetWhiteDirectionalLight(g_techLibraryLightX, g_techLibraryLightY, g_techLibraryLightZ);
	} else if (buttonResult == 2) {
		++g_techLibraryLightX;
		if (g_techLibraryLightX == 2) {
			g_techLibraryLightX = -1;
			++g_techLibraryLightY;
			if (g_techLibraryLightY == 2) {
				g_techLibraryLightY = -1;
				++g_techLibraryLightZ;
				if (g_techLibraryLightZ == 2) {
					g_techLibraryLightZ = -1;
				}
			}
		}
		ModelPreview_SetWhiteDirectionalLight(g_techLibraryLightX, g_techLibraryLightY, g_techLibraryLightZ);
	}

	FrontendDraw_RectCopy(&dirtyRect, &g_frontendSidebarButtonRects[3]);
	buttonResult = FrontendButton_DrawSpriteWithHoverText(
		&dirtyRect, (char*)"rotation", (char*)"rotation",
		(void*)FrontendString_Get((UIString)(STR_ROTATION_ON + (g_techLibraryRotationPaused == 0))),
		g_techLibraryRotationPaused ? (unsigned int)g_colorLightBlue : (unsigned int)g_colorPaleBlue,
		(unsigned int)g_colorLightBlue, 12, (char*)"jewelsound");
	if (buttonResult) {
		g_techLibraryRotationPaused ^= 1;
	}

	FrontendDraw_RectCopy(&dirtyRect, &g_frontendSidebarButtonRects[2]);
	buttonResult = FrontendButton_DrawSpriteWithHoverText(
		&dirtyRect, (char*)"zoom", (char*)"zoom", (void*)FrontendString_Get(STR_ZOOM),
		(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 11, (char*)"jewelsound");
	if (buttonResult == 1) {
		if (g_techLibraryZoomDistanceScale < 8) {
			++g_techLibraryZoomDistanceScale;
			ModelPreview_SetObjectWorldPosition(0, 300 * g_techLibraryZoomDistanceScale, 0);
		}
	} else if (buttonResult == 2) {
		if (g_techLibraryZoomDistanceScale > 1) {
			--g_techLibraryZoomDistanceScale;
			ModelPreview_SetObjectWorldPosition(0, 300 * g_techLibraryZoomDistanceScale, 0);
		}
	}

	FrontendDraw_RectCopy(&dirtyRect, &g_frontendSidebarButtonRects[1]);
	buttonResult = FrontendButton_DrawSpriteWithHoverText(
		&dirtyRect, (char*)"nextcraft", (char*)"nextcraft", (void*)FrontendString_Get(STR_CYCLE_CRAFT),
		(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 17, (char*)"jewelsound");
	if (buttonResult == 1) {
		int selectedIndex = g_techLibrarySelectedShipListIdx;
		int previousIndex = selectedIndex;
		int shipCount = g_shipCount;
		ShipListEntry* shipList = g_shipList;

		do {
			++selectedIndex;
			g_techLibrarySelectedShipListIdx = selectedIndex;
			if (selectedIndex >= shipCount) {
				selectedIndex = 1;
				g_techLibrarySelectedShipListIdx = selectedIndex;
			}
		} while (selectedIndex != previousIndex && !g_pilotData.craftKnown[g_shipList[selectedIndex].typeId]);

		memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
		g_techLibraryCraftStats.craftType = shipList[selectedIndex].typeId;
		BuildCraftTechStats(&g_techLibraryCraftStats);
		g_techLibraryModelLoadCountdown = 40;
		ModelPreview_SetObjectWorldPosition(0, 300 * g_techLibraryZoomDistanceScale, 0);
		ModelPreview_SetWhiteDirectionalLight(g_techLibraryLightX, g_techLibraryLightY, g_techLibraryLightZ);
	} else if (buttonResult == 2) {
		int selectedIndex = g_techLibrarySelectedShipListIdx;
		int previousIndex = selectedIndex;
		int shipCount = g_shipCount;
		ShipListEntry* shipList = g_shipList;

		do {
			--selectedIndex;
			g_techLibrarySelectedShipListIdx = selectedIndex;
			if (selectedIndex < 1) {
				selectedIndex = shipCount - 1;
				g_techLibrarySelectedShipListIdx = selectedIndex;
			}
		} while (selectedIndex != previousIndex && !g_pilotData.craftKnown[g_shipList[selectedIndex].typeId]);

		memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
		g_techLibraryCraftStats.craftType = shipList[selectedIndex].typeId;
		BuildCraftTechStats(&g_techLibraryCraftStats);
		g_techLibraryModelLoadCountdown = 40;
		ModelPreview_SetObjectWorldPosition(0, 300 * g_techLibraryZoomDistanceScale, 0);
		ModelPreview_SetWhiteDirectionalLight(g_techLibraryLightX, g_techLibraryLightY, g_techLibraryLightZ);
	}

	FrontendDraw_RectCopy(&dirtyRect, g_frontendSidebarButtonRects);
	buttonResult = FrontendButton_DrawSpriteWithHoverText(
		&dirtyRect, (char*)"nextgenus", (char*)"nextgenus", (void*)FrontendString_Get(STR_CYCLE_GENUS),
		(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 16, (char*)"jewelsound");
	if (buttonResult == 1) {
		int selectedIndex = g_techLibrarySelectedShipListIdx;
		int previousIndex = selectedIndex;
		ShipListEntry* shipList = g_shipList;
		int category = g_shipList[selectedIndex].category;

		do {
			++selectedIndex;
			g_techLibrarySelectedShipListIdx = selectedIndex;
			if (selectedIndex >= g_shipCount) {
				selectedIndex = 1;
				g_techLibrarySelectedShipListIdx = 1;
			}
		} while (selectedIndex != previousIndex &&
				 (g_shipList[selectedIndex].category == category ||
				  !g_pilotData.craftKnown[g_shipList[selectedIndex].typeId]));

		memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
		g_techLibraryCraftStats.craftType = shipList[selectedIndex].typeId;
		BuildCraftTechStats(&g_techLibraryCraftStats);
		g_techLibraryModelLoadCountdown = 40;
		ModelPreview_SetObjectWorldPosition(0, 300 * g_techLibraryZoomDistanceScale, 0);
		ModelPreview_SetWhiteDirectionalLight(g_techLibraryLightX, g_techLibraryLightY, g_techLibraryLightZ);
	} else if (buttonResult == 2) {
		int selectedIndex = g_techLibrarySelectedShipListIdx;
		int previousIndex = selectedIndex;
		ShipListEntry* shipList = g_shipList;
		int category = g_shipList[selectedIndex].category;

		do {
			--selectedIndex;
			g_techLibrarySelectedShipListIdx = selectedIndex;
			if (selectedIndex < 1) {
				selectedIndex = g_shipCount - 1;
				g_techLibrarySelectedShipListIdx = g_shipCount - 1;
			}
		} while (selectedIndex != previousIndex &&
				 (g_shipList[selectedIndex].category == category ||
				  !g_pilotData.craftKnown[g_shipList[selectedIndex].typeId]));

		memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
		g_techLibraryCraftStats.craftType = shipList[selectedIndex].typeId;
		BuildCraftTechStats(&g_techLibraryCraftStats);
		g_techLibraryModelLoadCountdown = 40;
		ModelPreview_SetObjectWorldPosition(0, 300 * g_techLibraryZoomDistanceScale, 0);
		ModelPreview_SetWhiteDirectionalLight(g_techLibraryLightX, g_techLibraryLightY, g_techLibraryLightZ);
	}

	return 1;
}

// FUNCTION: XWA 0x576520
int TechLibrary_DrawCraftSpecPanel(void) {
	enum {
		TECH_LIBRARY_SPEC_FONT_SIZE = 10,
		TECH_LIBRARY_SPEC_ROW_HEIGHT = 15,
		TECH_LIBRARY_SPEC_PANEL_LEFT = 100,
		TECH_LIBRARY_SPEC_PANEL_TOP = 90,
		TECH_LIBRARY_SPEC_PANEL_RIGHT = 539,
		TECH_LIBRARY_SPEC_ROW_BOTTOM = 104,
		TECH_LIBRARY_SPEC_PANEL_BOTTOM = 149,
		TECH_LIBRARY_DESCRIPTION_LEFT = 203,
		TECH_LIBRARY_DESCRIPTION_TOP = 369,
		TECH_LIBRARY_DESCRIPTION_RIGHT = 436,
		TECH_LIBRARY_DESCRIPTION_BOTTOM = 429,
		TECH_LIBRARY_PAGE_FRAME_COUNT = 100,
		TECH_LIBRARY_PAGE_SLIDE_IN_FRAMES = 15,
		TECH_LIBRARY_PAGE_SLIDE_OUT_START = 85,
		TECH_LIBRARY_DESCRIPTION_FRAME_COUNT = 200,
		TECH_LIBRARY_DESCRIPTION_FADE_FRAMES = 50,
		TECH_LIBRARY_DESCRIPTION_FADE_OUT_START = 150,
		TECH_LIBRARY_DESCRIPTION_LINE_STEP = 5,
		TECH_LIBRARY_DESCRIPTION_MEASURE_SENTINEL = 0x7fffff,
	};

	FrontendRect rowRect;
	FrontendRect clipRect;
	int craftSpecIndex;
	int pageIndex;
	int pageFrame;
	int topClipOffset;
	int bottomClipTrim;
	int textColor;
	int descriptionPage;
	int descriptionFrame;
	int descriptionLineCount;

	FrontendDraw_RectAssign(&rowRect, TECH_LIBRARY_SPEC_PANEL_LEFT, TECH_LIBRARY_SPEC_PANEL_TOP,
							TECH_LIBRARY_SPEC_PANEL_RIGHT, TECH_LIBRARY_SPEC_ROW_BOTTOM);
	rowRect.bottom += TECH_LIBRARY_SPEC_PANEL_BOTTOM - TECH_LIBRARY_SPEC_ROW_BOTTOM;
	FrontendDraw_AddDirtyRect(&rowRect);

	rowRect.bottom = TECH_LIBRARY_SPEC_ROW_BOTTOM;
	craftSpecIndex = g_techLibraryCraftStats.craftType - 1;
	if (craftSpecIndex < 0) {
		craftSpecIndex = 0;
	}

	pageIndex = g_techLibrarySpecPageAnimCounter / TECH_LIBRARY_PAGE_FRAME_COUNT;
	pageFrame = g_techLibrarySpecPageAnimCounter % TECH_LIBRARY_PAGE_FRAME_COUNT;
	bottomClipTrim = 0;

	if (g_techLibraryCraftStats.genusId == 0) {
		if (pageIndex > 3) {
			pageIndex = 0;
			g_techLibrarySpecPageAnimCounter = 0;
		}
	} else if (g_techLibraryCraftStats.genusId == 1 || g_techLibraryCraftStats.genusId == 2 ||
			   g_techLibraryCraftStats.genusId == 3 || g_techLibraryCraftStats.genusId == 4 ||
			   g_techLibraryCraftStats.genusId == 5 || g_techLibraryCraftStats.genusId == 17) {
		if (pageIndex > 2) {
			pageIndex = 0;
			g_techLibrarySpecPageAnimCounter = 0;
		}
	} else if (pageIndex > 1) {
		pageIndex = 0;
		g_techLibrarySpecPageAnimCounter = 0;
	}

	if (pageFrame < TECH_LIBRARY_PAGE_SLIDE_IN_FRAMES) {
		topClipOffset = 0;
		bottomClipTrim = TECH_LIBRARY_PAGE_SLIDE_IN_FRAMES - 1 - pageFrame;
	} else if (pageFrame >= TECH_LIBRARY_PAGE_SLIDE_OUT_START) {
		topClipOffset = pageFrame - TECH_LIBRARY_PAGE_SLIDE_OUT_START;
	}
	textColor = g_colorPaleBlue;

	switch (pageIndex) {
		case 0:
			FrontendDraw_RectCopy(&clipRect, &rowRect);
			clipRect.top += topClipOffset;
			clipRect.bottom -= bottomClipTrim;
			FrontendDisplay_SetScreenClipRect640x480(&clipRect);
			sprintf(g_frontendScratchBuffer, "%c%s %c%s", 4, FrontendString_Get(STR_GENUS), 1,
					FrontendString_Get((UIString)(STR_STARFIGHTER + g_techLibraryCraftStats.genusId)));
			FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
									  textColor);
			FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);

			FrontendDraw_RectCopy(&clipRect, &rowRect);
			clipRect.top += topClipOffset;
			clipRect.bottom -= bottomClipTrim;
			FrontendDisplay_SetScreenClipRect640x480(&clipRect);
			sprintf(g_frontendScratchBuffer, "%c%s %c%s", 4, FrontendString_Get(STR_MANUFACTURER), 1,
					g_techLibrarySpecTextTable[craftSpecIndex].manufacturer);
			FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
									  textColor);
			FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);

			FrontendDraw_RectCopy(&clipRect, &rowRect);
			clipRect.top += topClipOffset;
			clipRect.bottom -= bottomClipTrim;
			FrontendDisplay_SetScreenClipRect640x480(&clipRect);
			sprintf(g_frontendScratchBuffer, "%c%s %c%s", 4, FrontendString_Get(STR_IN_USE_BY), 1,
					g_techLibrarySpecTextTable[craftSpecIndex].inUseBy);
			FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
									  textColor);
			break;

		case 1:
			FrontendDraw_RectCopy(&clipRect, &rowRect);
			clipRect.top += topClipOffset;
			clipRect.bottom -= bottomClipTrim;
			FrontendDisplay_SetScreenClipRect640x480(&clipRect);
			sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_SHIELD_RATING), 1,
					g_techLibraryCraftStats.shieldRating, FrontendString_Get(STR_SBD));
			FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
									  textColor);
			FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);

			FrontendDraw_RectCopy(&clipRect, &rowRect);
			clipRect.top += topClipOffset;
			clipRect.bottom -= bottomClipTrim;
			FrontendDisplay_SetScreenClipRect640x480(&clipRect);
			if (!g_techLibraryCraftStats.hullRating) {
				const char* hullUnit = FrontendString_Get(STR_RU);

				sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_HULL_RATING), 1, 1,
						hullUnit);
			} else {
				const char* hullUnit = FrontendString_Get(STR_RU);
				int hullRating = g_techLibraryCraftStats.hullRating;

				sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_HULL_RATING), 1,
						hullRating, hullUnit);
			}
			FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
									  textColor);
			FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);
			break;

		case 2:
			if (g_techLibraryCraftStats.genusId == 0) {
				FrontendDraw_RectCopy(&clipRect, &rowRect);
				clipRect.top += topClipOffset;
				clipRect.bottom -= bottomClipTrim;
				FrontendDisplay_SetScreenClipRect640x480(&clipRect);
				sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_SPEED), 1,
						g_techLibraryCraftStats.speedRating, FrontendString_Get(STR_MGLT));
				FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
										  textColor);
				FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);

				FrontendDraw_RectCopy(&clipRect, &rowRect);
				clipRect.top += topClipOffset;
				clipRect.bottom -= bottomClipTrim;
				FrontendDisplay_SetScreenClipRect640x480(&clipRect);
				sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4, FrontendString_Get(STR_ACCELERATION), 1,
						g_techLibraryCraftStats.accelerationRating, FrontendString_Get(STR_MGLT_PER_SECOND));
				FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
										  textColor);
				FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);

				FrontendDraw_RectCopy(&clipRect, &rowRect);
				clipRect.top += topClipOffset;
				clipRect.bottom -= bottomClipTrim;
				FrontendDisplay_SetScreenClipRect640x480(&clipRect);
				sprintf(g_frontendScratchBuffer, "%c%s %c%d %s", 4,
						FrontendString_Get(STR_MANUEVERABILITY_RATING), 1,
						g_techLibraryCraftStats.maneuverRating, FrontendString_Get(STR_DPF));
				FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
										  textColor);
			} else if (g_techLibraryCraftStats.genusId == 17) {
				double displayedSize;
				const char* unitText;

				FrontendDraw_RectCopy(&clipRect, &rowRect);
				clipRect.top += topClipOffset;
				clipRect.bottom -= bottomClipTrim;
				FrontendDisplay_SetScreenClipRect640x480(&clipRect);
				displayedSize = (double)ModelPreview_GetDisplayedSizeMeters();
				if (displayedSize >= 1000.0) {
					unitText = FrontendString_Get(STR_KM);
					sprintf(g_frontendScratchBuffer, "%c%s %c%.1f %s", 4, FrontendString_Get(STR_SIZE), 1,
							displayedSize * 0.001, unitText);
				} else {
					unitText = FrontendString_Get(STR_METERS);
					sprintf(g_frontendScratchBuffer, "%c%s %c%.1f %s", 4, FrontendString_Get(STR_SIZE), 1,
							displayedSize, unitText);
				}
				FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
										  textColor);
			} else if (g_techLibraryCraftStats.genusId == 1 || g_techLibraryCraftStats.genusId == 2 ||
					   g_techLibraryCraftStats.genusId == 3 || g_techLibraryCraftStats.genusId == 4 ||
					   g_techLibraryCraftStats.genusId == 5) {
				double displayedSize;
				const char* unitText;

				FrontendDraw_RectCopy(&clipRect, &rowRect);
				clipRect.top += topClipOffset;
				clipRect.bottom -= bottomClipTrim;
				FrontendDisplay_SetScreenClipRect640x480(&clipRect);
				displayedSize = (double)ModelPreview_GetDisplayedSizeMeters();
				if (displayedSize >= 1000.0) {
					unitText = FrontendString_Get(STR_KM);
					sprintf(g_frontendScratchBuffer, "%c%s %c%.1f %s", 4, FrontendString_Get(STR_SIZE), 1,
							displayedSize * 0.001, unitText);
				} else {
					unitText = FrontendString_Get(STR_METERS);
					sprintf(g_frontendScratchBuffer, "%c%s %c%.1f %s", 4, FrontendString_Get(STR_SIZE), 1,
							displayedSize, unitText);
				}
				FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
										  textColor);
				FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);

				FrontendDraw_RectCopy(&clipRect, &rowRect);
				clipRect.top += topClipOffset;
				clipRect.bottom -= bottomClipTrim;
				FrontendDisplay_SetScreenClipRect640x480(&clipRect);
				sprintf(g_frontendScratchBuffer, "%c%s %c%s", 4, FrontendString_Get(STR_CREW), 1,
						g_techLibrarySpecTextTable[craftSpecIndex].crew);
				FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
										  textColor);
				FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);
			}
			break;

		case 3:
			if (g_techLibraryCraftStats.genusId == 0) {
				FrontendDraw_RectCopy(&clipRect, &rowRect);
				clipRect.top += topClipOffset;
				clipRect.bottom -= bottomClipTrim;
				FrontendDisplay_SetScreenClipRect640x480(&clipRect);
				sprintf(g_frontendScratchBuffer, "%c%s %c", 4, FrontendString_Get(STR_LASERS), 1);
				if (g_techLibraryCraftStats.laserCount) {
					strcat(
						g_frontendScratchBuffer,
						FrontendString_Get((UIString)(STR_ION_CANNONS + g_techLibraryCraftStats.laserCount)));
					strcat(g_frontendScratchBuffer, " ");
					strcat(g_frontendScratchBuffer, FrontendString_Get(STR_TURBOLASERS));
					if (g_techLibraryCraftStats.ionCount) {
						strcat(g_frontendScratchBuffer, " ");
						strcat(g_frontendScratchBuffer, FrontendString_Get(STR_AND));
						strcat(g_frontendScratchBuffer, " ");
					}
				}
				if (g_techLibraryCraftStats.ionCount) {
					strcat(
						g_frontendScratchBuffer,
						FrontendString_Get((UIString)(STR_ION_CANNONS + g_techLibraryCraftStats.ionCount)));
					strcat(g_frontendScratchBuffer, " ");
					strcat(g_frontendScratchBuffer, FrontendString_Get(STR_ION_CANNONS));
				}
				FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
										  textColor);
				FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);

				FrontendDraw_RectCopy(&clipRect, &rowRect);
				clipRect.top += topClipOffset;
				clipRect.bottom -= bottomClipTrim;
				FrontendDisplay_SetScreenClipRect640x480(&clipRect);
				sprintf(g_frontendScratchBuffer, "%c%s %c%d", 4,
						FrontendString_Get(STR_WARHEAD_CAPACITY_RATING), 1,
						g_techLibraryCraftStats.warheadRating);
				FrontendText_DrawCentered(TECH_LIBRARY_SPEC_FONT_SIZE, g_frontendScratchBuffer, &rowRect,
										  textColor);
				FrontendDraw_RectOffsetXY(&rowRect, 0, TECH_LIBRARY_SPEC_ROW_HEIGHT);
			}
			break;

		default:
			break;
	}

	FrontendText_PushGlyphGradientBg(g_techLibraryHighlightGradientBg);

	descriptionPage = g_techLibraryDescriptionScrollCounter / TECH_LIBRARY_DESCRIPTION_FRAME_COUNT;
	descriptionFrame = g_techLibraryDescriptionScrollCounter % TECH_LIBRARY_DESCRIPTION_FRAME_COUNT;
	FrontendDraw_RectAssign(&rowRect, TECH_LIBRARY_DESCRIPTION_LEFT, TECH_LIBRARY_DESCRIPTION_TOP,
							TECH_LIBRARY_DESCRIPTION_RIGHT, TECH_LIBRARY_DESCRIPTION_BOTTOM);
	descriptionLineCount =
		FrontendText_DrawWrappedClipped(TECH_LIBRARY_SPEC_FONT_SIZE,
										g_techLibrarySpecTextTable[craftSpecIndex].description, &rowRect,
										0xffff, 2, TECH_LIBRARY_DESCRIPTION_MEASURE_SENTINEL) /
		TECH_LIBRARY_DESCRIPTION_LINE_STEP;
	if (descriptionPage > descriptionLineCount) {
		descriptionPage = 0;
		g_techLibraryDescriptionScrollCounter = 0;
	}

	FrontendDraw_RectAssign(&rowRect, TECH_LIBRARY_DESCRIPTION_LEFT, TECH_LIBRARY_DESCRIPTION_TOP,
							TECH_LIBRARY_DESCRIPTION_RIGHT, TECH_LIBRARY_DESCRIPTION_BOTTOM);
	FrontendDisplay_SetScreenClipRect640x480(&rowRect);
	FrontendDraw_AddDirtyRect(&rowRect);
	if (descriptionFrame < TECH_LIBRARY_DESCRIPTION_FADE_FRAMES) {
		FrontendText_DrawWrappedClippedEx(
			TECH_LIBRARY_SPEC_FONT_SIZE, g_techLibrarySpecTextTable[craftSpecIndex].description, &rowRect,
			0xffff, 2, TECH_LIBRARY_DESCRIPTION_LINE_STEP * descriptionPage, descriptionFrame);
	} else if (descriptionFrame > TECH_LIBRARY_DESCRIPTION_FADE_OUT_START) {
		FrontendText_DrawWrappedClippedEx(TECH_LIBRARY_SPEC_FONT_SIZE,
										  g_techLibrarySpecTextTable[craftSpecIndex].description, &rowRect,
										  0xffff, 2, TECH_LIBRARY_DESCRIPTION_LINE_STEP * descriptionPage,
										  TECH_LIBRARY_DESCRIPTION_FRAME_COUNT - 1 - descriptionFrame);
	} else {
		FrontendText_DrawWrappedClipped(TECH_LIBRARY_SPEC_FONT_SIZE,
										g_techLibrarySpecTextTable[craftSpecIndex].description, &rowRect,
										0xffff, 2, TECH_LIBRARY_DESCRIPTION_LINE_STEP * descriptionPage);
	}

	FrontendText_PopGlyphGradientBg();
	FrontendDisplay_ResetScreenClipRect();
	return 1;
}

// FUNCTION: XWA 0x577490
int TechLibrary_GenerateCraftSpecCache(void) {
	XwaFile* stream;
	int shipIndex;

	stream = File_Open(AERON_VFS_ROOT_USER, "spec.rci", "wb");
	if (stream != NULL) {
		for (shipIndex = 1; shipIndex < g_shipCount; ++shipIndex) {
			char* craftModelName;

			craftModelName = FeDiskIo_GetCraftModelName((unsigned int)g_shipList[shipIndex].typeId);
			if (craftModelName != NULL) {
				ModelPreview_LoadModel(craftModelName, g_shipList[shipIndex].typeId);
				memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
				g_techLibraryCraftStats.craftType = g_shipList[shipIndex].typeId;
				BuildCraftTechStats(&g_techLibraryCraftStats);
			}
			File_WriteCount(stream, &g_techLibraryCraftStats, sizeof(g_techLibraryCraftStats));
		}
		File_Close(stream);
	}

	return 1;
}

// FUNCTION: XWA 0x574D70
int TechLibrary_Update(int frameCounter) {
	enum {
		TECH_LIBRARY_VIEWPORT_LEFT = 120,
		TECH_LIBRARY_VIEWPORT_TOP = 76,
		TECH_LIBRARY_VIEWPORT_RIGHT = 519,
		TECH_LIBRARY_VIEWPORT_BOTTOM = 420,
		TECH_LIBRARY_FULLSCREEN_RIGHT = 640,
		TECH_LIBRARY_FULLSCREEN_BOTTOM = 480,
		TECH_LIBRARY_TITLE_TOP = 70,
		TECH_LIBRARY_TITLE_BOTTOM = 86,
		TECH_LIBRARY_MODEL_LOAD_FRAMES = 40,
		TECH_LIBRARY_MODEL_DISTANCE_STEP = 300,
		TECH_LIBRARY_AUTO_ROTATE_DEGREES = 5,
		TECH_LIBRARY_DRAG_ROTATE_RANGE_DEGREES = 720,
		TECH_LIBRARY_PANEL_ARM_SOUND_VOLUME_MUL = 12,
	};

	int mouseY;
	int mouseX;
	int craftSpecIndex;
	FrontendRect rect;
#ifdef XWA_MODERN
	static int entryMoviePending;

	if (frameCounter == 0 || entryMoviePending) {
#else
	if (frameCounter == 0) {
#endif
		int foundNewCraft;
		int shipIndex;

#ifdef XWA_MODERN
		if (!entryMoviePending) {
			FrontendDisplay_SwitchDriver(0);
			FrontendColor_Init();
			FrontendDisplay_DisableOffscreenRestore();
			FrontendDisplay_UnlockBackBuffer();
			FrontendDisplay_ClearBackBuffer();
			FrontendDisplay_PresentFrame();
			FrontendDisplay_ClearBackBuffer();
			if (Movie_Play("techroom", 1)) {
				entryMoviePending = 1;
				return 0;
			}
		}
		entryMoviePending = 0;
		FrontendDisplay_EnableOffscreenRestore();
#else
		FrontendDisplay_SwitchDriver(0);
		FrontendColor_Init();
		FrontendDisplay_DisableOffscreenRestore();
		FrontendDisplay_UnlockBackBuffer();
		FrontendDisplay_ClearBackBuffer();
		FrontendDisplay_PresentFrame();
		FrontendDisplay_ClearBackBuffer();
		Movie_Play("techroom", 1);
		FrontendDisplay_EnableOffscreenRestore();
#endif

		if (g_optGenerate) {
			TechLibrary_GenerateCraftSpecCache();
		}

		musicState = MUSIC_STATE_FRONTEND_1250;
		if (g_gameConfig.datapadMusicEnabled) {
			Music_SetState(MUSIC_STATE_FRONTEND_1250);
			Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
		} else {
			Music_Stop();
		}

		g_techLibraryPreviewDragActive = 0;
		g_techLibraryExitTransitionActive = 0;
		g_frontendLeftBarAnimState = 0;
		g_frontendLeftBarPanelIndex = 5;
		FrontImage_SetSpriteFrame("leftbar5", 0);
		g_frontendRightBarAnimState = 0;
		g_frontendRightBarPanelIndex = 1;
		FrontImage_SetSpriteFrame("rightbar1", 0);
		FrontImage_SetSpriteFrame("rightbar2", 0);
		FrontImage_SetSpriteFrame("rightbar3", 0);
		g_techLibraryRotationPaused = 0;
		g_techLibraryZoomDistanceScale = 1;

		g_techLibraryBaseGradientBg = FrontendDisplay_PackRGB(0x29, 0x5a, 0xc6);
		g_techLibraryHighlightGradientBg = FrontendDisplay_PackRGB(0x21, 0x73, 0xff);
		FrontendText_SetGlyphGradientBg(g_techLibraryBaseGradientBg);

		g_techLibraryModelLoadCountdown = 0;
		g_techLibrarySpecPageAnimCounter = 0;
		g_techLibraryDescriptionScrollCounter = 0;
		g_techLibrarySelectedShipListIdx = 1;

		foundNewCraft = 0;
		for (shipIndex = 1; shipIndex < g_shipCount; ++shipIndex) {
			if (g_pilotData.craftKnown[g_shipList[shipIndex].typeId] != 2) {
				++g_techLibrarySelectedShipListIdx;
				if (g_techLibrarySelectedShipListIdx >= g_shipCount) {
					g_techLibrarySelectedShipListIdx = 1;
					break;
				}
			} else {
				foundNewCraft = 1;
				break;
			}
		}
		if (!foundNewCraft) {
			g_techLibrarySelectedShipListIdx = 1;
			for (shipIndex = 1; shipIndex < g_shipCount; ++shipIndex) {
				if (g_pilotData.craftKnown[g_shipList[shipIndex].typeId]) {
					break;
				}
				++g_techLibrarySelectedShipListIdx;
				if (g_techLibrarySelectedShipListIdx >= g_shipCount) {
					g_techLibrarySelectedShipListIdx = 1;
					break;
				}
			}
		}

		g_techLibraryPreviewPitchDeg = 110.0f;
		g_techLibraryPreviewYawDeg = 225.0f;
		g_techLibraryPreviewRollDeg = 0.0f;
		g_techLibraryLightX = 1;
		g_techLibraryLightY = 1;
		g_techLibraryLightZ = 1;
		g_techLibraryPreviewAngleD = 0.0f;

		ModelPreview_LoadTexture237();
		TechLibrary_LoadSpecTextTable();
		memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
		g_techLibraryCraftStats.craftType = g_shipList[g_techLibrarySelectedShipListIdx].typeId;
		BuildCraftTechStats(&g_techLibraryCraftStats);
		g_techLibraryModelLoadCountdown = TECH_LIBRARY_MODEL_LOAD_FRAMES;
		ModelPreview_SetWhiteDirectionalLight(g_techLibraryLightX, g_techLibraryLightY, g_techLibraryLightZ);
		ModelPreview_SetObjectWorldPosition(
			0, TECH_LIBRARY_MODEL_DISTANCE_STEP * g_techLibraryZoomDistanceScale, 0);
		ModelPreview_SetNodeSwitchIndex(0);

		FrontImage_RegisterResourceDefault("frontres\\techroom\\techglobe.bmp", "globe");
		FrontImage_DrawSpriteOpaque("globe", 0, 0);
		FrontImage_DrawSprite("techroom", 0, 0);
		FrontImage_DrawSprite("techtop", 172, 0);
		FrontendDisplay_LockOffscreenSurface();
		FrontImage_DrawSpriteOpaque("globe", 0, 0);
		FrontImage_DrawSprite("techroom", 0, 0);
		FrontImage_DrawSprite("techtop", 172, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
		FrontendDisplay_BlitOffscreenToFront();
		FrontendText_ResetGlyphScratchBuffer(20);
		FrontendSound_PlayUISound((char*)"panelarm", 1, 0, 255,
								  TECH_LIBRARY_PANEL_ARM_SOUND_VOLUME_MUL * g_gameConfig.sfxDatapadVolume,
								  63);
	}

	if (g_gameConfig.use3dHardware[0]) {
		uint16_t* row;
		int y;

		/* NOTE(remaster): deliberately NOT emitted — this 0x2000 fill is
		 * the COLOR KEY for the dirty-rect keyed composite (unrendered
		 * texels show the room background through it), not a visible
		 * backdrop. The HD reconstruction draws the room background and
		 * the PiP model; keyed texels must stay untouched. */
		row = (uint16_t*)g_drawSurfacePtr;
		for (y = 0; y < TECH_LIBRARY_FULLSCREEN_BOTTOM; ++y) {
			uint32_t* pixels;
			int x;

			pixels = (uint32_t*)row;
			for (x = 0; x < 320; ++x) {
				pixels[x] = 0x20002000u;
			}
			row += (unsigned int)FrontendDisplay_GetDrawSurfacePitch() / sizeof(*row);
		}
	}

	if (g_techLibraryExitTransitionActive) {
		TechLibrary_UpdateModelControls();
		if (g_frontendLeftBarAnimState == 3 && g_frontendRightBarAnimState == 3) {
			switch (g_techLibraryExitTransitionActive) {
				case 1:
					FrontendScreen_SetCallbacks(Concourse_Update, Concourse_Exit);
					break;
				default:
					break;
			}
		}
		if (g_gameConfig.use3dHardware[0]) {
			FrontendDisplay_SetDirtyRectBlitEnabled(1);
		}
		return 0;
	}

	if (FrontendDisplay_GetReactivatedFlag()) {
		char* craftModelName;

		craftModelName =
			FeDiskIo_GetCraftModelName((unsigned int)g_shipList[g_techLibrarySelectedShipListIdx].typeId);
		if (craftModelName != NULL) {
			XwaFile* stream;

			strcpy(g_frontendScratchBuffer, craftModelName);
			g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 4] = '\0';
			strcat(g_frontendScratchBuffer, "Exterior.opt");
			stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
			if (stream == NULL) {
				ModelPreview_LoadModel(craftModelName, g_shipList[g_techLibrarySelectedShipListIdx].typeId);
			} else {
				File_Close(stream);
				ModelPreview_LoadModel(g_frontendScratchBuffer,
									   g_shipList[g_techLibrarySelectedShipListIdx].typeId);
			}
			ModelPreview_SetObjectWorldPosition(
				0, TECH_LIBRARY_MODEL_DISTANCE_STEP * g_techLibraryZoomDistanceScale, 0);
			memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
			g_techLibraryCraftStats.craftType = g_shipList[g_techLibrarySelectedShipListIdx].typeId;
			BuildCraftTechStats(&g_techLibraryCraftStats);
		}
	}

	FrontendCursor_GetPos(&mouseX, &mouseY);
	craftSpecIndex = g_techLibraryCraftStats.craftType - 1;

	FrontendDraw_RectAssign(&rect, TECH_LIBRARY_VIEWPORT_LEFT, TECH_LIBRARY_VIEWPORT_TOP,
							TECH_LIBRARY_VIEWPORT_RIGHT, TECH_LIBRARY_VIEWPORT_BOTTOM);
	if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
		FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor1", g_cursorBitmap);
	} else {
		FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor", g_cursorBitmap);
	}

	if (g_techLibraryModelLoadCountdown == 0) {
		FrontendDraw_RectAssign(&rect, 0, 0, TECH_LIBRARY_FULLSCREEN_RIGHT, TECH_LIBRARY_FULLSCREEN_BOTTOM);
		ModelPreview_SetObjectEulerDegrees(g_techLibraryPreviewPitchDeg, g_techLibraryPreviewYawDeg,
										   g_techLibraryPreviewRollDeg);
		ModelPreview_SetObjectAngleDDegrees(g_techLibraryPreviewAngleD);
		ModelPreview_RenderViewport(rect.left, rect.top, rect.right - rect.left + 1,
									rect.bottom - rect.top + 1, NULL, 0, 0);
		FrontImage_GetResourceRect("globe", &rect);
		rect.bottom = TECH_LIBRARY_VIEWPORT_BOTTOM;
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY)) {
			if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
				FrontendCursor_SetImageResourceForCurrentTheme((char*)"cursor2", g_cursorBitmap);
				if (!g_techLibraryPreviewDragActive) {
					g_techLibraryPreviewLastMouseX = mouseX;
					g_techLibraryPreviewLastMouseY = mouseY;
					g_techLibraryPreviewDragActive = 1;
				} else {
					int deltaY;
					int deltaX;
					int angleDelta;

					deltaX = g_techLibraryPreviewLastMouseX - mouseX;
					deltaY = mouseY - g_techLibraryPreviewLastMouseY;
					if (g_techLibraryPreviewYawDeg < g_techLibraryQuarterRotationDegrees ||
						g_techLibraryPreviewYawDeg > g_techLibraryThreeQuarterRotationDegrees) {
						deltaY = -deltaY;
					}
					g_techLibraryPreviewLastMouseX = mouseX;
					g_techLibraryPreviewLastMouseY = mouseY;

					angleDelta =
						TECH_LIBRARY_DRAG_ROTATE_RANGE_DEGREES * deltaX / (rect.right - rect.left + 1);
					g_techLibraryPreviewYawDeg += (float)angleDelta;
					angleDelta =
						TECH_LIBRARY_DRAG_ROTATE_RANGE_DEGREES * deltaY / (rect.bottom - rect.top + 1);
					g_techLibraryPreviewPitchDeg += (float)angleDelta;
					if (g_techLibraryPreviewYawDeg >= g_techLibraryFullRotationDegrees) {
						g_techLibraryPreviewYawDeg -= g_techLibraryFullRotationDegrees;
					} else if (g_techLibraryPreviewYawDeg < g_techLibraryZeroDegrees) {
						g_techLibraryPreviewYawDeg -= g_techLibraryNegativeFullRotationDegrees;
					}
					if (g_techLibraryPreviewPitchDeg >= g_techLibraryFullRotationDegrees) {
						g_techLibraryPreviewPitchDeg -= g_techLibraryFullRotationDegrees;
					} else if (g_techLibraryPreviewPitchDeg < g_techLibraryZeroDegrees) {
						g_techLibraryPreviewPitchDeg -= g_techLibraryNegativeFullRotationDegrees;
					}
				}
			} else {
				g_techLibraryPreviewDragActive = 0;
				if (!g_techLibraryRotationPaused) {
					g_techLibraryPreviewYawDeg -= g_techLibraryNegativeAutoRotateDegrees;
					if (g_techLibraryPreviewYawDeg >= g_techLibraryFullRotationDegrees) {
						g_techLibraryPreviewYawDeg = 0.0f;
					}
				}
			}
		} else {
			g_techLibraryPreviewDragActive = 0;
			if (!g_techLibraryRotationPaused) {
				g_techLibraryPreviewYawDeg -= g_techLibraryNegativeAutoRotateDegrees;
				if (g_techLibraryPreviewYawDeg >= g_techLibraryFullRotationDegrees) {
					g_techLibraryPreviewYawDeg = 0.0f;
				}
			}
		}
		TechLibrary_DrawCraftSpecPanel();
		++g_techLibrarySpecPageAnimCounter;
		++g_techLibraryDescriptionScrollCounter;
	} else {
		if (g_techLibraryModelLoadCountdown == 1) {
			char* craftModelName;

			craftModelName =
				FeDiskIo_GetCraftModelName((unsigned int)g_shipList[g_techLibrarySelectedShipListIdx].typeId);
			if (craftModelName != NULL) {
				XwaFile* stream;

				strcpy(g_frontendScratchBuffer, craftModelName);
				g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 4] = '\0';
				strcat(g_frontendScratchBuffer, "Exterior.opt");
				stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
				if (stream == NULL) {
					ModelPreview_LoadModel(craftModelName,
										   g_shipList[g_techLibrarySelectedShipListIdx].typeId);
				} else {
					ModelPreview_LoadModel(g_frontendScratchBuffer,
										   g_shipList[g_techLibrarySelectedShipListIdx].typeId);
				}
				ModelPreview_SetObjectWorldPosition(
					0, TECH_LIBRARY_MODEL_DISTANCE_STEP * g_techLibraryZoomDistanceScale, 0);
				memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
				g_techLibraryCraftStats.craftType = g_shipList[g_techLibrarySelectedShipListIdx].typeId;
				BuildCraftTechStats(&g_techLibraryCraftStats);
				g_pilotData.craftKnown[g_shipList[g_techLibrarySelectedShipListIdx].typeId] = 1;
			}
			g_techLibrarySpecPageAnimCounter = 0;
			g_techLibraryDescriptionScrollCounter = 0;
		}
		if (((g_techLibraryModelLoadCountdown / 10) & 1) == 0) {
			FrontendDraw_RectAssign(&rect, 0, 0, TECH_LIBRARY_FULLSCREEN_RIGHT,
									TECH_LIBRARY_FULLSCREEN_BOTTOM);
			FrontendDraw_ForceFullScreenPresent();
			FrontendText_PushGlyphGradientBg(g_techLibraryHighlightGradientBg);
			FrontendText_DrawCentered(12, FrontendString_Get(STR_TECHROOM_ACCESSING_DATABASE), &rect,
									  g_colorSlateBlue);
			FrontendText_PopGlyphGradientBg();
		}
		--g_techLibraryModelLoadCountdown;
	}

	FrontendDraw_RectAssign(&rect, 0, TECH_LIBRARY_TITLE_TOP, 639, TECH_LIBRARY_TITLE_BOTTOM);
	sprintf(g_frontendScratchBuffer, "%s", g_techLibrarySpecTextTable[craftSpecIndex].designation);
	{
		int halfTextWidth = FrontendText_MeasureWidth(g_frontendScratchBuffer, 12) >> 1;

		rect.left = 320 - halfTextWidth;
		rect.right = 320 + halfTextWidth;
	}
	FrontendDraw_AddDirtyRect(&rect);
	FrontendText_DrawCentered(12, g_frontendScratchBuffer, &rect, g_colorOrangeRed);

	if (!g_gameConfig.use3dHardware[0]) {
		FrontImage_DrawSprite("techroom", 0, 0);
	}
	FrontImage_DrawSprite("techtop", 172, 0);
	FrontImage_GetResourceRect("techtop", &rect);
	FrontendDraw_RectOffsetXY(&rect, 172, 0);
	FrontendDraw_AddDirtyRect(&rect);

	{
		int exitTransitionState;

		exitTransitionState = 1;
		TechLibrary_UpdateModelControls();
		if (g_frontendRightBarAnimState == exitTransitionState) {
			FrontendDraw_RectCopy(&rect, &g_frontendSidebarButtonRects[9]);
			if (FrontendButton_DrawSpriteWithHoverText(
					&rect, (char*)"back", (char*)"back", (void*)FrontendString_Get(STR_BACK_TO_CONCOURSE),
					(unsigned int)g_colorPaleBlue, (unsigned int)g_colorLightBlue, 240,
					(char*)"jewelsound")) {
				g_frontendLeftBarAnimState = 2;
				g_frontendRightBarAnimState = 2;
				FrontendSound_PlayUISound(
					(char*)"panelarm", exitTransitionState, 0, 255,
					TECH_LIBRARY_PANEL_ARM_SOUND_VOLUME_MUL * g_gameConfig.sfxDatapadVolume, 63);
				g_techLibraryExitTransitionActive = exitTransitionState;
			}
		}

		FrontendDraw_RectAssign(&rect, 0, 191, 50, 265);
		if (FrontendDraw_PointInRect(&rect, mouseX, mouseY) &&
			(FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick())) {
			g_frontendLeftBarAnimState = 2;
			g_frontendRightBarAnimState = 2;
			FrontendSound_PlayUISound((char*)"panelarm", exitTransitionState, 0, 255,
									  TECH_LIBRARY_PANEL_ARM_SOUND_VOLUME_MUL * g_gameConfig.sfxDatapadVolume,
									  63);
			g_techLibraryExitTransitionActive = exitTransitionState;
		}

		{
			int previousUse3dHardware;
			int previous3dDevice;
			int previousBrightness;
			int escapeResult;

			if (g_gameConfig.use3dHardware[0]) {
				FrontendDisplay_SetDirtyRectBlitEnabled(exitTransitionState);
			}
			previousUse3dHardware = g_gameConfig.use3dHardware[0];
			previous3dDevice = g_gameConfig.threedDevice[0];
			previousBrightness = g_gameConfig.brightness[0];
			escapeResult = Frontend_HandleEscapeQuit(3);
			if (escapeResult == 1) {
				return 1;
			}

			if (g_gameConfig.threedDevice[0] != previous3dDevice) {
				ModelPreview_FreeTexture237();
				ModelPreview_FreeResources();
				FrontendDisplay_SwitchDriver(0);
				FrontendColor_Init();
				FrontendDisplay_LockOffscreenSurface();
				FrontImage_DrawSpriteOpaque("globe", 0, 0);
				FrontImage_DrawSprite("techroom", 0, 0);
				FrontImage_DrawSprite("techtop", 172, 0);
				FrontendDisplay_UnlockOffscreenSurface(1);
				FrontendDisplay_BlitOffscreenToFront();
			}

			if (previousUse3dHardware != g_gameConfig.use3dHardware[0] ||
				g_gameConfig.threedDevice[0] != previous3dDevice ||
				g_gameConfig.brightness[0] != previousBrightness) {
				char* craftModelName;

				craftModelName = FeDiskIo_GetCraftModelName(
					(unsigned int)g_shipList[g_techLibrarySelectedShipListIdx].typeId);
				if (craftModelName != NULL) {
					XwaFile* stream;

					strcpy(g_frontendScratchBuffer, craftModelName);
					g_frontendScratchBuffer[strlen(g_frontendScratchBuffer) - 4] = '\0';
					strcat(g_frontendScratchBuffer, "Exterior.opt");
					stream = File_Open(AERON_VFS_ROOT_ASSET, g_frontendScratchBuffer, "rb");
					if (stream == NULL) {
						ModelPreview_LoadModel(craftModelName,
											   g_shipList[g_techLibrarySelectedShipListIdx].typeId);
					} else {
						File_Close(stream);
						ModelPreview_LoadModel(g_frontendScratchBuffer,
											   g_shipList[g_techLibrarySelectedShipListIdx].typeId);
					}
					ModelPreview_SetObjectWorldPosition(
						0, TECH_LIBRARY_MODEL_DISTANCE_STEP * g_techLibraryZoomDistanceScale, 0);
					memset(&g_techLibraryCraftStats, 0, sizeof(g_techLibraryCraftStats));
					g_techLibraryCraftStats.craftType = g_shipList[g_techLibrarySelectedShipListIdx].typeId;
					BuildCraftTechStats(&g_techLibraryCraftStats);
				}
			}
		}
	}

	FrontendDisplay_ResetScreenClipRect();
	return 0;
}

// FUNCTION: XWA 0x574D10
int TechLibrary_Exit(int frameCounter) {
	(void)frameCounter;

	FrontImage_FreeResourceByName("globe");
	Keyboard_FlushCharBuffer();
	FrontendScreen_PopState();
	if (g_techLibrarySpecTextTable != NULL) {
		Mem_Free(g_techLibrarySpecTextTable);
		g_techLibrarySpecTextTable = NULL;
	}
	FrontendText_ResetGlyphScratch();
	ModelPreview_FreeResources();
	FrontendDisplay_RestorePrimaryDriver();
	FrontendColor_Init();
	g_pilotData.newCraftAddedToTechRoom = 0;
	return 0;
}
