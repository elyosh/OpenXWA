#include "xwa/audio/imuse/imuse.h"

// GLOBAL: XWA 0x78A0F0
int g_imGroupVolume[16];
// GLOBAL: XWA 0x78A130
int g_imGainTable[16];

// FLAGS: /O2 /Og-
// FUNCTION: XWA 0x58DB00
int ImGroupsInit(void) {
	int i;

	for (i = 0; i < 16; ++i) {
		g_imGroupVolume[i] = 127;
		g_imGainTable[i] = 127;
	}
	return 0;
}

// FUNCTION: XWA 0x58DB40
int ImSetGroupVol(unsigned int group, int value) {
	int groupIndex;
	int previousValue;

	if (group >= 16) {
		return -5;
	}

	if (value == -1) {
		return g_imGroupVolume[group];
	}

	if ((unsigned int)value > 127u) {
		return -5;
	}

	previousValue = g_imGroupVolume[group];
	if (group == 0) {
		g_imGroupVolume[0] = value;
		g_imGainTable[0] = value;
		for (groupIndex = 1; groupIndex < 16; ++groupIndex) {
			g_imGainTable[groupIndex] = ((g_imGroupVolume[groupIndex] + 1) * value) >> 7;
		}
	} else {
		g_imGroupVolume[group] = value;
		g_imGainTable[group] = ((value + 1) * g_imGroupVolume[0]) >> 7;
	}

	ImIncBusyCount();
	ImApplyVolumes();
	ImDecBusyCount();
	return previousValue;
}

// FUNCTION: XWA 0x58DC16
int ImGetGroupGain(unsigned int group) {
	if (group >= 16) {
		return -5;
	}
	return g_imGainTable[group];
}
