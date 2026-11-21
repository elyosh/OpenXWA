#include "xwa_runtime/config/modern_audio_options.h"

#include "aeron/log.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

static const char* const s_outputs[] = { "speakers", "headphones", NULL };
static const char* const s_layouts[] = { "stereo", "5.1", "7.1", NULL };

static const XwaOptionKey s_keys[] = {
	XWA_OPTION_CHOICE(XwaModernAudioOptions, output, s_outputs),
	XWA_OPTION_CHOICE(XwaModernAudioOptions, speaker_layout, s_layouts),
	XWA_OPTION_SWITCH(XwaModernAudioOptions, distance_filter),
};

static XwaModernAudioOptions s_shipped;
static XwaModernAudioOptions s_mod;
static XwaModernAudioOptions s_player;

static XwaOptionSection s_section = {
	.name = "audio",
	.mod_file = NULL,
	.log_channel = "xwa.audio",
	.keys = s_keys,
	.key_count = sizeof s_keys / sizeof s_keys[0],
	.size = sizeof(XwaModernAudioOptions),
	.shipped = &s_shipped,
	.mod = &s_mod,
	.player = &s_player,
};

static struct {
	XwaModernAudioOptions options;
	XwaModernAudioOptionsApplyFn apply;
	XwaModernAudioOptionsPersistFn persist;
	int configured;
	int dirty;
} s_state;

XwaOptionSection* XwaModernAudioOptions_Section(void) { return &s_section; }

static int s_classic2DSound;

int XwaModernAudioOptions_Sound3D(void) { return !s_classic2DSound; }

void XwaModernAudioOptions_SetClassic2DSound(int classic) { s_classic2DSound = classic != 0; }

int XwaModernAudioOptions_LayoutsForDevice(int device_channels) {
	if (device_channels <= 0 || device_channels >= 8) {
		return 3;
	}
	return device_channels >= 6 ? 2 : 1;
}

int XwaModernAudioOptions_OutputChannels(const XwaModernAudioOptions* options, int device_channels) {
	static const int channels[] = { 2, 6, 8 };
	int layout;

	if (!options || options->output == XWA_MODERN_AUDIO_HEADPHONES) {
		return 2;
	}
	layout = options->speaker_layout;
	if (layout >= XwaModernAudioOptions_LayoutsForDevice(device_channels)) {
		layout = XwaModernAudioOptions_LayoutsForDevice(device_channels) - 1;
	}
	return channels[layout < 0 ? 0 : layout > 2 ? 2 : layout];
}

static int XwaModernAudioOptions_IsValid(const XwaModernAudioOptions* options) {
	return options && options->output >= XWA_MODERN_AUDIO_SPEAKERS &&
		   options->output <= XWA_MODERN_AUDIO_HEADPHONES &&
		   options->speaker_layout >= XWA_MODERN_SPEAKERS_STEREO &&
		   options->speaker_layout <= XWA_MODERN_SPEAKERS_7_1;
}

void XwaModernAudioOptions_Configure(XwaModernAudioOptionsApplyFn apply,
									 XwaModernAudioOptionsPersistFn persist) {
	memset(&s_state, 0, sizeof s_state);
	XwaOptionSection_Get(&s_section, &s_state.options);
	s_state.apply = apply;
	s_state.persist = persist;
	s_state.configured = 1;
}

void XwaModernAudioOptions_Get(XwaModernAudioOptions* out) {
	if (!out) {
		return;
	}
	if (s_state.configured) {
		*out = s_state.options;
	} else {
		XwaOptionSection_Get(&s_section, out);
	}
}

int XwaModernAudioOptions_Set(const XwaModernAudioOptions* options) {
	XwaModernAudioOptions normalized;

	if (!s_state.configured || !XwaModernAudioOptions_IsValid(options)) {
		return 0;
	}
	normalized = *options;
	normalized.distance_filter = normalized.distance_filter != 0;
	if (memcmp(&normalized, &s_state.options, sizeof normalized) == 0) {
		return 1;
	}
	s_state.options = normalized;
	s_state.dirty = 1;
	if (s_state.apply) {
		s_state.apply(&s_state.options);
	}
	return 1;
}

int XwaModernAudioOptions_FlushWithError(char* error, size_t error_size) {
	if (error && error_size) {
		error[0] = '\0';
	}
	if (!s_state.configured || !s_state.dirty) {
		return 1;
	}
	if (!s_state.persist || !s_state.persist(&s_state.options, error, error_size)) {
		if (error && error_size && !error[0]) {
			snprintf(error, error_size, "%s", "could not save the audio options to config.yaml");
		}
		return 0;
	}
	s_state.dirty = 0;
	return 1;
}

int XwaModernAudioOptions_Flush(void) {
	char error[512];

	if (!XwaModernAudioOptions_FlushWithError(error, sizeof error)) {
		Aeron_LogError("xwa.config", "%s", error);
		return 0;
	}
	return 1;
}

int XwaModernAudioOptions_IsDirty(void) { return s_state.dirty; }
