#include "xwa/render/renderer_internal.h"

#include "xwa/math/fixed.h"

// FUNCTION: XWA 0x4EA1A0
void TRANSFM2_clipobjecteyez(int x, int y, int z) {
	int negViewZ;

	negViewZ = -viewZ;
	if (x > viewX) {
		viewX += MATH2_ABoverC32(negViewZ, x - viewX, z + negViewZ);
	} else {
		viewX -= MATH2_ABoverC32(negViewZ, viewX - x, z + negViewZ);
	}

	if (y > viewY) {
		viewY += MATH2_ABoverC32(negViewZ, y - viewY, z + negViewZ);
		viewZ = 1;
	} else {
		viewY -= MATH2_ABoverC32(negViewZ, viewY - y, z + negViewZ);
		viewZ = 1;
	}
}

// FUNCTION: XWA 0x4EA250
float TRANSFM2_ViewTransformX(float x, float y, float z) {
	return z * g_viewMtx02 + (y * g_viewMtx01 + x * g_viewMtx00);
}

// FUNCTION: XWA 0x4EA280
float TRANSFM2_ViewTransformY(float x, float y, float z) {
	float result = y * g_viewMtx11;

	return result + z * g_viewMtx12 + x * g_viewMtx10;
}

// FUNCTION: XWA 0x4EA2B0
float TRANSFM2_ViewTransformZ(float x, float y, float z) {
	return y * g_viewMtx21 + x * g_viewMtx20 + z * g_viewMtx22;
}

// FUNCTION: XWA 0x4EA2E0
int TRANSFM2_CamMatDotRow0(int x, int y, int z) {
	return Xwa_Q15Mul(x, g_camMatR0_X) + Xwa_Q15Mul(y, g_camMatR0_Y) + Xwa_Q15Mul(z, g_camMatR0_Z);
}

// FUNCTION: XWA 0x4EA320
int TRANSFM2_CamMatDotRow1(int x, int y, int z) {
	return Xwa_Q15Mul(x, g_camMatR1_X) + Xwa_Q15Mul(y, g_camMatR1_Y) + Xwa_Q15Mul(z, g_camMatR1_Z);
}

// FUNCTION: XWA 0x4EA360
int TRANSFM2_CamMatDotRow2(int x, int y, int z) {
	return Xwa_Q15Mul(x, g_camMatR2_X) + Xwa_Q15Mul(y, g_camMatR2_Y) + Xwa_Q15Mul(z, g_camMatR2_Z);
}

// FUNCTION: XWA 0x4EA3A0
int TRANSFM2_ProjectScreenX(int viewX, int viewZ) {
	int projectedX;

	projectedX = viewX;
	if (viewZ > 0) {
		projectedX = (int)(((double)(uint32_t)g_projScaleInt / (double)viewZ) * (double)viewX);
	} else if (viewZ < 0) {
		projectedX = 0;
	}

	return g_flightVpCenterX + projectedX;
}

// FUNCTION: XWA 0x4EA400
int TRANSFM2_ProjectScreenY(int viewY, int viewZ) {
	int projectedY;

	projectedY = viewY;
	if (viewZ > 0) {
		projectedY = (int)(((double)(uint32_t)g_projScaleInt / (double)viewZ) * (double)viewY);
	} else if (viewZ < 0) {
		projectedY = 0;
	}

	if (g_projAspectY) {
		if (projectedY < 0) {
			projectedY = -(int)MATH2_longfraction((uint32_t)-projectedY, (uint16_t)g_projAspectY);
		} else {
			projectedY = (int)MATH2_longfraction((uint32_t)projectedY, (uint16_t)g_projAspectY);
		}
	}

	return g_projOffsetY + g_flightVpCenterY + projectedY;
}

// FUNCTION: XWA 0x4EA480
int TRANSFM2_ProjectStarfieldScreenX(int viewX, int viewZ) {
	int projectedX;

	if (viewZ > 0) {
		projectedX = (int)(((double)(uint32_t)g_projScaleInt / (double)viewZ) * (double)viewX);
	} else if (viewZ < 0) {
		projectedX = 0;
	} else {
		return g_flightVpCenterX + viewX;
	}

	return g_flightVpCenterX + projectedX;
}

// FUNCTION: XWA 0x4EA4F0
int TRANSFM2_ProjectStarfieldScreenY(int viewY, int viewZ) {
	if (viewZ > 0) {
		viewY = (int)(((double)(uint32_t)g_projScaleInt / (double)viewZ) * (double)viewY);
	} else if (viewZ < 0) {
		viewY = 0;
	}

	if (g_projAspectY) {
		if (viewY < 0) {
			viewY = -(int)MATH2_longfraction((uint32_t)-viewY, (uint16_t)g_projAspectY);
		} else {
			viewY = (int)MATH2_longfraction((uint32_t)viewY, (uint16_t)g_projAspectY);
		}
	}

	return g_projOffsetY + g_flightVpCenterY + viewY;
}
