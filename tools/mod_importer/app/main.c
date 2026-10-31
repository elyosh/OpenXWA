/*
 * The OpenXWA Mod Importer: a window that imports an XWA Upgrade
 * installation into an OpenXWA mod folder, and chooses the mod that OpenXWA
 * uses. It shares OpenXWA's user folder and config.yaml.
 */

#include "aeron/aeron.h"
#include "aeron/log.h"
#include "ui.h"

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;
	AeronConfig config = {
		.org_name = "TotallyOpen",
		.app_name = "OpenXWA",
		.resource_path = "resources",
		.shader_path = "shaders",
		.window_title = "OpenXWA Mod Importer",
		.window_width = 1280,
		.window_height = 720,
		.logical_width = 1920,
		.logical_height = 1080,
		.presentation_mode = AERON_PRESENTATION_ASPECT_FIT,
		.clear_color_enabled = 1,
		.clear_color_rgba = { 0.01f, 0.012f, 0.016f, 1.0f },
	};
	if (!Aeron_Init(&config))
		return 1;
	char error[512];
	ImporterUi* app = ImporterUi_Create(error, sizeof error);
	if (!app) {
		Aeron_LogError("importer", "%s", error);
		Aeron_FatalError("OpenXWA Mod Importer", error);
		Aeron_Shutdown();
		return 1;
	}
	int exit_code = 0;
	for (;;) {
		const int32_t delta_us = Aeron_BeginFrame();
		if (Aeron_FatalErrorRequested()) {
			exit_code = 1;
			break;
		}
		const ImporterUiFrame frame = ImporterUi_Frame(app, (float)delta_us * 1e-6f);
		if (frame == IMPORTER_UI_QUIT)
			break;
		if (frame == IMPORTER_UI_FAILED || !Aeron_Present()) {
			Aeron_LogError("importer", "The window can't be drawn.");
			exit_code = 1;
			break;
		}
	}
	ImporterUi_Destroy(app);
	Aeron_Shutdown();
	return exit_code;
}
