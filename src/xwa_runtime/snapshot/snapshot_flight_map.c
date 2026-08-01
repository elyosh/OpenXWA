#include "xwa_runtime/snapshot/snapshot_flight_map.h"

#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/string_table.h"
#include "xwa/flight/ai/pai.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_map.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/math/trig2.h"
#include "xwa/render/renderer_internal.h"

#include <string.h>

static void map_label_char(char* out, uint32_t capacity, uint32_t* length, char value) {
	if (*length + 1 < capacity) {
		out[(*length)++] = value;
		out[*length] = '\0';
	}
}

static void map_label_string(char* out, uint32_t capacity, uint32_t* length, const char* value) {
	if (!value) {
		return;
	}
	while (*value) {
		map_label_char(out, capacity, length, *value++);
	}
}

static char map_label_base_color(int iff) {
	return iff == 0 ? 'Q' : ((iff == 1 || iff == 4) ? 'I' : (iff == 2 ? 'E' : (iff == 5 ? 'U' : 'M')));
}

static char map_label_bright_color(int iff) {
	return iff == 0 ? 'R' : ((iff == 1 || iff == 4) ? 'J' : (iff == 2 ? 'F' : (iff == 5 ? 'V' : 'N')));
}

/* Side-effect-free equivalent of Hud_AppendObjectDisplayName(slot, 2), the
 * exact label operation used by FlightMap_DrawObjectPass. */
static int map_format_label(uint16_t object_slot, char* out, uint32_t capacity) {
	if (!out || capacity == 0) {
		return 0;
	}
	out[0] = '\0';
	if (!g_objectTable || object_slot >= g_objectTableSlotCount) {
		return 0;
	}
	const ObjectRecord* object = &g_objectTable[object_slot];
	if (object->objectType == OBJ_None) {
		return 0;
	}
	const MobileObject* mobile = object->mobj;
	const int iff = mobile ? (uint8_t)mobile->iff : g_missionFlightGroups[object->flightGroupIdx].fg.iff;
	uint32_t length = 0;
	map_label_char(out, capacity, &length, (char)0xfe);
	map_label_char(out, capacity, &length, !mobile && iff == 5 ? 'V' : map_label_base_color(iff));

	if (mobile && !mobile->pCraft) {
		return (int)length;
	}
	map_label_char(out, capacity, &length, (char)0xfe);
	map_label_char(out, capacity, &length, map_label_bright_color(iff));
	const XwaFlightGroup* fg = &g_missionFlightGroups[object->flightGroupIdx].fg;
	if (!mobile) {
		map_label_string(out, capacity, &length, fg->name);
		return (int)length;
	}

	const CraftData* craft = mobile->pCraft;
	int craft_number;
	char separator;
	if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] >= 0) {
		map_label_string(out, capacity, &length, fg->name);
		craft_number = fg->disableWaveNumbering == 1 ||
							(!fg->globalUnit && fg->numberOfCraft == 1 && !fg->numberOfWaves)
						 ? 0
						 : (uint16_t)craft->craftIndexInGroup;
		separator = ' ';
	} else {
		map_label_string(out, capacity, &length, g_strPanelStrings[PANEL_STRING_NAME]);
		const unsigned display_group = (unsigned)object->flightGroupIdx + 1u;
		if (display_group >= 10u) {
			map_label_char(out, capacity, &length, (char)('0' + display_group / 10u));
		}
		map_label_char(out, capacity, &length, (char)('0' + display_group % 10u));
		separator = '-';
		craft_number = (uint16_t)craft->waveNumber + 1;
	}
	if (craft_number != 0) {
		if (craft_number > 999) {
			craft_number = 999;
		}
		map_label_char(out, capacity, &length, separator);
		if (craft_number >= 100) {
			map_label_char(out, capacity, &length, (char)('0' + craft_number / 100));
		}
		if (craft_number >= 10) {
			map_label_char(out, capacity, &length, (char)('0' + (craft_number / 10) % 10));
		}
		map_label_char(out, capacity, &length, (char)('0' + craft_number % 10));
	}
	return (int)length;
}

/* Distance-only form of trig2_ctop's lookup law. It intentionally lives at
 * the snapshot boundary so recovered trig scratch globals remain untouched. */
static uint32_t map_distance2d(uint32_t a, uint32_t b) {
	uint32_t divisor;
	if (a == b) {
		divisor = a;
		b = 0x100u;
	} else {
		if (a <= b) {
			const uint32_t temporary = b;
			b = a;
			a = temporary;
		}
		divisor = a;
		if (a != 0) {
			if ((a & 0xff000000u) == 0) {
				a <<= 8;
				b <<= 8;
				if ((a & 0xff000000u) == 0) {
					a <<= 8;
					b <<= 8;
				}
			}
			if (a == b) {
				b = 0x100u;
			} else {
				b = (b / (a >> 16)) & 0xffffu;
				b >>= 8;
			}
		} else {
			b = 0;
		}
	}
	const uint32_t root = g_squarerootable[(uint16_t)b];
	const uint32_t high = divisor >> 16;
	const uint32_t low = divisor & 0xffffu;
	return root * high + divisor + (((low * root + 0x8000u) >> 16) & 0xffffu);
}

static uint32_t map_abs_i32(int32_t value) {
	const uint32_t bits = (uint32_t)value;
	return value < 0 ? 0u - bits : bits;
}

static int32_t map_world_delta(int32_t to, int32_t from) {
	return (int32_t)((uint32_t)to - (uint32_t)from);
}

static uint16_t map_range(uint16_t from_slot, uint16_t to_slot) {
	if (!g_objectTable || from_slot >= g_objectTableSlotCount || to_slot >= g_objectTableSlotCount) {
		return 0;
	}
	const ObjectRecord* from = &g_objectTable[from_slot];
	const ObjectRecord* to = &g_objectTable[to_slot];
	const uint32_t x = map_abs_i32(map_world_delta(to->world_x, from->world_x));
	const uint32_t y = map_abs_i32(map_world_delta(to->world_y, from->world_y));
	const uint32_t z = map_abs_i32(map_world_delta(to->world_z, from->world_z));
	uint32_t range = map_distance2d(map_distance2d(x, y), z);
	range = (range * 161u) >> 16;
	return (uint16_t)(range >= 10000u ? 9999u : range);
}

static int map_resolve_ref(uint16_t ref, uint8_t fg_index, uint8_t order_index, int32_t out[3]) {
	if (ref < 0x8000u) {
		if (ref >= g_objectTableSlotCount || g_objectTable[ref].objectType == OBJ_None) {
			return 0;
		}
		out[0] = g_objectTable[ref].world_x;
		out[1] = g_objectTable[ref].world_y;
		out[2] = g_objectTable[ref].world_z;
		return 1;
	}
	if (fg_index >= 192) {
		return 0;
	}
	if (ref == 0x8000u) {
		ref = g_missionFgStats[fg_index].currentMissionPointRef;
	}
	const unsigned point = (unsigned)ref - 0x8000u;
	const XwaWaypoint* waypoint = point < 4u
		? &g_missionFlightGroups[fg_index].fg.missionPoints[point]
		: &g_missionFlightGroups[fg_index].fg.orders[4u * order_index].waypoints[point - 4u];
	out[0] = waypoint->x * 256;
	out[1] = -(waypoint->y * 256);
	out[2] = waypoint->z * 256;
	return 1;
}

static const AiController* map_effective_ai_controller(const CraftData* craft,
												const CraftData* const* resolving, unsigned depth) {
	if (!craft) {
		return NULL;
	}
	const ObjectRecord* linked = craft->effectiveAiObjectLink;
	if (!linked || linked->objectSignature != craft->turretAim.effectiveAiObjectSignature || !linked->mobj) {
		return &craft->aiController;
	}
	if (linked->mobj->pCharData) {
		return &linked->mobj->pCharData->aiController;
	}
	const CraftData* linked_craft = linked->mobj->pCraft;
	if (!linked_craft || linked_craft->aiLinkResolving || depth >= 16) {
		return &craft->aiController;
	}
	for (unsigned i = 0; i < depth; i++) {
		if (resolving[i] == linked_craft) {
			return &craft->aiController;
		}
	}
	const CraftData* next_resolving[16];
	memcpy(next_resolving, resolving, depth * sizeof resolving[0]);
	next_resolving[depth] = craft;
	return map_effective_ai_controller(linked_craft, next_resolving, depth + 1);
}

static int map_order_endpoint(uint16_t object_slot, int32_t out_world[3]) {
	if (!out_world || !g_objectTable || object_slot >= g_objectTableSlotCount) {
		return 0;
	}
	const ObjectRecord* object = &g_objectTable[object_slot];
	if (!object->mobj || !object->mobj->pCraft) {
		return 0;
	}
	const CraftData* craft = object->mobj->pCraft;
	uint16_t ref;
	if (object_slot >= g_activeRegionObjectSlotStart && object_slot < g_activeRegionCraftObjectSlotEnd) {
		const CraftData* resolving[16] = { 0 };
		const AiController* controller = map_effective_ai_controller(craft, resolving, 0);
		if (!controller) {
			return 0;
		}
		ref = controller->targetObjIdx;
	} else {
		ref = craft->modelIndex;
	}
	return ref != 0xffffu && map_resolve_ref(ref, object->flightGroupIdx, object->regionIdx, out_world);
}

static int map_primary_kind(uint8_t genus, uint8_t* render_kind, uint8_t* cull_kind) {
	switch (genus) {
		case GENUS_Fighter:
		case GENUS_Transport:
		case GENUS_Utility:
		case GENUS_Freighter:
		case GENUS_Starship:
		case GENUS_Platform:
		case GENUS_SatelliteBuoy:
		case GENUS_Container:
		case GENUS_PilotDroid:
		case GENUS_WeaponEmplacement:
			*render_kind = XWA_FLIGHT_MAP_RENDER_CRAFT;
			*cull_kind = XWA_FLIGHT_MAP_CULL_BOUNDS;
			return 1;
		case GENUS_PlayerProjectile:
		case GENUS_NpcProjectile:
			*render_kind = XWA_FLIGHT_MAP_RENDER_PROJECTILE;
			*cull_kind = XWA_FLIGHT_MAP_CULL_SPHERE;
			return 1;
		case GENUS_Debris:
		case GENUS_Explosion:
			*render_kind = XWA_FLIGHT_MAP_RENDER_SCENE_OBJECT;
			*cull_kind = XWA_FLIGHT_MAP_CULL_SPHERE;
			return 1;
		default:
			return 0;
	}
}

void XwaSnapshotFlightMap_Begin(XwaSnapshot* snapshot) {
	if (!snapshot) {
		return;
	}
	memset(&snapshot->flight_map, 0, sizeof snapshot->flight_map);
	snapshot->flight_map.active = (uint8_t)(g_players[g_localPlayer].mapCameraState != 0);
}

void XwaSnapshotFlightMap_CaptureObject(XwaSnapshot* snapshot, uint32_t slot,
										uint16_t flight_object_index) {
	if (!snapshot || !snapshot->flight_map.active || slot >= g_objectTableSlotCount || slot > UINT16_MAX ||
		flight_object_index == UINT16_MAX || flight_object_index >= snapshot->flight_object_count) {
		return;
	}
	const ObjectRecord* object = &g_objectTable[slot];
	uint8_t render_kind;
	uint8_t cull_kind;
	if (slot >= g_activeRegionObjectSlotStart && slot < g_explosionObjectSlotEnd) {
		if (!map_primary_kind(object->genusId, &render_kind, &cull_kind)) {
			return;
		}
	} else if (slot >= g_objScanStart && slot < g_regionStaticObjectSlotEnd) {
		if (object->genusId != GENUS_Mine && object->genusId != GENUS_DeathStarTunnelSegment) {
			return;
		}
		render_kind = XWA_FLIGHT_MAP_RENDER_SCENE_OBJECT;
		cull_kind = XWA_FLIGHT_MAP_CULL_BOUNDS;
	} else {
		return;
	}
	if (snapshot->flight_map.object_count >= XWA_SNAP_MAX_FLIGHT_MAP_OBJECTS) {
		snapshot->dropped_records++;
		return;
	}

	XwaFlightMapObject* map = &snapshot->flight_map.objects[snapshot->flight_map.object_count++];
	memset(map, 0, sizeof *map);
	map->flight_object_index = flight_object_index;
	map->label_offset = UINT16_MAX;
	map->max_bounds_extent = g_modelTypeTable[(uint16_t)object->objectType].maxBoundsExtent;
	map->box_extent = Targeting_GetObjectBoxExtent((int)slot);
	map->render_kind = render_kind;
	map->cull_kind = cull_kind;
	map->icon_id = object->objectType <= OBJ_NoAsset_222
					   ? g_flightMapIconByObjectType[(uint16_t)object->objectType]
					   : 0;
	map->effective_iff =
		object->mobj ? (uint8_t)object->mobj->iff : g_missionFlightGroups[object->flightGroupIdx].fg.iff;
	if (object->mobj) {
		map->move_x = object->mobj->moveX;
		map->move_y = object->mobj->moveY;
		map->movement_visible = (uint8_t)(object->mobj->state == 0);
	}
	map->label_visible =
		(uint8_t)(slot < g_activeRegionCraftObjectSlotEnd || !object->mobj || object->mobj->pCraft != NULL);
	if (map->label_visible && !(object->genusId == GENUS_Mine && !object->mobj)) {
		char label[256];
		const int length = map_format_label((uint16_t)slot, label, sizeof label);
		if (length > 0 && (uint32_t)snapshot->flight_map.label_bytes + (uint32_t)length + 1u <=
						 XWA_SNAP_FLIGHT_MAP_LABEL_BYTES) {
			map->label_offset = snapshot->flight_map.label_bytes;
			memcpy(&snapshot->flight_map.labels[snapshot->flight_map.label_bytes], label, (size_t)length + 1u);
			snapshot->flight_map.label_bytes =
				(uint16_t)(snapshot->flight_map.label_bytes + length + 1);
		}
	}

	const uint16_t focus = (uint16_t)g_players[g_localPlayer].viewState.cameraFocusObjIdx;
	const uint16_t target = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
	if (!g_replayViewMode && g_players[g_localPlayer].hyperspaceRuntime.targetBoxEnabled) {
		if ((uint16_t)slot == focus) {
			map->box_visible = 1;
			map->box_color_index = 0x2f;
		} else if ((uint16_t)slot == target) {
			map->box_visible = 1;
			map->box_color_index = 0x3b;
		} else if (render_kind == XWA_FLIGHT_MAP_RENDER_CRAFT && object->playerOwnerIdx != -1 &&
				   object->mobj && object->mobj->pCraft && !Object_HasActiveDecoyBeam((uint16_t)slot)) {
			const int player_iff = (uint16_t)g_players[g_localPlayer].playerIff;
			if (g_flightLocatePlayersEnabled ||
				(int8_t)object->mobj->pCraft->iffVisibility[player_iff] > 0 ||
				!Object_IsHostileToTeam((uint16_t)slot, player_iff)) {
				map->box_visible = 1;
				switch ((uint8_t)object->mobj->iff) {
					case 0:
						map->box_color_index = 63;
						break;
					case 1:
					case 4:
						map->box_color_index = 55;
						break;
					case 2:
						map->box_color_index = 51;
						break;
					default:
						map->box_color_index = 59;
						break;
				}
			}
		}
	}
	if (focus != 0xffffu && focus < g_objectTableSlotCount && g_objectTable[focus].objectType != OBJ_None &&
		object->genusId != GENUS_PlayerProjectile && object->genusId != GENUS_NpcProjectile) {
		map->range_value = map_range(focus, (uint16_t)slot);
	}
}

void XwaSnapshotFlightMap_End(XwaSnapshot* snapshot) {
	if (!snapshot || !snapshot->flight_map.active) {
		return;
	}
	const uint16_t target = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
	snapshot->flight_map.current_target_slot = target;
	if (target != 0xffffu && target < g_objectTableSlotCount &&
		g_objectTable[target].objectType != OBJ_None) {
		snapshot->flight_map.current_target_signature = g_objectTable[target].objectSignature;
		snapshot->flight_map.has_order_endpoint =
			(uint8_t)map_order_endpoint(target, snapshot->flight_map.order_endpoint_world);
	}
}
