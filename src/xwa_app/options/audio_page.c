#include "options/audio_page.h"

#include "aeron/audio.h"
#include "options/options_window.h"
#include "xwa_runtime/config/modern_audio_options.h"

#include <stdio.h>

/* Changes apply at once, except the speaker layout, and Close saves them. */

/* The system's own spatial audio for headphones would process the sound a
   second time. */
#if defined(_WIN32)
#define XWA_AUDIO_PAGE_SYSTEM_SPATIAL "Turn off Windows Sonic or Dolby Atmos for Headphones."
#elif defined(__APPLE__)
#define XWA_AUDIO_PAGE_SYSTEM_SPATIAL "Turn off Spatialize Stereo for your headphones in Control Center."
#else
#define XWA_AUDIO_PAGE_SYSTEM_SPATIAL "Turn off any system spatial audio for your headphones."
#endif

void XwaAudioPage_Draw(AeronUiContext* ui, const AeronInputSnapshot* input) {
	static const char* const outputs[] = { "Speakers", "Headphones" };
	static const char* const layouts[] = { "Stereo", "5.1", "7.1" };
	XwaModernAudioOptions options;
	const int device = Aeron_AudioDeviceChannels();
	bool changed = false;

	(void)input;
	if (!AeronUi_BeginScroll(ui, "Audio settings", XwaOptionsWindow_ScrollHeight()))
		return;
	XwaModernAudioOptions_Get(&options);

	AeronUi_Header(ui, "Output");
	changed |= AeronUi_Selector(ui, "Output", &options.output, outputs, 2) != 0;
	if (options.output == XWA_MODERN_AUDIO_HEADPHONES) {
		AeronUi_Help(ui, XWA_AUDIO_PAGE_SYSTEM_SPATIAL);
	} else {
		/* Only the layouts that fit the device, and a saved layout that no
		   longer does, so that it can be changed. */
		int count = XwaModernAudioOptions_LayoutsForDevice(device);
		if (options.speaker_layout >= count)
			count = options.speaker_layout + 1;
		if (count > 1)
			changed |= AeronUi_Selector(ui, "Speaker Layout", &options.speaker_layout, layouts, count) != 0;
		if (XwaModernAudioOptions_OutputChannels(&options, device) != Aeron_AudioOutputChannels())
			AeronUi_Help(ui, "Applies the next time OpenXWA starts.");
	}
	if (device > 0) {
		char text[64];
		snprintf(text, sizeof text, "Your audio device has %d %s.", device,
				 device == 1 ? "channel" : "channels");
		AeronUi_Help(ui, text);
	}

	AeronUi_Header(ui, "Effects");
	changed |= AeronUi_Toggle(ui, "Distance Filtering", &options.distance_filter) != 0;
	AeronUi_Help(ui, "Distant sounds lose their high frequencies, as through air.");
	changed |= AeronUi_Toggle(ui, "Doppler Effect", &options.doppler) != 0;
	AeronUi_Help(ui, "Sounds rise in pitch as they approach and fall as they move away.");

	if (changed && !XwaModernAudioOptions_Set(&options))
		XwaOptionsWindow_ReportError("Could not change the audio options.");
	AeronUi_EndScroll(ui);
}
