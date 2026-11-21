/* The projectile tables of the 2.02.00 executable. The tables by type have an
   entry for each weapon type, from 280 (OBJ_LaserRebel) to 307. The original
   code also reads them by object type, at an address 280 entries before the
   table (the _BiasedBase symbols of the matching build). laser.c includes this
   file, and so does the mod importer, for its copy of the stock tables. A
   mod's flight settings can change the game's copy
   (xwa_runtime/models/projectiles.h). */

/* The damage of a shot. */
// GLOBAL: XWA 0x5B64B8
int g_projectileDamageByType[XWA_PROJECTILE_TYPE_COUNT] = {
	250,   500,  200,  400, 200, 400,  10000, 3000, 1500, 1600, 800,  15000, 6000,    65000,
	35000, 3000, 6000, 600, 500, 2000, 0,     750,  1000, 750,  1000, 1200,  1000000, 0,
};

/* The speed of a shot. */
// GLOBAL: XWA 0x5B6528
uint16_t g_projectileSpeedByType[XWA_PROJECTILE_TYPE_COUNT] = {
	2000, 2000, 1800, 1800, 1400, 1600, 250, 500,  1000, 900,  400,  300, 600, 50,
	100,  600,  300,  1400, 500,  225,  0,   1200, 1000, 1250, 1100, 900, 0,   0,
};

/* The life of a shot: whole seconds, then a fraction of a second in Q16. */
// GLOBAL: XWA 0x5B6560
uint16_t g_projectileLifetimeSecondsByType[XWA_PROJECTILE_TYPE_COUNT] = {
	1, 1, 1, 1, 1, 2, 40, 25, 3, 3, 5, 45, 25, 120, 90, 25, 40, 3, 5, 10, 0, 3, 4, 3, 3, 4, 10, 0,
};

// GLOBAL: XWA 0x5B6598
uint16_t g_projectileLifetimeFracQ16ByType[XWA_PROJECTILE_TYPE_COUNT] = {
	0, 32768, 0, 32768, 32768, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32768, 0, 0, 32768, 0, 0,
};

/* How far ahead of its launcher a shot starts, in game units. */
// GLOBAL: XWA 0x5B65D0
int16_t g_projectileLaunchOffsetByType[XWA_PROJECTILE_TYPE_COUNT] = {
	921, 921, 921, 921, 921, 921, 512, 512, 921, 921, 921, 512, 512, 48,
	512, 512, 512, 921, 256, 256, 0,   921, 921, 921, 921, 921, 921, 0,
};

/* 0 for a laser. 1 or 2 for a warhead: the AI fires class 2 warheads at large
   targets and class 1 warheads at the others. */
// GLOBAL: XWA 0x5B6608
uint8_t g_projectileWarheadClassByType[XWA_PROJECTILE_TYPE_COUNT] = {
	0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 2, 1, 2, 2, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* The score of a warhead hit, which the player loses again when the warhead
   runs out of time. */
// GLOBAL: XWA 0x5B6628
uint16_t g_projectileScoreByType[XWA_PROJECTILE_TYPE_COUNT] = {
	0, 0, 0, 0, 0, 0, 9, 6, 0, 0, 0, 15, 15, 45, 24, 9, 9, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* The side of a shot (0 or 1, 0xff for none), and the weapon type of the other
   side. */
// GLOBAL: XWA 0x5B6660
uint8_t g_projectileRequiredIffByType[XWA_PROJECTILE_TYPE_COUNT] = {
	0,    0,    1,    1, 0xff, 0xff, 0xff, 0xff, 0, 1, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0, 0xff, 0xff, 0xff, 0,    0, 1, 1,    1,    1,    0xff,
};

// GLOBAL: XWA 0x5B6680
uint16_t g_projectileAlternateIffTypeByType[XWA_PROJECTILE_TYPE_COUNT] = {
	OBJ_LaserImperial,
	OBJ_LaserImperialTurbo,
	OBJ_LaserRebel,
	OBJ_LaserRebelTurbo,
	OBJ_LaserIon,
	OBJ_LaserIonTurbo,
	OBJ_WarheadTorpedo,
	OBJ_WarheadMissile,
	OBJ_WarheadLaser2,
	OBJ_WarheadLaser1,
	OBJ_WarheadIon,
	OBJ_WarheadAdvancedTorpedo,
	OBJ_WarheadAdvancedMissile,
	OBJ_WarheadSpaceBomb,
	OBJ_WarheadRocket,
	OBJ_WarheadMagPulse,
	OBJ_WarheadIonPulse,
	OBJ_LaserImperialTurbo_303,
	OBJ_WarheadFlare,
	OBJ_SparkTextureGroup3002_299,
	OBJ_ChaffTextureGroup5000,
	OBJ_LaserImperialTurbo_304,
	OBJ_LaserImperialTurbo_305,
	OBJ_WarheadLaser3,
	OBJ_LaserRebelTurbo_301,
	OBJ_LaserRebelTurbo_302,
	OBJ_LaserImperialDS,
	OBJ_NoAsset_307,
};

/* The first row of a warhead's homing profile in the two tables below: 0, 7,
   14, 21, 28 or 35. The warhead's homing tier picks the entry in the row. */
// GLOBAL: XWA 0x5B6770
uint8_t g_projectileHomingProfileBaseByType[XWA_PROJECTILE_TYPE_COUNT] = {
	0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 14, 21, 28, 14, 14, 0, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* The turn rate and the speed change of a homing warhead, for each entry of
   each homing profile row. */
// GLOBAL: XWA 0x5B6790
uint16_t g_projectileHomingTurnRateByProfile[42] = {
	0, 1024, 2048, 3072, 5120,  7168,  9216,  0, 512,  1024, 2048,  3072,  4608,  6144,
	0, 2048, 4096, 5120, 10240, 14336, 18432, 0, 32,   64,   80,    96,    112,   128,
	0, 512,  1024, 1280, 1536,  1792,  2048,  0, 4096, 8192, 12288, 16384, 20480, 24576,
};

// GLOBAL: XWA 0x5B67E8
uint16_t g_projectileHomingSpeedAdjustRateByProfile[42] = {
	0, 50, 100, 200, 300, 400, 500, 0, 25, 50, 100, 150, 200, 250, 0, 100, 200, 400, 600, 800, 1000,
	0, 0,  0,   0,   0,   0,   0,   0, 10, 20, 30,  40,  50,  60,  0, 100, 200, 400, 600, 800, 1000,
};
