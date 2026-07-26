#include "xwa/frontend/frontend_screen.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/util/memory.h"

#include <string.h>

// GLOBAL: XWA 0xA1C091
FrontendScreenState g_screenStates[FRONTEND_SCREEN_MAX_STACK];
// GLOBAL: XWA 0xA1C089
int g_screenStackTop;
// GLOBAL: XWA 0xA1C08D
int g_screenCallbacksDirty;
int g_modalScreenActive;
int g_modalScreenDepth;
FrontendScreenModalStatus g_modalScreenStatus = FRONTEND_SCREEN_MODAL_INACTIVE;
// GLOBAL: XWA 0xA1C075
FrontendScreenUpdateFn g_pendingScreenUpdateFn;
// GLOBAL: XWA 0xA1C079
FrontendRect g_pendingScreenRect;
static int g_frontendScreenRunFrameActive;
static FrontendScreenModalCleanupFn g_modalScreenCleanupFns[FRONTEND_SCREEN_MAX_STACK];

// FUNCTION: XWA 0x541810
void FrontendScreen_SetCallbacks(FrontendScreenUpdateFn updateFn, FrontendScreenExitFn exitFn) {
	FrontendScreenState* state;

	state = &g_screenStates[g_screenStackTop];
	state->updateFn = updateFn;
	state = &g_screenStates[g_screenStackTop];
	state->exitFn = exitFn;
	g_frameCounter = -1;
	g_screenCallbacksDirty = 1;
}

// FUNCTION: XWA 0x541860
int FrontendScreen_QueuePush(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect) {
	g_pendingScreenUpdateFn = updateFn;
	FrontendDraw_RectCopy(&g_pendingScreenRect, screenRect);
	return 1;
}

/* State-machine replacement for the setup portion of original
   FrontendScreen_RunModal @ 0x541890. */
int FrontendScreen_BeginModalWithCleanup(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect,
										 FrontendScreenModalCleanupFn cleanupFn) {
	if (g_modalScreenDepth >= FRONTEND_SCREEN_MAX_STACK) {
		return 0;
	}

	FrontendDisplay_UnlockBackBuffer();
	if (!FrontendScreen_PushState(updateFn, screenRect)) {
		g_modalScreenStatus = FRONTEND_SCREEN_MODAL_FAILED;
		return 0;
	}

	g_frameCounter = g_frontendScreenRunFrameActive ? -1 : 0;
	memset(g_joystickState.buttons.released, 0, sizeof(g_joystickState.buttons.released));
	if (g_glyphScratchTtl) {
		--g_glyphScratchTtl;
	}
	FrontendMouse_ClearClicks();
	g_modalScreenCleanupFns[g_modalScreenDepth] = cleanupFn;
	++g_modalScreenDepth;
	g_modalScreenActive = 1;
	g_modalScreenStatus = FRONTEND_SCREEN_MODAL_RUNNING;
	return 1;
}

int FrontendScreen_BeginModal(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect) {
	return FrontendScreen_BeginModalWithCleanup(updateFn, screenRect, 0);
}

/* State-machine replacement for the per-frame FrontendDisplay_RunFrame @ 0x53FD00
   calls inside original FrontendScreen_RunModal @ 0x541890. */
FrontendScreenModalStatus FrontendScreen_TickModal(void) {
	int result;

	if (!g_modalScreenActive) {
		return FRONTEND_SCREEN_MODAL_INACTIVE;
	}

	if (g_modalScreenStatus != FRONTEND_SCREEN_MODAL_RUNNING) {
		return g_modalScreenStatus;
	}

	result = FrontendScreen_RunFrame();
	if (result == 1) {
		g_modalScreenStatus = FRONTEND_SCREEN_MODAL_DONE;
	} else if (result == 2) {
		g_modalScreenStatus = FRONTEND_SCREEN_MODAL_QUIT;
	}

	return g_modalScreenStatus;
}

/* State-machine replacement for the cleanup portion of original
   FrontendScreen_RunModal @ 0x541890. */
void FrontendScreen_EndModal(void) {
	FrontendScreenModalCleanupFn cleanupFn;

	if (!g_modalScreenActive) {
		return;
	}

	cleanupFn = 0;
	if (g_modalScreenDepth > 0) {
		--g_modalScreenDepth;
		cleanupFn = g_modalScreenCleanupFns[g_modalScreenDepth];
		g_modalScreenCleanupFns[g_modalScreenDepth] = 0;
	}

	Keyboard_FlushCharBuffer();
	FrontendScreen_PopState();
	g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	FrontendMouse_ClearClicks();
	if (cleanupFn) {
		cleanupFn();
	}

	if (g_modalScreenDepth > 0) {
		g_modalScreenActive = 1;
		g_modalScreenStatus = FRONTEND_SCREEN_MODAL_RUNNING;
	} else {
		g_modalScreenActive = 0;
		g_modalScreenStatus = FRONTEND_SCREEN_MODAL_INACTIVE;
	}
}

int FrontendScreen_IsModalActive(void) { return g_modalScreenDepth > 0; }

FrontendScreenModalStatus FrontendScreen_GetModalStatus(void) { return g_modalScreenStatus; }

// FUNCTION: XWA 0x541930
int FrontendScreen_PushState(FrontendScreenUpdateFn updateFn, FrontendRect* screenRect) {
	FrontendRect rect;
	int slot;
	int width;
	int height;
	int wasBackBufferLocked;
	unsigned char* pixels;
	int x;
	int y;

	if (g_screenStackTop >= FRONTEND_SCREEN_MAX_STACK - 1) {
		return 0;
	}

	slot = g_screenStackTop;
	g_screenStates[slot].savedFrameCounter = g_frameCounter;
	g_screenStates[slot].savedClipRect.left = g_clipMinX;
	g_screenStates[slot].savedClipRect.right = g_clipMaxX;
	g_screenStates[slot].savedClipRect.top = g_clipMinY;
	g_screenStates[slot].savedClipRect.bottom = g_clipMaxY;

	if (screenRect != 0) {
		if (screenRect->left < 0) {
			screenRect->left = 0;
		}
		if (screenRect->top < 0) {
			screenRect->top = 0;
		}
		if (screenRect->right >= 640) {
			screenRect->right = 639;
		}
		if (screenRect->bottom >= 480) {
			screenRect->bottom = 479;
		}
		if (screenRect->right < screenRect->left || screenRect->bottom < screenRect->top) {
			return 0;
		}

		FrontendDraw_RectCopy(&g_screenStates[slot].savedRect, screenRect);
		width = screenRect->right - screenRect->left + 1;
		height = screenRect->bottom - screenRect->top + 1;
		if (g_displayBpp != 8) {
			if (g_displayBpp == 16) {
				unsigned short* dst;
				unsigned short* src;

				pixels = (unsigned char*)Mem_Alloc(2 * width * height);
				if (pixels == 0) {
					return 0;
				}

				wasBackBufferLocked = g_backBufferLocked.word & 0xff;
				if (g_offscreenRestoreEnabled) {
					FrontendDisplay_LockOffscreenSurface();
				} else {
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				}

				dst = (unsigned short*)pixels;
				src = (unsigned short*)(g_drawSurfacePtr + 2 * screenRect->left +
										screenRect->top * g_drawSurfacePitch);
				for (y = 0; y < height; ++y) {
					for (x = 0; x < width; ++x) {
						dst[x] = src[x];
					}
					src += g_drawSurfacePitch >> 1;
					dst += width;
				}

				if (g_offscreenRestoreEnabled) {
					FrontendDisplay_UnlockOffscreenSurface(1);
				}
				if (!wasBackBufferLocked) {
					FrontendDisplay_UnlockBackBuffer();
				}
			}
		} else {
			unsigned char* dst;
			unsigned char* src;

			pixels = (unsigned char*)Mem_Alloc(width * height);
			if (pixels == 0) {
				return 0;
			}

			wasBackBufferLocked = g_backBufferLocked.word & 0xff;
			if (g_offscreenRestoreEnabled) {
				FrontendDisplay_LockOffscreenSurface();
			} else {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}

			dst = pixels;
			src = g_drawSurfacePtr + screenRect->left + screenRect->top * g_drawSurfacePitch;
			for (y = 0; y < height; ++y) {
				memcpy(dst, src, width);
				src += g_drawSurfacePitch;
				dst += width;
			}

			if (g_offscreenRestoreEnabled) {
				FrontendDisplay_UnlockOffscreenSurface(1);
			}
			if (!wasBackBufferLocked) {
				FrontendDisplay_UnlockBackBuffer();
			}
		}

#ifdef XWA_MODERN
		/* Remaster: the engine saved this region of the persistent
		 * surface before the new screen paints over it. */
		XwaSnapshot_EmitSurfaceEvent(XWA_SURFACE_EVENT_SCREEN_PUSH_SAVE, screenRect->left, screenRect->top,
									 screenRect->right, screenRect->bottom);
#endif
		g_screenStates[slot].savedImage.width = width;
		g_screenStates[slot].savedImage.height = height;
		g_screenStates[slot].savedImage.pixels = pixels;
		if (g_displayBpp != 8) {
			if (g_displayBpp == 16) {
				g_screenStates[slot].savedImage.pixelCount = 2 * width * height;
				g_screenStates[slot].savedImage.isCompressed = 0;
			}
		} else {
			g_screenStates[slot].savedImage.pixelCount = width * height;
			g_screenStates[slot].savedImage.isCompressed = 0;
			FrontImage_CompressRLE(&g_screenStates[slot].savedImage);
		}

		if (g_offscreenRestoreEnabled) {
			FrontendDisplay_SaveBackBuffer();
		}
		FrontendDisplay_SetScreenClipRect640x480(screenRect);
	} else {
		g_screenStates[slot].savedImage.width = 0;
		g_screenStates[slot].savedImage.height = 0;
		g_screenStates[slot].savedImage.pixels = 0;
		g_screenStates[slot].savedImage.pixelCount = 0;
		FrontendDraw_RectAssign(&rect, 0, 0, 639, 479);
		FrontendDisplay_SetScreenClipRect640x480(&rect);
	}

	++g_screenStackTop;
	g_screenStates[g_screenStackTop].updateFn = updateFn;
	g_frameCounter = -1;
	return 1;
}

// FUNCTION: XWA 0x541CB0
void FrontendScreen_PopState(void) {
	FrontendScreenState* states = g_screenStates;
	int newStackTop = g_screenStackTop;
	FrontendRect fullRect;

	if (newStackTop == 0) {
		return;
	}

	--newStackTop;
	FrontendDraw_RectAssign(&fullRect, 0, 0, 640, 480);
	FrontendDisplay_SetScreenClipRect640x480(&fullRect);

	if (states[newStackTop].savedImage.pixelCount > 0) {
#ifdef XWA_MODERN
		/* Remaster: both bpp branches below restore the saved region
		 * of the persistent surface (raw pixel copies / BlitOpaque —
		 * unhookable as draws). One event covers the pop. */
		XwaSnapshot_EmitSurfaceEvent(XWA_SURFACE_EVENT_SCREEN_POP_RESTORE, states[newStackTop].savedRect.left,
									 states[newStackTop].savedRect.top, states[newStackTop].savedRect.right,
									 states[newStackTop].savedRect.bottom);
#endif
		if (g_displayBpp != 8) {
			if (g_displayBpp == 16) {
				int x;
				int width;
				int height;
				int rows;
				int wasBackBufferLocked;
				unsigned short* source;
				unsigned char* destRow;

				wasBackBufferLocked = g_backBufferLocked.word & 0xff;
				if (g_offscreenRestoreEnabled) {
					FrontendDisplay_LockOffscreenSurface();
				} else {
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				}

				height = g_screenStates[newStackTop].savedImage.height;
				width = g_screenStates[newStackTop].savedImage.width;
				source = (unsigned short*)g_screenStates[newStackTop].savedImage.pixels;
				destRow = g_drawSurfacePtr + g_screenStates[newStackTop].savedRect.top * g_drawSurfacePitch +
						  g_screenStates[newStackTop].savedRect.left * 2;
				if (height > 0) {
					rows = height;
					do {
						if (width > 0) {
							unsigned short* dst = (unsigned short*)destRow;
							const unsigned short* src = source;

							x = width;
							while (x--) {
								*dst++ = *src++;
							}
						}
						destRow += g_drawSurfacePitch;
						source += width;
						--rows;
					} while (rows != 0);
				}

				if (g_offscreenRestoreEnabled) {
					FrontendDisplay_UnlockOffscreenSurface(1);
				}
				if (!wasBackBufferLocked) {
					FrontendDisplay_UnlockBackBuffer();
				}
			}
		} else {
			int wasBackBufferLocked;

			wasBackBufferLocked = g_backBufferLocked.word & 0xff;
			if (g_offscreenRestoreEnabled) {
				FrontendDisplay_LockOffscreenSurface();
			} else {
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			}
			FrontImage_BlitOpaque(&g_screenStates[newStackTop].savedImage,
								  g_screenStates[newStackTop].savedRect.left,
								  g_screenStates[newStackTop].savedRect.top);
			if (g_offscreenRestoreEnabled) {
				FrontendDisplay_UnlockOffscreenSurface(1);
			}
			if (!wasBackBufferLocked) {
				FrontendDisplay_UnlockBackBuffer();
			}
		}

		Mem_Free(states[newStackTop].savedImage.pixels);
		states[newStackTop].savedImage.pixels = 0;
	}

	FrontendDisplay_SetScreenClipRect640x480(&states[newStackTop].savedClipRect);
	g_screenStackTop = newStackTop;
	g_frameCounter = states[newStackTop].savedFrameCounter;
}

/* Reimplements the retained screen update/cursor slice shared by original
   FrontendDisplay_RunMainLoop @ 0x53E760 and FrontendDisplay_RunFrame @ 0x53FD00.
   Aeron/XwaFrontendTask own the original Win32 message pump, frame throttle,
   keyboard-state refresh, presentation, and legacy CD-audio boundary work. */
int FrontendScreen_RunFrame(void) {
	FrontendScreenState* state;
	FrontendScreenExitFn exitFn;
	int result;

	state = &g_screenStates[g_screenStackTop];
	if (state->updateFn == 0) {
		return 0;
	}

	g_cursorLabelText[0] = 0;
	if (g_glyphScratchTtl) {
		memset(g_glyphScratchBuffer, 0, sizeof(g_glyphScratchBuffer));
	}

	g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	if (g_drawSurfacePtr == 0) {
		return 0;
	}

	exitFn = state->exitFn;
	g_frontendScreenRunFrameActive = 1;
	result = state->updateFn(g_frameCounter);
	if (g_screenCallbacksDirty == 1 || result == 1) {
		g_screenCallbacksDirty = 0;
		if (exitFn != 0) {
			exitFn(g_frameCounter);
		}
	}
	g_frontendScreenRunFrameActive = 0;

	FrontendDisplay_UnlockBackBuffer();
	FrontendDisplay_ClearReactivatedFlag();

	if (g_pendingScreenUpdateFn != 0) {
		FrontendScreen_PushState(g_pendingScreenUpdateFn, &g_pendingScreenRect);
		g_pendingScreenUpdateFn = 0;
	}

	if (g_cursorVisible == 1) {
		FrontendCursor_Draw();
	}

	memset(g_joystickState.buttons.released, 0, sizeof(g_joystickState.buttons.released));
	++g_frameCounter;
	if (g_glyphScratchTtl) {
		--g_glyphScratchTtl;
	}
	memset(&g_mouseClickLatch, 0, sizeof(g_mouseClickLatch));
	return result;
}
