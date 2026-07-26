#include "xwa/console/console.h"

#include "xwa/flight/death_star.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/random.h"

// GLOBAL: XWA 0x7B33A4
uint8_t g_consoleEnabled;

// FUNCTION: XWA 0x414BD0
void Console_RunAutoexec(void) {
	/* TODO: Reimplement legacy debug-console autoexec macro loading if needed by the port. */
}

// FUNCTION: XWA 0x414EE0
void Console_FreeMacros(void) {
	/* TODO: Reimplement legacy debug-console macro cleanup if macro loading is restored. */
}

// FUNCTION: XWA 0x414870
void Console_SetVar(int playerIdx, int varId, int value) {
	switch (varId) {
		case 0:
			g_players[playerIdx].mfd.enabled[1] = value != 0;
			break;
		case 1:
			g_players[playerIdx].yawRollSwap = value != 0;
			break;
		case 2:
			g_deathStarLaserGlowExtent = value;
			break;
		case 3:
			g_padlockMouseLookInvertPitch = value != 0;
			break;
		case 4:
			if (value == 2 || value == 3 || value == 6) {
				(void)GameRand2();
			}
			/* Legacy DirectInput force-feedback/debug test command. */
			break;
		default:
			break;
	}
}

// FUNCTION: XWA 0x413F70
uint16_t Console_ApplyKeyMacro(uint16_t key, int playerIdx) {
	(void)playerIdx;

	/* Legacy debug-console macro AST evaluation is not part of the modern port. */
	return key;
}

// FUNCTION: XWA 0x412700
void FlightConsole_DrawHistory(void) {
	/* TODO: Reimplement legacy debug-console history rendering if the console UI is restored. */
}

// FUNCTION: XWA 0x412790
void FlightConsole_DrawPrompt(int playerIdx) {
	(void)playerIdx;

	/* TODO: Reimplement legacy debug-console prompt rendering if the console UI is restored. */
}
