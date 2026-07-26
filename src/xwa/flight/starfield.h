#ifndef XWA_FLIGHT_STARFIELD_H
#define XWA_FLIGHT_STARFIELD_H

#include "xwa/math/vec3i.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct StarfieldPolarCoord {
	float azimuthRad;
	int   z;
	int   xyRadius;
} StarfieldPolarCoord;

typedef struct StarPaletteCycle {
	int       count;
	uint16_t* colors;
	int       frame;
} StarPaletteCycle;

typedef struct StarfieldScreenCoord {
	uint16_t x;
	uint16_t y;
} StarfieldScreenCoord;

typedef struct HyperspaceStarStreak {
	int offsetX;
	int offsetY;
	int offsetZ;
	int length;
	int rollAngle;
	int screenX;
	int screenY;
} HyperspaceStarStreak;

typedef char starfield_polar_coord_size[(sizeof(StarfieldPolarCoord) == 0x0c) ? 1 : -1];
typedef char starfield_polar_coord_z_offset[(offsetof(StarfieldPolarCoord, z) == 0x04) ? 1 : -1];
typedef char hyperspace_star_streak_size[(sizeof(HyperspaceStarStreak) == 0x1c) ? 1 : -1];

extern StarPaletteCycle** g_starPaletteCycles;
extern int16_t g_starAngularBinToIndex[128 * 128];
extern int     g_starColorStep16;
extern uint16_t g_starRedMask16;
extern int      g_starPaletteLastCycleTime;
extern Vec3i    g_starfieldVectors[2560];
extern Vec3i*   g_starfieldVectorsPtr;
extern Vec3i*   g_starfieldAngularVectorsPtr;
extern uint8_t  g_starPaletteCycleIds[2560];
extern uint16_t g_starPaletteCycleOffsets[2560];
extern uint16_t g_starBrightnessJitter[2560];
extern HyperspaceStarStreak g_hyperspaceStarStreaks[1024];
extern int g_hyperspaceStarStreakCount;
extern int g_hyperspaceTunnelFrameQ16;
extern int g_unusedHyperspaceTransitionYOffsetNeg;

void FlightStarfield_Init(void);
void FlightStarfield_ComputePolarCoords(Vec3i vec, StarfieldPolarCoord* outPolar);
void FlightStarfield_RandomSignedVec3(Vec3i* outVec);
char FlightStarfield_TryReserveAngularBin(StarfieldPolarCoord polar, int16_t starIndex);
void FlightStarfield_SetPaletteCycle(int cycleId, int colorCount, ...);
void FlightStarfield_DrawBrightStarFlare(uint8_t starIndex, StarfieldScreenCoord screenXY, uint16_t* pixel,
										 uint16_t baseColor);
void FlightStarfield_Render(void);
void FlightStarfield_BuildHyperspaceStreaks(void);
void FlightStarfield_BuildRandomHyperspaceStreaks(void);
void Flight_InitOutboundHyperspaceStreaks(void);
void Flight_InitInboundHyperspaceStreaks(void);
void Flight_RenderHyperspaceTransitionEffects(void);
void FlightStarfield_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
