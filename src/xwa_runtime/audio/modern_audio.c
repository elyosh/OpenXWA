#include "xwa_runtime/audio/modern_audio.h"

#include "xwa/audio/sound.h"

/* Modern port module; no original counterpart. */

void XwaModernAudio_SetDoppler(int enabled) {
	if (g_sound3DListener) {
		g_sound3DListener->lpVtbl->SetDopplerFactor(g_sound3DListener, enabled ? 1.0f : 0.0f, 0);
	}
}
