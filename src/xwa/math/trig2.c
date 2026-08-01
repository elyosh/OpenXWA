#include "xwa/math/trig2.h"

#include <math.h>

// GLOBAL: XWA 0x7B7004
int trig2_xmovedist;
// GLOBAL: XWA 0x7CAB58
uint16_t trig2_xyangle;
// GLOBAL: XWA 0x7D4B60
int trig2_polardistance;
// GLOBAL: XWA 0x7D4C50
int trig2_xoffset;
// GLOBAL: XWA 0x7FBB6C
int trig2_ymovedist;
// GLOBAL: XWA 0x7FFB68
uint16_t trig2_phi;
// GLOBAL: XWA 0x8052A4
int trig2_rho;
// GLOBAL: XWA 0x8B94B8
int trig2_yoffset;
// GLOBAL: XWA 0x8BF364
uint16_t trig2_signswap;
// GLOBAL: XWA 0x8C1CEA
uint16_t trig2_signx;
// GLOBAL: XWA 0x8C1CEC
uint16_t trig2_signy;
// GLOBAL: XWA 0x8C1CF4
uint16_t trig2_signz;
// GLOBAL: XWA 0x8D4444
int trig2_zmovedist;
// GLOBAL: XWA 0x8D9408
int trig2_zoffset;
// GLOBAL: XWA 0x8D962C
uint32_t trig2_divisorhilo;
// GLOBAL: XWA 0x91079C
uint16_t trig2_theta;
// GLOBAL: XWA 0x910DF6
uint16_t trig2_angleplane;
// GLOBAL: XWA 0x910DE4
uint16_t targetPitch;

// GLOBAL: XWA 0x5A9F3C
const float g_trig2AngleToRadians = 0.000095873722f;
// GLOBAL: XWA 0x5A9F40
const float g_trig2Q15Scale = 32767.0f;

// GLOBAL: XWA 0x5FEFD8
static const uint16_t g_arctantable[258] = {
	0,    41,   81,   122,  163,  204,  244,  285,  326,  367,  407,  448,  489,  529,  570,  610,  651,
	692,  732,  773,  813,  854,  894,  935,  975,  1015, 1056, 1096, 1136, 1177, 1217, 1257, 1297, 1337,
	1377, 1417, 1457, 1497, 1537, 1577, 1617, 1656, 1696, 1736, 1775, 1815, 1854, 1894, 1933, 1973, 2012,
	2051, 2090, 2129, 2168, 2207, 2246, 2285, 2324, 2363, 2401, 2440, 2478, 2517, 2555, 2593, 2632, 2670,
	2708, 2746, 2784, 2822, 2860, 2897, 2935, 2973, 3010, 3047, 3085, 3122, 3159, 3196, 3233, 3270, 3307,
	3344, 3380, 3417, 3453, 3490, 3526, 3562, 3598, 3634, 3670, 3706, 3742, 3778, 3813, 3849, 3884, 3920,
	3955, 3990, 4025, 4060, 4095, 4129, 4164, 4199, 4233, 4267, 4302, 4336, 4370, 4404, 4438, 4471, 4505,
	4539, 4572, 4605, 4639, 4672, 4705, 4738, 4771, 4803, 4836, 4869, 4901, 4933, 4966, 4998, 5030, 5062,
	5093, 5125, 5157, 5188, 5220, 5251, 5282, 5313, 5344, 5375, 5406, 5437, 5467, 5498, 5528, 5558, 5589,
	5619, 5649, 5679, 5708, 5738, 5768, 5797, 5826, 5856, 5885, 5914, 5943, 5972, 6000, 6029, 6057, 6086,
	6114, 6142, 6171, 6199, 6226, 6254, 6282, 6310, 6337, 6365, 6392, 6419, 6446, 6473, 6500, 6527, 6554,
	6580, 6607, 6633, 6660, 6686, 6712, 6738, 6764, 6790, 6815, 6841, 6867, 6892, 6917, 6943, 6968, 6993,
	7018, 7043, 7067, 7092, 7117, 7141, 7166, 7190, 7214, 7238, 7262, 7286, 7310, 7334, 7358, 7381, 7405,
	7428, 7451, 7474, 7498, 7521, 7544, 7566, 7589, 7612, 7634, 7657, 7679, 7702, 7724, 7746, 7768, 7790,
	7812, 7834, 7856, 7877, 7899, 7920, 7942, 7963, 7984, 8005, 8026, 8047, 8068, 8089, 8110, 8130, 8151,
	8172, 8192, 8192,
};

// GLOBAL: XWA 0x5FF1E0
const uint16_t g_squarerootable[258] = {
	0,     0,     2,     4,     8,     12,    18,    24,    32,    40,    50,    60,    72,    84,    98,
	112,   128,   144,   162,   180,   200,   220,   242,   264,   287,   312,   337,   363,   391,   419,
	448,   479,   510,   542,   575,   610,   645,   681,   718,   756,   795,   835,   876,   918,   961,
	1005,  1050,  1095,  1142,  1190,  1238,  1288,  1338,  1390,  1442,  1495,  1550,  1605,  1661,  1718,
	1776,  1835,  1895,  1955,  2017,  2079,  2143,  2207,  2273,  2339,  2406,  2474,  2543,  2612,  2683,
	2755,  2827,  2900,  2974,  3049,  3125,  3202,  3280,  3358,  3438,  3518,  3599,  3681,  3764,  3847,
	3932,  4017,  4103,  4190,  4278,  4367,  4456,  4547,  4638,  4730,  4822,  4916,  5010,  5105,  5201,
	5298,  5396,  5494,  5593,  5693,  5794,  5895,  5997,  6100,  6204,  6309,  6414,  6520,  6627,  6734,
	6843,  6952,  7061,  7172,  7283,  7395,  7508,  7621,  7735,  7850,  7966,  8082,  8199,  8317,  8435,
	8554,  8674,  8794,  8915,  9037,  9160,  9283,  9407,  9531,  9656,  9782,  9909,  10036, 10164, 10292,
	10421, 10551, 10681, 10812, 10944, 11076, 11209, 11343, 11477, 11612, 11747, 11883, 12019, 12157, 12294,
	12433, 12572, 12711, 12852, 12992, 13134, 13276, 13418, 13561, 13705, 13849, 13994, 14139, 14285, 14431,
	14578, 14726, 14874, 15022, 15171, 15321, 15471, 15622, 15773, 15925, 16077, 16230, 16384, 16537, 16692,
	16847, 17002, 17158, 17314, 17471, 17629, 17786, 17945, 18104, 18263, 18423, 18583, 18744, 18905, 19066,
	19229, 19391, 19554, 19718, 19882, 20046, 20211, 20376, 20542, 20708, 20875, 21042, 21209, 21377, 21546,
	21714, 21884, 22053, 22223, 22394, 22565, 22736, 22908, 23080, 23252, 23425, 23599, 23772, 23946, 24121,
	24296, 24471, 24647, 24823, 24999, 25176, 25353, 25531, 25709, 25887, 26066, 26245, 26424, 26604, 26784,
	26964, 27146, 0,
};

// GLOBAL: XWA 0x5FE8D0
static const uint16_t g_sinTable[513] = {
	0,     402,   804,   1206,  1608,  2010,  2412,  2814,  3216,  3617,  4019,  4420,  4821,  5222,  5623,
	6023,  6424,  6824,  7224,  7623,  8022,  8421,  8820,  9218,  9616,  10014, 10411, 10808, 11204, 11600,
	11996, 12391, 12785, 13180, 13573, 13966, 14359, 14751, 15143, 15534, 15924, 16314, 16703, 17091, 17479,
	17867, 18253, 18639, 19024, 19409, 19792, 20175, 20557, 20939, 21320, 21699, 22078, 22457, 22834, 23210,
	23586, 23961, 24335, 24708, 25080, 25451, 25821, 26190, 26558, 26925, 27291, 27656, 28020, 28383, 28745,
	29106, 29466, 29824, 30182, 30538, 30893, 31248, 31600, 31952, 32303, 32652, 33000, 33347, 33692, 34037,
	34380, 34721, 35062, 35401, 35738, 36075, 36410, 36744, 37076, 37407, 37736, 38064, 38391, 38716, 39040,
	39362, 39683, 40002, 40320, 40636, 40951, 41264, 41576, 41886, 42194, 42501, 42806, 43110, 43412, 43713,
	44011, 44308, 44604, 44898, 45190, 45480, 45769, 46056, 46341, 46624, 46906, 47186, 47464, 47741, 48015,
	48288, 48559, 48828, 49095, 49361, 49624, 49886, 50146, 50404, 50660, 50914, 51166, 51417, 51665, 51911,
	52156, 52398, 52639, 52878, 53114, 53349, 53581, 53812, 54040, 54267, 54491, 54714, 54934, 55152, 55368,
	55582, 55794, 56004, 56212, 56418, 56621, 56823, 57022, 57219, 57414, 57607, 57798, 57986, 58172, 58356,
	58538, 58718, 58896, 59071, 59244, 59415, 59583, 59750, 59914, 60075, 60235, 60392, 60547, 60700, 60851,
	60999, 61145, 61288, 61429, 61568, 61705, 61839, 61971, 62101, 62228, 62353, 62476, 62596, 62714, 62830,
	62943, 63054, 63162, 63268, 63372, 63473, 63572, 63668, 63763, 63854, 63944, 64031, 64115, 64197, 64277,
	64354, 64429, 64501, 64571, 64639, 64704, 64766, 64827, 64884, 64940, 64993, 65043, 65091, 65137, 65180,
	65220, 65259, 65294, 65328, 65358, 65387, 65413, 65436, 65457, 65476, 65492, 65505, 65516, 65525, 65531,
	65533, 65534, 65533, 65531, 65525, 65516, 65505, 65492, 65476, 65457, 65436, 65413, 65387, 65358, 65328,
	65294, 65259, 65220, 65180, 65137, 65091, 65043, 64993, 64940, 64884, 64827, 64766, 64704, 64639, 64571,
	64501, 64429, 64354, 64277, 64197, 64115, 64031, 63944, 63854, 63763, 63668, 63572, 63473, 63372, 63268,
	63162, 63054, 62943, 62830, 62714, 62596, 62476, 62353, 62228, 62101, 61971, 61839, 61705, 61568, 61429,
	61288, 61145, 60999, 60851, 60700, 60547, 60392, 60235, 60075, 59914, 59750, 59583, 59415, 59244, 59071,
	58896, 58718, 58538, 58356, 58172, 57986, 57798, 57607, 57414, 57219, 57022, 56823, 56621, 56418, 56212,
	56004, 55794, 55582, 55368, 55152, 54934, 54714, 54491, 54267, 54040, 53812, 53581, 53349, 53114, 52878,
	52639, 52398, 52156, 51911, 51665, 51417, 51166, 50914, 50660, 50404, 50146, 49886, 49624, 49361, 49095,
	48828, 48559, 48288, 48015, 47741, 47464, 47186, 46906, 46624, 46341, 46056, 45769, 45480, 45190, 44898,
	44604, 44308, 44011, 43713, 43412, 43110, 42806, 42501, 42194, 41886, 41576, 41264, 40951, 40636, 40320,
	40002, 39683, 39362, 39040, 38716, 38391, 38064, 37736, 37407, 37076, 36744, 36410, 36075, 35738, 35401,
	35062, 34721, 34380, 34037, 33692, 33347, 33000, 32652, 32303, 31952, 31600, 31248, 30893, 30538, 30182,
	29824, 29466, 29106, 28745, 28383, 28020, 27656, 27291, 26925, 26558, 26190, 25821, 25451, 25080, 24708,
	24335, 23961, 23586, 23210, 22834, 22457, 22078, 21699, 21320, 20939, 20557, 20175, 19792, 19409, 19024,
	18639, 18253, 17867, 17479, 17091, 16703, 16314, 15924, 15534, 15143, 14751, 14359, 13966, 13573, 13180,
	12785, 12391, 11996, 11600, 11204, 10808, 10411, 10014, 9616,  9218,  8820,  8421,  8022,  7623,  7224,
	6824,  6424,  6023,  5623,  5222,  4821,  4420,  4019,  3617,  3216,  2814,  2412,  2010,  1608,  1206,
	804,   402,   0,
};

// FUNCTION: XWA 0x4EAEA0
void trig2_calcarctan_core(int a, int b, int16_t* outRatio, int16_t* outAngle) {
	trig2_signswap = 0;

	if (a == b) {
		trig2_divisorhilo = (uint32_t)a;
		b = 0x100;
		a = 0;
	} else {
		if (a <= b) {
			int tmp;

			tmp = b;
			b = a;
			a = tmp;
			trig2_signswap = 1;
		}

		trig2_divisorhilo = (uint32_t)a;
		if (a != 0) {
			if ((a & 0xff000000) == 0) {
#ifdef XWA_MODERN
				a = (int)((uint32_t)a << 8);
				b = (int)((uint32_t)b << 8);
#else
				a <<= 8;
				b <<= 8;
#endif
				if ((a & 0xff000000) == 0) {
#ifdef XWA_MODERN
					a = (int)((uint32_t)a << 8);
					b = (int)((uint32_t)b << 8);
#else
					a <<= 8;
					b <<= 8;
#endif
				}
			}

			if (a == b) {
				a = (int)((uint32_t)b >> 16);
				b = 0x100;
			} else {
				b = (int)((uint32_t)b / ((uint32_t)a >> 16));
				b &= 0xffff;
				a = (uint8_t)b << 8;
				b >>= 8;
			}
		} else {
			a = (int)((uint32_t)b >> 16);
			b = 0;
		}
	}

	a &= 0xff00;
	*outAngle = (int16_t)b;
	*outRatio = (int16_t)g_arctantable[(b & 0xffff) + 1];
	*outRatio = (int16_t)(*outRatio - g_arctantable[(uint16_t)*outAngle]);
	*outRatio = (int16_t)((uint32_t)((*outRatio & 0xffff) * a) >> 16);
	*outRatio = (int16_t)(g_arctantable[(uint16_t)*outAngle] + *outRatio);

	if (trig2_signswap) {
		*outRatio = (int16_t)(0x4000 - *outRatio);
	}
}

// FUNCTION: XWA 0x4EAF90
int trig2_arctan(int y, int x) {
	int16_t ratio;
	int16_t angle;
	int result;

	if (y < 0) {
		trig2_signy = 1;
		y = -y;
	} else {
		trig2_signy = 0;
	}
	if (x < 0) {
		trig2_signx = 1;
		x = -x;
	} else {
		trig2_signx = 0;
	}

	trig2_calcarctan_core(x, y, &ratio, &angle);

#ifdef XWA_MODERN
	result = ratio;
#else
	result = *(int*)&ratio;
#endif
	if (trig2_signy) {
		result = -result;
	}
	if (trig2_signx) {
		result = 0x8000 - result;
	}

	return result;
}

// FUNCTION: XWA 0x4EA600
uint16_t trig2_arcsin(int sinQ15) {
	int16_t tableIndex;
	int16_t input;
	int magnitude;
	int16_t remainingSteps;
	int16_t interp;
	uint16_t result;
	int quotient;
	union {
		int value;
		uint8_t bytes[4];
	} quotientBytes;

	input = (int16_t)sinQ15;
	magnitude = sinQ15;
	if (input < 0) {
		tableIndex = 256;
		magnitude = (int)(0u - (uint32_t)sinQ15);
	} else {
		tableIndex = 256;
	}
	magnitude = (int)((uint32_t)magnitude << 1);
	result = (uint16_t)magnitude;

	remainingSteps = 256;
	while (1) {
		if (result >= g_sinTable[tableIndex]) {
			--remainingSteps;
			break;
		}

		--remainingSteps;
		++tableIndex;
		if (remainingSteps <= 0) {
			quotient = (int)((uint32_t)result << 16) / g_sinTable[tableIndex - 1];
			quotientBytes.value = quotient;
			result = (uint16_t)(0 - quotientBytes.bytes[1]);
			if (result != 0) {
				result >>= 2;
			} else {
				result = 0x4000;
			}
			if (input < 0) {
				result = (uint16_t)-result;
				result = (uint16_t)(result + 0x8000);
			}
			return result;
		}
	}

	result = (uint16_t)(result - g_sinTable[tableIndex - 1]);
	if (result != 0) {
		quotient = (int)((uint32_t)result << 16) /
				   (uint16_t)(g_sinTable[tableIndex - 2] - g_sinTable[tableIndex - 1]);
		quotientBytes.value = quotient;
		interp = quotientBytes.bytes[1];
	} else {
		interp = 0;
	}

	result = (uint16_t)(255 - remainingSteps);
	result = (uint16_t)(result << 8);
	result = (uint16_t)(result - interp);
	result >>= 2;
	if (input < 0) {
		result = (uint16_t)-result;
		result = (uint16_t)(result + 0x8000);
	}
	return result;
}

// FUNCTION: XWA 0x4EA5F0
int trig2_w_arcsin(int sinQ15) { return trig2_arcsin(sinQ15); }

// FUNCTION: XWA 0x4EAC30
void trig2_ctop(int dx, int dy, int dz) {
	int16_t xyRatio;
	int16_t xyAngle;
	int16_t pitchRatio;
	int16_t pitchAngle;
	uint32_t root;
	uint32_t divisor;
	uint32_t high;
	uint32_t low;
	uint32_t distance;

	if (dx < 0) {
		trig2_signx = 1;
		dx = -dx;
	} else {
		trig2_signx = 0;
	}

	trig2_xoffset = dx;
	if (dy < 0) {
		trig2_signy = 1;
		dy = -dy;
	} else {
		trig2_signy = 0;
	}

	trig2_yoffset = dy;
	if (dz < 0) {
		trig2_signz = 1;
		dz = -dz;
	} else {
		trig2_signz = 0;
	}

	trig2_zoffset = dz;
	trig2_calcarctan_core(dx, dy, &xyRatio, &xyAngle);
	trig2_angleplane = (uint16_t)xyRatio;
	root = g_squarerootable[(uint16_t)xyAngle];
	divisor = trig2_divisorhilo;
	high = divisor >> 16;
	low = divisor & 0xffffu;
	distance = root * high + divisor + (((low * root + 0x8000u) >> 16) & 0xffffu);
	trig2_polardistance = (int)distance;

	if (trig2_signy) {
		xyRatio = (int16_t)-xyRatio;
	}
	if (trig2_signx) {
		xyRatio = (int16_t)(0x8000u - (uint16_t)xyRatio);
	}
	trig2_xyangle = (uint16_t)(0x4000u - (uint16_t)xyRatio);

	trig2_calcarctan_core(trig2_polardistance, trig2_zoffset, &pitchRatio, &pitchAngle);
	root = g_squarerootable[(uint16_t)pitchAngle];
	divisor = trig2_divisorhilo;
	high = divisor >> 16;
	low = divisor & 0xffffu;
	distance = root * high + divisor + (((low * root + 0x8000u) >> 16) & 0xffffu);
	trig2_angleplane = (uint16_t)pitchRatio;
	trig2_polardistance = (int)distance;

	if (trig2_signz) {
		pitchRatio = (int16_t)-pitchRatio;
	}
	targetPitch = (uint16_t)(0x4000u - (uint16_t)pitchRatio);
}

// FUNCTION: XWA 0x4EADC0
void trig2_ctop2dim(int dx, int dy) {
	int ratio;
	int16_t angle;
	uint32_t root;
	uint32_t divisor;
	uint32_t distance;

	if (dx < 0) {
		trig2_signx = 1;
		dx = -dx;
	} else {
		trig2_signx = 0;
	}

	if (dy < 0) {
		trig2_signy = 1;
		dy = -dy;
	} else {
		trig2_signy = 0;
	}

	trig2_calcarctan_core(dx, dy, (int16_t*)&ratio, &angle);
	divisor = trig2_divisorhilo;
	root = g_squarerootable[(uint16_t)angle];
	distance = (((root * (divisor & 0xffffu) + 0x8000u) >> 16) & 0xffffu) + divisor + (divisor >> 16) * root;
	trig2_angleplane = (uint16_t)ratio;
	trig2_polardistance = (int)distance;

	if (trig2_signy) {
		ratio = -ratio;
	}
	if (trig2_signx) {
		ratio = (uint16_t)(0x8000u - (uint16_t)ratio);
	}
	trig2_xyangle = (uint16_t)(0x4000u - (uint16_t)ratio);
}

// FUNCTION: XWA 0x4EA5D0
int trig2_getsignedsin(int angle) {
	angle = (int16_t)angle;
	return (int)(sin(angle * g_trig2AngleToRadians) * g_trig2Q15Scale);
}

// FUNCTION: XWA 0x4EA7C0
int trig2_getsignedcos(int angle) {
	angle = (int16_t)angle;
	return (int)(cos(angle * g_trig2AngleToRadians) * g_trig2Q15Scale);
}

// FUNCTION: XWA 0x4EA570
int trig2_calcsineofangle(int angle) {
	uint16_t index;
	uint16_t base;
	uint16_t magnitude;
	int16_t signedDelta;

	index = (uint16_t)(((uint32_t)angle >> 6) & 0x1ffu);
	base = g_sinTable[index];
	magnitude = (uint16_t)(g_sinTable[index + 1] - base);
	signedDelta = (int16_t)magnitude;
	if (signedDelta < 0) {
		magnitude = (uint16_t)-magnitude;
	}

	angle = (int)(((uint32_t)(uint16_t)((uint32_t)angle << 10) * magnitude) >> 16);
	if (signedDelta < 0) {
		angle = -angle;
	}

	return (int)base + angle;
}

// FUNCTION: XWA 0x4EA710
int trig2_sinewordmult(int value, int angle) {
	uint16_t sign;
	uint16_t signDiff;
	uint32_t product;

	sign = (uint16_t)value & 0x8000u;
	if (sign) {
		value = (int)(0u - (uint32_t)value);
	}

	signDiff = sign ^ ((uint16_t)angle & 0x8000u);
	sign = g_sinTable[((uint32_t)angle >> 6) & 0x1ffu];
	product = (uint32_t)sign * (uint16_t)value + 0x8000u;
	if (signDiff) {
		product = 0u - product;
	}

	return (int)(product >> 16);
}

// FUNCTION: XWA 0x4EA7E0
int trig2_cosinewordmult(int value, int angle) {
	uint32_t anglePlus;
	uint16_t valueWord;
	uint16_t valueSign;
	uint16_t magnitude;
	uint16_t signDiff;
	uint32_t product;

	valueWord = (uint16_t)value;
	valueSign = valueWord & 0x8000u;
	magnitude = valueWord;
	if (valueSign) {
		magnitude = (uint16_t)(0u - (uint32_t)value);
	}

	anglePlus = (uint32_t)angle + 0x4000u;
	signDiff = valueSign ^ ((uint16_t)anglePlus & 0x8000u);
	valueSign = g_sinTable[(anglePlus >> 6) & 0x1ffu];
	product = (uint32_t)valueSign * magnitude + 0x8000u;
	if (signDiff) {
		product = 0u - product;
	}

	return (int)(uint16_t)(product >> 16);
}

// FUNCTION: XWA 0x4EA760
__inline int trig2_sinedwordmult(int value, int angle) {
	uint32_t sign;
	uint16_t tableValue;
	uint32_t lo;

	sign = 0;
	if (value < 0) {
		sign = 0x8000u;
		value = -value;
	}

	tableValue = g_sinTable[((uint32_t)angle >> 6) & 0x1ffu];
	sign ^= (uint32_t)angle;
	lo = (((uint32_t)value & 0xffffu) * tableValue + 0x8000u) >> 16;
	value = (int)((uint32_t)value >> 16);
	value = (int)((uint32_t)value * tableValue + lo);
	if (sign & 0x8000u) {
		return -value;
	}
	return value;
}

// FUNCTION: XWA 0x4EA840
__inline int trig2_cosinedwordmult(int value, int angle) {
	uint32_t sign;
	uint16_t tableValue;
	uint32_t lo;

	sign = 0;
	if (value < 0) {
		sign = 0x8000u;
		value = -value;
	}

	angle += 0x4000;
	tableValue = g_sinTable[((uint32_t)angle >> 6) & 0x1ffu];
	sign ^= (uint32_t)angle;
	lo = (((uint32_t)value & 0xffffu) * tableValue + 0x8000u) >> 16;
	value = (int)((uint32_t)value >> 16);
	value = (int)((uint32_t)value * tableValue + lo);
	if (sign & 0x8000u) {
		return -value;
	}
	return value;
}

// FUNCTION: XWA 0x4EAA60
void trig2_movexyz(int distance, int pitch, int heading) {
	int horizontalDistance;

	distance &= 0xffff;
	pitch = (uint16_t)(0x4000 - pitch);

	trig2_rho = distance;
	trig2_theta = (uint16_t)pitch;
	trig2_phi = (uint16_t)heading;

	horizontalDistance = trig2_sinedwordmult(distance, heading);
	trig2_xoffset = trig2_cosinedwordmult(horizontalDistance, pitch);
	trig2_yoffset = trig2_sinedwordmult(horizontalDistance, pitch);
	trig2_zoffset = trig2_cosinedwordmult(distance, heading);

	trig2_xmovedist = trig2_xoffset;
	trig2_ymovedist = trig2_yoffset;
	trig2_zmovedist = trig2_zoffset;
}

// FUNCTION: XWA 0x4EA8B0
void trig2_ptoc3dim(void) {
	uint16_t phi;
	int horizontalDistance;

	{
		uint32_t sign;
		uint16_t tableValue;
		uint32_t lowProduct;

		horizontalDistance = trig2_rho;
		sign = 0;
		if (horizontalDistance < 0) {
			sign = 0x8000u;
			horizontalDistance = -horizontalDistance;
		}

		phi = trig2_phi;
		tableValue = g_sinTable[((uint32_t)phi >> 6) & 0x1ffu];
		sign ^= (uint32_t)phi;
		lowProduct = (((uint32_t)horizontalDistance & 0xffffu) * tableValue + 0x8000u) >> 16;
		horizontalDistance = (int)((uint32_t)horizontalDistance >> 16);
		horizontalDistance = (int)((uint32_t)horizontalDistance * tableValue + lowProduct);
		if (sign & 0x8000u) {
			horizontalDistance = -horizontalDistance;
		}
	}

	trig2_xoffset = trig2_cosinedwordmult(horizontalDistance, trig2_theta);
	trig2_yoffset = trig2_sinedwordmult(horizontalDistance, trig2_theta);
	trig2_zoffset = trig2_cosinedwordmult(trig2_rho, phi);
}
