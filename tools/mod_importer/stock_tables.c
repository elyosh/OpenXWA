#include "stock_tables.h"

#include <string.h>

#include "util.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa_runtime/models/names.h"
#include "xwau_config.h"

/* The game's tables have the stock sizes because this code is built without
   XWA_MODERN. */
typedef char stock_object_types_check[(XWAU_GAME_OBJECT_TYPES == OBJ_Count) ? 1 : -1];
typedef char stock_craft_types_check[(XWAU_GAME_CRAFT_TYPES == XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT) ? 1 : -1];
typedef char stock_records_check[(XWAU_GAME_RECORDS == XWA_MODEL_DEF_COUNT) ? 1 : -1];
typedef char stock_weapon_slots_check[(XWA_CRAFT_WEAPON_SLOT_COUNT == XWA_STOCK_WEAPON_SLOT_COUNT) ? 1 : -1];
typedef char
	stock_engine_glows_check[(XWA_CRAFT_ENGINE_EMITTER_COUNT == XWA_STOCK_ENGINE_EMITTER_COUNT) ? 1 : -1];

/* Writes the `size` low bytes of `value` at `*out`, in little-endian order,
 * and moves `*out` past them. */
static void put(uint8_t** out, uint32_t value, size_t size) {
	for (size_t i = 0; i < size; ++i)
		*(*out)++ = (uint8_t)(value >> (8 * i));
}

static void put_points(uint8_t** out, const Vec3i* points, size_t count) {
	for (size_t i = 0; i < count; ++i) {
		put(out, (uint32_t)points[i].x, 4);
		put(out, (uint32_t)points[i].y, 4);
		put(out, (uint32_t)points[i].z, 4);
	}
}

/* Writes an object type entry with the executable's 24-byte layout. The
   texture pointers are zero because the game only sets them at run time. */
static void pack_object_type(const ObjectTypeInfo* type, uint8_t* out) {
	put(&out, type->recordFlags, 1);
	put(&out, type->assetFlags, 1);
	put(&out, type->familyId, 1);
	put(&out, type->genusId, 1);
	put(&out, (uint32_t)type->maxBoundsExtent, 4);
	put(&out, 0, 4);
	put(&out, 0, 4);
	put(&out, type->flags, 2);
	put(&out, (uint16_t)type->modelDefIndex, 2);
	put(&out, type->textureGroup, 2);
	put(&out, type->frameCount, 2);
}

/* Writes a craft record with the executable's packed 987-byte layout, in
   which pointers take 4 bytes. The name and engine glow pointers are zero
   because the game only sets them at run time. Returns false when the record
   doesn't come to 987 bytes. */
static bool pack_record(const ModelDef* d, uint8_t* record) {
	uint8_t* p = record;
	put(&p, 0, 4);
	put(&p, 0, 4);
	put(&p, d->craftPointValue, 2);
	put(&p, d->ratingWeight, 2);
	put(&p, d->hasHyperdrive, 1);
	put(&p, d->laserConvergeMode, 1);
	put(&p, d->hasShields, 1);
	put(&p, (uint32_t)d->shieldStrength, 4);
	put(&p, d->field_13, 1);
	put(&p, d->gap_14, 1);
	put(&p, (uint32_t)d->hullStrength, 4);
	put(&p, (uint32_t)d->systemDamageHullThreshold, 4);
	put(&p, d->systemStrength, 2);
	put(&p, d->componentMaxHp, 1);
	put(&p, d->maxSpeed, 2);
	put(&p, d->accelRate, 2);
	put(&p, d->decelRate, 2);
	put(&p, (uint16_t)d->yawRate, 2);
	put(&p, d->autoBankFactor, 2);
	put(&p, (uint16_t)d->rollRate, 2);
	put(&p, (uint16_t)d->pitchRate, 2);
	put(&p, d->maxTumbleAngle, 2);
	put(&p, d->maxPushRate, 2);
	memcpy(p, d->name, sizeof d->name);
	p += sizeof d->name;
	for (int i = 0; i < 3; ++i)
		put(&p, d->laserGroupWeaponType[i], 2);
	for (int i = 0; i < 3; ++i)
		put(&p, d->laserGroupFirstSlot[i], 1);
	for (int i = 0; i < 3; ++i)
		put(&p, d->laserGroupLastSlot[i], 1);
	for (int i = 0; i < 3; ++i)
		put(&p, d->laserGroupSlotCount[i], 1);
	for (int i = 0; i < 3; ++i)
		put(&p, d->laserGroupWeaponKind[i], 1);
	for (int i = 0; i < 3; ++i)
		put(&p, (uint32_t)d->laserGroupFireRange[i], 4);
	for (int i = 0; i < 3; ++i)
		put(&p, d->laserGroupFireCooldownTicks[i], 2);
	for (int i = 0; i < 2; ++i)
		put(&p, d->warheadLauncherType[i], 2);
	for (int i = 0; i < 2; ++i)
		put(&p, d->warheadLauncherFirstSlot[i], 1);
	for (int i = 0; i < 2; ++i)
		put(&p, d->warheadLauncherLastSlot[i], 1);
	for (int i = 0; i < 2; ++i)
		put(&p, d->warheadLauncherSlotCount[i], 1);
	for (int i = 0; i < 2; ++i)
		put(&p, d->warheadLauncherCapacity[i], 1);
	for (int i = 0; i < XWA_STOCK_WEAPON_SLOT_COUNT; ++i) {
		put(&p, (uint16_t)d->weaponHardpoints[i].x, 2);
		put(&p, (uint16_t)d->weaponHardpoints[i].z, 2);
		put(&p, (uint16_t)d->weaponHardpoints[i].y, 2);
		put(&p, d->weaponHardpoints[i].alternateMeshHardpointIdx, 1);
		put(&p, d->weaponHardpoints[i].meshIdx, 1);
	}
	for (int i = 0; i < XWA_STOCK_ENGINE_EMITTER_COUNT; ++i)
		put(&p, 0, 4);
	for (int i = 0; i < XWA_STOCK_ENGINE_EMITTER_COUNT; ++i)
		put(&p, d->engineGlowMeshIdx[i], 1);
	put(&p, d->engineGlowCount, 1);
	put(&p, d->countermeasureCount, 1);
	put(&p, (uint32_t)d->gunsightOffsetY, 4);
	put(&p, (uint16_t)d->cockpitEyeY, 2);
	put(&p, (uint16_t)d->cockpitEyeZ, 2);
	put(&p, (uint16_t)d->cockpitEyeX, 2);
	const int16_t* const turret_fields[] = { d->turretSeatPosY,    d->turretSeatPosZ,
											 d->turretSeatPosX,    d->turretMountAngleB,
											 d->turretMountAngleA, (const int16_t*)d->turretObjectType,
											 d->turretAimLimitA,   d->turretAimLimitB };
	for (size_t f = 0; f < sizeof turret_fields / sizeof turret_fields[0]; ++f)
		for (int i = 0; i < 2; ++i)
			put(&p, (uint16_t)turret_fields[f][i], 2);
	for (int i = 0; i < 11; ++i)
		put(&p, (uint32_t)d->meshAttachData[i], 4);
	put(&p, d->boundSizeShift, 2);
	put(&p, d->boundSizeX, 2);
	put(&p, d->boundSizeZ, 2);
	put(&p, d->boundSizeY, 2);
	put(&p, d->dockPointCount, 2);
	put_points(&p, d->dockPoints, sizeof d->dockPoints / sizeof d->dockPoints[0]);
	for (int i = 0; i < 6; ++i)
		put(&p, (uint32_t)d->childMountPoints[i], 4);
	put(&p, d->floatHardpointCount, 1);
	put(&p, d->auxHardpointCount, 1);
	put_points(&p, d->auxHardpoints, sizeof d->auxHardpoints / sizeof d->auxHardpoints[0]);
	put(&p, d->jammingPointCount, 1);
	put_points(&p, d->jammingPoints, sizeof d->jammingPoints / sizeof d->jammingPoints[0]);
	return p - record == XWAU_GAME_RECORD_SIZE;
}

XwauGameTables* ImportStockTables_Create(const char* game) {
	char models[1100];
	if (!Xwau_FindDir(game, "FlightModels", models, sizeof models)) {
		Report(game, "has no FlightModels folder");
		return NULL;
	}
	XwauGameTables* tables = XwauGameTables_CreateStock(models);
	if (!tables)
		return NULL;
	for (int t = 0; t < XWAU_GAME_OBJECT_TYPES; ++t)
		pack_object_type(&g_objectTypes[t], tables->types[t]);
	for (int c = 0; c < XWAU_GAME_CRAFT_TYPES; ++c)
		tables->craft[c] = g_objectTypeTables.craftTypeToObjectType[c];
	for (int r = 0; r < XWAU_GAME_RECORDS; ++r) {
		if (!pack_record(&g_modelDefs[r], tables->records[r])) {
			Report(NULL, "the stock craft records don't have the layout of the game's");
			XwauGameTables_Free(tables);
			return NULL;
		}
	}
	return tables;
}

/* The stock values. The game includes the same files. */
static const uint16_t k_stockHullIcons[] = {
#include "xwa/flight/hud/shield_silhouettes.inc"
};
static const uint16_t k_stockMapIcons[] = {
#include "xwa/flight/flight_map_icons.inc"
};
static const uint8_t k_stockHardpointKinds[] = {
#include "xwa/flight/hardpoint_weapon_kinds.inc"
};
/* The game's arrays have these sizes, so the copies must have them too. */
typedef char stock_tables_check[(sizeof k_stockHullIcons / sizeof k_stockHullIcons[0] == 72 &&
								 sizeof k_stockMapIcons / sizeof k_stockMapIcons[0] == OBJ_NoAsset_222 + 1 &&
								 sizeof k_stockHardpointKinds == 40)
									? 1
									: -1];

/* Wraps a game name table as an XwauNameList. */
static XwauNameList list_of(const XwaNameTable* table) {
	return (XwauNameList) { table->names, table->first, table->count };
}

void ImportStockTables_Names(XwauNames* out) {
	memset(out, 0, sizeof *out);
	out->object_types = list_of(&g_xwaObjectTypeNames);
	out->weapon_types = list_of(&g_xwaWeaponTypeNames);
	out->warheads = list_of(&g_xwaWarheadNames);
	out->genus = list_of(&g_xwaGenusNames);
	out->family = list_of(&g_xwaFamilyNames);
	out->object_type_flags = list_of(&g_xwaObjectTypeFlagNames);
	out->settable_object_type_flags = XWA_OBJECT_TYPE_SETTABLE_FLAGS;
	out->ship_list_categories = list_of(&g_xwaShipListCategoryNames);
	out->ship_list_category_texts = list_of(&g_xwaShipListCategoryTexts);
	out->laser_kinds = list_of(&g_xwaLaserKindNames);
	out->hardpoint_kinds = list_of(&g_xwaHardpointKindNames);
	out->ranks = list_of(&g_xwaRankNames);
	out->difficulties = list_of(&g_xwaDifficultyNames);
	out->reticle_slots = list_of(&g_xwaReticleSlotNames);
	out->sound_weapon_groups = list_of(&g_xwaSoundWeaponGroupNames);
	out->screen_sources = list_of(&g_xwaScreenSourceNames);
	out->hud_areas = list_of(&g_xwaHudAreaNames);
}

void ImportStockTables_Values(XwauStockValues* out) {
	out->hull_icons = k_stockHullIcons;
	out->hull_icon_count = sizeof k_stockHullIcons / sizeof k_stockHullIcons[0];
	out->map_icons = k_stockMapIcons;
	out->map_icon_count = sizeof k_stockMapIcons / sizeof k_stockMapIcons[0];
	out->hardpoint_kinds = k_stockHardpointKinds;
	out->hardpoint_kind_count = sizeof k_stockHardpointKinds / sizeof k_stockHardpointKinds[0];
}
