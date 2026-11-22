/*
 * Models menu: the material override, the material animation states, and the
 * levels of detail and texture filtering.
 */

#include "xwa_remaster/debug/debug_internal.h"

#include "xwa_remaster/flight.h"
#include "xwa_remaster/ship.h"

/* ---- Materials ------------------------------------------------------------- */

/* The state names of the converted models, by bit (AERON_ANIM_STATE_*). */
static const char* const k_material_states[31] = {
	"threat.laser",
	"threat.turret",
	"threat.beam",
	"threat.missile_locking",
	"threat.missile_locked",
	"cannon.1",
	"cannon.2",
	"cannon.3",
	"cannon.4",
	"cannon.5",
	"cannon.6",
	"cannon.7",
	"cannon.8",
	"broken.cmd",
	"broken.laser_ion",
	"broken.beam_weapon",
	"broken.shields",
	"broken.throttle",
	"broken.sensors",
	"broken.laser_recharge",
	"broken.engine_power",
	"broken.shield_recharge",
	"broken.beam_recharge",
	"hull.25",
	"hull.50",
	"hull.75",
	"laser_lock",
	"warhead.locking",
	"warhead.locked",
	"hangar",
	"target",
};

static void xwa_window_materials(void*) {
	XwaShipPbrTuning tuning;
	XwaRemasterShip_GetPbrTuning(&tuning);
	bool override_on = tuning.debug_material_override != 0.0f;
	bool changed = ImGui::Checkbox("Same material everywhere", &override_on);
	ImGui::SetItemTooltip("Shades every PBR material with the values below. For example, metallic 1\n"
						  "and roughness 0.1 show the environment's reflections.");
	ImGui::BeginDisabled(!override_on);
	changed |= ImGui::SliderFloat("Metallic", &tuning.debug_metallic, 0.0f, 1.0f, "%.2f");
	changed |= ImGui::SliderFloat("Roughness", &tuning.debug_roughness, 0.0f, 1.0f, "%.2f");
	ImGui::EndDisabled();
	if (changed) {
		tuning.debug_material_override = override_on ? 1.0f : 0.0f;
		XwaRemasterShip_SetPbrTuning(&tuning);
	}

	ImGui::SeparatorText("Animation states");
	XwaFlightMaterialAnimOverride o;
	XwaRemasterFlight_GetMaterialAnimOverride(&o);
	bool anim_changed = XwaDebug_Checkbox("Force states", &o.enabled);
	ImGui::SameLine();
	anim_changed |= XwaDebug_Checkbox("On every object", &o.all_objects);
	ImGui::SetItemTooltip("Off: only on the player's target.");
	ImGui::BeginDisabled(!o.enabled);
	static const char* const stages[] = { "None", "75%", "50%", "25%" };
	static const char* const energies[] = { "None", "Shields down", "Tractor beam", "Jamming beam" };
	int stage = (int)o.damage_stage, energy = (int)o.energy;
	anim_changed |= ImGui::Combo("Hull damage", &stage, stages, 4);
	anim_changed |= ImGui::Combo("Energy effect", &energy, energies, 4);
	unsigned int mask = o.state_mask;
	anim_changed |= ImGui::InputScalar("State mask", ImGuiDataType_U32, &mask, nullptr, nullptr, "%08X",
									   ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::SetItemTooltip("One bit per state, in the order of the list below.");
	ImGui::EndDisabled();
	if (anim_changed) {
		o.damage_stage = (uint32_t)stage;
		o.energy = (uint32_t)energy;
		o.state_mask = mask & 0x7FFFFFFFu;
		XwaRemasterFlight_SetMaterialAnimOverride(&o);
	}

	const uint32_t player = XwaRemasterFlight_GetPlayerMaterialStates();
	ImGui::Text("The player's states: %08X", player);
	for (int i = 0; i < 31; i++) {
		if (player >> i & 1u) {
			ImGui::BulletText("%s", k_material_states[i]);
		}
	}
}

/* ---- Detail and filtering ---------------------------------------------------- */

static void xwa_window_detail(void*) {
	XwaFlightLodParams lod;
	XwaRemasterFlight_GetLod(&lod);
	if (ImGui::SliderFloat("Largest LOD error", &lod.max_error, 0.0f, 4.0f, "%.2f px")) {
		XwaRemasterFlight_SetLod(&lod);
	}
	ImGui::SetItemTooltip("Models seen small draw simpler versions of themselves, with errors up to\n"
						  "this size, in pixels of a 1080-line view. 0 always draws them in full.");

	XwaFlightTextureFilteringParams filtering;
	XwaRemasterFlight_GetTextureFiltering(&filtering);
	bool changed = XwaDebug_Checkbox("Anisotropic filtering", &filtering.anisotropic);
	ImGui::BeginDisabled(!filtering.anisotropic);
	changed |= ImGui::SliderFloat("Largest anisotropy", &filtering.max_anisotropy, 1.0f, 16.0f, "%.0fx");
	ImGui::EndDisabled();
	if (XwaDebug_ResetButton("Reset filtering")) {
		XwaRemasterFlight_GetTextureFilteringDefault(&filtering);
		changed = true;
	}
	if (changed) {
		XwaRemasterFlight_SetTextureFiltering(&filtering);
	}
}

void XwaDebugModels_Register(void) {
	XwaDebug_AddWindow(XWA_DEBUG_MENU_MODELS, "Materials", xwa_window_materials, 400, 0);
	XwaDebug_AddWindow(XWA_DEBUG_MENU_MODELS, "Detail & Filtering", xwa_window_detail, 400, 0);
}
