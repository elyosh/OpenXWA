#ifndef XWA_FLIGHT_AI_INTERNAL_H
#define XWA_FLIGHT_AI_INTERNAL_H

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"

#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/audio/music.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_light.h"
#include "xwa/audio/fsfx.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/random.h"

#include <string.h>

extern PaiManeuverFunc g_aiCourseOrderManeuverTable[41];
extern PaiManeuverFunc g_orderTable[66];

extern const unsigned int g_aiStillAttackLastAttackerRangeBySkill[4];
extern const unsigned int g_aiAttackerSearchRangeBySkill[4];
extern const unsigned int g_aiWarheadThreatRangeBySkill[4];
extern const uint8_t g_aiUnderAttackFrontManeuverChoices[4];
extern const uint8_t g_aiUnderAttackSideRearManeuverChoices[8];

uint8_t* pai_getplandataptrbyname(const char* planName);
void pai_SetFlightGroupFormation(int flightGroupIdx, int formationType, int separation);
int16_t pai_FindMothershipObjectAnyRegion(int16_t mothershipFlightGroupIdx);
bool pai_IsStaticBoardingPlanId(uint16_t planId);

// AI trace (debug instrumentation; see pai.c). Retained, OFF by default; set
// g_paiTrace = 1 to enable runtime tracing of a flight group's AI state machine.
extern int g_paiTrace;
void pai_TraceManeuverTick(char maneuverResult);

#endif
