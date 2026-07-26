#ifndef XWA_FRONTEND_FRONTEND_DIALOG_H
#define XWA_FRONTEND_FRONTEND_DIALOG_H

#ifdef __cplusplus
extern "C" {
#endif

int FrontendDialog_ShowConfirmDialog(const char* line1, const char* line2, const char* line3,
									 const char* okayLabel, const char* cancelLabel);
#ifdef XWA_MODERN
int FrontendDialog_BeginConfirmDialog(const char* line1, const char* line2, const char* line3,
									  const char* okayLabel, const char* cancelLabel);
int FrontendDialog_TakeConfirmDialogResult(int* result);
#endif
int FrontendDialog_ConfirmUpdateCallback(int frameState);
int FrontendDialog_HasNetworkDismissPacket(void);
int FrontendDialog_PromptForPilotName(char* outName);
int FrontendDialog_EditText(char* text, unsigned int maxChars, const char* promptText);
int FrontendDialog_EditTextCallback(int frameState);
/* Returns -1 while the nonblocking modal is still active. */
int FrontendDialog_ShowDeathStarTourOutcome(void);

#ifdef __cplusplus
}
#endif

#endif
