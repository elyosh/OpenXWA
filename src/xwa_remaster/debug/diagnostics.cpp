/*
 * Diagnostics menu: the film benchmark.
 */

#include "xwa_remaster/debug/debug_internal.h"

#include "xwa_runtime/runtime/film_benchmark.h"

#include <cstdio>

/* Finds the film time to measure with --film-pause-at and --film-benchmark. */
static void xwa_window_film_benchmark(void*) {
	const bool playing = XwaFilmBenchmark_FilmPlaying() != 0;
	if (!playing) {
		ImGui::TextDisabled("No film is playing.");
	} else {
		const int ticks = (int)XwaFilmBenchmark_FilmTime();
		const int ms = ticks * 4;
		ImGui::Text("Film time: %d ticks (%d:%02d.%03d)", ticks, ms / 60000, ms / 1000 % 60, ms % 1000);
	}

	const bool frozen = XwaFilmBenchmark_IsFrozen() != 0;
	ImGui::BeginDisabled(!playing);
	if (ImGui::Button(frozen ? "Resume" : "Freeze")) {
		if (frozen) {
			XwaFilmBenchmark_Resume();
		} else {
			XwaFilmBenchmark_RequestFreeze();
		}
	}
	ImGui::EndDisabled();

	if (playing && frozen) {
		char name[256];
		char options[400];
		XwaFilmBenchmark_FilmName(name, sizeof name);
		/* Quoted: film names can hold spaces. */
		std::snprintf(options, sizeof options, "--play-film \"%s\" --film-pause-at %d", name,
					  (int)XwaFilmBenchmark_FilmTime());
		ImGui::Separator();
		ImGui::TextUnformatted(options);
		if (ImGui::Button("Copy")) {
			ImGui::SetClipboardText(options);
		}
		ImGui::SetItemTooltip("Add --film-benchmark to measure this frame and quit.");
	}
}

void XwaDebugDiagnostics_Register(void) {
	XwaDebug_AddWindow(XWA_DEBUG_MENU_DIAGNOSTICS, "Film Benchmark", xwa_window_film_benchmark, 470, 0);
}
