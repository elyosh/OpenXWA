#ifndef XWA_AUDIO_CD_AUDIO_H
#define XWA_AUDIO_CD_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

int CDAudio_CloseDevice(void);
int CDAudio_SuspendPlayback(void);
int CDAudio_RequestResumePlayback(void);

#ifdef __cplusplus
}
#endif

#endif
