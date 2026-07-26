#include "xwa/flight/film.h"
#include "xwa/flight/fediskio.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/frontend_button.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/render/renderer.h"

#include <stdio.h>
#include <string.h>

// GLOBAL: XWA 0x80DA40
uint8_t* g_filmWriteBuffer;
// GLOBAL: XWA 0x7FFD78
MemoryHandle g_filmWriteBufferHandle;
// GLOBAL: XWA 0x910798
int g_filmWriteBufferSize;
// GLOBAL: XWA 0x8C1CD4
int g_filmWriteBufferedBytes;
// GLOBAL: XWA 0x783020
int g_filmNamePromptOverwriteConfirm;
// GLOBAL: XWA 0x783028
char g_filmNamePromptName[128];

static int g_filmNamePromptRunActive;

// FUNCTION: XWA 0x541260
int FilmNamePrompt_Update(int frameCounter) {
	FrontendRect rect;
	XwaFile* existingFile;
	int buttonPressed;

	if (!frameCounter) {
		g_activeTextFieldId = 0;
		g_filmNamePromptOverwriteConfirm = 0;
		strcpy(g_filmNamePromptName, FrontendString_Get(STR_FILM_DEFAULT));
		FrontendCursor_SetImageResourceForCurrentTheme("cursor", g_cursorBitmap);
		FrontendCursor_SetPos(90, 236);
		FrontendText_PushGlyphGradientBg(g_colorNearBlack);
		Keyboard_FlushCharBuffer();
		FrontendDisplay_ClearBackBuffer();
		FrontImage_DrawSprite("dialogbox", 0, 0);
		FrontendDisplay_LockOffscreenSurface();
		FrontendDisplay_ClearBackBuffer();
		FrontImage_DrawSprite("dialogbox", 0, 0);
		FrontendDisplay_UnlockOffscreenSurface(1);
		FrontendText_ResetGlyphScratchBuffer(20);
	}

	FrontImage_DrawSprite("dialogbox", 0, 0);
	if (!g_filmNamePromptOverwriteConfirm) {
		FrontendDraw_RectAssign(&rect, 102, 216, 538, 236);
		FrontendText_DrawCentered(15, FrontendString_Get(STR_FILM_HEADER), &rect, g_colorLightBlue);
		FrontendDraw_RectOffsetXY(&rect, 0, 20);
		rect.left += 70;
		rect.right -= 70;
		FrontendDraw_FillRectTranslucent(&rect, 0, 0, (unsigned int)g_colorSlateBlue);
		buttonPressed =
			FrontendText_DrawEditableField(&rect, g_filmNamePromptName, 64, 0, 12, "\\*$~|:<>?/\t\".");

		FrontImage_GetResourceRect("yesbutton", &rect);
		FrontendDraw_RectOffsetXY(&rect, 62, 236 - ((rect.bottom - rect.top + 1) >> 1));
		buttonPressed |= FrontendButton_DrawSpriteHitTest(
			&rect, "yesbutton", "yesbutton", FrontendString_Get(STR_OKAY), 12, 0xffff, 20, "buttonsound");
		if (Keyboard_PeekChar() == '\r') {
			Keyboard_DequeueChar();
			buttonPressed = 1;
		}
		if (buttonPressed) {
			sprintf(g_frontendScratchBuffer, "film\\%s.flm", g_filmNamePromptName);
			existingFile = File_Open(AERON_VFS_ROOT_USER, g_frontendScratchBuffer, "rb");
			if (existingFile == 0) {
				FrontendMouse_ClearClicks();
				g_activeTextFieldId = 0;
				FrontendDraw_ForceFullScreenPresent();
				Keyboard_FlushCharBuffer();
				FrontendScreen_PopState();
				FrontendText_PopGlyphGradientBg();
				FrontendMouse_ClearInputGate();
				FrontendText_ResetGlyphScratch();
				FrontendScrollbar_RestoreState();
				return 1;
			}

			File_Close(existingFile);
			g_filmNamePromptOverwriteConfirm = 1;
			FrontendMouse_ClearClicks();
			FrontendCursor_SetPos(556, 236);
		}

		FrontImage_GetResourceRect("nobutton", &rect);
		FrontendDraw_RectOffsetXY(&rect, 528, 236 - ((rect.bottom - rect.top + 1) >> 1));
		buttonPressed = FrontendButton_DrawSpriteHitTest(
			&rect, "nobutton", "nobutton", FrontendString_Get(STR_CANCEL), 12, 0xffff, 21, "buttonsound");
		if (Keyboard_DequeueChar() == 27) {
			Keyboard_DiscardChar();
			buttonPressed = 1;
		}
		if (buttonPressed) {
			g_filmNamePromptName[0] = '\0';
			FrontendMouse_ClearClicks();
			g_activeTextFieldId = 0;
			FrontendDraw_ForceFullScreenPresent();
			Keyboard_FlushCharBuffer();
			FrontendScreen_PopState();
			FrontendText_PopGlyphGradientBg();
			FrontendMouse_ClearInputGate();
			FrontendText_ResetGlyphScratch();
			FrontendScrollbar_RestoreState();
			return 1;
		}
	} else {
		FrontendDraw_RectAssign(&rect, 102, 206, 538, 226);
		FrontendText_DrawCentered(15, FrontendString_Get(STR_FILM_DUPLICATE1), &rect, 0xffff);
		FrontendDraw_RectOffsetXY(&rect, 0, 20);
		FrontendText_DrawCentered(15, FrontendString_Get(STR_FILM_DUPLICATE2), &rect, 0xffff);
		FrontendDraw_RectOffsetXY(&rect, 0, 20);
		FrontendText_DrawCentered(15, FrontendString_Get(STR_FILM_DUPLICATE3), &rect, 0xffff);

		FrontImage_GetResourceRect("yesbutton", &rect);
		FrontendDraw_RectOffsetXY(&rect, 62, 236 - ((rect.bottom - rect.top + 1) >> 1));
		buttonPressed = FrontendButton_DrawSpriteHitTest(
			&rect, "yesbutton", "yesbutton", FrontendString_Get(STR_YES), 12, 0xffff, 20, "buttonsound");
		if (Keyboard_PeekChar() == '\r') {
			Keyboard_DequeueChar();
			buttonPressed = 1;
		}
		if (buttonPressed) {
			FrontendMouse_ClearClicks();
			g_activeTextFieldId = 0;
			FrontendDraw_ForceFullScreenPresent();
			Keyboard_FlushCharBuffer();
			FrontendScreen_PopState();
			FrontendText_PopGlyphGradientBg();
			FrontendMouse_ClearInputGate();
			FrontendText_ResetGlyphScratch();
			FrontendScrollbar_RestoreState();
			return 1;
		}

		FrontImage_GetResourceRect("nobutton", &rect);
		FrontendDraw_RectOffsetXY(&rect, 528, 236 - ((rect.bottom - rect.top + 1) >> 1));
		buttonPressed = FrontendButton_DrawSpriteHitTest(
			&rect, "nobutton", "nobutton", FrontendString_Get(STR_NO), 12, 0xffff, 21, "buttonsound");
		if (Keyboard_DequeueChar() == 27) {
			Keyboard_DiscardChar();
			buttonPressed = 1;
		}
		if (buttonPressed) {
			g_filmNamePromptOverwriteConfirm = 0;
			FrontendCursor_SetPos(90, 236);
			FrontendMouse_ClearClicks();
		}
	}

	return 0;
}

// FUNCTION: XWA 0x541760
char* FilmNamePrompt_Run(void) {
	FrontendScreenModalStatus modalStatus;
	FrontendRect screenRect;

	if (g_filmNamePromptRunActive) {
		modalStatus = FrontendScreen_GetModalStatus();
		if (modalStatus != FRONTEND_SCREEN_MODAL_INACTIVE && modalStatus != FRONTEND_SCREEN_MODAL_DONE) {
			return 0;
		}

		FrontendCursor_Hide();
		FrontendDisplay_UnlockBackBuffer();
		FrontendDisplay_ClearBackBuffer();
		FrontendDisplay_ClearOffscreenSurface();
		FrontendDisplay_PresentFrame();
		FrontendDisplay_ClearBackBuffer();
		FrontendCursor_FreeResources();
		FrontendSound_UnloadList("sfx\\sfx.lst");
		g_filmNamePromptRunActive = 0;
		return g_filmNamePromptName;
	}

	Keyboard_FlushCharBuffer();
	FrontendCursor_LoadResources();
	FrontImage_RebuildPaletteCache();
	FrontendColor_Init();
	FrontendText_SetGlyphGradientBg(g_colorNearBlack);
	FrontendSound_LoadList("sfx\\sfx.lst");
	FrontendCursor_Show();
	FrontendDraw_RectAssign(&screenRect, 0, 0, 639, 479);
	g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	if (!FrontendScreen_BeginModal(FilmNamePrompt_Update, &screenRect)) {
		FrontendCursor_Hide();
		FrontendDisplay_UnlockBackBuffer();
		FrontendCursor_FreeResources();
		FrontendSound_UnloadList("sfx\\sfx.lst");
		return 0;
	}

	g_filmNamePromptRunActive = 1;
	return 0;
}

// FUNCTION: XWA 0x4388D0
int Film_ReadBytes(void* dst, int size) {
	if (g_filmFile == 0) {
		g_filmPlaybackMode = 3;
		return 0;
	}

	{
		uint8_t* cursor;
		int remaining = size;

		if (remaining > 0) {
			cursor = dst;
			while (remaining > 0) {
				size_t readCount;

				if (remaining <= 0x2000) {
#ifdef XWA_MODERN
					readCount = File_ReadCount(g_filmFile, cursor, (size_t)remaining);
#else
					readCount = fread(cursor, (size_t)remaining, 1u, (FILE*)g_filmFile);
#endif
					cursor += remaining;
					remaining = 0;
				} else {
#ifdef XWA_MODERN
					readCount = File_ReadCount(g_filmFile, cursor, 0x2000u);
#else
					readCount = fread(cursor, 0x2000u, 1u, (FILE*)g_filmFile);
#endif
					cursor += 0x2000;
					remaining -= 0x2000;
				}
				if (readCount != 1) {
					g_filmPlaybackMode = 3;
					break;
				}
			}
		}
	}

	return g_filmPlaybackMode != 3;
}

// FUNCTION: XWA 0x438960
int Film_SeekPastHeaderAndMissionName(void) {
	if (g_filmFile == 0) {
		g_filmPlaybackMode = 3;
		return 0;
	}

#ifdef XWA_MODERN
	File_Seek(g_filmFile, 4, SEEK_SET);
	File_Seek(g_filmFile, 128, SEEK_CUR);
#else
	fseek((FILE*)g_filmFile, 4, SEEK_SET);
	fseek((FILE*)g_filmFile, 128, SEEK_CUR);
#endif
	return 1;
}

// FUNCTION: XWA 0x4389A0
int Film_SeekToEmbeddedMissionData(void) {
	if (g_filmFile == 0) {
		g_filmPlaybackMode = 3;
		return 0;
	}

#ifdef XWA_MODERN
	File_Seek(g_filmFile, 4, SEEK_SET);
	File_Seek(g_filmFile, 128, SEEK_CUR);
	File_Seek(g_filmFile, (int)sizeof(PilotData), SEEK_CUR);
#else
	fseek((FILE*)g_filmFile, 4, SEEK_SET);
	fseek((FILE*)g_filmFile, 128, SEEK_CUR);
	fseek((FILE*)g_filmFile, (int)sizeof(PilotData), SEEK_CUR);
#endif
	if ((uint16_t)g_filmVersion > 4u) {
#ifdef XWA_MODERN
		File_Seek(g_filmFile, 1, SEEK_CUR);
		File_Seek(g_filmFile, 1, SEEK_CUR);
#else
		fseek((FILE*)g_filmFile, 1, SEEK_CUR);
		fseek((FILE*)g_filmFile, 1, SEEK_CUR);
#endif
	}
#ifdef XWA_MODERN
	File_Seek(g_filmFile, 4, SEEK_CUR);
#else
	fseek((FILE*)g_filmFile, 4, SEEK_CUR);
#endif
	return 1;
}

// FUNCTION: XWA 0x438A40
int Film_SeekPastEmbeddedMissionData(void) {
	int embeddedMissionSize;

	if (g_filmFile == 0) {
		g_filmPlaybackMode = 3;
		return 0;
	}

#ifdef XWA_MODERN
	File_Seek(g_filmFile, 4, SEEK_SET);
	File_Seek(g_filmFile, 128, SEEK_CUR);
	File_Seek(g_filmFile, (int)sizeof(PilotData), SEEK_CUR);
#else
	fseek((FILE*)g_filmFile, 4, SEEK_SET);
	fseek((FILE*)g_filmFile, 128, SEEK_CUR);
	fseek((FILE*)g_filmFile, (int)sizeof(PilotData), SEEK_CUR);
#endif
	if ((uint16_t)g_filmVersion > 4u) {
#ifdef XWA_MODERN
		File_Seek(g_filmFile, 1, SEEK_CUR);
		File_Seek(g_filmFile, 1, SEEK_CUR);
#else
		fseek((FILE*)g_filmFile, 1, SEEK_CUR);
		fseek((FILE*)g_filmFile, 1, SEEK_CUR);
#endif
	}
	FeDiskIo_ReadWithRetryPrompt(&embeddedMissionSize, 4u, 1u, g_filmFile);
#ifdef XWA_MODERN
	File_Seek(g_filmFile, embeddedMissionSize, SEEK_CUR);
#else
	fseek((FILE*)g_filmFile, embeddedMissionSize, SEEK_CUR);
#endif
	return 1;
}

static int Film_CloseWriteFileOnError(void) {
	File_Close(g_filmFile);
	g_filmFile = 0;
	g_filmRecording = 0;
	return 0;
}

static int Film_WriteRawBytes(const void* src, size_t size) {
	if (size == 0 || File_WriteCount(g_filmFile, src, size)) {
		return 1;
	}

	return Film_CloseWriteFileOnError();
}

// FUNCTION: XWA 0x438740
int Film_WriteBytesBuffered(const void* src, size_t size) {
	size_t bufferSize;

	if (g_filmFile == 0) {
		return 0;
	}
	if (g_filmWriteBuffer == 0) {
		return 0;
	}

	bufferSize = (size_t)g_filmWriteBufferSize;
	if (size <= bufferSize) {
		size_t bufferedBytes;

		bufferedBytes = (size_t)g_filmWriteBufferedBytes;
		if (size + bufferedBytes > bufferSize) {
			if (Film_WriteRawBytes(g_filmWriteBuffer, bufferedBytes)) {
				g_filmWriteBufferedBytes = 0;
				bufferedBytes = 0;
			}
			if (g_filmRecording != 2) {
				return 0;
			}
		}

		memcpy(g_filmWriteBuffer + bufferedBytes, src, size);
		g_filmWriteBufferedBytes += (int)size;
		return 1;
	}

	if (Film_WriteRawBytes(g_filmWriteBuffer, (size_t)g_filmWriteBufferedBytes)) {
		g_filmWriteBufferedBytes = 0;
	}
	if (g_filmRecording != 2) {
		return 0;
	}

	while (size != 0) {
		size_t chunkSize;

		chunkSize = size >= 0x2000u ? 0x2000u : size;
		if (!Film_WriteRawBytes(src, chunkSize)) {
			return 0;
		}
		src = (const uint8_t*)src + chunkSize;
		size -= chunkSize;
	}

	return 1;
}

// FUNCTION: XWA 0x4386D0
int Film_FlushWriteBuffer(void) {
	if (g_filmFile == 0) {
		return 0;
	}
	if (g_filmWriteBuffer == 0) {
		return 0;
	}

	/* fwrite(buf, 0, 1, file) fails, unlike File_WriteCount's zero-byte case. */
#ifdef XWA_MODERN
	if (g_filmWriteBufferedBytes == 0 ||
		!File_WriteCount(g_filmFile, g_filmWriteBuffer, (size_t)g_filmWriteBufferedBytes)) {
		File_Close(g_filmFile);
#else
	if (fwrite(g_filmWriteBuffer, (size_t)g_filmWriteBufferedBytes, 1, (FILE*)g_filmFile) != 1) {
		fclose((FILE*)g_filmFile);
#endif
		g_filmFile = 0;
		g_filmRecording = 0;
		return 0;
	}

	g_filmWriteBufferedBytes = 0;
	return 1;
}
