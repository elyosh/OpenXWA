#include "xwa/flight/ai/ai_internal.h"
#include "xwa/flight/death_star.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/starfield.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/fixed.h"
#include "xwa/util/time.h"

// GLOBAL: XWA 0x9AF178
PaiManeuverFunc g_aiCourseOrderManeuverMode;

// GLOBAL: XWA 0x9AF17C
PaiManeuverInitFunc g_aiCurrentManeuverInitProc;

// GLOBAL: XWA 0x5B6E48
static const int16_t g_aiCourseOrderLocalOffsetXByVar[28] = {
	-3072, 0,     3072, -3072, 0,     3072, -3072, 0,     3072, -3072, 0,     3072, -3072, 0,
	3072,  -3072, 0,    3072,  -3072, 0,    3072,  -3072, 0,    3072,  -3072, 0,    3072,  0,
};

// GLOBAL: XWA 0x5B6E80
static const int16_t g_aiCourseOrderLocalOffsetYByVar[28] = {
	3072, 3072, 3072, 3072, 3072,  3072,  3072,  3072,  3072,  0,     0,     0,     0,     0,
	0,    0,    0,    0,    -3072, -3072, -3072, -3072, -3072, -3072, -3072, -3072, -3072, 0,
};

// GLOBAL: XWA 0x5B6EB8
static const int16_t g_aiCourseOrderLocalOffsetZByVar[28] = {
	3072, 3072,  3072,  0,     0,    0,    -3072, -3072, -3072, 3072, 3072,  3072,  0,     0,
	0,    -3072, -3072, -3072, 3072, 3072, 3072,  0,     0,     0,    -3072, -3072, -3072, 0,
};

// GLOBAL: XWA 0x5B6E40
const uint16_t g_aiTurnAwayStateDelayBySkill[4] = { 5, 3, 1, 0 };

// GLOBAL: XWA 0x5B6EF0
const uint16_t g_orderThrottleToCraftThrottleSpeed[11] = { 0x0000, 0x1999, 0x3334, 0x4cce, 0x6668, 0x8000,
														   0x999a, 0xb334, 0xccce, 0xe668, 0xffff };

// GLOBAL: XWA 0x5B7058
// Deceleration ramp for the into-hyperspace run, indexed by maneuverPhase (clamped to 0..10).
static const uint16_t g_aiCourseOrderSpeedByManeuverPhase[11] = {
	3600, 3600, 3600, 3600, 3600, 3600, 2400, 1800, 1200, 450, 300,
};

// GLOBAL: XWA 0x5B7080
const int16_t g_formPosX[34][6] = {
	{ 0, 1, -1, 2, -2, 3 },     { 0, 1, -2, -3, 2, 3 },     { 0, 0, 0, 0, 0, 0 },    { 0, 1, -1, 2, -2, 3 },
	{ 1, 2, 3, 4, 5, 6 },       { -1, -2, -3, -4, -5, -6 }, { 0, -1, 0, -1, 0, -1 }, { 0, 1, -1, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },       { 0, 1, -1, 1, -1, 0 },     { 0, 1, -1, 2, -2, 3 },  { 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },       { 0, 0, 0, 0, 0, 0 },       { 0, 0, 0, 0, 0, 0 },    { 0, 1, 2, 3, 4, 5 },
	{ -1, -2, -3, -4, -5, -6 }, { 0, 0, 1, 2, 3, 4 },       { 1, 2, 3, 4, 5, 6 },    { 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },       { 0, 1, -1, 2, -2, 3 },     { 0, 1, -1, 2, -2, 3 },  { -1, 1, -1, 1, -1, 1 },
	{ 0, 0, 0, 0, 0, 0 },       { -1, 1, -1, 1, -1, 1 },    { 0, 0, 0, -1, 1, 0 },   { 0, 1, -1, 0, 0, 0 },
	{ 0, 2, -3, 3, -2, 0 },     { 0, 0, 0, 0, 0, 0 },       { 0, 2, -3, 3, -2, 0 },  { -1, 1, -2, 2, -1, 1 },
	{ 0, 0, 0, 0, 0, 0 },       { -1, 1, -2, 2, -1, 1 },
};

// GLOBAL: XWA 0x5B7218
const int16_t g_formPosY[34][6] = {
	{ 0, -1, -1, -2, -2, -3 },  { 3, 2, 1, 0, -1, -2 },    { -1, -2, -3, -4, -5, -6 },
	{ 0, 0, 0, 0, 0, 0 },       { 0, -1, -2, -3, -4, -5 }, { 0, -1, -2, -3, -4, -5 },
	{ 0, 0, -1, -1, -2, -2 },   { 1, 0, 0, -1, 0, 0 },     { 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, -1 },      { 1, 2, 2, 3, 3, 4 },      { 0, -1, -1, -2, -2, -3 },
	{ 1, 2, 2, 3, 3, 4 },       { 0, 1, 2, 3, 4, 5 },      { 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },       { 0, 0, 0, 0, 0, 0 },      { 0, 1, 2, 3, 4, 5 },
	{ -1, -2, -3, -4, -5, -6 }, { 0, -1, -2, -3, -4, -5 }, { 0, -1, -2, -3, -4, -5 },
	{ 0, 0, 0, 0, 0, 0 },       { 0, 0, 0, 0, 0, 0 },      { 1, 1, 0, 0, -1, -1 },
	{ 1, 1, 0, 0, -1, -1 },     { 0, 0, 0, 0, 0, 0 },      { 1, 0, 0, 0, 0, -1 },
	{ 0, 0, 0, 0, 1, -1 },      { 3, -3, 1, 1, -3, 0 },    { 3, -3, 1, 1, -3, 0 },
	{ 0, 0, 0, 0, 0, 0 },       { 2, 2, 0, 0, -2, -2 },    { 2, 2, 0, 0, -2, -2 },
	{ 0, 0, 0, 0, 0, 0 },
};

// GLOBAL: XWA 0x5B73B0
const int16_t g_formPosZ[34][6] = {
	{ 0, 0, 0, 0, 0, 0 },       { 0, 1, -1, -2, 2, 3 }, { 0, 0, 0, 0, 0, 0 },       { 0, 0, 0, 0, 0, 0 },
	{ 0, 1, 2, 3, 4, 5 },       { 0, 1, 2, 3, 4, 5 },   { 0, 0, 0, 0, 0, 0 },       { 0, 0, 0, 0, 1, -1 },
	{ 0, 1, 2, 3, 4, 5 },       { 0, 1, 1, -1, -1, 0 }, { 0, 0, 0, 0, 0, 0 },       { 0, 1, -1, 2, -2, 3 },
	{ 0, 1, -1, 2, -2, 3 },     { 0, 0, 0, 0, 0, 0 },   { -1, -2, -3, -4, -5, -6 }, { 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },       { 0, 1, 2, 3, 4, 5 },   { 1, 2, 3, 4, 5, 6 },       { 0, 1, 2, 3, 4, 5 },
	{ -1, -2, -3, -4, -5, -6 }, { 0, 1, 1, 2, 2, 3 },   { 0, -1, -1, -2, -2, -3 },  { 0, 0, 0, 0, 0, 0 },
	{ 1, -1, 1, -1, 1, -1 },    { 1, 1, 0, 0, -1, -1 }, { 0, 1, -1, 0, 0, 0 },      { 1, 0, 0, -1, 0, 0 },
	{ 0, 0, 0, 0, 0, 0 },       { 0, 2, -3, 3, -2, 0 }, { 3, -3, 1, 1, -3, 0 },     { 0, 0, 0, 0, 0, 0 },
	{ 1, -1, 2, -2, 1, -1 },    { 2, 2, 0, 0, -2, -2 },
};

// GLOBAL: XWA 0x5B7548
const int16_t g_formationDivisor[34] = { 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
										 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 2, 2, 2 };

// GLOBAL: XWA 0x5B7070
const uint16_t g_aiAvoidAttackerDelaySecondsByGroupAI[8] = { 8, 6, 5, 3, 2, 1, 0, 0 };

// GLOBAL: XWA 0x749AC0
const uint16_t g_aiAvoidAttackerDelayFracQ16ByGroupAI[6] = { 0, 0, 0, 0, 0, 0 };

// GLOBAL: XWA 0x5A9CA0
const double g_aiOrbitHalfTurnRadians = 3.14159;

typedef struct PaiContextSnapshot {
	CraftData* curCraft;
	PaiContext context;
} PaiContextSnapshot;

static char paiman_awaitboardmaneuver(void);
static char paiman_headtowardmaneuver(void);
static char paiman_immelmannmaneuver(void);
static char paiman_zoommaneuver(void);
static char paiman_speedawaymaneuver(void);
static char paiman_avoidstarshipmaneuver(void);
static char paiman_setupattackmaneuver(void);
static __inline int paiman_LocalDotQ15UnsignedShift(int dx, int dy, int dz, int16_t axisX, int16_t axisY,
													int16_t axisZ);
static char paiman_attackmaneuver(void);
static char paiman_turninsidemaneuver(void);
static char paiman_outofhangarmaneuver(void);
static char paiman_scissorsmaneuver(void);
static char paiman_splitsmaneuver(void);
static char paiman_headtowardfullmaneuver(void);
static char paiman_turnawaymaneuver(void);
static char paiman_rendezvousmaneuver(void);
static void paiman_AdvanceOrderWaypoint(unsigned int objectIdx);
static char paiman_cruisemaneuver(void);
static char paiman_headonattackmaneuver(void);
static char paiman_followleadermaneuver(void);
static char paiman_runawaymaneuver(void);
static char paiman_escortmaneuver(void);
static void paiman_initwaitmaneuver(void);

static char paiman_turninsidemaneuver(void);
static char paiman_splitsmaneuver(void);
static char paiman_immelmannmaneuver(void);
static char paiman_scissorsmaneuver(void);
static char paiman_rendezvousmaneuver(void);
static char paiman_cruisemaneuver(void);
static void paiman_AdvanceOrderWaypoint(unsigned int objectIdx);
static char paiman_headtowardfullmaneuver(void);
static char paiman_runawaymaneuver(void);
static char paiman_headonattackmaneuver(void);
static char paiman_followleadermaneuver(void);
static char paiman_setupattackmaneuver(void);
static char paiman_attackmaneuver(void);
static char paiman_zoommaneuver(void);
static char paiman_boardmaneuver(void);
static char paiman_awaitboardmaneuver(void);
static char paiman_headtowardmaneuver(void);
static char paiman_speedawaymaneuver(void);
static char paiman_escortmaneuver(void);
static char paiman_intohyperspacemaneuver(void);
static char paiman_outofhyperspacemaneuver(void);
static char paiman_turnawaymaneuver(void);
static char paiman_outofhangarmaneuver(void);
static char paiman_avoidstarshipmaneuver(void);
static void paiman_initwaitmaneuver(void);
static char paiman_dropoffmaneuver(void);
static char paiman_kamikazemaneuver(void);
static char paiman_avoidattackermaneuver(void);
static char paiman_dodgemaneuver(void);
static char paiman_orbitmaneuver(void);
static char paiman_releasemaneuver(void);
static char paiman_backupmaneuver(void);
static char paiman_parkmaneuver(void);
static char paiman_workonmaneuver(void);
static char paiman_deathstarfollowmaneuver(void);
static char paiman_followtargetmaneuver(void);

static void paiman_initnullmaneuver(void) {}

#define PAI_INIT(fn) ((PaiManeuverInitFunc)(fn))

// GLOBAL: XWA 0x5B6F08
PaiManeuverFunc g_aiCourseOrderManeuverTable[41] = {
	paiorder_nullhandler,           /*  0: 0x4B8A30 */
	paiman_turninsidemaneuver,      /*  1: 0x4AB8B0 */
	paiman_splitsmaneuver,          /*  2: 0x4ABA00 */
	paiman_immelmannmaneuver,       /*  3: 0x4ABB00 */
	paiman_scissorsmaneuver,        /*  4: 0x4ABCD0 */
	paiman_rendezvousmaneuver,      /*  5: 0x4ABE20 */
	paiman_cruisemaneuver,          /*  6: 0x4AC030 */
	paiman_headtowardfullmaneuver,  /*  7: 0x4AC7B0 */
	paiman_runawaymaneuver,         /*  8: 0x4AC8E0 */
	paiman_headonattackmaneuver,    /*  9: 0x4AC990 */
	paiman_followleadermaneuver,    /* 10: 0x4AC9C0 */
	paiman_setupattackmaneuver,     /* 11: 0x4ACE00 */
	paiman_attackmaneuver,          /* 12: 0x4AD170 */
	paiman_zoommaneuver,            /* 13: 0x4ADF90 */
	paiman_zoommaneuver,            /* 14: 0x4ADF90 */
	paiman_splitsmaneuver,          /* 15: 0x4ABA00 */
	paiman_speedawaymaneuver,       /* 16 SPEEDAWAYMANR: 0x4AE1D0 */
	paiman_escortmaneuver,          /* 17 ESCORTMANR: 0x4B0190 */
	paiman_boardmaneuver,           /* 18 BOARDMANR: 0x4B0770 */
	paiman_awaitboardmaneuver,      /* 19 AWAITBOARDMANR: 0x4B25D0 */
	paiman_headtowardmaneuver,      /* 20 HEADTOWARDMANR: 0x4B2620 */
	paiman_intohyperspacemaneuver,  /* 21 INTOHYPERSPACEMANR: 0x4AE540 */
	paiman_outofhyperspacemaneuver, /* 22 OUTOFHYPERSPACEMANR: 0x4AFE20 */
	paiman_attackmaneuver,          /* 23 ROCKETATTACKMANR: 0x4AD170 */
	paiman_turnawaymaneuver,        /* 24 TURNAWAYMANR: 0x4B2730 */
	paiman_awaitboardmaneuver,      /* 25 STOPMANR: 0x4B25D0 */
	paiman_outofhangarmaneuver,     /* 26 OUTOFHANGARMANR: 0x4B2820 */
	paiman_splitsmaneuver,          /* 27 EVASIVEMANR: 0x4ABA00 */
	paiman_avoidstarshipmaneuver,   /* 28 AVOIDSTARSHIPMANR: 0x4B2990 */
	paiman_avoidstarshipmaneuver,   /* 29 WAITMANR: 0x4B2990 */
	paiman_dropoffmaneuver,         /* 30 DROPOFFMANR: 0x4B2A40 */
	paiman_kamikazemaneuver,        /* 31: 0x4B2D10 */
	paiman_avoidattackermaneuver,   /* 32: 0x4B2EF0 */
	paiman_dodgemaneuver,           /* 33: 0x4B3110 */
	paiman_orbitmaneuver,           /* 34: 0x4B3250 */
	paiman_releasemaneuver,         /* 35: 0x4B3A40 */
	paiman_backupmaneuver,          /* 36: 0x4B4100 */
	paiman_parkmaneuver,            /* 37: 0x4B4980 */
	paiman_workonmaneuver,          /* 38: 0x4B5390 */
	paiman_deathstarfollowmaneuver, /* 39: 0x4B5910 */
	paiman_followtargetmaneuver,    /* 40: 0x4B86E0 */
};

// GLOBAL: XWA 0x5B6FB0
PaiManeuverInitFunc g_maneuverInitTable[41] = {
	PAI_INIT(paiorder_nullhandler),               /*  0: 0x4B8A30 */
	PAI_INIT(paiman_initturninsidemaneuver),      /*  1: 0x4AB860 */
	PAI_INIT(paiman_initsplitsmaneuver),          /*  2: 0x4AB9A0 */
	PAI_INIT(paiman_initimmelmannmaneuver),       /*  3: 0x4ABA20 */
	PAI_INIT(paiman_initscissorsmaneuver),        /*  4: 0x4ABBE0 */
	PAI_INIT(paiman_initrendezvousmaneuver),      /*  5: 0x4ABDB0 */
	PAI_INIT(paiman_initcruisemaneuver),          /*  6: 0x4ABE90 */
	PAI_INIT(paiman_initheadtowardfullmaneuver),  /*  7: 0x4AC780 */
	PAI_INIT(paiman_initrunawaymaneuver),         /*  8: 0x4AC7F0 */
	PAI_INIT(paiman_initheadonattackmaneuver),    /*  9: 0x4AC900 */
	paiman_initnullmaneuver,                      /* 10: 0x49AA30 */
	PAI_INIT(paiman_initsetupattackmaneuver),     /* 11: 0x4ACDE0 */
	PAI_INIT(paiman_initattackmaneuver),          /* 12: 0x4ACE60 */
	PAI_INIT(paiman_initzoommaneuver),            /* 13: 0x4ADE90 */
	PAI_INIT(paiman_initdivemaneuver),            /* 14: 0x4ADFA0 */
	PAI_INIT(paiman_initsplitsdivemaneuver),      /* 15: 0x4AE040 */
	PAI_INIT(paiman_initspeedawaymaneuver),       /* 16: 0x4AE0E0 */
	paiman_initnullmaneuver,                      /* 17: 0x49AA30 */
	PAI_INIT(paiman_initboardmaneuver),           /* 18: 0x4B0760 */
	PAI_INIT(paiman_initawaitboardmaneuver),      /* 19: 0x4B2590 */
	PAI_INIT(paiman_initheadtowardmaneuver),      /* 20: 0x4B2610 */
	PAI_INIT(paiman_initintohyperspacemaneuver),  /* 21: 0x4AE3F0 */
	PAI_INIT(paiman_initoutofhyperspacemaneuver), /* 22: 0x4AFD50 */
	PAI_INIT(paiman_initattackmaneuver),          /* 23: 0x4ACE60 */
	PAI_INIT(paiman_initturnawaymaneuver),        /* 24: 0x4B2670 */
	PAI_INIT(paiman_initawaitboardmaneuver),      /* 25: 0x4B2590 */
	PAI_INIT(paiman_initoutofhangarmaneuver),     /* 26: 0x4B2800 */
	PAI_INIT(paiman_initsplitsdivemaneuver),      /* 27: 0x4AE040 */
	PAI_INIT(paiman_initavoidstarshipmaneuver),   /* 28: 0x4B28E0 */
	PAI_INIT(paiman_initwaitmaneuver),            /* 29: 0x4B29A0 */
	PAI_INIT(paiman_initawaitboardmaneuver),      /* 30: 0x4B2590 */
	PAI_INIT(paiman_initkamikazemaneuver),        /* 31: 0x4B30F0 */
	PAI_INIT(paiman_initavoidattackermaneuver),   /* 32: 0x4B2D30 */
	PAI_INIT(paiman_initkamikazemaneuver),        /* 33: 0x4B30F0 */
	PAI_INIT(paiman_initorbitmaneuver),           /* 34: 0x4B3130 */
	PAI_INIT(paiman_initreleasemaneuver),         /* 35: 0x4B37B0 */
	PAI_INIT(paiman_initbackupmaneuver),          /* 36: 0x4B3F10 */
	PAI_INIT(paiman_initparkmaneuver),            /* 37: 0x4B4180 */
	PAI_INIT(paiman_initworkonmaneuver),          /* 38: 0x4B4EB0 */
	PAI_INIT(paiman_initawaitboardmaneuver),      /* 39: 0x4B2590 */
	PAI_INIT(paiman_initfollowtargetmaneuver),    /* 40: 0x4B8650 */
};

// FUNCTION: XWA 0x4AB770
void paiman_initmaneuver(void) {
	const XwaOrder* order;

	g_objectTable[g_paiContext.aiObjIdx].mobj->motionFlags = 0;
	g_curCraft->pushAccumX = 0;
	g_curCraft->pushAccumY = 0;
	g_curCraft->pushAccumZ = 0;
	g_curCraft->aiFlight.reactionTimer = 0;
	g_curCraft->aiFlight.maneuverCounter = 0;
	g_curCraft->warheadLockTicks = 0;

	order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				 .fg.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot];
	g_curCraft->commandedSpeed = order->speed;
	g_curCraft->commandedSpeed *= 5;
	g_curCraft->aiFlight.enterFlag = 4;
	g_paiContext.aiController->maneuverPhase = 0;
	g_aiCurrentManeuverInitProc = g_maneuverInitTable[g_paiContext.aiController->maneuverMode];
	g_aiCurrentManeuverInitProc();
}

// FUNCTION: XWA 0x4B0760
void paiman_initboardmaneuver(void) {
	g_paiContext.aiController->maneuverPhase = 0;
	return;
}

// FUNCTION: XWA 0x4B25D0
static char paiman_awaitboardmaneuver(void) {
	g_curCraft->aiFlight.enterFlag = 0;
	g_curCraft->aiFlight.headingState = 0;
	g_curCraft->aiFlight.turnState = 0;
	g_curCraft->throttleSpeed = 0;
	return 0;
}

// FUNCTION: XWA 0x4B2620
static char paiman_headtowardmaneuver(void) {
	paiman_setflighttotarget(0, 1);
	g_curCraft->aiFlight.enterFlag = 1;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = 0;
	return 0;
}

// FUNCTION: XWA 0x4AB9A0
int paiman_initsplitsmaneuver(void) {
	g_curCraft->aiFlight.rollStep = -1;
	g_paiContext.aiController->targetRoll = 0x8000;
	g_curCraft->aiFlight.turnState = 0;
	g_curCraft->aiFlight.headingForce = 1;
	g_paiContext.aiController->targetZAngle = 0x4000;
	g_curCraft->aiFlight.headingState = 2;
	g_curCraft->aiFlight.headingStep = -1;
	return 0xffff;
}

// FUNCTION: XWA 0x4ABA20
AiController* paiman_initimmelmannmaneuver(void) {
	AiController* result;
	Q16Angle pitch;

	g_curCraft->throttleSpeed = -1;
	g_curCraft->aiFlight.enterFlag = 1;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = 0;
	g_curCraft->aiFlight.turnState = 0;
	g_paiContext.aiController->targetZAngle = 0x4000;
	g_curCraft->aiFlight.headingStep = -1;
	g_curCraft->aiFlight.headingForce = 0;

	pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
	if (pitch < 0x4000u) {
		g_curCraft->aiFlight.headingState = 2;
	} else if (pitch > 0x4000u) {
		g_curCraft->aiFlight.headingState = 1;
	} else {
		g_curCraft->aiFlight.headingState = 3;
	}

	result = g_paiContext.aiController;
	result->maneuverTimer = 0;
	return result;
}

// FUNCTION: XWA 0x4ABB00
static char paiman_immelmannmaneuver(void) {
	switch ((uint8_t)g_paiContext.aiController->maneuverPhase) {
		case 0:
			if (g_curCraft->aiFlight.headingState == 3) {
				g_curCraft->aiFlight.headingState = 1;
				g_curCraft->aiFlight.headingStep = -1;
				g_curCraft->aiFlight.headingForce = 1;
				g_paiContext.aiController->targetZAngle = 0x4000;
				g_paiContext.aiController->maneuverPhase = 1;
			}
			return 0;

		case 1:
			if (g_curCraft->aiFlight.headingState == 3) {
				g_curCraft->aiFlight.enterFlag = 1;
				g_curCraft->aiFlight.rollStep = -1;
				g_curCraft->aiFlight.rollAccel = 0x4000;
				g_paiContext.aiController->targetRoll = 0;
				g_curCraft->aiFlight.turnState = 0;
				g_paiContext.aiController->maneuverPhase = 2;
			}
			return 0;

		case 2:
			if (g_curCraft->aiFlight.enterFlag == 4 && g_curCraft->aiFlight.headingState == 3) {
				return 1;
			}
			return 0;
	}

	return 0;
}

// FUNCTION: XWA 0x4ADFA0
void paiman_initdivemaneuver(void) {
	g_curCraft->throttleSpeed = (uint16_t)-1;
	g_paiContext.aiController->targetZAngle = (uint16_t)((GameRand() & 0x0fff) + 22528);
	g_curCraft->aiFlight.climbState = 0;
	if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
		g_curCraft->aiFlight.headingState = 1;
	} else {
		g_curCraft->aiFlight.headingState = 2;
	}
	g_curCraft->aiFlight.headingForce = 0;
	g_curCraft->aiFlight.headingStep = -1;
	g_paiContext.aiController->maneuverTimer = 1180;
	return;
}

// FUNCTION: XWA 0x4ADE90
void paiman_initzoommaneuver(void) {
	g_curCraft->throttleSpeed = (uint16_t)-1;
	g_curCraft->aiFlight.enterFlag = 3;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = (uint16_t)GameRand();
	g_paiContext.aiController->targetZAngle = (uint16_t)(0x2000 - (GameRand() & 0x0fff));
	g_paiContext.aiController->maneuverTimer = 236 * ((GameRand() & 3) + 3);
	g_curCraft->aiFlight.climbState = 0;
	g_curCraft->aiFlight.diveState = 0;
	if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
		g_curCraft->aiFlight.headingState = 1;
	} else {
		g_curCraft->aiFlight.headingState = 2;
	}
	g_curCraft->aiFlight.headingForce = 0;
	g_curCraft->aiFlight.headingStep = -1;
	return;
}

// FUNCTION: XWA 0x4ADF90
static char paiman_zoommaneuver(void) {
	if (g_paiContext.aiController->maneuverTimer == 0) {
		return 1;
	} else {
		return 0;
	}
}

// FUNCTION: XWA 0x4AE040
void paiman_initsplitsdivemaneuver(void) {
	g_curCraft->aiFlight.enterFlag = 1;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = 0x8000;
	g_curCraft->aiFlight.turnState = 0;
	g_curCraft->aiFlight.headingForce = 1;
	g_paiContext.aiController->targetZAngle = (uint16_t)((GameRand() & 0x3fff) + 0x4000);
	g_curCraft->aiFlight.headingState = 2;
	g_curCraft->aiFlight.headingStep = (pai_GetEffectiveSkillValue(g_curCraft) >> 1) + 0x8000;
	return;
}

// FUNCTION: XWA 0x4AE1D0
static char paiman_speedawaymaneuver(void) {
	AiController* aiController;

	aiController = g_paiContext.aiController;
	if (aiController->aiPlanState == 0) {
		unsigned int objIdx;
		int pushDelta;
		int yawOffset;
		unsigned int effectiveSkill;

		aiController->targetXYAngle = (uint16_t)(0u - aiController->targetXYAngle);
		objIdx = g_paiContext.aiObjIdx;
		pushDelta = (GameRand() & 0x1f) + 50;
		yawOffset = (int)(uint8_t)GameRand() + 0x180;
		if (g_curCraft->pushAccumZ >= 0) {
			pushDelta = -pushDelta;
			yawOffset = -yawOffset;
		}
		g_curCraft->pushAccumZ = (uint16_t)pushDelta;
		g_paiContext.aiController->targetXYAngle = (uint16_t)(g_objectTable[objIdx].yaw + yawOffset);
		effectiveSkill = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
		paiman_setturn((effectiveSkill >> 1) + 0x8000);
		g_paiContext.aiController->aiPlanState = 118;
		aiController = g_paiContext.aiController;
	}
	if (aiController->maneuverTimer == 0) {
		return 1;
	}
	return 0;
}

// FUNCTION: XWA 0x4B2590
int paiman_initawaitboardmaneuver(void) {
	g_curCraft->aiFlight.enterFlag = 0;
	g_curCraft->aiFlight.headingState = 0;
	g_curCraft->aiFlight.turnState = 0;
	g_curCraft->throttleSpeed = 0;
	return 0;
}

// FUNCTION: XWA 0x4B28E0
void paiman_initavoidstarshipmaneuver(void) {
	unsigned int effectiveSkill;

	g_paiContext.aiController->aiPlanState = 236 * ((GameRand() & 3) + 12);
	effectiveSkill = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
	paiman_setturn((effectiveSkill >> 1) + 0x8000);
	g_curCraft->aiFlight.headingStep = -1;
	g_curCraft->aiFlight.headingForce = 0;

	if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
		g_curCraft->aiFlight.headingState = 1;
	} else {
		g_curCraft->aiFlight.headingState = 2;
	}
}

// FUNCTION: XWA 0x4B2D30
void paiman_initavoidattackermaneuver(void) {
	uint16_t pitch;
	uint16_t turnStep;
	unsigned int skillValue;
	uint16_t groupAI;

	g_paiContext.aiController->maneuverPhase = (char)(GameRand() & 1);
	if (g_paiContext.aiController->maneuverPhase) {
		g_paiContext.aiController->targetXYAngle =
			(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw + 0x4000u);
	} else {
		g_paiContext.aiController->targetXYAngle =
			(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - 0x4000u);
	}

	skillValue = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
	turnStep = (uint16_t)((skillValue >> 1) + 0x8000u);
	paiman_setturn(turnStep);

	pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
	if (pitch < 0x4000u) {
		g_paiContext.aiController->targetZAngle = (uint16_t)(pitch + 0x3000u);
		g_curCraft->aiFlight.headingState = 2;
	} else {
		g_paiContext.aiController->targetZAngle = (uint16_t)(pitch - 0x3000u);
		g_curCraft->aiFlight.headingState = 1;
	}

	g_curCraft->aiFlight.headingStep = -1;
	g_curCraft->aiFlight.headingForce = 0;
	g_curCraft->aiFlight.enterFlag = 3;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = (uint16_t)GameRand();
	g_paiContext.aiController->maneuverTimer = 236 * ((GameRand() & 7) + 10);

	groupAI = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.groupAI;
	g_paiContext.aiController->aiPlanState =
		(int)((uint16_t)MATH2_fraction(g_aiAvoidAttackerDelayFracQ16ByGroupAI[groupAI], 0x00ecu) +
			  236u * g_aiAvoidAttackerDelaySecondsByGroupAI[groupAI]);
}

// FUNCTION: XWA 0x4B8650
void paiman_initfollowtargetmaneuver(void) {
	int playerOwnerIdx;
	uint16_t targetObjIdx;

	g_curCraft->aiFlight.enterFlag = 0;
	g_curCraft->aiFlight.headingState = 0;
	g_curCraft->aiFlight.turnState = 0;
	g_curCraft->aiFlight.turnAccel = 0x4000;
	g_curCraft->aiFlight.pitchAccel = 0x4000;

	playerOwnerIdx = g_objectTable[g_paiContext.aiObjIdx].playerOwnerIdx;
	if (playerOwnerIdx != -1) {
		targetObjIdx = g_paiContext.aiController->targetObjIdx;
		if (targetObjIdx != 0xFFFF && targetObjIdx != g_players[playerOwnerIdx].currentTargetObjectIdx) {
			Player_SetTarget(targetObjIdx, (unsigned int)playerOwnerIdx);
		}
	}
}

// FUNCTION: XWA 0x4B4180
void paiman_initparkmaneuver(void) {
	uint16_t pitch;
	uint16_t targetObjIdx;
	uint16_t targetFgIdx;
	unsigned int targetModelIndex;
	unsigned int dockPointCount;
	int selectedDockPoint;
	uint32_t bestRange;
	unsigned int dockPointIdx;
	uint8_t elapsedHours;
	uint8_t elapsedMinutes;
	uint8_t elapsedSeconds;

	elapsedHours = g_missionElapsedClock.hours;
	elapsedMinutes = g_missionElapsedClock.minutes;
	elapsedSeconds = g_missionElapsedClock.seconds;
	if ((uint8_t)(elapsedHours | elapsedMinutes | elapsedSeconds) != 0) {
		pai_CalcAnglesToAimPoint();
		if (trig2_polardistance < 2048) {
			g_curCraft->throttleSpeed = 0;
			g_curCraft->aiFlight.diveState = 0;
			g_curCraft->aiFlight.climbState = 0;
			g_paiContext.aiController->targetZAngle = 0x4000;
			g_curCraft->aiFlight.headingStep = -1;
			g_curCraft->aiFlight.headingForce = 0;

			pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
			if (pitch < 0x4000u) {
				g_curCraft->aiFlight.headingState = 2;
			} else if (pitch > 0x4000u) {
				g_curCraft->aiFlight.headingState = 1;
			} else {
				g_curCraft->aiFlight.headingState = 3;
			}

			g_curCraft->aiFlight.enterFlag = 1;
			g_curCraft->aiFlight.rollStep = -1;
			g_curCraft->aiFlight.rollAccel = -1;
			g_paiContext.aiController->targetRoll = 0;
			g_curCraft->aiFlight.turnState = 0;
			g_paiContext.aiController->maneuverPhase = 1;
			return;
		} else {
			paiman_setflighttotarget(0, 1);
			g_paiContext.aiController->maneuverPhase = 0;
			return;
		}
	}

	g_paiContext.aiRequireLiveOrderTarget = 0;
	g_paiContext.aiTargetSearchFlags = 7;

	targetObjIdx = (uint16_t)paifight_FindNearestAttackOrderTarget(
		(MissionTriggerVariableType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target1Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target1,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target1OrTarget2,
		(MissionTriggerVariableType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target2Type,
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			.fg
			.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					g_paiContext.curOrderCoord.fields.orderSlot]
			.target2);
	targetFgIdx = 0;

	if (targetObjIdx == 0xffffu) {
		while ((uint16_t)targetFgIdx < (int16_t)g_missionHeader.numFlightGroups) {
			int matchesTarget1;
			int matchesTarget2;

			matchesTarget1 = Mission_FlightGroupMatchesTriggerVariable(
				targetFgIdx,
				(MissionTriggerVariableType)
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.target1Type,
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg
					.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot]
					.target1);
			matchesTarget2 = Mission_FlightGroupMatchesTriggerVariable(
				targetFgIdx,
				(MissionTriggerVariableType)
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.target2Type,
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg
					.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot]
					.target2);

			if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg
					.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot]
					.target1OrTarget2 == 1) {
				matchesTarget1 |= matchesTarget2;
			} else {
				matchesTarget1 &= matchesTarget2;
			}

			if ((int16_t)matchesTarget1 != 0) {
				Mission_ResolveObjectOrMissionPointWorldLoc(0x8000u, targetFgIdx,
															g_missionFlightGroups[g_currentFlightGroupIdx]
																.fg.missionPointRegions[XWA_FG_POINT_START_1],
															0);
				targetModelIndex =
					(uint16_t)g_modelTypeTable[g_objectTypeTables.craftTypeToObjectType
												   [g_missionFlightGroups[targetFgIdx].fg.craftType]]
						.modelIndex;
				break;
			}

			++targetFgIdx;
		}

		if ((uint16_t)targetFgIdx == (int16_t)g_missionHeader.numFlightGroups) {
			return;
		}
	} else {
		Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
		targetModelIndex =
			(uint16_t)g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].modelIndex;
	}

	bestRange = UINT32_MAX;
	selectedDockPoint = 0xffff;
	dockPointCount = (uint16_t)g_modelDefs[targetModelIndex].dockPointCount;

	for (dockPointIdx = 0; dockPointIdx < dockPointCount; ++dockPointIdx) {
		int dockWorldX;
		int dockWorldY;
		int dockWorldZ;
		int occupied;
		uint32_t scanObjIdx;

		dockWorldX = worldlocx + g_modelDefs[targetModelIndex].dockPoints[dockPointIdx].x;
		dockWorldY = worldlocy + g_modelDefs[targetModelIndex].dockPoints[dockPointIdx].y;
		dockWorldZ = worldlocz + g_modelDefs[targetModelIndex].dockPoints[dockPointIdx].z;
		g_targetRangeScore =
			collide_roughdistance3d(dockWorldX - g_objectTable[g_paiContext.aiObjIdx].world_x,
									dockWorldY - g_objectTable[g_paiContext.aiObjIdx].world_y,
									dockWorldZ - g_objectTable[g_paiContext.aiObjIdx].world_z);

		if ((uint32_t)g_targetRangeScore >= bestRange) {
			continue;
		}

		occupied = 0;
		for (scanObjIdx = g_activeRegionObjectSlotStart; scanObjIdx < g_activeRegionCraftObjectSlotEnd;
			 ++scanObjIdx) {
			AiController* effectiveController;

			if (g_objectTable[scanObjIdx].objectType == 0 || scanObjIdx == g_paiContext.aiObjIdx) {
				continue;
			}

			effectiveController = pai_GetEffectiveAIController(g_objectTable[scanObjIdx].mobj->pCraft);
			if (dockWorldX == effectiveController->aimPointX &&
				dockWorldY == effectiveController->aimPointY &&
				dockWorldZ == effectiveController->aimPointZ) {
				occupied = 1;
				break;
			}
		}

		if (!occupied) {
			selectedDockPoint = dockPointIdx;
			bestRange = (uint32_t)g_targetRangeScore;
		}
	}

	if (selectedDockPoint != 0xffff) {
		const Vec3i* dockPoint;

		dockPoint = &g_modelDefs[targetModelIndex].dockPoints[selectedDockPoint];

		g_objectTable[g_paiContext.aiObjIdx].world_x = worldlocx + dockPoint->x;
		g_objectTable[g_paiContext.aiObjIdx].world_y = worldlocy + dockPoint->y;
		g_objectTable[g_paiContext.aiObjIdx].world_z = worldlocz + dockPoint->z;
		g_paiContext.aiController->aimPointX = g_objectTable[g_paiContext.aiObjIdx].world_x;
		g_paiContext.aiController->aimPointY = g_objectTable[g_paiContext.aiObjIdx].world_y;
		g_paiContext.aiController->aimPointZ = g_objectTable[g_paiContext.aiObjIdx].world_z;
		g_objectTable[g_paiContext.aiObjIdx].world_z -= g_modelDefs[g_curCraft->modelIndex].meshAttachData[4];

		g_objectTable[g_paiContext.aiObjIdx].mobj->prevWorldX = g_objectTable[g_paiContext.aiObjIdx].world_x;
		g_objectTable[g_paiContext.aiObjIdx].mobj->prevWorldY = g_objectTable[g_paiContext.aiObjIdx].world_y;
		g_objectTable[g_paiContext.aiObjIdx].mobj->prevWorldZ = g_objectTable[g_paiContext.aiObjIdx].world_z;

		trig2_ctop2dim(worldlocx - g_objectTable[g_paiContext.aiObjIdx].world_x,
					   worldlocy - g_objectTable[g_paiContext.aiObjIdx].world_y);
		g_objectTable[g_paiContext.aiObjIdx].yaw = trig2_xyangle;
		g_objectTable[g_paiContext.aiObjIdx].pitch = 0x4000;
		g_objectTable[g_paiContext.aiObjIdx].roll = 0;
		g_objectTable[g_paiContext.aiObjIdx].mobj->orientMatrixDirty = 1;
		g_objectTable[g_paiContext.aiObjIdx].mobj->moveVectorDirty = 1;
		g_paiContext.aiController->maneuverPhase = 2;
		g_paiContext.aiController->maneuverTimer =
			Mission_DecodeOrderTime(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										.fg
										.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
												g_paiContext.curOrderCoord.fields.orderSlot]
										.variable1);
		g_paiContext.aiController->maneuverTimer *= 236;
		g_paiContext.aiController->targetXYAngle = (uint16_t)(trig2_xyangle + 0x8000u);
		if (targetObjIdx == 0xffffu) {
			g_paiContext.aiController->targetObjIdx = targetFgIdx;
		} else {
			g_paiContext.aiController->targetObjIdx = targetObjIdx;
		}
	}

	g_curCraft->throttleSpeed = 0;
}

// FUNCTION: XWA 0x4B2800
void paiman_initoutofhangarmaneuver(void) {
	g_paiContext.aiController->maneuverTimer = 2360;
	FlightObject_InitMeshAnimationDefaults((int)g_paiContext.aiObjIdx);
}

// FUNCTION: XWA 0x4B2990
static char paiman_avoidstarshipmaneuver(void) { return 0; }

// FUNCTION: XWA 0x4B3F10
void paiman_initbackupmaneuver(void) {
	uint16_t throttle;
	Q16Angle pitch;

	g_curCraft->aiFlight.diveState = 0;
	g_curCraft->aiFlight.climbState = 0;
	g_paiContext.aiController->targetZAngle = 0x4000;
	g_curCraft->aiFlight.headingStep = -1;
	g_curCraft->aiFlight.headingForce = 0;

	pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
	if (pitch < 0x4000u) {
		g_curCraft->aiFlight.headingState = 2;
	} else if (pitch > 0x4000u) {
		g_curCraft->aiFlight.headingState = 1;
	} else {
		g_curCraft->aiFlight.headingState = 3;
	}

	g_curCraft->aiFlight.enterFlag = 1;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = 0;
	g_curCraft->aiFlight.turnState = 0;
	g_objectTable[g_paiContext.aiObjIdx].mobj->motionFlags = 1;

	g_paiContext.aiController->aiPlanState =
		236 * Mission_DecodeOrderTime(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
										  .fg
										  .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
												  g_paiContext.curOrderCoord.fields.orderSlot]
										  .variable1);
	g_paiContext.aiController->aimPointX = g_objectTable[g_paiContext.aiObjIdx].world_x;
	g_paiContext.aiController->aimPointY = g_objectTable[g_paiContext.aiObjIdx].world_y;
	g_paiContext.aiController->aimPointZ = g_objectTable[g_paiContext.aiObjIdx].world_z;

	throttle = g_orderThrottleToCraftThrottleSpeed
		[g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			 .fg
			 .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					 g_paiContext.curOrderCoord.fields.orderSlot]
			 .throttle];
	if (throttle == 0) {
		throttle = 0xffff;
	}

	g_curCraft->throttleSpeed = throttle;
}

// FUNCTION: XWA 0x4B37B0
void paiman_initreleasemaneuver(void) {
	uint16_t pitch;

	if (g_curCraft->carriedObjectIndex != 0xffffu) {
		g_paiContext.aiController->orderScratch.goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
															[g_paiContext.curOrderCoord.fields.orderSlot] = 0;
	}

	pai_CalcAnglesToAimPoint();
	if (trig2_polardistance == 0) {
		g_paiContext.aiController->maneuverDist = 0;
		g_curCraft->throttleSpeed = 0;
		g_curCraft->aiFlight.diveState = 0;
		g_curCraft->aiFlight.climbState = 0;
		g_paiContext.aiController->targetZAngle = 0x4000;
		g_curCraft->aiFlight.headingStep = -1;
		g_curCraft->aiFlight.headingForce = 0;
		pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
		if (pitch < 0x4000u) {
			g_curCraft->aiFlight.headingState = 2;
		} else if (pitch > 0x4000u) {
			g_curCraft->aiFlight.headingState = 1;
		} else {
			g_curCraft->aiFlight.headingState = 3;
		}
		g_curCraft->aiFlight.enterFlag = 1;
		g_curCraft->aiFlight.rollStep = -1;
		g_curCraft->aiFlight.rollAccel = -1;
		g_paiContext.aiController->targetRoll = 0;
		g_curCraft->aiFlight.turnState = 0;
		g_paiContext.aiController->maneuverPhase = 2;
		return;
	}

	if (trig2_polardistance < 2048) {
		g_paiContext.aiController->maneuverDist = 0;
		g_curCraft->throttleSpeed = 0;
		g_curCraft->aiFlight.diveState = 0;
		g_curCraft->aiFlight.climbState = 0;
		g_paiContext.aiController->targetZAngle = 0x4000;
		g_curCraft->aiFlight.headingStep = -1;
		g_curCraft->aiFlight.headingForce = 0;
		pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
		if (pitch < 0x4000u) {
			g_curCraft->aiFlight.headingState = 2;
		} else if (pitch > 0x4000u) {
			g_curCraft->aiFlight.headingState = 1;
		} else {
			g_curCraft->aiFlight.headingState = 3;
		}
		g_curCraft->aiFlight.enterFlag = 1;
		g_curCraft->aiFlight.rollStep = -1;
		g_curCraft->aiFlight.rollAccel = -1;
		g_paiContext.aiController->targetRoll = 0;
		g_curCraft->aiFlight.turnState = 0;
		g_paiContext.aiController->maneuverPhase = 1;
		return;
	}

	g_paiContext.aiController->maneuverDist =
		ModelBounds_GetSizeZ((uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType);
	if (g_curCraft->carriedObjectIndex != 0xffffu) {
		g_paiContext.aiController->maneuverDist +=
			ModelBounds_GetSizeZ((uint16_t)g_objectTable[g_curCraft->carriedObjectIndex].objectType);
	}

	paiman_setflighttotarget(0, 1);
	g_paiContext.aiController->maneuverPhase = 0;
}

// FUNCTION: XWA 0x4AB8E0
void paiman_UpdateTurnInsideHeading(unsigned int fallbackObjIdx) {
	unsigned int turnStep;

	if (g_curCraft->lastAttackerObjIdx != 0xffffu) {
		if ((g_paiContext.aiController->maneuverPhase & 1) != 0) {
			g_paiContext.aiController->targetXYAngle =
				(uint16_t)(g_objectTable[g_curCraft->lastAttackerObjIdx].yaw - 0x4000);
		} else {
			g_paiContext.aiController->targetXYAngle =
				(uint16_t)(g_objectTable[g_curCraft->lastAttackerObjIdx].yaw + 0x4000);
		}
	} else {
		if ((g_paiContext.aiController->maneuverPhase & 1) != 0) {
			g_paiContext.aiController->targetXYAngle = (uint16_t)(g_objectTable[fallbackObjIdx].yaw - 0x4000);
		} else {
			g_paiContext.aiController->targetXYAngle = (uint16_t)(g_objectTable[fallbackObjIdx].yaw + 0x4000);
		}
	}

	turnStep = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
	turnStep = (turnStep >> 1) + 0x8000;
	paiman_setturn(turnStep);
	g_paiContext.aiController->aiPlanState = 236 * g_aiTurnAwayStateDelayBySkill[g_paiContext.aiSkillTier];
}

// FUNCTION: XWA 0x4ACDE0
void paiman_initsetupattackmaneuver(void) {
	g_curCraft->throttleSpeed = (uint16_t)-1;
	paiman_attacktarget(0, 0);
}

// FUNCTION: XWA 0x4ACE00
static char paiman_setupattackmaneuver(void) {
	uint16_t yawDelta;

	paiman_attacktarget(0, 0);
	yawDelta =
		(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - g_paiContext.aiController->targetXYAngle);
	if (yawDelta >= 0x3000u && yawDelta <= 0xd000u) {
		g_curCraft->throttleSpeed = 0x8000;
	} else {
		g_curCraft->throttleSpeed = (uint16_t)-1;
	}
	return 0;
}

static __inline int paiman_LocalDotQ15UnsignedShift(int dx, int dy, int dz, int16_t axisX, int16_t axisY,
													int16_t axisZ) {
	return Xwa_Dot3Q15ReuseXSlot(dx, dy, dz, axisX, axisY, axisZ);
}

// FUNCTION: XWA 0x4B4EB0
void paiman_initworkonmaneuver(void) {
	int targetObjIdx;
	ObjectRecord* targetObj;
	ObjectRecord* selfObj;
	MobileObject* targetMobj;
	CraftData* targetCraft;
	int deltaX;
	int deltaZ;
	int deltaY;
	int meshIdx;
	int vertexIdx;
	int vertexX;
	int vertexY;
	int vertexZ;
	const XwaOrder* order;

	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	targetObj = &g_objectTable[targetObjIdx];
	selfObj = &g_objectTable[g_paiContext.aiObjIdx];
	deltaY = selfObj->world_y - targetObj->world_y;
	deltaX = selfObj->world_x - targetObj->world_x;
	deltaZ = selfObj->world_z - targetObj->world_z;
	targetMobj = targetObj->mobj;
	targetCraft = targetMobj->pCraft;

	if (targetMobj != NULL) {
		if (targetMobj->orientMatrixDirty) {
			FVIEW_calcrotatemove(targetObj->pitch, targetObj->yaw, targetObj);
			FVIEW_calcrotateorient(targetObj->roll, targetObj->angleD, targetObj);
		}

		g_rotatedX = Xwa_Dot3Q15ReuseXSlot(deltaX, deltaY, deltaZ, targetMobj->cachedSideX,
										   targetMobj->cachedSideY, targetMobj->cachedSideZ);
		g_rotatedY = -Xwa_Dot3Q15ReuseXSlot(deltaX, deltaY, deltaZ, targetMobj->cachedFwdX,
											targetMobj->cachedFwdY, targetMobj->cachedFwdZ);
		g_rotatedZ = Xwa_Dot3Q15ReuseXSlot(deltaX, deltaY, deltaZ, targetMobj->cachedUpX,
										   targetMobj->cachedUpY, targetMobj->cachedUpZ);

		order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					 .fg.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot];
		meshIdx = ModelMesh_FindNearestLiveComponentByType((uint16_t)targetObj->objectType,
														   (MeshType)order->variable2, g_rotatedX, g_rotatedY,
														   g_rotatedZ, targetCraft);
		if (meshIdx == -1) {
			meshIdx =
				ModelMesh_FindNearestLiveComponentByType((uint16_t)targetObj->objectType, MESH_Default,
														 g_rotatedX, g_rotatedY, g_rotatedZ, targetCraft);
		}

		vertexIdx =
			ModelMesh_FindNearestVertexForPoint((uint16_t)targetObj->objectType, g_rotatedX, g_rotatedY,
												g_rotatedZ, meshIdx, 0, NULL, NULL, targetCraft);
		vertexX = ModelMesh_GetVertexX((uint16_t)targetObj->objectType, meshIdx, vertexIdx);
		vertexY = -ModelMesh_GetVertexY((uint16_t)targetObj->objectType, meshIdx, vertexIdx);
		vertexZ = ModelMesh_GetVertexZ((uint16_t)targetObj->objectType, meshIdx, vertexIdx);

		g_rotatedX = Xwa_Dot3Q15ReuseXSlot(vertexX, vertexY, vertexZ, targetMobj->cachedSideX,
										   targetMobj->cachedFwdX, targetMobj->cachedUpX);
		g_rotatedY = Xwa_Dot3Q15ReuseXSlot(vertexX, vertexY, vertexZ, targetMobj->cachedSideY,
										   targetMobj->cachedFwdY, targetMobj->cachedUpY);
		g_rotatedZ = Xwa_Dot3Q15ReuseXSlot(vertexX, vertexY, vertexZ, targetMobj->cachedSideZ,
										   targetMobj->cachedFwdZ, targetMobj->cachedUpZ);

		g_paiContext.aiController->aimPointX = g_rotatedX + targetObj->world_x;
		g_paiContext.aiController->aimPointY = g_rotatedY + targetObj->world_y;
		g_paiContext.aiController->aimPointZ = g_rotatedZ + targetObj->world_z;
		pai_CalcAnglesToAimPoint();
		if ((int)trig2_polardistance < 2048) {
			Q16Angle pitch;

			g_curCraft->throttleSpeed = 0;
			g_curCraft->aiFlight.diveState = 0;
			g_curCraft->aiFlight.climbState = 0;
			g_paiContext.aiController->targetZAngle = 0x4000;
			g_curCraft->aiFlight.headingStep = -1;
			g_curCraft->aiFlight.headingForce = 0;

			pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
			if (pitch < 0x4000u) {
				g_curCraft->aiFlight.headingState = 2;
			} else if (pitch > 0x4000u) {
				g_curCraft->aiFlight.headingState = 1;
			} else {
				g_curCraft->aiFlight.headingState = 3;
			}

			g_curCraft->aiFlight.enterFlag = 1;
			g_curCraft->aiFlight.rollStep = -1;
			g_curCraft->aiFlight.rollAccel = -1;
			g_paiContext.aiController->targetRoll = 0;
			g_curCraft->aiFlight.turnState = 0;
			g_paiContext.aiController->maneuverPhase = 1;
		} else {
			paiman_setflighttotarget(0, 1);
			g_paiContext.aiController->maneuverPhase = 0;
		}
	}
}

// FUNCTION: XWA 0x4ACE60
void paiman_initattackmaneuver(void) {
	uint16_t targetObjIdx;
	ObjectRecord* targetObj;

	g_paiContext.aiController->targetComponent = 0xffffu;
	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	targetObj = &g_objectTable[targetObjIdx];

	if (g_objectTable[targetObjIdx].mobj != NULL) {
		ModelGenusId genusId;

		genusId = targetObj->genusId;
		if (genusId == GENUS_Platform || genusId == GENUS_Starship || genusId == GENUS_Freighter) {
			MeshType meshTypeFilter;
			MeshType fallbackMeshType;
			MobileObject* targetMobj;
			CraftData* targetCraft;
			int dx;
			int dy;
			int dz;
			int localSide;
			int localFwd;
			int localUp;
			uint16_t targetComponent;

			meshTypeFilter = MESH_MainHull;
			fallbackMeshType = MESH_Default;
			if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "capfreeldr1pln") == 0) {
				meshTypeFilter =
					(MeshType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.variable1;
				fallbackMeshType =
					(MeshType)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.variable2;
			}
			if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "playercapldr2pln") == 0 &&
				g_curCraft->playerCommandCraftTypeFilter != 0) {
				meshTypeFilter = (MeshType)(g_curCraft->playerCommandCraftTypeFilter - 1);
			}

			dx = g_paiContext.aiCurrentPointX - targetObj->world_x;
			dy = g_paiContext.aiCurrentPointY - targetObj->world_y;
			dz = g_paiContext.aiCurrentPointZ - targetObj->world_z;
			targetMobj = targetObj->mobj;
			targetCraft = targetMobj->pCraft;
			if (targetMobj->orientMatrixDirty) {
				FVIEW_calcrotatemove(targetObj->pitch, targetObj->yaw, targetObj);
				FVIEW_calcrotateorient(targetObj->roll, targetObj->angleD, targetObj);
			}

			localSide = paiman_LocalDotQ15UnsignedShift(dx, dy, dz, targetMobj->cachedSideX,
														targetMobj->cachedSideY, targetMobj->cachedSideZ);
			localFwd = -paiman_LocalDotQ15UnsignedShift(dx, dy, dz, targetMobj->cachedFwdX,
														targetMobj->cachedFwdY, targetMobj->cachedFwdZ);
			localUp = paiman_LocalDotQ15UnsignedShift(dx, dy, dz, targetMobj->cachedUpX,
													  targetMobj->cachedUpY, targetMobj->cachedUpZ);

			targetComponent = 0xffff;
			if (meshTypeFilter != MESH_Default) {
				targetComponent = ModelMesh_FindNearestLiveComponentByType(
					(uint16_t)g_objectTable[targetObjIdx].objectType, meshTypeFilter, localSide, localFwd,
					localUp, targetCraft);
			}
			if (targetComponent == 0xffff && fallbackMeshType != MESH_Default) {
				targetComponent = ModelMesh_FindNearestLiveComponentByType(
					(uint16_t)g_objectTable[targetObjIdx].objectType, fallbackMeshType, localSide, localFwd,
					localUp, targetCraft);
			}
			g_paiContext.aiController->targetComponent = (uint16_t)targetComponent;
		}
	}

	paiman_attacktarget(0, 0);
	g_curCraft->throttleSpeed = (uint16_t)-1;
}

// FUNCTION: XWA 0x4AD170
static char paiman_attackmaneuver(void) {
	unsigned int targetObjIdx;
	uint16_t reactionThreshold;

	g_curCraft->pushAccumX = 0;
	g_curCraft->pushAccumY = 0;
	g_curCraft->pushAccumZ = 0;
	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	reactionThreshold = g_modelDefs[g_curCraft->modelIndex].field_13;

	if (g_paiContext.aiController->maneuverPhase != 0) {
		if (g_paiContext.aiController->maneuverPhase == 2) {
			if (g_paiContext.aiController->maneuverTimer == 0) {
				return 1;
			}
			if (g_paiContext.aiController->aiPlanState == 0 && g_curCraft->aiFlight.reactionTimer != 0) {
				g_curCraft->aiFlight.turnState = 2;
				g_curCraft->aiFlight.turnStep = 0xffffu;
				g_curCraft->aiFlight.turnAccel = (int16_t)0x8000;
				g_paiContext.aiController->targetXYAngle =
					(uint16_t)(g_paiContext.aiController->targetXYAngle - 0x2000u);
				g_paiContext.aiController->aiPlanState =
					(int)((uint16_t)MATH2_fraction(GameRand(), 0x00ecu) + 236u);
				g_paiContext.aiController->maneuverPhase = 1;
			}
		} else if (g_paiContext.aiController->maneuverPhase == 1) {
			if (g_paiContext.aiController->maneuverTimer == 0) {
				return 1;
			}
			if (g_paiContext.aiController->aiPlanState == 0 && g_curCraft->aiFlight.reactionTimer != 0) {
				g_curCraft->aiFlight.turnState = 2;
				g_curCraft->aiFlight.turnStep = 0xffffu;
				g_curCraft->aiFlight.turnAccel = (int16_t)0x8000;
				g_paiContext.aiController->targetXYAngle =
					(uint16_t)(g_paiContext.aiController->targetXYAngle + 0x2000u);
				g_paiContext.aiController->aiPlanState =
					(int)((uint16_t)MATH2_fraction(GameRand(), 0x00ecu) + 236u);
				g_paiContext.aiController->maneuverPhase = 2;
			}
		}
		return 0;
	}

	{
		unsigned int rangeToTarget;
		int rangeLimit;

		pai_CalcAnglesToAimPoint();
		rangeToTarget = (unsigned int)trig2_polardistance;
		paiorder_UpdateInspectionVisibility(g_paiContext.aiObjIdx, targetObjIdx,
											(unsigned int)trig2_polardistance);
		if (g_paiContext.aiController->targetObjIdx >= g_activeRegionCraftObjectSlotEnd) {
			rangeLimit = 5120;
		} else {
			ModelGenusId targetGenus;

			targetGenus = g_objectTable[targetObjIdx].genusId;
			if (targetGenus == GENUS_Starship || targetGenus == GENUS_Platform ||
				targetGenus == GENUS_Container || targetGenus == GENUS_Freighter) {
				if (g_paiContext.aiController->targetComponent != 0xffffu) {
					rangeLimit =
						ModelMesh_GetComponentMaxExtent((uint16_t)g_objectTable[targetObjIdx].objectType,
														g_paiContext.aiController->targetComponent) /
						2;
				} else {
					rangeLimit = ModelBounds_GetMaxExtent((uint16_t)g_objectTable[targetObjIdx].objectType);
					if (rangeLimit >= 0x2000) {
						rangeLimit = 0x2000;
					}
				}
				if (g_objectTable[targetObjIdx].objectType == OBJ_SuperStarDestroyer) {
					rangeLimit += 0x8000;
				}
				if (g_objectTable[targetObjIdx].objectType == OBJ_RepairYard) {
					rangeLimit += 0x8000;
				}
				if (g_objectTable[targetObjIdx].objectType == OBJ_ShipYard) {
					rangeLimit += 0x8000;
				}
			} else {
				uint8_t groupAI;
				uint16_t yawDelta;
				uint16_t pitchDelta;

				groupAI = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.groupAI;
				if (groupAI == 5) {
					rangeLimit = 1536;
				} else if (groupAI == 4) {
					rangeLimit = 2304;
				} else if (groupAI == 3) {
					rangeLimit = 3072;
				} else if (groupAI == 2) {
					rangeLimit = 4096;
				} else {
					rangeLimit = 5120;
				}
				yawDelta =
					(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - g_objectTable[targetObjIdx].yaw);
				if (yawDelta >= 0x8000u) {
					yawDelta = (uint16_t)(0u - yawDelta);
				}
				pitchDelta = (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].pitch -
										g_objectTable[targetObjIdx].pitch);
				if (pitchDelta >= 0x8000u) {
					pitchDelta = (uint16_t)(0u - pitchDelta);
				}
				if (pitchDelta > 0x4000u || yawDelta > 0x4000u) {
					rangeLimit <<= 1;
				}
			}
		}

		if ((g_curCraft->systemFlags & 1u) != 0) {
			int maxShield;

			maxShield = Craft_GetObjectMaxShield((unsigned short)g_paiContext.aiObjIdx);
			if (g_curCraft->shieldFront < maxShield / 10) {
				if ((uint32_t)g_curCraft->hullDamage >= (uint32_t)g_curCraft->systemDamageHullThreshold) {
					reactionThreshold >>= 1;
				} else {
					--reactionThreshold;
				}
			}
		}

		if ((int)trig2_polardistance >= rangeLimit &&
			g_curCraft->aiFlight.reactionTimer < reactionThreshold) {
			if (g_paiContext.aiController->aiPlanState == 0) {
				g_paiContext.aiController->aiPlanState = 472;
			}
			if (g_curCraft->aiFlight.reactionTimer != 0 &&
				(g_objectTable[targetObjIdx].genusId == GENUS_Starship ||
				 g_objectTable[targetObjIdx].genusId == GENUS_Platform)) {
				if (g_paiContext.aiController->aiPlanState < 236) {
					paiman_attacktarget(0x0800, 0x0800);
				} else {
					paiman_attacktarget(0xf800, 0xf800);
				}
			} else {
				paiman_attacktarget(0, 0);
			}

			if (g_paiContext.aiController->maneuverMode == 12) {
				uint16_t yawDelta;

				yawDelta = (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw -
									  g_paiContext.aiController->targetXYAngle);
				if (yawDelta >= 0x3000u && yawDelta <= 0xd000u) {
					g_curCraft->throttleSpeed = (uint16_t)-21846;
				} else if (rangeToTarget > 0x8000u) {
					g_curCraft->throttleSpeed = (uint16_t)-1;
				} else {
					ObjectRecord* targetObj;
					MobileObject* targetMobj;

					targetObj = &g_objectTable[g_paiContext.aiController->targetObjIdx];
					targetMobj = targetObj->mobj;
					if (targetMobj == NULL) {
						g_curCraft->throttleSpeed = 0x8000;
					} else if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								   .fg.groupAI >= 4u) {
						yawDelta = (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - targetObj->yaw);
						if (yawDelta >= 0x3000u && yawDelta <= 0xd000u) {
							g_curCraft->throttleSpeed = (uint16_t)-1;
						} else {
							uint16_t desiredSpeed;
							uint16_t maxSpeed;

							if (strcmp(g_planTable[g_paiContext.aiController->pendingPlanId].name,
									   "inspectldr2pln") == 0 ||
								strcmp(g_planTable[g_paiContext.aiController->pendingPlanId].name,
									   "playerinspectldr2pln") == 0) {
								desiredSpeed =
									(uint16_t)(targetMobj->speed + (rangeToTarget > 0x4000u ? 60u : 30u));
							} else {
								desiredSpeed =
									(uint16_t)(targetMobj->speed + (rangeToTarget > 0x4000u ? 20u : 0u));
								if (desiredSpeed < 10u) {
									desiredSpeed = 10;
								}
							}
							maxSpeed = (uint16_t)g_curCraft->aiFlight.maxSpeedCache;
							if (desiredSpeed >= maxSpeed) {
								g_curCraft->throttleSpeed = (uint16_t)-1;
							} else {
								g_curCraft->throttleSpeed = (uint16_t)MATH2_divide(desiredSpeed, maxSpeed);
							}
						}
					} else {
						g_curCraft->throttleSpeed = (uint16_t)-1;
					}
				}
			} else {
				g_curCraft->throttleSpeed = (uint16_t)-16384;
			}

			if (g_curCraft->waveNumber != 0) {
				unsigned int scanObjIdx;
				int nearestObjIdx;
				uint8_t nearestWave;

				nearestWave = 0xffu;
				nearestObjIdx = 0xffff;
				for (scanObjIdx = g_activeRegionObjectSlotStart;
					 scanObjIdx < g_activeRegionCraftObjectSlotEnd; ++scanObjIdx) {
					ObjectRecord* scanObj;
					CraftData* scanCraft;

					scanObj = &g_objectTable[scanObjIdx];
					if (g_paiContext.aiObjIdx != scanObjIdx && scanObj->objectType != OBJ_None &&
						scanObj->flightGroupIdx == g_paiContext.curOrderCoord.fields.flightGroupIdx) {
						scanCraft = scanObj->mobj->pCraft;
						if (g_curCraft->waveNumber > scanCraft->waveNumber) {
							pai_ObjectRefUpdateApproxRangeScore(g_paiContext.aiObjIdx, scanObjIdx);
							if ((unsigned int)g_targetRangeScore < 1000u &&
								scanCraft->waveNumber < nearestWave) {
								nearestWave = scanCraft->waveNumber;
								nearestObjIdx = (int)scanObjIdx;
							}
						}
					}
				}

				if (nearestObjIdx != 0xffff) {
					g_curCraft->pushAccumX =
						g_objectTable[g_paiContext.aiObjIdx].world_x - g_objectTable[nearestObjIdx].world_x;
					g_curCraft->pushAccumY =
						g_objectTable[g_paiContext.aiObjIdx].world_y - g_objectTable[nearestObjIdx].world_y;
					g_curCraft->pushAccumZ = 2 * (g_objectTable[g_paiContext.aiObjIdx].world_z -
												  g_objectTable[nearestObjIdx].world_z);
				}
			}
			return 0;
		}
	}

	if ((int16_t)g_curCraft->warheadLockTicks > 0) {
		ModelGenusId targetGenus;

		targetGenus = g_objectTable[targetObjIdx].genusId;
		if (targetGenus == GENUS_Starship || targetGenus == GENUS_Platform ||
			targetGenus == GENUS_Freighter || targetGenus == GENUS_Container ||
			(targetGenus == GENUS_Transport && g_missionFormatVersion >= 14)) {
			if (!Object_IsHostileToTeam((uint16_t)g_paiContext.aiObjIdx,
										(uint16_t)g_players[g_localPlayer].playerIff)) {
				if (g_curCraft->aiFlight.reactionTimer >= reactionThreshold) {
					fsfx_speakorderack(g_localPlayer, (int)g_paiContext.aiObjIdx, 3, -1, 0xffffu, 0xffffu);
				}
				fsfx_speakorderack(g_localPlayer, (int)g_paiContext.aiObjIdx, 10, -1, 0xffffu, 0xffffu);
			}
		}
	}

	g_curCraft->aiFlight.maneuverCounter = 0;
	g_curCraft->warheadLockTicks = 0;
	if (g_curCraft->aiFlight.reactionTimer >= reactionThreshold &&
		g_curCraft->aiFlight.threatObjIdx != 0xffffu) {
		int threatObjIdx;
		ObjectRecord* threatObj;

		threatObjIdx = g_curCraft->aiFlight.threatObjIdx;
		threatObj = &g_objectTable[threatObjIdx];
		if (threatObj->genusId == GENUS_Fighter) {
			uint16_t yawDelta;
			uint16_t pitchDelta;

			yawDelta = (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - threatObj->yaw);
			if (yawDelta >= 0x8000u) {
				yawDelta = (uint16_t)(0u - yawDelta);
			}
			pitchDelta = (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].pitch - threatObj->pitch);
			if (pitchDelta >= 0x8000u) {
				pitchDelta = (uint16_t)(0u - pitchDelta);
			}
			if (yawDelta >= 0x3000u || pitchDelta >= 0x3000u) {
				g_paiContext.aiController->maneuverMode =
					g_aiUnderAttackFrontManeuverChoices[GameRand() & 3u];
			} else {
				g_paiContext.aiController->maneuverMode =
					g_aiUnderAttackSideRearManeuverChoices[GameRand() & 7u];
				if (g_curCraft->cmTypeId == 2 && g_curCraft->cmAmmoCount != 0 &&
					g_curCraft->cmFireCooldownTimer == 0) {
					laser_createcountermeasureprojectile(g_paiContext.aiObjIdx, OBJ_WarheadFlare);
				}
			}

			g_curCraft->lastAttackerObjIdx = g_curCraft->aiFlight.threatObjIdx;
			g_objectTable[g_paiContext.aiObjIdx].mobj->motionFlags = 0;
			g_curCraft->pushAccumX = 0;
			g_curCraft->pushAccumY = 0;
			g_curCraft->pushAccumZ = 0;
			g_curCraft->aiFlight.reactionTimer = 0;
			g_curCraft->aiFlight.maneuverCounter = 0;
			g_curCraft->warheadLockTicks = 0;
			{
				const XwaOrder* order;

				order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							 .fg.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										g_paiContext.curOrderCoord.fields.orderSlot];
				g_curCraft->commandedSpeed = (int16_t)order->speed;
			}
			g_curCraft->commandedSpeed = (int16_t)(g_curCraft->commandedSpeed * 5);
			g_curCraft->aiFlight.enterFlag = 4;
			g_paiContext.aiController->maneuverPhase = 0;
			g_aiCurrentManeuverInitProc = g_maneuverInitTable[g_paiContext.aiController->maneuverMode];
			g_aiCurrentManeuverInitProc();
			g_curCraft->throttleSpeed = (uint16_t)-1;
			return 0;
		}
	}

	{
		int yawOffset;
		unsigned int turnStep;

		if (g_objectTable[targetObjIdx].genusId == GENUS_Platform) {
			yawOffset = (GameRand() & 0x3fff) + 12288;
		} else if (g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_PilotDroid) {
			yawOffset = (GameRand() & 0x0fff) + 28672;
		} else {
			yawOffset = (GameRand() & 0x3fff) + 0x4000;
		}
		if ((uint16_t)GameRand() >= 0x8000u) {
			yawOffset = -yawOffset;
		}

		g_paiContext.aiController->targetXYAngle =
			(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw + (uint16_t)yawOffset);
		turnStep = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
		turnStep = (turnStep >> 1) + 0x8000;
		paiman_setturn(turnStep);
		g_paiContext.aiController->targetZAngle = (uint16_t)(GameRand() & 0x7fff);
		g_curCraft->aiFlight.climbState = 0;
		g_curCraft->aiFlight.diveState = 0;
		if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
			g_curCraft->aiFlight.headingState = 1;
		} else {
			g_curCraft->aiFlight.headingState = 2;
		}
		g_curCraft->aiFlight.headingForce = 0;
		g_curCraft->aiFlight.headingStep = -1;
		g_curCraft->throttleSpeed = (uint16_t)-1;
	}

	if (g_paiContext.aiController->targetObjIdx >= g_activeRegionCraftObjectSlotEnd) {
		g_paiContext.aiController->maneuverTimer = 236 * ((GameRand() & 3) + 2);
		g_paiContext.aiController->maneuverPhase = 1;
		return 0;
	}

	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	{
		ModelGenusId targetGenus;

		targetGenus = g_objectTable[targetObjIdx].genusId;
		if (targetGenus == GENUS_Starship || targetGenus == GENUS_Platform ||
			targetGenus == GENUS_Container || targetGenus == GENUS_Freighter) {
			g_paiContext.aiController->maneuverTimer = 236 * ((GameRand() & 7) + 20);
			targetGenus = g_objectTable[g_paiContext.aiController->targetObjIdx].genusId;
			if (targetGenus == GENUS_Container) {
				g_paiContext.aiController->maneuverTimer /= 2;
				g_paiContext.aiController->maneuverTimer -= 944;
				g_paiContext.aiController->maneuverPhase = 1;
				return 0;
			}
			if (targetGenus == GENUS_Platform) {
				g_paiContext.aiController->maneuverTimer -= 1888;
			}
			g_paiContext.aiController->maneuverPhase = 1;
			return 0;
		}
	}

	if (g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_PilotDroid) {
		g_paiContext.aiController->maneuverTimer = 236 * ((GameRand() & 3) + 7);
		g_paiContext.aiController->maneuverPhase = 1;
		return 0;
	}
	g_paiContext.aiController->maneuverTimer = 236 * ((GameRand() & 3) + 2);
	g_paiContext.aiController->maneuverPhase = 1;
	return 0;
}

// FUNCTION: XWA 0x4B5BE0
void paiman_attacktarget(uint16_t yawOffset, uint16_t pitchOffset) {
	uint16_t targetObjIdx;
	unsigned int targetComponent;
	unsigned int objectType;
	unsigned int aiObjIdx;
	uint16_t yawDelta;
	uint16_t pitch;

	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	if (targetObjIdx == 0xffffu) {
		return;
	}

	if (g_paiContext.aiController->maneuverMode != 23 && g_objectTable[targetObjIdx].mobj != NULL) {
		paiman_calcplanelead(targetObjIdx);
	} else {
		pai_UpdateAimPointFromOrderTarget();
		if (g_paiContext.aiController->targetComponent != 0xffffu) {
			targetComponent = g_paiContext.aiController->targetComponent;
			objectType = (uint16_t)g_objectTable[g_paiContext.aiController->targetObjIdx].objectType;
			if (g_objectTable[g_paiContext.aiController->targetObjIdx].mobj != NULL) {
				pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_paiContext.aiController->targetObjIdx],
													ModelMesh_GetCenterX(objectType, targetComponent),
													ModelMesh_GetCenterZ(objectType, targetComponent),
													-ModelMesh_GetCenterY(objectType, targetComponent));
				g_paiContext.aiController->aimPointX += g_rotatedX;
				g_paiContext.aiController->aimPointY += g_rotatedY;
				g_paiContext.aiController->aimPointZ += g_rotatedZ;
			}
		}
	}

	trig2_ctop(g_paiContext.aiController->aimPointX - g_objectTable[g_paiContext.aiObjIdx].world_x,
			   g_paiContext.aiController->aimPointY - g_objectTable[g_paiContext.aiObjIdx].world_y,
			   g_paiContext.aiController->aimPointZ - g_objectTable[g_paiContext.aiObjIdx].world_z);
	g_paiContext.aiController->targetXYAngle = (uint16_t)(trig2_xyangle + (uint16_t)yawOffset);

	if (g_curCraft->aiFlight.enterFlag != 2) {
		unsigned int turnStep;

		turnStep = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
		turnStep = (turnStep >> 1) + 0x8000;
		paiman_setturn(turnStep);
		aiObjIdx = g_paiContext.aiObjIdx;
	} else {
		yawDelta =
			(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - g_paiContext.aiController->targetXYAngle);
		aiObjIdx = g_paiContext.aiObjIdx;
		if (yawDelta > 0x8000u) {
			yawDelta = (uint16_t)(0u - yawDelta);
		}
		if (yawDelta >= 0x2000u || trig2_polardistance < 0x10000) {
			unsigned int turnStep;

			turnStep = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
			turnStep = (turnStep >> 1) + 0x8000;
			paiman_setturn(turnStep);
			aiObjIdx = g_paiContext.aiObjIdx;
		}
	}

	if (g_curCraft->aiFlight.enterFlag == 3) {
		g_curCraft->aiFlight.enterFlag = 0;
		aiObjIdx = g_paiContext.aiObjIdx;
	}

	pitch = (uint16_t)(targetPitch + (uint16_t)pitchOffset);
	if (pitch > 0x8000u) {
		pitch = 0x8000u;
	}

	if (pitch != g_objectTable[aiObjIdx].pitch) {
		g_paiContext.aiController->targetZAngle = pitch;
		g_curCraft->aiFlight.headingStep = -1;
		if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
			g_curCraft->aiFlight.headingState = 1;
		} else {
			g_curCraft->aiFlight.headingState = 2;
		}
		g_curCraft->throttleSpeed = (uint16_t)-1;
		g_curCraft->aiFlight.climbState = 0;
		g_curCraft->aiFlight.diveState = 0;
		g_curCraft->aiFlight.headingForce = 0;
	}
}

// FUNCTION: XWA 0x4B5E90
void paiman_calcplanelead(int targetObjIdx) {
	int targetWorldX;
	int aimX;
	int targetWorldY;
	int aimY;
	int targetWorldZ;
	int aimZ;
	int deltaX;
	int deltaY;
	int deltaZ;
	uint16_t leadFrames;
	uint16_t targetComponent;

	if (g_objectTable[targetObjIdx].mobj->speed == 0) {
		leadFrames = 0;
	} else {
		uint16_t projectileType;
		uint16_t projectileSpeed;
		uint16_t combinedSpeed;
		uint16_t targetSpeed;
		uint16_t yawDelta;
		uint16_t denom;
		uint16_t estimatedFrames;
		uint16_t skillValue;

		pai_ObjectRefDirectionToObjectRef(g_paiContext.aiObjIdx, g_paiContext.aiController->targetObjIdx);
		if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "disableldr1pln") == 0 &&
			g_paiContext.aiController->hasLiveTarget == 1) {
			projectileType = OBJ_LaserIon;
		} else {
			projectileType = g_curCraft->laserProjectileTypeId[0];
			if (projectileType <= OBJ_LaserRebel) {
				projectileType = OBJ_LaserRebel;
			}
		}

		projectileSpeed = g_projectileSpeedByType[projectileType - OBJ_LaserRebel];
		combinedSpeed = (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].mobj->speed + projectileSpeed);
		yawDelta = (uint16_t)(g_objectTable[targetObjIdx].yaw - g_objectTable[g_paiContext.aiObjIdx].yaw);
		targetSpeed = g_objectTable[targetObjIdx].mobj->speed;
		if (yawDelta >= 0x8000u) {
			yawDelta = (uint16_t)(0u - yawDelta);
		}

		if (yawDelta < 0x4000u) {
			combinedSpeed = (uint16_t)(combinedSpeed - trig2_cosinewordmult(targetSpeed, yawDelta));
		} else {
			combinedSpeed = (uint16_t)(combinedSpeed + trig2_cosinewordmult(targetSpeed, yawDelta));
		}

		denom = (uint16_t)(9 * combinedSpeed);
		combinedSpeed = (uint16_t)(combinedSpeed / 5);
		denom = (uint16_t)(2 * denom + combinedSpeed);
		if (denom == 0) {
			denom = 19;
		}

		estimatedFrames = (uint16_t)(trig2_polardistance / denom);
		estimatedFrames = (uint16_t)(estimatedFrames * g_simStepScale + (g_simStepScale >> 1));
		skillValue = pai_GetEffectiveSkillValue(g_curCraft);
		leadFrames = (uint16_t)MATH2_fraction(estimatedFrames, skillValue);
	}

	targetComponent = g_paiContext.aiController->targetComponent;

	if (targetComponent == 0xffffu) {
		targetWorldX = g_objectTable[targetObjIdx].world_x;
		targetWorldY = g_objectTable[targetObjIdx].world_y;
		targetWorldZ = g_objectTable[targetObjIdx].world_z;
		aimX = targetWorldX;
		aimY = targetWorldY;
		aimZ = targetWorldZ;
	} else {
		unsigned int objectType = (uint16_t)g_objectTable[targetObjIdx].objectType;
		int rotatedX;

		pai_RotateLocalVectorToWorldScratch(&g_objectTable[targetObjIdx],
											ModelMesh_GetCenterX(objectType, targetComponent),
											ModelMesh_GetCenterZ(objectType, targetComponent),
											-ModelMesh_GetCenterY(objectType, targetComponent));
		rotatedX = g_rotatedX;

		targetWorldX = g_objectTable[targetObjIdx].world_x;
		aimX = targetWorldX + rotatedX;
		targetWorldY = g_objectTable[targetObjIdx].world_y;
		aimY = targetWorldY + g_rotatedY;
		targetWorldZ = g_objectTable[targetObjIdx].world_z;
		aimZ = targetWorldZ + g_rotatedZ;
	}

	if (leadFrames != 0) {
		MobileObject* targetMobj;

		targetMobj = g_objectTable[targetObjIdx].mobj;
		deltaX = targetWorldX - targetMobj->prevWorldX;
		deltaY = targetWorldY - targetMobj->prevWorldY;
		deltaZ = targetWorldZ - targetMobj->prevWorldZ;
		deltaX *= (int)leadFrames;
		deltaY *= (int)leadFrames;
		deltaZ *= (int)leadFrames;
		g_paiContext.aiController->aimPointX = deltaX + aimX;
		g_paiContext.aiController->aimPointY = deltaY + aimY;
		g_paiContext.aiController->aimPointZ = deltaZ + aimZ;
	} else {
		g_paiContext.aiController->aimPointX = aimX;
		g_paiContext.aiController->aimPointY = aimY;
		g_paiContext.aiController->aimPointZ = aimZ;
	}
}

// FUNCTION: XWA 0x4B6130
void paiman_calcformation(void) {
	uint16_t leaderObjIdx;
	ObjectRecord* leaderObj;
	CraftData* leaderCraft;
	uint8_t formationSlot;
	uint16_t formationType;
	int16_t formX;
	int16_t formY;
	int16_t formZ;
	uint16_t modelIndex;
	uint16_t separationScale;
	int16_t boundSizeX;
	int16_t boundSizeY;
	int16_t boundSizeZ;
	int16_t fwdArg;
	int16_t upArg;
	int16_t sideArg;
	int16_t divisor;

	if (g_curCraft->followPlayerMode == 0) {
		leaderObjIdx = (uint16_t)g_curCraft->leader_obj_idx;
		formationType = (uint8_t)g_curCraft->aiFlight.formationType;
		leaderObj = &g_objectTable[leaderObjIdx];
		leaderCraft = leaderObj->mobj->pCraft;
		formationSlot = g_curCraft->waveNumber;
	} else {
		leaderObjIdx = (uint16_t)g_players[g_curCraft->followPlayerIdx].objectIndex;
		if (leaderObjIdx == 0xffffu) {
			return;
		}
		formationSlot = g_curCraft->followFormationSlot;
		leaderObj = &g_objectTable[leaderObjIdx];
		leaderCraft = leaderObj->mobj->pCraft;
		formationType = (uint8_t)leaderCraft->aiFlight.formationType;
	}

	formX = (int16_t)(g_formPosX[formationType][formationSlot] - g_formPosX[formationType][0]);
	formY = (int16_t)(g_formPosY[formationType][formationSlot] - g_formPosY[formationType][0]);
	formZ = (int16_t)(g_formPosZ[formationType][formationSlot] - g_formPosZ[formationType][0]);

	modelIndex = leaderCraft->modelIndex;
	separationScale = (uint8_t)leaderCraft->aiFlight.separation + 1;
	boundSizeX = (int16_t)g_modelDefs[modelIndex].boundSizeX;
	boundSizeZ = (int16_t)g_modelDefs[modelIndex].boundSizeZ;
	boundSizeY = (int16_t)g_modelDefs[modelIndex].boundSizeY;
	fwdArg = separationScale * boundSizeY * formY;
	upArg = separationScale * boundSizeZ * formZ;
	sideArg = separationScale * boundSizeX * formX;

	if ((uint16_t)separationScale == 1) {
		sideArg += formX * (boundSizeX / 2);
		upArg += formZ * (boundSizeZ / 2);
		fwdArg += formY * (boundSizeY / 2);
	}

	divisor = g_formationDivisor[formationType];
	if (divisor != 1) {
		sideArg = (int16_t)sideArg / divisor;
		fwdArg = (int16_t)fwdArg / divisor;
		upArg = (int16_t)upArg / divisor;
	}

	pai_calcrotatedpoint(leaderObj, (int16_t)sideArg, (int16_t)upArg, (int16_t)fwdArg);
	if (g_modelDefs[modelIndex].boundSizeShift != 0) {
		uint16_t shift;

		shift = g_modelDefs[modelIndex].boundSizeShift;
		g_rotatedX <<= shift;
		g_rotatedY <<= shift;
		g_rotatedZ <<= shift;
	}

	g_curCraft->pushAccumX =
		g_rotatedX + g_objectTable[leaderObjIdx].world_x - g_objectTable[g_paiContext.aiObjIdx].world_x;
	g_curCraft->pushAccumY =
		g_rotatedY + g_objectTable[leaderObjIdx].world_y - g_objectTable[g_paiContext.aiObjIdx].world_y;
	g_curCraft->pushAccumZ =
		g_rotatedZ + g_objectTable[leaderObjIdx].world_z - g_objectTable[g_paiContext.aiObjIdx].world_z;
}

// FUNCTION: XWA 0x4AB860
int paiman_initturninsidemaneuver(void) {
	int maneuverTimer;

	g_paiContext.aiController->maneuverPhase = (char)(GameRand() & 1);
	paiman_UpdateTurnInsideHeading(g_paiContext.aiObjIdx);
	maneuverTimer = 236 * ((GameRand() & 7) + 10);
	g_paiContext.aiController->maneuverTimer = maneuverTimer;
	return maneuverTimer;
}

// FUNCTION: XWA 0x4AB8B0
static char paiman_turninsidemaneuver(void) {
	if (g_paiContext.aiController->maneuverTimer == 0) {
		return 1;
	}
	if (g_paiContext.aiController->aiPlanState == 0) {
		paiman_UpdateTurnInsideHeading(g_paiContext.aiObjIdx);
	}
	return 0;
}

// FUNCTION: XWA 0x4B2670
void paiman_initturnawaymaneuver(void) {
	unsigned int aiObjIdx;
	unsigned int effectiveSkillValue;

	aiObjIdx = g_paiContext.aiObjIdx;

	if (g_curCraft->lastAttackerObjIdx != 0xffffu) {
		g_paiContext.aiController->targetXYAngle = g_objectTable[g_curCraft->lastAttackerObjIdx].yaw;
	} else {
		g_paiContext.aiController->targetXYAngle = (uint16_t)(g_objectTable[aiObjIdx].yaw + 0x8000);
	}

	effectiveSkillValue = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
	paiman_setturn((effectiveSkillValue >> 1) + 0x8000);
	g_paiContext.aiController->aiPlanState = 236 * g_aiTurnAwayStateDelayBySkill[g_paiContext.aiSkillTier];
	g_paiContext.aiController->maneuverTimer = 3540;
}

// FUNCTION: XWA 0x4B2820
static char paiman_outofhangarmaneuver(void) {
	int maneuverTimer;

	maneuverTimer = g_paiContext.aiController->maneuverTimer;
	if (maneuverTimer == 0) {
		uint16_t fgIndex;
		int regionIdx;
		uint16_t order;
		uint8_t planId;
		uint8_t* exitHangarPlan;

		fgIndex = g_paiContext.curOrderCoord.fields.flightGroupIdx;
		regionIdx = g_paiContext.curOrderCoord.fields.regionIdx;
		order = g_missionFlightGroups[fgIndex].fg.orders[4 * regionIdx].order;
		if (g_curCraft->leader_obj_idx == -1) {
			planId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
		} else {
			planId = g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
		}
		pai_SetFlightGroupFormation(fgIndex, g_missionFlightGroups[fgIndex].fg.formation,
									g_missionFlightGroups[fgIndex].fg.formationSpacing);
		exitHangarPlan = pai_getplandataptrbyname("exithangarpln");
		exitHangarPlan[3] = planId;

		return 1;
	}

	if (maneuverTimer <= 1888 && g_paiContext.aiSelfCraft->sFoilState != 0) {
		g_paiContext.aiSelfCraft->sFoilState = 1;
	}
	return 0;
}

// FUNCTION: XWA 0x4ABBE0
void paiman_initscissorsmaneuver(void) {
	unsigned int turnStep;

	if (g_curCraft->lastAttackerObjIdx != 0xffffu) {
		g_paiContext.aiController->targetXYAngle =
			(uint16_t)(g_objectTable[g_curCraft->lastAttackerObjIdx].yaw + 0x8000);
	} else {
		g_paiContext.aiController->targetXYAngle =
			(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw + 0x8000);
	}

	turnStep = pai_GetEffectiveSkillValue(g_curCraft);
	paiman_setturn((uint16_t)((turnStep >> 1) + 0x8000));
	g_curCraft->aiFlight.enterFlag = 3;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = (uint16_t)GameRand();
	g_paiContext.aiController->maneuverTimer = 236 * ((GameRand() & 7) + 10);
	g_paiContext.aiController->aiPlanState = 472;
}

// FUNCTION: XWA 0x4ABCD0
static char paiman_scissorsmaneuver(void) {
	if (g_paiContext.aiController->maneuverTimer == 0) {
		g_curCraft->aiFlight.enterFlag = 4;
		return 1;
	}

	if (g_paiContext.aiController->aiPlanState == 0) {
		g_curCraft->aiFlight.turnState = 1;
		g_paiContext.aiController->targetXYAngle =
			(uint16_t)(g_paiContext.aiController->targetXYAngle + 0x8000u);
		g_paiContext.aiController->targetRoll ^= 0x8000u;
		g_paiContext.aiController->aiPlanState =
			(int)((uint16_t)MATH2_fraction((uint16_t)GameRand(), 0x00ecu) + 472u);

		if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.groupAI >= 3u &&
			((uint8_t)GameRand() & 3) == 3 && g_curCraft->cmTypeId == 2 && g_curCraft->cmAmmoCount != 0 &&
			g_curCraft->cmFireCooldownTimer == 0) {
			laser_createcountermeasureprojectile(g_paiContext.aiObjIdx, OBJ_WarheadFlare);
		}
	}
	return 0;
}

// FUNCTION: XWA 0x4B5AD0
CraftData* paiman_setflighttotarget(Q16Angle pitchBias, int driveHeading) {
	unsigned int effectiveSkillValue;
	unsigned int objIdx;
	CraftData* result;

	objIdx = (uint16_t)g_paiContext.aiObjIdx;
	trig2_ctop(g_paiContext.aiController->aimPointX - g_objectTable[objIdx].world_x,
			   g_paiContext.aiController->aimPointY - g_objectTable[objIdx].world_y,
			   g_paiContext.aiController->aimPointZ - g_objectTable[objIdx].world_z);
	g_paiContext.aiController->targetXYAngle = (uint16_t)(trig2_xyangle + (uint16_t)pitchBias);
	effectiveSkillValue = (uint16_t)pai_GetEffectiveSkillValue(g_curCraft);
	paiman_setturn((uint16_t)((effectiveSkillValue >> 1) + 0x4000));

	result = (CraftData*)(uintptr_t)driveHeading;
	if (result != NULL) {
		g_paiContext.aiController->targetZAngle = targetPitch;
		g_curCraft->aiFlight.headingStep = -1;
		g_curCraft->aiFlight.headingForce = 0;
		if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
			g_curCraft->aiFlight.headingState = 1;
		} else {
			g_curCraft->aiFlight.headingState = 2;
		}
		g_curCraft->aiFlight.climbState = 0;
		result = g_curCraft;
		g_curCraft->aiFlight.diveState = 0;
	}

	return result;
}

// FUNCTION: XWA 0x4B2610
CraftData* paiman_initheadtowardmaneuver(void) { return paiman_setflighttotarget(0, 1); }

// FUNCTION: XWA 0x4ABA00
static char paiman_splitsmaneuver(void) {
	if (g_curCraft->aiFlight.enterFlag == 4 && g_curCraft->aiFlight.headingState == 3) {
		return 1;
	} else {
		return 0;
	}
}

// FUNCTION: XWA 0x4AC780
void paiman_initheadtowardfullmaneuver(void) {
	paiman_setflighttotarget(0, 1);
	g_curCraft->throttleSpeed = (uint16_t)-1;
	g_paiContext.aiController->aiPlanState = 236;
	return;
}

// FUNCTION: XWA 0x4AC7B0
static char paiman_headtowardfullmaneuver(void) {
	if (g_paiContext.aiController->aiPlanState == 0) {
		pai_UpdateAimPointFromOrderTarget();
		paiman_setflighttotarget(0, 1);
		g_curCraft->throttleSpeed = (uint16_t)-1;
		g_paiContext.aiController->aiPlanState = 236;
	}
	return 0;
}

// FUNCTION: XWA 0x4B30F0
void paiman_initkamikazemaneuver(void) {
	pai_UpdateAimPointFromOrderTarget();
	paiman_setflighttotarget(0, 1);
	g_curCraft->throttleSpeed = (uint16_t)-1;
	return;
}

// FUNCTION: XWA 0x4AFD50
int paiman_initoutofhyperspacemaneuver(void) {
	g_curCraft->objectKind = 6;
	g_objectTable[g_paiContext.aiObjIdx].mobj->speed = 3600;
	g_paiContext.aiController->maneuverPhase = 0;
	g_paiContext.aiController->aiPlanState = 236;
	g_paiContext.aiController->targetSignature = 0;
	g_paiContext.aiController->hasLiveTarget = 0;

	if (g_objectTable[g_paiContext.aiObjIdx].mobj->framesAlive == 0) {
		g_paiContext.aiController->targetObjIdx = 0x8000;
		pai_UpdateAimPointFromOrderTarget();
	}

	g_paiContext.aiController->maneuverDist = 655360;
	g_paiContext.aiController->maneuverTimer = 2596;
	g_curCraft->aiFlight.objSignatures[0] = (uint16_t)g_paiContext.aiController->thinkInterval;
	g_paiContext.aiController->thinkInterval = 29;
	return Music_TriggerOutOfHyperspaceSequenceForObject(g_paiContext.aiObjIdx);
}

// FUNCTION: XWA 0x4B2730
static char paiman_turnawaymaneuver(void) {
	unsigned int aiObjIdx;
	unsigned int effectiveSkillValue;

	if (g_paiContext.aiController->maneuverTimer == 0) {
		return 1;
	}

	if (g_paiContext.aiController->aiPlanState == 0) {
		aiObjIdx = g_paiContext.aiObjIdx;

		if (g_curCraft->lastAttackerObjIdx != 0xffffu) {
			g_paiContext.aiController->targetXYAngle = g_objectTable[g_curCraft->lastAttackerObjIdx].yaw;
		} else {
			g_paiContext.aiController->targetXYAngle = (uint16_t)(g_objectTable[aiObjIdx].yaw + 0x8000);
		}

		effectiveSkillValue = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
		paiman_setturn((effectiveSkillValue >> 1) + 0x8000);
		g_paiContext.aiController->aiPlanState =
			236 * g_aiTurnAwayStateDelayBySkill[g_paiContext.aiSkillTier];
	}

	return 0;
}

// FUNCTION: XWA 0x4AE3F0
void paiman_initintohyperspacemaneuver(void) {
	int result;

	g_paiContext.aiController->maneuverPhase = 0;
	pai_CalcAnglesToAimPoint();
	if (trig2_polardistance == 0) {
		return;
	}

	if (g_curCraft->workingSubsystems == 0) {
		return;
	}

	if (g_curCraft->leader_obj_idx != -1) {
		paiman_calcformation();
		g_curCraft->pushAccumX = 0;
		g_curCraft->pushAccumY = 0;
		g_curCraft->pushAccumZ = 0;
		g_paiContext.aiController->aimPointX += g_rotatedX;
		g_paiContext.aiController->aimPointY += g_rotatedY;
		g_paiContext.aiController->aimPointZ += g_rotatedZ;
	}

	paiman_setflighttotarget(0, 1);
	result = strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "hyperspacepln");
	if (result == 0) {
		const XwaOrder* order;

		order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					 .fg.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot];
		result = g_orderThrottleToCraftThrottleSpeed[order->throttle];
		g_curCraft->throttleSpeed = (uint16_t)result;
		return;
	}

	g_curCraft->throttleSpeed = (uint16_t)-1;
	return;
}

// FUNCTION: XWA 0x4ABDB0
uint16_t paiman_initrendezvousmaneuver(void) {
	uint16_t throttle;

	paiman_setflighttotarget(0, 1);
	throttle = g_orderThrottleToCraftThrottleSpeed
		[g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			 .fg
			 .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					 g_paiContext.curOrderCoord.fields.orderSlot]
			 .throttle];
	if (throttle == 0) {
		throttle = 0xffff;
	}
	g_curCraft->throttleSpeed = throttle;
	return throttle;
}

// FUNCTION: XWA 0x4ABE20
static char paiman_rendezvousmaneuver(void) {
	uint16_t throttle;

	paiman_setflighttotarget(0, 1);
	throttle = g_orderThrottleToCraftThrottleSpeed
		[g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			 .fg
			 .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					 g_paiContext.curOrderCoord.fields.orderSlot]
			 .throttle];
	if (throttle == 0) {
		throttle = 0xffff;
	}
	g_curCraft->throttleSpeed = throttle;
	return 0;
}

// FUNCTION: XWA 0x4ABE90
void paiman_initcruisemaneuver(void) {
	unsigned int throttleSpeed;
	uint16_t throttle;

	g_curCraft->aiFlight.diveState = 0;
	g_curCraft->aiFlight.climbState = 0;
	g_paiContext.aiController->targetZAngle = 0x4000;
	g_curCraft->aiFlight.headingStep = -1;
	g_curCraft->aiFlight.headingForce = 0;

	if (g_objectTable[g_paiContext.aiObjIdx].pitch < 0x4000u) {
		g_curCraft->aiFlight.headingState = 2;
	} else if (g_objectTable[g_paiContext.aiObjIdx].pitch > 0x4000u) {
		g_curCraft->aiFlight.headingState = 1;
	} else {
		g_curCraft->aiFlight.headingState = 3;
	}

	g_curCraft->aiFlight.enterFlag = 1;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = 0;
	g_curCraft->aiFlight.turnState = 0;
	pai_CalcAnglesToAimPoint();
	g_paiContext.aiController->maneuverDist = trig2_polardistance;
	if (trig2_polardistance != 0) {
		if (g_objectTable[g_paiContext.aiObjIdx].roll < 0x8000u) {
			paiman_setflighttotarget(0, 1);
		}

		throttle = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					   .fg
					   .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							   g_paiContext.curOrderCoord.fields.orderSlot]
					   .throttle;
		throttleSpeed = g_orderThrottleToCraftThrottleSpeed[throttle];
		g_curCraft->throttleSpeed = throttleSpeed;
	} else {
		g_curCraft->throttleSpeed = 0;
		g_paiContext.aiController->maneuverPhase = 1;
	}
	g_paiContext.aiController->aiPlanState = 236;
}

// FUNCTION: XWA 0x4AC420
static void paiman_AdvanceOrderWaypoint(unsigned int objectIdx) {
	uint8_t planId;

	(void)objectIdx;

	planId = g_paiContext.aiController->currentPlanId;
	++g_paiContext.aiController->waypointIndex;
	if ((uint8_t)g_paiContext.aiController->waypointIndex == 8 ||
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg
				.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
						g_paiContext.curOrderCoord.fields.orderSlot]
				.waypoints[(uint8_t)g_paiContext.aiController->waypointIndex]
				.enabled == 0) {
		if (strcmp(g_planTable[planId].name, "release1pln") != 0 &&
			strcmp(g_planTable[planId].name, "deliverpln") != 0 &&
			strcmp(g_planTable[planId].name, "workon1pln") != 0) {
			g_paiContext.aiController->waypointIndex = 0;
			if (strcmp(g_planTable[planId].name, "formldr1pln") == 0 ||
				strcmp(g_planTable[planId].name, "formevadeldr1pln") == 0 ||
				strcmp(g_planTable[planId].name, "starshipformpln") == 0) {
				++g_paiContext.aiController->orderScratch
					  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
								   [g_paiContext.curOrderCoord.fields.orderSlot];
			}
		} else {
			--g_paiContext.aiController->waypointIndex;
			++g_paiContext.aiController->orderScratch
				  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
							   [g_paiContext.curOrderCoord.fields.orderSlot];
			return;
		}
	}

	g_paiContext.aiController->targetObjIdx =
		(uint16_t)(0x8004u + (uint8_t)g_paiContext.aiController->waypointIndex);
	g_paiContext.aiController->targetSignature = 0;
	g_paiContext.aiController->hasLiveTarget = 0;
	pai_UpdateAimPointFromOrderTarget();

	if ((strcmp(g_planTable[planId].name, "release1pln") == 0 ||
		 strcmp(g_planTable[planId].name, "deliverpln") == 0) &&
		g_curCraft->carriedObjectIndex != 0xffffu) {
		g_paiContext.aiController->aimPointZ +=
			ModelBounds_GetSizeZ((uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType);
		g_paiContext.aiController->aimPointZ +=
			ModelBounds_GetSizeZ((uint16_t)g_objectTable[g_curCraft->carriedObjectIndex].objectType);
	}
}

// FUNCTION: XWA 0x4AC030
static char paiman_cruisemaneuver(void) {
	AiController* aiController;
	unsigned int aiObjIdx;
	unsigned int initialObjIdx;
	int distance;
	int minDistance;
	int zDelta;
	uint16_t throttle;
	int throttleSpeed;
	uint16_t yawDelta;
	char oldWaypointIndex;

	if (g_paiContext.aiController->maneuverPhase != 0) {
		return 0;
	}

	pai_CalcAnglesToAimPoint();
	initialObjIdx = g_paiContext.aiObjIdx;
	if (g_objectTable[initialObjIdx].genusId == GENUS_PilotDroid) {
		minDistance = 466;
	} else if (g_objectTable[initialObjIdx].genusId == GENUS_Starship) {
		minDistance = 0x2000;
	} else {
		minDistance = 0x1000;
	}

	aiController = g_paiContext.aiController;
	distance = trig2_polardistance;
	if ((distance > aiController->maneuverDist && distance < 0x10000) || distance < minDistance) {
		oldWaypointIndex = aiController->waypointIndex;
		paiman_AdvanceOrderWaypoint(initialObjIdx);
		if (oldWaypointIndex == g_paiContext.aiController->waypointIndex) {
			if (g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_Starship ||
				g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_Freighter) {
				g_curCraft->aiFlight.turnState = 3;
				g_curCraft->throttleSpeed = 0;
				g_curCraft->pushAccumX =
					g_paiContext.aiController->aimPointX - g_objectTable[g_paiContext.aiObjIdx].world_x;
				g_curCraft->pushAccumY =
					g_paiContext.aiController->aimPointY - g_objectTable[g_paiContext.aiObjIdx].world_y;
				g_curCraft->pushAccumZ =
					g_paiContext.aiController->aimPointZ - g_objectTable[g_paiContext.aiObjIdx].world_z;
				return 0;
			}
			if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "workon1pln") == 0) {
				--g_paiContext.aiController->orderScratch
					  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
								   [g_paiContext.curOrderCoord.fields.orderSlot];
				return 1;
			}
		}
		pai_CalcAnglesToAimPoint();
		g_paiContext.aiController->maneuverDist = trig2_polardistance;
	} else {
		aiController->maneuverDist = distance;
	}

	if (g_paiContext.aiController->aiPlanState == 0) {
		if (g_curCraft->aiFlight.diveState != 1 && g_curCraft->aiFlight.climbState != 1) {
			zDelta = g_paiContext.aiController->aimPointZ - g_objectTable[g_paiContext.aiObjIdx].world_z;
			if (zDelta < 0) {
				zDelta = -zDelta;
			}
			if (zDelta > 512) {
				g_paiContext.aiController->targetZAngle = targetPitch;
				if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
					g_curCraft->aiFlight.headingState = 1;
				} else {
					g_curCraft->aiFlight.headingState = 2;
				}
				g_curCraft->aiFlight.climbState = 1;
				g_curCraft->throttleSpeed = (uint16_t)-16384;
			}
		}

		paiman_setflighttotarget(0, 1);
		g_paiContext.aiController->aiPlanState = 118;
		aiObjIdx = g_paiContext.aiObjIdx;
		if (g_curCraft->aiFlight.turnState == 3 && g_objectTable[g_paiContext.aiObjIdx].roll != 0 &&
			g_curCraft->aiFlight.enterFlag != 1) {
			g_curCraft->aiFlight.enterFlag = 1;
			g_curCraft->aiFlight.rollStep = -1;
			g_curCraft->aiFlight.rollAccel = 0x4000;
			g_paiContext.aiController->targetRoll = 0;
		}
	}
	aiObjIdx = g_paiContext.aiObjIdx;

	throttle = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				   .fg
				   .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
						   g_paiContext.curOrderCoord.fields.orderSlot]
				   .throttle;
	if (g_objectTable[aiObjIdx].genusId == GENUS_Starship) {
		yawDelta = (uint16_t)(g_objectTable[aiObjIdx].yaw - g_paiContext.aiController->targetXYAngle);
		if (yawDelta >= 0x8000u) {
			yawDelta = (uint16_t)(0u - yawDelta);
		}
		if (g_curCraft->aiFlight.turnState == 2 && yawDelta >= 0x1000u) {
			throttleSpeed = (uint16_t)MATH2_fraction(g_orderThrottleToCraftThrottleSpeed[throttle], 0xc000u);
			g_curCraft->throttleSpeed = (uint16_t)throttleSpeed;
			g_curCraft->aiFlight.enterFlag = 4;
			return 0;
		}
	}

	throttleSpeed = g_orderThrottleToCraftThrottleSpeed[throttle];
	g_curCraft->throttleSpeed = (uint16_t)throttleSpeed;
	return 0;
}

// FUNCTION: XWA 0x4AC900
CraftData* paiman_initheadonattackmaneuver(void) {
	CraftData* result;

	g_paiContext.aiController->targetObjIdx = g_curCraft->lastAttackerObjIdx;
	if (g_curCraft->lastAttackerObjIdx != 0xffffu) {
		g_paiContext.aiController->targetSignature =
			g_objectTable[g_curCraft->lastAttackerObjIdx].objectSignature;
	} else {
		g_paiContext.aiController->targetSignature = 0;
	}

	g_paiContext.aiController->hasLiveTarget = 1;
	result = paiman_setflighttotarget(0, 1);
	g_curCraft->throttleSpeed = (uint16_t)-1;
	g_paiContext.aiController->maneuverTimer = 1888;
	return result;
}

// FUNCTION: XWA 0x4AC990
static char paiman_headonattackmaneuver(void) {
	if (g_paiContext.aiController->maneuverTimer == 0) {
		return 1;
	}
	paiman_setflighttotarget(0, 1);
	g_curCraft->throttleSpeed = (uint16_t)-1;
	return 0;
}

// FUNCTION: XWA 0x4AC9C0
static char paiman_followleadermaneuver(void) {
	unsigned int leaderObjIdx;
	unsigned int selfObjIdx;
	unsigned int maxUint16;
	unsigned int skillValue;
	ObjectRecord* selfObj;
	Q16Angle* selfPitchPtr;
	CraftData* leaderCraft;
	uint16_t turnStep;
	uint16_t leaderSpeed;
	uint16_t selfSpeed;
	unsigned int oldThrottle;
	unsigned int speedDelta;
	uint16_t pitchDelta;
	uint16_t selfPitch;
	uint16_t leaderPitch;
	uint16_t leaderRoll;
	uint16_t rollDelta;
	ModelGenusId genusId;

	maxUint16 = 0xffffu;
	if (g_curCraft->followPlayerMode == 0) {
		leaderObjIdx = (unsigned int)g_curCraft->leader_obj_idx;
	} else {
		leaderObjIdx = (unsigned int)g_players[g_curCraft->followPlayerIdx].objectIndex;
		if (leaderObjIdx == maxUint16) {
			return 0;
		}
	}
	leaderCraft = g_objectTable[leaderObjIdx].mobj->pCraft;
	pai_ObjectRefDirectionToObjectRef(leaderObjIdx, g_paiContext.aiObjIdx);
	selfObjIdx = g_paiContext.aiObjIdx;
	selfObj = &g_objectTable[selfObjIdx];
	genusId = selfObj->genusId;

	if ((genusId == GENUS_Fighter && (int)trig2_polardistance > 0x2000) ||
		(genusId != GENUS_Starship && (int)trig2_polardistance > 0x10000) ||
		(int)trig2_polardistance > 0x50000) {
		g_paiContext.aiController->aimPointX = g_objectTable[leaderObjIdx].world_x;
		g_paiContext.aiController->aimPointY = g_objectTable[leaderObjIdx].world_y;
		g_paiContext.aiController->aimPointZ = g_objectTable[leaderObjIdx].world_z;
		paiman_setflighttotarget(0, 1);
		g_curCraft->throttleSpeed = maxUint16;
		g_curCraft->pushAccumX = 0;
		g_curCraft->pushAccumY = 0;
		g_curCraft->pushAccumZ = 0;
		return 0;
	}

	if (leaderCraft->aiFlight.turnState == 2) {
		g_paiContext.aiController->targetXYAngle = pai_GetEffectiveAIController(leaderCraft)->targetXYAngle;
		skillValue = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & maxUint16;
		turnStep = (uint16_t)((skillValue >> 2) + 0x4000u);
		paiman_setturn(turnStep);
		g_curCraft->aiFlight.turnAccel = -1;
	} else if (g_objectTable[leaderObjIdx].yaw != selfObj->yaw) {
		g_paiContext.aiController->targetXYAngle = g_objectTable[leaderObjIdx].yaw;
		skillValue = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & maxUint16;
		turnStep = (uint16_t)((skillValue >> 2) + 0x4000u);
		paiman_setturn(turnStep);
		if (g_objectTable[leaderObjIdx].playerOwnerIdx != -1) {
			g_curCraft->aiFlight.turnAccel = 0x4000;
		} else {
			g_curCraft->aiFlight.turnAccel = -1;
		}
	}

	selfObjIdx = g_paiContext.aiObjIdx;
	if (g_objectTable[leaderObjIdx].playerOwnerIdx != -1) {
		leaderSpeed = g_objectTable[leaderObjIdx].mobj->speed;
		selfSpeed = g_objectTable[selfObjIdx].mobj->speed;
		if (leaderSpeed > selfSpeed) {
			oldThrottle = g_curCraft->throttleSpeed;
			speedDelta = (50 * (unsigned int)leaderSpeed - 50 * (unsigned int)selfSpeed) & maxUint16;
			g_curCraft->throttleSpeed = (uint16_t)((oldThrottle & maxUint16) + speedDelta);
			if (g_curCraft->throttleSpeed < (uint16_t)oldThrottle) {
				g_curCraft->throttleSpeed = maxUint16;
			}
		} else if (leaderSpeed < selfSpeed) {
			oldThrottle = g_curCraft->throttleSpeed;
			speedDelta = (50 * (unsigned int)selfSpeed - 50 * (unsigned int)leaderSpeed) & maxUint16;
			g_curCraft->throttleSpeed = (uint16_t)((oldThrottle & maxUint16) - speedDelta);
			if (g_curCraft->throttleSpeed > (uint16_t)oldThrottle) {
				g_curCraft->throttleSpeed = 0;
			}
		}
	} else {
		oldThrottle = leaderCraft->throttleSpeed;
		g_curCraft->throttleSpeed = (uint16_t)oldThrottle;
	}

	selfObjIdx = g_paiContext.aiObjIdx;
	selfObj = &g_objectTable[selfObjIdx];
	selfPitchPtr = &selfObj->pitch;
	leaderPitch = g_objectTable[leaderObjIdx].pitch;
	selfPitch = *selfPitchPtr;
	pitchDelta = (uint16_t)(selfPitch - leaderPitch);
	if (pitchDelta >= 0x8000u) {
		pitchDelta = (uint16_t)(0u - pitchDelta);
	}
	if (pitchDelta < 0x400u) {
		*selfPitchPtr = leaderPitch;
		g_curCraft->aiFlight.headingState = 0;
	} else {
		g_paiContext.aiController->targetZAngle = leaderPitch;
		if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
			g_curCraft->aiFlight.headingState = 1;
		} else {
			g_curCraft->aiFlight.headingState = 2;
		}
		g_curCraft->aiFlight.headingStep = (int16_t)maxUint16;
		g_curCraft->aiFlight.headingForce = 0;
	}

	if (leaderCraft->aiFlight.impactObjIdx == (uint16_t)maxUint16 && g_curCraft->aiFlight.turnState != 2) {
		leaderRoll = g_objectTable[leaderObjIdx].roll;
		rollDelta = (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].roll - leaderRoll);
		if (rollDelta != 0) {
			g_paiContext.aiController->targetRoll = leaderRoll;
			g_curCraft->aiFlight.rollStep = (int16_t)maxUint16;
			if (g_curCraft->aiFlight.enterFlag != 1) {
				g_curCraft->aiFlight.rollAccel = 0x4000;
			}
			g_curCraft->aiFlight.enterFlag = 1;
		}
	}

	if (g_objectTable[leaderObjIdx].playerOwnerIdx == -1) {
		paiman_calcformation();
		return 0;
	}
	if (g_objectTable[leaderObjIdx].mobj->speed > 10u) {
		paiman_calcformation();
		return 0;
	}

	g_curCraft->pushAccumX = 0;
	g_curCraft->pushAccumY = 0;
	g_curCraft->pushAccumZ = 0;
	return 0;
}

// FUNCTION: XWA 0x4AC7F0
void paiman_initrunawaymaneuver(void) {
	uint16_t pitch;

	g_curCraft->aiFlight.diveState = 0;
	g_curCraft->aiFlight.climbState = 0;
	g_paiContext.aiController->targetZAngle = 0x4000;
	g_curCraft->aiFlight.headingStep = -1;
	g_curCraft->aiFlight.headingForce = 0;

	pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
	if (pitch < 0x4000u) {
		g_curCraft->aiFlight.headingState = 2;
	} else if (pitch > 0x4000u) {
		g_curCraft->aiFlight.headingState = 1;
	} else {
		g_curCraft->aiFlight.headingState = 3;
	}

	g_curCraft->aiFlight.enterFlag = 1;
	g_curCraft->aiFlight.rollStep = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_paiContext.aiController->targetRoll = 0;
	g_curCraft->aiFlight.turnState = 0;
	if (g_objectTable[g_paiContext.aiObjIdx].roll < 0x8000u) {
		paiman_setflighttotarget(0x8000, 1);
	}
}

// FUNCTION: XWA 0x4AC8E0
static char paiman_runawaymaneuver(void) {
	paiman_setflighttotarget(0x8000, 1);
	g_curCraft->throttleSpeed = (uint16_t)-1;
	return 0;
}

// FUNCTION: XWA 0x4AE0E0
void paiman_initspeedawaymaneuver(void) {
	unsigned int objIdx;
	int pushDelta;
	int yawOffset;
	unsigned int effectiveSkill;

	g_curCraft->throttleSpeed = (uint16_t)-1;
	g_paiContext.aiController->maneuverTimer = 4720;
	g_paiContext.aiController->targetXYAngle =
		(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw + (GameRand() & 0xffu));

	objIdx = g_paiContext.aiObjIdx;
	pushDelta = (GameRand() & 0x1f) + 50;
	yawOffset = (int)(uint8_t)GameRand() + 0x180;
	if (g_curCraft->pushAccumZ >= 0) {
		pushDelta = -pushDelta;
		yawOffset = -yawOffset;
	}

	g_curCraft->pushAccumZ = (uint16_t)pushDelta;
	g_paiContext.aiController->targetXYAngle = (uint16_t)(g_objectTable[objIdx].yaw + (uint16_t)yawOffset);
	effectiveSkill = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
	paiman_setturn((effectiveSkill >> 1) + 0x8000);
	g_paiContext.aiController->aiPlanState = 118;
}

// FUNCTION: XWA 0x4B0190
static char paiman_escortmaneuver(void) {
	int16_t escortTargetFG;
	unsigned int targetObjIdx;
	uint16_t scanObjIdx;
	CraftData* targetCraft;
	AiController* targetAi;
	unsigned int effectiveSkill;
	unsigned int escortRange;
	uint16_t targetSpeed;
	uint16_t selfSpeed;
	uint16_t oldThrottle;
	Q16Angle* selfPitchPtr;
	uint16_t targetPitch;
	uint16_t pitchDelta;
	Q16Angle* selfRollPtr;
	uint16_t targetRoll;
	uint16_t rollDelta;
	uint16_t variable1;

	escortTargetFG = (uint8_t)g_paiContext.aiController->escortTargetFG;
	targetObjIdx = 0xffffu;
	g_paiContext.aiController->targetObjIdx = (uint16_t)-1;
	scanObjIdx = (uint16_t)g_activeRegionObjectSlotStart;
	if ((unsigned int)scanObjIdx < g_activeRegionCraftObjectSlotEnd) {
		while (1) {
			if (g_objectTable[scanObjIdx].objectType != OBJ_None &&
				(uint16_t)g_objectTable[scanObjIdx].flightGroupIdx == escortTargetFG &&
				g_objectTable[scanObjIdx].mobj->pCraft->leader_obj_idx == -1) {
				targetObjIdx = scanObjIdx;
				g_paiContext.aiController->targetObjIdx = scanObjIdx;
				break;
			}
			++scanObjIdx;
			if ((unsigned int)scanObjIdx >= g_activeRegionCraftObjectSlotEnd) {
				break;
			}
		}
	}

	if (targetObjIdx != 0xffffu) {
		pai_ObjectRefDirectionToObjectRef(targetObjIdx, g_paiContext.aiObjIdx);
		targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
		targetAi = pai_GetEffectiveAIController(targetCraft);
		escortRange = 0x8000u;
		if (g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent >= 3000) {
			escortRange = 0x20000u;
		}

		if ((unsigned int)trig2_polardistance <= escortRange && targetCraft->workingSubsystems != 0) {
			if (targetCraft->aiFlight.turnState == 2) {
				g_paiContext.aiController->targetXYAngle = targetAi->targetXYAngle;
				effectiveSkill = pai_GetEffectiveSkillValue(g_curCraft);
				paiman_setturn((effectiveSkill >> 3) + 0x4000u);
			} else if (g_objectTable[targetObjIdx].yaw != g_objectTable[g_paiContext.aiObjIdx].yaw) {
				g_paiContext.aiController->targetXYAngle = g_objectTable[targetObjIdx].yaw;
				effectiveSkill = pai_GetEffectiveSkillValue(g_curCraft);
				paiman_setturn((effectiveSkill >> 3) + 0x4000u);
			}

			targetSpeed = g_objectTable[targetObjIdx].mobj->speed;
			selfSpeed = g_objectTable[g_paiContext.aiObjIdx].mobj->speed;
			if (targetSpeed > selfSpeed) {
				oldThrottle = g_curCraft->throttleSpeed;
				g_curCraft->throttleSpeed =
					(uint16_t)(oldThrottle + (uint16_t)(50 * targetSpeed - 50 * selfSpeed));
				if (g_curCraft->throttleSpeed < oldThrottle) {
					g_curCraft->throttleSpeed = (uint16_t)-1;
				}
			} else if (targetSpeed < selfSpeed) {
				oldThrottle = g_curCraft->throttleSpeed;
				g_curCraft->throttleSpeed =
					(uint16_t)(oldThrottle - (uint16_t)(50 * selfSpeed - 50 * targetSpeed));
				if (g_curCraft->throttleSpeed > oldThrottle) {
					g_curCraft->throttleSpeed = 0;
				}
			}

			selfPitchPtr = &g_objectTable[g_paiContext.aiObjIdx].pitch;
			targetPitch = g_objectTable[targetObjIdx].pitch;
			pitchDelta = (uint16_t)(*selfPitchPtr - targetPitch);
			if (pitchDelta >= 0x8000u) {
				pitchDelta = (uint16_t)(0u - pitchDelta);
			}
			if (pitchDelta < 0x400u) {
				*selfPitchPtr = targetPitch;
				g_curCraft->aiFlight.headingState = 0;
			} else {
				g_paiContext.aiController->targetZAngle = targetPitch;
				if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch) {
					g_curCraft->aiFlight.headingState = 1;
				} else {
					g_curCraft->aiFlight.headingState = 2;
				}
				g_curCraft->aiFlight.headingStep = -1;
				g_curCraft->aiFlight.headingForce = 0;
			}

			selfRollPtr = &g_objectTable[g_paiContext.aiObjIdx].roll;
			targetRoll = g_objectTable[targetObjIdx].roll;
			rollDelta = (uint16_t)(*selfRollPtr - targetRoll);
			if (rollDelta >= 0x8000u) {
				rollDelta = (uint16_t)(0u - rollDelta);
			}
			if (rollDelta < 0x400u) {
				*selfRollPtr = targetRoll;
				g_objectTable[g_paiContext.aiObjIdx].mobj->orientMatrixDirty = 1;
				g_curCraft->aiFlight.enterFlag = 0;
			} else {
				g_paiContext.aiController->targetRoll = targetRoll;
				g_curCraft->aiFlight.rollStep = -1;
				g_curCraft->aiFlight.rollAccel = -1;
				g_curCraft->aiFlight.enterFlag = 1;
			}

			variable1 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							.fg
							.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									g_paiContext.curOrderCoord.fields.orderSlot]
							.variable1;
			pai_calcrotatedpoint(&g_objectTable[targetObjIdx], g_aiCourseOrderLocalOffsetXByVar[variable1],
								 g_aiCourseOrderLocalOffsetYByVar[variable1],
								 g_aiCourseOrderLocalOffsetZByVar[variable1]);
			if (g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent >= 3000) {
				int rotatedZ = g_rotatedZ;
				int rotatedY = g_rotatedY;
				int rotatedX = g_rotatedX;

				g_rotatedX = rotatedX * 16;
				g_rotatedY = rotatedY * 16;
				g_rotatedZ = rotatedZ * 16;
			}

			if (g_objectTable[targetObjIdx].playerOwnerIdx == -1 ||
				g_objectTable[targetObjIdx].mobj->speed > 10u) {
				g_curCraft->pushAccumX = g_rotatedX + g_objectTable[targetObjIdx].world_x -
										 g_objectTable[g_paiContext.aiObjIdx].world_x;
				g_curCraft->pushAccumY = g_rotatedY + g_objectTable[targetObjIdx].world_y -
										 g_objectTable[g_paiContext.aiObjIdx].world_y;
				g_curCraft->pushAccumZ = g_rotatedZ + g_objectTable[targetObjIdx].world_z -
										 g_objectTable[g_paiContext.aiObjIdx].world_z;
			} else {
				g_curCraft->pushAccumX = 0;
				g_curCraft->pushAccumY = 0;
				g_curCraft->pushAccumZ = 0;
			}
			return 0;
		}

		g_paiContext.aiController->aimPointX = g_objectTable[targetObjIdx].world_x;
		g_paiContext.aiController->aimPointY = g_objectTable[targetObjIdx].world_y;
		g_paiContext.aiController->aimPointZ = g_objectTable[targetObjIdx].world_z;
		if (targetCraft->workingSubsystems == 0) {
			paiman_setflighttotarget(0x4000, 1);
		} else {
			paiman_setflighttotarget(0, 1);
		}
		if ((int)trig2_polardistance > 0x10000) {
			g_curCraft->throttleSpeed = (uint16_t)-1;
		} else {
			g_curCraft->throttleSpeed = 0x4000u;
		}
		g_curCraft->pushAccumX = 0;
		g_curCraft->pushAccumY = 0;
		g_curCraft->pushAccumZ = 0;
	} else {
		paiman_setflighttotarget(0, 1);
		g_curCraft->throttleSpeed = 0x8000u;
	}
	return 0;
}

// FUNCTION: XWA 0x4B3130
AiController* paiman_initorbitmaneuver(void) {
	unsigned int waypointEnabled;
	unsigned int throttleSpeed;

	waypointEnabled = (uint16_t)g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						  .fg
						  .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								  g_paiContext.curOrderCoord.fields.orderSlot]
						  .waypoints[1]
						  .enabled;
	waypointEnabled = waypointEnabled != 0 ? 5u : 0u;
	g_paiContext.aiController->targetObjIdx = (uint16_t)(0x8000u + waypointEnabled);

	pai_UpdateAimPointFromOrderTarget();
	paiman_setflighttotarget(0, 1);
	g_paiContext.aiController->maneuverDist = trig2_polardistance;
	throttleSpeed = g_orderThrottleToCraftThrottleSpeed
		[g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
			 .fg
			 .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
					 g_paiContext.curOrderCoord.fields.orderSlot]
			 .throttle];
	g_curCraft->throttleSpeed = (uint16_t)throttleSpeed;

	Mission_ResolveObjectOrMissionPointWorldLoc(0x8004u, g_paiContext.curOrderCoord.fields.flightGroupIdx,
												g_paiContext.curOrderCoord.fields.regionIdx,
												g_paiContext.curOrderCoord.fields.orderSlot);
	trig2_ctop2dim(g_paiContext.aiController->aimPointX - worldlocx,
				   g_paiContext.aiController->aimPointY - worldlocy);
	{
		AiController* result;

		result = g_paiContext.aiController;
		result->orbitRadius = trig2_polardistance;
		return result;
	}
}

// FUNCTION: XWA 0x4B29A0
static void paiman_initwaitmaneuver(void) {
	g_paiContext.aiController->maneuverTimer =
		Mission_DecodeOrderTime(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
									.fg
									.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
											g_paiContext.curOrderCoord.fields.orderSlot]
									.variable1);
	g_paiContext.aiController->maneuverTimer *= 236;
	g_curCraft->aiFlight.enterFlag = 0;
	g_curCraft->aiFlight.headingState = 0;
	g_curCraft->aiFlight.turnState = 0;
	g_curCraft->throttleSpeed = 0;
}

// FUNCTION: XWA 0x4B6FC0
void paiman_ApplyBoardingPlanCompletionEffects(unsigned int boarderObjIdx, int planId, int orderSlot,
											   unsigned int targetObjIdx) {
	CraftData* boarderCraft;
	CraftData* targetCraft;
	AiController* boarderAi;
	AiController* targetAi;
	uint8_t targetFlightGroupIdx;
	uint16_t targetFlightGroupIdxWide;
	PlanRecord* plan;
	int i;
	int systemIdx;
	PaiContextSnapshot savedContext;
	CraftData* savedCurCraft;

	boarderCraft = g_objectTable[boarderObjIdx].mobj->pCraft;
	boarderAi = pai_GetEffectiveAIController(boarderCraft);
	if (g_objectTable[targetObjIdx].mobj != NULL) {
		targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
		targetAi = pai_GetEffectiveAIController(targetCraft);
	}
	targetFlightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
	targetFlightGroupIdxWide = targetFlightGroupIdx;
	plan = &g_planTable[planId];

	if (strcmp(plan->name, "boardtogivepln") == 0) {
		uint8_t emptyCargoIndex;

		emptyCargoIndex = 0xffu;
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			if (boarderCraft->cargoIndex != emptyCargoIndex) {
				targetCraft->cargoIndex = boarderCraft->cargoIndex;
				targetCraft->boardingState = 2;
			} else {
				if (boarderCraft->specialCargoName[0] != '\0') {
					for (i = 0; i < 16; ++i) {
						targetCraft->specialCargoName[i] = boarderCraft->specialCargoName[i];
					}
					targetCraft->boardingState = 2;
				}
			}
		}

		if ((intptr_t)&boarderAi->orderScratch.goalProgress[orderSlot][1] >=
			(intptr_t)g_missionFlightGroups[g_objectTable[boarderObjIdx].flightGroupIdx]
				.fg.orders[4 * g_objectTable[boarderObjIdx].regionIdx + orderSlot]
				.variable2) {
			if (boarderCraft->cargoIndex != emptyCargoIndex) {
				boarderCraft->cargoIndex = emptyCargoIndex;
			} else {
				boarderCraft->specialCargoName[0] = '\0';
			}
		}
		boarderCraft->boardingState = 1;

		if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
			uint16_t featureBit;

			featureBit = 1;
			for (i = 0; i < 13; ++i) {
				if ((targetCraft->installedHudFeatureMask & featureBit) != 0 &&
					(targetCraft->activeHudFeatureMask & featureBit) == 0) {
					targetCraft->activeHudFeatureMask |= featureBit;
				}
				featureBit <<= 1;
			}

			for (systemIdx = 0; systemIdx < 10; ++systemIdx) {
				targetCraft->systemDisplaySlotBySystem[systemIdx] = (uint8_t)systemIdx;
				targetCraft->systemHealth[systemIdx] = 100;
				targetCraft->systemTimer[systemIdx] = 0;
			}
		}

		msg_emitCraftMessage(boarderObjIdx, boarderCraft, MSG_REPORT_DOCK_COMP);
		fsfx_SpeakTacticalOfficerEvent(4, 105, boarderObjIdx, 0xffffu);
	} else if (strcmp(plan->name, "boardtotakepln") == 0) {
		uint8_t emptyCargoIndex;

		emptyCargoIndex = 0xffu;
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			if (targetCraft->cargoIndex != emptyCargoIndex) {
				boarderCraft->cargoIndex = targetCraft->cargoIndex;
				targetCraft->cargoIndex = emptyCargoIndex;
				targetCraft->boardingState = 1;
				boarderCraft->boardingState = 2;
			} else {
				if (targetCraft->specialCargoName[0] != '\0') {
					for (i = 0; i < 16; ++i) {
						boarderCraft->specialCargoName[i] = targetCraft->specialCargoName[i];
					}
					targetCraft->specialCargoName[0] = '\0';
				}
				targetCraft->boardingState = 1;
				boarderCraft->boardingState = 2;
			}
		} else {
			boarderCraft->boardingState = 2;
		}
		msg_emitCraftMessage(boarderObjIdx, boarderCraft, MSG_REPORT_DOCK_COMP);
		fsfx_SpeakTacticalOfficerEvent(4, 107, boarderObjIdx, 0xffffu);
	} else if (strcmp(plan->name, "boardtoexchangepln") == 0) {
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			if (boarderCraft->cargoIndex != 0xffu) {
				uint8_t cargoIndex;

				cargoIndex = boarderCraft->cargoIndex;
				boarderCraft->cargoIndex = targetCraft->cargoIndex;
				targetCraft->cargoIndex = cargoIndex;
			} else {
				for (i = 0; i < 16; ++i) {
					char cargoChar;

					cargoChar = boarderCraft->specialCargoName[i];
					boarderCraft->specialCargoName[i] = targetCraft->specialCargoName[i];
					targetCraft->specialCargoName[i] = cargoChar;
				}
			}
			targetCraft->boardingState = 2;
		}
		boarderCraft->boardingState = 2;
		msg_emitCraftMessage(boarderObjIdx, boarderCraft, MSG_REPORT_DOCK_COMP);
	} else if (strcmp(plan->name, "boardtocapturepln") == 0) {
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			g_paiContext.aiObjIdx = boarderObjIdx;
			g_paiContext.curOrderCoord.fields.flightGroupIdx = g_objectTable[boarderObjIdx].flightGroupIdx;
			paiman_TransferObjectToAiTeam(targetObjIdx, targetCraft, 0x80u);
			if (targetCraft->wasCaptured) {
				if (targetCraft->aiFlight.maxSpeedCache) {
					targetAi->pendingPlanId = (uint8_t)pai_findplanbyname("flyhomeevadepln");
				} else {
					targetAi->pendingPlanId = (uint8_t)pai_findplanbyname("stationaryldrpln");
				}
				msg_emitCraftMessage(targetObjIdx, targetCraft, MSG_CAPTURED);
			} else {
				int order;
				unsigned int planNameIndex;

				order = g_missionFlightGroups[targetFlightGroupIdxWide]
							.fg.orders[4 * g_objectTable[targetObjIdx].regionIdx]
							.order;
				if (targetCraft->leader_obj_idx == -1) {
					planNameIndex = g_orderLeaderBuiltinPlanNameIndex[order];
				} else {
					planNameIndex = g_orderFollowerBuiltinPlanNameIndex[order];
				}
				targetAi->pendingPlanId = g_builtinPlanIdByNameIndex[planNameIndex];
			}
			fsfx_SpeakTacticalOfficerEvent(4, 107, boarderObjIdx, 0xffffu);
			fsfx_SpeakTacticalOfficerEvent(4, 96, targetObjIdx, 0xffffu);
			savedContext.context = g_paiContext;
			savedCurCraft = g_curCraft;
			g_curCraft = targetCraft;
			pai_setupcraftcontext(targetObjIdx);
			pai_ApplyPendingPlanTargetAndManeuver(targetObjIdx);
			g_curCraft = savedCurCraft;
			g_paiContext = savedContext.context;
		}
	} else if (strcmp(plan->name, "boardtodestroypln") == 0) {
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			XwaOrder* order;

			Mission_CreditDestructionDamageContributors(boarderObjIdx, targetObjIdx);
			if (g_objectTable[targetObjIdx].mobj->lifetimeTimer != 0) {
				order = &g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							 .fg.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										g_paiContext.curOrderCoord.fields.orderSlot];
				g_objectTable[targetObjIdx].mobj->lifetimeTimer = Mission_DecodeOrderTime(order->variable3);
				g_objectTable[targetObjIdx].mobj->lifetimeTimer *= 236;
			}
			targetAi->pendingPlanId = (uint8_t)pai_findplanbyname("selfdestroypln");
			savedContext.context = g_paiContext;
			savedCurCraft = g_curCraft;
			g_curCraft = targetCraft;
			pai_setupcraftcontext(targetObjIdx);
			pai_ApplyPendingPlanTargetAndManeuver(targetObjIdx);
			g_curCraft = savedCurCraft;
			g_paiContext = savedContext.context;
		}
		fsfx_SpeakTacticalOfficerEvent(4, 107, boarderObjIdx, 0xffffu);
	} else if (strcmp(plan->name, "boardtopickuppln") == 0) {
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			uint16_t savedWorkingSubsystems;

			savedWorkingSubsystems = targetCraft->workingSubsystems;
			g_paiContext.aiObjIdx = boarderObjIdx;
			g_paiContext.curOrderCoord.fields.flightGroupIdx = g_objectTable[boarderObjIdx].flightGroupIdx;
			paiman_TransferObjectToAiTeam(targetObjIdx, targetCraft, 0x80u);
			boarderCraft->carriedObjectIndex = (uint16_t)targetObjIdx;
			if ((uint16_t)targetObjIdx == 0xffffu) {
				boarderCraft->aiFlight.motionScale = -1;
			} else {
				ObjectTypeId boarderType;
				ObjectTypeId targetType;
				int boarderSizeZ;
				int boarderSizeYZ;
				int targetSizeZ;
				int targetSizeYZ;
				int boarderVolume;
				int targetVolume;
				int volumeRatio;
				int motionScale;

				boarderType = g_objectTable[(uint16_t)boarderObjIdx].objectType;
				boarderSizeZ = ModelBounds_GetSizeZ(boarderType);
				boarderSizeYZ = ModelBounds_GetSizeY(boarderType) * boarderSizeZ;
				boarderVolume = ModelBounds_GetSizeX(boarderType) * boarderSizeYZ;
				targetType = g_objectTable[(uint16_t)targetObjIdx].objectType;
				targetSizeZ = ModelBounds_GetSizeZ(targetType);
				targetSizeYZ = ModelBounds_GetSizeY(targetType) * targetSizeZ;
				targetVolume = ModelBounds_GetSizeX(targetType) * targetSizeYZ;
				volumeRatio = boarderVolume / targetVolume;
				if (volumeRatio == 0) {
					motionScale = 0x8000;
				} else if (volumeRatio >= 10) {
					motionScale = 0xe666;
				} else {
					motionScale = 2621 * volumeRatio + 0x8000;
				}
				boarderCraft->aiFlight.motionScale = (int16_t)motionScale;
			}
			targetCraft->carrierObjIdx = (uint16_t)boarderObjIdx;
			targetCraft->workingSubsystems = savedWorkingSubsystems;
			fsfx_SpeakTacticalOfficerEvent(4, 105, boarderObjIdx, 0xffffu);
		} else {
			++g_missionFgStats[targetFlightGroupIdx].outcomeCount[6];
			g_objectTable[targetObjIdx].objectType = OBJ_None;
			fsfx_SpeakTacticalOfficerEvent(4, 105, boarderObjIdx, 0xffffu);
		}
	} else if (strcmp(plan->name, "boardtocontactpln") == 0) {
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			targetCraft->boardingState = 2;
		}
		msg_emitCraftMessage(boarderObjIdx, boarderCraft, MSG_REPORT_DOCK_COMP);
		fsfx_SpeakTacticalOfficerEvent(4, 107, boarderObjIdx, 0xffffu);
	} else if (strcmp(plan->name, "boardtorepairpln") == 0) {
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			targetCraft->subsystemDamage = 0;
			targetCraft->workingSubsystems = targetCraft->systemFlags;
			targetCraft->objectKind = 0;
			targetCraft->boardingState = 3;
		}
		msg_emitCraftMessage(boarderObjIdx, boarderCraft, MSG_REPORT_DOCK_COMP);
		fsfx_SpeakTacticalOfficerEvent(4, 107, boarderObjIdx, 0xffffu);
		fsfx_SpeakTacticalOfficerEvent(4, 94, targetObjIdx, 0xffffu);
	}

	if (g_objectTable[targetObjIdx].mobj != NULL) {
		MobileObject* boarderMobj;
		MobileObject* targetMobj;

		boarderMobj = g_objectTable[boarderObjIdx].mobj;
		targetMobj = g_objectTable[targetObjIdx].mobj;
		if (boarderMobj->iff == targetMobj->iff) {
			uint8_t team;

			team = boarderMobj->team;
			if ((int8_t)targetCraft->iffVisibility[team] < 1) {
				int teamIdx;
				int8_t maxVisibility;

				maxVisibility = 0;
				teamIdx = 10;
				do {
					if ((int8_t)targetCraft->iffVisibility[10 - teamIdx] > maxVisibility) {
						maxVisibility = (int8_t)targetCraft->iffVisibility[10 - teamIdx];
					}
					--teamIdx;
				} while (teamIdx != 0);
				targetCraft->iffVisibility[team] = (uint8_t)(maxVisibility + 1);
				++g_missionFgStats[targetFlightGroupIdxWide].outcomeCount[8];
				if (g_missionFlightGroups[targetFlightGroupIdxWide].fg.specialCargoCraft ==
					targetCraft->waveNumber) {
					g_missionFgStats[targetFlightGroupIdxWide].specialCargoOutcome[8] = 1;
				}
			}
		} else {
			fsfx_PlaySound(60, 0xffffu, (unsigned int)g_localPlayer);
		}
	}

	++boarderAi->orderScratch.goalProgress[g_objectTable[boarderObjIdx].regionIdx][orderSlot];
	boarderCraft->aiFlight.objSignatures[boarderCraft->aiFlight.objSignatureCount] =
		g_objectTable[targetObjIdx].objectSignature;
	++boarderCraft->aiFlight.objSignatureCount;
	if (boarderCraft->aiFlight.objSignatureCount >= 10u) {
		--boarderCraft->aiFlight.objSignatureCount;
	}

	if (boarderCraft->aiFlight.objSignatureCount == 1) {
		uint8_t boarderFlightGroupIdx;

		boarderFlightGroupIdx = g_objectTable[boarderObjIdx].flightGroupIdx;
		++g_missionFgStats[boarderFlightGroupIdx].outcomeCount[12];
		if (g_missionFlightGroups[boarderFlightGroupIdx].fg.specialCargoCraft == boarderCraft->waveNumber) {
			g_missionFgStats[boarderFlightGroupIdx].specialCargoOutcome[12] = 1;
		}
	}

	if (g_objectTable[targetObjIdx].mobj != NULL) {
		++targetCraft->aiFlight.orderActionCounter;
		++g_missionFgStats[targetFlightGroupIdxWide].outcomeCount[10];
		if (g_missionFlightGroups[targetFlightGroupIdxWide].fg.specialCargoCraft == targetCraft->waveNumber) {
			g_missionFgStats[targetFlightGroupIdxWide].specialCargoOutcome[10] = 1;
		}
	}

	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
		boarderAi->candidateTargetIdx = 0xffffu;
	}
}

// FUNCTION: XWA 0x4B65E0
bool paiman_UpdateBoardOrPickupAutopilot(unsigned int playerIdx) {
	uint16_t boarderObjIdx;
	CraftData* boarderCraft;
	AiController* ai;
	uint16_t targetObjIdx;
	ObjectRecord* targetObj;
	CraftData* targetCraft;
	uint16_t targetModelIdx;
	uint16_t targetFlightGroupIdx;

	if (g_players[playerIdx].objectIndex == 0xffff) {
		return true;
	}

	boarderObjIdx = (uint16_t)g_players[playerIdx].objectIndex;
	boarderCraft = g_objectTable[boarderObjIdx].mobj->pCraft;
	ai = pai_GetEffectiveAIController(boarderCraft);
	targetObjIdx = ai->targetObjIdx;
	if (targetObjIdx == 0xffffu || targetObjIdx >= g_activeRegionCraftObjectSlotEnd) {
		return true;
	}

	targetObj = &g_objectTable[targetObjIdx];
	if (targetObj->objectType == OBJ_None) {
		return true;
	}

	if (targetObj->mobj != NULL) {
		targetCraft = targetObj->mobj->pCraft;
		targetModelIdx = targetCraft->modelIndex;
	} else {
		targetModelIdx = 0xffffu;
	}
	targetFlightGroupIdx = targetObj->flightGroupIdx;

	switch (ai->maneuverPhase) {
		case 0: {
			int dx;
			int dy;
			int dz;

			if (targetObj->mobj != NULL) {
				int attachDelta;
				int localForward;

				localForward = g_modelDefs[targetModelIdx].meshAttachData[0];
				if (targetObj->genusId == GENUS_Fighter || targetObj->genusId == GENUS_Transport) {
					attachDelta = g_modelDefs[targetModelIdx].meshAttachData[1] -
								  g_modelDefs[boarderCraft->modelIndex].meshAttachData[3];
				} else if (g_objectTable[boarderObjIdx].genusId == GENUS_Fighter ||
						   g_objectTable[boarderObjIdx].genusId == GENUS_Transport) {
					attachDelta = g_modelDefs[targetModelIdx].meshAttachData[1] -
								  g_modelDefs[boarderCraft->modelIndex].meshAttachData[4];
				} else {
					attachDelta = g_modelDefs[targetModelIdx].meshAttachData[2] -
								  g_modelDefs[boarderCraft->modelIndex].meshAttachData[4];
				}
				if (targetObj->objectType != OBJ_SpaceColony2) {
					attachDelta += 2 * (g_modelDefs[targetModelIdx].meshAttachData[2] -
										g_modelDefs[g_curCraft->modelIndex].meshAttachData[4]);
				}
				if (attachDelta < 0) {
					attachDelta = 0x7000;
				}
				pai_RotateLocalVectorToWorldScratch(targetObj, 0, attachDelta, localForward);
				dx = g_objectTable[targetObjIdx].world_x - g_objectTable[boarderObjIdx].world_x + g_rotatedX;
				boarderCraft->pushAccumX = dx;
				dy = g_objectTable[targetObjIdx].world_y - g_objectTable[boarderObjIdx].world_y + g_rotatedY;
				boarderCraft->pushAccumY = dy;
				dz = g_objectTable[targetObjIdx].world_z - g_objectTable[boarderObjIdx].world_z + g_rotatedZ;
				boarderCraft->pushAccumZ = dz;
			} else {
				Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
				dx = worldlocx - g_objectTable[boarderObjIdx].world_x;
				boarderCraft->pushAccumX = dx;
				dy = worldlocy - g_objectTable[boarderObjIdx].world_y;
				boarderCraft->pushAccumY = dy;
				dz = worldlocz - g_objectTable[boarderObjIdx].world_z + 0x80;
				boarderCraft->pushAccumZ = dz;
			}

			if (dx < 0) {
				dx = -dx;
			}
			if (dy < 0) {
				dy = -dy;
			}
			if (dz < 0) {
				dz = -dz;
			}
			if (dx + dy + dz < 0x180) {
				boarderCraft->pushAccumX = 0;
				boarderCraft->pushAccumY = 0;
				boarderCraft->pushAccumZ = 0;
				ai->maneuverPhase = 1;
				boarderCraft->aiFlight.rollAccel = 0x4000;
				boarderCraft->aiFlight.turnAccel = 0x4000;
				boarderCraft->aiFlight.pitchAccel = 0x4000;
				return false;
			}

			trig2_ctop(boarderCraft->pushAccumX, boarderCraft->pushAccumY, boarderCraft->pushAccumZ);
			paiman_SlewCraftOrientation(boarderObjIdx, trig2_xyangle, targetPitch, 0);
			return false;
		}

		case 1: {
			int dx;
			int dy;
			int dz;
			Q16Angle targetYaw;
			Q16Angle alignPitch;
			Q16Angle targetRoll;

			if (targetObj->mobj != NULL) {
				int attachDelta;
				int localForward;

				localForward = g_modelDefs[targetModelIdx].meshAttachData[0];
				if (targetObj->genusId == GENUS_Fighter || targetObj->genusId == GENUS_Transport) {
					attachDelta = g_modelDefs[targetModelIdx].meshAttachData[1] -
								  g_modelDefs[boarderCraft->modelIndex].meshAttachData[3];
				} else if (g_objectTable[boarderObjIdx].genusId == GENUS_Fighter ||
						   g_objectTable[boarderObjIdx].genusId == GENUS_Transport) {
					attachDelta = g_modelDefs[targetModelIdx].meshAttachData[1] -
								  g_modelDefs[boarderCraft->modelIndex].meshAttachData[4];
				} else {
					attachDelta = g_modelDefs[targetModelIdx].meshAttachData[2] -
								  g_modelDefs[boarderCraft->modelIndex].meshAttachData[4];
				}
				pai_RotateLocalVectorToWorldScratch(targetObj, 0, attachDelta, localForward);
				dx = g_objectTable[targetObjIdx].world_x - g_objectTable[boarderObjIdx].world_x + g_rotatedX;
				boarderCraft->pushAccumX = dx;
				dy = g_objectTable[targetObjIdx].world_y - g_objectTable[boarderObjIdx].world_y + g_rotatedY;
				boarderCraft->pushAccumY = dy;
				dz = g_objectTable[targetObjIdx].world_z - g_objectTable[boarderObjIdx].world_z + g_rotatedZ;
				boarderCraft->pushAccumZ = dz;
				targetYaw = g_objectTable[targetObjIdx].yaw;
				alignPitch = g_objectTable[targetObjIdx].pitch;
				targetRoll = g_objectTable[targetObjIdx].roll;
			} else {
				Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
				dx = worldlocx - g_objectTable[boarderObjIdx].world_x;
				boarderCraft->pushAccumX = dx;
				dy = worldlocy - g_objectTable[boarderObjIdx].world_y;
				boarderCraft->pushAccumY = dy;
				dz = worldlocz - g_objectTable[boarderObjIdx].world_z + 0x80;
				boarderCraft->pushAccumZ = dz;
				targetYaw = 0;
				alignPitch = 0x4000;
				targetRoll = 0;
			}

			paiman_SlewCraftOrientation(boarderObjIdx, targetYaw, alignPitch, targetRoll);

			if (dx < 0) {
				dx = -dx;
			}
			if (dy < 0) {
				dy = -dy;
			}
			if (dz < 0) {
				dz = -dz;
			}
			if (dx + dy + dz < 0x10) {
				boarderCraft->pushAccumX = 0;
				boarderCraft->pushAccumY = 0;
				boarderCraft->pushAccumZ = 0;
				ai->maneuverPhase = 2;

				{
					int waitMultiplier;

					waitMultiplier = 2;
					if (g_players[playerIdx].inputDisabledFlag == 1) {
						const XwaOrder* order;

						order = &g_missionFlightGroups[g_objectTable[boarderObjIdx].flightGroupIdx]
									 .fg.orders[4 * g_objectTable[boarderObjIdx].regionIdx +
												(uint8_t)ai->currentOrderSlot];
						waitMultiplier = Mission_DecodeOrderTime(order->variable1);
					}
					ai->aiPlanState = 236;
					ai->maneuverTimer = 236 * waitMultiplier;
				}

				if (g_objectTable[boarderObjIdx].mobj != NULL &&
					boarderCraft->aiFlight.orderActionFlag == 0) {
					boarderCraft->aiFlight.orderActionFlag = 1;
					++g_missionFgStats[g_objectTable[boarderObjIdx].flightGroupIdx].outcomeCount[26];
					if (g_missionFlightGroups[g_objectTable[boarderObjIdx].flightGroupIdx]
							.fg.specialCargoCraft == boarderCraft->waveNumber) {
						g_missionFgStats[g_objectTable[boarderObjIdx].flightGroupIdx]
							.specialCargoOutcome[26] = 1;
					}
				}

				if (g_objectTable[targetObjIdx].mobj != NULL && targetCraft->aiFlight.reserved0C == 0) {
					targetCraft->aiFlight.reserved0C = 1;
					++g_missionFgStats[targetFlightGroupIdx].outcomeCount[25];
					if (g_missionFlightGroups[targetFlightGroupIdx].fg.specialCargoCraft ==
						targetCraft->waveNumber) {
						g_missionFgStats[targetFlightGroupIdx].specialCargoOutcome[25] = 1;
					}
				}

				msg_formatObjectName(boarderObjIdx, 1, g_flightTextScratchBuffer);
				msg_addMessagePtr(0, g_flightTextScratchBuffer);
				msg_formatObjectName(targetObjIdx, 1, outName);
				msg_addMessagePtr(1, outName);
				g_msgSenderIff = (uint8_t)g_objectTable[boarderObjIdx].mobj->iff;
				if (g_players[g_localPlayer].regionIndex == g_objectTable[boarderObjIdx].regionIdx) {
					msg_emitInFlightMessage(MSG_HAS_DOCKED, g_localPlayer);
				}
			}
			break;
		}

		case 2:
			if (ai->maneuverTimer == 0) {
				switch (g_players[playerIdx].inputDisabledFlag) {
					case 1:
						paiman_ApplyBoardingPlanCompletionEffects(
							boarderObjIdx, ai->currentPlanId, (uint8_t)ai->currentOrderSlot, targetObjIdx);
						ai->maneuverTimer = 472;
						msg_emitInFlightMessage(MSG_BOARD_COMPLETE, (int)playerIdx);
						break;

					case 2:
						if (targetObj->mobj != NULL) {
							uint16_t savedWorkingSubsystems;
							ObjectTypeId carrierType;
							ObjectTypeId targetType;
							int carrierVolume;
							int targetVolume;
							int volumeRatio;
							int motionScale;

							savedWorkingSubsystems = targetCraft->workingSubsystems;
							g_paiContext.aiObjIdx = boarderObjIdx;
							g_paiContext.curOrderCoord.fields.flightGroupIdx =
								g_objectTable[boarderObjIdx].flightGroupIdx;
							paiman_TransferObjectToAiTeam(targetObjIdx, targetCraft, 0x80u);
							boarderCraft->carriedObjectIndex = targetObjIdx;
							carrierType = g_objectTable[boarderObjIdx].objectType;
							carrierVolume = ModelBounds_GetSizeY(carrierType);
							carrierVolume *= ModelBounds_GetSizeX(carrierType);
							carrierVolume *= ModelBounds_GetSizeZ(carrierType);
							targetType = targetObj->objectType;
							targetVolume = ModelBounds_GetSizeY(targetType);
							targetVolume *= ModelBounds_GetSizeX(targetType);
							targetVolume *= ModelBounds_GetSizeZ(targetType);
							volumeRatio = carrierVolume / targetVolume;
							if (volumeRatio == 0) {
								motionScale = 0x8000;
							} else {
								if (volumeRatio >= 10) {
									motionScale = 0xe666;
								} else {
									motionScale = 2621 * volumeRatio + 0x8000;
								}
							}
							boarderCraft->aiFlight.motionScale = (int16_t)motionScale;
							targetCraft->carrierObjIdx = boarderObjIdx;
							targetCraft->workingSubsystems = savedWorkingSubsystems;
							++targetCraft->aiFlight.orderActionCounter;
							++g_missionFgStats[targetFlightGroupIdx].outcomeCount[10];
							if (g_missionFlightGroups[targetFlightGroupIdx].fg.specialCargoCraft ==
								targetCraft->waveNumber) {
								g_missionFgStats[targetFlightGroupIdx].specialCargoOutcome[10] = 1;
							}
							msg_emitInFlightMessage(MSG_PICKUP_SECURED, (int)playerIdx);
							fsfx_PlaySound(136, 0xffffu, playerIdx);
						} else {
							++g_missionFgStats[targetObj->flightGroupIdx].outcomeCount[6];
							targetObj->objectType = OBJ_None;
						}

						ai->maneuverTimer = 118;
						ai->maneuverPhase = 3;
						return false;

					case 3:
						ai->maneuverTimer = 236;
						msg_emitInFlightMessage(MSG_BOARD_COMPLETE, (int)playerIdx);
						break;

					default:
						break;
				}
				ai->maneuverPhase = 3;
			}
			break;

		case 3:
			if (playerIdx == (unsigned int)g_localPlayer) {
				ForceFeedback_PlayBoardOrPickupReleaseEffect();
			}

			if (ai->maneuverTimer == 0) {
				ai->targetSignature = 0;
				ai->hasLiveTarget = 0;
				ai->maneuverMode = 0;
				ai->targetObjIdx = 0xffffu;
				return true;
			}

			if (g_objectTable[targetObjIdx].mobj != NULL) {
				pai_calcrotatedpoint(&g_objectTable[boarderObjIdx], 0, 0x100, 0);
				boarderCraft->pushAccumX = g_rotatedX;
				boarderCraft->pushAccumY = g_rotatedY;
				boarderCraft->pushAccumZ = g_rotatedZ;
			} else {
				boarderCraft->pushAccumX = 0;
				boarderCraft->pushAccumY = 0;
				boarderCraft->pushAccumZ = 500;
			}
			return false;

		default:
			break;
	}
	return false;
}

// FUNCTION: XWA 0x4B2360
void paiman_TransferObjectToAiTeam(unsigned int objectIdx, CraftData* craft, uint8_t ownerFlag) {
	MobileObject* objectMobj;
	MobileObject* aiMobj;
	ObjectRecord* object;
	ObjectRecord* aiObject;
	unsigned int aiObjIdx;
	uint8_t oldTeam;
	int8_t aiTeam;
	uint8_t owningTeam;
	uint8_t specialCargoCraft;
	unsigned int previousOwnerFgIdx;
	uint8_t flightGroupIdx;

	aiObjIdx = g_paiContext.aiObjIdx;
	object = &g_objectTable[objectIdx];
	aiObject = &g_objectTable[aiObjIdx];
	flightGroupIdx = object->flightGroupIdx;
	aiMobj = aiObject->mobj;
	objectMobj = object->mobj;
	aiTeam = (int8_t)aiMobj->team;
	oldTeam = objectMobj->team;

	if (oldTeam != aiTeam) {
		owningTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
		if (owningTeam == aiTeam) {
			specialCargoCraft = g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft;
			--g_missionFgStats[flightGroupIdx].outcomeCount[6];
			if (specialCargoCraft == craft->waveNumber) {
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[6] = 0;
			}
			craft->wasCaptured = 0;
		} else {
			if (owningTeam == oldTeam) {
				specialCargoCraft = g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft;
				++g_missionFgStats[flightGroupIdx].outcomeCount[6];
				if (specialCargoCraft == craft->waveNumber) {
					g_missionFgStats[flightGroupIdx].specialCargoOutcome[6] = 1;
				}
				if ((int8_t)craft->iffVisibility[aiTeam] < 1) {
					++g_missionFgStats[flightGroupIdx].outcomeCount[8];
					++g_missionFgStats[flightGroupIdx].teamInspected[aiTeam];
					if (specialCargoCraft == craft->waveNumber) {
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[8] = 1;
						++g_missionFgStats[flightGroupIdx].teamSpecialCargoInspected[aiTeam];
					}
					craft->iffVisibility[aiTeam] = 1;
					aiObjIdx = g_paiContext.aiObjIdx;
				}
			} else {
				previousOwnerFgIdx = (uint8_t)craft->wasCaptured & 0x7fu;
				--g_missionFlightRuntimeState
					  .teamFgCounters[TEAM_FG_COUNTER_TRANSFER]
									 [g_missionFlightGroups[previousOwnerFgIdx].fg.team][flightGroupIdx];
			}

			++g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_TRANSFER]
														[g_objectTable[aiObjIdx].mobj->team][flightGroupIdx];
			craft->wasCaptured = (char)((uint8_t)g_paiContext.curOrderCoord.raw | ownerFlag);
		}
	}

	g_objectTable[objectIdx].mobj->iff = g_objectTable[g_paiContext.aiObjIdx].mobj->iff;
	g_objectTable[objectIdx].mobj->team = g_objectTable[g_paiContext.aiObjIdx].mobj->team;
	craft->workingSubsystems = craft->systemFlags;
	craft->subsystemDamage = 0;
	craft->objectKind = 0;
	craft->lastAttackerObjIdx = 0xffffu;
}

// FUNCTION: XWA 0x4B80A0
char paiman_SlewCraftOrientation(uint16_t objectIdx, Q16Angle targetYaw, Q16Angle targetPitch,
								 Q16Angle targetRoll) {
	CraftData* craft;
	char changed;

	craft = g_objectTable[objectIdx].mobj->pCraft;
	changed = 0;

	if (g_objectTable[objectIdx].roll != targetRoll) {
		uint16_t delta;
		uint16_t oldAccel;
		uint16_t step;
		int16_t elapsedStep;
		int16_t accelStep;

		delta = (uint16_t)(targetRoll - g_objectTable[objectIdx].roll);
		oldAccel = (uint16_t)craft->aiFlight.rollAccel;
		if (oldAccel != 0xffffu) {
			accelStep = (int16_t)((int32_t)(oldAccel * (uint32_t)g_elapsedTicks) / 236);
			craft->aiFlight.rollAccel = (int16_t)(craft->aiFlight.rollAccel + accelStep);
			if (oldAccel > (uint16_t)craft->aiFlight.rollAccel) {
				craft->aiFlight.rollAccel = (int16_t)0xffffu;
			}
			if (craft->aiFlight.rollAccel == 0) {
				craft->aiFlight.rollAccel = (int16_t)0xffffu;
			}
		}
		elapsedStep =
			(int16_t)((int32_t)((uint32_t)g_elapsedTicks * (uint16_t)craft->aiFlight.rollRate) / 236);
		step = (uint16_t)MATH2_fraction((uint16_t)elapsedStep, (uint16_t)craft->aiFlight.rollAccel);
		step = (uint16_t)(2u * MATH2_fraction(step, 0x8000u));
		if (delta < 0x8000u) {
			if (delta <= step) {
				g_objectTable[objectIdx].roll = targetRoll;
			} else {
				g_objectTable[objectIdx].roll = (Q16Angle)(g_objectTable[objectIdx].roll + step);
			}
		} else {
			delta = (uint16_t)-delta;
			if (delta <= step) {
				g_objectTable[objectIdx].roll = targetRoll;
			} else {
				g_objectTable[objectIdx].roll = (Q16Angle)(g_objectTable[objectIdx].roll - step);
			}
		}
		changed = 1;
	}

	if (g_objectTable[objectIdx].yaw != targetYaw) {
		uint16_t delta;
		uint16_t oldAccel;
		uint16_t step;
		int16_t elapsedStep;
		int16_t accelStep;

		delta = (uint16_t)(targetYaw - g_objectTable[objectIdx].yaw);
		oldAccel = (uint16_t)craft->aiFlight.turnAccel;
		if (oldAccel != 0xffffu) {
			accelStep =
				(int16_t)((int32_t)((uint32_t)g_elapsedTicks * (uint16_t)g_curCraft->aiFlight.turnAccel) /
						  236);
			craft->aiFlight.turnAccel = (int16_t)(craft->aiFlight.turnAccel + accelStep);
			if (oldAccel > (uint16_t)craft->aiFlight.turnAccel) {
				craft->aiFlight.turnAccel = (int16_t)0xffffu;
			}
			if (craft->aiFlight.turnAccel == 0) {
				craft->aiFlight.turnAccel = (int16_t)0xffffu;
			}
		}
		elapsedStep =
			(int16_t)((int32_t)((uint32_t)g_elapsedTicks * (uint16_t)craft->aiFlight.turnRate) / 236);
		step = (uint16_t)MATH2_fraction((uint16_t)elapsedStep, (uint16_t)craft->aiFlight.turnAccel);
		step = (uint16_t)MATH2_fraction(step, 0x8000u);
		if (delta < 0x8000u) {
			if (delta > step) {
				g_objectTable[objectIdx].yaw = (Q16Angle)(g_objectTable[objectIdx].yaw + step);
			} else {
				g_objectTable[objectIdx].yaw = targetYaw;
			}
		} else if ((uint16_t)-delta > step) {
			g_objectTable[objectIdx].yaw = (Q16Angle)(g_objectTable[objectIdx].yaw - step);
		} else {
			g_objectTable[objectIdx].yaw = targetYaw;
		}
		changed = 1;
	}

	if (g_objectTable[objectIdx].pitch != targetPitch) {
		uint16_t pitchDelta;
		uint16_t oldAccel;
		uint16_t step;
		int16_t elapsedStep;
		int16_t accelStep;

		pitchDelta = (uint16_t)(targetPitch - g_objectTable[objectIdx].pitch);
		if (pitchDelta >= 0x8000u) {
			pitchDelta = (uint16_t)-pitchDelta;
		}

		oldAccel = (uint16_t)craft->aiFlight.pitchAccel;
		if (oldAccel != 0xffffu) {
			accelStep = (int16_t)((int32_t)(oldAccel * (uint32_t)g_elapsedTicks) / 236);
			craft->aiFlight.pitchAccel = (int16_t)(craft->aiFlight.pitchAccel + accelStep);
			if (oldAccel > (uint16_t)craft->aiFlight.pitchAccel) {
				craft->aiFlight.pitchAccel = (int16_t)0xffffu;
			}
			if (craft->aiFlight.pitchAccel == 0) {
				craft->aiFlight.pitchAccel = (int16_t)0xffffu;
			}
		}
		elapsedStep =
			(int16_t)((int32_t)((uint32_t)g_elapsedTicks * (uint16_t)craft->aiFlight.pitchRate) / 236);
		step = (uint16_t)MATH2_fraction((uint16_t)elapsedStep, (uint16_t)craft->aiFlight.pitchAccel);
		step = (uint16_t)MATH2_fraction(step, 0x8000u);

		if (targetPitch <= g_objectTable[objectIdx].pitch) {
			if (pitchDelta <= step) {
				g_objectTable[objectIdx].pitch = targetPitch;
			} else {
				g_objectTable[objectIdx].pitch = (Q16Angle)(g_objectTable[objectIdx].pitch - step);
				if (g_objectTable[objectIdx].pitch >= 0xe000u) {
					g_objectTable[objectIdx].pitch = (Q16Angle)-g_objectTable[objectIdx].pitch;
					g_objectTable[objectIdx].yaw = (Q16Angle)(g_objectTable[objectIdx].yaw + 0x8000u);
					g_objectTable[objectIdx].roll = (Q16Angle)(g_objectTable[objectIdx].roll + 0x8000u);
				}
			}
		} else {
			if (pitchDelta <= step) {
				g_objectTable[objectIdx].pitch = targetPitch;
			} else {
				g_objectTable[objectIdx].pitch = (Q16Angle)(g_objectTable[objectIdx].pitch + step);
				if (g_objectTable[objectIdx].pitch >= 0x8000u) {
					g_objectTable[objectIdx].pitch = (Q16Angle)-g_objectTable[objectIdx].pitch;
					g_objectTable[objectIdx].yaw = (Q16Angle)(g_objectTable[objectIdx].yaw + 0x8000u);
					g_objectTable[objectIdx].roll = (Q16Angle)(g_objectTable[objectIdx].roll + 0x8000u);
				}
			}
		}
		changed = 1;
	}

	if (changed) {
		g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
		g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4B8460
bool paiman_UpdatePlayerTargetTrackingAutopilot(unsigned int playerIdx) {
	uint16_t playerObjIdx;
	CraftData* playerCraft;
	AiController* ai;
	uint16_t aiTargetObjIdx;
	uint16_t targetObjIdx;

	playerObjIdx = (uint16_t)g_players[playerIdx].objectIndex;
	if (g_players[playerIdx].objectIndex != 0xffff) {
		MobileObject* targetMobj;
		uint16_t targetSpeed;
		int targetBackOffset;

		playerCraft = g_objectTable[playerObjIdx].mobj->pCraft;
		ai = pai_GetEffectiveAIController(playerCraft);
		targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
		aiTargetObjIdx = ai->targetObjIdx;

		if (pai_IsObjectTargetable(targetObjIdx)) {
			if (aiTargetObjIdx != targetObjIdx) {
				playerCraft->aiFlight.turnAccel = 0x4000;
				playerCraft->aiFlight.pitchAccel = 0x4000;
				ai->targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
			}

			Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
			targetBackOffset =
				6 * g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent;
			if (targetBackOffset > 0x8000) {
				targetBackOffset = 0x8000;
			}
			worldlocz -= targetBackOffset;

			trig2_ctop(worldlocx - g_objectTable[playerObjIdx].world_x,
					   worldlocy - g_objectTable[playerObjIdx].world_y,
					   worldlocz - g_objectTable[playerObjIdx].world_z);

			targetMobj = g_objectTable[targetObjIdx].mobj;
			targetSpeed = (uint16_t)(uintptr_t)targetMobj;
			if (targetMobj != NULL) {
				targetSpeed = (uint16_t)targetMobj->speed;
			}

			if (trig2_polardistance > (targetSpeed != 0 ? 4096 : 0x2000)) {
				playerCraft->throttleSpeed = 0xffff;
			} else {
				uint16_t maxSpeed;

				maxSpeed = (uint16_t)playerCraft->aiFlight.maxSpeedCache;
				if (targetSpeed >= maxSpeed) {
					playerCraft->throttleSpeed = 0xffff;
				} else {
					playerCraft->throttleSpeed = (uint16_t)MATH2_divide(targetSpeed, maxSpeed);
				}

				if (playerCraft->throttleSpeed != 0xffffu && trig2_polardistance > 6206) {
					playerCraft->throttleSpeed = (uint16_t)(playerCraft->throttleSpeed + 2048u);
					if (playerCraft->throttleSpeed < 0x0800u) {
						playerCraft->throttleSpeed = 0xffff;
					}
				}
			}

			if (trig2_polardistance > 2796) {
				paiman_SlewCraftOrientation(playerObjIdx, trig2_xyangle, targetPitch, 0);
			}
		}
	}

	return false;
}

// FUNCTION: XWA 0x4B86E0
static char paiman_followtargetmaneuver(void) {
	uint16_t targetObjIdx;
	MobileObject* targetMobj;
	uint16_t targetSpeed;
	int targetBackOffset;

	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	if (targetObjIdx == 0xffffu) {
		return 0;
	}

	Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
	targetBackOffset = 6 * g_modelTypeTable[(uint16_t)g_objectTable[targetObjIdx].objectType].maxBoundsExtent;
	if (targetBackOffset > 0x8000) {
		targetBackOffset = 0x8000;
	}
	worldlocz -= targetBackOffset;

	trig2_ctop(worldlocx - g_objectTable[g_paiContext.aiObjIdx].world_x,
			   worldlocy - g_objectTable[g_paiContext.aiObjIdx].world_y,
			   worldlocz - g_objectTable[g_paiContext.aiObjIdx].world_z);

	targetMobj = g_objectTable[targetObjIdx].mobj;
	targetSpeed = (uint16_t)(uintptr_t)targetMobj;
	if (targetMobj != NULL) {
		targetSpeed = targetMobj->speed;
	}

	if (trig2_polardistance > (targetSpeed != 0 ? 4096 : 0x2000)) {
		CraftData* craft = g_curCraft;

		craft->throttleSpeed = 0xffff;
	} else {
		uint16_t maxSpeed;

		maxSpeed = (uint16_t)g_curCraft->aiFlight.maxSpeedCache;
		if (targetSpeed >= maxSpeed) {
			g_curCraft->throttleSpeed = 0xffff;
		} else {
			g_curCraft->throttleSpeed = (uint16_t)MATH2_divide(targetSpeed, maxSpeed);
		}

		if (g_curCraft->throttleSpeed != 0xffffu && trig2_polardistance > 6206) {
			g_curCraft->throttleSpeed = (uint16_t)(g_curCraft->throttleSpeed + 2048u);
			if (g_curCraft->throttleSpeed < 0x0800u) {
				g_curCraft->throttleSpeed = 0xffff;
			}
		}
	}

	if (trig2_polardistance > 2796) {
		paiman_SlewCraftOrientation(g_paiContext.aiObjIdx, (Q16Angle)trig2_xyangle, (Q16Angle)targetPitch, 0);
	}

	return 0;
}

// FUNCTION: XWA 0x4B64F0
uint16_t paiman_setpower(int objIdx, uint16_t throttle) {
	(void)objIdx;

	g_curCraft->throttleSpeed = throttle;
	return throttle;
}

// FUNCTION: XWA 0x4B6510
void paiman_setspeed(int objIdx, unsigned int desiredSpeed) {
	CraftData* pCraft;
	uint16_t powerMargin;
	uint16_t adjustedMaxSpeed;
	uint32_t throttleSpeed;

	pCraft = g_objectTable[objIdx].mobj->pCraft;
	powerMargin = (uint16_t)(6 - pCraft->beamLevel - pCraft->laserRedirect - pCraft->shieldRedirect);

	if (powerMargin >= 0x8000u) {
		adjustedMaxSpeed = (uint16_t)(pCraft->aiFlight.maxSpeedCache -
									  MATH2_fraction((uint16_t)(-8192 * powerMargin),
													 (uint16_t)pCraft->aiFlight.maxSpeedCache));
	} else {
		uint32_t scaledPowerMargin = powerMargin;
		uint16_t maxSpeed = (uint16_t)pCraft->aiFlight.maxSpeedCache;

		adjustedMaxSpeed =
			(uint16_t)(pCraft->aiFlight.maxSpeedCache +
					   MATH2_fraction((uint16_t)(scaledPowerMargin << 13), (uint16_t)maxSpeed));
	}

	if (desiredSpeed >= adjustedMaxSpeed) {
		g_curCraft->throttleSpeed = 0xffff;
		return;
	}

	throttleSpeed = MATH2_divide((uint16_t)desiredSpeed, adjustedMaxSpeed);
	g_curCraft->throttleSpeed = (uint16_t)throttleSpeed;
}

// FUNCTION: XWA 0x4B63E0
void paiman_setturn(uint16_t turnStep) {
	ObjectRecord* obj;
	uint16_t targetYaw;
	uint16_t oldYaw;
	uint16_t yawDelta;
	uint16_t turnThreshold;

	obj = &g_objectTable[g_paiContext.aiObjIdx];
	targetYaw = g_paiContext.aiController->targetXYAngle;
	oldYaw = obj->yaw;
	yawDelta = (uint16_t)(oldYaw - targetYaw);
	if (yawDelta >= 0x8000u) {
		yawDelta = (uint16_t)-yawDelta;
	}

	if (obj->genusId == GENUS_Starship || obj->playerOwnerIdx != -1) {
		turnThreshold = 24;
	} else {
		turnThreshold = 0x300;
	}

	if (yawDelta <= turnThreshold) {
		g_objectTable[g_paiContext.aiObjIdx].yaw = targetYaw;
		if ((g_modelTypeTable[g_objectTable[g_paiContext.aiObjIdx].objectType].flags &
			 MODEL_TYPE_FLAG_YAW_UPDATES_ANGLE_D) != 0) {
			g_objectTable[g_paiContext.aiObjIdx].angleD =
				(uint16_t)(g_objectTable[g_paiContext.aiObjIdx].angleD +
						   (uint16_t)(g_objectTable[g_paiContext.aiObjIdx].yaw - oldYaw));
		}
		g_objectTable[g_paiContext.aiObjIdx].mobj->orientMatrixDirty = 1;
		g_objectTable[g_paiContext.aiObjIdx].mobj->moveVectorDirty = 1;
		g_curCraft->aiFlight.turnState = 3;
		return;
	}

	if (g_curCraft->aiFlight.turnState != 2) {
		g_curCraft->aiFlight.turnAccel = 0x4000;
	}
	g_curCraft->aiFlight.turnState = 2;
	g_curCraft->aiFlight.turnStep = turnStep;
	return;
}

// FUNCTION: XWA 0x4B5930
void paiman_BeginPlayerFollowOverride(int objectIdx, int playerIdx) {
	CraftData* pCraft;
	AiController* effectiveAiController;

	if (objectIdx == 0xffff || playerIdx == -1) {
		return;
	}

	pCraft = g_objectTable[objectIdx].mobj->pCraft;
	effectiveAiController = pai_GetEffectiveAIController(pCraft);
	pCraft->aiController.thinkInterval = 0;

	if (strcmp(g_planTable[effectiveAiController->currentPlanId].name, "deathstarfollowpln") == 0) {
		return;
	}

	if (pCraft->followPlayerMode == 0) {
		pCraft->savedPendingPlan = effectiveAiController->pendingPlanId;
		pCraft->savedCurrentPlan = effectiveAiController->currentPlanId;
	}

	pCraft->followPlayerMode = 2;
	pCraft->followPlayerIdx = (uint8_t)playerIdx;
	pCraft->followTimer = 60;
	pCraft->followFormationSlot = 0;
}

// FUNCTION: XWA 0x4B7BA0
bool paiman_UpdatePlayerDeliveryAutopilot(unsigned int playerIdx) {
	PlayerData* player;
	uint16_t playerObjIdx;
	CraftData* playerCraft;
	AiController* ai;
	uint16_t carriedObjIdx;
	int attachOffset;

	player = &g_players[playerIdx];
	if (g_players[playerIdx].objectIndex == 0xffff) {
		return true;
	}

	playerObjIdx = (uint16_t)player->objectIndex;
	playerCraft = g_objectTable[playerObjIdx].mobj->pCraft;
	ai = pai_GetEffectiveAIController(playerCraft);
	carriedObjIdx = playerCraft->carriedObjectIndex;

	if ((int16_t)carriedObjIdx == -1) {
		playerCraft->aiFlight.motionScale = (int16_t)carriedObjIdx;
		return true;
	}

	switch (ai->maneuverPhase) {
		case 0: {
			ObjectRecord* carriedObj;
			uint8_t carriedGenus;
			int carriedSizeZ;
			int absDx;
			int absDy;
			int absDz;

			carriedObj = &g_objectTable[carriedObjIdx];
			carriedGenus = carriedObj->genusId;
			if (carriedGenus <= GENUS_Utility) {
				attachOffset = g_modelDefs[playerCraft->modelIndex].meshAttachData[3];
			} else {
				attachOffset = g_modelDefs[playerCraft->modelIndex].meshAttachData[4];
			}

			carriedSizeZ = ModelBounds_GetSizeZ((uint16_t)carriedObj->objectType);
			absDx = playerCraft->pushAccumX = ai->aimPointX - g_objectTable[playerObjIdx].world_x;
			absDy = playerCraft->pushAccumY = ai->aimPointY - g_objectTable[playerObjIdx].world_y;
			absDz = playerCraft->pushAccumZ =
				ai->aimPointZ + 2 * carriedSizeZ - g_objectTable[playerObjIdx].world_z - attachOffset;

			if (absDx < 0) {
				absDx = -absDx;
			}
			if (absDy < 0) {
				absDy = -absDy;
			}
			if (absDz < 0) {
				absDz = -absDz;
			}
			if (absDx + absDy + absDz < 3072) {
				ai->maneuverPhase = 1;
				playerCraft->aiFlight.rollAccel = 0x4000;
				playerCraft->aiFlight.turnAccel = 0x4000;
				playerCraft->aiFlight.pitchAccel = 0x4000;
			} else {
				trig2_ctop(playerCraft->pushAccumX, playerCraft->pushAccumY, playerCraft->pushAccumZ);
				paiman_SlewCraftOrientation(playerObjIdx, trig2_xyangle, targetPitch, 0);
			}
			return false;
		}

		case 1: {
			ObjectRecord* carriedObj;
			uint8_t carriedGenus;
			int alignZOffset;
			int absDx;
			int absDy;
			int absDz;

			carriedObj = &g_objectTable[carriedObjIdx];
			carriedGenus = carriedObj->genusId;
			if (carriedGenus <= GENUS_Utility) {
				attachOffset = g_modelDefs[playerCraft->modelIndex].meshAttachData[3];
			} else {
				attachOffset = g_modelDefs[playerCraft->modelIndex].meshAttachData[4];
			}

			alignZOffset = ModelBounds_GetSizeZ((uint16_t)carriedObj->objectType) - attachOffset;
			paiman_SlewCraftOrientation(playerObjIdx, 0, 0x4000u, 0);
			absDx = playerCraft->pushAccumX = ai->aimPointX - g_objectTable[playerObjIdx].world_x;
			absDy = playerCraft->pushAccumY = ai->aimPointY - g_objectTable[playerObjIdx].world_y;
			absDz = playerCraft->pushAccumZ =
				ai->aimPointZ - g_objectTable[playerObjIdx].world_z + alignZOffset;

			if (absDx < 0) {
				absDx = -absDx;
			}
			if (absDy < 0) {
				absDy = -absDy;
			}
			if (absDz < 0) {
				absDz = -absDz;
			}
			if (absDx + absDy + absDz >= 16 || g_objectTable[playerObjIdx].yaw != 0 ||
				g_objectTable[playerObjIdx].pitch != 0x4000u || g_objectTable[playerObjIdx].roll != 0) {
				return false;
			}

			playerCraft->pushAccumX = 0;
			playerCraft->pushAccumY = 0;
			playerCraft->pushAccumZ = 0;
			ai->maneuverTimer = 472;
			ai->maneuverPhase = (char)(ai->maneuverPhase + 1);
			return false;
		}

		case 2: {
			ObjectRecord* carriedObj;
			CraftData* carriedCraft;
			int16_t targetFlightGroupIdx;
			int orderSlot;

			if (ai->maneuverTimer != 0) {
				return false;
			}

			carriedObj = &g_objectTable[carriedObjIdx];
			carriedCraft = carriedObj->mobj->pCraft;
			targetFlightGroupIdx = g_objectTable[ai->targetObjIdx].flightGroupIdx;
			for (orderSlot = 0; orderSlot < 4; ++orderSlot) {
				uint8_t planId;

				planId = g_builtinPlanIdByNameIndex
					[g_orderLeaderBuiltinPlanNameIndex
						 [g_missionFlightGroups[g_objectTable[playerObjIdx].flightGroupIdx]
							  .fg.orders[orderSlot + 4 * g_objectTable[playerObjIdx].regionIdx]
							  .order]];
				if (strcmp(g_planTable[planId].name, "deliverpln") == 0 &&
					g_missionFlightGroups[g_objectTable[playerObjIdx].flightGroupIdx]
							.fg.orders[orderSlot + 4 * g_objectTable[playerObjIdx].regionIdx]
							.variable1 == targetFlightGroupIdx) {
					++g_missionFgStats[carriedObj->flightGroupIdx].outcomeCount[28];
					if (g_missionFlightGroups[carriedObj->flightGroupIdx].fg.specialCargoCraft ==
						carriedCraft->waveNumber) {
						g_missionFgStats[carriedObj->flightGroupIdx].specialCargoOutcome[28] = 1;
					}
				}
			}

			carriedCraft->carrierObjIdx = 0xffffu;
			playerCraft->carriedObjectIndex = 0xffffu;
			playerCraft->aiFlight.motionScale = -1;

			{
				AiController* carriedAi;

				carriedAi = pai_GetEffectiveAIController(carriedCraft);
				carriedAi->aimPointX = ai->aimPointX;
				carriedAi->aimPointY = ai->aimPointY;
				carriedAi->aimPointZ = ai->aimPointZ;
			}

			playerCraft->pushAccumX = 0;
			playerCraft->pushAccumY = 0;
			playerCraft->pushAccumZ = 1000;
			ai->maneuverTimer = 236;
			ai->maneuverPhase = 3;
			fsfx_PlaySound(18, 0xffffu, playerIdx);
			msg_emitInFlightMessage(MSG_OBJECT_DELIVERED, (int)playerIdx);
			return false;
		}

		case 3:
			if (ai->maneuverTimer == 0) {
				return true;
			}
			break;

		default:
			break;
	}

	return false;
}

// FUNCTION: XWA 0x4B0770
static char paiman_boardmaneuver(void) {
	uint16_t targetObjIdx;
	ObjectRecord* objRecord;
	MobileObject* mobj;
	CraftData* craft;
	AiController* targetAi;
	uint16_t targetModelIdx;
	uint16_t targetFlightGroupIdx;
	uint16_t targetOutcomeFlightGroupIdx;
	uint16_t targetSignature;
	unsigned int orderTime;

	targetObjIdx = g_paiContext.aiController->targetObjIdx;
	orderTime =
		Mission_DecodeOrderTime(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
									.fg
									.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
											g_paiContext.curOrderCoord.fields.orderSlot]
									.variable1);
	objRecord = &g_objectTable[targetObjIdx];
	mobj = g_objectTable[targetObjIdx].mobj;
	if (mobj != NULL) {
		craft = mobj->pCraft;
		targetAi = pai_GetEffectiveAIController(craft);
		targetModelIdx = craft->modelIndex;
		targetFlightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
		targetOutcomeFlightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
		targetSignature = g_objectTable[targetObjIdx].objectSignature;
	} else {
		targetModelIdx = 0xffffu;
		targetFlightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
		targetOutcomeFlightGroupIdx = g_objectTable[targetObjIdx].flightGroupIdx;
		targetSignature = g_objectTable[targetObjIdx].objectSignature;
	}

	switch (g_paiContext.aiController->maneuverPhase) {
		case 0: {
			if (g_objectTable[targetObjIdx].mobj != NULL) {
				ModelDef* targetModelDef;
				int attachZ;
				int upOffset;
				int extraOffset;

				targetModelDef = &g_modelDefs[targetModelIdx];
				attachZ = targetModelDef->meshAttachData[0];
				if (g_objectTable[targetObjIdx].genusId == GENUS_Fighter ||
					g_objectTable[targetObjIdx].genusId == GENUS_Transport) {
					upOffset = targetModelDef->meshAttachData[1] -
							   g_modelDefs[g_curCraft->modelIndex].meshAttachData[3];
				} else if (g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_Fighter ||
						   g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_Transport) {
					upOffset = targetModelDef->meshAttachData[1] + targetModelDef->meshAttachData[2] -
							   g_modelDefs[g_curCraft->modelIndex].meshAttachData[4];
				} else {
					upOffset = 2 * targetModelDef->meshAttachData[2] -
							   g_modelDefs[g_curCraft->modelIndex].meshAttachData[4];
				}
				extraOffset = 2 * (targetModelDef->meshAttachData[2] -
								   g_modelDefs[g_curCraft->modelIndex].meshAttachData[4]);
				if (extraOffset > 932)
					extraOffset = 932;
				pai_RotateLocalVectorToWorldScratch(objRecord, 0, upOffset + extraOffset, attachZ);
				g_paiContext.aiController->aimPointX = g_rotatedX + g_objectTable[targetObjIdx].world_x;
				g_paiContext.aiController->aimPointY = g_rotatedY + g_objectTable[targetObjIdx].world_y;
				g_paiContext.aiController->aimPointZ = g_rotatedZ + g_objectTable[targetObjIdx].world_z;
			} else {
				Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
				g_paiContext.aiController->aimPointX = worldlocx;
				g_paiContext.aiController->aimPointY = worldlocy;
				g_paiContext.aiController->aimPointZ = worldlocz + 2048;
			}

			paiman_setflighttotarget(0, 1);
			if ((int)trig2_polardistance > 0x4000) {
				g_curCraft->throttleSpeed = 0xffffu;
				return 0;
			}
			if ((int)trig2_polardistance > 0x2000) {
				g_curCraft->throttleSpeed = 0xc000u;
				return 0;
			}
			if ((int)trig2_polardistance > 2048) {
				g_curCraft->throttleSpeed = 0x8000u;
				return 0;
			}
			{
				int playerOwnerIdx;

				playerOwnerIdx = g_objectTable[targetObjIdx].playerOwnerIdx;
				if (playerOwnerIdx != -1 && g_objectTable[targetObjIdx].mobj != NULL &&
					g_objectTable[targetObjIdx].mobj->speed != 0) {
					HudInFlightMessageRecord* rec;
					unsigned int slot;

					if (playerOwnerIdx != g_localPlayer)
						return 0;
					slot = 0;
					rec = g_readyMessagePaneQueue;
					while (rec < &g_readyMessagePaneQueue[10]) {
						if (rec->stateOrMessageId == MSG_ZERO_THROTTLE_FOR_RELOAD)
							break;
						++rec;
						++slot;
					}
					if (slot < 10u)
						return 0;
					g_msgSenderIff =
						g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.iff;
					msg_emitInFlightMessage(MSG_ZERO_THROTTLE_FOR_RELOAD, g_localPlayer);
					fsfx_PlaySound(60, 0xffffu, (unsigned int)g_localPlayer);
					return 0;
				}
				g_curCraft->throttleSpeed = 0;
				g_paiContext.aiController->maneuverPhase = 1;
				return 0;
			}
		}

		case 1: {
			int pushX;
			int pushY;
			int pushZ;
			Q16Angle alignRoll;
			Q16Angle alignYaw;
			Q16Angle alignPitch;
			int absX;
			int absY;
			int absZ;

			if (g_objectTable[targetObjIdx].mobj != NULL) {
				ModelDef* targetModelDef;
				int attachZ;
				int upOffset;

				targetModelDef = &g_modelDefs[targetModelIdx];
				attachZ = targetModelDef->meshAttachData[0];
				if (g_objectTable[targetObjIdx].genusId == GENUS_Fighter ||
					g_objectTable[targetObjIdx].genusId == GENUS_Transport) {
					upOffset = targetModelDef->meshAttachData[1] -
							   g_modelDefs[g_curCraft->modelIndex].meshAttachData[3];
				} else if (g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_Fighter ||
						   g_objectTable[g_paiContext.aiObjIdx].genusId == GENUS_Transport) {
					upOffset = targetModelDef->meshAttachData[1] -
							   g_modelDefs[g_curCraft->modelIndex].meshAttachData[4];
				} else {
					upOffset = targetModelDef->meshAttachData[2] -
							   g_modelDefs[g_curCraft->modelIndex].meshAttachData[4];
				}
				pai_RotateLocalVectorToWorldScratch(objRecord, 0, upOffset, attachZ);
				g_curCraft->pushAccumX = g_rotatedX + g_objectTable[targetObjIdx].world_x -
										 g_objectTable[g_paiContext.aiObjIdx].world_x;
				pushX = g_curCraft->pushAccumX;
				g_curCraft->pushAccumY = g_rotatedY + g_objectTable[targetObjIdx].world_y -
										 g_objectTable[g_paiContext.aiObjIdx].world_y;
				pushY = g_curCraft->pushAccumY;
				g_curCraft->pushAccumZ = g_rotatedZ + g_objectTable[targetObjIdx].world_z -
										 g_objectTable[g_paiContext.aiObjIdx].world_z;
				pushZ = g_curCraft->pushAccumZ;
				alignRoll = g_objectTable[targetObjIdx].roll;
				alignYaw = g_objectTable[targetObjIdx].yaw;
				alignPitch = g_objectTable[targetObjIdx].pitch;
			} else {
				Mission_ResolveObjectOrMissionPointWorldLoc(targetObjIdx, 0, 0, 0);
				g_curCraft->pushAccumX = worldlocx - g_objectTable[g_paiContext.aiObjIdx].world_x;
				pushX = g_curCraft->pushAccumX;
				g_curCraft->pushAccumY = worldlocy - g_objectTable[g_paiContext.aiObjIdx].world_y;
				pushY = g_curCraft->pushAccumY;
				alignPitch = 0x4000;
				g_curCraft->pushAccumZ = worldlocz - g_objectTable[g_paiContext.aiObjIdx].world_z + 128;
				pushZ = g_curCraft->pushAccumZ;
				alignRoll = 0;
				alignYaw = 0;
			}

			if (g_objectTable[g_paiContext.aiObjIdx].roll != alignRoll) {
				g_curCraft->aiFlight.enterFlag = 1;
				g_curCraft->aiFlight.rollAccel = -1;
				g_curCraft->aiFlight.rollStep = 0x8000u;
				g_paiContext.aiController->targetRoll = alignRoll;
			}
			if (g_objectTable[g_paiContext.aiObjIdx].yaw != alignYaw) {
				g_curCraft->aiFlight.turnState = 2;
				g_curCraft->aiFlight.turnStep = 0x8000u;
				g_curCraft->aiFlight.turnAccel = -1;
				g_paiContext.aiController->targetXYAngle = alignYaw;
			}
			if (g_objectTable[g_paiContext.aiObjIdx].pitch != g_objectTable[targetObjIdx].pitch) {
				g_paiContext.aiController->targetZAngle = alignPitch;
				g_curCraft->aiFlight.headingStep = 0x8000u;
				g_curCraft->aiFlight.headingForce = 0;
				if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch)
					g_curCraft->aiFlight.headingState = 1;
				else
					g_curCraft->aiFlight.headingState = 2;
			}

			absX = pushX < 0 ? -pushX : pushX;
			absY = pushY < 0 ? -pushY : pushY;
			absZ = pushZ < 0 ? -pushZ : pushZ;
			if (absX + absY + absZ >= 16)
				return 0;
			g_curCraft->pushAccumX = 0;
			g_curCraft->pushAccumY = 0;
			g_curCraft->pushAccumZ = 0;
			g_paiContext.aiController->maneuverPhase = 2;
			g_paiContext.aiController->maneuverTimer = 236 * orderTime;
			g_paiContext.aiController->aiPlanState = 236;

			if (g_objectTable[g_paiContext.aiObjIdx].mobj != NULL &&
				g_curCraft->aiFlight.orderActionFlag == 0) {
				g_curCraft->aiFlight.orderActionFlag = 1;
				++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[26];
				if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg.specialCargoCraft == g_curCraft->waveNumber)
					g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.specialCargoOutcome[26] = 1;
			}
			if (g_objectTable[targetObjIdx].mobj != NULL && craft->aiFlight.reserved0C == 0) {
				craft->aiFlight.reserved0C = 1;
				++g_missionFgStats[targetFlightGroupIdx].outcomeCount[25];
				if (g_missionFlightGroups[targetFlightGroupIdx].fg.specialCargoCraft == craft->waveNumber)
					g_missionFgStats[targetFlightGroupIdx].specialCargoOutcome[25] = 1;
			}

			msg_formatObjectName(g_paiContext.aiObjIdx, 1, g_flightTextScratchBuffer);
			msg_addMessagePtr(0, g_flightTextScratchBuffer);
			msg_formatObjectName(targetObjIdx, 1, outName);
			msg_addMessagePtr(1, outName);
			g_msgSenderIff = (uint8_t)g_objectTable[g_paiContext.aiObjIdx].mobj->iff;
			if (g_players[g_localPlayer].regionIndex == g_objectTable[g_paiContext.aiObjIdx].regionIdx)
				msg_emitInFlightMessage(MSG_HAS_DOCKED, g_localPlayer);

			if (orderTime < 2)
				return 0;
			if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtogivepln") == 0 ||
				strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtoexchangepln") ==
					0 ||
				strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtocontactpln") ==
					0 ||
				strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtopickuppln") == 0 ||
				strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtorepairpln") == 0) {
				fsfx_SpeakTacticalOfficerEvent(4, 104, g_paiContext.aiObjIdx, 0xffffu);
				return 0;
			}
			if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtocapturepln") !=
					0 &&
				strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtotakepln") != 0 &&
				strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtodestroypln") != 0)
				return 0;
			fsfx_SpeakTacticalOfficerEvent(4, 106, g_paiContext.aiObjIdx, 0xffffu);
			return 0;
		}

		case 2: {
			uint16_t anyRearmed;
			uint16_t i;

			if (g_paiContext.aiController->maneuverTimer != 0) {
				if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtogivepln") !=
						0 ||
					g_objectTable[targetObjIdx].playerOwnerIdx == -1 ||
					g_paiContext.aiController->aiPlanState != 0)
					return 0;

				anyRearmed = 0;
				for (i = 0; i < craft->warheadLauncherCount; ++i) {
					int first;
					int last;
					int slot;

					if (!craft->warheadSlotTypeIds[i])
						continue;
					first = g_modelDefs[targetModelIdx].warheadLauncherFirstSlot[i];
					last = g_modelDefs[targetModelIdx].warheadLauncherLastSlot[i];
					if (first > last)
						continue;
					for (slot = first; slot <= last; ++slot) {
						uint16_t warhead;
						uint16_t targetCount;
						uint8_t status1;

						warhead =
							g_missionFlightGroups[g_objectTable[targetObjIdx].flightGroupIdx].fg.warhead;
						if (i == 1)
							warhead = 5;
						targetCount =
							(uint16_t)MATH2_fraction(g_modelDefs[targetModelIdx].warheadLauncherValue[i],
													 g_warheadAmmoCounts[warhead]);
						if (targetCount == 0)
							targetCount = 1;
						status1 =
							g_missionFlightGroups[g_objectTable[targetObjIdx].flightGroupIdx].fg.status1;
						if (status1 == 1)
							targetCount = (uint16_t)(targetCount * 2);
						else if (status1 == 2)
							targetCount = (uint16_t)(targetCount >> 1);
						if (targetCount == 0)
							targetCount = 1;
						if (targetCount > 99u)
							targetCount = 99;
						if (craft->warheadData[slot].count < targetCount) {
							++craft->warheadData[slot].count;
							if (targetObjIdx == (uint16_t)g_players[g_localPlayer].objectIndex) {
								if (warhead == OBJ_WarheadAdvancedTorpedo ||
									warhead == OBJ_WarheadAdvancedMissile ||
									warhead == OBJ_WarheadSpaceBomb || warhead == OBJ_WarheadRocket)
									fsfx_PlaySound(136, targetObjIdx, (unsigned int)g_localPlayer);
								else
									fsfx_PlaySound(137, targetObjIdx, (unsigned int)g_localPlayer);
							}
							anyRearmed = 1;
						}
						craft->warheadData[slot].laserCharge = 127;
					}
				}

				if (craft->cmTypeId)
					craft->cmAmmoCount = g_modelDefs[targetModelIdx].countermeasureCount;

				{
					uint16_t repaired;
					uint16_t bit;
					uint16_t n;

					repaired = anyRearmed;
					bit = 1;
					for (n = 0; n < 10; ++n) {
						if ((craft->systemFlags & bit) != 0 && (craft->workingSubsystems & bit) == 0) {
							craft->workingSubsystems |= bit;
							repaired = 1;
							break;
						}
						bit = (uint16_t)(bit * 2);
					}
					g_paiContext.aiController->aiPlanState = 472;
					if (repaired)
						g_paiContext.aiController->maneuverTimer = 1416;
				}
				return 0;
			}

			if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtogivepln") == 0) {
				if (g_objectTable[targetObjIdx].mobj != NULL) {
					if (g_curCraft->cargoIndex != 0xffu) {
						craft->cargoIndex = g_curCraft->cargoIndex;
					} else {
						char* dst = craft->specialCargoName;
						int k = 0;
						int nn = 16;

						do {
							++k;
							++dst;
							--nn;
							dst[-1] = g_curCraft->specialCargoName[k - 1];
						} while (nn);
					}
					craft->boardingState = 2;
				}
				// The original compares the address of this progress slot against the
				// order's byte-sized variable2. Preserve that Win32 expression for the
				// matching build, while spelling out its always-true effect on modern hosts.
#ifdef XWA_MODERN
				if (g_curCraft->cargoIndex != 0xffu)
					g_curCraft->cargoIndex = 0xffu;
				else
					g_curCraft->specialCargoName[0] = '\0';
#else
				if ((int)&g_paiContext.aiController->orderScratch
						.goalProgress[g_paiContext.curOrderCoord.fields.orderSlot][1] >=
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.variable2) {
					if (g_curCraft->cargoIndex != 0xffu)
						g_curCraft->cargoIndex = 0xffu;
					else
						g_curCraft->specialCargoName[0] = '\0';
				}
#endif
				g_curCraft->boardingState = 1;
				if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
					int doHud;
					int k;

					doHud = 0;
					if (g_objectTable[targetObjIdx].playerOwnerIdx == g_localPlayer) {
						int objectIndex;

						objectIndex = g_players[g_localPlayer].objectIndex;
						if (objectIndex != 0xffff) {
							ObjectTypeId t;

							t = g_objectTable[objectIndex].objectType;
							if (t == OBJ_XWing || t == OBJ_YWing || t == OBJ_AWing || t == OBJ_Z95 ||
								t == OBJ_BWing)
								doHud = 1;
						}
					}
					if (doHud) {
						uint16_t featureBit;
						int m;

						featureBit = 1;
						for (m = 0; m < 13; ++m) {
							if ((craft->installedHudFeatureMask & featureBit) != 0 &&
								(craft->activeHudFeatureMask & featureBit) == 0)
								craft->activeHudFeatureMask |= featureBit;
							featureBit = (uint16_t)(featureBit * 2);
						}
					}
					for (k = 0; k < 10; ++k) {
						craft->systemDisplaySlotBySystem[k] = (uint8_t)k;
						craft->systemHealth[k] = 100;
						craft->systemTimer[k] = 0;
					}
				}
				msg_emitCraftMessage(g_paiContext.aiObjIdx, g_curCraft, MSG_REPORT_DOCK_COMP);
				fsfx_SpeakTacticalOfficerEvent(4, 105, g_paiContext.aiObjIdx, 0xffffu);
			} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtotakepln") ==
					   0) {
				if (g_objectTable[targetObjIdx].mobj != NULL) {
					if (craft->cargoIndex != 0xffu) {
						g_curCraft->cargoIndex = craft->cargoIndex;
						craft->cargoIndex = 0xffu;
					} else {
						char* src = craft->specialCargoName;
						int k = 0;
						int nn = 16;

						do {
							g_curCraft->specialCargoName[k++] = *src++;
						} while (--nn);
						craft->specialCargoName[0] = '\0';
					}
					craft->boardingState = 1;
				}
				g_curCraft->boardingState = 2;
				msg_emitCraftMessage(g_paiContext.aiObjIdx, g_curCraft, MSG_REPORT_DOCK_COMP);
				fsfx_SpeakTacticalOfficerEvent(4, 107, g_paiContext.aiObjIdx, 0xffffu);
			} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "boardtoexchangepln") == 0) {
				if (g_objectTable[targetObjIdx].mobj != NULL) {
					if (g_curCraft->cargoIndex != 0xffu) {
						uint8_t cargoIndex;

						cargoIndex = g_curCraft->cargoIndex;
						g_curCraft->cargoIndex = craft->cargoIndex;
						craft->cargoIndex = cargoIndex;
					} else {
						char* pp = craft->specialCargoName;
						int k = 0;
						int nn = 16;

						for (;;) {
							char tmp;

							tmp = g_curCraft->specialCargoName[k];
							g_curCraft->specialCargoName[k] = *pp;
							*pp = tmp;
							++k;
							++pp;
							if (!--nn)
								break;
						}
					}
					craft->boardingState = 2;
				}
				g_curCraft->boardingState = 2;
				msg_emitCraftMessage(g_paiContext.aiObjIdx, g_curCraft, MSG_REPORT_DOCK_COMP);
			} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "boardtocapturepln") == 0 ||
					   strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "playerboardtocapturepln") == 0) {
				if (g_objectTable[targetObjIdx].mobj != NULL) {
					paiman_TransferObjectToAiTeam(targetObjIdx, craft, 0x80u);
					if (craft->wasCaptured) {
						if (craft->aiFlight.maxSpeedCache)
							targetAi->pendingPlanId = (uint8_t)pai_findplanbyname("flyhomeevadepln");
						else
							targetAi->pendingPlanId = (uint8_t)pai_findplanbyname("stationaryldrpln");
						msg_emitCraftMessage(targetObjIdx, craft, MSG_CAPTURED);
					} else {
						uint8_t order;

						order = g_missionFlightGroups[targetFlightGroupIdx]
									.fg.orders[4 * g_objectTable[targetObjIdx].regionIdx]
									.order;
						if (craft->leader_obj_idx == -1)
							targetAi->pendingPlanId =
								g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
						else
							targetAi->pendingPlanId =
								g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
					}
					fsfx_SpeakTacticalOfficerEvent(4, 107, g_paiContext.aiObjIdx, 0xffffu);
					fsfx_SpeakTacticalOfficerEvent(4, 96, targetObjIdx, 0xffffu);
					{
						PaiContext savedContext;
						CraftData* savedCraft;

						savedContext = g_paiContext;
						savedCraft = g_curCraft;
						g_curCraft = craft;
						pai_setupcraftcontext(targetObjIdx);
						pai_ApplyPendingPlanTargetAndManeuver(targetObjIdx);
						g_curCraft = savedCraft;
						g_paiContext = savedContext;
					}
				}
			} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "boardtodestroypln") == 0 ||
					   strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "playerboardtodestroypln") == 0) {
				if (g_objectTable[targetObjIdx].mobj != NULL) {
					Mission_CreditDestructionDamageContributors(g_paiContext.aiObjIdx, targetObjIdx);
					if (g_objectTable[targetObjIdx].mobj->lifetimeTimer != 0) {
						g_objectTable[targetObjIdx].mobj->lifetimeTimer = Mission_DecodeOrderTime(
							g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg
								.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										g_paiContext.curOrderCoord.fields.orderSlot]
								.variable3);
						g_objectTable[targetObjIdx].mobj->lifetimeTimer *= 236;
					}
					targetAi->pendingPlanId = (uint8_t)pai_findplanbyname("selfdestroypln");
					{
						PaiContext savedContext;
						CraftData* savedCraft;

						savedContext = g_paiContext;
						savedCraft = g_curCraft;
						g_curCraft = craft;
						pai_setupcraftcontext(targetObjIdx);
						pai_ApplyPendingPlanTargetAndManeuver(targetObjIdx);
						g_curCraft = savedCraft;
						g_paiContext = savedContext;
					}
				}
				fsfx_SpeakTacticalOfficerEvent(4, 107, g_paiContext.aiObjIdx, 0xffffu);
			} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "boardtopickuppln") == 0 ||
					   strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "playerboardtopickuppln") == 0) {
				if (g_objectTable[targetObjIdx].mobj != NULL) {
					ModelGenusId boarderGenus;

					boarderGenus = g_objectTable[g_paiContext.aiObjIdx].genusId;
					if ((boarderGenus == GENUS_Starship || boarderGenus == GENUS_Freighter) &&
						(g_objectTable[targetObjIdx].genusId == GENUS_PilotDroid ||
						 g_objectTable[targetObjIdx].genusId == GENUS_SatelliteBuoy)) {
						Mission_RecordCraftOutcome(targetObjIdx, targetOutcomeFlightGroupIdx, 0x12u);
						g_objectTable[targetObjIdx].objectType = OBJ_None;
					} else {
						uint16_t savedWorkingSubsystems;

						savedWorkingSubsystems = craft->workingSubsystems;
						paiman_TransferObjectToAiTeam(targetObjIdx, craft, 0x80u);
						g_curCraft->carriedObjectIndex = targetObjIdx;
						craft->carrierObjIdx = g_paiContext.aiObjIdx;
						craft->workingSubsystems = savedWorkingSubsystems;
					}
				} else {
					++g_missionFgStats[targetFlightGroupIdx].outcomeCount[6];
					g_objectTable[targetObjIdx].objectType = OBJ_None;
				}
				fsfx_SpeakTacticalOfficerEvent(4, 105, g_paiContext.aiObjIdx, 0xffffu);
			} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "boardtocontactpln") == 0) {
				if (g_objectTable[targetObjIdx].mobj != NULL)
					craft->boardingState = 2;
				msg_emitCraftMessage(g_paiContext.aiObjIdx, g_curCraft, MSG_REPORT_DOCK_COMP);
				fsfx_SpeakTacticalOfficerEvent(4, 107, g_paiContext.aiObjIdx, 0xffffu);
			} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "boardtorepairpln") == 0 ||
					   strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
							  "playerboardtorepairpln") == 0) {
				if (g_objectTable[targetObjIdx].mobj != NULL) {
					craft->subsystemDamage = 0;
					craft->workingSubsystems = craft->systemFlags;
					craft->objectKind = 0;
					craft->boardingState = 3;
				}
				msg_emitCraftMessage(g_paiContext.aiObjIdx, g_curCraft, MSG_REPORT_DOCK_COMP);
				fsfx_SpeakTacticalOfficerEvent(4, 107, g_paiContext.aiObjIdx, 0xffffu);
				fsfx_SpeakTacticalOfficerEvent(4, 94, targetObjIdx, 0xffffu);
			}

			if (g_objectTable[targetObjIdx].mobj != NULL) {
				MobileObject* boarderMobj;

				boarderMobj = g_objectTable[g_paiContext.aiObjIdx].mobj;
				if (boarderMobj->iff == g_objectTable[targetObjIdx].mobj->iff) {
					uint8_t team;

					team = boarderMobj->team;
					if ((int8_t)craft->iffVisibility[team] < 1) {
						int8_t maxVis;
						uint8_t* vis;
						int nn;

						maxVis = 0;
						vis = craft->iffVisibility;
						nn = 10;
						do {
							if ((int8_t)*vis > maxVis)
								maxVis = (int8_t)*vis;
							++vis;
						} while (--nn);
						craft->iffVisibility[team] = (uint8_t)(maxVis + 1);
						++g_missionFgStats[targetFlightGroupIdx].outcomeCount[8];
						if (g_missionFlightGroups[targetFlightGroupIdx].fg.specialCargoCraft ==
							craft->waveNumber)
							g_missionFgStats[targetFlightGroupIdx].specialCargoOutcome[8] = 1;
					}
				} else {
					fsfx_PlaySound(60, 0xffffu, (unsigned int)g_localPlayer);
				}
			}

			++g_paiContext.aiController->orderScratch
				  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
							   [g_paiContext.curOrderCoord.fields.orderSlot];
			g_curCraft->aiFlight.objSignatures[g_curCraft->aiFlight.objSignatureCount++] = targetSignature;
			if (g_curCraft->aiFlight.objSignatureCount >= 10u)
				--g_curCraft->aiFlight.objSignatureCount;
			if (g_curCraft->aiFlight.objSignatureCount == 1) {
				++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[12];
				if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg.specialCargoCraft == g_curCraft->waveNumber)
					g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.specialCargoOutcome[12] = 1;
			}
			if (g_objectTable[targetObjIdx].mobj != NULL) {
				++craft->aiFlight.orderActionCounter;
				++g_missionFgStats[targetFlightGroupIdx].outcomeCount[10];
				if (g_missionFlightGroups[targetFlightGroupIdx].fg.specialCargoCraft == craft->waveNumber)
					g_missionFgStats[targetFlightGroupIdx].specialCargoOutcome[10] = 1;
			}
			g_paiContext.aiController->maneuverPhase = 3;
			g_paiContext.aiController->maneuverTimer = 2360;
			if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "boardtopickuppln") == 0)
				g_paiContext.aiController->maneuverTimer = 236;
			if (g_objectTable[targetObjIdx].playerOwnerIdx == -1 && g_curCraft->followPlayerMode != 1)
				return 0;
			g_paiContext.aiController->candidateTargetIdx = 0xffffu;
			return 0;
		}
		case 3:
			if (g_paiContext.aiController->maneuverTimer != 0) {
				if (g_objectTable[targetObjIdx].mobj == NULL) {
					g_curCraft->pushAccumX = 0;
					g_curCraft->pushAccumY = 0;
					g_curCraft->pushAccumZ = 500;
					return 0;
				}
				pai_calcrotatedpoint(&g_objectTable[g_paiContext.aiObjIdx], 0, 0x4000, 0);
				g_curCraft->pushAccumX = g_rotatedX + g_objectTable[targetObjIdx].world_x -
										 g_objectTable[g_paiContext.aiObjIdx].world_x;
				g_curCraft->pushAccumY = g_rotatedY + g_objectTable[targetObjIdx].world_y -
										 g_objectTable[g_paiContext.aiObjIdx].world_y;
				g_curCraft->pushAccumZ = g_rotatedZ + g_objectTable[targetObjIdx].world_z -
										 g_objectTable[g_paiContext.aiObjIdx].world_z;
				return 0;
			}
			g_paiContext.aiController->targetObjIdx = 0xffffu;
			g_paiContext.aiController->targetSignature = 0;
			g_paiContext.aiController->hasLiveTarget = 0;
			return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4AE290
// Advance the active hyperspace plan to its next enabled waypoint. Returns 1 when
// the pending plan is not the hyperspace plan, or the run has no further enabled
// waypoint (the caller treats this as "done"). Otherwise retargets to the new
// waypoint, refreshes the aim point (adding the rotated formation offset for
// wingmen), and returns 0.
char paiman_AdvanceHyperspaceWaypoint(void) {
	if (strcmp(g_planTable[g_paiContext.aiController->pendingPlanId].name, "hyperspacepln") != 0) {
		return 1;
	}

	++g_paiContext.aiController->waypointIndex;
	if (g_paiContext.aiController->waypointIndex == 8 ||
		g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
				.fg
				.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
						g_paiContext.curOrderCoord.fields.orderSlot]
				.waypoints[(uint8_t)g_paiContext.aiController->waypointIndex]
				.enabled == 0) {
		return 1;
	}

	g_paiContext.aiController->targetObjIdx =
		(uint16_t)((uint8_t)g_paiContext.aiController->waypointIndex - 0x7FFC);
	g_paiContext.aiController->targetSignature = 0;
	g_paiContext.aiController->hasLiveTarget = 0;
	pai_UpdateAimPointFromOrderTarget();
	if (g_curCraft->leader_obj_idx != -1) {
		paiman_calcformation();
		g_curCraft->pushAccumX = 0;
		g_curCraft->pushAccumY = 0;
		g_curCraft->pushAccumZ = 0;
		g_paiContext.aiController->aimPointX += g_rotatedX;
		g_paiContext.aiController->aimPointY += g_rotatedY;
		g_paiContext.aiController->aimPointZ += g_rotatedZ;
	}
	return 0;
}

// FUNCTION: XWA 0x4B8870
// Returns whether objectIdx is a current objective target for playerIdx and
// goalCondType. Checks the enabled FG goals in the object's flight group plus the
// runtime global-goal trigger slots for the player's IFF/team.
int paiman_IsObjectCurrentPlayerObjectiveTarget(int goalCondType, int playerIdx, uint16_t objectIdx) {
	uint16_t playerIff;
	int result;
	int sawCompletedGoal;
	unsigned int flightGroupIdx;
	unsigned int goalIdx;
	int triggerCondition;

	result = 0;
	sawCompletedGoal = 0;
	playerIff = g_players[playerIdx].playerIff;
	flightGroupIdx = g_objectTable[objectIdx].flightGroupIdx;
	if (flightGroupIdx > (unsigned int)(int16_t)g_missionHeader.numFlightGroups) {
		return 0;
	}

	for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
		if (g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.enabledForTeam[playerIff] &&
			g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.argument ==
				(uint8_t)goalCondType &&
			g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.points >= 0) {
			if (g_missionFgStats[flightGroupIdx].goalState[8 * playerIff + goalIdx] == 4) {
				result = 1;
				sawCompletedGoal = 1;
			} else {
				result = sawCompletedGoal;
			}
		}
	}

	triggerCondition = g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[0].condition;
	if (triggerCondition != 10 && triggerCondition != 0) {
		if ((uint16_t)Mission_ObjectMatchesTriggerVariable(
				objectIdx,
				g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[0].variableType,
				g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[0].variable)) {
			result = 1;
		}
	}
	triggerCondition = g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[1].condition;
	if (triggerCondition != 10 && triggerCondition != 0) {
		if ((uint16_t)Mission_ObjectMatchesTriggerVariable(
				objectIdx,
				g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[1].variableType,
				g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[0].triggers[1].variable)) {
			result = 1;
		}
	}
	triggerCondition = g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[0].condition;
	if (triggerCondition != 10 && triggerCondition != 0) {
		if ((uint16_t)Mission_ObjectMatchesTriggerVariable(
				objectIdx,
				g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[0].variableType,
				g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[0].variable)) {
			result = 1;
		}
	}
	triggerCondition = g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[1].condition;
	if (triggerCondition != 10 && triggerCondition != 0) {
		if ((uint16_t)Mission_ObjectMatchesTriggerVariable(
				objectIdx,
				g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[1].variableType,
				g_missionGlobalGoals[playerIff][goalCondType].triggerPairs[1].triggers[1].variable)) {
			result = 1;
		}
	}

	return result != 0;
}

// FUNCTION: XWA 0x4AE540
static char paiman_intohyperspacemaneuver(void) {
	if (g_curCraft->workingSubsystems == 0) {
		return 0;
	}

	switch ((uint8_t)g_paiContext.aiController->maneuverPhase) {
		case 0: {
			/* approach the departure point / begin hyperspace */
			int variable1;
			int playerOwnerIdx;
			/* Preserve the original legacy code shape while keeping modern builds deterministic. */
#ifdef XWA_MODERN
			int orderIdx = 0;
#else
			int orderIdx;
#endif
			int isHyperPlan;
			int routeIdx;
			int deltaX;
			int deltaY;
			int deltaZ;
			ObjectRecord* selfObj;
			MobileObject* selfMobj;

			pai_CalcAnglesToAimPoint();
			if ((int)trig2_polardistance >= 0x4000 || !paiman_AdvanceHyperspaceWaypoint()) {
				goto LABEL_187;
			}

			g_curCraft->objectKind = 5;
			g_curCraft->aiFlight.enterFlag = 0;
			g_curCraft->aiFlight.headingState = 0;
			g_curCraft->aiFlight.turnState = 0;

			if (g_curCraft->leader_obj_idx == -1) {
				isHyperPlan = 0;
				if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "hyperspacepln") ==
					0) {
					isHyperPlan = 1;
					variable1 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
									.fg
									.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
											g_paiContext.curOrderCoord.fields.orderSlot]
									.variable1;
					orderIdx = variable1;
				} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
								  "homeviahyperspacepln") == 0) {
					isHyperPlan = 1;
					orderIdx = g_paiContext.aiController->targetObjIdx;
					variable1 = orderIdx;
				} else {
					variable1 = orderIdx;
				}

				if (isHyperPlan == 0) {
					goto LABEL_181;
				}

				routeIdx = variable1 + 4 * g_paiContext.curOrderCoord.fields.regionIdx +
						   g_paiContext.curOrderCoord.fields.regionIdx;
				if (g_missionRegionHyperPoints.departureRoutePointValid[0][routeIdx] != 0) {
					g_paiContext.aiController->aimPointX =
						g_missionRegionHyperPoints.departureRoutePoint[0][routeIdx].x;
					g_paiContext.aiController->aimPointY =
						g_missionRegionHyperPoints
							.departureRoutePoint[0][4 * g_paiContext.curOrderCoord.fields.regionIdx +
													variable1 + g_paiContext.curOrderCoord.fields.regionIdx]
							.y;
					g_paiContext.aiController->aimPointZ =
						g_missionRegionHyperPoints
							.departureRoutePoint[0][4 * g_paiContext.curOrderCoord.fields.regionIdx +
													variable1 + g_paiContext.curOrderCoord.fields.regionIdx]
							.z;

					deltaX = g_paiContext.aiController->aimPointX - g_paiContext.aiCurrentPointX;
					deltaZ = g_paiContext.aiController->aimPointZ - g_paiContext.aiCurrentPointZ;
					deltaY = g_paiContext.aiController->aimPointY - g_paiContext.aiCurrentPointY;
					selfObj = &g_objectTable[g_paiContext.aiObjIdx];
					if (selfObj->mobj->orientMatrixDirty) {
						FVIEW_calcrotatemove(selfObj->pitch, selfObj->yaw,
											 &g_objectTable[g_paiContext.aiObjIdx]);
						FVIEW_calcrotateorient(selfObj->roll, selfObj->angleD, selfObj);
					}
					selfMobj = selfObj->mobj;
					g_paiContext.aiController->aimPointX =
						paiman_LocalDotQ15UnsignedShift(deltaX, deltaY, deltaZ, selfMobj->cachedSideX,
														selfMobj->cachedSideY, selfMobj->cachedSideZ);
					selfMobj = selfObj->mobj;
					g_paiContext.aiController->aimPointY =
						-paiman_LocalDotQ15UnsignedShift(deltaX, deltaY, deltaZ, selfMobj->cachedFwdX,
														 selfMobj->cachedFwdY, selfMobj->cachedFwdZ);
					selfMobj = selfObj->mobj;
					g_paiContext.aiController->aimPointZ =
						paiman_LocalDotQ15UnsignedShift(deltaX, deltaY, deltaZ, selfMobj->cachedUpX,
														selfMobj->cachedUpY, selfMobj->cachedUpZ);
				} else {
					g_paiContext.aiController->aimPointX = 0;
					g_paiContext.aiController->aimPointY = 0;
					g_paiContext.aiController->aimPointZ = 0;
				}
			}

			variable1 = orderIdx;

		LABEL_181:
			g_paiContext.aiController->waypointIndex = 1;
			g_paiContext.aiController->maneuverPhase = 1;
			playerOwnerIdx = g_objectTable[g_paiContext.aiObjIdx].playerOwnerIdx;
			if (playerOwnerIdx != -1) {
				if (playerOwnerIdx == g_localPlayer) {
					g_players[g_localPlayer].padlockActive = 0;
					g_players[g_localPlayer].lookYawOffset = 0;
					g_players[g_localPlayer].lookPitchOffset = 0;
					Hud_ClearReadyMessageQueue();
					Flight_InitOutboundHyperspaceStreaks();
					FlightLight_SetLocalPlayerPulseEnabled(4, 0);
					FlightLight_SetLocalPlayerPulseEnabled(5, 0);
					FlightLight_SetLocalPlayerPulseEnabled(3, 1);
					g_localPlayerLightPulses[3].startTime = g_gameTime;
				}

				g_players[playerOwnerIdx].hyperspacePhase = PLAYER_HYPERSPACE_OUTBOUND;
				g_players[playerOwnerIdx].hyperspaceRuntime.targetRegionOrMode = (uint8_t)variable1;
				g_players[playerOwnerIdx].hyperspaceRuntime.phaseElapsedTicks = 0;
				g_players[playerOwnerIdx].viewState.externalCameraActive = 0;
				g_players[playerOwnerIdx].viewState.playerInputBlocked = 0;
				g_players[playerOwnerIdx].viewState.cameraFocusObjIdx = g_players[playerOwnerIdx].objectIndex;
				Hud_SetHudViewState(19, playerOwnerIdx);
				g_players[playerOwnerIdx].viewState.hudAimX = 0;
				g_players[playerOwnerIdx].viewState.hudAimY = 0;
				g_players[playerOwnerIdx].viewState.cameraPanDeltaX = 0;
				g_players[playerOwnerIdx].viewState.cameraPanDeltaY = 0;
				g_players[playerOwnerIdx].viewState.cameraPanDeltaZ = 0;
				g_players[playerOwnerIdx].viewState.cameraPitchDelta = 0;
				g_players[playerOwnerIdx].viewState.cameraYawDelta = 0;
				g_players[playerOwnerIdx].viewState.cameraRollDelta = 0;
				g_players[playerOwnerIdx].viewState.field_32 = 0;
				++g_paiContext.aiController->orderScratch
					  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
								   [g_paiContext.curOrderCoord.fields.orderSlot];
				if (pai_IsPlanCompleteForOrderSlot(g_paiContext.aiController->currentPlanId,
												   g_paiContext.curOrderCoord.fields.orderSlot,
												   g_paiContext.curOrderCoord.fields.regionIdx)) {
					uint8_t* completion = &g_paiContext.aiController->orderScratch
											   .completionState[g_paiContext.curOrderCoord.fields.regionIdx]
															   [g_paiContext.curOrderCoord.fields.orderSlot];
					*completion = (*completion != 0) + 3;
				}
				g_paiContext.aiController->maneuverPhase = 3;
			}

		LABEL_187:
			if (g_curCraft->objectKind != 5) {
				paiman_setflighttotarget(0, 1);
				if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "hyperspacepln") ==
					0) {
					int throttleSpeed = g_orderThrottleToCraftThrottleSpeed
						[g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
							 .fg
							 .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
									 g_paiContext.curOrderCoord.fields.orderSlot]
							 .throttle];
					g_curCraft->throttleSpeed = (uint16_t)throttleSpeed;
					return 0;
				}
				g_curCraft->throttleSpeed = (uint16_t)-1;
			}
			return 0;
		}

		case 1: {
			/* perform the departure */
			int mothershipDepart;
			int currentPlanId;
			int departState;
			uint16_t flightGroupIdx;

			g_curCraft->objectKind = 5;
			mothershipDepart = 0;
			currentPlanId = g_paiContext.aiController->currentPlanId;
			departState = 0;
			if (!(strcmp(g_planTable[currentPlanId].name, "hyperspacepln") == 0 ||
				  strcmp(g_planTable[currentPlanId].name, "homeviahyperspacepln") == 0 ||
				  (mothershipDepart = 1,
				   g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.departMethod !=
					   2))) {
				departState = 1;
			}

			if (g_objectTable[g_paiContext.aiObjIdx].mobj->speed < 0xE10u && departState != 1) {
				return 0;
			}

			if (!departState) {
				msg_emitCraftMessage(g_paiContext.aiObjIdx, g_curCraft, 143);
			} else {
				msg_emitCraftMessage(g_paiContext.aiObjIdx, g_curCraft, 532);
			}

			if (mothershipDepart) {
				CraftData* craft;

				craft = g_curCraft;
				if (!craft->wasCaptured) {
					if (!g_paiContext.aiController->orderStateFlag &&
						(g_curCraft->aiFlight.goHomeFlag || (!g_curCraft->aiFlight.missionAbortedFlag &&
															 !g_curCraft->aiFlight.departTimerFlag))) {
						int specialCargoFlag = 0;
						flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
						++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[16];
						if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
							g_missionFgStats[flightGroupIdx].specialCargoOutcome[16] = 1;
							specialCargoFlag = 1;
						}
						Mission_ApplyTeamGoalScoreAllEnabledTeams(12, flightGroupIdx, specialCargoFlag);
						craft = g_curCraft;
					}
				}

				flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
				if (!craft->wasCaptured && g_paiContext.aiController->orderStateFlag == 1 &&
					!craft->aiFlight.missionAbortedFlag && !craft->aiFlight.departTimerFlag) {
					++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[27];
					if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[27] = 1;
					}
				}

				if (craft->wasCaptured) {
					int team;
					int specialCargoFlag;
					unsigned int k;

					team = g_objectTable[g_paiContext.aiObjIdx].mobj->team;
					specialCargoFlag = 0;
					++g_missionFgStats[flightGroupIdx].teamCondition44Count[team];
					if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
						specialCargoFlag = 1;
						++g_missionFgStats[flightGroupIdx].teamCondition44SpecialCargo[team];
					}
					Mission_ApplyTeamGoalScoreForTeam(44, flightGroupIdx, specialCargoFlag, (uint8_t)team);
					craft = g_curCraft;
					flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
					for (k = 0; k < 0xA; ++k) {
						if (k != (unsigned int)team && k != g_missionFlightGroups[flightGroupIdx].fg.team) {
							uint8_t specialCargoCraft =
								g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft;
							++g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamCount[k];
							if (specialCargoCraft == craft->waveNumber) {
								g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamSpecialCargo[k] = 1;
							}
						}
					}
				}

				Mission_RecordCraftOutcome(g_paiContext.aiObjIdx, flightGroupIdx, 0x11u);
				if (g_inHangarReady && (uint32_t)g_hangarSourceObjIdx == g_paiContext.aiObjIdx) {
					Player_HandleHyperspaceCommand(
						g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft, g_localPlayer, 4);
				}
				g_objectTable[g_paiContext.aiObjIdx].objectType = OBJ_None;
				Craft_ClearEffectiveAiObjectLink(g_curCraft);
				if (Object_IsHostileToTeam(g_paiContext.aiObjIdx,
										   (uint16_t)g_players[g_localPlayer].playerIff) &&
					paiman_IsObjectCurrentPlayerObjectiveTarget(0, g_localPlayer, g_paiContext.aiObjIdx)) {
					fsfx_speakorderack(g_localPlayer, -1, 28, -1, 0xFFFFu, 0xFFFFu);
				}

				craft = g_curCraft;
				if (g_curCraft->carriedObjectIndex != 0xFFFF) {
					uint16_t carriedIdx = g_curCraft->carriedObjectIndex;
					if (g_objectTable[carriedIdx].mobj) {
						uint16_t carriedFg = g_objectTable[carriedIdx].flightGroupIdx;
						MobileObject* carriedMobj;
						CraftData* carriedCraft;

						Mission_RecordCraftOutcome(carriedIdx, carriedFg, 0x11u);
						carriedMobj = g_objectTable[carriedIdx].mobj;
						carriedCraft = carriedMobj->pCraft;
						if (carriedCraft->wasCaptured) {
							int team2 = carriedMobj->team;
							int specialCargoFlag;
							unsigned int m;

							++g_missionFgStats[carriedFg].teamCondition44Count[team2];
							if (g_missionFlightGroups[carriedFg].fg.specialCargoCraft ==
								carriedCraft->waveNumber) {
								++g_missionFgStats[carriedFg].teamCondition44SpecialCargo[team2];
								specialCargoFlag = 1;
							} else {
								specialCargoFlag = 0;
							}
							Mission_ApplyTeamGoalScoreForTeam(44, carriedFg, specialCargoFlag,
															  (uint8_t)team2);
							for (m = 0; m < 0xA; ++m) {
								if (m != (unsigned int)team2 &&
									m != g_missionFlightGroups[carriedFg].fg.team) {
									++g_missionFgStats[carriedFg].teamCondition44OtherTeamCount[m];
									if (g_missionFlightGroups[carriedFg].fg.specialCargoCraft ==
										carriedCraft->waveNumber) {
										g_missionFgStats[carriedFg].teamCondition44OtherTeamSpecialCargo[m] =
											1;
									}
								}
							}
							++g_missionFgStats[carriedFg].outcomeCount[24];
							if (g_missionFlightGroups[carriedFg].fg.specialCargoCraft ==
								carriedCraft->waveNumber) {
								g_missionFgStats[carriedFg].specialCargoOutcome[24] = 1;
							}
						} else {
							int specialCargoFlag = 0;
							++g_missionFgStats[carriedFg].outcomeCount[23];
							if (g_missionFlightGroups[carriedFg].fg.specialCargoCraft ==
								carriedCraft->waveNumber) {
								g_missionFgStats[carriedFg].specialCargoOutcome[23] = 1;
								specialCargoFlag = 1;
							}
							Mission_ApplyTeamGoalScoreAllEnabledTeams(46, carriedFg, specialCargoFlag);
						}
						g_objectTable[carriedIdx].objectType = OBJ_None;
						Craft_ClearEffectiveAiObjectLink(carriedCraft);
						craft = g_curCraft;
					}
				}

				if (craft->nextLinkObjectIdx == 0xFFFF) {
					uint16_t link = craft->linkedPrevObjectIdx;
					if (link != 0xFFFF) {
						do {
							CraftData* linkCraft = g_objectTable[link].mobj->pCraft;
							Mission_RecordCraftOutcome(link, g_objectTable[link].flightGroupIdx, 0x11u);
							g_objectTable[link].objectType = OBJ_None;
							Craft_ClearEffectiveAiObjectLink(linkCraft);
							link = linkCraft->linkedPrevObjectIdx;
						} while (link != 0xFFFF);
					}
				}
				return 0;
			}

			g_curCraft->objectKind = 7;
			++g_paiContext.aiController->orderScratch
				  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
							   [g_paiContext.curOrderCoord.fields.orderSlot];
			if (pai_IsPlanCompleteForOrderSlot(g_paiContext.aiController->currentPlanId,
											   g_paiContext.curOrderCoord.fields.orderSlot,
											   g_paiContext.curOrderCoord.fields.regionIdx)) {
				uint8_t* completion = &g_paiContext.aiController->orderScratch
										   .completionState[g_paiContext.curOrderCoord.fields.regionIdx]
														   [g_paiContext.curOrderCoord.fields.orderSlot];
				*completion = (*completion != 0) + 3;
			}

			flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
			++g_missionFgStats[flightGroupIdx]
				  .tailEventCounts[g_objectTable[g_paiContext.aiObjIdx].regionIdx + 10];
			if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == g_curCraft->waveNumber) {
				++g_missionFgStats[flightGroupIdx]
					  .tailEventCounts[g_objectTable[g_paiContext.aiObjIdx].regionIdx + 15];
			}
			g_paiContext.aiController->maneuverPhase = 2;
			g_paiContext.aiController->aiPlanState = 944;

			if (g_curCraft->carriedObjectIndex != 0xFFFF) {
				uint16_t carriedIdx = g_curCraft->carriedObjectIndex;
				MobileObject* m = g_objectTable[carriedIdx].mobj;
				if (m) {
					CraftData* c = m->pCraft;
					if (c) {
						c->objectKind = 7;
						++g_missionFgStats[g_objectTable[carriedIdx].flightGroupIdx]
							  .tailEventCounts[g_objectTable[carriedIdx].regionIdx + 10];
						if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg.specialCargoCraft == c->waveNumber) {
							++g_missionFgStats[g_objectTable[carriedIdx].flightGroupIdx]
								  .tailEventCounts[g_objectTable[carriedIdx].regionIdx + 15];
						}
					}
				}
			}

			if (g_curCraft->nextLinkObjectIdx != 0xFFFF) {
				return 0;
			}
			{
				uint16_t link = g_curCraft->linkedPrevObjectIdx;
				if (link == 0xFFFF) {
					return 0;
				}
				do {
					CraftData* linkCraft = g_objectTable[link].mobj->pCraft;
					linkCraft->objectKind = 7;
					if (g_objectTable[link].flightGroupIdx !=
						g_objectTable[g_paiContext.aiObjIdx].flightGroupIdx) {
						++g_missionFgStats[g_objectTable[link].flightGroupIdx]
							  .tailEventCounts[g_objectTable[link].regionIdx + 10];
						if (g_missionFlightGroups[g_objectTable[link].flightGroupIdx].fg.specialCargoCraft ==
							linkCraft->waveNumber) {
							++g_missionFgStats[g_objectTable[link].flightGroupIdx]
								  .tailEventCounts[g_objectTable[link].regionIdx + 15];
						}
					}
					link = linkCraft->linkedPrevObjectIdx;
				} while (link != 0xFFFF);
			}
			return 0;
		}

		case 2:
			if (g_paiContext.aiController->aiPlanState != 0) {
				return 0;
			}
			/* relocate craft + cargo + links into the destination region */
			{
				AiController* effectiveAi;
				int orderIdx;
				int savedRegionIdx;
				int oldSelfObjIdx;
				unsigned int freeSlot;
				int flightGroupIdx;
				int arrivalX;
				int arrivalY;
				int arrivalZ;
				unsigned int foundOrderSlot;
				int allSlotsBusy;
				int worldX;
				int worldY;
				int worldZ;
				Q16Angle spawnYaw;
				Q16Angle spawnPitch;
				PaiContext savedContext;
				CraftData* savedCurCraft;
				CraftData* spawnCraft;
				unsigned int scan;
				uint16_t linkedPrevObjectIdx;
				int prevNewSlot;

				if (g_curCraft->leader_obj_idx != -1) {
					effectiveAi = pai_GetEffectiveAIController(g_paiContext.aiTargetCraft);
					if (g_objectTable[g_paiContext.aiLeaderObjIdx].objectType &&
						(g_paiContext.aiTargetCraft->objectKind == 7 || effectiveAi->maneuverMode == 21)) {
						g_paiContext.aiController->aiPlanState = 59;
						return 0;
					}
				}

				if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
						   "homeviahyperspacepln") == 0) {
					orderIdx = (uint16_t)g_paiContext.aiController->targetObjIdx;
				} else {
					orderIdx = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								   .fg
								   .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										   g_paiContext.curOrderCoord.fields.orderSlot]
								   .variable1;
				}

				savedRegionIdx = regionIdx;
				Mission_SetActiveRegionObjectRanges(orderIdx);

				freeSlot = g_activeRegionObjectSlotStart;
				for (; freeSlot < g_activeRegionCraftObjectSlotEnd; ++freeSlot) {
					if (g_objectTable[freeSlot].objectType == OBJ_None) {
						break;
					}
				}
				if (freeSlot < g_activeRegionCraftObjectSlotEnd) {

					oldSelfObjIdx = g_paiContext.aiObjIdx;
					Object_CopyStatePreservingStorage(freeSlot, g_paiContext.aiObjIdx);
					g_objectTable[freeSlot].regionIdx = orderIdx;
					if (g_inHangarReady && g_hangarSourceObjIdx == oldSelfObjIdx) {
						g_hangarSavedMissionRegionIdx = (uint16_t)orderIdx;
						g_hangarSourceObjIdx = freeSlot;
					}

					flightGroupIdx = g_paiContext.curOrderCoord.fields.flightGroupIdx;
					++g_missionFgStats[flightGroupIdx].tailEventCounts[orderIdx];
					if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft ==
						g_curCraft->waveNumber) {
						++g_missionFgStats[flightGroupIdx].tailEventCounts[orderIdx + 5];
					}

					if (g_missionRegionHyperPoints.arrivalPointValid[0][5 * orderIdx + savedRegionIdx]) {
						arrivalX =
							g_missionRegionHyperPoints.arrivalPoint[0][5 * orderIdx + savedRegionIdx].x;
						arrivalY =
							g_missionRegionHyperPoints.arrivalPoint[0][5 * orderIdx + savedRegionIdx].y;
						arrivalZ =
							g_missionRegionHyperPoints.arrivalPoint[0][5 * orderIdx + savedRegionIdx].z;
					} else {
						arrivalX = 0;
						arrivalY = 0;
						arrivalZ = 0;
					}

					foundOrderSlot = 0;
					allSlotsBusy = 0;
					for (foundOrderSlot = 0; foundOrderSlot < 4; ++foundOrderSlot) {
						if (g_paiContext.aiController->orderScratch
								.completionState[orderIdx][foundOrderSlot] == 0) {
							break;
						}
					}
					if (foundOrderSlot == 4) {
						unsigned int i;
						for (i = 0; i < 4; ++i) {
							if (g_paiContext.aiController->orderScratch.completionState[orderIdx][i] == 1) {
								break;
							}
						}
						if (i == 4) {
							allSlotsBusy = 1;
						}
						foundOrderSlot = 0;
					}

					if (g_missionFlightGroups[flightGroupIdx]
							.fg.orders[4 * orderIdx + foundOrderSlot]
							.waypoints[0]
							.enabled) {
						Mission_ResolveObjectOrMissionPointWorldLoc(0x8004u, flightGroupIdx, orderIdx,
																	foundOrderSlot);
						trig2_ctop(worldlocx - arrivalX, worldlocy - arrivalY, worldlocz - arrivalZ);
						spawnYaw = trig2_xyangle;
						spawnPitch = targetPitch;
					} else {
						spawnPitch = 0x4000;
						spawnYaw = 0;
					}
					targetPitch = spawnPitch;
					trig2_xyangle = spawnYaw;

					g_objectTable[freeSlot].yaw = spawnYaw;
					g_objectTable[freeSlot].pitch = spawnPitch;
					g_objectTable[freeSlot].roll = 0;
					g_objectTable[freeSlot].angleD = 0;
					g_objectTable[freeSlot].mobj->orientMatrixDirty = 1;
					g_objectTable[freeSlot].mobj->moveVectorDirty = 1;

					if (g_curCraft->leader_obj_idx != -1) {
						paiman_calcformation();
						g_curCraft->pushAccumX = 0;
						g_curCraft->pushAccumY = 0;
						g_curCraft->pushAccumZ = 0;
						worldX = arrivalX + g_rotatedX;
						worldY = arrivalY + g_rotatedY;
						worldZ = arrivalZ + g_rotatedZ;
					} else {
						pai_RotateLocalVectorToWorldScratch(
							&g_objectTable[freeSlot], g_paiContext.aiController->aimPointX,
							g_paiContext.aiController->aimPointZ, g_paiContext.aiController->aimPointY);
						worldX = arrivalX - g_rotatedX;
						worldY = arrivalY + g_rotatedY;
						worldZ = arrivalZ - g_rotatedZ;
					}

					trig2_xyangle += 0x8000;
					targetPitch = 0x8000 - targetPitch;
					trig2_movexyz(0xFFFF, trig2_xyangle, targetPitch);
					trig2_ymovedist *= 8;
					trig2_zmovedist *= 8;
					g_objectTable[freeSlot].world_x = worldX + 8 * trig2_xmovedist;
					trig2_xmovedist *= 8;
					g_objectTable[freeSlot].world_y = worldY + trig2_ymovedist;
					g_objectTable[freeSlot].world_z = worldZ + trig2_zmovedist;
					g_objectTable[freeSlot].mobj->prevWorldX = g_objectTable[freeSlot].world_x;
					g_objectTable[freeSlot].mobj->prevWorldY = g_objectTable[freeSlot].world_y;
					g_objectTable[freeSlot].mobj->prevWorldZ = g_objectTable[freeSlot].world_z;
					if (g_objectTable[freeSlot].mobj->pCraft) {
						g_objectTable[freeSlot].mobj->pCraft->aiController.aimPointX = worldX;
						g_objectTable[freeSlot].mobj->pCraft->aiController.aimPointY = worldY;
						g_objectTable[freeSlot].mobj->pCraft->aiController.aimPointZ = worldZ;
					}

					savedContext = g_paiContext;
					spawnCraft = g_objectTable[freeSlot].mobj->pCraft;
					savedCurCraft = g_curCraft;
					g_curCraft = spawnCraft;
					spawnCraft->aiController.pendingPlanId = g_builtinPlanIdByNameIndex[52];
					if (allSlotsBusy) {
						g_curCraft->aiController.currentPlanId = g_builtinPlanIdByNameIndex[47];
					} else if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name,
									  "homeviahyperspacepln")) {
						g_curCraft->aiController.currentPlanId = g_builtinPlanIdByNameIndex
							[g_orderLeaderBuiltinPlanNameIndex
								 [g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
									  .fg.orders[4 * orderIdx + foundOrderSlot]
									  .order]];
					}
					g_curCraft->aiController.currentOrderSlot = (char)foundOrderSlot;
					pai_setupcraftcontext(freeSlot);
					pai_ApplyPendingPlanTargetAndManeuver(freeSlot);
					g_curCraft = savedCurCraft;
					g_paiContext = savedContext;
					g_objectTable[savedContext.aiObjIdx].objectType = OBJ_None;

					if (g_curCraft->leader_obj_idx == -1) {
						for (scan = 0; scan < g_regionObjectSlotEnd; ++scan) {
							if (g_objectTable[scan].objectType) {
								MobileObject* m = g_objectTable[scan].mobj;
								if (m) {
									CraftData* c = m->pCraft;
									if (c && c->leader_obj_idx == oldSelfObjIdx) {
										c->leader_obj_idx = freeSlot;
									}
								}
							}
						}
					}

					if (g_curCraft->carriedObjectIndex != 0xFFFF) {
						unsigned int carrierFreeSlot;
						carrierFreeSlot = g_activeRegionObjectSlotStart;
						if (g_activeRegionObjectSlotStart >= g_activeRegionCraftObjectSlotEnd) {
							goto LABEL_74;
						}
						for (; carrierFreeSlot < g_activeRegionCraftObjectSlotEnd; ++carrierFreeSlot) {
							if (g_objectTable[carrierFreeSlot].objectType == OBJ_None) {
								break;
							}
						}
						if (carrierFreeSlot >= g_activeRegionCraftObjectSlotEnd) {
							goto LABEL_74;
						}

						Object_CopyStatePreservingStorage(carrierFreeSlot, g_curCraft->carriedObjectIndex);
						g_objectTable[carrierFreeSlot].regionIdx = orderIdx;
						++g_missionFgStats[g_objectTable[carrierFreeSlot].flightGroupIdx]
							  .tailEventCounts[orderIdx];
						{
							MobileObject* m = g_objectTable[carrierFreeSlot].mobj;
							if (m) {
								CraftData* c = m->pCraft;
								if (c) {
									int fg = g_objectTable[carrierFreeSlot].flightGroupIdx;
									c->objectKind = 0;
									if (g_missionFlightGroups[fg].fg.specialCargoCraft == c->waveNumber) {
										++g_missionFgStats[fg].tailEventCounts[orderIdx + 5];
									}
									c->carrierObjIdx = freeSlot;
								}
							}
						}
						{
							uint16_t oldCarried = g_curCraft->carriedObjectIndex;
							g_objectTable[freeSlot].mobj->pCraft->carriedObjectIndex = carrierFreeSlot;
							g_objectTable[oldCarried].objectType = OBJ_None;
						}
					}

				LABEL_74:
					linkedPrevObjectIdx = g_curCraft->linkedPrevObjectIdx;
					prevNewSlot = freeSlot;
					while (linkedPrevObjectIdx != 0xFFFF) {
						uint16_t linkFreeSlot;
						for (linkFreeSlot = (uint16_t)g_activeRegionObjectSlotStart;
							 linkFreeSlot < g_activeRegionCraftObjectSlotEnd; ++linkFreeSlot) {
							if (g_objectTable[linkFreeSlot].objectType == OBJ_None) {
								break;
							}
						}
						if (linkFreeSlot < g_activeRegionCraftObjectSlotEnd) {
							MobileObject* m;
							CraftData* c;
							uint8_t* newFgIdx;
							uint16_t prevSlot;

							Object_CopyStatePreservingStorage(linkFreeSlot, linkedPrevObjectIdx);
							g_objectTable[linkFreeSlot].regionIdx = orderIdx;
							m = g_objectTable[linkFreeSlot].mobj;
							newFgIdx = &g_objectTable[linkFreeSlot].flightGroupIdx;
							c = m->pCraft;
							if (*newFgIdx != g_objectTable[freeSlot].flightGroupIdx) {
								++g_missionFgStats[*newFgIdx].tailEventCounts[orderIdx];
								if (g_missionFlightGroups[*newFgIdx].fg.specialCargoCraft == c->waveNumber) {
									++g_missionFgStats[*newFgIdx].tailEventCounts[orderIdx + 5];
								}
							}
							prevSlot = (uint16_t)prevNewSlot;
							c->objectKind = 0;
							prevNewSlot = linkFreeSlot;
							g_objectTable[prevSlot].mobj->pCraft->linkedPrevObjectIdx = linkFreeSlot;
							c->nextLinkObjectIdx = prevSlot;
							g_objectTable[linkedPrevObjectIdx].objectType = OBJ_None;
							linkedPrevObjectIdx =
								g_objectTable[linkedPrevObjectIdx].mobj->pCraft->linkedPrevObjectIdx;
						}
					}

				} else {
					g_paiContext.aiController->aiPlanState = 236;
				}

				Mission_SetActiveRegionObjectRanges(savedRegionIdx);
				return 0;
			}

		default:
			return 0;
	}
}

// FUNCTION: XWA 0x4AFE20
static char paiman_outofhyperspacemaneuver(void) {
	AiController* aiController;
	char reached;
	uint8_t planId;
	PlanRecord* plan;

	pai_GetEffectiveAIController(g_paiContext.aiTargetCraft);
	aiController = g_paiContext.aiController;
	if (g_paiContext.aiController->aiPlanState == 0) {
		++g_paiContext.aiController->maneuverPhase;
		if ((uint8_t)g_paiContext.aiController->maneuverPhase > 10) {
			g_paiContext.aiController->maneuverPhase = 10;
		}
		g_objectTable[g_paiContext.aiObjIdx].mobj->speed =
			g_aiCourseOrderSpeedByManeuverPhase[(uint8_t)g_paiContext.aiController->maneuverPhase];
		g_paiContext.aiController->aiPlanState = 236;
		aiController = g_paiContext.aiController;
	}

	reached = 0;
	trig2_ctop(aiController->aimPointX - g_objectTable[g_paiContext.aiObjIdx].world_x,
			   aiController->aimPointY - g_objectTable[g_paiContext.aiObjIdx].world_y,
			   aiController->aimPointZ - g_objectTable[g_paiContext.aiObjIdx].world_z);

	if (g_objectTable[g_paiContext.aiObjIdx].objectType == OBJ_SuperStarDestroyer &&
		trig2_polardistance > g_paiContext.aiController->maneuverDist &&
		g_paiContext.aiController->maneuverPhase == 10) {
		reached = 1;
	} else if (trig2_polardistance < g_paiContext.aiController->maneuverDist ||
			   trig2_polardistance > 0x10000) {
		g_paiContext.aiController->maneuverDist = trig2_polardistance;
	} else {
		reached = 1;
	}

	if (reached) {
		if (strcmp(g_planTable[g_paiContext.aiController->currentPlanId].name, "homeviahyperspacepln") == 0) {
			if (g_curCraft->leader_obj_idx == -1) {
				planId = g_builtinPlanIdByNameIndex[47];
			} else {
				planId = g_builtinPlanIdByNameIndex[48];
			}
		} else {
			uint16_t order = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								 .fg
								 .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										 g_paiContext.curOrderCoord.fields.orderSlot]
								 .order;
			if (g_curCraft->leader_obj_idx == -1) {
				planId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
			} else {
				planId = g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
			}
		}

		pai_getplandataptrbyname("outofhyperspacepln")[3] = planId;
		g_paiContext.aiController->thinkInterval = g_curCraft->aiFlight.objSignatures[0];
		g_curCraft->objectKind = 0;
		g_paiContext.aiController->targetObjIdx = (uint16_t)-1;
		g_paiContext.aiController->targetSignature = 0;
		g_paiContext.aiController->hasLiveTarget = 0;

		plan = &g_planTable[planId];
		if (strcmp(plan->name, "nullpln") == 0 || strcmp(plan->name, "stationaryldrpln") == 0 ||
			strcmp(plan->name, "stationaryflwpln") == 0) {
			g_objectTable[g_paiContext.aiObjIdx].mobj->speed = 0;
			g_curCraft->throttleSpeed = 0;
			return 1;
		}

		g_objectTable[g_paiContext.aiObjIdx].mobj->speed = g_curCraft->aiFlight.maxSpeedCache;
		return 1;
	}

	return 0;
}

// FUNCTION: XWA 0x4B2A40
static char paiman_dropoffmaneuver(void) {
	uint16_t formationSlotIdx;
	uint16_t variable2;
	int leaderObjIdx;
	unsigned int scanSlot;
	int scanObjIdx;
	ObjectRecord* scanObj;
	int minZ;
	int dx;
	int dy;
	int dz;
	PaiContext savedContext;
	CraftData* savedCurCraft;

	if (g_paiContext.aiController->maneuverPhase == 0) {
		formationSlotIdx = (uint8_t)g_paiContext.aiController->waypointIndex;
		leaderObjIdx = -1;
		variable2 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.variable2;

		// Latch the flight group's standing (unled) leader as the formation basis.
		for (scanSlot = g_activeRegionObjectSlotStart; (uint16_t)scanSlot < g_activeRegionCraftObjectSlotEnd;
			 ++scanSlot) {
			scanObjIdx = (uint16_t)scanSlot;
			scanObj = &g_objectTable[scanObjIdx];
			if (scanObj->objectType != OBJ_None && scanObj->flightGroupIdx == variable2 &&
				scanObj->mobj->pCraft->leader_obj_idx == -1) {
				leaderObjIdx = scanObjIdx;
			}
		}

		Mission_ResolveFormationSlotWorldLoc(variable2, formationSlotIdx, leaderObjIdx);

		minZ = ModelBounds_GetMinZ((uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType);

		g_curCraft->pushAccumX = worldlocx - g_objectTable[g_paiContext.aiObjIdx].world_x;
		dx = g_curCraft->pushAccumX;
		g_curCraft->pushAccumY = worldlocy - g_objectTable[g_paiContext.aiObjIdx].world_y;
		dy = g_curCraft->pushAccumY;
		g_curCraft->pushAccumZ = worldlocz - minZ - g_objectTable[g_paiContext.aiObjIdx].world_z;
		dz = g_curCraft->pushAccumZ;

		trig2_ctop(dx, dy, dz);

		if (dx < 0) {
			dx = -dx;
		}
		if (dy < 0) {
			dy = -dy;
		}
		if (dz < 0) {
			dz = -dz;
		}

		if (dy + dx > 256 && g_objectTable[g_paiContext.aiObjIdx].yaw != trig2_xyangle) {
			g_curCraft->aiFlight.turnState = 2;
			g_curCraft->aiFlight.turnStep = 0x8000u;
			g_curCraft->aiFlight.turnAccel = -1;
			g_paiContext.aiController->targetXYAngle = trig2_xyangle;
		}

		if (dx + dy + dz < 32) {
			savedContext = g_paiContext;
			savedCurCraft = g_curCraft;
			g_currentFlightGroupIdx = variable2;
			g_spawnLeaderObjIdx = leaderObjIdx;
			Mission_StartFlightGroupArrival(formationSlotIdx);
			g_curCraft = savedCurCraft;
			g_paiContext = savedContext;
			++savedContext.aiController->maneuverPhase;
			g_paiContext.aiController->maneuverTimer = 118;
			g_curCraft->pushAccumZ = 1500;
		}
	} else if (g_paiContext.aiController->maneuverTimer == 0) {
		g_paiContext.aiController->maneuverPhase = 0;
		++g_paiContext.aiController->waypointIndex;
	}

	g_paiContext.aiController->orderScratch.goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
														[g_paiContext.curOrderCoord.fields.orderSlot] =
		g_paiContext.aiController->waypointIndex;
	return 0;
}

// FUNCTION: XWA 0x4B2D10
static char paiman_kamikazemaneuver(void) {
	pai_UpdateAimPointFromOrderTarget();
	paiman_attacktarget(0, 0);
	return 0;
}

// FUNCTION: XWA 0x4B2EF0
static char paiman_avoidattackermaneuver(void) {
	int yawJitter;
	uint16_t newXYAngle;
	uint16_t pitch;
	int pitchJitter;
	uint16_t groupAI;

	if (g_paiContext.aiController->maneuverTimer == 0) {
		g_curCraft->aiFlight.enterFlag = 4;
		return 1;
	}

	if (g_paiContext.aiController->aiPlanState == 0) {
		unsigned int effectiveSkill;

		yawJitter = GameRand() & 0xFFF;
		g_paiContext.aiController->maneuverPhase ^= 1;
		if (g_paiContext.aiController->maneuverPhase) {
			newXYAngle = g_objectTable[g_paiContext.aiObjIdx].yaw + yawJitter + 0x3000;
		} else {
			newXYAngle = g_objectTable[g_paiContext.aiObjIdx].yaw - yawJitter - 0x3000;
		}
		g_paiContext.aiController->targetXYAngle = newXYAngle;

		effectiveSkill = (unsigned int)pai_GetEffectiveSkillValue(g_curCraft) & 0xffffu;
		paiman_setturn((effectiveSkill >> 1) + 0x8000);
		g_paiContext.aiController->targetRoll ^= 0x8000;

		pitchJitter = GameRand() & 0xFFF;
		pitch = g_objectTable[g_paiContext.aiObjIdx].pitch;
		if (pitch < 0x4000) {
			g_paiContext.aiController->targetZAngle = pitchJitter + pitch + 0x2000;
			g_curCraft->aiFlight.headingState = 2;
		} else {
			g_paiContext.aiController->targetZAngle = pitch - pitchJitter - 0x2000;
			g_curCraft->aiFlight.headingState = 1;
		}

		groupAI = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx].fg.groupAI;
		g_paiContext.aiController->aiPlanState =
			(uint16_t)MATH2_fraction(g_aiAvoidAttackerDelayFracQ16ByGroupAI[groupAI], 0xEC) +
			236 * g_aiAvoidAttackerDelaySecondsByGroupAI[groupAI];
		g_paiContext.aiController->aiPlanState += (uint16_t)MATH2_fraction(GameRand(), 0xEC);

		if (groupAI >= 3 && (GameRand() & 7) == 7 && g_curCraft->cmTypeId == 2 && g_curCraft->cmAmmoCount &&
			!g_curCraft->cmFireCooldownTimer) {
			laser_createcountermeasureprojectile(g_paiContext.aiObjIdx, OBJ_WarheadFlare);
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4B3110
static char paiman_dodgemaneuver(void) {
	pai_UpdateAimPointFromOrderTarget();
	paiman_setflighttotarget(0, 1);
	return 0;
}

// FUNCTION: XWA 0x4B3250
static char paiman_orbitmaneuver(void) {
	bool orbitClockwise;
	char orbitDirection;
	uint8_t maneuverPhase;

	orbitDirection = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						 .fg
						 .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								 g_paiContext.curOrderCoord.fields.orderSlot]
						 .variable2;
	if (orbitDirection == 0)
		orbitClockwise = true;
	else
		orbitClockwise = false;
	maneuverPhase = g_paiContext.aiController->maneuverPhase;

	if (maneuverPhase == 0) {
		int centerX;
		int centerY;
		int centerZ;

		pai_CalcAnglesToAimPoint();
		if ((int)trig2_polardistance < 2048) {
			Mission_ResolveObjectOrMissionPointWorldLoc(
				0x8004u, g_paiContext.curOrderCoord.fields.flightGroupIdx,
				g_paiContext.curOrderCoord.fields.regionIdx, g_paiContext.curOrderCoord.fields.orderSlot);
			centerZ = worldlocz;
			centerX = worldlocx;
			centerY = worldlocy;

			Mission_ResolveObjectOrMissionPointWorldLoc(
				(uint16_t)(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
									   .fg
									   .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
											   g_paiContext.curOrderCoord.fields.orderSlot]
									   .waypoints[1]
									   .enabled != 0
							   ? 0x8005
							   : 0x8000),
				g_paiContext.curOrderCoord.fields.flightGroupIdx, g_paiContext.curOrderCoord.fields.regionIdx,
				g_paiContext.curOrderCoord.fields.orderSlot);

			g_paiContext.aiController->aimPointX = centerX;
			g_paiContext.aiController->aimPointY = centerY;
			g_paiContext.aiController->aimPointZ = 2 * centerZ - worldlocz;
			g_paiContext.aiController->maneuverPhase = 1;
			if (!orbitClockwise)
				paiman_setflighttotarget(0x3E00, 0);
			else
				paiman_setflighttotarget(0xC200, 0);
			g_paiContext.aiController->maneuverDist = 4 * g_paiContext.aiController->orbitRadius;
			g_paiContext.aiController->targetZAngle = 0x4000;
			g_curCraft->aiFlight.headingStep = -1;
			g_curCraft->aiFlight.headingForce = 0;
			if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch)
				g_curCraft->aiFlight.headingState = 1;
			else
				g_curCraft->aiFlight.headingState = 2;
			return 0;
		}
	} else {
		int centerX;
		int centerY;
		int centerZ;
		int targetX;
		int targetY;
		int deltaZx2;
		int deltaZ;

		Mission_ResolveObjectOrMissionPointWorldLoc(0x8004u, g_paiContext.curOrderCoord.fields.flightGroupIdx,
													g_paiContext.curOrderCoord.fields.regionIdx,
													g_paiContext.curOrderCoord.fields.orderSlot);
		centerY = worldlocy;
		centerX = worldlocx;
		centerZ = worldlocz;

		Mission_ResolveObjectOrMissionPointWorldLoc(
			(uint16_t)(g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								   .fg
								   .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										   g_paiContext.curOrderCoord.fields.orderSlot]
								   .waypoints[1]
								   .enabled != 0
						   ? 0x8005
						   : 0x8000),
			g_paiContext.curOrderCoord.fields.flightGroupIdx, g_paiContext.curOrderCoord.fields.regionIdx,
			g_paiContext.curOrderCoord.fields.orderSlot);

		if (maneuverPhase == 1) {
			targetX = 2 * centerX - worldlocx;
			targetY = 2 * centerY - worldlocy;
		} else {
			targetX = worldlocx;
			targetY = worldlocy;
		}

		trig2_ctop2dim(g_objectTable[g_paiContext.aiObjIdx].world_x - targetX,
					   g_objectTable[g_paiContext.aiObjIdx].world_y - targetY);

		if (((int)trig2_polardistance > g_paiContext.aiController->maneuverDist &&
			 (int)trig2_polardistance < 4096) ||
			(int)trig2_polardistance < 96) {
			g_paiContext.aiController->maneuverPhase ^= 3;
			if (g_paiContext.aiController->maneuverPhase == 1) {
				g_paiContext.aiController->aimPointZ = 2 * centerZ - worldlocz;
				++g_paiContext.aiController->orderScratch
					  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
								   [g_paiContext.curOrderCoord.fields.orderSlot];
			} else {
				g_paiContext.aiController->aimPointZ = worldlocz;
			}
			g_paiContext.aiController->maneuverDist = 4 * g_paiContext.aiController->orbitRadius;
		} else {
			g_paiContext.aiController->maneuverDist = trig2_polardistance;
		}

		deltaZx2 = 2 * (g_paiContext.aiController->aimPointZ - centerZ);
		deltaZ = g_paiContext.aiController->aimPointZ - g_objectTable[g_paiContext.aiObjIdx].world_z;
		if (deltaZx2 != 0 || deltaZ != 0) {
			int speedScaled = (g_objectTable[g_paiContext.aiObjIdx].mobj->speed << 16) / 3600;
			int arcTicks;
			if (speedScaled == 0)
				speedScaled = 1;
			arcTicks = (int)((double)g_paiContext.aiController->orbitRadius * g_aiOrbitHalfTurnRadians) /
					   speedScaled;
			if (arcTicks == 0)
				arcTicks = 1;
			if (deltaZx2 / arcTicks != 0)
				g_curCraft->pushAccumZ = deltaZx2 / arcTicks;
			else
				g_curCraft->pushAccumZ = deltaZ;
			if (deltaZx2 > 0) {
				if (g_objectTable[g_paiContext.aiObjIdx].world_z > g_paiContext.aiController->aimPointZ)
					g_curCraft->pushAccumZ = 0;
			} else {
				if (g_objectTable[g_paiContext.aiObjIdx].world_z < g_paiContext.aiController->aimPointZ)
					g_curCraft->pushAccumZ = 0;
			}
		} else {
			g_curCraft->pushAccumZ = 0;
		}

		trig2_ctop2dim(g_objectTable[g_paiContext.aiObjIdx].world_x - centerX,
					   g_objectTable[g_paiContext.aiObjIdx].world_y - centerY);
		if (g_paiContext.aiController->orbitRadius + 512 < (int)trig2_polardistance) {
			if (!orbitClockwise)
				paiman_setflighttotarget(0x3D00, 0);
			else
				paiman_setflighttotarget(0xC300, 0);
		} else if (g_paiContext.aiController->orbitRadius - 512 > (int)trig2_polardistance) {
			if (!orbitClockwise)
				paiman_setflighttotarget(0x4200, 0);
			else
				paiman_setflighttotarget(0xBE00, 0);
		} else {
			if (!orbitClockwise)
				paiman_setflighttotarget(0x3F00, 0);
			else
				paiman_setflighttotarget(0xC100, 0);
		}
		return 0;
	}

	paiman_setflighttotarget(0, 1);
	return 0;
}
#ifndef XWA_MODERN
// TODO: route through the platform debug-output layer. The original calls the
// Win32 import OutputDebugStringA; this temporary shim preserves the call site.
void OutputDebugStringA(const char* lpOutputString) { (void)lpOutputString; }
#endif

// FUNCTION: XWA 0x4B3A40
static char paiman_releasemaneuver(void) {
	if (g_curCraft->carriedObjectIndex == 0xFFFF) {
		g_paiContext.aiController->orderScratch.goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
															[g_paiContext.curOrderCoord.fields.orderSlot] = 1;
		return 0;
	}

	switch ((uint8_t)g_paiContext.aiController->maneuverPhase) {
		case 0:
			pai_CalcAnglesToAimPoint();
#ifndef XWA_MODERN
			sprintf(g_flightTextScratchBuffer, "Distance to delivery: %ld\n", (long)trig2_polardistance);
			OutputDebugStringA(g_flightTextScratchBuffer);
#endif
			if ((int)trig2_polardistance < 3072) {
				g_curCraft->throttleSpeed = 0;
				g_paiContext.aiController->maneuverPhase = 1;
				return 0;
			}
			g_paiContext.aiController->aimPointZ += g_paiContext.aiController->maneuverDist;
			paiman_setflighttotarget(0, 1);
			g_paiContext.aiController->aimPointZ -= g_paiContext.aiController->maneuverDist;
			if ((int)trig2_polardistance > 0x8000) {
				g_curCraft->throttleSpeed = 0xFFFF;
				return 0;
			}
			if ((int)trig2_polardistance > 0x4000) {
				g_curCraft->throttleSpeed = 0xC000;
				return 0;
			}
			if ((int)trig2_polardistance > 0x2000) {
				g_curCraft->throttleSpeed = 0x8000;
				return 0;
			}
			if ((int)trig2_polardistance > 2048)
				g_curCraft->throttleSpeed = 0x4000;
			return 0;
		case 1: {
			uint16_t carriedObjectIndex = g_curCraft->carriedObjectIndex;
			int attachOffset;
			int dropZ;
			int absX;
			int absY;
			int absZ;

			if (g_objectTable[carriedObjectIndex].genusId <= GENUS_Utility)
				attachOffset = g_modelDefs[g_curCraft->modelIndex].meshAttachData[3];
			else
				attachOffset = g_modelDefs[g_curCraft->modelIndex].meshAttachData[4];
			dropZ =
				ModelBounds_GetSizeZ((uint16_t)g_objectTable[carriedObjectIndex].objectType) - attachOffset;

			if (g_objectTable[g_paiContext.aiObjIdx].yaw != 0) {
				g_curCraft->aiFlight.turnState = 2;
				g_curCraft->aiFlight.turnStep = 0x8000u;
				g_curCraft->aiFlight.turnAccel = -1;
				g_paiContext.aiController->targetXYAngle = 0;
			}
			if (g_objectTable[g_paiContext.aiObjIdx].pitch != 0x4000) {
				g_paiContext.aiController->targetZAngle = 0x4000;
				g_curCraft->aiFlight.headingStep = 0x8000;
				g_curCraft->aiFlight.headingForce = 0;
				if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch)
					g_curCraft->aiFlight.headingState = 1;
				else
					g_curCraft->aiFlight.headingState = 2;
			}

			g_curCraft->pushAccumX =
				g_paiContext.aiController->aimPointX - g_objectTable[g_paiContext.aiObjIdx].world_x;
			absX = g_curCraft->pushAccumX;
			g_curCraft->pushAccumY =
				g_paiContext.aiController->aimPointY - g_objectTable[g_paiContext.aiObjIdx].world_y;
			absY = g_curCraft->pushAccumY;
			g_curCraft->pushAccumZ =
				dropZ + g_paiContext.aiController->aimPointZ - g_objectTable[g_paiContext.aiObjIdx].world_z;
			absZ = g_curCraft->pushAccumZ;
			if (absX < 0)
				absX = -absX;
			if (absY < 0)
				absY = -absY;
			if (absZ < 0)
				absZ = -absZ;
			if (absX + absY + absZ < 16) {
				if (g_objectTable[g_paiContext.aiObjIdx].yaw == 0 &&
					g_objectTable[g_paiContext.aiObjIdx].pitch == 0x4000) {
					g_curCraft->pushAccumX = 0;
					g_curCraft->pushAccumY = 0;
					g_curCraft->pushAccumZ = 0;
					++g_paiContext.aiController->maneuverPhase;
					g_paiContext.aiController->maneuverTimer = 472;
					return 0;
				}
			}
			return 0;
		}
		case 2: {
			ObjectRecord* carriedObj;
			CraftData* carriedCraft;
			int fgIdx;
			AiController* effectiveController;

			if (g_paiContext.aiController->maneuverTimer != 0)
				return 0;

			carriedObj = &g_objectTable[g_curCraft->carriedObjectIndex];
			carriedCraft = carriedObj->mobj->pCraft;
			++g_missionFgStats[carriedObj->flightGroupIdx].outcomeCount[28];
			fgIdx = carriedObj->flightGroupIdx;
			if (g_missionFlightGroups[fgIdx].fg.specialCargoCraft == carriedCraft->waveNumber)
				g_missionFgStats[fgIdx].specialCargoOutcome[28] = 1;
			carriedCraft->carrierObjIdx = 0xFFFF;
			g_curCraft->carriedObjectIndex = 0xFFFF;
			effectiveController = pai_GetEffectiveAIController(carriedCraft);
			effectiveController->aimPointX = g_paiContext.aiController->aimPointX;
			effectiveController->aimPointY = g_paiContext.aiController->aimPointY;
			effectiveController->aimPointZ = g_paiContext.aiController->aimPointZ;
			g_paiContext.aiController->orderScratch
				.goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
							 [g_paiContext.curOrderCoord.fields.orderSlot] = 1;
			g_curCraft->pushAccumX = 0;
			g_curCraft->pushAccumY = 0;
			g_curCraft->pushAccumZ = 1000;
			g_paiContext.aiController->maneuverTimer = 1180;
			break;
		}
	}

	return 0;
}
// FUNCTION: XWA 0x4B4100
static char paiman_backupmaneuver(void) {
	unsigned int variable2 = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								 .fg
								 .orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
										 g_paiContext.curOrderCoord.fields.orderSlot]
								 .variable2;
	if (variable2 != 0) {
		pai_CalcAnglesToAimPoint();
		if ((unsigned int)trig2_polardistance > 4071 * variable2)
			g_paiContext.aiController->aiPlanState = 0;
	}
	return 0;
}
// FUNCTION: XWA 0x4B4980
static char paiman_parkmaneuver(void) {
	char maneuverPhase = g_paiContext.aiController->maneuverPhase;
	switch ((uint8_t)maneuverPhase) {
		case 0:
			pai_CalcAnglesToAimPoint();
			if ((int)trig2_polardistance < 2048) {
				g_curCraft->throttleSpeed = 0;
				g_paiContext.aiController->maneuverPhase = 1;
				return 0;
			}
			{
				int sizeZ = ModelBounds_GetSizeZ((uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType);
				g_paiContext.aiController->aimPointZ += sizeZ;
				paiman_setflighttotarget(0, 1);
				g_paiContext.aiController->aimPointZ -= sizeZ;
			}
			if ((int)trig2_polardistance > 0x8000) {
				g_curCraft->throttleSpeed = 0xFFFF;
				return 0;
			}
			if ((int)trig2_polardistance > 0x4000) {
				g_curCraft->throttleSpeed = 0xC000;
				return 0;
			}
			if ((int)trig2_polardistance > 0x2000) {
				g_curCraft->throttleSpeed = 0x8000;
				return 0;
			}
			if ((int)trig2_polardistance > 2048) {
				g_curCraft->throttleSpeed = 0x4000;
				return 0;
			}
			break;
		case 1:
			if (g_paiContext.aiController->targetObjIdx == 0xFFFF) {
				++g_paiContext.aiController->maneuverPhase;
				g_paiContext.aiController->maneuverTimer = Mission_DecodeOrderTime(
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.variable1);
				g_paiContext.aiController->maneuverTimer *= 236;
				g_paiContext.aiController->targetXYAngle = 0;
				return 0;
			} else {
				int absX;
				int absY;
				int absZ;
				int localZOffset;

				trig2_ctop2dim(g_objectTable[g_paiContext.aiController->targetObjIdx].world_x -
								   g_paiContext.aiController->aimPointX,
							   g_objectTable[g_paiContext.aiController->targetObjIdx].world_y -
								   g_paiContext.aiController->aimPointY);
				if (g_objectTable[g_paiContext.aiObjIdx].yaw != trig2_xyangle) {
					g_curCraft->aiFlight.turnState = 2;
					g_curCraft->aiFlight.turnStep = 0x8000u;
					g_curCraft->aiFlight.turnAccel = 0x4000;
					g_paiContext.aiController->targetXYAngle = trig2_xyangle;
				}
				if (g_objectTable[g_paiContext.aiObjIdx].pitch != 0x4000) {
					g_paiContext.aiController->targetZAngle = 0x4000;
					g_curCraft->aiFlight.headingStep = 0x8000;
					g_curCraft->aiFlight.headingForce = 0;
					if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch)
						g_curCraft->aiFlight.headingState = 1;
					else
						g_curCraft->aiFlight.headingState = 2;
				}
				localZOffset = -g_modelDefs[g_curCraft->modelIndex].meshAttachData[4];
				g_curCraft->pushAccumX =
					g_paiContext.aiController->aimPointX - g_objectTable[g_paiContext.aiObjIdx].world_x;
				absX = g_curCraft->pushAccumX;
				g_curCraft->pushAccumY =
					g_paiContext.aiController->aimPointY - g_objectTable[g_paiContext.aiObjIdx].world_y;
				absY = g_curCraft->pushAccumY;
				g_curCraft->pushAccumZ = g_paiContext.aiController->aimPointZ -
										 g_objectTable[g_paiContext.aiObjIdx].world_z + localZOffset;
				absZ = g_curCraft->pushAccumZ;
				if (absX < 0)
					absX = -absX;
				if (absY < 0)
					absY = -absY;
				if (absZ < 0)
					absZ = -absZ;
				if (absX + absY + absZ >= 16)
					return 0;
				if (g_objectTable[g_paiContext.aiObjIdx].yaw != trig2_xyangle ||
					g_objectTable[g_paiContext.aiObjIdx].pitch != 0x4000)
					return 0;
				g_curCraft->pushAccumX = 0;
				g_curCraft->pushAccumY = 0;
				g_curCraft->pushAccumZ = 0;
				++g_paiContext.aiController->maneuverPhase;
				g_paiContext.aiController->maneuverTimer = Mission_DecodeOrderTime(
					g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
						.fg
						.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
								g_paiContext.curOrderCoord.fields.orderSlot]
						.variable1);
				g_paiContext.aiController->maneuverTimer *= 236;
				g_paiContext.aiController->targetXYAngle = trig2_xyangle + 0x8000;
				return 0;
			}
		case 2:
			if (g_paiContext.aiController->maneuverTimer != 0)
				return 0;
			++g_paiContext.aiController->maneuverPhase;
			g_curCraft->pushAccumZ = 466;
			g_curCraft->aiFlight.turnAccel = 0x4000;
			return 0;
		case 3:
			if (g_objectTable[g_paiContext.aiObjIdx].yaw != g_paiContext.aiController->targetXYAngle) {
				g_curCraft->aiFlight.turnState = 2;
				g_curCraft->aiFlight.turnStep = 0x8000u;
				return 0;
			}
			++g_paiContext.aiController->maneuverPhase;
			g_curCraft->throttleSpeed = 0x2000;
			g_paiContext.aiController->maneuverTimer = 1180;
			return 0;
		case 4:
			if (g_paiContext.aiController->maneuverTimer == 0)
				++g_paiContext.aiController->maneuverPhase;
			break;
	}

	return 0;
}
// FUNCTION: XWA 0x4B5390
static char paiman_workonmaneuver(void) {
	uint8_t maneuverPhase = g_paiContext.aiController->maneuverPhase;
	uint16_t targetObjIdx = g_paiContext.aiController->targetObjIdx;
	int maxY;
	switch (maneuverPhase) {
		case 0:
			pai_CalcAnglesToAimPoint();
			if ((int)trig2_polardistance < 1024) {
				g_curCraft->throttleSpeed = 0;
				g_paiContext.aiController->maneuverPhase = 1;
				return 0;
			}
			paiman_setflighttotarget(0, 1);
			if ((int)trig2_polardistance > 0x8000) {
				g_curCraft->throttleSpeed = 0xFFFF;
				return 0;
			}
			if ((int)trig2_polardistance > 0x4000) {
				g_curCraft->throttleSpeed = 0xE000;
				return 0;
			}
			if ((int)trig2_polardistance > 0x2000) {
				g_curCraft->throttleSpeed = 0xC000;
				return 0;
			}
			if ((int)trig2_polardistance > 2048) {
				g_curCraft->throttleSpeed = 0x8000;
				return 0;
			}
			if ((int)trig2_polardistance > 1024) {
				g_curCraft->throttleSpeed = 0x6000;
				return 0;
			}
			break;
		case 1: {
			int absX;
			int absY;
			int absZ;

			trig2_ctop(g_objectTable[targetObjIdx].world_x - g_paiContext.aiCurrentPointX,
					   g_objectTable[targetObjIdx].world_y - g_paiContext.aiCurrentPointY,
					   g_objectTable[targetObjIdx].world_z - g_paiContext.aiCurrentPointZ);
			if (g_objectTable[g_paiContext.aiObjIdx].yaw != trig2_xyangle) {
				g_curCraft->aiFlight.turnState = 2;
				g_curCraft->aiFlight.turnStep = 0x8000u;
				g_curCraft->aiFlight.turnAccel = 0x4000;
				g_paiContext.aiController->targetXYAngle = trig2_xyangle;
			}
			if (g_objectTable[g_paiContext.aiObjIdx].pitch != targetPitch) {
				g_paiContext.aiController->targetZAngle = targetPitch;
				g_curCraft->aiFlight.headingStep = 0x8000;
				g_curCraft->aiFlight.headingForce = 0;
				if (g_paiContext.aiController->targetZAngle <= g_objectTable[g_paiContext.aiObjIdx].pitch)
					g_curCraft->aiFlight.headingState = 1;
				else
					g_curCraft->aiFlight.headingState = 2;
			}
			maxY = ModelBounds_GetMaxY((uint16_t)g_objectTable[g_paiContext.aiObjIdx].objectType);
			if (maxY < 0)
				maxY = -maxY;
			g_curCraft->pushAccumX =
				g_paiContext.aiController->aimPointX - g_objectTable[g_paiContext.aiObjIdx].world_x;
			absX = g_curCraft->pushAccumX;
			g_curCraft->pushAccumY =
				g_paiContext.aiController->aimPointY - g_objectTable[g_paiContext.aiObjIdx].world_y;
			absY = g_curCraft->pushAccumY;
			g_curCraft->pushAccumZ =
				g_paiContext.aiController->aimPointZ - g_objectTable[g_paiContext.aiObjIdx].world_z;
			absZ = g_curCraft->pushAccumZ;
			if (absX < 0)
				absX = -absX;
			if (absY < 0)
				absY = -absY;
			if (absZ < 0)
				absZ = -absZ;
			if (absX + absY + absZ >= maxY)
				return 0;
			g_curCraft->pushAccumX = 0;
			g_curCraft->pushAccumY = 0;
			g_curCraft->pushAccumZ = 0;
			++g_paiContext.aiController->maneuverPhase;
			g_paiContext.aiController->maneuverTimer = Mission_DecodeOrderTime(
				g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg
					.orders[4 * g_paiContext.curOrderCoord.fields.regionIdx +
							g_paiContext.curOrderCoord.fields.orderSlot]
					.variable1);
			g_paiContext.aiController->maneuverTimer *= 236;
			return 0;
		}
		case 2:
			if (g_paiContext.aiController->maneuverTimer != 0)
				return 0;
			++g_paiContext.aiController->maneuverPhase;
			g_curCraft->aiFlight.turnAccel = 0x4000;
			++g_paiContext.aiController->orderScratch
				  .goalProgress[g_paiContext.curOrderCoord.fields.regionIdx]
							   [g_paiContext.curOrderCoord.fields.orderSlot];
			++g_objectTable[targetObjIdx].mobj->pCraft->aiFlight.orderActionCounter;
			g_objectTable[targetObjIdx].mobj->pCraft->boardingState = 3;
			++g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].outcomeCount[31];
			if (g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
					.fg.specialCargoCraft != g_curCraft->waveNumber)
				return 0;
			g_missionFgStats[g_paiContext.curOrderCoord.fields.flightGroupIdx].specialCargoOutcome[31] = 1;
			return 0;
		case 3: {
			uint16_t targetXYAngle;

			trig2_ctop(g_objectTable[targetObjIdx].world_x - g_paiContext.aiCurrentPointX,
					   g_objectTable[targetObjIdx].world_y - g_paiContext.aiCurrentPointY,
					   g_objectTable[targetObjIdx].world_z - g_paiContext.aiCurrentPointZ);
			targetXYAngle = trig2_xyangle + 0x8000;
			if (g_objectTable[g_paiContext.aiObjIdx].yaw != targetXYAngle) {
				g_curCraft->aiFlight.turnState = 2;
				g_curCraft->aiFlight.turnStep = 0x8000u;
				g_paiContext.aiController->targetXYAngle = targetXYAngle;
			} else {
				++g_paiContext.aiController->maneuverPhase;
				g_curCraft->throttleSpeed = 0x4000;
				g_paiContext.aiController->maneuverTimer = 1180;
			}
			return 0;
		}
		case 4:
			if (g_paiContext.aiController->maneuverTimer == 0)
				return 1;
			break;
	}

	return 0;
}

// FUNCTION: XWA 0x4B5910
static char paiman_deathstarfollowmaneuver(void) {
	DeathStar_UpdateFollowOverrideCraft(g_paiContext.aiObjIdx);
	return 0;
}

// FUNCTION: XWA 0x4B5A00
void paiman_RefreshDeathStarPlayerFollow(int objectIdx, int playerIdx) {
	CraftData* craft;
	AiController* effectiveAiController;
	uint8_t groupAI;
	int thinkInterval;

	if (objectIdx == 0xffff || playerIdx == -1) {
		return;
	}

	craft = g_objectTable[objectIdx].mobj->pCraft;
	effectiveAiController = pai_GetEffectiveAIController(craft);

	if (strcmp(g_planTable[effectiveAiController->currentPlanId].name, "deathstarfollowpln") != 0) {
		return;
	}

	craft->followPlayerMode = 2;
	craft->followPlayerIdx = (uint8_t)playerIdx;
	craft->followTimer = 16;
	groupAI = g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.groupAI;
	thinkInterval = g_aiThinkIntervalByGroupAI[groupAI];
	craft->aiController.thinkInterval = thinkInterval;
}
