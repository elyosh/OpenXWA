#include "xwa_runtime/audio/sound_slots.h"

#include "xwa/audio/sound.h"

#include <string.h>

/* Modern port module; no original counterpart. */

int XwaSoundSlots_FindInaudible(void) {
	DS3DVector listener;
	double bestRatio = 0.0;
	int best = -1;
	int slot;

	if (!g_sound3DEnabled || !g_sound3DListener ||
		g_sound3DListener->lpVtbl->GetPosition(g_sound3DListener, &listener) < 0) {
		return -1;
	}
	for (slot = 0; slot < g_maxActiveSounds; ++slot) {
		const ActiveSoundInstance* inst = &g_activeSoundInstances[slot];
		DSBufferCaps caps;
		DS3DVector position;
		uint32_t status;
		uint32_t mode;
		float maxDistance;
		double dx, dy, dz, ratio;

		if (inst->soundId == -1 || !inst->buffer || !inst->buffer3D || inst->priority >= 125) {
			continue;
		}
		if (inst->buffer->lpVtbl->GetStatus(inst->buffer, &status) < 0 || (status & DSBSTATUS_PLAYING) == 0 ||
			(status & DSBSTATUS_LOOPING) != 0) {
			continue;
		}
		/* Only these buffers are silent past their max distance. In 3D mode,
		   the game creates every sound effect with the flag. */
		memset(&caps, 0, sizeof(caps));
		caps.dwSize = sizeof(caps);
		if (inst->buffer->lpVtbl->GetCaps(inst->buffer, &caps) < 0 ||
			(caps.dwFlags & DSBCAPS_MUTE3DATMAXDISTANCE) == 0) {
			continue;
		}
		if (inst->buffer3D->lpVtbl->GetMode(inst->buffer3D, &mode) < 0 || mode == 2 /* DS3DMODE_DISABLE */ ||
			inst->buffer3D->lpVtbl->GetMaxDistance(inst->buffer3D, &maxDistance) < 0 || maxDistance <= 0.0f ||
			inst->buffer3D->lpVtbl->GetPosition(inst->buffer3D, &position) < 0) {
			continue;
		}
		/* The positions the mixer uses, set by the last 3D update. */
		dx = (double)position.x - listener.x;
		dy = (double)position.y - listener.y;
		dz = (double)position.z - listener.z;
		ratio = (dx * dx + dy * dy + dz * dz) / ((double)maxDistance * maxDistance);
		if (ratio >= 1.0 && ratio > bestRatio) {
			best = slot;
			bestRatio = ratio;
		}
	}
	return best;
}
