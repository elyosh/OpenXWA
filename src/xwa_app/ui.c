#include "ui.h"

#include <stdio.h>
#include <string.h>

bool XwaAppUi_Init(XwaAppUi* ui, char* error, size_t error_capacity) {
	if (!ui) {
		if (error && error_capacity)
			snprintf(error, error_capacity, "invalid application UI configuration");
		return false;
	}
	memset(ui, 0, sizeof *ui);
	AeronCommandBuffer* command = Aeron_AcquireCommandBuffer();
	if (!command) {
		if (error && error_capacity)
			snprintf(error, error_capacity, "UI font upload command-buffer acquisition failed");
		return false;
	}
	if (!AeronFontAtlas_LoadKtx2Vfs(&ui->font, command, Aeron_GetVfs(), AERON_VFS_ROOT_RESOURCE,
									"assets/fonts/menu", 64u * 1024u * 1024u)) {
		Aeron_CancelCommandBuffer(command);
		if (error && error_capacity)
			snprintf(error, error_capacity,
					 "The options font, assets/fonts/menu.fnt and menu.ktx2, is missing from %s. Reinstall "
					 "OpenXWA.",
					 Aeron_ResourceRoot());
		return false;
	}
	if (!Aeron_SubmitCommandBuffer(command)) {
		AeronFontAtlas_Release(&ui->font);
		if (error && error_capacity)
			snprintf(error, error_capacity, "UI font atlas upload submission failed");
		return false;
	}
	ui->context = AeronUi_Create(NULL);
	if (!ui->context) {
		AeronFontAtlas_Release(&ui->font);
		if (error && error_capacity)
			snprintf(error, error_capacity, "application UI context creation failed");
		return false;
	}
	AeronUi_SetFonts(ui->context, &(AeronUiFontSet) {
									  .regular = &ui->font,
									  .title = &ui->font,
								  });
	/* The text is drawn 2 to 5 times smaller than the font atlas. A negative
	   bias reads a larger mip level, which keeps it sharper. */
	AeronUiTheme theme = *AeronUi_GetTheme(ui->context);
	theme.text_mip_bias = -0.75f;
	AeronUi_SetTheme(ui->context, &theme);
	return true;
}

void XwaAppUi_Shutdown(XwaAppUi* ui) {
	if (!ui)
		return;
	AeronUi_Destroy(ui->context);
	AeronFontAtlas_Release(&ui->font);
	memset(ui, 0, sizeof *ui);
}

AeronUiContext* XwaAppUi_Context(XwaAppUi* ui) { return ui ? ui->context : NULL; }
