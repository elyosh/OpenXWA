#include "xwa_runtime/hooks/orientation_hook.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define XWA_ORIENTATION_PI 3.14159265358979323846f
#define XWA_ORIENTATION_HALF_PI (XWA_ORIENTATION_PI * 0.5f)
#define XWA_ORIENTATION_BAM_TO_RAD (XWA_ORIENTATION_PI / 32767.0f)
#define XWA_ORIENTATION_RAD_TO_BAM (32767.0f / XWA_ORIENTATION_PI)
#define XWA_ORIENTATION_GIMBAL_EPSILON 1.0e-5f

static float XwaOrientation_ClampRadians(float angle) { return atan2f(sinf(angle), cosf(angle)); }

static int16_t XwaOrientation_RoundAngle(float angle) {
	int value = (int)roundf(angle);

	if (value > 32767) {
		value = 32767;
	} else if (value < -32767) {
		value = -32767;
	}

	return (int16_t)value;
}

static void XwaOrientation_ToRadians(XwaOrientationAngles angles, float* pitch, float* yaw, float* roll) {
	*yaw = XwaOrientation_ClampRadians(-(float)(int16_t)angles.yaw * XWA_ORIENTATION_BAM_TO_RAD);
	*pitch = XwaOrientation_ClampRadians(-XWA_ORIENTATION_HALF_PI -
										 (float)(int16_t)angles.pitch * XWA_ORIENTATION_BAM_TO_RAD);
	*roll = XwaOrientation_ClampRadians(-(float)(int16_t)angles.roll * XWA_ORIENTATION_BAM_TO_RAD);
}

static XwaOrientationAngles XwaOrientation_FromRadians(float pitch, float yaw, float roll) {
	XwaOrientationAngles result;
	int16_t headingXY;
	int16_t headingZ;
	int16_t headingRoll;

	headingXY = XwaOrientation_RoundAngle(XwaOrientation_ClampRadians(-yaw) * XWA_ORIENTATION_RAD_TO_BAM);
	headingZ = XwaOrientation_RoundAngle(XwaOrientation_ClampRadians(-XWA_ORIENTATION_HALF_PI - pitch) *
										 XWA_ORIENTATION_RAD_TO_BAM);
	headingRoll = XwaOrientation_RoundAngle(XwaOrientation_ClampRadians(-roll) * XWA_ORIENTATION_RAD_TO_BAM);

	/* The hook selects the equivalent XWA Euler representation offset by
	 * half a turn in yaw and roll. */
	result.yaw = (Q16Angle)((uint16_t)headingXY + 0x8000u);
	result.pitch = (Q16Angle)(uint16_t)(int16_t)-headingZ;
	result.roll = (Q16Angle)((uint16_t)headingRoll + 0x8000u);
	return result;
}

/*
 * Column-major 3x3 matrix. Each column is a current body axis in world space.
 * This layout is the portable equivalent of the DirectXMath hook's m.r axes.
 */
static void XwaOrientation_RotateLocal(float matrix[3][3], int axisColumn, float angle) {
	float axisX;
	float axisY;
	float axisZ;
	float cosine;
	float sine;
	float oneMinusCosine;
	float rotation[3][3];
	float rotated[3][3];
	int column;
	int row;

	if (angle == 0.0f) {
		return;
	}

	axisX = matrix[axisColumn][0];
	axisY = matrix[axisColumn][1];
	axisZ = matrix[axisColumn][2];
	cosine = cosf(angle);
	sine = sinf(angle);
	oneMinusCosine = 1.0f - cosine;

	rotation[0][0] = cosine + axisX * axisX * oneMinusCosine;
	rotation[0][1] = axisX * axisY * oneMinusCosine - axisZ * sine;
	rotation[0][2] = axisX * axisZ * oneMinusCosine + axisY * sine;
	rotation[1][0] = axisY * axisX * oneMinusCosine + axisZ * sine;
	rotation[1][1] = cosine + axisY * axisY * oneMinusCosine;
	rotation[1][2] = axisY * axisZ * oneMinusCosine - axisX * sine;
	rotation[2][0] = axisZ * axisX * oneMinusCosine - axisY * sine;
	rotation[2][1] = axisZ * axisY * oneMinusCosine + axisX * sine;
	rotation[2][2] = cosine + axisZ * axisZ * oneMinusCosine;

	for (column = 0; column < 3; ++column) {
		for (row = 0; row < 3; ++row) {
			rotated[column][row] = rotation[row][0] * matrix[column][0] +
								   rotation[row][1] * matrix[column][1] +
								   rotation[row][2] * matrix[column][2];
		}
	}
	memcpy(matrix, rotated, sizeof(rotated));
}

static void XwaOrientation_MatrixToQuaternion(const float matrix[3][3], float quaternion[4]) {
	float m00 = matrix[0][0];
	float m01 = matrix[1][0];
	float m02 = matrix[2][0];
	float m10 = matrix[0][1];
	float m11 = matrix[1][1];
	float m12 = matrix[2][1];
	float m20 = matrix[0][2];
	float m21 = matrix[1][2];
	float m22 = matrix[2][2];
	float trace = m00 + m11 + m22;
	float scale;

	if (trace > 0.0f) {
		scale = sqrtf(trace + 1.0f) * 2.0f;
		quaternion[3] = 0.25f * scale;
		quaternion[0] = (m21 - m12) / scale;
		quaternion[1] = (m02 - m20) / scale;
		quaternion[2] = (m10 - m01) / scale;
	} else if (m00 > m11 && m00 > m22) {
		scale = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
		quaternion[3] = (m21 - m12) / scale;
		quaternion[0] = 0.25f * scale;
		quaternion[1] = (m01 + m10) / scale;
		quaternion[2] = (m02 + m20) / scale;
	} else if (m11 > m22) {
		scale = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
		quaternion[3] = (m02 - m20) / scale;
		quaternion[0] = (m01 + m10) / scale;
		quaternion[1] = 0.25f * scale;
		quaternion[2] = (m12 + m21) / scale;
	} else {
		scale = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
		quaternion[3] = (m10 - m01) / scale;
		quaternion[0] = (m02 + m20) / scale;
		quaternion[1] = (m12 + m21) / scale;
		quaternion[2] = 0.25f * scale;
	}
}

static void XwaOrientation_QuaternionToEuler(const float quaternion[4], float* pitch, float* yaw,
											 float* roll) {
	float x = quaternion[0];
	float y = quaternion[1];
	float z = quaternion[2];
	float w = quaternion[3];
	float xx = x * x;
	float yy = y * y;
	float zz = z * z;
	float m31 = 2.0f * x * z + 2.0f * y * w;
	float m32 = 2.0f * y * z - 2.0f * x * w;
	float m33 = 1.0f - 2.0f * xx - 2.0f * yy;
	float cosYaw = sqrtf(m33 * m33 + m31 * m31);

	*pitch = atan2f(-m32, cosYaw);
	if (cosYaw > XWA_ORIENTATION_GIMBAL_EPSILON) {
		float m12 = 2.0f * x * y + 2.0f * z * w;
		float m22 = 1.0f - 2.0f * xx - 2.0f * zz;

		*yaw = atan2f(m31, m33);
		*roll = atan2f(m12, m22);
	} else {
		float m11 = 1.0f - 2.0f * yy - 2.0f * zz;
		float m21 = 2.0f * x * y - 2.0f * z * w;

		*yaw = 0.0f;
		*roll = atan2f(-m21, m11);
	}

	*pitch = XwaOrientation_ClampRadians(*pitch);
	*yaw = XwaOrientation_ClampRadians(*yaw);
	*roll = XwaOrientation_ClampRadians(*roll);
}

XwaOrientationAngles XwaOrientation_ApplyPitchYaw(XwaOrientationAngles current, int pitchDeltaQ16,
												  int negYawDeltaQ16) {
	float matrix[3][3] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
	};
	float quaternion[4];
	float pitch;
	float yaw;
	float roll;

	XwaOrientation_ToRadians(current, &pitch, &yaw, &roll);
	XwaOrientation_RotateLocal(matrix, 1, yaw);
	XwaOrientation_RotateLocal(matrix, 0, pitch);
	XwaOrientation_RotateLocal(matrix, 2, roll);

	XwaOrientation_RotateLocal(matrix, 1, -(float)negYawDeltaQ16 * XWA_ORIENTATION_BAM_TO_RAD);
	XwaOrientation_RotateLocal(matrix, 0, (float)pitchDeltaQ16 * XWA_ORIENTATION_BAM_TO_RAD);

	XwaOrientation_MatrixToQuaternion(matrix, quaternion);
	XwaOrientation_QuaternionToEuler(quaternion, &pitch, &yaw, &roll);
	return XwaOrientation_FromRadians(pitch, yaw, roll);
}
