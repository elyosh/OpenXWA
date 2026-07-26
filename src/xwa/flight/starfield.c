#include "xwa/flight/starfield.h"
#include "xwa/flight/fediskio.h"

#include "xwa/assets/model_texture.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"
#include "xwa/util/time.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot.h"
#endif

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x761DE4
StarPaletteCycle** g_starPaletteCycles;
// GLOBAL: XWA 0x7589D8
int16_t g_starAngularBinToIndex[128 * 128];
// GLOBAL: XWA 0x761DD8
int g_starColorStep16;
// GLOBAL: XWA 0x761DDC
uint16_t g_starRedMask16;
// GLOBAL: XWA 0x761DE0
int g_starPaletteLastCycleTime;
// GLOBAL: XWA 0x74E9D8
Vec3i g_starfieldVectors[2560];
// GLOBAL: XWA 0x5FE694
Vec3i* g_starfieldVectorsPtr = g_starfieldVectors;
// GLOBAL: XWA 0x5FE690
Vec3i* g_starfieldAngularVectorsPtr = &g_starfieldVectors[512];
// GLOBAL: XWA 0x756BD8
uint8_t g_starPaletteCycleIds[2560];
// GLOBAL: XWA 0x74D5D8
uint16_t g_starPaletteCycleOffsets[2560];
// GLOBAL: XWA 0x7575D8
uint16_t g_starBrightnessJitter[2560];
// GLOBAL: XWA 0x910E40
HyperspaceStarStreak g_hyperspaceStarStreaks[1024];
// GLOBAL: XWA 0x8BF374
int g_hyperspaceStarStreakCount;
// GLOBAL: XWA 0x68C96C
int g_hyperspaceTunnelFrameQ16;
// GLOBAL: XWA 0x9C6748
int g_unusedHyperspaceTransitionYOffsetNeg;
// GLOBAL: XWA 0x5A9E70
const float g_starfieldZero = 0.0f;
// GLOBAL: XWA 0x5A9E78
const double g_starfieldNegativeTauRadians = -6.283185307179586;
// GLOBAL: XWA 0x5A9E80
const float g_starfieldRadiansToAzimuthBinScale = 20.371832f;
// GLOBAL: XWA 0x5A9E88
const double g_starfieldRadiansToElevationBinScale = 40.74366429772945;
// GLOBAL: XWA 0x5A9E90
const float g_starfieldQ15ToUnitScale = 0.000030517578f;
// GLOBAL: XWA 0x5A9E98
const double g_starfieldTauRadians = 6.283185307179586;
// GLOBAL: XWA 0x5A9EA0
const float g_hyperspaceRandomStreakYOffsetScale = -262144.0f;
// GLOBAL: XWA 0x5A99CC
const float g_hyperspaceTransitionTickScale = 0.0084745763f;
// GLOBAL: XWA 0x5A99D0
const float g_hyperspaceTransitionAlphaScale = 255.0f;
// GLOBAL: XWA 0x5A99D4
const float g_hyperspaceTransitionUnitScale = 1.0f;
// GLOBAL: XWA 0x5A99D8
const float g_hyperspaceInboundLengthGrowth = 5000.0f;
// GLOBAL: XWA 0x5A99DC
const float g_hyperspaceInboundBaseLength = 10000.0f;
// GLOBAL: XWA 0x5A99E0
const float g_hyperspaceTransitionYOffsetScale = -0.1f;
// GLOBAL: XWA 0x5A99E4
const float g_hyperspaceTunnelWidthScale = -2.0f;

// GLOBAL: XWA 0x5B5CD0
Vec3f g_hyperspaceStreakQuadVertices[4] = {
	{ 64.0f, 0.0f, 0.0f },
	{ 64.0f, -256.0f, 0.0f },
	{ -64.0f, -256.0f, 0.0f },
	{ -64.0f, 0.0f, 0.0f },
};

OptTexCoord g_hyperspaceStreakQuadTexCoords[4] = {
	{ 1.0f, 0.0f },
	{ 1.0f, 1.0f },
	{ 0.0f, 1.0f },
	{ 0.0f, 0.0f },
};

Vec3f g_hyperspaceStreakQuadVertNormals[1] = {
	{ 0.0f, 0.0f, 1.0f },
};

typedef struct HyperspaceStreakQuadFaceData {
	int edgeCount;
	FaceRecord faces[1];
	Vec3f faceNormals[1];
	FaceTextureGradients faceTexturing[1];
} HyperspaceStreakQuadFaceData;

HyperspaceStreakQuadFaceData g_hyperspaceStreakQuadFaceData = {
	4,
	{
		{
			{ 0, 1, 2, 3 },
			{ 0, 1, 2, 3 },
			{ 0, 0, 0, 0 },
			{ 1, 2, 3, 0 },
		},
	},
	{ { 0.0f, 0.0f, 1.0f } },
	{ { { 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } } },
};

OptNode g_hyperspaceStreakQuadVertsNode = {
	NULL, OPT_MESHVERTS, 0, NULL, 4, g_hyperspaceStreakQuadVertices,
};
OptNode g_hyperspaceStreakQuadTexCoordsNode = {
	NULL, OPT_TEXCOORDS, 0, NULL, 4, g_hyperspaceStreakQuadTexCoords,
};
OptNode g_hyperspaceStreakQuadVertNormalsNode = {
	NULL, OPT_VERTNORMALS, 0, NULL, 1, g_hyperspaceStreakQuadVertNormals,
};
OptNode g_hyperspaceStreakQuadFaceNode = {
	NULL, OPT_FACEDATA, 0, NULL, 1, &g_hyperspaceStreakQuadFaceData,
};
OptNode* g_hyperspaceStreakQuadRootChildren[4] = {
	&g_hyperspaceStreakQuadVertsNode,
	&g_hyperspaceStreakQuadTexCoordsNode,
	&g_hyperspaceStreakQuadVertNormalsNode,
	&g_hyperspaceStreakQuadFaceNode,
};
OptNode g_hyperspaceStreakQuadRootNode = {
	NULL, OPT_GROUP, 4, g_hyperspaceStreakQuadRootChildren, 4, g_hyperspaceStreakQuadRootChildren,
};
OptNode* g_hyperspaceStreakQuadRootNodes[1] = {
	&g_hyperspaceStreakQuadRootNode,
};
// GLOBAL: XWA 0x5B5E28
OptimizedPolyObject g_hyperspaceModelHeaderPatch = {
	NULL,
	0,
	1,
	g_hyperspaceStreakQuadRootNodes,
};

// FUNCTION: XWA 0x4DD450
void FlightStarfield_Init(void) {
	int16_t colorStep16;
	int angularVectorIndex;
	int directVectorIndex;
	int mappedVectorIndex;
	int16_t starIndex;
	StarPaletteCycle** paletteCycles;
	Vec3i starVec;
	Vec3i jitterVec;
	StarfieldPolarCoord polar;
	int i;

	if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
		return;
	}

	if (Display_IsPixelFormat555()) {
		colorStep16 = 0x421;
		g_starRedMask16 = 0x7c00u;
		g_starColorStep16 = colorStep16;
	} else {
		colorStep16 = 0x841;
		g_starRedMask16 = 0xf800u;
		g_starColorStep16 = colorStep16;
	}
	DebugPrintfChannel(0x8000, "Staraddvalue set to 0x%4x.\n", colorStep16);

	memset(g_starAngularBinToIndex, 0xff, sizeof(g_starAngularBinToIndex));
	paletteCycles = (StarPaletteCycle**)Memory_AllocTagged("STARPALARRAY", 4u * sizeof(*g_starPaletteCycles));
	g_starPaletteCycles = paletteCycles;
	memset(paletteCycles, 0, 4u * sizeof(*paletteCycles));

	starIndex = 0;
	mappedVectorIndex = 0;
	angularVectorIndex = 0;

	FlightStarfield_SetPaletteCycle(0, 39, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
									19, 20, 21, 22, 23, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
	FlightStarfield_SetPaletteCycle(1, 64, 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 7, 8, 10, 12, 14, 16, 17,
									18, 19, 20, 20, 21, 21, 22, 22, 22, 23, 23, 23, 23, 22, 22, 22, 21, 21,
									20, 20, 19, 18, 17, 16, 14, 12, 10, 8, 7, 6, 5, 4, 4, 3, 3, 2, 2, 1, 1, 1,
									0, 0, 0);
	FlightStarfield_SetPaletteCycle(2, 45, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 23, 23, 22, 20, 18, 16,
									14, 12, 10, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 2, 1, 0, 0, 0, 0, 0,
									0, 0, 0);
	FlightStarfield_SetPaletteCycle(3, 57, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
									19, 20, 21, 22, 23, 23, 23, 23, 23, 20, 22, 18, 21, 18, 20, 19, 16, 18,
									16, 17, 16, 15, 13, 14, 12, 12, 11, 10, 8, 5, 2, 0, 0, 0, 0, 0, 0, 0);

	directVectorIndex = 0;
	while (directVectorIndex < 512 || angularVectorIndex < 2048) {
		FlightStarfield_RandomSignedVec3(&starVec);
		FlightStarfield_ComputePolarCoords(starVec, &polar);

		if (polar.xyRadius > abs(polar.z)) {
			if (angularVectorIndex >= 2048) {
				continue;
			}
			if (FlightStarfield_TryReserveAngularBin(polar, (int16_t)(mappedVectorIndex + 512))) {
				starVec.z = 0x7fff * polar.z / polar.xyRadius;
				starVec.x = 0x7fff * starVec.x / polar.xyRadius;
				starVec.y = 0x7fff * starVec.y / polar.xyRadius;
				FlightStarfield_RandomSignedVec3(&jitterVec);
				jitterVec.x >>= 4;
				starVec.x += jitterVec.x;
				jitterVec.y >>= 4;
				starVec.y += jitterVec.y;
				jitterVec.z >>= 4;
				starVec.z += jitterVec.z;
				g_starfieldAngularVectorsPtr[angularVectorIndex] = starVec;
				++mappedVectorIndex;
				++angularVectorIndex;
			}
		} else if (directVectorIndex < 512) {
			if (FlightStarfield_TryReserveAngularBin(polar, starIndex) && polar.z < 0) {
				int positiveZ;

				positiveZ = -polar.z;
				polar.z = positiveZ;
				starVec.z = -starVec.z;
				if (positiveZ) {
					int scaledRadius;

					scaledRadius = 0x7fff * polar.xyRadius / positiveZ;
					starVec.x = scaledRadius * starVec.x / polar.xyRadius;
					starVec.y = scaledRadius * starVec.y / polar.xyRadius;
				} else {
					starVec.x = GameRand2() & 0x3fff;
					starVec.y = GameRand2() & 0x3fff;
				}
				polar.z = 0x7fff;
				starVec.z = 0x7fff;
				FlightStarfield_RandomSignedVec3(&jitterVec);
				jitterVec.x >>= 4;
				starVec.x += jitterVec.x;
				jitterVec.y >>= 4;
				starVec.y += jitterVec.y;
				jitterVec.z >>= 4;
				starVec.z += jitterVec.z;
				g_starfieldVectorsPtr[directVectorIndex] = starVec;
				++starIndex;
				++directVectorIndex;
			}
		}
	}

	for (i = 0; i < 2560; ++i) {
		g_starPaletteCycleIds[i] = (uint8_t)((uint16_t)GameRand2() % 4);
		g_starPaletteCycleOffsets[i] =
			(uint16_t)GameRand2() % g_starPaletteCycles[g_starPaletteCycleIds[i]]->count;
		g_starBrightnessJitter[i] = (uint16_t)(GameRand2() & 7);
	}
	g_starPaletteLastCycleTime = 0;
}

// FUNCTION: XWA 0x4DD970
void FlightStarfield_ComputePolarCoords(Vec3i vec, StarfieldPolarCoord* outPolar) {
	int xyRadius;

	xyRadius = (int)sqrt((double)(vec.y * vec.y + vec.x * vec.x));
	outPolar->xyRadius = xyRadius;
	outPolar->z = vec.z;
	if (xyRadius != 0) {
		float azimuthRad;

		azimuthRad = (float)acos((double)vec.y / (double)xyRadius);
		outPolar->azimuthRad = azimuthRad;
		if (vec.x > 0) {
			outPolar->azimuthRad = -azimuthRad;
		}
	}
}

// FUNCTION: XWA 0x4DD3F0
void FlightStarfield_RandomSignedVec3(Vec3i* outVec) {
	outVec->x = (uint16_t)GameRand2();
	outVec->y = (uint16_t)GameRand2();
	outVec->z = (uint16_t)GameRand2();

	if ((outVec->x & 0x8000) != 0) {
		outVec->x = -(outVec->x & 0x7fff);
	}
	if ((outVec->y & 0x8000) != 0) {
		outVec->y = -(outVec->y & 0x7fff);
	}
	if ((outVec->z & 0x8000) != 0) {
		outVec->z = -(outVec->z & 0x7fff);
	}
}

// FUNCTION: XWA 0x4DDAE0
char FlightStarfield_TryReserveAngularBin(StarfieldPolarCoord polar, int16_t starIndex) {
	int azimuthBin;
	int elevationBin;
	int binIndex;
	int16_t* bin;

	if (polar.azimuthRad < g_starfieldZero) {
		polar.azimuthRad -= g_starfieldNegativeTauRadians;
	}

	azimuthBin = (int)(polar.azimuthRad * g_starfieldRadiansToAzimuthBinScale);
	if (azimuthBin >= 128) {
		azimuthBin = 127;
	}

	if (polar.xyRadius == 0) {
		elevationBin = 127;
	} else {
		elevationBin =
			(int)(atan((double)polar.z / (double)polar.xyRadius) * g_starfieldRadiansToElevationBinScale);
		if (elevationBin < 0) {
			elevationBin = -elevationBin;
		}
		if (elevationBin >= 128) {
			elevationBin = 127;
		}
	}

	binIndex = elevationBin + (azimuthBin << 7);
	bin = &g_starAngularBinToIndex[binIndex];
	if (*bin != -1) {
		return 0;
	}

	*bin = starIndex;
	return 1;
}

// FUNCTION: XWA 0x4DD9D0
void FlightStarfield_SetPaletteCycle(int cycleId, int colorCount, ...) {
	va_list args;
	int i;
	StarPaletteCycle* cycle;

	if (cycleId < 0 || cycleId >= 4) {
		DebugPrintfChannel(0x8000, "Invalid star palette index: %d.\n", cycleId);
		return;
	}
	if (colorCount < 1) {
		DebugPrintfChannel(0x8000, "Invalid number of star palette entries: %d.\n", colorCount);
		return;
	}

	if (g_starPaletteCycles[cycleId] == NULL) {
		StarPaletteCycle* newCycle;

		g_starPaletteCycles[cycleId] =
			(StarPaletteCycle*)Memory_AllocTagged("STARPALETTE", sizeof(*newCycle));
		newCycle = g_starPaletteCycles[cycleId];
		newCycle->count = 0;
		newCycle->colors = NULL;
		newCycle->frame = 0;
	}

	cycle = g_starPaletteCycles[cycleId];
	if (cycle->colors) {
		Memory_FreeTagged("STARPALDATA", cycle->colors);
		cycle->colors = NULL;
	}

	cycle->count = colorCount;
	cycle->frame = 0;
	cycle->colors = (uint16_t*)Memory_AllocTagged("STARPALDATA", sizeof(uint16_t) * (size_t)colorCount * 2u);

	va_start(args, colorCount);
	for (i = 0; i < colorCount; ++i) {
		int16_t brightnessStep = (int16_t)va_arg(args, int);
		cycle->colors[i] = (uint16_t)(g_starColorStep16 * brightnessStep);
	}
	va_end(args);

	memcpy(&cycle->colors[colorCount], cycle->colors, sizeof(uint16_t) * (size_t)colorCount);
}

// FUNCTION: XWA 0x4DE190
void FlightStarfield_DrawBrightStarFlare(uint8_t starIndex, StarfieldScreenCoord screenXY, uint16_t* pixel,
										 uint16_t baseColor) {
	uint16_t flareColor;

	flareColor = baseColor;
	if ((baseColor & 0x1fu) > 0x10u) {
		flareColor = (uint16_t)(g_starColorStep16 * (baseColor & 7u));
		if ((starIndex & 1u) != 0) {
			if (screenXY.x < (uint16_t)g_flightVpWidth - 1) {
				pixel[1] = flareColor;
			}
			if (screenXY.x > 0) {
				pixel[-1] = flareColor;
			}
			if (screenXY.y < (uint16_t)g_flightVpHeight - 1) {
				pixel[(unsigned int)g_surfacePitch >> 1] = flareColor;
			}
			if (screenXY.y > 0) {
				pixel[-(g_surfacePitch >> 1)] = flareColor;
			}
		} else if (screenXY.x > 0) {
			pixel[-1] = flareColor;
			if (screenXY.y > 0) {
				pixel[-(g_surfacePitch >> 1)] = flareColor;
				flareColor = (uint16_t)(flareColor - g_starColorStep16);
				pixel[-(g_surfacePitch >> 1) - 1] = flareColor;
			}
		} else if (screenXY.y > 0) {
			pixel[-(g_surfacePitch >> 1)] = flareColor;
		}
	}
}

// FUNCTION: XWA 0x4DDBF0
void FlightStarfield_Render(void) {
	int densityStep;
	int directStarIndex;
	int cameraBin;
	int scanAzimuthBin;
	int angularBinOffset;
	int azimuthBinsToScan;
	int camForwardX;
	int camForwardY;
	StarfieldScreenCoord screenXY;

	if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
		return;
	}

	if (g_gameTime - g_starPaletteLastCycleTime > 59) {
		int cycleIndex;

		for (cycleIndex = 0; cycleIndex < 4; ++cycleIndex) {
			StarPaletteCycle* cycle;

			++g_starPaletteCycles[cycleIndex]->frame;
			cycle = g_starPaletteCycles[cycleIndex];
			if (cycle->frame >= cycle->count) {
				cycle->frame = 0;
			}
		}
		g_starPaletteLastCycleTime = g_gameTime;
	}

	densityStep = 0;
	if (TRANSFM2_CamMatDotRow2(0, 0, 0x7fff) > 0) {
		for (directStarIndex = 0; directStarIndex < 512; ++directStarIndex) {
			int starX;
			int starY;
			int starZ;
			int viewX;
			int viewY;
			int viewZ;
			uint16_t screenX;
			uint16_t screenY;

			++densityStep;
			if (densityStep > 4) {
				densityStep = 1;
			}
			if (densityStep < (uint16_t)g_starDensity) {
				continue;
			}

			starX = g_starfieldVectorsPtr[directStarIndex].x;
			starY = g_starfieldVectorsPtr[directStarIndex].y;
			starZ = g_starfieldVectorsPtr[directStarIndex].z;
			viewX = TRANSFM2_CamMatDotRow0(starX, starY, starZ);
			viewY = TRANSFM2_CamMatDotRow1(starX, starY, starZ);
			viewZ = TRANSFM2_CamMatDotRow2(starX, starY, starZ);

			if (viewZ <= 0) {
				screenX = 0xffffu;
				screenY = 0xffffu;
			} else {
				screenX = (uint16_t)TRANSFM2_ProjectStarfieldScreenX(viewX, viewZ);
				screenY = (uint16_t)TRANSFM2_ProjectStarfieldScreenY(viewY, viewZ);
			}
			screenXY.x = screenX;
			screenXY.y = screenY;

			if ((unsigned int)screenXY.x < (unsigned int)g_screenWidth &&
				(unsigned int)screenXY.y < (unsigned int)g_screenHeight) {
				uint16_t* pixel;

				pixel = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase + 2u * screenXY.x +
									(unsigned int)screenXY.y * (unsigned int)g_surfacePitch);
				if (g_useHardware3D || *pixel == g_flightTextPalette[g_flightColorEscapeBypassChar]) {
					StarPaletteCycle* paletteCycle;
					uint16_t starColor;

					paletteCycle = g_starPaletteCycles[g_starPaletteCycleIds[directStarIndex]];
					starColor =
						paletteCycle
							->colors[paletteCycle->frame + g_starPaletteCycleOffsets[directStarIndex]];
					*pixel = (uint16_t)(starColor + g_starBrightnessJitter[directStarIndex]);
					FlightStarfield_DrawBrightStarFlare((uint8_t)directStarIndex, screenXY, pixel, starColor);
				}
			}
		}
	} else {
		for (directStarIndex = 0; directStarIndex < 512; ++directStarIndex) {
			int starX;
			int starY;
			int starZ;
			int viewX;
			int viewY;
			int viewZ;
			uint16_t screenX;
			uint16_t screenY;

			++densityStep;
			if (densityStep > 4) {
				densityStep = 1;
			}
			if (densityStep < (uint16_t)g_starDensity) {
				continue;
			}

			starX = g_starfieldVectorsPtr[directStarIndex].x;
			starY = g_starfieldVectorsPtr[directStarIndex].y;
			starZ = -g_starfieldVectorsPtr[directStarIndex].z;
			viewX = TRANSFM2_CamMatDotRow0(starX, starY, starZ);
			viewY = TRANSFM2_CamMatDotRow1(starX, starY, starZ);
			viewZ = TRANSFM2_CamMatDotRow2(starX, starY, starZ);

			if (viewZ <= 0) {
				screenX = 0xffffu;
				screenY = 0xffffu;
			} else {
				screenX = (uint16_t)TRANSFM2_ProjectStarfieldScreenX(viewX, viewZ);
				screenY = (uint16_t)TRANSFM2_ProjectStarfieldScreenY(viewY, viewZ);
			}
			screenXY.x = screenX;
			screenXY.y = screenY;

			if ((unsigned int)screenXY.x < (unsigned int)g_screenWidth &&
				(unsigned int)screenXY.y < (unsigned int)g_screenHeight) {
				uint16_t* pixel;

				pixel = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase + 2u * screenXY.x +
									(unsigned int)screenXY.y * (unsigned int)g_surfacePitch);
				if (g_useHardware3D || *pixel == g_flightTextPalette[g_flightColorEscapeBypassChar]) {
					StarPaletteCycle* paletteCycle;
					uint16_t starColor;

					paletteCycle = g_starPaletteCycles[g_starPaletteCycleIds[directStarIndex]];
					starColor =
						paletteCycle
							->colors[paletteCycle->frame + g_starPaletteCycleOffsets[directStarIndex]];
					*pixel = (uint16_t)(starColor + g_starBrightnessJitter[directStarIndex]);
					FlightStarfield_DrawBrightStarFlare((uint8_t)directStarIndex, screenXY, pixel, starColor);
				}
			}
		}
	}

	camForwardX = g_camMatR2_X;
	camForwardY = g_camMatR2_Y;
	trig2_ctop(camForwardX, camForwardY, 0);
	if (trig2_polardistance == 0) {
		cameraBin = 0;
	} else {
		double cameraAzimuthRad;

		cameraAzimuthRad = acos((double)((camForwardY << 15) / trig2_polardistance) * 0.000030517578f);
		if (camForwardX > 0) {
			cameraAzimuthRad = 6.283185307179586 - cameraAzimuthRad;
		}
		cameraBin = (int)(cameraAzimuthRad * 20.371832f);
		if (cameraBin >= 128) {
			cameraBin = 127;
		}
	}

	scanAzimuthBin = cameraBin - 16;
	if (scanAzimuthBin < 0) {
		scanAzimuthBin += 128;
	}
	azimuthBinsToScan = 32;
	angularBinOffset = scanAzimuthBin << 7;
	do {
		int elevationBin;

		for (elevationBin = 0; elevationBin < 128; ++elevationBin) {
			int16_t mappedStarIndex16;
			int binIndex;
			int mappedStarIndex;
			int mappedStarX;
			int mappedStarY;
			int mappedStarZ;
			int mappedViewX;
			int mappedViewY;
			int mappedViewZ;
			uint16_t mappedScreenX;
			uint16_t mappedScreenY;

			++densityStep;
			if (densityStep > 4) {
				densityStep = 1;
			}
			if (densityStep < (uint16_t)g_starDensity) {
				continue;
			}

			binIndex = angularBinOffset + elevationBin;
			mappedStarIndex16 = g_starAngularBinToIndex[binIndex];
			if (mappedStarIndex16 == -1) {
				continue;
			}

			mappedStarIndex = mappedStarIndex16;
			mappedStarX = g_starfieldVectors[mappedStarIndex].x;
			mappedStarY = g_starfieldVectors[mappedStarIndex].y;
			mappedStarZ = g_starfieldVectors[mappedStarIndex].z;
			mappedViewX = TRANSFM2_CamMatDotRow0(mappedStarX, mappedStarY, mappedStarZ);
			mappedViewY = TRANSFM2_CamMatDotRow1(mappedStarX, mappedStarY, mappedStarZ);
			mappedViewZ = TRANSFM2_CamMatDotRow2(mappedStarX, mappedStarY, mappedStarZ);

			if (mappedViewZ <= 0) {
				mappedScreenX = 0xffffu;
				mappedScreenY = 0xffffu;
			} else {
				mappedScreenX = (uint16_t)TRANSFM2_ProjectStarfieldScreenX(mappedViewX, mappedViewZ);
				mappedScreenY = (uint16_t)TRANSFM2_ProjectStarfieldScreenY(mappedViewY, mappedViewZ);
			}
			screenXY.x = mappedScreenX;
			screenXY.y = mappedScreenY;

			if ((unsigned int)screenXY.x < (unsigned int)g_screenWidth &&
				(unsigned int)screenXY.y < (unsigned int)g_screenHeight) {
				uint16_t* pixel;

				pixel = (uint16_t*)((uint8_t*)g_flightSwFramebufferBase + 2u * screenXY.x +
									(unsigned int)screenXY.y * (unsigned int)g_surfacePitch);
				if (g_useHardware3D || *pixel == g_flightTextPalette[g_flightColorEscapeBypassChar]) {
					StarPaletteCycle* paletteCycle;
					uint16_t starColor;

					paletteCycle = g_starPaletteCycles[g_starPaletteCycleIds[mappedStarIndex]];
					starColor =
						paletteCycle
							->colors[paletteCycle->frame + g_starPaletteCycleOffsets[mappedStarIndex]];
					*pixel = (uint16_t)(starColor + g_starBrightnessJitter[mappedStarIndex]);
					FlightStarfield_DrawBrightStarFlare((uint8_t)mappedStarIndex, screenXY, pixel, starColor);
				}
			}
		}

		++scanAzimuthBin;
		angularBinOffset += 128;
		if (scanAzimuthBin >= 128) {
			scanAzimuthBin -= 128;
			angularBinOffset -= 0x4000;
		}
		--azimuthBinsToScan;
	} while (azimuthBinsToScan != 0);
}

// FUNCTION: XWA 0x4DE290
void FlightStarfield_BuildHyperspaceStreaks(void) {
	int directStarIndex;
	int cameraBin;
	int scanAzimuthBin;
	int angularBinOffset;
	int azimuthBinsToScan;

	g_hyperspaceStarStreakCount = 0;

	if (TRANSFM2_CamMatDotRow2(0, 0, 0x7fff) > 0) {
		for (directStarIndex = 0; directStarIndex < 512; ++directStarIndex) {
			int starX;
			int starY;
			int starZ;
			int viewX;
			int viewY;
			int viewZ;
			uint16_t screenX;
			uint16_t screenY;
			Vec3i* starVector;

			starVector = &g_starfieldVectorsPtr[directStarIndex];
			starX = starVector->x;
			starY = starVector->y;
			starZ = starVector->z;
			viewX = TRANSFM2_CamMatDotRow0(starX, starY, starZ);
			viewY = TRANSFM2_CamMatDotRow1(starX, starY, starZ);
			viewZ = TRANSFM2_CamMatDotRow2(starX, starY, starZ);

			if (viewZ <= 0) {
				screenX = 0xffffu;
				screenY = 0xffffu;
			} else {
				screenX = (uint16_t)TRANSFM2_ProjectStarfieldScreenX(viewX, viewZ);
				screenY = (uint16_t)TRANSFM2_ProjectStarfieldScreenY(viewY, viewZ);
			}

			if ((unsigned int)screenX < (unsigned int)g_screenWidth &&
				(unsigned int)screenY < (unsigned int)g_screenHeight) {
				int streakIdx;
				int absOffsetX;
				int absOffsetZ;
				int length;

				streakIdx = g_hyperspaceStarStreakCount;
				g_hyperspaceStarStreaks[streakIdx].screenX = screenX;
				g_hyperspaceStarStreaks[streakIdx].screenY = screenY;
				g_hyperspaceStarStreaks[streakIdx].offsetX =
					(((int)screenX - (uint16_t)g_flightVpCenterX) << 14) / g_projScaleInt;
				g_hyperspaceStarStreaks[streakIdx].offsetY = 0x4000;
				g_hyperspaceStarStreaks[streakIdx].offsetZ =
					((g_projOffsetY + (uint16_t)g_flightVpHeight - (uint16_t)g_flightVpCenterY - (int)screenY)
					 << 14) /
					g_projScaleInt;

				absOffsetX = g_hyperspaceStarStreaks[streakIdx].offsetX;
				if (absOffsetX < 0) {
					absOffsetX = -absOffsetX;
				}
				absOffsetZ = g_hyperspaceStarStreaks[streakIdx].offsetZ;
				if (absOffsetZ < 0) {
					absOffsetZ = -absOffsetZ;
				}
				length = (absOffsetX + absOffsetZ) >> 8;
				if (length < 0) {
					length = 0;
				}
				g_hyperspaceStarStreaks[streakIdx].length = length + 1;
				g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].rollAngle =
					(int)(uint16_t)trig2_arctan(g_hyperspaceStarStreaks[streakIdx].offsetZ,
												g_hyperspaceStarStreaks[streakIdx].offsetX) +
					0x4000;
				if (++g_hyperspaceStarStreakCount == 1024) {
					return;
				}
			}
		}
	} else {
		for (directStarIndex = 0; directStarIndex < 512; ++directStarIndex) {
			int starX;
			int starY;
			int starZ;
			int viewX;
			int viewY;
			int viewZ;
			uint16_t screenX;
			uint16_t screenY;
			Vec3i* starVector;

			starVector = &g_starfieldVectorsPtr[directStarIndex];
			starX = starVector->x;
			starY = starVector->y;
			starZ = -starVector->z;
			viewX = TRANSFM2_CamMatDotRow0(starX, starY, starZ);
			viewY = TRANSFM2_CamMatDotRow1(starX, starY, starZ);
			viewZ = TRANSFM2_CamMatDotRow2(starX, starY, starZ);

			if (viewZ <= 0) {
				screenX = 0xffffu;
				screenY = 0xffffu;
			} else {
				screenX = (uint16_t)TRANSFM2_ProjectStarfieldScreenX(viewX, viewZ);
				screenY = (uint16_t)TRANSFM2_ProjectStarfieldScreenY(viewY, viewZ);
			}

			if ((unsigned int)screenX < (unsigned int)g_screenWidth &&
				(unsigned int)screenY < (unsigned int)g_screenHeight) {
				int streakIdx;
				int absOffsetX;
				int absOffsetZ;
				int length;

				streakIdx = g_hyperspaceStarStreakCount;
				g_hyperspaceStarStreaks[streakIdx].screenX = screenX;
				g_hyperspaceStarStreaks[streakIdx].screenY = screenY;
				g_hyperspaceStarStreaks[streakIdx].offsetX =
					(((int)screenX - (uint16_t)g_flightVpCenterX) << 14) / g_projScaleInt;
				g_hyperspaceStarStreaks[streakIdx].offsetY = 0x4000;
				g_hyperspaceStarStreaks[streakIdx].offsetZ =
					((g_projOffsetY + (uint16_t)g_flightVpHeight - (uint16_t)g_flightVpCenterY - (int)screenY)
					 << 14) /
					g_projScaleInt;

				absOffsetX = g_hyperspaceStarStreaks[streakIdx].offsetX;
				if (absOffsetX < 0) {
					absOffsetX = -absOffsetX;
				}
				absOffsetZ = g_hyperspaceStarStreaks[streakIdx].offsetZ;
				if (absOffsetZ < 0) {
					absOffsetZ = -absOffsetZ;
				}
				length = (absOffsetX + absOffsetZ) >> 8;
				if (length < 0) {
					length = 0;
				}
				g_hyperspaceStarStreaks[streakIdx].length = length + 1;
				g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].rollAngle =
					(int)(uint16_t)trig2_arctan(g_hyperspaceStarStreaks[streakIdx].offsetZ,
												g_hyperspaceStarStreaks[streakIdx].offsetX) +
					0x4000;
				if (++g_hyperspaceStarStreakCount == 1024) {
					return;
				}
			}
		}
	}

	{
		int cameraX;
		int cameraY;

		cameraX = g_camMatR2_X;
		cameraY = g_camMatR2_Y;
		trig2_ctop(cameraX, cameraY, 0);
		if (trig2_polardistance == 0) {
			cameraBin = trig2_polardistance;
		} else {
			double cameraAzimuthRad;

			cameraAzimuthRad =
				acos((double)((cameraY * 32768) / (int)trig2_polardistance) * g_starfieldQ15ToUnitScale);
			if (cameraX > 0) {
				cameraAzimuthRad = g_starfieldTauRadians - cameraAzimuthRad;
			}
			cameraBin = (int)(cameraAzimuthRad * 20.371832f);
			if (cameraBin >= 128) {
				cameraBin = 127;
			}
		}
	}

	scanAzimuthBin = cameraBin - 16;
	if (scanAzimuthBin < 0) {
		scanAzimuthBin += 128;
	}
	azimuthBinsToScan = 32;
	angularBinOffset = scanAzimuthBin << 7;
	do {
		int elevationBin;

		for (elevationBin = 0; elevationBin < 128; ++elevationBin) {
			int16_t mappedStarIndex16;
			int mappedStarIndex;

			mappedStarIndex16 = g_starAngularBinToIndex[angularBinOffset + elevationBin];
			if (mappedStarIndex16 == -1) {
				continue;
			}

			mappedStarIndex = mappedStarIndex16;
			{
				int starX;
				int starY;
				int starZ;
				int viewX;
				int viewY;
				int viewZ;
				uint16_t screenX;
				uint16_t screenY;
				Vec3i* starVector;

				starVector = &g_starfieldVectors[mappedStarIndex];
				starX = starVector->x;
				starY = starVector->y;
				starZ = starVector->z;
				viewX = TRANSFM2_CamMatDotRow0(starX, starY, starZ);
				viewY = TRANSFM2_CamMatDotRow1(starX, starY, starZ);
				viewZ = TRANSFM2_CamMatDotRow2(starX, starY, starZ);

				if (viewZ <= 0) {
					screenX = 0xffffu;
					screenY = 0xffffu;
				} else {
					screenX = (uint16_t)TRANSFM2_ProjectStarfieldScreenX(viewX, viewZ);
					screenY = (uint16_t)TRANSFM2_ProjectStarfieldScreenY(viewY, viewZ);
				}

				if ((unsigned int)screenX < (unsigned int)g_screenWidth &&
					(unsigned int)screenY < (unsigned int)g_screenHeight) {
					int streakIdx;
					int absOffsetX;
					int absOffsetZ;
					int length;

					streakIdx = g_hyperspaceStarStreakCount;

					g_hyperspaceStarStreaks[streakIdx].screenX = screenX;
					g_hyperspaceStarStreaks[streakIdx].screenY = screenY;
					g_hyperspaceStarStreaks[streakIdx].offsetX =
						(((int)screenX - (uint16_t)g_flightVpCenterX) << 14) / g_projScaleInt;
					g_hyperspaceStarStreaks[streakIdx].offsetY = 0x4000;
					g_hyperspaceStarStreaks[streakIdx].offsetZ =
						((g_projOffsetY + (uint16_t)g_flightVpHeight - (uint16_t)g_flightVpCenterY -
						  (int)screenY)
						 << 14) /
						g_projScaleInt;

					absOffsetX = g_hyperspaceStarStreaks[streakIdx].offsetX;
					if (absOffsetX < 0) {
						absOffsetX = -absOffsetX;
					}
					absOffsetZ = g_hyperspaceStarStreaks[streakIdx].offsetZ;
					if (absOffsetZ < 0) {
						absOffsetZ = -absOffsetZ;
					}
					length = (absOffsetX + absOffsetZ) >> 8;
					if (length < 0) {
						length = 0;
					}
					g_hyperspaceStarStreaks[streakIdx].length = length + 1;
					g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].rollAngle =
						(int)(uint16_t)trig2_arctan(g_hyperspaceStarStreaks[streakIdx].offsetZ,
													g_hyperspaceStarStreaks[streakIdx].offsetX) +
						0x4000;
					if (++g_hyperspaceStarStreakCount == 1024) {
						return;
					}
				}
			}
		}

		++scanAzimuthBin;
		angularBinOffset += 128;
		if (scanAzimuthBin >= 128) {
			scanAzimuthBin -= 128;
			angularBinOffset -= 0x4000;
		}
		--azimuthBinsToScan;
	} while (azimuthBinsToScan != 0);
}

// FUNCTION: XWA 0x4DE870
void FlightStarfield_BuildRandomHyperspaceStreaks(void) {
	g_hyperspaceStarStreakCount = 0;
	do {
		int offsetX;
		int offsetZ;
		int averageOffset;

		do {
			int randX;
			int randZ;
			int scale;

			randX = rand() & 0x3fff;
			randZ = rand() & 0x3fff;
			scale = (rand() & 3) + 2;
			offsetX = (randX * scale) >> 3;
			offsetZ = (randZ * scale) >> 3;
			averageOffset = (offsetZ + offsetX) >> 1;
		} while (averageOffset < 8);

		g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].length = (averageOffset >> 7) + 1;
		if ((rand() & 0x1000) != 0) {
			g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetX = offsetX;
		} else {
			g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetX = -offsetX;
		}

		if ((rand() & 0x1000) != 0) {
			g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetZ = offsetZ;
		} else {
			g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetZ = -offsetZ;
		}
		g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetY =
			(int)(Particle_RandUnitFloat() * g_hyperspaceRandomStreakYOffsetScale);
		g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].rollAngle =
			(int)(uint16_t)trig2_arctan(g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetZ,
										g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetX) +
			0x4000;

		++g_hyperspaceStarStreakCount;
	} while (g_hyperspaceStarStreakCount < 1024);
}

// FUNCTION: XWA 0x47D6F0
void Flight_InitOutboundHyperspaceStreaks(void) {
	FlightStarfield_BuildHyperspaceStreaks();
	g_hyperspaceTunnelFrameQ16 = 0x10000;
#ifdef XWA_MODERN
	XwaSnapshot_NoteHyperspaceVisibleStreakCount((uint32_t)g_hyperspaceStarStreakCount);
#endif
}

// FUNCTION: XWA 0x47D700
void Flight_InitInboundHyperspaceStreaks(void) {
	FlightStarfield_BuildRandomHyperspaceStreaks();
	g_hyperspaceTunnelFrameQ16 = 0x10000;
#ifdef XWA_MODERN
	XwaSnapshot_NoteHyperspaceVisibleStreakCount((uint32_t)g_hyperspaceStarStreakCount);
#endif
}

// FUNCTION: XWA 0x47D710
void Flight_RenderHyperspaceTransitionEffects(void) {
	ObjectTypeId tunnelModelType;
	int modelType;
	FlightTexQuad quad;

	tunnelModelType = OBJ_AnimationTextureGroup3051;
	modelType = OBJ_AnimationTextureGroup3051;

	if (g_players[g_localPlayer].hyperspacePhase != PLAYER_HYPERSPACE_REGION_TRANSFER) {
		ObjectRecord savedObject0;
		MobileObject savedMobile0;
		OptimizedPolyObject* laserModel;
		OptimizedPolyObject savedLaserModelHeader;
		int transitionYOffset;
		int initialStreakCount;
		PlayerHyperspacePhase phase;

		savedObject0 = g_objectTable[0];
		savedMobile0 = *g_objectTable[0].mobj;

		laserModel = (OptimizedPolyObject*)Memory_LockHandle(g_loadedModels.byObjectType[OBJ_LaserRebel]);
		savedLaserModelHeader = *laserModel;
		g_hyperspaceModelHeaderPatch.selfMarker = laserModel;
		*laserModel = g_hyperspaceModelHeaderPatch;

		transitionYOffset = 0;
		initialStreakCount = g_hyperspaceStarStreakCount;
#ifdef XWA_MODERN
		XwaSnapshot_NoteHyperspaceVisibleStreakCount((uint32_t)initialStreakCount);
#endif
		phase = g_players[g_localPlayer].hyperspacePhase;

		if (phase == PLAYER_HYPERSPACE_OUTBOUND) {
			uint32_t phaseTicks;
			double fadeTicks;

			phaseTicks = g_players[g_localPlayer].hyperspaceRuntime.phaseElapsedTicks;
			fadeTicks = (double)(phaseTicks - 472u);
			if (phaseTicks < 472u) {
				uint32_t scaledTicks;

				scaledTicks = phaseTicks >> 2;
				g_hyperspaceStreakQuadVertices[1].y = (float)((double)scaledTicks * (double)scaledTicks);
				g_hyperspaceStreakQuadVertices[2].y = g_hyperspaceStreakQuadVertices[1].y;
				if (g_hyperspaceStarStreakCount + 45 < 1024) {
					int streaksToAppend;

					for (streaksToAppend = 0; streaksToAppend < 45; ++streaksToAppend) {
						int offsetX;
						int offsetZ;
						int averageOffset;

						do {
							int randX;
							int randZ;
							int scale;

							randX = rand() & 0x3fff;
							randZ = rand() & 0x3fff;
							scale = (rand() & 3) + 2;
							offsetX = (randX * scale) >> 3;
							offsetZ = (randZ * scale) >> 3;
							averageOffset = (offsetZ + offsetX) >> 1;
						} while (averageOffset < 8);

						g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].length =
							(averageOffset >> 7) + 1;
						if ((rand() & 0x1000) != 0) {
							g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetX = offsetX;
						} else {
							g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetX = -offsetX;
						}

						if ((rand() & 0x1000) != 0) {
							g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetZ = offsetZ;
						} else {
							g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetZ = -offsetZ;
						}
						g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetY = 0x4000;
						{
							int streakIndex;
							Q16Angle rollAngle;

							rollAngle =
								trig2_arctan(g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetZ,
											 g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].offsetX);
							g_hyperspaceStarStreaks[g_hyperspaceStarStreakCount].rollAngle =
								(uint16_t)rollAngle + 0x4000;
							streakIndex = g_hyperspaceStarStreakCount;
							g_hyperspaceStarStreakCount = streakIndex + 1;
						}
					}
				}
			} else {
				uint32_t yOffsetTerm;
				int alpha;

				g_hyperspaceStreakQuadVertices[1].y = 20000.0f;
				g_hyperspaceStreakQuadVertices[2].y = 20000.0f;

				yOffsetTerm = phaseTicks * 2u - 944u;
				transitionYOffset = (int)((double)yOffsetTerm * (double)yOffsetTerm);
				fadeTicks *= g_hyperspaceTransitionTickScale;
				fadeTicks *= g_hyperspaceTransitionAlphaScale;
				alpha = (int)fadeTicks;
				modelType = OBJ_LightingEffectTextureGroup1000;

				quad.screenY = g_flightVpCenterY - g_projOffsetY;
				quad.screenX = g_flightVpCenterX;
				quad.depthZ = 0x4000;
				quad.rotationAngle = 0;
				FeDiskIo_SelectTextureFrame(OBJ_LightingEffectTextureGroup1000, 2u, 256);
				quad.screenSize = (uint16_t)(32 * g_flightVpWidth);

				RenderQuad_DrawModelTexture(OBJ_LightingEffectTextureGroup1000, &quad,
											(int)(((uint32_t)(uint16_t)alpha << 24) + 0x00ffffffu));
			}
		} else if (phase == PLAYER_HYPERSPACE_INBOUND) {
			uint32_t phaseTicks;
			double phaseTicksAsDouble;
			int remainingTicks;

			phaseTicks = g_players[g_localPlayer].hyperspaceRuntime.phaseElapsedTicks;
			phaseTicksAsDouble = (double)phaseTicks;
			remainingTicks = 236 - (int)phaseTicks;
			if (remainingTicks < 0) {
				remainingTicks = 0;
			}

			if (remainingTicks < 118) {
				float inboundScale;

				inboundScale =
					g_hyperspaceInboundBaseLength -
					(g_hyperspaceTransitionUnitScale - remainingTicks * g_hyperspaceTransitionTickScale) *
						g_hyperspaceInboundLengthGrowth;
				g_hyperspaceStreakQuadVertices[1].y = inboundScale;
				g_hyperspaceStreakQuadVertices[2].y = inboundScale;
				transitionYOffset = (int)((float)(8 * remainingTicks) * (float)(4 * remainingTicks) *
										  g_hyperspaceTransitionYOffsetScale);
			} else {
				int alpha;

				g_hyperspaceStreakQuadVertices[1].y = 10000.0f;
				g_hyperspaceStreakQuadVertices[2].y = 10000.0f;
				transitionYOffset = (int)((float)(8 * remainingTicks) * (float)(4 * remainingTicks) *
										  g_hyperspaceTransitionYOffsetScale);
				phaseTicksAsDouble *= g_hyperspaceTransitionTickScale;
				alpha = (int)((g_hyperspaceTransitionUnitScale - phaseTicksAsDouble) *
							  g_hyperspaceTransitionAlphaScale);
				modelType = OBJ_LightingEffectTextureGroup1000;

				quad.screenY = g_flightVpCenterY - g_projOffsetY;
				quad.screenX = g_flightVpCenterX;
				quad.depthZ = 0x4000;
				quad.rotationAngle = 0;
				FeDiskIo_SelectTextureFrame(OBJ_LightingEffectTextureGroup1000, 2u, 256);
				quad.screenSize = (uint16_t)(32 * g_flightVpWidth);

				RenderQuad_DrawModelTexture(OBJ_LightingEffectTextureGroup1000, &quad,
											(int)(((uint32_t)(uint16_t)alpha << 24) + 0x00ffffffu));
			}
			g_unusedHyperspaceTransitionYOffsetNeg = -transitionYOffset;
		}

		g_objectTable[0].objectType = OBJ_LaserRebel;
		g_objectTable[0].genusId = GENUS_NpcProjectile;
		g_objectTable[0].angleD = 0;
		g_objectTable[0].yaw = 0;
		g_objectTable[0].pitch = 0x4000;

		{
			int i;

			for (i = 0; i < initialStreakCount; ++i) {
				ObjectRecord* transitionObject;
				int length;

				g_objectTable[0].world_x =
					g_hyperspaceStarStreaks[i].offsetX + g_players[g_localPlayer].viewState.savedTargetX;
				g_objectTable[0].world_y = g_hyperspaceStarStreaks[i].offsetY +
										   g_players[g_localPlayer].viewState.savedTargetY -
										   transitionYOffset;
				g_objectTable[0].world_z =
					g_hyperspaceStarStreaks[i].offsetZ + g_players[g_localPlayer].viewState.savedTargetZ;
				g_objectTable[0].roll = (Q16Angle)g_hyperspaceStarStreaks[i].rollAngle;

				length = g_hyperspaceStarStreaks[i].length;
				g_hyperspaceStreakQuadVertices[0].x = (float)length;
				g_hyperspaceStreakQuadVertices[1].x = g_hyperspaceStreakQuadVertices[0].x;
				g_hyperspaceStreakQuadVertices[2].x = (float)-length;
				g_hyperspaceStreakQuadVertices[3].x = g_hyperspaceStreakQuadVertices[2].x;

				transitionObject = &g_objectTable[0];
				transitionObject->mobj->orientMatrixDirty = 1;
				FVIEW_SetObjectTransform(transitionObject->roll, transitionObject->pitch,
										 transitionObject->yaw, transitionObject->angleD, transitionObject);
				RenderScene_DrawObjectModel(transitionObject);
			}
		}

		g_objectTable[0] = savedObject0;
		*g_objectTable[0].mobj = savedMobile0;
		tunnelModelType = (ObjectTypeId)modelType;
		*laserModel = savedLaserModelHeader;
	}

	if (g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_REGION_TRANSFER) {
		uint16_t tunnelModelTypeIndex;
		int tunnelFrameQ16;
		unsigned int frame;

		tunnelModelTypeIndex = (uint16_t)tunnelModelType;
		g_hyperspaceTunnelFrameQ16 += 786432 * (uint16_t)g_elapsedTicks / 236;
		tunnelFrameQ16 = g_hyperspaceTunnelFrameQ16;
		frame = (unsigned int)tunnelFrameQ16 >> 16;
		if (frame > g_modelTypeTable[tunnelModelTypeIndex].frameCount) {
			frame = 1;
			g_hyperspaceTunnelFrameQ16 = (tunnelFrameQ16 & 0xffffu) | 0x10000u;
		}

		quad.screenX = g_flightVpCenterX;
		quad.screenY = g_flightVpCenterY - g_projOffsetY;
		quad.depthZ = 0x4000;
		quad.rotationAngle = 0;
		FeDiskIo_SelectTextureFrame(tunnelModelType, (uint16_t)frame, 256);
		quad.screenSize =
			(uint16_t)(int)((float)-g_projOffsetY - (float)g_flightVpWidth * g_hyperspaceTunnelWidthScale);
		RenderQuad_DrawModelTexture((ObjectTypeId)tunnelModelTypeIndex, &quad, -1);
	}
}

// FUNCTION: XWA 0x4DDB70
void FlightStarfield_Shutdown(void) {
	int i;

	if (!g_starPaletteCycles) {
		return;
	}

	for (i = 0; i < 4; ++i) {
		StarPaletteCycle* cycle;

		cycle = g_starPaletteCycles[i];
		if (cycle) {
			if (cycle->colors) {
				Memory_FreeTagged("STARPALDATA", cycle->colors);
				g_starPaletteCycles[i]->colors = NULL;
			}
			Memory_FreeTagged("STARPALETTE", g_starPaletteCycles[i]);
			g_starPaletteCycles[i] = NULL;
		}
	}

	Memory_FreeTagged("STARPALARRAY", g_starPaletteCycles);
	g_starPaletteCycles = NULL;
}
