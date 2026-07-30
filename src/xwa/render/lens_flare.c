#include "xwa/flight/fediskio.h"
#include "xwa/flight/net_session.h"
#include "xwa/render/renderer_internal.h"

// FUNCTION: XWA 0x4942E0
void LensFlare_InitQueue(void) {
	uint16_t defaultSize;
	int sourceIndex;

	g_lensFlareQueueCount = 0;
	defaultSize = (uint16_t)(g_projScaleDiv512 * 256.0f);
	for (sourceIndex = 0; sourceIndex < 4; ++sourceIndex) {
		int quadIndex;
		uint16_t size96;

		for (quadIndex = 0; quadIndex < 7; ++quadIndex) {
			g_lensFlareQueue[sourceIndex].quads[quadIndex].depthZ = 1;
			g_lensFlareQueue[sourceIndex].quads[quadIndex].rotationAngle = 0;
			g_lensFlareQueue[sourceIndex].quads[quadIndex].screenSize = defaultSize;
		}

		g_lensFlareQueue[sourceIndex].quads[1].screenSize = (uint16_t)(g_projScaleDiv512 * 160.0f);
		g_lensFlareQueue[sourceIndex].quads[2].screenSize = (uint16_t)(g_projScaleDiv512 * 128.0f);
		size96 = (uint16_t)(g_projScaleDiv512 * 96.0f);
		g_lensFlareQueue[sourceIndex].quads[3].screenSize = size96;
		g_lensFlareQueue[sourceIndex].quads[6].screenSize = size96;
	}
}

// FUNCTION: XWA 0x494420
void LensFlare_QueueSource(int argbColor) {
	int screenX;
	int screenY;
	int deltaX;
	int deltaY;
	int centerX;
	int centerY;
	LensFlareSource* source;

	if (!g_gameConfig.lensFlare[NetSession_GetPlayerCount() > 1]) {
		return;
	}
	if (g_lensFlareQueueCount == 4) {
		return;
	}
	if (viewZ <= 0) {
		return;
	}
	if (!g_useHardware3D) {
		return;
	}

	screenX = TRANSFM2_ProjectScreenX(viewX, viewZ);
	deltaX = screenX - g_flightVpCenterX;
	screenY = g_flightVpHeight - TRANSFM2_ProjectScreenY(viewY, viewZ);
	centerY = g_flightVpCenterY;
	deltaY = screenY - centerY;

	if (screenX < 0 || (unsigned int)screenX >= g_screenWidth || screenY < 0 ||
		(unsigned int)screenY >= g_screenHeight) {
		return;
	}

	source = &g_lensFlareQueue[(uint16_t)g_lensFlareQueueCount];
	source->quads[0].screenX = screenX - (deltaX >> 4);
	source->quads[0].screenY = screenY - (deltaY >> 4);
	source->quads[1].screenX = screenX - (deltaX >> 2);
	source->quads[1].screenY = screenY - (deltaY >> 2);

	centerX = g_flightVpCenterX;
	source->quads[6].screenX = centerX + (deltaX >> 1);
	source->quads[6].screenY = centerY + (deltaY >> 1);
	source->quads[3].screenX = centerX - (deltaX >> 2);
	source->quads[3].screenY = centerY - (deltaY >> 2);
	source->quads[4].screenX = centerX - (deltaX >> 1);
	source->quads[4].screenY = centerY - (deltaY >> 1);
	source->quads[5].screenX = centerX - deltaX;
	source->quads[5].screenY = centerY - deltaY;

	++g_lensFlareQueueCount;
	source->quads[2].screenX = centerX + (deltaX >> 3);
	source->quads[2].screenY = centerY + (deltaY >> 3);
	source->argbColor = argbColor;
}

// FUNCTION: XWA 0x494380
void LensFlare_RenderQueuedSources(void) {
	int flareIndex;
	int sourceIndex;

	for (flareIndex = 0; flareIndex < 7; ++flareIndex) {
		uint16_t frame;

		frame = (uint16_t)flareIndex;
		if (flareIndex == 3) {
			frame = 1;
		} else if (flareIndex == 6) {
			frame = 4;
		}
		sourceIndex = 0;
		if (sourceIndex < (uint16_t)g_lensFlareQueueCount) {
			int sourceCount;

			frame = (uint16_t)(frame + 3);
			do {
				FeDiskIo_SelectTextureFrame(OBJ_LightingEffectTextureGroup1000, frame, 256);
				RenderQuad_DrawModelTexture(OBJ_LightingEffectTextureGroup1000,
											&g_lensFlareQueue[sourceIndex].quads[flareIndex],
											g_lensFlareQueue[sourceIndex].argbColor);
				sourceCount = (unsigned int)g_lensFlareQueueCount & 0xffff;
				++sourceIndex;
			} while (sourceIndex < sourceCount);
		}
	}

	g_lensFlareQueueCount = 0;
}
