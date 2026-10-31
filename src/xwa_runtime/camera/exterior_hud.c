#include "xwa_runtime/camera/exterior_hud.h"

#include "xwa/flight/hangar.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/config/modern_camera_options.h"

/* Modern port module; no original counterpart. */

int XwaExteriorHud_Shown(int playerIdx) {
	const PlayerData* player = &g_players[playerIdx];
	XwaModernCameraOptions options;

	if (playerIdx != g_localPlayer || !g_useHardware3D || !player->viewState.externalCameraActive ||
		player->viewState.cameraFocusObjIdx != player->objectIndex || !player->savedHudEnabled ||
		player->mapCameraState || g_inHangarReady || g_replayViewMode ||
		(g_filmPlaybackMode && g_filmOverlayActive == 1)) {
		return 0;
	}
	XwaModernCameraOptions_Get(&options);
	return options.enabled && options.exterior_hud;
}

int XwaExteriorHud_HidesHud(int playerIdx) {
	return g_players[playerIdx].viewState.externalCameraActive && !XwaExteriorHud_Shown(playerIdx);
}

int XwaExteriorHud_HudVisible(int playerIdx) {
	return g_players[playerIdx].hudEnabled || XwaExteriorHud_Shown(playerIdx);
}
