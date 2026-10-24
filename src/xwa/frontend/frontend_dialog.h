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

/* The confirmations that a click on a button opens. */
typedef enum FrontendConfirmClick {
	FRONTEND_CONFIRM_CLICK_NONE = 0,
	FRONTEND_CONFIRM_CLICK_DELETE_FILM,
	FRONTEND_CONFIRM_CLICK_CLEAR_SKIRMISH,
	FRONTEND_CONFIRM_CLICK_LEAVE_HOSTED_SETUP,
	FRONTEND_CONFIRM_CLICK_LEAVE_JOINED_SETUP,
	FRONTEND_CONFIRM_CLICK_RESTART_HOSTED_BRIEFING,
	FRONTEND_CONFIRM_CLICK_LEAVE_JOINED_BRIEFING,
	FRONTEND_CONFIRM_CLICK_FLY_AGAIN,
	FRONTEND_CONFIRM_CLICK_DISCONNECT_DEBRIEF,
	FRONTEND_CONFIRM_CLICK_SKIP_MISSION,
} FrontendConfirmClick;

/* A confirmation that a click opens. The original shows it and waits for the
   answer in the same call. In the port the dialog runs over several frames,
   and the click happens on one frame only, so the caller can't just repeat
   its call. Call this on every frame of the screen, with `clicked` set on the
   frame of the click: it opens the dialog then and returns 0. On a later
   frame, it returns 1 if the player confirmed the dialog that `id` opened.
   `answered`, when not NULL, is set to 1 on the call that gets the answer,
   whether it is OK or Cancel. The lines are only used when `clicked` is
   set. */
int FrontendDialog_ConfirmClick(FrontendConfirmClick id, int clicked, int* answered, const char* line1,
								const char* line2, const char* line3, const char* okayLabel,
								const char* cancelLabel);
/* Returns 1 while the dialog that `id` opened waits for its answer to be
   taken. */
int FrontendDialog_ConfirmClickPending(FrontendConfirmClick id);
#endif
int FrontendDialog_ConfirmUpdateCallback(int frameState);
int FrontendDialog_CreatePilotNameCallback(int frameState);
int FrontendDialog_DeathStarTourOutcomeCallback(int frameState);
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
