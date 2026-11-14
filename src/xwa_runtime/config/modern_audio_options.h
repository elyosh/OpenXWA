#ifndef XWA_RUNTIME_MODERN_AUDIO_OPTIONS_H
#define XWA_RUNTIME_MODERN_AUDIO_OPTIONS_H

#include "xwa_runtime/config/option_section.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The audio options. They are read from the `audio` section of the shipped
 * remaster/config.yaml, which has every key, and of the player's
 * config.yaml, which can override each of them. Mods don't change them. The
 * Audio tab of OpenXWA Options changes them and saves them in the player's
 * config.yaml. */

typedef enum XwaModernAudioOutput {
	XWA_MODERN_AUDIO_SPEAKERS = 0,
	XWA_MODERN_AUDIO_HEADPHONES,
} XwaModernAudioOutput;

typedef enum XwaModernSpeakerLayout {
	XWA_MODERN_SPEAKERS_STEREO = 0,
	XWA_MODERN_SPEAKERS_5_1,
	XWA_MODERN_SPEAKERS_7_1,
} XwaModernSpeakerLayout;

/* The fields have the names of the keys. */
typedef struct XwaModernAudioOptions {
	int output; /* XwaModernAudioOutput */
	/* XwaModernSpeakerLayout. A change applies the next time OpenXWA
	   starts, because the output's channel count can't change while sounds
	   exist. */
	int speaker_layout;
	int distance_filter;
	int doppler;
} XwaModernAudioOptions;

XwaOptionSection* XwaModernAudioOptions_Section(void);

/* The channel count of the output for `options`: 2 for headphones, or the
 * speaker layout's. When the device has fewer channels, the largest layout
 * that fits it: a 5.1 output on a 2-channel device would only be mixed down
 * again. `device_channels` is 0 when unknown. */
int XwaModernAudioOptions_OutputChannels(const XwaModernAudioOptions* options, int device_channels);
/* How many speaker layouts fit a device with `device_channels` channels, 0
   when unknown: stereo, then 5.1, then 7.1. */
int XwaModernAudioOptions_LayoutsForDevice(int device_channels);

typedef void (*XwaModernAudioOptionsApplyFn)(const XwaModernAudioOptions* options);
typedef int (*XwaModernAudioOptionsPersistFn)(const XwaModernAudioOptions* options, char* error,
											  size_t error_size);

/* Starts from the values read from the config files. `apply` runs on each
   change. */
void XwaModernAudioOptions_Configure(XwaModernAudioOptionsApplyFn apply,
									 XwaModernAudioOptionsPersistFn persist);
void XwaModernAudioOptions_Get(XwaModernAudioOptions* out);
int XwaModernAudioOptions_Set(const XwaModernAudioOptions* options);
/* Saves the options if they changed. On failure, FlushWithError writes the
   reason in `error`, and Flush logs it. */
int XwaModernAudioOptions_FlushWithError(char* error, size_t error_size);
int XwaModernAudioOptions_Flush(void);
int XwaModernAudioOptions_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif
