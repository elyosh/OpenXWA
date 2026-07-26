#ifndef XWA_FLIGHT_HUD_HUD_H
#define XWA_FLIGHT_HUD_HUD_H

#include "xwa/flight/object/object.h"
#include "xwa/render/renderer.h"
#include "xwa/util/memory.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct HudInFlightMessageRecord {
	uint16_t stateOrMessageId;
	uint16_t voiceSfxId;
	uint16_t clockWord;
	uint8_t  clockTick;
	uint8_t  clockMinute;
	uint8_t  clockHour;
	uint8_t  paneType;
	uint16_t senderIff;
	uint8_t  ageTicks;
	uint8_t  showCount;
	char     text[70];
} HudInFlightMessageRecord;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) HudInFlightMessageRecord {
	uint16_t stateOrMessageId;
	uint16_t voiceSfxId;
	uint16_t clockWord;
	uint8_t  clockTick;
	uint8_t  clockMinute;
	uint8_t  clockHour;
	uint8_t  paneType;
	uint16_t senderIff;
	uint8_t  ageTicks;
	uint8_t  showCount;
	char     text[70];
} HudInFlightMessageRecord;
#endif

typedef struct PlayerFlightTransientTimers {
	uint16_t readyMessagePaneTimer;
	uint16_t field_02;
	uint16_t field_04;
	uint16_t systemMessagePaneTimer;
	uint16_t flightGroupMessagePaneTimer;
	uint16_t targetDescriptionRefreshTimer;
	uint16_t field_0C;
	uint16_t field_0E;
	uint16_t dynamicMusicCooldown;
	uint16_t debrisRecycleCooldown;
	uint16_t missionSuccessMusicTimer;
	uint16_t missionLossMusicTimer;
	uint16_t field_18;
} PlayerFlightTransientTimers;

typedef char hud_in_flight_message_record_size[(sizeof(HudInFlightMessageRecord) == 0x54) ? 1 : -1];
typedef char player_flight_transient_timers_size[(sizeof(PlayerFlightTransientTimers) == 0x1A) ? 1 : -1];

typedef struct HudElementEnabled {
	unsigned char enabled;
	unsigned char reserved[3];
} HudElementEnabled;

typedef struct HudDrawTarget {
	uint16_t flags;
	void*    pixels;
	int32_t  width;
	int32_t  width2;
	int32_t  maxX;
	int32_t  height;
	int32_t  height2;
	int32_t  maxY;
	int32_t  clipX0;
	int32_t  clipY0;
	int32_t  clipX1;
	int32_t  clipY1;
	int32_t  viewX0;
	int32_t  viewY0;
	int32_t  viewX1;
	int32_t  viewY1;
	int32_t  bpp;
	int32_t  bytesPerPixel;
	int32_t  pitch;
	int32_t  flipY;
	int32_t  field78;
	int32_t  field82;
} HudDrawTarget;

typedef struct RadarEllipseClampLimit {
	uint8_t xLimit;
	uint8_t yLimit;
} RadarEllipseClampLimit;

typedef struct HudCockpitMaskSprite {
	uint16_t spriteId;
	int16_t  x;
	int16_t  y;
} HudCockpitMaskSprite;

typedef struct RadarBlip {
	uint16_t x;
	uint16_t y;
	uint16_t color;
} RadarBlip;

typedef union RadarBlipCount {
	int value;
	uint16_t lowWord;
} RadarBlipCount;

typedef struct HudPoint {
	int x;
	int y;
} HudPoint;

typedef enum HudThreatIndicatorSlot {
	HUD_THREAT_INDICATOR_CENTER = 0,
	HUD_THREAT_INDICATOR_LEFT   = 1,
	HUD_THREAT_INDICATOR_RIGHT  = 2,
	HUD_THREAT_INDICATOR_BOTTOM = 3,
} HudThreatIndicatorSlot;

typedef enum InFlightMessageId {
	MSG_VERSION = 0,
	MSG_MISSION_PAUSED = 1,
	MSG_MISSION_RESUMED = 2,
	MSG_LASERS_ARMED = 3,
	MSG_IONS_ARMED = 4,
	MSG_CANNONS_SINGLE = 5,
	MSG_CANNONS_DUAL = 6,
	MSG_CANNONS_LINKED = 7,
	MSG_LASER_SYSTEMS_LINKED = 8,
	MSG_LASERS_CONVERGE_NA = 9,
	MSG_LASERS_CONVERGE_OFF = 10,
	MSG_LASERS_CONVERGE_MIN = 11,
	MSG_LASERS_CONVERGE_MED = 12,
	MSG_LASERS_CONVERGE_MAX = 13,
	MSG_LASERS_CONVERGE_ON = 14,
	MSG_LAUNCHER_ARMED_PROTON = 15,
	MSG_LAUNCHER_ARMED_CONCUSSION = 16,
	MSG_LAUNCHER_ARMED_ADV_PROTON = 17,
	MSG_LAUNCHER_ARMED_ADV_CONCUSSION = 18,
	MSG_LAUNCHER_ARMED_SPACE_BOMB = 19,
	MSG_LAUNCHER_ARMED_HEAVY_ROCKET = 20,
	MSG_LAUNCHER_ARMED_MAG_PULSE = 21,
	MSG_LAUNCHER_ARMED_ION_PULSE = 22,
	MSG_LAUNCHER_ARMED_FUTURE1 = 23,
	MSG_LAUNCHER_ARMED_FUTURE2 = 24,
	MSG_LAUNCHER_SINGLE_PROTON = 25,
	MSG_LAUNCHER_SINGLE_CONCUSSION = 26,
	MSG_LAUNCHER_SINGLE_ADV_PROTON = 27,
	MSG_LAUNCHER_SINGLE_ADV_CONCUSSION = 28,
	MSG_LAUNCHER_SINGLE_SPACE_BOMB = 29,
	MSG_LAUNCHER_SINGLE_HEAVY_ROCKET = 30,
	MSG_LAUNCHER_SINGLE_MAG_PULSE = 31,
	MSG_LAUNCHER_SINGLE_ION_PULSE = 32,
	MSG_LAUNCHER_SINGLE_FUTURE1 = 33,
	MSG_LAUNCHER_SINGLE_FUTURE2 = 34,
	MSG_LAUNCHER_DUAL_PROTON = 35,
	MSG_LAUNCHER_DUAL_CONCUSSION = 36,
	MSG_LAUNCHER_DUAL_ADV_PROTON = 37,
	MSG_LAUNCHER_DUAL_ADV_CONCUSSION = 38,
	MSG_LAUNCHER_DUAL_SPACE_BOMB = 39,
	MSG_LAUNCHER_DUAL_HEAVY_ROCKET = 40,
	MSG_LAUNCHER_DUAL_MAG_PULSE = 41,
	MSG_LAUNCHER_DUAL_ION_PULSE = 42,
	MSG_LAUNCHER_DUAL_FUTURE1 = 43,
	MSG_LAUNCHER_DUAL_FUTURE2 = 44,
	MSG_LAUNCHER_EMPTY_PROTON = 45,
	MSG_LAUNCHER_EMPTY_CONCUSSION = 46,
	MSG_LAUNCHER_EMPTY_ADV_PROTON = 47,
	MSG_LAUNCHER_EMPTY_ADV_CONCUSSION = 48,
	MSG_LAUNCHER_EMPTY_SPACE_BOMB = 49,
	MSG_LAUNCHER_EMPTY_HEAVY_ROCKET = 50,
	MSG_LAUNCHER_EMPTY_MAG_PULSE = 51,
	MSG_LAUNCHER_EMPTY_ION_PULSE = 52,
	MSG_LAUNCHER_EMPTY_FUTURE1 = 53,
	MSG_LAUNCHER_EMPTY_FUTURE2 = 54,
	MSG_LAUNCHER_1FIRE_PROTON = 55,
	MSG_LAUNCHER_1FIRE_CONCUSSION = 56,
	MSG_LAUNCHER_1FIRE_ADV_PROTON = 57,
	MSG_LAUNCHER_1FIRE_ADV_CONCUSSION = 58,
	MSG_LAUNCHER_1FIRE_SPACE_BOMB = 59,
	MSG_LAUNCHER_1FIRE_HEAVY_ROCKET = 60,
	MSG_LAUNCHER_1FIRE_MAG_PULSE = 61,
	MSG_LAUNCHER_1FIRE_ION_PULSE = 62,
	MSG_LAUNCHER_1FIRE_FUTURE1 = 63,
	MSG_LAUNCHER_1FIRE_FUTURE2 = 64,
	MSG_LAUNCHER_2FIRE_PROTON = 65,
	MSG_LAUNCHER_2FIRE_CONCUSSION = 66,
	MSG_LAUNCHER_2FIRE_ADV_PROTON = 67,
	MSG_LAUNCHER_2FIRE_ADV_CONCUSSION = 68,
	MSG_LAUNCHER_2FIRE_SPACE_BOMB = 69,
	MSG_LAUNCHER_2FIRE_HEAVY_ROCKET = 70,
	MSG_LAUNCHER_2FIRE_MAG_PULSE = 71,
	MSG_LAUNCHER_2FIRE_ION_PULSE = 72,
	MSG_LAUNCHER_2FIRE_FUTURE1 = 73,
	MSG_LAUNCHER_2FIRE_FUTURE2 = 74,
	MSG_SHIELD_FULL_FWD = 75,
	MSG_SHIELD_EVEN = 76,
	MSG_SHIELD_FULL_AFT = 77,
	MSG_LASER_REDIRECT_OFF = 78,
	MSG_LASER_REDIRECT_LOW = 79,
	MSG_LASER_REDIRECT_NORMAL = 80,
	MSG_LASER_REDIRECT_HI = 81,
	MSG_LASER_REDIRECT_FULL = 82,
	MSG_SHIELD_REDIRECT_OFF = 83,
	MSG_SHIELD_REDIRECT_LOW = 84,
	MSG_SHIELD_REDIRECT_NORMAL = 85,
	MSG_SHIELD_REDIRECT_HI = 86,
	MSG_SHIELD_REDIRECT_FULL = 87,
	MSG_BEAM_REDIRECT_OFF = 88,
	MSG_BEAM_REDIRECT_LOW = 89,
	MSG_BEAM_REDIRECT_NORMAL = 90,
	MSG_BEAM_REDIRECT_HI = 91,
	MSG_BEAM_REDIRECT_FULL = 92,
	MSG_SYSTEMCOND = 93,
	MSG_DAMAGED = 94,
	MSG_REPAIRED = 95,
	MSG_UNAVAILABLE = 96,
	MSG_ENGINE = 97,
	MSG_FLIGHTCONTROL = 98,
	MSG_LASER = 99,
	MSG_ION = 100,
	MSG_PROTON = 101,
	MSG_BEAM = 102,
	MSG_TARGCOMPUTER = 103,
	MSG_COUNTERMEASURE = 104,
	MSG_SHIELDS = 105,
	MSG_HYPERDRIVE = 106,
	MSG_LAUNCHER = 107,
	MSG_COMMUNICATIONS = 108,
	MSG_DETAIL_LEVEL = 109,
	MSG_DETAIL_LOWEST = 110,
	MSG_DETAIL_HIGH = 111,
	MSG_DETAIL_HIGHEST = 112,
	MSG_HYPER_PREPARE = 113,
	MSG_HYPER_ABORTED = 114,
	MSG_ENTER_HYPER = 115,
	MSG_NO_HYPERSPACE = 116,
	MSG_RETURN_HANGAR = 117,
	MSG_RETURN_HANGAR2 = 118,
	MSG_NO_HYPER = 119,
	MSG_INITIATING_HYPER = 120,
	MSG_ASK_ABOUT_HYPER = 121,
	MSG_SENSOR_OBJ = 122,
	MSG_SENSOR_OBJS = 123,
	MSG_MISSILE_WARNING = 124,
	MSG_052_MISSILE_TARGETED = 125,
	MSG_050_MISSILE_LOCK_PROMPT = 126,
	MSG_ENGINE_NO = 127,
	MSG_ENGINE_13 = 128,
	MSG_ENGINE_23 = 129,
	MSG_ENGINE_FULL = 130,
	MSG_CONFIGURATION_SAVED = 131,
	MSG_CMD_MODE_OFF = 132,
	MSG_CMD_MODE_ON = 133,
	MSG_SFOILS_OPENING = 134,
	MSG_SFOILS_CLOSING = 135,
	MSG_SFOILS_OPEN = 136,
	MSG_SFOILS_CLOSED = 137,
	MSG_SFOILS_NO_FIRE = 138,
	MSG_TRANSFER_TO_LASER = 139,
	MSG_TRANSFER_TO_SHIELDS = 140,
	MSG_FRAME_MANY = 141,
	MSG_FRAME_SINGLE = 142,
	MSG_HYPERSPACED = 143,
	MSG_DESTROYED = 144,
	MSG_DISABLED = 145,
	MSG_FIXED = 146,
	MSG_CAPTURED = 147,
	MSG_DOCKED = 148,
	MSG_ENTERED_HANGAR = 149,
	MSG_IDED = 150,
	MSG_IS_HYPERSPACING = 151,
	MSG_FGMANY = 152,
	MSG_FGONE = 153,
	MSG_NO_COMMLINK = 154,
	MSG_ACK_MANY = 155,
	MSG_ACK_SINGLE = 156,
	MSG_ACK_HEAD_HOME = 157,
	MSG_ACK_COVER_ME = 158,
	MSG_ACK_EVASIVE = 159,
	MSG_ACK_WAITING = 160,
	MSG_ACK_GOING = 161,
	MSG_ACK_USING_TARGET = 162,
	MSG_ACK_IGNORE_TARGET = 163,
	MSG_ACK_TACTICAL_ATTACK_COMPONENT = 164,
	MSG_ACK_TACTICAL_TARGET_TYPE = 165,
	MSG_ACK_TACTICAL_DISABLE_TARGET = 166,
	MSG_ACK_TACTICAL_INSPECT_TARGET = 167,
	MSG_ACK_FORMATION_REJOIN = 168,
	MSG_ACK_FORMATION_LINE_ABREAST = 169,
	MSG_ACK_FORMATION_LINE_ASTERN = 170,
	MSG_ACK_FORMATION_VIC = 171,
	MSG_ACK_FORMATION_FINGER_FOUR = 172,
	MSG_ACK_FORMATION_STAR = 173,
	MSG_ACK_FORMATION_TIGHTEN = 174,
	MSG_ACK_FORMATION_LOOSEN = 175,
	MSG_ACK_SUPPORT_REPAIR_TARGET = 176,
	MSG_ACK_SUPPORT_CAPTURE_TARGET = 177,
	MSG_ACK_SUPPORT_PICKUP_TARGET = 178,
	MSG_ACK_SUPPORT_DESTROY_TARGET = 179,
	MSG_REPORT_DOCK_COMP = 180,
	MSG_REPORT_IN_MANY = 181,
	MSG_REPORT_IN_SINGLE = 182,
	MSG_AI_STATIONARY = 183,
	MSG_AI_LEAD_FORM = 184,
	MSG_AI_PROTECT = 185,
	MSG_AI_LOOK_ATTACK = 186,
	MSG_AI_LOOK_ESCORTERS = 187,
	MSG_AI_SETUP_ATTACK = 188,
	MSG_AI_ATTACK_TARGET = 189,
	MSG_AI_EVASIVE = 190,
	MSG_AI_FOLLOW_LEADER = 191,
	MSG_AI_LOOK_DISABLE = 192,
	MSG_AI_FLYING_ESCORT = 193,
	MSG_AI_LOOK_BOARD = 194,
	MSG_AI_LOOK_CAPTURE = 195,
	MSG_AI_LOOK_PICKUP = 196,
	MSG_AI_DOCKING = 197,
	MSG_AI_FLY_RENDEZVOUS = 198,
	MSG_AI_AWAITING_BOARD = 199,
	MSG_AI_AWAITING_REPAIR = 200,
	MSG_AI_HOLDING_STATION = 201,
	MSG_AI_FLY_HOME = 202,
	MSG_AI_ENTER_HANGAR = 203,
	MSG_AI_EXIT_HANGAR = 204,
	MSG_AI_INTO_HYPER = 205,
	MSG_AI_OUT_HYPER = 206,
	MSG_AI_PATROL = 207,
	MSG_AI_WAIT_RETURN = 208,
	MSG_AI_WAIT_CREATE = 209,
	MSG_AI_CHANGING_ORDERS = 210,
	MSG_AI_WAIT_GO = 211,
	MSG_AI_WAIT = 212,
	MSG_AI_DROP_OFF = 213,
	MSG_AI_SELF_DESTRUCT = 214,
	MSG_AI_KAMIKAZE = 215,
	MSG_AI_ORBIT = 216,
	MSG_AI_TRANSFER_CARGO = 217,
	MSG_AI_SELF_CAPTURE = 218,
	MSG_AI_RELEASE1 = 219,
	MSG_AI_RELEASE2 = 220,
	MSG_AI_DELIVER = 221,
	MSG_AI_CHANGE_SIDES = 222,
	MSG_AI_START_OVER = 223,
	MSG_AI_BACK_UP = 224,
	MSG_AI_HYPER_BUOY = 225,
	MSG_AI_DISAPPEAR = 226,
	MSG_AI_REPAIR_SELF = 227,
	MSG_AI_HYPER_HOME = 228,
	MSG_AI_INSPECT_1 = 229,
	MSG_AI_INSPECT_2 = 230,
	MSG_AI_INSPECT_3 = 231,
	MSG_AI_PLAYER_CAPTURE_LDR_1 = 232,
	MSG_AI_PLAYER_CAPTURE_LDR_2 = 233,
	MSG_AI_PLAYER_CAPTURE_LDR_3 = 234,
	MSG_AI_PLAYER_CAPTURE_LDR_4 = 235,
	MSG_AI_PLAYER_CAPTURE_LDR_5 = 236,
	MSG_AI_PLAYER_FOLLOW = 237,
	MSG_AI_PLAYER_INSPECT_LDR_1 = 238,
	MSG_AI_PLAYER_INSPECT_LDR_2 = 239,
	MSG_AI_PLAYER_INSPECT_LDR_3 = 240,
	MSG_AI_PLAYER_DISABLE_LDR_2 = 241,
	MSG_AI_PLAYER_BOARD_TO_REPAIR = 242,
	MSG_AI_PLAYER_BOARD_TO_CAPTURE = 243,
	MSG_AI_PLAYER_BOARD_TO_PICK_UP = 244,
	MSG_AI_PLAYER_BOARD_TO_DESTROY = 245,
	MSG_AI_PLAYER_BOARD_TO_DEFUSE = 246,
	MSG_AI_PLAYER_BOARD_2 = 247,
	MSG_AI_PLAYER_BOARD_3 = 248,
	MSG_AI_RESUME_MISSION = 249,
	MSG_AI_HOMING_1 = 250,
	MSG_AI_HOMING_2 = 251,
	MSG_AI_PARK_1 = 252,
	MSG_AI_PARK_2 = 253,
	MSG_AI_WORK_ON_1 = 254,
	MSG_AI_WORK_ON_2 = 255,
	MSG_AI_WORK_ON_3 = 256,
	MSG_AI_DEATH_STAR_FOLLOW = 257,
	MSG_MISSION_COMPLETE = 258,
	MSG_MISSION_SECONDARY = 259,
	MSG_MISSION_BONUS = 260,
	MSG_BLANK = 261,
	MSG_POINTS_AWARDED = 262,
	MSG_LEVEL_COMPLETED = 263,
	MSG_BONUS_POINTS = 264,
	MSG_PENALTY_POINTS = 265,
	MSG_TWO_MIN_WARNING = 266,
	MSG_ONE_MIN_WARNING = 267,
	MSG_TIME_OUT = 268,
	MSG_TIME_LIMIT_IMPOSED_1 = 269,
	MSG_TIME_LIMIT_IMPOSED_2 = 270,
	MSG_MISSION_IMPOSSIBLE = 271,
	MSG_RADIO_BLANK = 272,
	MSG_EMPIRE_DRAWS = 273,
	MSG_REBEL_DRAWS = 274,
	MSG_EMPIRE_LOST_WIN = 275,
	MSG_REBEL_LOST_WIN = 276,
	MSG_EJECTED_SAFELY = 277,
	MSG_DIED = 278,
	MSG_HANGAR_TRACTOR = 279,
	MSG_END_MISSION = 280,
	MSG_END_MISSION_PENALTY = 281,
	MSG_END_MISSION_INCOMPLETE = 282,
	MSG_END_MISSION_DISCONNECT = 283,
	MSG_END_MISSION_ABORT = 284,
	MSG_COLLISION = 285,
	MSG_ENGINE_WASH_DAMAGE = 286,
	MSG_NO_CRAFT_LEFT = 287,
	MSG_NO_CRAFT_TARGETED = 288,
	MSG_TARGET_DESTROYED = 289,
	MSG_NO_PLAYER_LOCATING = 290,
	MSG_NOT_EQUIPPED_SHIELDS = 291,
	MSG_NOT_EQUIPPED_BEAM = 292,
	MSG_NOT_EQUIPPED_SFOIL = 293,
	MSG_NOT_EQUIPPED_SLAM = 294,
	MSG_NO_REINFORCE = 295,
	MSG_REINFORCE = 296,
	MSG_NOMORE_REINFORCE = 297,
	MSG_REINFORCE_CONFIRM = 298,
	MSG_HAS_DOCKED = 299,
	MSG_ENTERING_OBJ = 300,
	MSG_ENTERING_OBJS = 301,
	MSG_TRACTOR_BEAM_ON = 302,
	MSG_JAMMING_BEAM_ON = 303,
	MSG_DECOY_BEAM_ON = 304,
	MSG_ENERGY_BEAM_ON = 305,
	MSG_FUTURE1_BEAM_ON = 306,
	MSG_FUTURE2_BEAM_ON = 307,
	MSG_TRACTOR_BEAM_OFF = 308,
	MSG_JAMMING_BEAM_OFF = 309,
	MSG_DECOY_BEAM_OFF = 310,
	MSG_ENERGY_BEAM_OFF = 311,
	MSG_FUTURE1_BEAM_OFF = 312,
	MSG_FUTURE2_BEAM_OFF = 313,
	MSG_TRACTOR_BEAM_SELECT = 314,
	MSG_JAMMING_BEAM_SELECT = 315,
	MSG_DECOY_BEAM_SELECT = 316,
	MSG_ENERGY_BEAM_SELECT = 317,
	MSG_FUTURE1_BEAM_SELECT = 318,
	MSG_FUTURE2_BEAM_SELECT = 319,
	MSG_BEAM_NO_ENERGY = 320,
	MSG_BEAM_DISRUPTED = 321,
	MSG_TARGET_BLOCKED = 322,
	MSG_RELOAD_ON_WAY = 323,
	MSG_FLY_TO_RELOAD = 324,
	MSG_ZERO_THROTTLE_FOR_RELOAD = 325,
	MSG_ABORTING_RELOAD = 326,
	MSG_ACCEL_TIME = 327,
	MSG_NORMAL_TIME = 328,
	MSG_OBJECT_TARGETED = 329,
	MSG_OBJECT_DESTROYED = 330,
	MSG_OBJECT_IGNORED = 331,
	MSG_WAITING_ENGINE_NO = 332,
	MSG_GOING_ENGINE_FULL = 333,
	MSG_INITIATE_PICKUP = 334,
	MSG_PICKUP_NO_TARGET = 335,
	MSG_PICKUP_NOT_VALID = 336,
	MSG_PICKUP_ALREADY_CARRY = 337,
	MSG_PICKUP_NOT_STATIONARY = 338,
	MSG_PICKUP_TOO_LARGE = 339,
	MSG_PICKUP_TOO_FAR_AWAY = 340,
	MSG_PICKUP_ABORTED = 341,
	MSG_PICKUP_SECURED = 342,
	MSG_INITIATE_BOARDING = 343,
	MSG_DOCK_TOO_FAR_AWAY1 = 344,
	MSG_DOCK_TOO_FAR_AWAY2 = 345,
	MSG_DOCK_NOT_STATIONARY = 346,
	MSG_DOCK_NOT_VALID = 347,
	MSG_INITIATE_DOCKING = 348,
	MSG_DOCK_NO_LOCATIONS = 349,
	MSG_DOCK_NO_SERVICE = 350,
	MSG_DOCK_ABORT = 351,
	MSG_BOARD_COMPLETE = 352,
	MSG_OBJECT_RELEASED = 353,
	MSG_OBJECT_DELIVERED = 354,
	MSG_BUOY_ACTIVATED = 355,
	MSG_ACK_WINGMAN = 356,
	MSG_USE_MY_TARGET = 357,
	MSG_IGNORE_MY_TARGET = 358,
	MSG_HEAD_HOME = 359,
	MSG_WAIT_FOR_ORDERS = 360,
	MSG_GO_AHEAD = 361,
	MSG_EVADE = 362,
	MSG_REPORT_IN = 363,
	MSG_COVER_ME_ATTACKER = 364,
	MSG_COVER_ME = 365,
	MSG_MATCHING_SPEEDS = 366,
	MSG_TRY_MATCHING = 367,
	MSG_OWN_SIDE = 368,
	MSG_MISSILE_DESTROYED = 369,
	MSG_LASER_DRAINAGE = 370,
	MSG_OVERDRIVE_ON = 371,
	MSG_OVERDRIVE_OFF = 372,
	MSG_OVERDRIVE_UNABLE = 373,
	MSG_BRIGHTNESS = 374,
	MSG_BRIGHTNESS_16BIT = 375,
	MSG_FFEEDBACK = 376,
	MSG_FFEEDBACK_CENTERING = 377,
	MSG_FEEDBACK_UNUSED = 378,
	MSG_FILM_NO_PAUSE = 379,
	MSG_FILM_NO_OPTIONS = 380,
	MSG_JUMP_TO_NEW_CRAFT = 381,
	MSG_PREVIOUS_DESTROYED = 382,
	MSG_PREVIOUS_HYPERSPACED = 383,
	MSG_PREVIOUS_ENTERED_HANGAR = 384,
	MSG_NO_WINGMEN = 385,
	MSG_NO_JUMPING = 386,
	MSG_FIRST_PLACE = 387,
	MSG_218_SECOND = 388,
	MSG_219_THIRD = 389,
	MSG_220_FOURTH = 390,
	MSG_221_FIFTH = 391,
	MSG_222_SIXTH = 392,
	MSG_223_SEVEN = 393,
	MSG_224_EIGHT = 394,
	MSG_225_NINTH = 395,
	MSG_226_TENTH = 396,
	MSG_GENERAL_WIN = 397,
	MSG_OTHER_PLAYER_WIN = 398,
	MSG_OTHER_TEAM_WIN = 399,
	MSG_INSPECT_PLACE = 400,
	MSG_TARGET_DESC_BLANK = 401,
	MSG_TARGET_TYPES = 402,
	MSG_234_BASE = 403,
	MSG_235_STATION = 404,
	MSG_236_MISSION_CRITICAL_CRAFT = 405,
	MSG_237_CONVOY_CRAFT = 406,
	MSG_238_STRIKE_CRAFT = 407,
	MSG_239_REARMING_CRAFT = 408,
	MSG_240_PRIMARY_TARGET = 409,
	MSG_241_SECONDARY_TARGET = 410,
	MSG_242_TERTIARY_TARGET = 411,
	MSG_243_RES_CENTER = 412,
	MSG_244_FACILITY = 413,
	MSG_245_FRIENDLY_CRAFT = 414,
	MSG_246_YOUR_WINGMAN = 415,
	MSG_247_CRAFT = 416,
	MSG_248_CARGO = 417,
	MSG_249_MINE = 418,
	MSG_250_SATELLITE = 419,
	MSG_251_PROBE = 420,
	MSG_252_NAV_BUOY = 421,
	MSG_253_WARHEAD = 422,
	MSG_254_YOUR_SHIP = 423,
	MSG_TARGET_RELS = 424,
	MSG_255_REL_ENEMY = 425,
	MSG_256_REL_FRIENDLY = 426,
	MSG_257_REL_OUR = 427,
	MSG_NEUTRAL = 428,
	MSG_TARGET_ORDERS = 429,
	MSG_259_STOP_ATTACK = 430,
	MSG_TARGET_DESTROY = 431,
	MSG_261_DESTROY_IT = 432,
	MSG_TARGET_ATTACK = 433,
	MSG_263_ATTACK_IT = 434,
	MSG_TARGET_CAPTURE = 435,
	MSG_265_SEIZE_DISABLE = 436,
	MSG_266_INSPECT_IT = 437,
	MSG_TARGET_INSPECT = 438,
	MSG_TARGET_BOARDED = 439,
	MSG_269_BOARD_DISABLE = 440,
	MSG_TARGET_DISABLE = 441,
	MSG_271_DISABLE_IT = 442,
	MSG_272_COMPLETE_MISSION = 443,
	MSG_273_PROTECT_IT = 444,
	MSG_274_DROP_CARGO = 445,
	MSG_275_PROTECT_IT2 = 446,
	MSG_276_RECOVER = 447,
	MSG_277_RECOVER_DISABLE = 448,
	MSG_TARGET_DESTROY_CMP = 449,
	MSG_TARGET_DISABLE_CMP = 450,
	MSG_TARGET_INSPECT_CMP = 451,
	MSG_TARGET_INSPECT_SPC = 452,
	MSG_NO_WARHEADS = 453,
	MSG_TRANSFERRING_ALL = 454,
	MSG_WEAPONS_JAMMED = 455,
	MSG_NO_DCM = 456,
	MSG_NO_DCM_CHAFF = 457,
	MSG_NO_DCM_FLARE = 458,
	MSG_NO_DCM_CLUSTER = 459,
	MSG_NO_DCM_FUTURE1 = 460,
	MSG_CHAFF_FIRED = 461,
	MSG_CHAFF_DONE = 462,
	MSG_CHAFF_SUCCESS = 463,
	MSG_FLARE_FIRED = 464,
	MSG_FLARE_SUCCESS = 465,
	MSG_FLARE_FAILURE = 466,
	MSG_CLUSTER_FIRED = 467,
	MSG_PLAYER_MESSAGE = 468,
	MSG_ENTER_MESSAGE_TEAM = 469,
	MSG_ENTER_MESSAGE_ENEMY = 470,
	MSG_ENTER_MESSAGE_ALL = 471,
	MSG_MESSAGE_SENT = 472,
	MSG_MESSAGE_ABORTED = 473,
	MSG_PLAYER_QUIT = 474,
	MSG_PLAYER_NO_MORE = 475,
	MSG_MORE_PLAYERS = 476,
	MSG_1_PLAYER = 477,
	MSG_YOU_MUST_WAIT = 478,
	MSG_SINGLE_WITHDRAW = 479,
	MSG_MANY_WITHDRAW = 480,
	MSG_WINGMAN_ABORT = 481,
	MSG_ABORT_SHIELDS_50 = 482,
	MSG_ABORT_SHIELDS_25 = 483,
	MSG_ABORT_SHIELDS_0 = 484,
	MSG_ABORT_HULL_75 = 485,
	MSG_ABORT_HULL_50 = 486,
	MSG_ABORT_HULL_25 = 487,
	MSG_ABORT_WARHEADS_OUT = 488,
	MSG_ABORT_UNDER_ATTACK = 489,
	MSG_ABORT_BEEN_IDED = 490,
	MSG_NODAMAGEDSYSTEMS = 491,
	MSG_KILLED_BY = 492,
	MSG_KILLED_BY_MOST_DAMAGE = 493,
	MSG_SYSTEMSGS_DISABLED = 494,
	MSG_SYSTEMSGS_ENABLED = 495,
	MSG_MSGLOGGING_DISABLED = 496,
	MSG_MSGLOGGING_ENABLED = 497,
	MSG_VOICE_CHAT_MY_TEAM_CHANNEL = 498,
	MSG_VOICE_CHAT_MY_TARGET_CHANNEL = 499,
	MSG_VOICE_CHAT_ALL_PLAYERS_CHANNEL = 500,
	MSG_VOICE_CHAT_ENEMY_TEAM_CHANNEL = 501,
	MSG_VOICE_CHAT_PLAYER_CHANNEL = 502,
	MSG_VOICE_CHAT_CHANGING_CHANNEL = 503,
	MSG_VOICE_CHAT_RECORDING_STARTED = 504,
	MSG_VOICE_CHAT_RECORDING_STOPPED = 505,
	MSG_VOICE_CHAT_BUFFER_FILLED = 506,
	MSG_VOICE_CHAT_TARGET_IS_NOT_A_PLAYER = 507,
	MSG_VOICE_CHAT_ONLY_AVAILABLE_IN_MULTIPLAYER = 508,
	MSG_VOICE_CHAT_UNABLE_TO_RECORD = 509,
	MSG_VOICE_CHAT_CHANNEL_BUSY = 510,
	MSG_VOICE_CHAT_CHANNEL_AVAILABLE = 511,
	MSG_YARD_COUNTDOWN_GETREADY = 512,
	MSG_YARD_COUNTDOWN_5 = 513,
	MSG_YARD_COUNTDOWN_4 = 514,
	MSG_YARD_COUNTDOWN_3 = 515,
	MSG_YARD_COUNTDOWN_2 = 516,
	MSG_YARD_COUNTDOWN_1 = 517,
	MSG_YARD_COUNTDOWN_GO = 518,
	MSG_GUNNER = 519,
	MSG_PILOTING = 520,
	MSG_MOVE_TO_GUNNER = 521,
	MSG_MOVE_TO_PILOTING = 522,
	MSG_GUNNER_DEFENSIVE = 523,
	MSG_GUNNER_AUTOFIRE = 524,
	MSG_GUNNER_LINKED = 525,
	MSG_PILOT_TARGET_FOLLOW_ON = 526,
	MSG_PILOT_TARGET_FOLLOW_OFF = 527,
	MSG_HANGAR_RESTART = 528,
	MSG_MISSION_LOST = 529,
	MSG_GUNNER_BLOCKED = 530,
	MSG_GUNNER_DISABLED = 531,
	MSG_REACHED_PLANET = 532,
	MSG_MISSION_WILL_BE_LOST = 533,
	MSG_ALREADY_TARGETED = 534,
	MSG_UNKNOWN_CRAFT = 535,
	MSG_NOW_DISABLED = 536,
	MSG_TOTAL_MESSAGES = 537,
} InFlightMessageId;

extern HudInFlightMessageRecord g_systemMessagePane;
extern HudInFlightMessageRecord g_flightGroupMessagePane;
extern HudInFlightMessageRecord g_readyMessagePaneQueue[11];
extern uint16_t                 g_messageLogWriteIndex;
extern uint16_t                 g_messageLogTotalCount;
extern uint16_t                 g_messageLogWrapped;
extern int                      g_messageLogFileWriteRequested;
extern int                      g_flightMessagePaneLayoutInitSentinel;
extern int                      g_flightMessagePanesForceExpire;
extern int                      g_unusedFlightMessagePaneLayoutScalar0;
extern int                      g_unusedFlightMessagePaneLayoutScalar1;
extern int                      g_unusedFlightMessagePaneLayoutScalar2;
extern int                      g_unusedFlightMessagePaneLayoutScalar3;
extern int                      g_unusedFlightMessagePaneLayoutScalar4;
extern int                      g_unusedFlightMessagePaneLayoutScalar5;
extern int                      g_unusedFlightMessagePaneLayoutScalar6;
extern int                      g_unusedFlightMessagePaneLayoutScalar7;
extern int                      g_unusedFlightMessagePaneLayoutScalar8;
extern int                      g_unusedFlightMessagePaneLayout505;
extern int                      g_unusedFlightMessagePaneLayout150;
extern uint16_t                 g_unusedReadyMessagePaneInitialState;
extern int                      g_targetDescriptionMessageId;
extern HudInFlightMessageRecord* g_messageLogRecords;
extern MemoryHandle             g_messageLogHandle;
extern uint8_t                  g_readyMessageQueueCount;
extern uint16_t                 g_pendingHudMessageVoiceSfxId;
extern uint16_t                 g_msgSenderIff;
extern const uint8_t            g_messageTextPrefixColorCodes[24];
extern const uint8_t            g_messageSenderIffColorCodes[8];
extern PlayerFlightTransientTimers g_playerFlightTransientTimers[8];
extern uint16_t                 g_msgArgTable[4];
extern const char*              g_msgPtrs[4];
extern char                     g_flightTextScratchBuffer[256];
extern char                     outName[256];
extern const uint8_t            g_targetDescDesignationUsesRelationText[24];
extern int                      g_flightSystemMessagesEnabled;
extern uint16_t                 g_replayViewMode;
extern char                     g_mfdCommandSecondaryTargetLabels[10][30];
extern char                     g_mfdCommandPrimaryTargetLabels[7][30];
extern int                      g_mfdCommandNodeSwitchColorChar;
extern const int                g_mfdCommandSubMenuItemCount[6];
extern const int                g_mfdCommandSubMenuFirstItemIndex[6];
extern HudElementEnabled   g_hudElementEnabled[12];
extern HudDrawTarget       g_defaultHudDrawTarget;
extern HudDrawTarget*      g_drawTarget;
extern uint16_t*           g_curImageBlendLut;
extern uint16_t*           g_curImagePalette;
extern uint8_t*            g_curImageRLE;
extern uint16_t            g_curImageWidth;
extern uint16_t            g_curImageHeight;
extern int                 g_curImageRunRemaining;
extern struct Sprite*      g_curImage;
extern uint8_t             g_hudUseAlphaSpriteAtlas10100;
extern int                 g_hudAlphaSpriteGroupOffset;
extern uint16_t            g_shieldSilhouetteSpriteIdByObjectType[72];
extern int32_t             g_reticleLaserHardpointCount;
extern int                 g_reticleLaserHardpointIndices[16];
extern int                 g_reticleWarheadHardpointCount;
extern int                 g_reticleWarheadHardpointIndices[16];
extern HudPoint            g_reticleLaserAimPoints[16];
extern int                 g_reticleCenterX;
extern int                 g_reticleCenterY;
extern uint8_t             g_reticleDirty;
extern int                 g_reticleDrawX;
extern int                 g_reticleDrawY;
extern uint8_t             g_hudLaserChargeDisplayDrawn;
extern int                 g_reticleAimPointDistanceBias;
extern FlightTexQuad       g_hudLaserChargeQuads[16];
extern FlightTexQuad       g_hudIonChargeQuads[16];
extern int                 g_hudEnergyBankLaserSelector;
extern int                 g_hudEnergyBankIonSelector;
extern uint16_t            g_hudEnergyChargeTripleSegmentStepX;
extern uint16_t            g_hudEnergyChargeTripleInitialBackstepX;
extern uint16_t            g_hudEnergyChargeNonTripleSegmentStepX;
extern uint16_t            g_hudEnergyChargeNonTripleInitialBackstepX;
extern int                 g_hudEnergyBarHighChargeSpriteOffset;
extern int                 g_hudEnergyBarLowChargeSpriteOffset;
extern int                 g_hudEnergyBarTripleBankSpriteOffset;
extern uint32_t            g_hudEnergyBarMaxSegments;
extern uint16_t            g_hudBeamChargeUnitsPerSegment;
extern uint16_t            g_hudBeamChargeColorBandUnits;
extern uint16_t            g_hudBeamChargeSegmentCount;
extern int                 g_hudWeaponModeWarhead;
extern const uint32_t      g_hudSubsystemLaserLabelSurfaceWidth;
extern const uint32_t      g_hudSubsystemLaserLabelSurfaceHeight;
extern const uint32_t      g_hudSubsystemShieldLabelSurfaceWidth;
extern const uint32_t      g_hudSubsystemShieldLabelSurfaceHeight;
extern const uint32_t      g_hudSubsystemEngineLabelSurfaceWidth;
extern const uint32_t      g_hudSubsystemEngineLabelSurfaceHeight;
extern const uint32_t      g_hudSubsystemBeamLabelSurfaceWidth;
extern const uint32_t      g_hudSubsystemBeamLabelSurfaceHeight;
extern uint8_t             g_hudShieldPercentLabelsInitialized;
extern uint16_t            g_hudShieldFrontPercentCached;
extern uint16_t            g_hudShieldRearPercentCached;
extern uint8_t             g_lastShieldDamageSide;
extern const uint32_t      g_hudShieldBarArgbBySegmentCount[11];
extern const uint32_t      argbColor[4];
extern uint8_t             g_systemMessagePaneVisible;
extern uint8_t             g_flightGroupMessagePaneVisible;
extern uint8_t             g_readyMessagePaneVisible;
extern const HudThreatIndicatorSlot g_hudThreatIndicatorSlotCenter;
extern const HudThreatIndicatorSlot g_hudThreatIndicatorSlotLeft;
extern const HudThreatIndicatorSlot g_hudThreatIndicatorSlotRight;
extern const HudThreatIndicatorSlot g_hudThreatIndicatorSlotBottom;
extern uint8_t             g_incomingMissileWarningFlashActive;
extern int                 g_panelLastLaserThreatVoiceTime;
extern int                 g_panelLastBeamThreatVoiceTime;
extern int                 g_incomingMissileWarningFlashFrame;
extern void*               g_hudSystemMessagePaneSurface;
extern void*               g_hudFlightGroupMessagePaneSurface;
extern void*               g_hudReadyMessagePaneSurface;
extern int                 g_hudSystemMessagePaneSurfaceWidth;
extern int                 g_hudSystemMessagePaneSurfaceHeight;
extern int                 g_hudFlightGroupMessagePaneSurfaceWidth;
extern int                 g_hudFlightGroupMessagePaneSurfaceHeight;
extern int                 g_hudReadyMessagePaneSurfaceWidth;
extern int                 g_hudReadyMessagePaneSurfaceHeight;
extern int16_t             g_hudSystemMessagePaneX;
extern int16_t             g_hudSystemMessagePaneY;
extern int16_t             g_hudFlightGroupMessagePaneX;
extern int16_t             g_hudFlightGroupMessagePaneY;
extern int16_t             g_hudReadyMessagePaneX;
extern int16_t             g_hudReadyMessagePaneY;
extern uint32_t            g_hudColors[7];
extern int                 g_hudCenterX;
extern int                 g_hudCenterY;
extern int                 g_hudCmdFrameY;
extern uint16_t            g_hudRadarScopeOffsetX;
extern uint16_t            g_hudRadarScopeOffsetY;
extern uint16_t            g_hudRadarFrameLeftOffsetX;
extern uint16_t            g_hudRadarFrameRightOffsetX;
extern uint16_t            g_hudRadarFrameOffsetY;
extern int                 g_hudLaserThreatSlot0OffsetX;
extern int                 g_hudLaserThreatSlot0OffsetY;
extern int                 g_hudWarheadThreatSlot0OffsetX;
extern int                 g_hudWarheadThreatSlot0OffsetY;
extern int                 g_hudLaserThreatSlot1OffsetX;
extern int                 g_hudLaserThreatSlot1OffsetY;
extern int                 g_hudWarheadThreatSlot1OffsetX;
extern int                 g_hudWarheadThreatSlot1OffsetY;
extern int                 g_hudLaserThreatSlot2OffsetX;
extern int                 g_hudLaserThreatSlot2OffsetY;
extern int                 g_hudWarheadThreatSlot2OffsetX;
extern int                 g_hudWarheadThreatSlot2OffsetY;
extern int                 g_hudLaserThreatSlot3OffsetX;
extern int                 g_hudLaserThreatSlot3OffsetY;
extern int                 g_hudWarheadThreatSlot3OffsetX;
extern int                 g_hudWarheadThreatSlot3OffsetY;
extern const uint32_t      g_hudWarheadCountTextSurfaceWidth;
extern const uint32_t      g_hudWarheadCountTextSurfaceHeight;
extern const uint32_t      g_hudSpeedTextSurfaceWidth;
extern const uint32_t      g_hudSpeedTextSurfaceHeight;
extern const uint32_t      g_hudThrottleTextSurfaceWidth;
extern const uint32_t      g_hudThrottleTextSurfaceHeight;
extern const uint32_t      g_hudCraftNameTextSurfaceWidth;
extern const uint32_t      g_hudCraftNameTextSurfaceHeight;
extern const uint32_t      g_hudMissionClockTextSurfaceWidth;
extern const uint32_t      g_hudMissionClockTextSurfaceHeight;
extern const uint32_t      g_hudCountermeasureCountTextSurfaceWidth;
extern const uint32_t      g_hudCountermeasureCountTextSurfaceHeight;
extern const uint32_t      g_hudProvingGroundStatusTextSurfaceWidth;
extern const uint32_t      g_hudProvingGroundStatusTextSurfaceHeight;
extern uint16_t            g_hudSpeedTextY;
extern uint16_t            g_hudSpeedTextX;
extern uint16_t            g_hudCraftNameTextY;
extern uint16_t            g_hudCraftNameTextX;
extern uint16_t            g_hudThrottleTextY;
extern uint16_t            g_hudThrottleTextX;
extern uint16_t            g_hudMissionClockTextY;
extern uint16_t            g_hudMissionClockTextX;
extern uint16_t            g_hudWarheadCountTextY;
extern uint16_t            g_hudWarheadCountTextX;
extern uint16_t            g_hudDualWarheadCountTextX;
extern uint16_t            g_hudCountermeasureCountTextY;
extern uint16_t            g_hudCountermeasureCountTextX;
extern uint16_t            g_hudProvingGroundStatusTextX;
extern uint16_t            g_hudProvingGroundStatusTextY;
extern const uint32_t      g_hudReticleWarheadCountSurfaceWidth;
extern const uint32_t      g_hudReticleWarheadCountSurfaceHeight;
extern int                 g_hudWarheadCountLeftReticleOffsetX;
extern int                 g_hudWarheadCountRightReticleOffsetX;
extern int                 g_hudWarheadCountReticleOffsetY;
extern const uint32_t      g_hudShieldPercentTextSurfaceWidth;
extern const uint32_t      g_hudShieldPercentTextSurfaceHeight;
extern uint16_t            g_hudFrontShieldPercentTextX;
extern uint16_t            g_hudFrontShieldPercentTextY;
extern uint16_t            g_hudRearShieldPercentTextX;
extern uint16_t            g_hudRearShieldPercentTextY;
extern uint16_t            g_hudSubsystemLabelLaserX;
extern uint16_t            g_hudSubsystemLabelLaserY;
extern uint16_t            g_hudSubsystemLabelShieldX;
extern uint16_t            g_hudSubsystemLabelShieldY;
extern uint16_t            g_hudSubsystemLabelEngineY;
extern uint16_t            g_hudSubsystemLabelEngineX;
extern uint16_t            g_hudSubsystemLabelBeamX;
extern uint16_t            g_hudSubsystemLabelBeamY;
extern unsigned int        g_hudCmdPanelOriginX;
extern unsigned int        g_hudCmdPanelOriginY;
extern unsigned int        g_hudCmdTargetNameTextY;
extern unsigned int        g_hudCmdOrderLineY;
extern unsigned int        g_hudCmdShieldLabelX;
extern unsigned int        g_hudCmdShieldLabelY;
extern unsigned int        g_hudCmdShieldPercentX;
extern unsigned int        g_hudCmdShieldPercentY;
extern unsigned int        g_hudCmdHullLabelX;
extern unsigned int        g_hudCmdHullLabelY;
extern unsigned int        g_hudCmdHullPercentX;
extern unsigned int        g_hudCmdHullPercentY;
extern unsigned int        g_hudCmdSystemLabelX;
extern unsigned int        g_hudCmdSystemLabelY;
extern unsigned int        g_hudCmdSystemPercentX;
extern unsigned int        g_hudCmdSystemPercentY;
extern unsigned int        g_hudCmdDistanceLabelX;
extern unsigned int        g_hudCmdDistanceLabelY;
extern unsigned int        g_hudCmdDistanceValueX;
extern unsigned int        g_hudCmdDistanceValueY;
extern unsigned int        g_hudCmdTargetStatusX;
extern unsigned int        g_hudCmdTargetStatusY;
extern unsigned int        g_hudCmdComponentLineY;
extern uint16_t            g_hudCmdPanelWidth;
extern uint16_t            g_hudCmdPanelHeight;
extern void*               g_hudCmdTexPixels;
extern char                g_hudTargetNameText[30];
extern char                g_hudTargetStatusText[30];
extern unsigned int        g_hudTargetShieldDisplayPct;
extern unsigned int        g_hudTargetSystemDisplayPct;
extern unsigned int        g_hudTargetHullDisplayPct;
extern unsigned int        g_hudTargetDistanceWhole;
extern unsigned int        g_hudTargetDistanceFrac;
extern const int           g_hudOffscreenTargetTextMargin;
extern const unsigned int  g_hudFilmRecordingIndicatorSurfaceWidth;
extern const unsigned int  g_hudFilmRecordingIndicatorSurfaceHeight;
extern void*               g_hudFilmRecordingIndicatorSurface;
extern uint16_t            g_hudFilmRecTextX;
extern uint16_t            g_hudFilmRecTextY;
extern int                 g_hudMfdSurfaceWidth;
extern int                 g_hudMfdSurfaceHeight;
extern int                 g_hudMfdSurfaceY;
extern int                 g_hudMfdTextInsetX;
extern int                 g_hudMfdFrameSideOffsetX;
extern int                 g_hudMfdFrameY;
extern int                 g_hudLaserChargeSingleY;
extern int                 g_hudLaserChargePairLeftOffsetX;
extern int                 g_hudLaserChargePairRightOffsetX;
extern int                 g_hudLaserChargePairY;
extern int                 g_hudLaserChargeTripleLeftOffsetX;
extern int                 g_hudLaserChargeTripleRightOuterOffsetX;
extern int                 g_hudLaserChargeTripleRightInnerOffsetX;
extern int                 g_hudLaserChargeTripleY;
extern int                 g_hudLaserChargeQuadLeftOffsetX;
extern int                 g_hudLaserChargeQuadRightOffsetX;
extern int                 g_hudLaserChargeQuadUpperY;
extern int                 g_hudLaserChargeQuadLowerY;
extern int                 g_hudLaserChargeFiveLeftOffsetX;
extern int                 g_hudLaserChargeFiveReserved0;
extern int                 g_hudLaserChargeFiveRightOffsetX;
extern int                 g_hudLaserChargeFiveUpperY;
extern int                 g_hudLaserChargeFiveLowerY;
extern int                 g_hudLaserChargeFiveSixReserved0;
extern int                 g_hudLaserChargeFiveSixReserved1;
extern int                 g_hudLaserChargeSixLeftOffsetX;
extern int                 g_hudLaserChargeSixRightOuterOffsetX;
extern int                 g_hudLaserChargeSixRightInnerOffsetX;
extern int                 g_hudLaserChargeSixUpperY;
extern int                 g_hudLaserChargeSixLowerY;
extern int                 g_hudIonChargeYOffsetFromLaser;
extern int                 g_hudIonChargeSingleY;
extern int                 g_hudIonChargePairLeftOffsetX;
extern int                 g_hudIonChargePairRightOffsetX;
extern int                 g_hudIonChargePairY;
extern int                 g_hudIonChargeTripleLeftOffsetX;
extern int                 g_hudIonChargeTripleRightInnerOffsetX;
extern int                 g_hudIonChargeTripleRightOuterOffsetX;
extern int                 g_hudIonChargeTripleY;
extern int                 g_hudIonChargeQuadLeftOffsetX;
extern int                 g_hudIonChargeQuadRightOffsetX;
extern int                 g_hudIonChargeQuadUpperY;
extern int                 g_hudIonChargeQuadLowerY;
extern int                 g_hudBeamGaugeRightOffsetX;
extern int                 g_hudBeamGaugeBottomOffsetY;
extern int                 g_hudBeamIconRightOffsetX;
extern int                 g_hudBeamIconBottomOffsetY;
extern int                 g_hudBeamChargeRightOffsetX;
extern int                 g_hudBeamChargeBottomOffsetY;
extern int                 g_hudBeamChargeSegmentOffsetY0;
extern int                 g_hudBeamChargeSegmentOffsetY1;
extern int                 g_hudBeamChargeSegmentOffsetY2;
extern int                 g_hudBeamChargeSegmentOffsetY3;
extern int                 g_hudBeamChargeSegmentOffsetY4;
extern int                 g_hudBeamChargeSegmentOffsetY5;
extern int                 g_hudBeamChargeSegmentOffsetY6;
extern int                 g_hudBeamChargeSegmentOffsetY7;
extern int                 g_hudShieldGaugeSideOffsetX;
extern int                 g_hudShieldGaugeBottomOffsetY;
extern uint16_t            g_hudShieldLayoutInitOnlyY;
extern int                 g_hudShieldHullIconSideOffsetX;
extern int                 g_hudShieldHullIconBottomOffsetY;
extern int                 g_hudShieldBarSideOffsetX;
extern int                 g_hudShieldFrontUpperBarY;
extern int                 g_hudShieldFrontLowerBarY;
extern int                 g_hudShieldRearUpperBarY;
extern int                 g_hudShieldRearLowerBarY;
extern uint16_t            g_hudMfdPaneWidth;
extern uint16_t            g_hudMfdPaneHeight;
extern uint16_t            g_mfdCommandMenuNumberX;
extern uint16_t            g_mfdCommandMenuTextX;
extern uint16_t            g_mfdCommandMenuLineExtraSpacingY;
extern void*               g_hudMfdLeftTexPixels;
extern void*               g_hudMfdRightTexPixels;
extern void*               g_hudMfdTitleTexPixels;
extern void*               hudTex4;
extern void*               hudTex5;
extern void*               hudTex6;
extern void*               hudTex7;
extern void*               hudTex8;
extern void*               hudTex9;
extern void*               hudTex10;
extern void*               hudTex11;
extern void*               hudTex12;
extern void*               hudTex13;
extern void*               hudTex15;
extern void*               hudTex16;
extern void*               hudTex17;
extern void*               hudTex18;
extern void*               hudTex19;
extern void*               g_hudFpsCountPixels;
extern int                 g_flightFpsOverlayMode;
extern uint8_t             g_filmOverlayMfdVisible;
extern uint8_t             g_mfdLeftNeedsRedraw;
extern uint8_t             g_mfdRightNeedsRedraw;
extern const char          g_mfdCraftListStatusLetters[11];
extern int16_t             g_mfdFriendlyCraftSelectedRowCache;
extern uint16_t            g_mfdFriendlyCraftLayoutInitOnlyX;
extern uint16_t            g_mfdFriendlyCraftShieldHullHeaderX;
extern uint16_t            g_mfdFriendlyCraftNameColumnWidth;
extern uint16_t            g_mfdFriendlyCraftTargetColumnWidth;
extern int16_t             g_mfdFlightGroupsSelectedRowCache;
extern uint16_t            g_mfdFlightGroupsNameColumnWidth;
extern uint16_t            g_mfdFlightGroupsShieldHullHeaderX;
extern uint16_t            g_mfdFlightGroupsShieldHullValueX;
extern uint16_t            g_mfdFlightGroupsTargetColumnX;
extern uint16_t            g_mfdFlightGroupsTargetColumnWidth;
extern uint16_t            g_mfdFlightGroupsStatusColumnX;
extern uint16_t            g_mfdFlightGroupsStatusColumnWidth;
extern int                 g_mfdMessageLogLeftScrollOffset;
extern int                 g_mfdMessageLogRightScrollOffset;
extern int                 g_mfdMessageLogLastDrawTotalCount;
extern uint8_t             g_mfdMessageLogSharedPageLeftDrawn;
extern int                 g_mfdMessageLogLeftWrappedLineCount;
extern int                 g_mfdMessageLogRightWrappedLineCount;
extern uint16_t            g_messageLogDrawTotalCount;
extern uint8_t*            g_cockpitMaskBitmap;
extern uint8_t*            g_cockpitMaskRle;
extern uint8_t             g_sceneBypassCockpitMask;
extern HudCockpitMaskSprite g_hudCockpitMaskSprites[6];
extern uint8_t             g_radarEllipseClampTable[74];
extern int                 g_radarEllipseClampRadius;
extern RadarBlip           g_radarForeBlipBufferA[48];
extern RadarBlip           g_radarForeBlipBufferB[48];
extern RadarBlip           g_radarAftBlipBufferA[48];
extern RadarBlip           g_radarAftBlipBufferB[48];
extern RadarBlipCount      g_radarForeBlipCount;
extern RadarBlipCount      g_radarAftBlipCount;
extern uint16_t            g_radarForePrevBlipCount;
extern uint16_t            g_radarAftPrevBlipCount;
extern RadarBlip*          g_radarForeEraseBlips;
extern RadarBlip*          g_radarAftEraseBlips;
extern RadarBlip*          g_radarForeDrawBlips;
extern RadarBlip*          g_radarAftDrawBlips;
extern int                 g_hudRadarCenterOffsetX;
extern int                 g_hudRadarCenterY;
extern uint8_t             g_radarBlipBufferParity;
extern uint8_t             g_radarTargetMarkerBackgroundSaved;
extern int16_t             radarx;
extern int16_t             radary;
extern uint16_t            g_radarTargetMarkerDrawX;
extern uint16_t            g_radarTargetMarkerDrawY;
extern int16_t             g_radarTargetMarkerRestoreX;
extern int16_t             g_radarTargetMarkerRestoreY;

void     Hud_RenderHud(void);
int      Hud_UpdateHUD(void);
void     Hud_InitHUD(void);
void     Hud_FreeHUDResources(void);
void     Hud_ResetHudRuntimeState(void);
uint16_t Hud_GetSystemMessagePaneState(void);
void     Hud_ClearReadyMessageQueue(void);
void     Hud_ClearSystemTextPane(void);
void     Hud_ClearFlightGroupTextPane(void);
void     Hud_ClearReadyMessageTextPane(void);
void     Hud_ClearFlightSurface(void);
void     Hud_RedrawSoftwareHudFrame(void);
int      Hud_ForceHudRefresh(int playerIdx, int mode);
void     Hud_SetupCraftEntryHudMasks(void);
void     Hud_SetFilmOverlayMfdVisible(char visible);
void     Hud_ToggleMfdOverlay(int playerIdx);
void     Hud_ToggleMfdSide(int playerIdx, int mfdIndex);
void     Hud_EnterPlayerMapView(int playerIdx);
void     Hud_RestorePlayerHudState(int playerIdx);
void     Hud_SyncSoftwareHudMasks(int playerIdx, char hudEnabled);
void Hud_SetHudEnabled(int playerIdx, char hudEnabled);
void     Hud_SyncLocalSoftwareHudMasks(char hudEnabled);
void     Hud_BlitSoftwareHudTextPanes(void);
void     Hud_ResetFlightMessagePanes(int forceExpireActiveMessages);
void     Hud_DrawFpsOverlay(void);
void     Hud_AdvanceFlightMessagePaneTimers(void);
void     Hud_UpdateFlightMessagePanes(void);
uint16_t Hud_MeasureFlightMessagePaneText(int16_t paneType);
void     Hud_SetFlightMessagePaneTimer(int16_t paneType);
void     Hud_ShowFlightMessagePane(int16_t paneType, char playVoice);
void     Hud_EndHudMessageLine(int unused, char lastChar);
void     Hud_AppendObjectDisplayName(uint16_t objectOrWaypointIdx, char formatFlags);
void     Hud_UpdateTargetInfoCache(void);
void     Hud_DrawHudMessageTextPane(void* textSurface, uint16_t width, uint16_t height, char* text,
									int16_t resourceTextId);
void     Hud_DrawSystemTextPane(char* text, int16_t resourceTextId);
void     Hud_DrawFlightGroupTextPane(char* text, int16_t resourceTextId);
void     Hud_DrawReadyMessageTextPane(char* text, int16_t resourceTextId);
void     Mfd_InitCommandMenuRuntimeState(void);
void     Mfd_ExecuteCommandMenuSelection(int playerIdx);
void     Mfd_SelectCommandMenuItem(int playerIdx);
char     Mfd_IsCommandMenuItemAvailable(uint16_t playerIdx, uint16_t menuRow, int16_t menuItem);
void     Mfd_UpdateCommandMenuTargets(void);
void     Mfd_BuildScratchCraftListName(uint16_t objectIdx, int viewerPlayerIdx, char includeIffColor);
void     Mfd_DrawCommandObjectOrderLine(unsigned int objectIdx, int unused, int16_t y);
void     Mfd_DrawCommandMenuPage(int mfdSide, void* mfdSurface);
void     Mfd_DrawConsolePage(int mfdSide, void* mfdSurface);
void     Mfd_DrawFriendlyCraftPage(int mfdSide, void* mfdSurface);
void     Mfd_DrawFlightGroupsPage(int mfdSide, void* mfdSurface);
void     Mfd_DrawMissionGoalsPage(int mfdSide, void* mfdSurface);
void     Mfd_DrawMapLegendPage(int mfdSide, void* mfdSurface);
void     Mfd_DrawMissionScoreboardPage(int mfdSide, void* mfdSurface);
void     Mfd_DrawRaceScoreboardPage(int mfdSide, void* mfdSurface);
void     Mfd_DrawMessageLogPage(int mfdSide, void* mfdSurface);
void     Mfd_DrawFilmLeftStatusPage(void);
void     Mfd_DrawFilmRightOptionsPage(void);
int16_t  goals_DrawConditionText(uint16_t craftType, uint16_t condition, uint16_t amountTextVariant,
								  int16_t conditionRowBase);
int16_t  goals_outputgoal(uint16_t targetId, uint16_t condition, uint16_t targetType,
						   uint16_t goalStatus, uint16_t amountOp,
						   uint16_t timeLimit5SecUnits, const char* conditionTextOverride,
						   int percentComplete, int goalTitleIndex);
int      msg_addMessagePtr(uint16_t slot, const char* text);
void     msg_emitInFlightMessage(int messageId, int playerIdx);
void     msg_emitLocalPlayerCraftMessage(uint16_t messageId);
int      msg_radioMessage(uint16_t objIdx, CraftData* craft, int16_t hudMessageId,
						  uint16_t voiceVariant, int16_t broadcast);
void     msg_reportfgcreation(uint16_t flightGroupIdx, uint16_t speciesIdx);
void     msg_emitCraftMessage(uint16_t objIdx, CraftData* craft, int16_t msgTemplateId);
void msg_reportmessage(uint16_t objIdx, CraftData* craft, int16_t msgTemplateId);
void     msg_formatObjectName(uint16_t objIdx, uint16_t nameMode, char* outName);
int      msg_BuildTargetDescription(ObjectIndex targetObjIdx, int playerIdx, int emitHudMessage,
									 int returnActionableOnly);
void     msg_writeMessageLogFile(void);
void     Hud_EnableHudDrawElements(void);
void     Hud_MarkFilmOverlayElementsVisible(void);
void     Hud_DrawMfdFrames3D(void);
void     Hud_DrawFilmMfdFrames3D(void);
void     Hud_DrawHangarFilmMfdOverlay(void);
void     Hud_DrawHudTargetInsetIfEnabled(int playerIdx);
void     Hud_Update3DCrt(uint16_t x, uint16_t y, uint16_t width, uint16_t height, int unused);
void     Hud_UpdateMfdPages(void);
void     Hud_UpdateWarheadCnt(void);
void     Hud_OutputWarheadCount(uint16_t warheadSlotIdx, int16_t displaySlot, int16_t warheadBank);
void Hud_SetDrawTargetSurface(void);
void     Hud_BlitSpriteType7(int16_t x, int y);
uint8_t* Hud_BlitSpriteType23(int16_t x, int y);
void     Hud_SetupResourceData(int group, uint16_t index);
void     Hud_DrawImageToDIB(int16_t x, int16_t y);
void     Hud_UpdateHUDMask(int hudMaskIndex, int enabled);
CraftData* Hud_GetCraftPointer(void);
void     Hud_SetupReticle(void);
void     Hud_DetermineLockStatus(void);
void     Hud_UpdateHUDText(void);
void     Hud_DrawOffscreenTargetIndicator2D(void);
void     Hud_DrawReticle2D(void);
void     Hud_DrawReticle3D(void);
void     Hud_DrawTargetArrow3D(void);
void     Hud_DrawCMD2D(void);
void     Hud_DrawMfdFrames2D(void);
void     Hud_DrawFilmMfdFrames2D(void);
void     Hud_DrawCMD3D(void);
void     Hud_DrawRadars2D(void);
void     Hud_DrawRadars3D(void);
void     Hud_DrawRadarFrames3D(void);
void     Hud_DrawThreatIndicator2D(HudThreatIndicatorSlot indicatorSlot, int state);
void     Hud_DrawThreatIndicator3D(HudThreatIndicatorSlot indicatorSlot, int state);
void     Hud_UpdateThreatIndicators(void);
void     Hud_DrawPowerSettings2D(void);
void     Hud_DrawPowerSettings3D(void);
void     Hud_AddBlipToRadar(uint16_t targetObjIdx);
void     Hud_SetupLaserChargePositions3D(void);
void     Hud_DrawLaserCharge2D(void);
void     Hud_DrawLaserCharge3D(void);
void     Hud_DrawEnergyBar2D(int isIonBank, int bankWeaponCount);
void     Hud_DrawEnergyBar3D(int isIonBank, int bankWeaponCount);
void     Hud_DrawBeamStrength2D(void);
void     Hud_DrawBeamStrength3D(void);
void     Hud_DrawShieldStrength2D(void);
void     Hud_DrawShieldStrength3D(void);
void     Hud_UpdateShieldPercentLabels(void);
void     Hud_DrawFilmRecordingIndicator(void);
void     Hud_DrawFilmOverlayMfdTitles(void);
void     Hud_DrawNetworkStatusIndicators(void);
void     Hud_DrawBoxInXTrans(int x, int y, int width, int height, int colorIdx, int depth);
int16_t  Hud_DrawBoxOverlayHW(int x, int y, int width, int height, int colorIdx, int depth);
void     Hud_DrawTargetBoxReadout(int boxX, int boxY, int boxWidth, int boxHeight);
void     Hud_UpdateCMDText(void);
int      Hud_PointCamera(uint16_t targetObjIdx, int16_t fitDistanceFlag, int playerIdx);
void     Hud_DrawCmdTargetOrderLine(void);
void     Hud_DrawCmdSystemPercent(void);
void     Hud_DrawCmdTargetComponentLine(void);
void     Hud_DrawCmdTargetStatusText(void);
uint16_t Hud_GetPlayerShieldSilhouetteSpriteId(void);
uint8_t* Hud_DrawTarget_GetPixelPtr(int x, int y);
int      Hud_MissionFG_GetCraftNumberIfShown(int flightGroupIdx, const CraftData* craft);
void     Hud_SetMfdPage(int playerIdx, int mfdIndex, uint8_t page);
void     Hud_CycleActiveMfdPage(uint16_t playerIdx, int direction);
void     Hud_SetHudViewState(int hudViewState, int playerIdx);
char     Hud_IsMfdPageAvailable(uint16_t playerIdx, uint8_t page);
void     Hud_CompressMask(void);

#ifdef __cplusplus
}
#endif

#endif
