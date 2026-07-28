#ifndef XWA_RUNTIME_PORT_H
#define XWA_RUNTIME_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int XwaPort_Init(void);
void XwaPort_SetCommandLine(const char* commandLine);
void XwaPort_Tick(int32_t delta_us);
/* Select whether the compatibility renderer should produce a classic flight
 * frame. The port still runs the recovered render path for its view, audio, and
 * snapshot side effects; the DirectDraw/Direct3D shims discard only GPU work. */
void XwaPort_SetClassicFlightRenderingEnabled(int enabled);
/* Re-submits the last complete classic frame during a deliberate transition
 * before a newly enabled HD flight frame has been produced. */
void XwaPort_SubmitRetainedClassicFrame(void);
uint64_t XwaPort_GetClassicFlightFrameSerial(void);
/* Host-paused frame: the game tick is skipped (sim + snapshots
 * freeze); a required classic layer is re-submitted so CLASSIC/SPLIT
 * composition stays live under the shell's pause (Cmd+P). */
void XwaPort_PausedFrame(void);
/* Queues the original Scroll Lock flight action for keyboards without that key.
 * Returns nonzero when an active flight accepted the request. */
int XwaPort_QueueMouseLookToggle(void);
/* Latched release of the flight mouse capture (Ctrl+Alt+M), so the pointer
 * returns to the OS while flight keeps running. Cleared by the hotkey, by a
 * click inside the window, or when the session leaves flight. */
void XwaPort_ToggleMouseCapture(void);
int XwaPort_IsMouseCaptureSuspended(void);
/* Nonzero once the window has held input focus at least once since
 * XwaPort_Init. The frontend/movie focus pause is gated on this so a launch
 * where the OS never handed focus back does not begin as a silent freeze. */
int XwaPort_EverHadFocus(void);
void XwaPort_Shutdown(void);
int XwaPort_ShouldQuit(void);
int XwaPort_GetExitCode(void);
uint64_t XwaPort_NextWakeDeadlineUs(void);

#ifdef __cplusplus
}
#endif

#endif
