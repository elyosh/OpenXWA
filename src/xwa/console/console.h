#ifndef XWA_CONSOLE_CONSOLE_H
#define XWA_CONSOLE_CONSOLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t g_consoleEnabled;

void Console_RunAutoexec(void);
void Console_FreeMacros(void);
void Console_SetVar(int playerIdx, int varId, int value);
uint16_t Console_ApplyKeyMacro(uint16_t key, int playerIdx);
void FlightConsole_DrawHistory(void);
void FlightConsole_DrawPrompt(int playerIdx);

#ifdef __cplusplus
}
#endif

#endif
