#include "xwa/frontend/frontend_color.h"

#include "xwa/frontend/frontend_display.h"

// GLOBAL: XWA 0xABD1E0
int g_colorMutedGreen;
// GLOBAL: XWA 0xABD1E4
int g_colorNearBlack;
// GLOBAL: XWA 0xABD200
int g_hudPreviewColors[5];
// GLOBAL: XWA 0xABD218
int g_colorGray;
// GLOBAL: XWA 0xABD220
int g_colorOrangeRed;
// GLOBAL: XWA 0xABD224
int g_colorLightBlue;
// GLOBAL: XWA 0xABD228
int g_colorPaleBlue;
// GLOBAL: XWA 0xABD234
int g_colorSlateBlue;
// GLOBAL: XWA 0xABD240
int g_colorRed2;
// GLOBAL: XWA 0xABD244
int g_colorNavy2;
// GLOBAL: XWA 0xABD248
int g_colorBlue2;
// GLOBAL: XWA 0xABD24C
int g_colorYellow2;
// GLOBAL: XWA 0xABD250
int g_colorViolet;
// GLOBAL: XWA 0xABD254
int g_colorSpringGreen;
// GLOBAL: XWA 0xABD258
int g_colorMutedGreen2;
// GLOBAL: XWA 0xABD25C
int g_colorCyan;
// GLOBAL: XWA 0xABD260
int g_colorAzure;
// GLOBAL: XWA 0xABD264
int g_colorOrange;
// GLOBAL: XWA 0xABD268
int g_colorDimGreenBlend;
// GLOBAL: XWA 0xABD780
int g_colorDarkGray;
// GLOBAL: XWA 0xABD784
int g_colorPaleCyan;
// GLOBAL: XWA 0xABD7BC
int g_colorRed;
// GLOBAL: XWA 0xABD7D0
int g_colorMidGray;
// GLOBAL: XWA 0xABD7D8
int g_colorTeal;
// GLOBAL: XWA 0xAE2A30
int g_colorGreen;
// GLOBAL: XWA 0xAE2A34
int g_colorBlue;
// GLOBAL: XWA 0xAE2A48
int g_colorYellow;
// GLOBAL: XWA 0xAE2A54
int g_colorGreen2;
// GLOBAL: XWA 0xB07C6C
int g_colorNavy;

// GLOBAL: XWA 0xABD7A0
int g_indexedColors[5];
// GLOBAL: XWA 0xAE2A00
int g_pulseColorRamp[12];
// GLOBAL: XWA 0x9F4A20
int g_textShadeRamps[5][8];

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x528140
int FrontendColor_GetIndexed(int index) { return g_indexedColors[index]; }

// FUNCTION: XWA 0x52A250
int FrontendColor_Init(void) {
	g_colorGreen2 = FrontendDisplay_PackRGB(0, 0xff, 0);
	g_colorNavy = FrontendDisplay_PackRGB(0, 0, 0x80);
	g_colorMutedGreen = FrontendDisplay_PackRGB(0x40, 0x80, 0x40);
	g_colorGreen = FrontendDisplay_PackRGB(0, 0xff, 0);
	g_colorRed = FrontendDisplay_PackRGB(0xff, 0, 0);
	g_colorBlue = FrontendDisplay_PackRGB(0, 0, 0xff);
	g_colorYellow = FrontendDisplay_PackRGB(0xff, 0xff, 0);
	g_colorGray = FrontendDisplay_PackRGB(0x60, 0x60, 0x60);
	g_colorMidGray = FrontendDisplay_PackRGB(0x90, 0x90, 0x90);
	g_colorDarkGray = FrontendDisplay_PackRGB(0x40, 0x40, 0x40);
	g_colorPaleCyan = FrontendDisplay_PackRGB(0xc4, 0xfc, 0xfc);
	g_colorTeal = FrontendDisplay_PackRGB(0x30, 0x6f, 0x7b);
	g_colorNearBlack = FrontendDisplay_PackRGB(0x1e, 0x1e, 0x1e);

	g_colorDimGreenBlend = g_colorMutedGreen;
	if (FrontendDisplay_GetPixelFormat555()) {
		g_colorDimGreenBlend = (unsigned short)((((unsigned int)g_colorDimGreenBlend >> 1) & 0x3def) +
												(((unsigned int)g_colorNearBlack >> 1) & 0x3def));
	} else {
		g_colorDimGreenBlend = (unsigned short)((((unsigned int)g_colorNearBlack >> 1) & 0x7bef) +
												(((unsigned int)g_colorDimGreenBlend >> 1) & 0x7bef));
	}

	g_indexedColors[0] = 0xff7020c0u;
	g_hudPreviewColors[0] = FrontendDisplay_PackRGB(0x70, 0x20, 0xc0);
	g_indexedColors[1] = 0xffb97a00u;
	g_hudPreviewColors[1] = FrontendDisplay_PackRGB(0xb9, 0x7a, 0);
	g_indexedColors[2] = 0xff2a3891u;
	g_hudPreviewColors[2] = FrontendDisplay_PackRGB(0x2a, 0x38, 0x91);
	g_indexedColors[3] = 0xff401408u;
	g_hudPreviewColors[3] = FrontendDisplay_PackRGB(0x40, 0x14, 8);
	g_indexedColors[4] = 0xff004040u;
	g_hudPreviewColors[4] = FrontendDisplay_PackRGB(0, 0x40, 0x40);

	g_colorSlateBlue = FrontendDisplay_PackRGB(0x30, 0x40, 0x80);
	g_colorLightBlue = FrontendDisplay_PackRGB(0x60, 0x80, 0xff);
	g_colorPaleBlue = FrontendDisplay_PackRGB(0x9c, 0xc0, 0xff);
	g_colorOrangeRed = FrontendDisplay_PackRGB(0xce, 0x39, 0x10);
	g_colorRed2 = g_colorRed;
	g_colorNavy2 = g_colorNavy;
	g_colorBlue2 = g_colorBlue;
	g_colorYellow2 = g_colorYellow;
	g_colorViolet = FrontendDisplay_PackRGB(0x80, 0, 0xff);
	g_colorSpringGreen = FrontendDisplay_PackRGB(0, 0xff, 0x80);
	g_colorMutedGreen2 = g_colorMutedGreen;
	g_colorCyan = FrontendDisplay_PackRGB(0, 0xff, 0xff);
	g_colorAzure = FrontendDisplay_PackRGB(0, 0x80, 0xff);
	g_colorOrange = FrontendDisplay_PackRGB(0xff, 0x80, 0);

	g_pulseColorRamp[0] = FrontendDisplay_PackRGB(0x80, 0x80, 0);
	g_pulseColorRamp[1] = FrontendDisplay_PackRGB(0x95, 0x95, 0);
	g_pulseColorRamp[2] = FrontendDisplay_PackRGB(0xaa, 0xaa, 0);
	g_pulseColorRamp[3] = FrontendDisplay_PackRGB(0xc0, 0xc0, 0);
	g_pulseColorRamp[4] = FrontendDisplay_PackRGB(0xd5, 0xd5, 0);
	g_pulseColorRamp[5] = FrontendDisplay_PackRGB(0xea, 0xea, 0);
	g_pulseColorRamp[6] = FrontendDisplay_PackRGB(0xff, 0xff, 0);
	g_pulseColorRamp[7] = FrontendDisplay_PackRGB(0xea, 0xea, 0);
	g_pulseColorRamp[8] = FrontendDisplay_PackRGB(0xd5, 0xd5, 0);
	g_pulseColorRamp[9] = FrontendDisplay_PackRGB(0xc0, 0xc0, 0);
	g_pulseColorRamp[10] = FrontendDisplay_PackRGB(0xaa, 0xaa, 0);
	g_pulseColorRamp[11] = FrontendDisplay_PackRGB(0x95, 0x95, 0);

	g_textShadeRamps[0][0] = FrontendDisplay_PackRGB(0, 0x48, 0);
	g_textShadeRamps[0][1] = FrontendDisplay_PackRGB(0, 0x60, 0);
	g_textShadeRamps[0][2] = FrontendDisplay_PackRGB(0, 0x78, 0);
	g_textShadeRamps[0][3] = FrontendDisplay_PackRGB(0, 0x94, 0);
	g_textShadeRamps[0][4] = FrontendDisplay_PackRGB(0, 0xac, 0);
	g_textShadeRamps[0][5] = FrontendDisplay_PackRGB(0, 0xc8, 0);
	g_textShadeRamps[0][6] = FrontendDisplay_PackRGB(0, 0xe0, 0);
	g_textShadeRamps[0][7] = FrontendDisplay_PackRGB(0, 0xfc, 0);

	g_textShadeRamps[1][0] = FrontendDisplay_PackRGB(0x48, 0, 0);
	g_textShadeRamps[1][1] = FrontendDisplay_PackRGB(0x60, 0, 0);
	g_textShadeRamps[1][2] = FrontendDisplay_PackRGB(0x78, 0, 0);
	g_textShadeRamps[1][3] = FrontendDisplay_PackRGB(0x94, 0, 0);
	g_textShadeRamps[1][4] = FrontendDisplay_PackRGB(0xac, 0, 0);
	g_textShadeRamps[1][5] = FrontendDisplay_PackRGB(0xc8, 0, 0);
	g_textShadeRamps[1][6] = FrontendDisplay_PackRGB(0xe0, 0, 0);
	g_textShadeRamps[1][7] = FrontendDisplay_PackRGB(0xfc, 0, 0);

	g_textShadeRamps[2][0] = FrontendDisplay_PackRGB(0x48, 0x48, 0);
	g_textShadeRamps[2][1] = FrontendDisplay_PackRGB(0x60, 0x60, 0);
	g_textShadeRamps[2][2] = FrontendDisplay_PackRGB(0x78, 0x78, 0);
	g_textShadeRamps[2][3] = FrontendDisplay_PackRGB(0x94, 0x94, 0);
	g_textShadeRamps[2][4] = FrontendDisplay_PackRGB(0xac, 0xac, 0);
	g_textShadeRamps[2][5] = FrontendDisplay_PackRGB(0xc8, 0xc8, 0);
	g_textShadeRamps[2][6] = FrontendDisplay_PackRGB(0xe0, 0xe0, 0);
	g_textShadeRamps[2][7] = FrontendDisplay_PackRGB(0xfc, 0xfc, 0);

	g_textShadeRamps[3][0] = FrontendDisplay_PackRGB(0x10, 0x10, 0x48);
	g_textShadeRamps[3][1] = FrontendDisplay_PackRGB(0x20, 0x20, 0x60);
	g_textShadeRamps[3][2] = FrontendDisplay_PackRGB(0x20, 0x20, 0x78);
	g_textShadeRamps[3][3] = FrontendDisplay_PackRGB(0x30, 0x30, 0x94);
	g_textShadeRamps[3][4] = FrontendDisplay_PackRGB(0x40, 0x40, 0xac);
	g_textShadeRamps[3][5] = FrontendDisplay_PackRGB(0x50, 0x50, 0xc8);
	g_textShadeRamps[3][6] = FrontendDisplay_PackRGB(0x60, 0x60, 0xe0);
	g_textShadeRamps[3][7] = FrontendDisplay_PackRGB(0x60, 0x80, 0xfc);

	g_textShadeRamps[4][0] = FrontendDisplay_PackRGB(0x48, 0, 0x48);
	g_textShadeRamps[4][1] = FrontendDisplay_PackRGB(0x60, 0, 0x60);
	g_textShadeRamps[4][2] = FrontendDisplay_PackRGB(0x78, 0, 0x78);
	g_textShadeRamps[4][3] = FrontendDisplay_PackRGB(0x94, 0, 0x94);
	g_textShadeRamps[4][4] = FrontendDisplay_PackRGB(0xac, 0, 0xac);
	g_textShadeRamps[4][5] = FrontendDisplay_PackRGB(0xc8, 0, 0xc8);
	g_textShadeRamps[4][6] = FrontendDisplay_PackRGB(0xe0, 0, 0xe0);
	g_textShadeRamps[4][7] = FrontendDisplay_PackRGB(0xfc, 0, 0xfc);

	return 1;
}
