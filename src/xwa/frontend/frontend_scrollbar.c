#include "xwa/frontend/frontend_scrollbar.h"

#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_input.h"

#include <string.h>

// GLOBAL: XWA 0x7833D8
int g_scrollableControlCount;
// GLOBAL: XWA 0x7831B0
int g_frontendFirstVisibleLine;
// GLOBAL: XWA 0x7831C8
int g_scrollableControlIds[32];
// GLOBAL: XWA 0x7833DC
int g_scrollableControlCountSaved;
// GLOBAL: XWA 0x783248
int g_scrollableControlIdsSaved[32];
// GLOBAL: XWA 0x7833E0
int g_scrollbarDragStartY;
// GLOBAL: XWA 0x7833E4
int g_scrollbarDragStartIndex;

// FUNCTION: XWA 0x555550
int Frontend_ResetScrollableControls(void) {
	g_scrollableControlCount = 0;
	FrontendMouse_ClearInputGate();
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5554D0
int Frontend_RegisterScrollableControl(int controlId) {
	int count;
	unsigned int index;

	count = g_scrollableControlCount;
	if ((unsigned int)count >= 32u) {
		return 0;
	}

	index = 0;
	while (index < (unsigned int)count) {
		if (g_scrollableControlIds[index] == controlId) {
			return 1;
		}
		++index;
	}

	g_scrollableControlIds[count] = controlId;
	g_scrollableControlCount = count + 1;
	return 1;
}

#ifndef XWA_MODERN
#pragma function(memcpy)
#endif
// FUNCTION: XWA 0x555510
int Frontend_CycleScrollableFocus(void) {
	int firstControlId;

	if (g_scrollableControlCount == 0) {
		return 0;
	}

	firstControlId = g_scrollableControlIds[0];
#ifdef XWA_MODERN
	// DEVIATION: the original uses memcpy here, but the source and destination
	// overlap (dest = src - one entry), so memcpy is undefined behavior on the
	// overlap. memmove is the overlap-safe equivalent for modern builds.
	memmove(g_scrollableControlIds, &g_scrollableControlIds[1], (size_t)(4 * g_scrollableControlCount - 4));
#else
	memcpy(g_scrollableControlIds, &g_scrollableControlIds[1], (size_t)(4 * g_scrollableControlCount - 4));
#endif
	g_scrollableControlIds[g_scrollableControlCount - 1] = firstControlId;
	return 1;
}
#ifndef XWA_MODERN
#pragma intrinsic(memcpy)
#endif

// FUNCTION: XWA 0x555570
int FrontendScrollbar_SaveState(void) {
	memcpy(g_scrollableControlIdsSaved, g_scrollableControlIds, sizeof(g_scrollableControlIdsSaved));
	g_scrollableControlCountSaved = g_scrollableControlCount;
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x5555A0
int FrontendScrollbar_RestoreState(void) {
	FrontendMouse_ClearInputGate();
	memcpy(g_scrollableControlIds, g_scrollableControlIdsSaved, sizeof(g_scrollableControlIds));
	g_scrollableControlCount = g_scrollableControlCountSaved;
	return 1;
}

// FUNCTION: XWA 0x5555D0
int FrontendScrollbar_Draw(FrontendRect* rect, int currentIndex, int itemCount, int minIndex, int pageSize,
						   int color, int controlId) {
	int mouseX;
	int mouseY;
	int width;
	int height;
	int resultIndex;
	int gateId;
	int trackFillY;
	int trackFillHeight;
	int minScrollIndex;
	int buttonBaseIndex;
	int dragIndex;
	int dragThumbTop;
	FrontendRect scrollbarRect;
	FrontendRect thumbRect;
	FrontendRect buttonRect;

	/* The original function takes this argument but draws only fixed scrollbar sprites. */
	(void)color;

	Frontend_RegisterScrollableControl(controlId);
	FrontendCursor_GetPos(&mouseX, &mouseY);
	width = rect->right - rect->left;
	height = rect->bottom - rect->top;
	resultIndex = currentIndex;
	if (width >= height) {
		return resultIndex;
	}

	if (Keyboard_PeekChar() == 9) {
		Keyboard_DiscardChar();
		Frontend_CycleScrollableFocus();
	}

	gateId = controlId + 1000;
	if (!FrontendMouse_IsGateOwner(gateId)) {
		if (FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick()) {
			g_scrollbarDragStartY = 0;
			g_scrollbarDragStartIndex = 0;
		}

		FrontendDraw_RectCopy(&scrollbarRect, rect);
		FrontendDraw_RectAssign(
			&thumbRect, scrollbarRect.left,
			currentIndex * (height - 60) / (itemCount - minIndex - 1) + scrollbarRect.top + 20,
			scrollbarRect.right,
			currentIndex * (height - 60) / (itemCount - minIndex - 1) + scrollbarRect.top + 40);

		scrollbarRect.bottom -= width;
		trackFillHeight = scrollbarRect.bottom - (width + scrollbarRect.top) - 4;
		trackFillY = 0;
		scrollbarRect.top += width;
		if (trackFillHeight > 0) {
			do {
				FrontImage_DrawSprite("scrollbar", scrollbarRect.left, trackFillY + scrollbarRect.top + 2);
				trackFillY += 2;
			} while (trackFillY < trackFillHeight);
		}

		FrontImage_DrawSprite("scrolltop", scrollbarRect.left, scrollbarRect.top - 1);
		FrontImage_DrawSprite("scrollbottom", scrollbarRect.left, trackFillY + scrollbarRect.top + 2);

		if (FrontendDraw_PointInRect(&scrollbarRect, mouseX, mouseY) &&
			(FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick())) {
			if (mouseY < thumbRect.top) {
				minScrollIndex = minIndex;
				resultIndex = currentIndex - pageSize;
				if (currentIndex - pageSize < minIndex) {
					resultIndex = minIndex;
				}
			} else if (mouseY > thumbRect.bottom) {
				resultIndex = pageSize + currentIndex;
				if (pageSize + currentIndex >= itemCount) {
					resultIndex = itemCount - 1;
				}
			}
		}

		minScrollIndex = minIndex;
		if (g_scrollableControlIds[0] == controlId) {
			if (Keyboard_IsKeyDown(0x21u)) {
				resultIndex = currentIndex - pageSize;
				if (currentIndex - pageSize < minScrollIndex) {
					resultIndex = minScrollIndex;
				}
			} else if (Keyboard_IsKeyDown(0x22u)) {
				resultIndex = pageSize + currentIndex;
				if (pageSize + currentIndex >= itemCount) {
					resultIndex = itemCount - 1;
				}
			}
		}

		FrontendDraw_RectAssign(&buttonRect, rect->left, rect->top, rect->right, width + rect->top);
		if (FrontendDraw_PointInRect(&buttonRect, mouseX, mouseY) &&
			(FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown())) {
			if (width >= 18) {
				FrontImage_DrawSprite("slideud", rect->left, rect->top);
			} else {
				FrontImage_DrawSprite("sslideud", rect->left, rect->top);
			}

			if (!g_scrollbarDragStartY) {
				buttonBaseIndex = currentIndex;
				if (currentIndex > minScrollIndex) {
					resultIndex = currentIndex - 1;
				}

				if (g_scrollbarDragStartIndex) {
					g_scrollbarDragStartIndex >>= 2;
					if (!g_scrollbarDragStartIndex) {
						g_scrollbarDragStartIndex = 1;
					}
					g_scrollbarDragStartY = g_scrollbarDragStartIndex;
				} else {
					g_scrollbarDragStartIndex = 12;
					g_scrollbarDragStartY = 12;
				}
			} else {
				--g_scrollbarDragStartY;
				buttonBaseIndex = currentIndex;
			}
		} else {
			if (width >= 18) {
				FrontImage_DrawSprite("slideuu", rect->left, rect->top);
			} else {
				FrontImage_DrawSprite("sslideuu", rect->left, rect->top);
			}
			buttonBaseIndex = currentIndex;
		}

		if (g_scrollableControlIds[0] == controlId && Keyboard_IsKeyDown(0x26u) &&
			buttonBaseIndex > minScrollIndex) {
			resultIndex = buttonBaseIndex - 1;
		}

		FrontendDraw_RectAssign(&buttonRect, rect->left, rect->bottom - width, rect->right, rect->bottom);
		if (FrontendDraw_PointInRect(&buttonRect, mouseX, mouseY)) {
			if (FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown()) {
				if (width >= 18) {
					FrontImage_DrawSprite("slidedd", rect->left, rect->bottom - width);
				} else {
					FrontImage_DrawSprite("sslidedd", rect->left, rect->bottom - width);
				}

				if (g_scrollbarDragStartY) {
					--g_scrollbarDragStartY;
				} else {
					if (buttonBaseIndex < itemCount - 1) {
						resultIndex = buttonBaseIndex + 1;
					}

					if (!g_scrollbarDragStartIndex) {
						g_scrollbarDragStartIndex = 12;
						g_scrollbarDragStartY = 12;
					} else {
						g_scrollbarDragStartIndex >>= 2;
						if (!g_scrollbarDragStartIndex) {
							g_scrollbarDragStartIndex = 1;
							g_scrollbarDragStartY = 1;
						} else {
							g_scrollbarDragStartY = g_scrollbarDragStartIndex;
						}
					}
				}
			} else if (width < 18) {
				FrontImage_DrawSprite("sslidedu", rect->left, rect->bottom - width);
			} else {
				FrontImage_DrawSprite("slidedu", rect->left, rect->bottom - width);
			}
		} else if (width < 18) {
			FrontImage_DrawSprite("sslidedu", rect->left, rect->bottom - width);
		} else {
			FrontImage_DrawSprite("slidedu", rect->left, rect->bottom - width);
		}

		if (g_scrollableControlIds[0] == controlId && Keyboard_IsKeyDown(0x28u) &&
			buttonBaseIndex < itemCount - 1) {
			resultIndex = buttonBaseIndex + 1;
		}

		if (g_scrollableControlIds[0] == controlId) {
			FrontImage_DrawSprite("slidethumb", thumbRect.left, thumbRect.top);
		} else {
			FrontImage_DrawSprite("slidethumboff", thumbRect.left, thumbRect.top);
		}

		if (FrontendMouse_IsGateOpen() && FrontendDraw_PointInRect(&thumbRect, mouseX, mouseY) &&
			(FrontendMouse_GetLeftDown() || FrontendMouse_GetRightDown())) {
			FrontendMouse_SetInputGate(gateId);
		}

		return resultIndex;
	}

	trackFillY = 0;
	g_scrollbarDragStartY = 0;
	g_scrollbarDragStartIndex = 0;
	if (FrontendMouse_GetLeftClickFor(gateId) || FrontendMouse_GetRightClickFor(gateId)) {
		FrontendMouse_ClearInputGate();
		FrontendMouse_ClearClicks();
	}

	FrontendDraw_RectCopy(&scrollbarRect, rect);
	scrollbarRect.bottom -= width;
	trackFillHeight = scrollbarRect.bottom - (width + scrollbarRect.top) - 4;
	scrollbarRect.top += width;
	if (trackFillHeight > 0) {
		do {
			FrontImage_DrawSprite("scrollbar", scrollbarRect.left, trackFillY + scrollbarRect.top + 2);
			trackFillY += 2;
		} while (trackFillY < trackFillHeight);
	}

	FrontImage_DrawSprite("scrolltop", scrollbarRect.left, scrollbarRect.top - 1);
	FrontImage_DrawSprite("scrollbottom", scrollbarRect.left, trackFillY + scrollbarRect.top + 2);

	FrontendDraw_RectAssign(&buttonRect, rect->left, rect->top, rect->right, width + rect->top);
	if (width >= 18) {
		FrontImage_DrawSprite("slideuu", rect->left, rect->top);
	} else {
		FrontImage_DrawSprite("sslideuu", rect->left, rect->top);
	}

	FrontendDraw_RectAssign(&buttonRect, rect->left, rect->bottom - width, rect->right, rect->bottom);
	if (width >= 18) {
		FrontImage_DrawSprite("slidedu", rect->left, rect->bottom - width);
	} else {
		FrontImage_DrawSprite("sslidedu", rect->left, rect->bottom - width);
	}

	dragIndex = (itemCount - minIndex) * (mouseY - width - rect->top) / (height - 60);
	resultIndex = dragIndex;
	if (dragIndex < minIndex) {
		dragIndex = minIndex;
		resultIndex = minIndex;
	}

	if (dragIndex >= itemCount) {
		resultIndex = itemCount - 1;
	}

	if (mouseY < rect->top + 30) {
		dragThumbTop = rect->top + 30;
	} else if (mouseY >= rect->bottom - 30) {
		dragThumbTop = rect->bottom - 31;
	} else {
		dragThumbTop = mouseY;
	}

	FrontendDraw_RectAssign(&buttonRect, rect->left, dragThumbTop, rect->right, dragThumbTop + 20);
	if (g_scrollableControlIds[0] == controlId) {
		FrontImage_DrawSprite("slidethumb", buttonRect.left, buttonRect.top - 10);
	} else {
		FrontImage_DrawSprite("slidethumboff", buttonRect.left, buttonRect.top - 10);
	}

	return resultIndex;
}
