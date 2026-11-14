#include "xwa_runtime/audio/modern_audio.h"

#include "xwa/audio/sound.h"

/* Modern port module; no original counterpart. */

static int s_doppler;

void XwaModernAudio_SetDoppler(int enabled) {
	s_doppler = enabled != 0;
	XwaModernAudio_ApplyDoppler();
}

void XwaModernAudio_ApplyDoppler(void) {
	if (g_sound3DListener) {
		g_sound3DListener->lpVtbl->SetDopplerFactor(g_sound3DListener, s_doppler ? 1.0f : 0.0f, 0);
	}
}
