#include "xwa/frontend/frontend_mission.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	MISSION_FORMAT_XWA_V18 = 18,
	MISSION_MODERN_STRING_TAIL = 0x8000,
	MISSION_BRIEFING_TEAM_COUNT = 2,
	MISSION_BRIEFING_LABEL_COUNT = 192,
	MISSION_BRIEFING_ICON_COUNT = 10,
	MISSION_BRIEFING_STRING_COUNT = 128,
	MAX_MISSION_FLIGHT_GROUPS = 192,
	MAX_MISSION_MESSAGES = 64,
	MAX_GLOBAL_GOALS_PER_TEAM = 7,
	MAX_MISSION_TEAMS = 10,
};

typedef struct MissionFile {
	int16_t formatVersion;
	XwaMissionHeader header;
	XwaFlightGroup flightGroups[MAX_MISSION_FLIGHT_GROUPS];
	XwaMessage messages[MAX_MISSION_MESSAGES];
	XwaGlobalGoal globalGoals[MAX_MISSION_TEAMS][MAX_GLOBAL_GOALS_PER_TEAM];
	XwaTeam teams[MAX_MISSION_TEAMS];
	int messagePresent[MAX_MISSION_MESSAGES];
	int globalGoalCount[MAX_MISSION_TEAMS];
	int teamPresent[MAX_MISSION_TEAMS];
	long briefingOffset;
	long textTailOffset;
	long overrideOffset;
	long endOffset;
} MissionFile;

static const char* g_triggerVarNames[] = {
	"None",          "FlightGroup", "ShipType",       "ShipClass",      "ObjectType",   "IFF",
	"ShipOrders",    "CraftWhen",   "GlobalGroup",    "AILevel",        "Status",       "AllCraft",
	"Team",          "PlayerNum",   "BeforeTime",     "NotFlightGroup", "NotShipType",  "NotShipClass",
	"NotObjectType", "NotIFF",      "NotGlobalGroup", "NotTeam",        "NotPlayerNum", "GlobalUnit",
	"NotGlobalUnit", "GlobalCargo", "NotGlobalCargo",
};

static const char* g_orderNames[] = {
	"Null",
	"UpdateCourse",
	"UnderAttack",
	"StillAttack",
	"FlyHome",
	"FighterShoot",
	"GunnerSelfDefense",
	"GunnerOffense",
	"MissileDefense",
	"ScanForTarget",
	"WaitRun",
	"BreakOff",
	"LeaderDead",
	"CoverLeader",
	"FollowLeadAttack",
	"AbortMission",
	"OnTail",
	"Always",
	"CheckEscort",
	"LeaderGoHome",
	"Hyperspace",
	"EnterHangar",
	"Mothership",
	"EscortTarget",
	"LookForCraftToBoard",
	"AbortBoard",
	"ReturnBoard",
	"AwaitBoard",
	"MakeDisabled",
	"NearTarget",
	"RocketsOnBoard",
	"AvoidHit",
	"WaitForAllReturn",
	"WaitForAllCreate",
	"Evasive",
	"TargetFromPlayer",
	"AvoidStarship",
	"CheckHyper",
	"StopGoHome",
	"CompleteGoHome",
	"CompleteGoOther",
	"CompleteFollow",
	"WaitGoOther",
	"OrderSwitch",
	"KillSelf",
	"DropoffDest",
	"AbortMotherWait",
	"CheckConditional",
	"TransferCargo",
	"SelfCapture",
	"CheckRelease",
	"CheckDeliver",
	"ChangeSides",
	"StartOver",
	"Disappear",
	"CommandFromPlayer",
	"ResumeMission",
	"ScanForPlayerTargetType",
	"Inspected",
	"ScanForPlayerInspectType",
	"ComponentGone",
	"RepairOneself",
	"FlyHomeOtherRegion",
	"PickedUpObject",
	"LookForPark",
	"PlayerGone",
};

static const uint8_t g_orderLeaderPlanNameIndex[] = {
	0x01, 0x2f, 0x03, 0x05, 0x27, 0x2a, 0x2b, 0x45, 0x08, 0x09, 0x14, 0x13, 0x1c, 0x1d, 0x1e,
	0x1f, 0x20, 0x21, 0x25, 0x42, 0x42, 0x38, 0x3a, 0x3b, 0x3c, 0x3c, 0x3e, 0x3f, 0x01, 0x35,
	0x01, 0x22, 0x44, 0x01, 0x01, 0x01, 0x43, 0x46, 0x4b, 0x4d, 0x4f, 0x01, 0x45, 0x52, 0x2a,
	0x55, 0x50, 0x4c, 0x01, 0x53, 0x49, 0x01, 0x4a, 0x57, 0x01, 0x01, 0x01, 0x6e, 0x01, 0x51,
	0x01, 0x70, 0x01, 0x74, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t g_orderFollowerPlanNameIndex[] = {
	0x02, 0x30, 0x04, 0x06, 0x29, 0x2a, 0x2b, 0x0e, 0x0e, 0x0e, 0x18, 0x0e, 0x1c, 0x1d, 0x1e,
	0x1f, 0x20, 0x21, 0x04, 0x42, 0x42, 0x39, 0x3a, 0x3b, 0x39, 0x39, 0x39, 0x39, 0x02, 0x36,
	0x02, 0x22, 0x44, 0x01, 0x01, 0x01, 0x43, 0x46, 0x4b, 0x4d, 0x4f, 0x01, 0x0e, 0x52, 0x2a,
	0x55, 0x50, 0x4c, 0x01, 0x53, 0x49, 0x01, 0x4a, 0x57, 0x01, 0x01, 0x01, 0x6e, 0x01, 0x51,
	0x01, 0x70, 0x01, 0x74, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const char* g_builtinPlanNames[] = {
	"nullpln",
	"stationaryldrpln",
	"stationaryflwpln",
	"formldr1pln",
	"formflw1pln",
	"formevadeldr1pln",
	"formevadeflw1pln",
	"capldr1pln",
	"capescortersldr1pln",
	"caprespondldr1pln",
	"capldr2pln",
	"capldr3pln",
	"capldr4pln",
	"capldr5pln",
	"capflw1pln",
	"capflw2pln",
	"capflw3pln",
	"capflw4pln",
	"capflw5pln",
	"disableldr1pln",
	"escortldr1pln",
	"escortldr2pln",
	"escortldr3pln",
	"escortldr4pln",
	"escortflw1pln",
	"escortflw2pln",
	"escortflw3pln",
	"escortflw4pln",
	"boardtogivepln",
	"boardtotakepln",
	"boardtoexchangepln",
	"boardtocapturepln",
	"boardtodestroypln",
	"boardtopickuppln",
	"boardtocontactpln",
	"board2pln",
	"board3pln",
	"dropoffldr1pln",
	"dropoffldr2pln",
	"rendezvous1pln",
	"rendezvous2pln",
	"rendezvousflw1pln",
	"disabledpln",
	"waitforboardpln",
	"craftwaitforgopln",
	"flyhomepln",
	"followhomepln",
	"flyhomeevadepln",
	"followhomeevadepln",
	"enterhangarpln",
	"exithangarpln",
	"intohyperspacepln",
	"outofhyperspacepln",
	"starshipintohyperpln",
	"starshipfollowhomepln",
	"starshipstatpln",
	"starshipformpln",
	"starshipfollowpln",
	"starshipwaitreturnpln",
	"starshipwaitcreatepln",
	"starshipprotectpln",
	"starshipescortpln",
	"starshipattackpln",
	"starshipdisablepln",
	"starshipwaitforgopln",
	"variablepln",
	"waitpln",
	"selfdestroypln",
	"boardtorepairpln",
	"capfreeldr1pln",
	"kamikaze1pln",
	"kamikaze2pln",
	"kamikaze3pln",
	"hyperspacepln",
	"transfercargopln",
	"orbitpln",
	"selfcapturepln",
	"release1pln",
	"release2pln",
	"deliverpln",
	"changesidespln",
	"startoverpln",
	"backuppln",
	"hyperbuoypln",
	"disappearpln",
	"repaironeselfpln",
	"homeviahyperspacepln",
	"inspectldr1pln",
	"inspectldr2pln",
	"inspectldr3pln",
	"playercapldr1pln",
	"playercapldr2pln",
	"playercapldr3pln",
	"playercapldr4pln",
	"playercapldr5pln",
	"playerfollowpln",
	"playerinspectldr1pln",
	"playerinspectldr2pln",
	"playerinspectldr3pln",
	"playerdisableldr2pln",
	"playerboardtorepairpln",
	"playerboardtocapturepln",
	"playerboardtopickuppln",
	"playerboardtodestroypln",
	"playerboardtodefusepln",
	"playerboard2pln",
	"playerboard3pln",
	"resumemissionpln",
	"homing1pln",
	"homing2pln",
	"park1pln",
	"park2pln",
	"workon1pln",
	"workon2pln",
	"workon3pln",
	"deathstarfollowpln",
	"followtarget1pln",
	"followtarget2pln",
	"followtarget3pln",
};

static const char* g_hangarNames[] = {
	"Junkyard", "Simulator1", "Quickstart",    "Simulator2",
	"Skirmish", "DeathStar",  "MonCalCruiser", "FamilyTransport",
};

static const char* name_or_unknown(const char* const* names, size_t count, unsigned int value) {
	return value < count && names[value] != NULL ? names[value] : "?";
}

static const char* trigger_var_name(unsigned int value) {
	return name_or_unknown(g_triggerVarNames, sizeof(g_triggerVarNames) / sizeof(g_triggerVarNames[0]),
						   value);
}

static const char* order_name(unsigned int value) {
	return name_or_unknown(g_orderNames, sizeof(g_orderNames) / sizeof(g_orderNames[0]), value);
}

static const char* order_plan_name(unsigned int orderId, int leader) {
	const uint8_t* table;
	unsigned int planNameIndex;

	table = leader ? g_orderLeaderPlanNameIndex : g_orderFollowerPlanNameIndex;
	if (orderId >= sizeof(g_orderLeaderPlanNameIndex) / sizeof(g_orderLeaderPlanNameIndex[0])) {
		return "?";
	}

	planNameIndex = table[orderId];
	return name_or_unknown(g_builtinPlanNames, sizeof(g_builtinPlanNames) / sizeof(g_builtinPlanNames[0]),
						   planNameIndex);
}

static const char* plan_meaning(const char* planName) {
	if (strcmp(planName, "boardtopickuppln") == 0 || strcmp(planName, "playerboardtopickuppln") == 0) {
		return "board/pick up target";
	}
	if (strcmp(planName, "release1pln") == 0 || strcmp(planName, "release2pln") == 0) {
		return "release carried object";
	}
	if (strcmp(planName, "deliverpln") == 0 || strcmp(planName, "dropoffldr1pln") == 0 ||
		strcmp(planName, "dropoffldr2pln") == 0) {
		return "deliver/drop off cargo";
	}
	if (strcmp(planName, "followtarget1pln") == 0 || strcmp(planName, "followtarget2pln") == 0 ||
		strcmp(planName, "followtarget3pln") == 0) {
		return "follow target";
	}
	if (strcmp(planName, "waitpln") == 0 || strcmp(planName, "craftwaitforgopln") == 0 ||
		strcmp(planName, "starshipwaitforgopln") == 0) {
		return "wait";
	}
	if (strcmp(planName, "flyhomepln") == 0 || strcmp(planName, "followhomepln") == 0 ||
		strcmp(planName, "homeviahyperspacepln") == 0) {
		return "return home";
	}
	if (strcmp(planName, "intohyperspacepln") == 0 || strcmp(planName, "hyperspacepln") == 0 ||
		strcmp(planName, "starshipintohyperpln") == 0) {
		return "hyperspace";
	}
	if (strcmp(planName, "stationaryldrpln") == 0 || strcmp(planName, "stationaryflwpln") == 0) {
		return "hold position";
	}
	if (strcmp(planName, "nullpln") == 0) {
		return "no plan";
	}
	return "";
}

static const char* hangar_name(unsigned int value) {
	return name_or_unknown(g_hangarNames, sizeof(g_hangarNames) / sizeof(g_hangarNames[0]), value);
}

static const char* target_pair_op_name(unsigned int value) { return value == 1 ? "OR" : "AND"; }

static void copy_fixed_string(char* dst, size_t dstSize, const char* src, size_t srcSize) {
	size_t len;

	if (dstSize == 0) {
		return;
	}

	len = 0;
	while (len < srcSize && src[len] != '\0') {
		++len;
	}
	if (len >= dstSize) {
		len = dstSize - 1;
	}
	memcpy(dst, src, len);
	dst[len] = '\0';
}

static int read_exact(FILE* fp, void* dst, size_t size, const char* what) {
	if (fread(dst, 1, size, fp) != size) {
		fprintf(stderr, "mission_dump: failed to read %s\n", what);
		return 0;
	}
	return 1;
}

static int read_modern_string(FILE* fp, char text[65], const char* what) {
	memset(text, 0, 65);
	if (!read_exact(fp, text, 1, what)) {
		return 0;
	}
	if (text[0] != '\0' && !read_exact(fp, &text[1], 63, what)) {
		return 0;
	}
	text[64] = '\0';
	return 1;
}

static int read_i16(FILE* fp, int16_t* value, const char* what) {
	uint8_t bytes[2];

	if (!read_exact(fp, bytes, sizeof(bytes), what)) {
		return 0;
	}
	*value = (int16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
	return 1;
}

static int seek_relative(FILE* fp, long count, const char* what) {
	if (fseek(fp, count, SEEK_CUR) != 0) {
		fprintf(stderr, "mission_dump: failed to skip %s: %s\n", what, strerror(errno));
		return 0;
	}
	return 1;
}

static int skip_briefing_content(FILE* fp, const XwaMissionHeader* header) {
	int teamIdx;

	for (teamIdx = 0; teamIdx < MISSION_BRIEFING_TEAM_COUNT; ++teamIdx) {
		int textIdx;
		int blockSize;

		blockSize = header->body.secondaryVersion == 98 ? 12810 : 3210;
		if (!seek_relative(fp, blockSize, "briefing map block") ||
			!seek_relative(fp, MISSION_BRIEFING_LABEL_COUNT * 24L, "briefing labels") ||
			!seek_relative(fp, MISSION_BRIEFING_ICON_COUNT, "briefing icons")) {
			return 0;
		}

		for (textIdx = 0; textIdx < MISSION_BRIEFING_STRING_COUNT * 2; ++textIdx) {
			int16_t textLength;

			if (!read_i16(fp, &textLength, "briefing text length")) {
				return 0;
			}
			if (textLength > 0 && !seek_relative(fp, textLength, "briefing text")) {
				return 0;
			}
		}
	}

	return 1;
}

static int load_mission(const char* path, MissionFile* mission) {
	FILE* fp;
	int i;

	memset(mission, 0, sizeof(*mission));

	fp = fopen(path, "rb");
	if (fp == NULL) {
		fprintf(stderr, "mission_dump: cannot open %s: %s\n", path, strerror(errno));
		return 0;
	}

	if (!read_i16(fp, &mission->formatVersion, "format version") ||
		!read_exact(fp, &mission->header, sizeof(mission->header), "mission header")) {
		fclose(fp);
		return 0;
	}

	if (mission->header.numFlightGroups > MAX_MISSION_FLIGHT_GROUPS ||
		mission->header.numMessages > MAX_MISSION_MESSAGES) {
		fprintf(stderr, "mission_dump: invalid counts: %u flight groups, %u messages\n",
				(unsigned)mission->header.numFlightGroups, (unsigned)mission->header.numMessages);
		fclose(fp);
		return 0;
	}

	for (i = 0; i < (int)mission->header.numFlightGroups; ++i) {
		if (!read_exact(fp, &mission->flightGroups[i], sizeof(mission->flightGroups[i]), "flight group")) {
			fclose(fp);
			return 0;
		}
	}

	for (i = 0; i < (int)mission->header.numMessages; ++i) {
		int16_t indexedRecord;

		if (!read_i16(fp, &indexedRecord, "message index")) {
			fclose(fp);
			return 0;
		}
		if (indexedRecord < 0 || indexedRecord >= MAX_MISSION_MESSAGES) {
			fprintf(stderr, "mission_dump: invalid message index %d\n", indexedRecord);
			fclose(fp);
			return 0;
		}
		if (!read_exact(fp, &mission->messages[indexedRecord], sizeof(mission->messages[indexedRecord]),
						"message")) {
			fclose(fp);
			return 0;
		}
		mission->messagePresent[indexedRecord] = 1;
	}

	for (i = 0; i < MAX_MISSION_TEAMS; ++i) {
		int16_t globalGoalCount;

		if (!read_i16(fp, &globalGoalCount, "global goal count")) {
			fclose(fp);
			return 0;
		}
		if (globalGoalCount < 0 || globalGoalCount > MAX_GLOBAL_GOALS_PER_TEAM) {
			fprintf(stderr, "mission_dump: invalid global goal count %d for team %d\n", globalGoalCount, i);
			fclose(fp);
			return 0;
		}
		mission->globalGoalCount[i] = globalGoalCount;
		if (globalGoalCount > 0 &&
			!read_exact(fp, mission->globalGoals[i], sizeof(XwaGlobalGoal) * (size_t)globalGoalCount,
						"global goals")) {
			fclose(fp);
			return 0;
		}
	}

	for (i = 0; i < MAX_MISSION_TEAMS; ++i) {
		int16_t teamPresent;

		if (!read_i16(fp, &teamPresent, "team-present flag")) {
			fclose(fp);
			return 0;
		}
		mission->teamPresent[i] = teamPresent != 0;
		if (teamPresent != 0 &&
			!read_exact(fp, &mission->teams[i], sizeof(mission->teams[i]), "team record")) {
			fclose(fp);
			return 0;
		}
	}

	mission->briefingOffset = ftell(fp);
	if (!skip_briefing_content(fp, &mission->header)) {
		fclose(fp);
		return 0;
	}
	mission->textTailOffset = ftell(fp);
	if (mission->formatVersion == MISSION_FORMAT_XWA_V18 &&
		!seek_relative(fp, MISSION_MODERN_STRING_TAIL, "modern string tail")) {
		fclose(fp);
		return 0;
	}
	mission->overrideOffset = ftell(fp);

	if (fseek(fp, 0, SEEK_END) != 0) {
		fprintf(stderr, "mission_dump: failed to seek end: %s\n", strerror(errno));
		fclose(fp);
		return 0;
	}
	mission->endOffset = ftell(fp);
	fclose(fp);
	return 1;
}

static void print_waypoint(const char* label, const XwaWaypoint* wp) {
	printf("%s(x=%d y=%d z=%d enabled=%d)", label, wp->x, wp->y, wp->z, wp->enabled);
}

static void print_trigger(const char* label, const XwaTrigger* trigger) {
	printf("%s{cond=%u varType=%u(%s) var=%u amount=%u param=%d}", label, (unsigned)trigger->condition,
		   (unsigned)trigger->variableType, trigger_var_name(trigger->variableType),
		   (unsigned)trigger->variable, (unsigned)trigger->amount, (int)trigger->parameter);
}

static void print_trigger_pair(const char* label, const XwaTriggerPair* pair) {
	printf("%s t1OrT2=%u reservedIo=%u\n", label, (unsigned)pair->t1OrT2, (unsigned)pair->ioReserved);
	print_trigger("    trigger1=", &pair->triggers[0]);
	printf("\n");
	print_trigger("    trigger2=", &pair->triggers[1]);
	printf("\n");
}

static int trigger_nonempty(const XwaTrigger* trigger) {
	return trigger->condition || trigger->variableType || trigger->variable || trigger->amount ||
		   trigger->parameter;
}

static int trigger_pair_nonempty(const XwaTriggerPair* pair) {
	return trigger_nonempty(&pair->triggers[0]) || trigger_nonempty(&pair->triggers[1]) || pair->t1OrT2 ||
		   pair->ioReserved;
}

static void format_named_target(const MissionFile* mission, unsigned int type, unsigned int value, char* out,
								size_t outSize) {
	char text[65];

	if (outSize == 0) {
		return;
	}

	switch (type) {
		case 1:
		case 15:
			if (value < mission->header.numFlightGroups) {
				copy_fixed_string(text, sizeof(text), mission->flightGroups[value].name,
								  sizeof(mission->flightGroups[value].name));
				snprintf(out, outSize, "FG %03u \"%s\"", value, text);
			} else {
				snprintf(out, outSize, "FG %u (out of range)", value);
			}
			break;
		case 5:
		case 19:
			if (value < 4) {
				copy_fixed_string(text, sizeof(text), mission->header.body.iffNames[value],
								  sizeof(mission->header.body.iffNames[value]));
				snprintf(out, outSize, "IFF %u \"%s\"", value, text);
			} else {
				snprintf(out, outSize, "IFF %u", value);
			}
			break;
		case 8:
		case 20:
			if (value < 32) {
				copy_fixed_string(text, sizeof(text), mission->header.body.globalGroups[value].name,
								  sizeof(mission->header.body.globalGroups[value].name));
				snprintf(out, outSize, "GlobalGroup %u \"%s\"", value, text);
			} else {
				snprintf(out, outSize, "GlobalGroup %u", value);
			}
			break;
		case 12:
		case 21:
			if (value < MAX_MISSION_TEAMS && mission->teamPresent[value]) {
				copy_fixed_string(text, sizeof(text), mission->teams[value].name,
								  sizeof(mission->teams[value].name));
				snprintf(out, outSize, "Team %u \"%s\"", value, text);
			} else {
				snprintf(out, outSize, "Team %u", value);
			}
			break;
		case 23:
		case 24:
			if (value < 40) {
				copy_fixed_string(text, sizeof(text), mission->header.body.globalUnits[value].name,
								  sizeof(mission->header.body.globalUnits[value].name));
				snprintf(out, outSize, "GlobalUnit %u \"%s\"", value, text);
			} else {
				snprintf(out, outSize, "GlobalUnit %u", value);
			}
			break;
		case 25:
		case 26:
			if (value < 16) {
				copy_fixed_string(text, sizeof(text), mission->header.body.globalCargos[value].name,
								  sizeof(mission->header.body.globalCargos[value].name));
				snprintf(out, outSize, "GlobalCargo %u \"%s\"", value, text);
			} else {
				snprintf(out, outSize, "GlobalCargo %u", value);
			}
			break;
		default:
			snprintf(out, outSize, "%u", value);
			break;
	}
}

static void print_order_target(const MissionFile* mission, const char* label, unsigned int type,
							   unsigned int value) {
	char desc[96];

	format_named_target(mission, type, value, desc, sizeof(desc));
	printf("%s type=%u(%s) value=%u [%s]", label, type, trigger_var_name(type), value, desc);
}

static void print_order_summary(const MissionFile* mission, int orderIndex, const XwaOrder* order) {
	const char* leaderPlan;
	const char* followerPlan;
	const char* meaning;
	char target1[96];
	char target2[96];
	char target3[96];
	char target4[96];

	leaderPlan = order_plan_name(order->order, 1);
	followerPlan = order_plan_name(order->order, 0);
	meaning = plan_meaning(leaderPlan);
	format_named_target(mission, order->target1Type, order->target1, target1, sizeof(target1));
	format_named_target(mission, order->target2Type, order->target2, target2, sizeof(target2));
	format_named_target(mission, order->secondaryTargetTypes[XWA_ORDER_TARGET_3],
						order->secondaryTargets[XWA_ORDER_TARGET_3], target3, sizeof(target3));
	format_named_target(mission, order->secondaryTargetTypes[XWA_ORDER_TARGET_4],
						order->secondaryTargets[XWA_ORDER_TARGET_4], target4, sizeof(target4));

	printf("  Order %02d summary: handler=%s", orderIndex, order_name(order->order));
	if (order->order == 0) {
		printf(" inactive/null order");
	} else {
		printf(" leaderPlan=%s", leaderPlan);
		if (meaning[0] != '\0') {
			printf(" (%s)", meaning);
		}
		printf(" followerPlan=%s", followerPlan);
	}
	printf("\n");
	printf("    targets12 %s: %s %s %s\n", target_pair_op_name(order->target1OrTarget2), target1,
		   target_pair_op_name(order->target1OrTarget2), target2);
	printf("    targets34 %s: %s %s %s\n", target_pair_op_name(order->target3OrTarget4), target3,
		   target_pair_op_name(order->target3OrTarget4), target4);
}

static void print_order(const MissionFile* mission, int orderIndex, const XwaOrder* order) {
	int i;

	print_order_summary(mission, orderIndex, order);
	printf("  Order %02d region=%d slot=%d id=%u(%s) throttle=%u speed=%u vars=%u,%u,%u,%u io=%u\n",
		   orderIndex, orderIndex / 4, orderIndex % 4, (unsigned)order->order, order_name(order->order),
		   (unsigned)order->throttle, (unsigned)order->speed, (unsigned)order->variable1,
		   (unsigned)order->variable2, (unsigned)order->variable3, (unsigned)order->variable4,
		   (unsigned)order->ioReserved);
	print_order_target(mission, "    target1:", order->target1Type, order->target1);
	printf("\n");
	print_order_target(mission, "    target2:", order->target2Type, order->target2);
	printf(" pairOp=%u(%s)\n", (unsigned)order->target1OrTarget2,
		   target_pair_op_name(order->target1OrTarget2));
	print_order_target(mission, "    target3:", order->secondaryTargetTypes[XWA_ORDER_TARGET_3],
					   order->secondaryTargets[XWA_ORDER_TARGET_3]);
	printf("\n");
	print_order_target(mission, "    target4:", order->secondaryTargetTypes[XWA_ORDER_TARGET_4],
					   order->secondaryTargets[XWA_ORDER_TARGET_4]);
	printf(" pairOp=%u(%s)\n", (unsigned)order->target3OrTarget4,
		   target_pair_op_name(order->target3OrTarget4));
	for (i = 0; i < 8; ++i) {
		printf("    waypoint%d: x=%d y=%d z=%d enabled=%d\n", i, order->waypoints[i].x, order->waypoints[i].y,
			   order->waypoints[i].z, order->waypoints[i].enabled);
	}
}

static void print_fg_goal(int goalIndex, const XwaGoalFG* goal) {
	int i;

	printf("  FGGoal %d: arg=%u cond=%u amount=%u points=%d parameter=%u activeSequence=%u teams=", goalIndex,
		   (unsigned)goal->payload.argument, (unsigned)goal->payload.condition,
		   (unsigned)goal->payload.amount, (int)goal->payload.points, (unsigned)goal->payload.parameter,
		   (unsigned)goal->payload.activeSequence);
	for (i = 0; i < 10; ++i) {
		printf("%s%u", i ? "," : "", (unsigned)goal->payload.enabledForTeam[i]);
	}
	printf("\n");
}

static void print_global_goal(int teamIdx, int goalIdx, const XwaGlobalGoal* goal) {
	char name[17];
	int i;

	copy_fixed_string(name, sizeof(name), goal->name, sizeof(goal->name));
	printf("Team %d GlobalGoal %d: name=\"%s\" version=%u rawDelay=%u rawPoints=%d activeSequence=%u "
		   "t12AndOrT34=%u pointsPerTrigger=",
		   teamIdx, goalIdx, name, (unsigned)goal->version, (unsigned)goal->rawDelay, (int)goal->rawPoints,
		   (unsigned)goal->activeSequence, (unsigned)goal->t12AndOrT34);
	for (i = 0; i < 4; ++i) {
		printf("%s%d", i ? "," : "", (int)goal->rawPointsPerTrigger[i]);
	}
	printf("\n");
	print_trigger_pair("  triggers12:", &goal->triggerPairs[0]);
	print_trigger_pair("  triggers34:", &goal->triggerPairs[1]);
}

static int fg_goal_nonempty(const XwaGoalFG* goal) {
	int i;

	if (goal->payload.argument || goal->payload.condition || goal->payload.amount || goal->payload.points ||
		goal->payload.parameter || goal->payload.activeSequence) {
		return 1;
	}
	for (i = 0; i < 10; ++i) {
		if (goal->payload.enabledForTeam[i]) {
			return 1;
		}
	}
	return 0;
}

static void dump_header(const MissionFile* mission) {
	const XwaMissionHeaderBody* body;
	int i;

	body = &mission->header.body;
	printf("Mission\n");
	printf("  formatVersion=%d flightGroups=%u messages=%u\n", mission->formatVersion,
		   (unsigned)mission->header.numFlightGroups, (unsigned)mission->header.numMessages);
	printf("  hangar=%u(%s) goalsUnimportant=%u timeLimitMin=%u endWhenComplete=%u briefingOfficer=%u "
		   "briefingLogo=%u briefingOfficerEntryLine=%u secondaryVersion=%u\n",
		   (unsigned)body->hangar, hangar_name(body->hangar), (unsigned)body->goalsUnimportant,
		   (unsigned)body->timeLimitMin, (unsigned)body->endMissionWhenComplete,
		   (unsigned)body->briefingOfficer, (unsigned)body->briefingLogo,
		   (unsigned)body->briefingOfficerEntryLine, (unsigned)body->secondaryVersion);
	printf("  legacy: time=%u:%u winType=%u backdrop=%u rescue=%u allWayShown=%u\n",
		   (unsigned)body->legacyTimeLimitMin, (unsigned)body->legacyTimeLimitSec,
		   (unsigned)body->legacyWinType, (unsigned)body->legacyBackdrop, (unsigned)body->legacyRescue,
		   (unsigned)body->legacyAllWayShown);

	for (i = 0; i < 4; ++i) {
		char text[65];

		copy_fixed_string(text, sizeof(text), body->iffNames[i], sizeof(body->iffNames[i]));
		printf("  IFF %d: \"%s\"\n", i, text);
	}
	for (i = 0; i < 4; ++i) {
		char text[65];

		copy_fixed_string(text, sizeof(text), body->regions[i].name, sizeof(body->regions[i].name));
		printf("  Region %d: name=\"%s\" id=%d\n", i, text, body->regions[i].id);
	}
	for (i = 0; i < 16; ++i) {
		const XwaGlobalCargo* cargo;
		char text[65];

		cargo = &body->globalCargos[i];
		copy_fixed_string(text, sizeof(text), cargo->name, sizeof(cargo->name));
		if (text[0] || cargo->id || cargo->count || cargo->type || cargo->volume || cargo->value ||
			cargo->volatility) {
			printf(
				"  GlobalCargo %02d: name=\"%s\" id=%d count=%d type=%u volume=%u value=%u volatility=%u\n",
				i, text, cargo->id, cargo->count, (unsigned)cargo->type, (unsigned)cargo->volume,
				(unsigned)cargo->value, (unsigned)cargo->volatility);
		}
	}
	for (i = 0; i < 32; ++i) {
		const XwaGlobalUnit* unit;
		char name[65];
		char special[21];

		unit = &body->globalGroups[i];
		copy_fixed_string(name, sizeof(name), unit->name, sizeof(unit->name));
		copy_fixed_string(special, sizeof(special), unit->specialCargo, sizeof(unit->specialCargo));
		if (name[0] || unit->leader || unit->specialCargoCraft || special[0] || unit->randomSpecialCraft) {
			printf("  GlobalGroup %02d: name=\"%s\" leader=%u specialCargoCraft=%u special=\"%s\" "
				   "randomSpecial=%u\n",
				   i, name, (unsigned)unit->leader, (unsigned)unit->specialCargoCraft, special,
				   (unsigned)unit->randomSpecialCraft);
		}
	}
	for (i = 0; i < 40; ++i) {
		const XwaGlobalUnit* unit;
		char name[65];
		char special[21];

		unit = &body->globalUnits[i];
		copy_fixed_string(name, sizeof(name), unit->name, sizeof(unit->name));
		copy_fixed_string(special, sizeof(special), unit->specialCargo, sizeof(unit->specialCargo));
		if (name[0] || unit->leader || unit->specialCargoCraft || special[0] || unit->randomSpecialCraft) {
			printf("  GlobalUnit %02d: name=\"%s\" leader=%u specialCargoCraft=%u special=\"%s\" "
				   "randomSpecial=%u\n",
				   i, name, (unsigned)unit->leader, (unsigned)unit->specialCargoCraft, special,
				   (unsigned)unit->randomSpecialCraft);
		}
	}
}

static void dump_messages(const MissionFile* mission) {
	int i;

	printf("\nMessages\n");
	for (i = 0; i < MAX_MISSION_MESSAGES; ++i) {
		const XwaMessage* msg;
		char text[81];
		char voice[9];
		int team;

		if (!mission->messagePresent[i]) {
			continue;
		}
		msg = &mission->messages[i];
		copy_fixed_string(text, sizeof(text), msg->message, sizeof(msg->message));
		copy_fixed_string(voice, sizeof(voice), msg->voice, sizeof(msg->voice));
		printf("Message %02d: text=\"%s\" voice=\"%s\" sourceFG=%d type=%d rawDelay=%u triggers12Or34=%u "
			   "colorIff=%u speakerHeader=%u specialMeaning=%u io=%u teams=",
			   i, text, voice, msg->originatingFG, msg->type, (unsigned)msg->rawDelay,
			   (unsigned)msg->triggers12OrTriggers34, (unsigned)msg->colorIff, (unsigned)msg->speakerHeader,
			   (unsigned)msg->specialMeaning, (unsigned)msg->ioReserved);
		for (team = 0; team < 10; ++team) {
			printf("%s%u", team ? "," : "", (unsigned)msg->sentToTeam[team]);
		}
		printf("\n");
		print_trigger_pair("  triggers[0]:", &msg->triggers[0]);
		print_trigger_pair("  triggers[1]:", &msg->triggers[1]);
		print_trigger_pair("  special:", &msg->special);
	}
}

static void dump_goals(const MissionFile* mission) {
	int team;
	int goal;

	printf("\nGlobal Goals\n");
	for (team = 0; team < MAX_MISSION_TEAMS; ++team) {
		for (goal = 0; goal < mission->globalGoalCount[team]; ++goal) {
			print_global_goal(team, goal, &mission->globalGoals[team][goal]);
		}
	}
}

static void dump_teams(const MissionFile* mission) {
	int team;

	printf("\nTeams\n");
	for (team = 0; team < MAX_MISSION_TEAMS; ++team) {
		const XwaTeam* t;
		char name[17];
		int i;

		if (!mission->teamPresent[team]) {
			continue;
		}
		t = &mission->teams[team];
		copy_fixed_string(name, sizeof(name), t->name, sizeof(t->name));
		printf("Team %d: name=\"%s\" allies=", team, name);
		for (i = 0; i < 10; ++i) {
			printf("%s%u", i ? "," : "", (unsigned)t->allies[i]);
		}
		printf(" eomRawDelay=%u,%u,%u eomSourceFG=%u,%u,%u\n", (unsigned)t->eomRawDelay[0],
			   (unsigned)t->eomRawDelay[1], (unsigned)t->eomRawDelay[2], (unsigned)t->eomSourceFG[0],
			   (unsigned)t->eomSourceFG[1], (unsigned)t->eomSourceFG[2]);
		for (i = 0; i < 6; ++i) {
			char line[65];

			copy_fixed_string(line, sizeof(line), t->endOfMissionMessages[i],
							  sizeof(t->endOfMissionMessages[i]));
			if (line[0]) {
				printf("  EOM %d: \"%s\"\n", i, line);
			}
		}
		for (i = 0; i < 3; ++i) {
			char voice[21];

			copy_fixed_string(voice, sizeof(voice), t->voiceIDs[i], sizeof(t->voiceIDs[i]));
			if (voice[0]) {
				printf("  VoiceID %d: \"%s\"\n", i, voice);
			}
		}
	}
}

static void dump_flight_groups(const MissionFile* mission) {
	int fgIdx;

	printf("\nFlight Groups\n");
	for (fgIdx = 0; fgIdx < (int)mission->header.numFlightGroups; ++fgIdx) {
		const XwaFlightGroup* fg;
		char name[21];
		char cargo[21];
		char specialCargo[21];
		char role[26];
		char pilot[21];
		int i;

		fg = &mission->flightGroups[fgIdx];
		copy_fixed_string(name, sizeof(name), fg->name, sizeof(fg->name));
		copy_fixed_string(cargo, sizeof(cargo), fg->cargo, sizeof(fg->cargo));
		copy_fixed_string(specialCargo, sizeof(specialCargo), fg->specialCargo, sizeof(fg->specialCargo));
		copy_fixed_string(role, sizeof(role), fg->craftRole, sizeof(fg->craftRole));
		copy_fixed_string(pilot, sizeof(pilot), fg->pilotID, sizeof(fg->pilotID));

		printf("FG %03d: name=\"%s\" craftType=%u count=%u waves=%u waveDelay=%u status1=%u status2=%u "
			   "iff=%u team=%u groupAI=%u playerNumber=%u playerCraft=%u\n",
			   fgIdx, name, (unsigned)fg->craftType, (unsigned)fg->numberOfCraft, (unsigned)fg->numberOfWaves,
			   (unsigned)fg->wavesDelay, (unsigned)fg->status1, (unsigned)fg->status2, (unsigned)fg->iff,
			   (unsigned)fg->team, (unsigned)fg->groupAI, (unsigned)fg->playerNumber,
			   (unsigned)fg->playerCraft);
		printf("  designation enable=%u/%u values=%u/%u comm=%u radio=%u markings=%u formation=%u spacing=%u "
			   "globalGroup=%u globalUnit=%u cargoIdx=%u specialCargoIdx=%u\n",
			   (unsigned)fg->enableDesignation, (unsigned)fg->enableDesignation2, (unsigned)fg->designation1,
			   (unsigned)fg->designation2, (unsigned)fg->comm, (unsigned)fg->radio, (unsigned)fg->markings,
			   (unsigned)fg->formation, (unsigned)fg->formationSpacing, (unsigned)fg->globalGroup,
			   (unsigned)fg->globalUnit, (unsigned)fg->globalCargoIndex,
			   (unsigned)fg->globalSpecialCargoIndex);
		printf("  cargo=\"%s\" specialCargo=\"%s\" role=\"%s\" pilot=\"%s\" specialCargoCraft=%u "
			   "randomSpecial=%u\n",
			   cargo, specialCargo, role, pilot, (unsigned)fg->specialCargoCraft,
			   (unsigned)fg->randomSpecialCargoCraft);
		printf("  loadout warhead=%u beam=%u countermeasures=%u handicap=%u explosionTime=%u "
			   "optionalCategory=%u\n",
			   (unsigned)fg->warhead, (unsigned)fg->beam, (unsigned)fg->countermeasures,
			   (unsigned)fg->handicap, (unsigned)fg->craftExplosionTime, (unsigned)fg->optionalCraftCategory);
		printf("  arrival method=%u mothership=%u difficulty=%u randDelay=%u:%u delay=%u:%u onlyIfHuman=%u "
			   "stopArrivingWhen=%u\n",
			   (unsigned)fg->arrivalMethod, (unsigned)fg->arrivalMothership, (unsigned)fg->arrivalDifficulty,
			   (unsigned)fg->arrivalRandDelayMinutes, (unsigned)fg->arrivalRandDelaySeconds,
			   (unsigned)fg->arrivalDelayMinutes, (unsigned)fg->arrivalDelaySeconds,
			   (unsigned)fg->arriveOnlyIfHuman, (unsigned)fg->stopArrivingWhen);
		print_trigger_pair("  arrival[0]:", &fg->arrival[0]);
		print_trigger_pair("  arrival[1]:", &fg->arrival[1]);
		printf("  arrivals12Or34=%u\n", (unsigned)fg->arrivals12OrArrivals34);
		printf("  departure method=%u mothership=%u delay=%u:%u clock=%u:%u abortTrigger=%u "
			   "alternateMothership=%u altUsed=%u capturedMothership=%u capturedViaMothership=%u "
			   "editorMothership=%d\n",
			   (unsigned)fg->departMethod, (unsigned)fg->departureMothership,
			   (unsigned)fg->departureDelayMinutes, (unsigned)fg->departureDelaySeconds,
			   (unsigned)fg->departureClockMin, (unsigned)fg->departureClockSec, (unsigned)fg->abortTrigger,
			   (unsigned)fg->alternateMothership, (unsigned)fg->alternateMothershipUsed,
			   (unsigned)fg->capturedDepartureMothership, (unsigned)fg->capturedDepartViaMothership,
			   (int)fg->editorMothership);
		print_trigger_pair("  departure:", &fg->departure);

		print_waypoint("  start0=", &fg->missionPoints[XWA_FG_POINT_START_1]);
		printf(" region=%u\n", (unsigned)fg->missionPointRegions[XWA_FG_POINT_START_1]);
		print_waypoint("  start1=", &fg->missionPoints[XWA_FG_POINT_START_2]);
		printf(" region=%u\n", (unsigned)fg->missionPointRegions[XWA_FG_POINT_START_2]);
		print_waypoint("  captureHyper=", &fg->missionPoints[XWA_FG_POINT_CAPTURE_HYPER]);
		printf(" region=%u\n", (unsigned)fg->missionPointRegions[XWA_FG_POINT_CAPTURE_HYPER]);
		print_waypoint("  hyper=", &fg->missionPoints[XWA_FG_POINT_HYPER]);
		printf(" region=%u\n", (unsigned)fg->missionPointRegions[XWA_FG_POINT_HYPER]);
		printf("  backdrop=%d disableWaveNumbering=%u editorWaypointShown=%u linkId=%d "
			   "yaw/pitch/roll=%u/%u/%u\n",
			   fg->backdrop, (unsigned)fg->disableWaveNumbering, (unsigned)fg->editorWaypointShown,
			   (int)fg->linkId, (unsigned)fg->yaw, (unsigned)fg->pitch, (unsigned)fg->roll);

		for (i = 0; i < 8; ++i) {
			printf("  optionalWarhead[%d]=%u\n", i, (unsigned)fg->optionalWarheads[i]);
		}
		for (i = 0; i < 6; ++i) {
			printf("  optionalBeam[%d]=%u\n", i, (unsigned)fg->optionalBeams[i]);
		}
		for (i = 0; i < 4; ++i) {
			printf("  optionalCountermeasure[%d]=%u\n", i, (unsigned)fg->optionalCountermeasures[i]);
		}
		for (i = 0; i < 10; ++i) {
			printf("  optionalCraft[%d]=%u count=%u waves=%u\n", i, (unsigned)fg->optionalCraft[i],
				   (unsigned)fg->numberOfOptionalCraft[i], (unsigned)fg->numberOfOptionalCraftWaves[i]);
		}

		for (i = 0; i < 16; ++i) {
			print_order(mission, i, &fg->orders[i]);
			if (trigger_pair_nonempty(&fg->skipTriggers[i])) {
				print_trigger_pair("    skipTrigger:", &fg->skipTriggers[i]);
			}
		}
		for (i = 0; i < 8; ++i) {
			if (fg_goal_nonempty(&fg->fgGoals[i])) {
				print_fg_goal(i, &fg->fgGoals[i]);
			}
		}
		printf("\n");
	}
}

static void print_override_string(const char* kind, int a, int b, int c, const char* text) {
	if (text[0] == '\0') {
		return;
	}
	if (c >= 0) {
		printf("%s[%d][%d][%d]=\"%s\"\n", kind, a, b, c, text);
	} else if (b >= 0) {
		printf("%s[%d][%d]=\"%s\"\n", kind, a, b, text);
	} else {
		printf("%s[%d]=\"%s\"\n", kind, a, text);
	}
}

static void dump_printable_tail(FILE* fp, long begin, long end) {
	char span[129];
	int ch;
	long spanStart;
	int spanLen;

	if (begin >= end) {
		return;
	}
	if (fseek(fp, begin, SEEK_SET) != 0) {
		fprintf(stderr, "mission_dump: cannot seek modern text tail: %s\n", strerror(errno));
		return;
	}

	printf("\nModern Text Tail Printable Spans\n");
	spanStart = begin;
	spanLen = 0;
	while (ftell(fp) < end) {
		long pos;

		pos = ftell(fp);
		ch = fgetc(fp);
		if (ch == EOF) {
			break;
		}

		if (ch >= 0x20 && ch <= 0x7e) {
			if (spanLen == 0) {
				spanStart = pos;
			}
			if (spanLen < (int)sizeof(span) - 1) {
				span[spanLen++] = (char)ch;
			}
		} else {
			if (spanLen >= 4) {
				span[spanLen] = '\0';
				printf("  +0x%04lx \"%s\"\n", spanStart - begin, span);
			}
			spanLen = 0;
		}
	}

	if (spanLen >= 4) {
		span[spanLen] = '\0';
		printf("  +0x%04lx \"%s\"\n", spanStart - begin, span);
	}
}

static void dump_overrides(const char* path, const MissionFile* mission) {
	FILE* fp;
	char text[65];
	int fgIdx;
	int groupIdx;
	int slotIdx;
	int teamIdx;
	int goalIdx;
	int conditionIdx;
	int textIdx;

	printf("\nTail\n");
	printf("  briefingOffset=0x%lx textTailOffset=0x%lx overrideOffset=0x%lx endOffset=0x%lx\n",
		   mission->briefingOffset, mission->textTailOffset, mission->overrideOffset, mission->endOffset);
	printf("  briefingBytes=%ld textTailBytes=%ld overrideBytes=%ld\n",
		   mission->textTailOffset - mission->briefingOffset,
		   mission->overrideOffset - mission->textTailOffset, mission->endOffset - mission->overrideOffset);

	fp = fopen(path, "rb");
	if (fp == NULL) {
		fprintf(stderr, "mission_dump: cannot reopen %s: %s\n", path, strerror(errno));
		return;
	}
	dump_printable_tail(fp, mission->textTailOffset, mission->overrideOffset);
	if (fseek(fp, mission->overrideOffset, SEEK_SET) != 0) {
		fprintf(stderr, "mission_dump: cannot seek override strings: %s\n", strerror(errno));
		fclose(fp);
		return;
	}

	printf("\nOverride Strings\n");
	for (fgIdx = 0; fgIdx < (int)mission->header.numFlightGroups; ++fgIdx) {
		for (groupIdx = 0; groupIdx < 8; ++groupIdx) {
			for (slotIdx = 0; slotIdx < 3; ++slotIdx) {
				if (!read_modern_string(fp, text, "flight-group override string")) {
					fclose(fp);
					return;
				}
				print_override_string("FgOverride", fgIdx, groupIdx, slotIdx, text);
			}
		}
	}
	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		for (goalIdx = 0; goalIdx < 7; ++goalIdx) {
			for (conditionIdx = 0; conditionIdx < 4; ++conditionIdx) {
				for (textIdx = 0; textIdx < 3; ++textIdx) {
					if (!read_modern_string(fp, text, "global-goal override string")) {
						fclose(fp);
						return;
					}
					if (text[0]) {
						printf("GlobalGoalOverride[%d][%d][%d][%d]=\"%s\"\n", teamIdx, goalIdx, conditionIdx,
							   textIdx, text);
					}
				}
			}
		}
	}
	for (fgIdx = 0; fgIdx < (int)mission->header.numFlightGroups; ++fgIdx) {
		for (groupIdx = 0; groupIdx < 4; ++groupIdx) {
			for (slotIdx = 0; slotIdx < 4; ++slotIdx) {
				if (!read_modern_string(fp, text, "AI override string")) {
					fclose(fp);
					return;
				}
				print_override_string("AiOverride", fgIdx, groupIdx, slotIdx, text);
			}
		}
	}

	printf("OverrideStringsEnd=0x%lx remainingBytes=%ld\n", ftell(fp), mission->endOffset - ftell(fp));
	fclose(fp);
}

static void usage(const char* argv0) { fprintf(stderr, "usage: %s MISSION.TIE\n", argv0); }

int main(int argc, char** argv) {
	MissionFile mission;

	if (argc != 2) {
		usage(argv[0]);
		return 2;
	}

	if (!load_mission(argv[1], &mission)) {
		return 1;
	}

	dump_header(&mission);
	dump_messages(&mission);
	dump_goals(&mission);
	dump_teams(&mission);
	dump_flight_groups(&mission);
	dump_overrides(argv[1], &mission);
	return 0;
}
