#include "xwa_runtime/snapshot/snapshot_hud.h"

#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_def.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"

#include <string.h>

typedef struct XwaHudCaptureSlot {
	XwaHudState completed;
	XwaHudState building;
	uint32_t next_epoch;
	uint8_t building_active;
} XwaHudCaptureSlot;

typedef struct XwaHudPaneScope {
	uint16_t pane;
	int origin_x, origin_y;
} XwaHudPaneScope;

enum { XWA_HUD_PANE_SCOPE_DEPTH = 8 };

static XwaHudCaptureSlot g_hud_capture;
static XwaHudPaneScope g_hud_pane_scopes[XWA_HUD_PANE_SCOPE_DEPTH];
static uint8_t g_hud_pane_scope_depth;
static XwaHudTargetBoxLayer g_hud_target_box_layer = XWA_HUD_TARGET_BOX_AFTER_FIXED;

static void hud_initialize_reticle_indices(XwaHudReticle* reticle) {
	int i;
	for (i = 0; i < 16; i++) {
		reticle->laser_hardpoint_index[i] = -1;
		reticle->warhead_hardpoint_index[i] = -1;
	}
}

void XwaSnapshotHud_BeginClassicFrame(void) {
	XwaHudState* h = &g_hud_capture.building;
	memset(h, 0, sizeof *h);
	hud_initialize_reticle_indices(&h->reticle);
	h->classic_frame_epoch = ++g_hud_capture.next_epoch;
	g_hud_capture.building_active = 1;
	g_hud_pane_scope_depth = 0;
	g_hud_target_box_layer = XWA_HUD_TARGET_BOX_AFTER_FIXED;
}

void XwaSnapshotHud_EndClassicFrame(void) {
	if (!g_hud_capture.building_active) {
		return;
	}
	if (g_hud_pane_scope_depth != 0) {
		g_hud_capture.building.pane_scope_errors++;
		g_hud_pane_scope_depth = 0;
	}
	g_hud_capture.building.classic_frame_valid = 1;
	g_hud_capture.completed = g_hud_capture.building;
	g_hud_capture.building_active = 0;
	g_hud_target_box_layer = XWA_HUD_TARGET_BOX_AFTER_FIXED;
}

void XwaSnapshotHud_Reset(void) {
	memset(&g_hud_capture, 0, sizeof g_hud_capture);
	hud_initialize_reticle_indices(&g_hud_capture.completed.reticle);
	hud_initialize_reticle_indices(&g_hud_capture.building.reticle);
	g_hud_pane_scope_depth = 0;
	g_hud_target_box_layer = XWA_HUD_TARGET_BOX_AFTER_FIXED;
}

static XwaHudPane* hud_find_or_add_pane(XwaHudState* h, XwaHudPaneId pane, int origin_x, int origin_y,
										int width, int height) {
	for (uint16_t i = 0; i < h->pane_count; i++) {
		if (h->panes[i].id == pane)
			return &h->panes[i];
	}
	if (h->pane_count >= XWA_SNAP_MAX_HUD_PANES)
		return NULL;
	XwaHudPane* out = &h->panes[h->pane_count++];
	memset(out, 0, sizeof *out);
	out->id = (uint16_t)pane;
	out->visible = 1;
	out->generation = h->classic_frame_epoch;
	out->origin_x = (int16_t)origin_x;
	out->origin_y = (int16_t)origin_y;
	out->classic_w = (uint16_t)width;
	out->classic_h = (uint16_t)height;
	return out;
}

static void hud_push_pane(XwaHudPaneId pane, int origin_x, int origin_y, int width, int height,
						  int retain_origin) {
	if (!g_hud_capture.building_active)
		return;
	XwaHudState* h = &g_hud_capture.building;
	if (pane <= XWA_HUD_PANE_NONE || pane >= XWA_SNAP_MAX_HUD_PANES || width <= 0 || height <= 0 ||
		g_hud_pane_scope_depth >= XWA_HUD_PANE_SCOPE_DEPTH ||
		!hud_find_or_add_pane(h, pane, retain_origin ? origin_x : 0, retain_origin ? origin_y : 0, width,
							  height)) {
		h->pane_scope_errors++;
		return;
	}
	XwaHudPaneScope* scope = &g_hud_pane_scopes[g_hud_pane_scope_depth++];
	scope->pane = (uint16_t)pane;
	scope->origin_x = origin_x;
	scope->origin_y = origin_y;
}

void XwaSnapshotHud_PushPane(XwaHudPaneId pane, int origin_x, int origin_y, int width, int height) {
	hud_push_pane(pane, origin_x, origin_y, width, height, 1);
}

void XwaSnapshotHud_PushRelativePane(XwaHudPaneId pane, int origin_x, int origin_y, int width, int height) {
	hud_push_pane(pane, origin_x, origin_y, width, height, 0);
}

void XwaSnapshotHud_PopPane(void) {
	if (!g_hud_capture.building_active)
		return;
	if (g_hud_pane_scope_depth == 0) {
		g_hud_capture.building.pane_scope_errors++;
		return;
	}
	g_hud_pane_scope_depth--;
}

void XwaSnapshotHud_EmitFlightGlyph(uint8_t ch, uint8_t font_tier, int x, int y, uint8_t scale,
									uint8_t classic_w, uint32_t argb) {
	if (!g_hud_capture.building_active || g_hud_pane_scope_depth == 0)
		return;
	XwaHudState* h = &g_hud_capture.building;
	if (h->glyph_count >= XWA_SNAP_MAX_HUD_GLYPHS) {
		h->glyph_dropped++;
		return;
	}
	const XwaHudPaneScope* scope = &g_hud_pane_scopes[g_hud_pane_scope_depth - 1];
	const int local_x = x - scope->origin_x;
	const int local_y = y - scope->origin_y;
	if (local_x < -32768 || local_x > 32767 || local_y < -32768 || local_y > 32767) {
		h->glyph_dropped++;
		return;
	}
	XwaHudGlyph* glyph = &h->glyphs[h->glyph_count++];
	memset(glyph, 0, sizeof *glyph);
	glyph->pane = scope->pane;
	glyph->ch = ch;
	glyph->font_tier = font_tier;
	glyph->x = (int16_t)local_x;
	glyph->y = (int16_t)local_y;
	glyph->scale = scale;
	glyph->classic_w = classic_w;
	glyph->argb = argb;
}

void XwaSnapshotHud_NoteReticleReady(int slot, int ready) {
	if (g_hud_capture.building_active && slot >= 0 && slot < 16)
		g_hud_capture.building.reticle.ready[slot] = (uint8_t)(ready != 0);
}

void XwaSnapshotHud_NoteReticleInRange(int in_range) {
	if (g_hud_capture.building_active) {
		g_hud_capture.building.reticle.visible = 1;
		g_hud_capture.building.reticle.in_range = (uint8_t)(in_range != 0);
	}
}

void XwaSnapshotHud_NoteThreat(int slot, int state) {
	if (!g_hud_capture.building_active)
		return;
	XwaHudThreats* threats = &g_hud_capture.building.threats;
	switch (slot) {
		case 0:
			threats->laser = (uint8_t)state;
			break;
		case 1:
			threats->turret = (uint8_t)state;
			break;
		case 2:
			threats->beam = (uint8_t)state;
			break;
		case 3:
			threats->missile = (uint8_t)state;
			threats->flash_frame = (uint8_t)g_incomingMissileWarningFlashFrame;
			break;
		default:
			break;
	}
}

void XwaSnapshotHud_BeginRadar(int classic_radius) {
	if (!g_hud_capture.building_active)
		return;
	g_hud_capture.building.radar_blip_count = 0;
	g_hud_capture.building.radar_target_marker_visible = 0;
	g_hud_capture.building.radar_classic_radius =
		(uint16_t)(classic_radius > 0 && classic_radius <= 0xffff ? classic_radius : 0);
}

void XwaSnapshotHud_NoteRadarTargetMarker(int radar, int local_x, int local_y) {
	if (!g_hud_capture.building_active || radar < 0 || radar > 1 || local_x < -32768 || local_x > 32767 ||
		local_y < -32768 || local_y > 32767)
		return;
	XwaHudState* h = &g_hud_capture.building;
	h->radar_target_marker_visible = 1;
	h->radar_target_marker_radar = (uint8_t)radar;
	h->radar_target_marker_local_x = (int16_t)local_x;
	h->radar_target_marker_local_y = (int16_t)local_y;
}

void XwaSnapshotHud_NoteRadarBlip(uint16_t slot, uint16_t signature, int radar, int targeted, int local_x,
								  int local_y, uint16_t color_index) {
	if (!g_hud_capture.building_active || radar < 0 || radar > 1 || local_x < -32768 || local_x > 32767 ||
		local_y < -32768 || local_y > 32767)
		return;
	XwaHudState* h = &g_hud_capture.building;
	if (h->radar_classic_radius == 0 || h->radar_blip_count >= XWA_SNAP_MAX_HUD_RADAR_BLIPS)
		return;
	XwaHudRadarBlip* blip = &h->radar_blips[h->radar_blip_count++];
	blip->slot = slot;
	blip->signature = signature;
	blip->radar = (uint8_t)radar;
	blip->targeted = (uint8_t)(targeted != 0);
	blip->local_x = (int16_t)local_x;
	blip->local_y = (int16_t)local_y;
	blip->color_index = color_index;
}

void XwaSnapshotHud_NoteTargetBox(uint16_t slot, uint16_t signature, uint16_t component, uint16_t color_index,
								  int selected, int extent) {
	if (!g_hud_capture.building_active)
		return;
	XwaHudState* h = &g_hud_capture.building;
	if (h->target_box_count >= XWA_SNAP_MAX_HUD_TARGET_BOXES)
		return;
	XwaHudTargetBox* box = &h->target_boxes[h->target_box_count++];
	box->slot = slot;
	box->signature = signature;
	box->component = component;
	box->color_index = (uint8_t)color_index;
	box->selected = (uint8_t)(selected != 0);
	box->layer = (uint8_t)g_hud_target_box_layer;
	box->extent = extent;
}

XwaHudTargetBoxLayer XwaSnapshotHud_SetTargetBoxLayer(XwaHudTargetBoxLayer layer) {
	XwaHudTargetBoxLayer previous = g_hud_target_box_layer;
	if (layer == XWA_HUD_TARGET_BOX_BEFORE_FIXED || layer == XWA_HUD_TARGET_BOX_AFTER_FIXED)
		g_hud_target_box_layer = layer;
	return previous;
}

void XwaSnapshotHud_NoteCrt(const XwaHudCrt* crt) {
	if (g_hud_capture.building_active && crt)
		g_hud_capture.building.crt = *crt;
}

static uint32_t hud_capture_mode_flags(const PlayerData* player) {
	uint32_t flags = 0;
	if (player->viewState.externalCameraActive)
		flags |= XWA_HUD_MODE_EXTERNAL_CAMERA;
	if (player->mapCameraState)
		flags |= XWA_HUD_MODE_MAP;
	if (g_inHangarReady)
		flags |= XWA_HUD_MODE_HANGAR_READY;
	if (g_hangarAutoCam)
		flags |= XWA_HUD_MODE_HANGAR_AUTOCAM;
	if (player->hyperspacePhase)
		flags |= XWA_HUD_MODE_HYPERSPACE;
	if (player->regionSessionId)
		flags |= XWA_HUD_MODE_REGION_SESSION;
	if (g_flightMissionEndPending)
		flags |= XWA_HUD_MODE_MISSION_END;
	if (g_filmPlaybackMode)
		flags |= XWA_HUD_MODE_FILM_PLAYBACK;
	if (g_filmPlaybackMode && g_filmOverlayActive == 1)
		flags |= XWA_HUD_MODE_FILM_OVERLAY;
	if (g_replayViewMode)
		flags |= XWA_HUD_MODE_REPLAY_VIEW;
	if (g_provingGroundsModeActive)
		flags |= XWA_HUD_MODE_PROVING_GROUND;
	if (g_flightPlayerCount > 1)
		flags |= XWA_HUD_MODE_MULTIPLAYER;
	if (g_flightConfPowerVr)
		flags |= XWA_HUD_MODE_POWER_VR;
	if (player->cockpitVisible)
		flags |= XWA_HUD_MODE_COCKPIT_VISIBLE;
	if (player->padlockActive)
		flags |= XWA_HUD_MODE_PADLOCK;
	if (g_filmRecording)
		flags |= XWA_HUD_MODE_FILM_RECORDING;
	return flags;
}

static void hud_capture_top_level(XwaHudState* out, const PlayerData* player) {
	int i;
	out->hud_enabled = (uint8_t)(player->hudEnabled != 0);
	out->film_mfd_visible = (uint8_t)(g_filmOverlayMfdVisible != 0);
	out->classic_hud_scale = g_flightHudScaleFactor;
	out->mode_flags = hud_capture_mode_flags(player);
	out->element_enabled_mask = 0;
	for (i = 0; i < 12; i++) {
		if (g_hudElementEnabled[i].enabled) {
			out->element_enabled_mask |= (uint16_t)(1u << i);
		}
	}
	memcpy(out->mfd_enabled, player->mfd.enabled, sizeof out->mfd_enabled);
	memcpy(out->mfd_page, player->mfd.page, sizeof out->mfd_page);
	out->mfd_active = player->mfd.activeIndex;
	out->mfd_menu_row = player->mfd.menuRow;
	out->mfd_menu_item = player->mfd.menuItem;
	memcpy(out->hud_colors, g_hudColors, sizeof out->hud_colors);
}

static void hud_capture_target(XwaHudTarget* out, const PlayerData* player) {
	const int target_slot = (int)(uint16_t)player->currentTargetObjectIdx;
	out->valid = 0;
	out->slot = 0xffffu;
	out->signature = 0;
	out->selected_component = (uint16_t)player->selectedTargetComponent;
	out->padlock_active = (uint8_t)(player->padlockActive != 0);
	memcpy(out->name, g_hudTargetNameText, sizeof out->name);
	memcpy(out->status, g_hudTargetStatusText, sizeof out->status);
	out->name[sizeof out->name - 1] = '\0';
	out->status[sizeof out->status - 1] = '\0';
	out->distance_whole = g_hudTargetDistanceWhole;
	out->distance_frac = g_hudTargetDistanceFrac;
	out->shield_pct = g_hudTargetShieldDisplayPct;
	out->system_pct = g_hudTargetSystemDisplayPct;
	out->hull_pct = g_hudTargetHullDisplayPct;
	if (target_slot == 0xffff || g_objectTable == NULL || target_slot < 0 ||
		(uint32_t)target_slot >= g_objectTableSlotCount ||
		g_objectTable[target_slot].objectType == OBJ_None) {
		return;
	}
	out->slot = (uint16_t)target_slot;
	out->signature = g_objectTable[target_slot].objectSignature;
	out->valid = 1;
}

static void hud_capture_instruments(XwaHudState* out, const ObjectRecord* object, const MobileObject* mobile,
									const CraftData* craft, const ModelDef* model) {
	XwaHudInstruments* instruments = &out->instruments;
	int i;
	instruments->player_object_type = object->objectType;
	instruments->player_model_index = craft->modelIndex;
	instruments->throttle_speed = craft->throttleSpeed;
	instruments->speed = mobile->speed;
	instruments->engine_output_scale = craft->engineOutputScale;
	instruments->hull_damage = craft->hullDamage;
	instruments->hull_max = craft->hullMax;
	instruments->shield_front = craft->shieldFront;
	instruments->shield_rear = craft->shieldRear;
	instruments->shield_max = Craft_GetObjectMaxShield(out->player_slot);
	instruments->subsystem_damage = craft->subsystemDamage;
	instruments->installed_features = craft->installedHudFeatureMask;
	instruments->active_features = craft->activeHudFeatureMask;
	instruments->system_flags = craft->systemFlags;
	instruments->working_subsystems = craft->workingSubsystems;
	instruments->laser_redirect = craft->laserRedirect;
	instruments->shield_redirect = craft->shieldRedirect;
	instruments->beam_level = craft->beamLevel;
	instruments->beam_type = craft->beamTypeId;
	instruments->beam_present = craft->beamPresent;
	instruments->beam_active = craft->beamActive;
	instruments->cm_type = craft->cmTypeId;
	instruments->cm_count = craft->cmAmmoCount;
	instruments->shield_damage_flash = (uint8_t)(g_playerFlightTransientTimers[g_localPlayer].field_02 != 0);
	instruments->hull_damage_flash = (uint8_t)(g_playerFlightTransientTimers[g_localPlayer].field_04 != 0);
	instruments->last_shield_damage_side = g_lastShieldDamageSide;
	instruments->cannon_count = craft->cannonClassCount;
	instruments->laser_slot_count = craft->laserSlotCount;
	instruments->warhead_launcher_count = craft->warheadLauncherCount;
	instruments->energy_bank_laser_selector = (uint8_t)g_hudEnergyBankLaserSelector;
	instruments->energy_bank_ion_selector = (uint8_t)g_hudEnergyBankIonSelector;
	memcpy(instruments->laser_group_last_slot, model->laserGroupLastSlot,
		   sizeof instruments->laser_group_last_slot);
	memcpy(instruments->warhead_first_slot, model->warheadLauncherFirstSlot,
		   sizeof instruments->warhead_first_slot);
	memcpy(instruments->warhead_last_slot, model->warheadLauncherLastSlot,
		   sizeof instruments->warhead_last_slot);
	memcpy(instruments->warhead_slot_count, model->warheadLauncherSlotCount,
		   sizeof instruments->warhead_slot_count);
	instruments->shield_silhouette_sprite = object->objectType <= OBJ_ContainerBrick
												? g_shieldSilhouetteSpriteIdByObjectType[object->objectType]
												: 0;
	memcpy(instruments->laser_link_mode, craft->laserLinkMode, sizeof instruments->laser_link_mode);
	memcpy(instruments->laser_link_next_slot, craft->laserLinkNextSlot,
		   sizeof instruments->laser_link_next_slot);
	memcpy(instruments->laser_projectile_type, craft->laserProjectileTypeId,
		   sizeof instruments->laser_projectile_type);
	for (i = 0; i < 16; i++) {
		instruments->weapon_type[i] = craft->warheadData[i].weaponType;
		instruments->laser_charge[i] = craft->warheadData[i].laserCharge;
		instruments->warhead_count[i] = craft->warheadData[i].count;
		out->reticle.hardpoint_kind[i] = craft->warheadData[i].weaponType;
		out->reticle.hardpoint_local[i][0] = model->weaponHardpoints[i].x;
		out->reticle.hardpoint_local[i][1] = model->weaponHardpoints[i].z;
		out->reticle.hardpoint_local[i][2] = model->weaponHardpoints[i].y;
	}
	instruments->warhead_lock_ticks = craft->warheadLockTicks;
	memcpy(instruments->system_display_slot, craft->systemDisplaySlotBySystem,
		   sizeof instruments->system_display_slot);
	memcpy(instruments->system_health, craft->systemHealth, sizeof instruments->system_health);
	memcpy(instruments->system_timer, craft->systemTimer, sizeof instruments->system_timer);
}

static void hud_capture_direct_state(XwaHudState* out) {
	const PlayerData* player;
	const ObjectRecord* object;
	const MobileObject* mobile;
	const CraftData* craft;
	const ModelDef* model;
	int player_slot;

	out->valid = 0;
	if (g_localPlayer < 0 || g_localPlayer >= XWA_PLAYER_COUNT) {
		return;
	}
	player = &g_players[g_localPlayer];
	hud_capture_top_level(out, player);
	hud_capture_target(&out->target, player);
	out->reticle.weapon_mode = player->selectedWeaponMode;
	out->reticle.selected_warhead = player->selectedWarhead;
	out->reticle.missile_lock_state = player->missileLockState;
	out->reticle.look_yaw = player->lookYawOffset;
	out->reticle.look_pitch = player->lookPitchOffset;
	out->reticle.seat = (uint8_t)player->currentSeatIdx;
	out->reticle.turret_auto_fire = (uint8_t)(player->turretAutoFireState != 0);
	out->reticle.laser_hardpoint_count =
		(uint8_t)(g_reticleLaserHardpointCount < 0
					  ? 0
					  : (g_reticleLaserHardpointCount > 16 ? 16 : g_reticleLaserHardpointCount));
	out->reticle.warhead_hardpoint_count =
		(uint8_t)(g_reticleWarheadHardpointCount < 0
					  ? 0
					  : (g_reticleWarheadHardpointCount > 16 ? 16 : g_reticleWarheadHardpointCount));
	for (int i = 0; i < 16; i++) {
		out->reticle.laser_hardpoint_index[i] = (int16_t)g_reticleLaserHardpointIndices[i];
		out->reticle.warhead_hardpoint_index[i] = (int16_t)g_reticleWarheadHardpointIndices[i];
		out->reticle.aim_offset[i][0] = (int16_t)(g_reticleLaserAimPoints[i].x - g_reticleCenterX);
		out->reticle.aim_offset[i][1] = (int16_t)(g_reticleLaserAimPoints[i].y - g_reticleCenterY);
	}

	player_slot = player->objectIndex;
	out->player_slot = 0xffffu;
	out->player_signature = 0;
	if (g_objectTable == NULL || player_slot < 0 || player_slot == 0xffff ||
		(uint32_t)player_slot >= g_objectTableSlotCount) {
		return;
	}
	object = &g_objectTable[player_slot];
	if (object->objectType == OBJ_None || object->objectSignature != player->boundObjectSignature) {
		return;
	}
	out->player_slot = (uint16_t)player_slot;
	out->player_signature = object->objectSignature;
	mobile = object->mobj;
	if (mobile == NULL || mobile->pCraft == NULL) {
		return;
	}
	craft = mobile->pCraft;
	if (craft->modelIndex >= XWA_MODEL_DEF_COUNT) {
		return;
	}
	model = &g_modelDefs[craft->modelIndex];
	hud_capture_instruments(out, object, mobile, craft, model);
	out->valid = 1;
}

void XwaSnapshotHud_Capture(XwaHudState* out) {
	if (out) {
		*out = g_hud_capture.completed;
		hud_capture_direct_state(out);
	}
}
