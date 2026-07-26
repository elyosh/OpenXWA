#include "xwa/render/renderer_internal.h"

// FUNCTION: XWA 0x439B00
float Math3D_Dot3(const float* lhs, const float* rhs) {
	return rhs[1] * lhs[1] + rhs[2] * lhs[2] + lhs[0] * rhs[0];
}

// FUNCTION: XWA 0x439BA0
float Math3D_RotateVec3X(Vec3f* vec, Matrix3x3* matrix) {
	return matrix->m[6] * vec->z + vec->y * matrix->m[3] + matrix->m[0] * vec->x;
}

// FUNCTION: XWA 0x439BD0
float Math3D_RotateVec3Y(Vec3f* vec, Matrix3x3* matrix) {
	return vec->y * matrix->m[4] + vec->z * matrix->m[7] + matrix->m[1] * vec->x;
}

// FUNCTION: XWA 0x439C00
float Math3D_RotateVec3Z(Vec3f* vec, Matrix3x3* matrix) {
	return matrix->m[8] * vec->z + vec->y * matrix->m[5] + matrix->m[2] * vec->x;
}

// FUNCTION: XWA 0x439C30
Matrix3x3* Math3D_MulMatrix3x3(Matrix3x3* dst, Matrix3x3* rhs) {
	float old0;
	float old1;
	float old2;
	float old3;
	float old4;
	float old5;
	float old6;
	float old7;
	float old8;
	float new0;
	float new1;
	float new2;
	float new3;
	float new4;
	float new5;
	float new6;
	float new7;
	float new8;

	old0 = dst->m[0];
	old1 = dst->m[1];
	old2 = dst->m[2];
	old3 = dst->m[3];
	old4 = dst->m[4];
	old5 = dst->m[5];
	old6 = dst->m[6];
	old7 = dst->m[7];
	old8 = dst->m[8];

	new1 = old0 * rhs->m[1] + rhs->m[4] * old1 + rhs->m[7] * old2;
	new2 = old0 * rhs->m[2] + old1 * rhs->m[5] + old2 * rhs->m[8];
	new3 = rhs->m[0] * old3 + rhs->m[6] * old5 + rhs->m[3] * old4;
	new4 = rhs->m[7] * old5 + rhs->m[4] * old4 + rhs->m[1] * old3;
	new5 = old5 * rhs->m[8] + old3 * rhs->m[2] + old4 * rhs->m[5];
	new6 = rhs->m[0] * old6 + old8 * rhs->m[6] + old7 * rhs->m[3];
	new7 = old7 * rhs->m[4] + old8 * rhs->m[7] + old6 * rhs->m[1];
	new8 = old6 * rhs->m[2] + old8 * rhs->m[8] + old7 * rhs->m[5];
	new0 = old1 * rhs->m[3] + rhs->m[0] * old0 + rhs->m[6] * old2;

	dst->m[1] = new1;
	dst->m[2] = new2;
	dst->m[0] = new0;
	dst->m[3] = new3;
	dst->m[4] = new4;
	dst->m[5] = new5;
	dst->m[6] = new6;
	dst->m[7] = new7;
	dst->m[8] = new8;
	return dst;
}

// FUNCTION: XWA 0x439DB0
Matrix3x3* Math3D_MulMatrix3x3T(Matrix3x3* dst, Matrix3x3* rhs) {
	float old0;
	float old1;
	float old2;
	float old3;
	float old4;
	float old5;
	float old6;
	float old7;
	float old8;
	float new0;
	float new1;
	float new2;
	float new3;
	float new4;
	float new5;
	float new6;
	float new7;
	float new8;

	old0 = dst->m[0];
	old1 = dst->m[1];
	old2 = dst->m[2];
	old3 = dst->m[3];
	old4 = dst->m[4];
	old5 = dst->m[5];
	old6 = dst->m[6];
	old7 = dst->m[7];
	old8 = dst->m[8];

	new1 = rhs->m[0] * old1 + old7 * rhs->m[6] + old4 * rhs->m[3];
	new2 = rhs->m[0] * old2 + rhs->m[6] * old8 + rhs->m[3] * old5;
	new3 = old0 * rhs->m[1] + old6 * rhs->m[7] + old3 * rhs->m[4];
	new4 = old7 * rhs->m[7] + old1 * rhs->m[1] + old4 * rhs->m[4];
	new5 = rhs->m[7] * old8 + rhs->m[1] * old2 + rhs->m[4] * old5;
	new6 = old0 * rhs->m[2] + rhs->m[5] * old3 + rhs->m[8] * old6;
	new7 = rhs->m[5] * old4 + rhs->m[8] * old7 + rhs->m[2] * old1;
	new8 = rhs->m[2] * old2 + rhs->m[5] * old5 + rhs->m[8] * old8;
	new0 = old6 * rhs->m[6] + rhs->m[0] * old0 + rhs->m[3] * old3;

	dst->m[1] = new1;
	dst->m[2] = new2;
	dst->m[0] = new0;
	dst->m[3] = new3;
	dst->m[4] = new4;
	dst->m[5] = new5;
	dst->m[6] = new6;
	dst->m[7] = new7;
	dst->m[8] = new8;
	return dst;
}

// FUNCTION: XWA 0x439F30
Matrix3x3* Math3D_BuildAxisAngleMatrix(Matrix3x3* out, float* axisAngle) {
	float cosAngle;
	float sinAngle;
	float oneMinusCos;
	float axisX;
	float axisY;
	float axisZ;

	cosAngle = (float)cos(axisAngle[3]);
	sinAngle = (float)sin(axisAngle[3]);
	oneMinusCos = 1.0f - cosAngle;
	axisY = axisAngle[1];
	axisZ = axisAngle[2];
	axisX = axisAngle[0];

	out->m[1] = oneMinusCos * axisY * axisX + sinAngle * axisZ;
	out->m[0] = oneMinusCos * axisX * axisX + cosAngle;
	out->m[2] = oneMinusCos * axisZ * axisX - sinAngle * axisY;
	out->m[3] = oneMinusCos * axisY * axisX - sinAngle * axisZ;
	out->m[4] = oneMinusCos * axisY * axisY + cosAngle;
	out->m[5] = oneMinusCos * axisZ * axisY + sinAngle * axisX;
	out->m[6] = oneMinusCos * axisZ * axisX + sinAngle * axisY;
	out->m[7] = oneMinusCos * axisZ * axisY - sinAngle * axisX;
	out->m[8] = oneMinusCos * axisZ * axisZ + cosAngle;
	return out;
}

// FUNCTION: XWA 0x439B30
void Math3D_RotateVec3(Vec3f* vec, Matrix3x3* matrix) {
	float x;
	float y;
	float z;
	float m0;
	float m6;

	x = vec->x;
	y = vec->y;
	z = vec->z;
	m0 = matrix->m[0];
	m6 = matrix->m[6];
	vec->x = (m0 * x + m6 * z) + matrix->m[3] * y;
	vec->y = matrix->m[7] * z + matrix->m[4] * y + matrix->m[1] * x;
	vec->z = matrix->m[8] * z + matrix->m[5] * y + matrix->m[2] * x;
}
