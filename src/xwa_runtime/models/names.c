#include "xwa_runtime/models/names.h"

#include "aeron/config_file.h"
#include "aeron/log.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* Object type names, indexed by object type. They follow the port's enum
 * names:
 *   - when the enum name has the slot number, the name keeps it, such as
 *     xwing_103;
 *   - free slots are slot_<n>;
 *   - image group types use the group number, such as blast_mark_group_3050;
 *   - weapon types 280 to 307 use the weapon names. */
static const char* const s_objectTypes[] = {
	"none",
	"xwing",
	"ywing",
	"awing",
	"bwing",
	"tie_fighter",
	"tie_interceptor",
	"tie_bomber",
	"tie_advanced",
	"tie_defender",
	"ird_fighter",
	"toscan_fighter",
	"missile_boat",
	"twing",
	"z95",
	"r41",
	"assault_gunboat",
	"skipray_blast_boat",
	"tie_bizarro",
	"tie_big_gun",
	"tie_warheads",
	"tie_bomb",
	"tie_booster",
	"cloakshape_fighter",
	"razor_fighter",
	"planetary_fighter",
	"supa_fighter",
	"piggyback",
	"booster",
	"preybird_fighter",
	"xwing_30",
	"slave_one",
	"slave_two",
	"slot_33",
	"slot_34",
	"slot_35",
	"slot_36",
	"slot_37",
	"slot_38",
	"slot_39",
	"slot_40",
	"slot_41",
	"slot_42",
	"tug",
	"combat_utility_vehicle",
	"heavy_lifter",
	"mole_miner",
	"escape_pod",
	"escape_pod_a",
	"slot_49",
	"shuttle",
	"escort_shuttle",
	"stormtrooper_transport",
	"assault_transport",
	"escort_transport",
	"system_patrol_craft",
	"scout_craft",
	"muurian_transport",
	"corellian_transport2",
	"millenium_falcon2",
	"outrider",
	"imperial_landing_craft",
	"assault_shuttle",
	"luxury_yacht",
	"ferryboat_liner",
	"family_transport",
	"slot_66",
	"slot_67",
	"slot_68",
	"slot_69",
	"slot_70",
	"container_brick",
	"container_hex_box",
	"container_tube",
	"container_pronged",
	"container_hemisphere",
	"container_slotted",
	"container_hourglass",
	"container_gem",
	"container_yshaped",
	"cargo_canister",
	"propane_tank",
	"container_grande",
	"container_box",
	"container_sphere",
	"container_hanger",
	"slot_86",
	"slot_87",
	"slot_88",
	"slot_89",
	"slot_90",
	"bulk_freighter",
	"cargo_ferry",
	"modular_conveyor",
	"container_transport",
	"tuna_boat2",
	"star_galleon",
	"mod_action_transport",
	"mobquet_transport",
	"xiytiar_transport",
	"freighter_con_b",
	"freighter_con_g",
	"freighter_box",
	"xwing_103",
	"xwing_104",
	"crew_cabin_front",
	"connector_rod",
	"engine_back",
	"xwing_108",
	"xwing_109",
	"engine_front",
	"crew_cabin_back",
	"xwing_112",
	"xwing_113",
	"xwing_114",
	"suprosa",
	"slot_116",
	"slot_117",
	"slot_118",
	"slot_119",
	"slot_120",
	"slot_121",
	"slot_122",
	"slot_123",
	"slot_124",
	"slot_125",
	"corvette2",
	"mod_corvette",
	"frigate2",
	"mod_frigate",
	"passenger_liner",
	"carrack_cruiser",
	"strike_cruiser",
	"escort_carrier",
	"dreadnaught2",
	"calamari_cruiser_new",
	"light_calamari_cruiser",
	"interdictor2",
	"victory_star_destroyer2",
	"imperial_star_destroyer2",
	"super_star_destroyer",
	"mod_strike_cruiser",
	"lancer_frigate",
	"bulk_cruiser",
	"assault_frigate",
	"corellian_gunship",
	"marauder_corvette",
	"imperial_research_ship",
	"calamari_winged",
	"victory_star_destroyer2_149",
	"imperial_star_destroyer2_150",
	"slot_151",
	"slot_152",
	"platform1",
	"platform2",
	"platform3",
	"platform4",
	"platform5",
	"platform6",
	"asteroid_base",
	"factory",
	"ship_yard",
	"repair_yard",
	"golan_one",
	"golan_two",
	"golan_three",
	"derilyn_platform",
	"sensor_array",
	"comm_relay",
	"space_colony1",
	"space_colony2",
	"space_colony3",
	"casino",
	"cargo_facility1",
	"cargo_facility2",
	"asteroid_mining_unit",
	"processing_plant",
	"rebel_platform",
	"imperial_research_center",
	"family_base",
	"family_repair_yard",
	"pirate_shipyard",
	"industrial_complex",
	"pirate_junkyard_base",
	"slot_184",
	"slot_185",
	"slot_186",
	"slot_187",
	"slot_188",
	"slot_189",
	"slot_190",
	"slot_191",
	"laser_bat",
	"ion_bat",
	"asteroid_laser_battery",
	"asteroid_warhead_launcher",
	"gun_platform",
	"gun_pad",
	"gun_warhead_pad",
	"homing_mine_a",
	"homing_mine_b",
	"proximity_mine_a",
	"proximity_mine_b",
	"rebel_pilot",
	"imperial_pilot",
	"civilian_pilot",
	"zero_g_stormtrooper",
	"zero_g_utility",
	"marko",
	"r2d2",
	"comm_sat1",
	"comm_sat2",
	"comm_sat3",
	"probe",
	"probe_capsule",
	"nav_buoy1",
	"nav_buoy2",
	"nav_buoy3",
	"hyper_buoy",
	"mine1",
	"mine2",
	"mine3",
	"slot_222",
	"asteroid_hr1",
	"asteroid_hr2",
	"asteroid_hr3",
	"asteroid_hr4",
	"asteroid_hr5",
	"asteroid_hr6",
	"slot_229",
	"slot_230",
	"slot_231",
	"slot_232",
	"debris_group_4000",
	"debris_group_4001",
	"debris_group_4002",
	"debris_group_4003",
	"lighting_effect_group_1000",
	"animation_group_3051",
	"hud_group_12000",
	"group_6000_sprite_100",
	"group_6000_sprite_200",
	"group_6000_sprite_300",
	"group_6000_sprite_400",
	"group_6000_sprite_500",
	"group_6000_sprite_600",
	"group_6000_sprite_700",
	"group_7000_sprite_100",
	"group_7000_sprite_200",
	"group_7000_sprite_300",
	"group_7000_sprite_400",
	"group_8000_sprite_100",
	"group_8000_sprite_200",
	"group_8000_sprite_300",
	"group_8000_sprite_400",
	"group_8000_sprite_500",
	"group_8000_sprite_600",
	"wrap_back_group_19000_sprite_100",
	"wrap_back_group_18000_sprite_100",
	"backdrop_group_9001_sprite_1100",
	"backdrop_group_9001_sprite_1100_260",
	"backdrop_group_9001_sprite_1100_261",
	"backdrop_group_9001_sprite_1100_262",
	"backdrop_group_9001_sprite_1100_263",
	"explosion_group_2000",
	"explosion_group_2001",
	"explosion_group_2002",
	"explosion_group_2003",
	"explosion_group_2004",
	"explosion_group_2005",
	"explosion_group_2006",
	"spark_group_3000",
	"spark_group_3001",
	"spark_group_3002",
	"spark_group_3003",
	"spark_group_3004",
	"animation_group_3005",
	"animation_group_3006_sprite_0",
	"animation_group_2007",
	"animation_group_2008",
	"rebel_laser",
	"rebel_laser_turbo",
	"imperial_laser",
	"imperial_laser_turbo",
	"ion_cannon",
	"ion_cannon_turbo",
	"torpedo",
	"missile",
	"turbolaser_red",
	"turbolaser_green",
	"turbolaser_ion",
	"advanced_torpedo",
	"advanced_missile",
	"space_bomb",
	"heavy_rocket",
	"mag_pulse",
	"ion_pulse",
	"turbolaser_red_light",
	"flare",
	"spark_projectile",
	"chaff",
	"turbolaser_red_medium",
	"turbolaser_red_heavy",
	"turbolaser_green_light",
	"turbolaser_green_medium",
	"turbolaser_green_heavy",
	"death_star_laser",
	"custom_weapon",
	"hangar",
	"hangar_crane",
	"hangar_crate",
	"hangar_droid",
	"hangar_droid2",
	"hangar_generator",
	"hangar_monitor",
	"hangar_work_stand",
	"hangar_roof_crane",
	"corellian_transport_gunner",
	"slot_318",
	"slot_319",
	"death_star_ii_group_17001",
	"death_star_ii_group_17002",
	"ds_container",
	"ds_reactor_core",
	"ds_reactor_cylinder",
	"ds_tripod_gun",
	"ds_focus_lens",
	"ds_tankwlights",
	"ds_tankwlights2",
	"ds_bent_tube_gray",
	"ds_tankw_pipe_red01",
	"ds_bent_tube_red",
	"ds_bent_tube_red_lites01",
	"ds_bent_tube_red_lites02",
	"ds_bent_tube_red_lites03",
	"ds_bent_tube_red_lites04",
	"ds_bent_tube_red_lites05",
	"ds_tankw_pipe_blue01",
	"ds_bent_tube_blue01",
	"ds_bent_tube_blue_lites01",
	"ds_bent_tube_blue_lites02",
	"ds_bent_tube_blue_lites03",
	"ds_bent_tube_blue_lites04",
	"ds_two_tank",
	"ds_two_tank2",
	"ds_girder",
	"ds_girder02",
	"ds_girder03",
	"ds_reactor_core_room",
	"ds_accel_chamber",
	"ds_focus_chamber",
	"ds_3rd_room",
	"ds_entrance",
	"ds_reactor_room_tube",
	"ds_accel_tube",
	"ds_ynot_tube",
	"ds_angle_tube",
	"ds_dead_end",
	"ds_tube_entry",
	"ds_tri_room_entrance",
	"ds_tri_room",
	"ds_tri_room_exit",
	"ds_big_tube_entrance",
	"ds_big_tube",
	"ds_big_tube_exit",
	"ds_square_tube01",
	"ds_basement",
	"ds_basement01",
	"ds_basement02",
	"ds_alpha_walls",
	"ds_alpha_walls01",
	"ds_alpha_walls02",
	"ds_3_layer_tube",
	"ds_box_in_box1",
	"ds_box_in_box2",
	"ds_box_in_box3",
	"chute_mouth",
	"chute_tunnel",
	"salvage_room",
	"compactor",
	"accel_ring",
	"accel_ring2",
	"smelting_room",
	"sr_tube_nobend",
	"sr_tube_up",
	"sr_tube_down",
	"sr_tube_lh",
	"sr_tube_rh",
	"centrifuge",
	"accel_ring3",
	"container_grande_pg",
	"asteroid01",
	"asteroid02",
	"asteroid03",
	"junk01",
	"junk02",
	"junk03",
	"junk04",
	"junk05",
	"junk06",
	"junk07",
	"junk08",
	"junk09",
	"junk10",
	"junk_block",
	"molten_block",
	"rubble01",
	"rubble02",
	"rubble03",
	"rubble04",
	"rubble05",
	"rubble06",
	"rubble07",
	"rubble08",
	"rubble09",
	"rubble10",
	"rubble11",
	"rubble12",
	"font_group_16000",
	"blast_mark_group_3050",
	"hud_group_13000_sprite_0",
	"wrap_back_group_18000",
	"wrap_back_group_19000",
	"wrap_back_group_19100",
	"wrap_back_group_19200",
	"wrap_back_group_19300",
	"wrap_back_group_19400",
	"planet_group_6010",
	"planet_group_6011",
	"planet_group_6012",
	"planet_group_6013",
	"planet_group_6014",
	"planet_group_6020",
	"planet_group_6021",
	"planet_group_6022",
	"planet_group_6023",
	"planet_group_6024",
	"planet_group_6025",
	"planet_group_6026",
	"planet_group_6027",
	"planet_group_6028",
	"planet_group_6029",
	"planet_group_6030",
	"planet_group_6031",
	"planet_group_6032",
	"planet_group_6033",
	"planet_group_6034",
	"planet_group_6040",
	"planet_group_6041",
	"planet_group_6042_sprite_0",
	"planet_group_6043",
	"group_6050_sprite_0",
	"planet_group_6060",
	"planet_group_6061",
	"planet_group_6062",
	"planet_group_6063",
	"planet_group_6064",
	"planet_group_6070",
	"planet_group_6071",
	"planet_group_6072",
	"planet_group_6073",
	"planet_group_6074",
	"planet_group_6075",
	"planet_group_6076",
	"planet_group_6077",
	"planet_group_6078",
	"planet_group_6079",
	"planet_group_6080",
	"planet_group_6081",
	"planet_group_6082",
	"planet_group_6083",
	"planet_group_6084",
	"planet_group_6090",
	"planet_group_6091",
	"planet_group_6092",
	"planet_group_6093",
	"planet_group_6094",
	"planet_group_6100",
	"planet_group_6101",
	"planet_group_6102",
	"planet_group_6103",
	"planet_group_6104",
	"planet_group_6110",
	"planet_group_6111",
	"planet_group_6112",
	"planet_group_6113",
	"planet_group_6114",
	"death_star_fire_group_6250_sprite_0",
	"death_star_fire_group_6251",
	"work_droid1",
	"work_droid2",
	"corvette_explode",
	"corvette_explode_492",
	"corvette_explode_493",
	"corvette_explode_494",
	"corvette_explode_495",
	"corvette_explode_496",
	"corvette_explode_497",
	"corvette_explode_498",
	"corvette_explode_499",
	"escape_pod_a_500",
	"nebula_group_7001_sprite_100",
	"nebula_group_7002_sprite_200",
	"nebula_group_7003_sprite_300",
	"nebula_group_7004_sprite_400",
	"nebula_group_7005_sprite_500",
	"nebula_group_7006_sprite_600",
	"nebula_group_7007_sprite_700",
	"nebula_group_7008_sprite_800",
	"nebula_group_7009_sprite_900",
	"nebula_group_7010_sprite_1000",
	"galaxy_group_8001_sprite_100",
	"galaxy_group_8002_sprite_200",
	"galaxy_group_8003_sprite_300",
	"galaxy_group_8004_sprite_400",
	"galaxy_group_8005_sprite_500",
	"galaxy_group_8006_sprite_600",
	"galaxy_group_8007_sprite_700",
	"galaxy_group_8008_sprite_800",
	"galaxy_group_8009_sprite_900",
	"galaxy_group_8010_sprite_1000",
	"backdrop_group_9001_sprite_1100_521",
	"backdrop_group_9002_sprite_1200",
	"backdrop_group_9003_sprite_1300",
	"backdrop_group_9004_sprite_1400",
	"backdrop_group_9005_sprite_1500",
	"backdrop_group_9006_sprite_1600",
	"backdrop_group_9007_sprite_1700",
	"backdrop_group_9008_sprite_1800",
	"backdrop_group_9009_sprite_1900",
	"backdrop_group_9010_sprite_2000",
	"backdrop_group_9100_sprite_5100",
	"nebula_group_7011_sprite_1100",
	"nebula_group_7012_sprite_1200",
	"nebula_group_7013_sprite_1300",
	"nebula_group_7014_sprite_1400",
	"nebula_group_7015_sprite_1500",
	"map_icon_group_14800",
	"trail_group_21000_sprite_0",
	"trail_group_21005",
	"trail_group_21010",
	"trail_group_21015",
	"trail_group_21020",
	"trail_group_21025",
	"particle_group_22000",
	"particle_group_22003",
	"particle_group_22005",
	"particle_group_22007",
	"animation_group_3055",
	"hull_icon_group_26000",
	"ds_laser_internal",
	"animation_group_3200",
	"animation_group_3100",
	"animation_group_3300",
	"animation_group_3400",
	"animation_group_3500",
	"ds_laser_power_source",
};
typedef char object_types_check[(sizeof s_objectTypes / sizeof s_objectTypes[0] == 557) ? 1 : -1];

/* The mod's new name for each object type, or NULL to keep the stock
   name. The names point into s_objectTypeRenameTexts. */
static const char* s_objectTypeRenames[557];
static char s_objectTypeRenameTexts[557][XWA_NAME_MAX];

const XwaNameTable g_xwaObjectTypeNames = { "object type", s_objectTypes, 0, 557, s_objectTypeRenames };
const XwaNameTable g_xwaWeaponTypeNames = { "weapon type", s_objectTypes + 280, 280, 28,
											s_objectTypeRenames + 280 };

static const char* const s_warheads[] = {
	"space_bomb",         "heavy_rocket",     "missile",   "torpedo",
	"advanced_missile",   "advanced_torpedo", "mag_pulse", "ion_pulse",
	"advanced_mag_pulse", "cluster_bomb",
};
typedef char warheads_check[(sizeof s_warheads / sizeof s_warheads[0] == 10) ? 1 : -1];
const XwaNameTable g_xwaWarheadNames = { "warhead", s_warheads, 1, 10, NULL };

/* Genus 6 has no name: the game uses its slot range for the players' shots,
   and no object type has it. Genus 16 has no name because its slot range is
   empty, so an object with it can't spawn. */
static const char* const s_genus[] = {
	"fighter",     "transport",          "utility",       "freighter",         "starship", "platform",
	NULL,          "projectile",         "mine",          "satellite_buoy",    "asteroid", "debris",
	"sprite",      "explosion",          "large_scenery", "death_star_tunnel", NULL,       "container",
	"pilot_droid", "weapon_emplacement", "rubble",        "salvage_junk",
};
typedef char genus_check[(sizeof s_genus / sizeof s_genus[0] == 22) ? 1 : -1];
const XwaNameTable g_xwaGenusNames = { "genus", s_genus, 0, 22, NULL };

static const char* const s_families[] = {
	"craft", "weapon", "object", "debris", "sprite", "explosion", "scenery",
};
typedef char families_check[(sizeof s_families / sizeof s_families[0] == 7) ? 1 : -1];
const XwaNameTable g_xwaFamilyNames = { "family", s_families, 0, 7, NULL };

/* Bits 0x10 and 0x40 have no name, because the game never reads them. */
static const char* const s_objectTypeFlags[] = {
	"player_targetable",
	"ai_targetable",
	"nav_buoy",
	"hardware_only",
	NULL,
	"single_mip",
	NULL,
	"static_object",
	"animated_sprite",
	"animation_loops",
	"color_key_border",
	"float_hardpoints",
	"spins",
	"collision_face_groups",
	"color_key",
	"alpha",
};
typedef char object_type_flags_check[(sizeof s_objectTypeFlags / sizeof s_objectTypeFlags[0] == 16) ? 1 : -1];
const XwaNameTable g_xwaObjectTypeFlagNames = { "object type flag", s_objectTypeFlags, 0, 16, NULL };

static const char* const s_shipListCategories[] = {
	"fighter", "light_transport",    "utility", "container",      "heavy_transport", "starship",
	"station", "weapon_emplacement", "mine",    "satellite_buoy", "droid",
};
static const char* const s_shipListCategoryTexts[] = {
	"Fighter",
	"Shuttle/Light Transport",
	"Utility Craft",
	"Container",
	"Freighter/Heavy Transport",
	"Starship",
	"Station",
	"Weapon emplacement",
	"Mine",
	"Satellite/Buoy",
	"Droid",
};
typedef char
	ship_list_categories_check[(sizeof s_shipListCategories / sizeof s_shipListCategories[0] == 11 &&
								sizeof s_shipListCategoryTexts / sizeof s_shipListCategoryTexts[0] == 11)
								   ? 1
								   : -1];
const XwaNameTable g_xwaShipListCategoryNames = { "ship list category", s_shipListCategories, 0, 11, NULL };
const XwaNameTable g_xwaShipListCategoryTexts = { "ship list category text", s_shipListCategoryTexts, 0, 11,
												  NULL };

/* Kind 0 is `none`, for example in an empty laser group. */
static const char* const s_laserKinds[] = {
	"none", "laser_cannon", "ion_cannon", NULL, "turret", "long_range_turret",
};
typedef char laser_kinds_check[(sizeof s_laserKinds / sizeof s_laserKinds[0] == 6) ? 1 : -1];
const XwaNameTable g_xwaLaserKindNames = { "laser group kind", s_laserKinds, 0, 6, NULL };

static const char* const s_hardpointKinds[] = { "none", "laser", "warhead" };
typedef char hardpoint_kinds_check[(sizeof s_hardpointKinds / sizeof s_hardpointKinds[0] == 3) ? 1 : -1];
const XwaNameTable g_xwaHardpointKindNames = { "hardpoint kind", s_hardpointKinds, 0, 3, NULL };

static const char* const s_ranks[] = { "novice", "officer", "veteran", "ace", "top_ace", "super_ace" };
typedef char ranks_check[(sizeof s_ranks / sizeof s_ranks[0] == 6) ? 1 : -1];
const XwaNameTable g_xwaRankNames = { "rank", s_ranks, 0, 6, NULL };

static const char* const s_difficulties[] = { "easy", "medium", "hard" };
typedef char difficulties_check[(sizeof s_difficulties / sizeof s_difficulties[0] == 3) ? 1 : -1];
const XwaNameTable g_xwaDifficultyNames = { "difficulty", s_difficulties, 0, 3, NULL };

/* Slots 15 to 18 are the threat indicators in laser mode, and slots 19 to 22
   the threat indicators in warhead mode. Both use the HudThreatIndicatorSlot
   order. */
static const char* const s_reticleSlots[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"laser",
	"laser_in_range",
	"warhead",
	"warhead_lock",
	"cannon_ready",
	"cannon",
	NULL,
	NULL,
	NULL,
	NULL,
	"threats.laser_mode.laser",
	"threats.laser_mode.turret",
	"threats.laser_mode.beam",
	"threats.laser_mode.warhead",
	"threats.warhead_mode.laser",
	"threats.warhead_mode.turret",
	"threats.warhead_mode.beam",
	"threats.warhead_mode.warhead",
};
typedef char reticle_slots_check[(sizeof s_reticleSlots / sizeof s_reticleSlots[0] == 23) ? 1 : -1];
const XwaNameTable g_xwaReticleSlotNames = { "reticle slot", s_reticleSlots, 0, 23, NULL };

/* Group 6 also plays for weapon types 297, 301 and 302, and group 7 also
   plays for 303, 304 and 305. */
static const char* const s_soundWeaponGroups[] = {
	"rebel_laser",      "rebel_laser_turbo", "imperial_laser",   "imperial_laser_turbo", "ion_cannon",
	"ion_cannon_turbo", "turbolaser_red",    "turbolaser_green", "turbolaser_ion",       "custom_weapon",
};
typedef char
	sound_weapon_groups_check[(sizeof s_soundWeaponGroups / sizeof s_soundWeaponGroups[0] == 10) ? 1 : -1];
const XwaNameTable g_xwaSoundWeaponGroupNames = { "weapon sound group", s_soundWeaponGroups, 0, 10, NULL };

/* Sources 27, 28 and 29 have no name. They have no place on the HUD, so
   they draw nothing. */
static const char* const s_screenSources[] = {
	"left_sensor",
	"right_sensor",
	"laser_recharge",
	"shield_recharge",
	"engine_power",
	"beam_recharge",
	"shields",
	"beam_level",
	"targeting_computer",
	"quad_lasers_left",
	"quad_lasers_right",
	"left_text_box",
	"right_text_box",
	"speed_throttle",
	"missiles",
	"name_time",
	"ship_count",
	"quad_lasers",
	"dual_lasers_left",
	"dual_lasers_right",
	"dual_lasers",
	"b_wing_lasers",
	"six_lasers",
	"six_lasers_left",
	"six_lasers_right",
	"shields_front",
	"shields_back",
	NULL,
	NULL,
	NULL,
	"text_radio",
	"text_system",
	"text_command",
	"target_name",
	"target_shields",
	"target_hull",
	"target_cargo",
	"target_systems",
	"target_distance",
	"target_subcomponent",
	"eight_lasers",
	"throttle_bar",
	"autosize_missiles_left",
	"autosize_missiles_right",
	"autosize_speed",
	"autosize_chaff",
	"autosize_name",
	"autosize_time",
	"autosize_missiles_both",
	"autosize_missiles_trimmed",
	"dual_laser_energy_left",
	"dual_laser_energy_right",
	"quad_laser_energy_left_up",
	"quad_laser_energy_left_down",
	"quad_laser_energy_right_up",
	"quad_laser_energy_right_down",
	"all_laser_energy",
};
typedef char screen_sources_check
	[(sizeof s_screenSources / sizeof s_screenSources[0] == XWA_SCREEN_SOURCE_COUNT) ? 1 : -1];
const XwaNameTable g_xwaScreenSourceNames = { "screen source", s_screenSources, 0, XWA_SCREEN_SOURCE_COUNT,
											  NULL };

static const char* const s_hudAreas[] = {
	"left_sensor",       "right_sensor",      "shields",        "beam",
	"target_and_lasers", "left_text_box",     "right_text_box", "top_left",
	"top_right",         "text_radio_system", "text_command",
};
typedef char hud_areas_check[(sizeof s_hudAreas / sizeof s_hudAreas[0] == 11) ? 1 : -1];
const XwaNameTable g_xwaHudAreaNames = { "HUD area", s_hudAreas, 0, 11, NULL };

/* Returns the name at `index` in `table`, using the mod's renames. */
static const char* Names_Current(const XwaNameTable* table, int index) {
	if (table->renames != NULL && table->renames[index] != NULL) {
		return table->renames[index];
	}
	return table->names[index];
}

const char* XwaNames_Name(const XwaNameTable* table, int value) {
	if (value < table->first || value - table->first >= table->count) {
		return NULL;
	}
	return Names_Current(table, value - table->first);
}

int XwaNames_Find(const XwaNameTable* table, const char* text) {
	int index;

	if (text == NULL) {
		return -1;
	}
	for (index = 0; index < table->count; ++index) {
		const char* name = Names_Current(table, index);

		if (name != NULL && strcmp(name, text) == 0) {
			return table->first + index;
		}
	}
	return -1;
}

int XwaNames_ValidName(const char* text) {
	size_t i;

	if (text == NULL || text[0] == '\0' || strlen(text) >= XWA_NAME_MAX) {
		return 0;
	}
	for (i = 0; text[i] != '\0'; ++i) {
		const char c = text[i];

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
			return 0;
		}
	}
	return 1;
}

void XwaNames_RenameObjectType(int objectType, const char* name) {
	if (objectType < 0 || objectType >= g_xwaObjectTypeNames.count) {
		return;
	}
	if (name == NULL || !XwaNames_ValidName(name)) {
		s_objectTypeRenames[objectType] = NULL;
		return;
	}
	strcpy(s_objectTypeRenameTexts[objectType], name);
	s_objectTypeRenames[objectType] = s_objectTypeRenameTexts[objectType];
}

void XwaNames_ResetObjectTypeNames(void) { memset(s_objectTypeRenames, 0, sizeof s_objectTypeRenames); }

const char* XwaNames_StockName(const XwaNameTable* table, int value) {
	if (value < table->first || value - table->first >= table->count) {
		return NULL;
	}
	return table->names[value - table->first];
}

void XwaNames_LogUnknown(const XwaNameTable* table, const char* path, int line, const char* key,
						 const char* text) {
	/* Long tables aren't listed in the message. The reference tables list
	   them. */
	enum { LIST_MAX = 40 };
	char list[1024];
	size_t length = 0;
	int named = 0;
	int index;

	list[0] = '\0';
	for (index = 0; index < table->count; ++index) {
		named += Names_Current(table, index) != NULL;
	}
	for (index = 0; named <= LIST_MAX && index < table->count; ++index) {
		const char* name = Names_Current(table, index);
		const int written =
			name == NULL ? 0
						 : snprintf(list + length, sizeof list - length, "%s%s", length ? ", " : "", name);
		if (written < 0 || (size_t)written >= sizeof list - length) {
			break;
		}
		length += (size_t)written;
	}
	/* Tell the user when the name is a stock name that the mod renamed. */
	for (index = 0; table->renames != NULL && text != NULL && index < table->count; ++index) {
		if (table->renames[index] != NULL && table->names[index] != NULL &&
			strcmp(table->names[index], text) == 0) {
			Aeron_LogWarn("xwa.models", "%s:%d: %s: the mod renamed %s '%s' to '%s'", path, line, key,
						  table->what, text, table->renames[index]);
			return;
		}
	}
	if (list[0]) {
		Aeron_LogWarn("xwa.models", "%s:%d: %s: unknown %s '%s'. Use one of: %s", path, line, key,
					  table->what, text ? text : "", list);
	} else {
		Aeron_LogWarn("xwa.models", "%s:%d: %s: unknown %s '%s'", path, line, key, table->what,
					  text ? text : "");
	}
}

/* Writes a short description of the value of `node`, which isn't a string,
   to `out`, such as "4" or "a list". */
static void Names_DescribeValue(const AeronConfigNode* node, char* out, size_t size) {
	switch (AeronConfigNode_Type(node)) {
		case AERON_CONFIG_BOOL:
			snprintf(out, size, "%s", AeronConfigNode_Bool(node, 0) ? "true" : "false");
			break;
		case AERON_CONFIG_INT:
			snprintf(out, size, "%lld", (long long)AeronConfigNode_Int(node, 0));
			break;
		case AERON_CONFIG_FLOAT:
			snprintf(out, size, "%g", AeronConfigNode_Float(node, 0.0));
			break;
		case AERON_CONFIG_MAP:
			snprintf(out, size, "a map");
			break;
		case AERON_CONFIG_SEQUENCE:
			snprintf(out, size, "a list");
			break;
		default:
			snprintf(out, size, "empty");
			break;
	}
}

int XwaNames_Read(const XwaNameTable* table, const char* path, const char* key, const AeronConfigNode* node,
				  int* out) {
	const char* text;
	int value;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING) {
		char description[64];

		Names_DescribeValue(node, description, sizeof description);
		Aeron_LogWarn("xwa.models", "%s:%d: %s must be a name, but it is %s", path,
					  AeronConfigNode_Line(node), key, description);
		return 0;
	}
	text = AeronConfigNode_String(node, NULL);
	value = XwaNames_Find(table, text);

	if (value < 0) {
		XwaNames_LogUnknown(table, path, AeronConfigNode_Line(node), key, text);
		return 0;
	}
	*out = value;
	return 1;
}
