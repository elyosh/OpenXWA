#include "xwa_runtime/runtime/film_benchmark.h"

#include "aeron/aeron.h"
#include "aeron/audio.h"
#include "aeron/log.h"
#include "aeron/time.h"
#include "aeron/vfs.h"
#include "xwa/assets/file_io.h"
#include "xwa/audio/fsfx.h"
#include "xwa/audio/sound.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_flight.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_screen.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

extern int g_currentCdDisk;

#define FILM_BENCHMARK_NONE UINT32_MAX
/* The fast-forward stops this long before the target, in film time (2 s), so
   that the last steps play at normal speed and the renderer's state settles
   as in normal playback: auto exposure, temporal history, the models of ships
   that arrived. */
#define FILM_FAST_FORWARD_LEAD_TICKS 500
/* The host time spent on film steps in one host frame of the fast-forward,
   so that the window stays responsive. */
#define FILM_FAST_FORWARD_SLICE_US 30000u

typedef enum FilmBenchmarkState {
	FILM_BENCHMARK_OFF,
	FILM_BENCHMARK_WAITING, /* the start screen didn't run yet */
	FILM_BENCHMARK_PLAYING,
	FILM_BENCHMARK_DONE,
	FILM_BENCHMARK_FAILED,
} FilmBenchmarkState;

/* One host frame while the film plays. FILM_BENCHMARK_NONE: not measured in
   that frame. The film time is the recorded time of the last step played; it
   doesn't depend on the host's timing. */
typedef struct FilmBenchmarkRecord {
	int32_t filmTime;
	uint32_t flightUs;
	uint32_t captureUs;
	uint32_t remasterUs;
	uint32_t hostFrameUs;
} FilmBenchmarkRecord;

static FilmBenchmarkState s_state;
static char s_filmPath[300];
static char s_csvPath[1024];
static FilmBenchmarkRecord* s_records;
static size_t s_recordCount;
static size_t s_recordCapacity;
static uint32_t s_pendingFlightUs = FILM_BENCHMARK_NONE;
static uint32_t s_pendingCaptureUs = FILM_BENCHMARK_NONE;

/* The freeze target, -1 without one, and whether to measure the frozen frame. */
static int32_t s_pauseAt = -1;
static int s_measure;
static int s_measured;
static int s_frozen;
static int s_freezeRequested;
static uint32_t s_frozenFramesPresented;
static int s_fastForwarding;
/* The fast-forward ran in this host frame, so its record isn't kept. */
static int s_fastForwardFrame;
/* Flight commits since the film started, up to 2. */
static uint32_t s_flightCommits;
static int s_lastCommitValid;
static int32_t s_lastCommitFilmTime;

static uint32_t FilmBenchmark_Clamp(uint64_t us) {
	return us < FILM_BENCHMARK_NONE ? (uint32_t)us : FILM_BENCHMARK_NONE - 1;
}

void XwaFilmBenchmark_Configure(const char* filmName, const char* csvPath, int32_t pauseAt, int measure) {
	const size_t length = filmName != NULL ? strlen(filmName) : 0;
	const int hasExtension = length >= 4 && (strcmp(filmName + length - 4, ".flm") == 0 ||
											 strcmp(filmName + length - 4, ".FLM") == 0);

	s_state = FILM_BENCHMARK_OFF;
	s_filmPath[0] = '\0';
	s_csvPath[0] = '\0';
	s_pauseAt = pauseAt >= 0 ? pauseAt : -1;
	s_measure = measure != 0;
	if (length == 0) {
		return;
	}
	snprintf(s_filmPath, sizeof s_filmPath, "film\\%s%s", filmName, hasExtension ? "" : ".flm");
	if (csvPath != NULL) {
		snprintf(s_csvPath, sizeof s_csvPath, "%s", csvPath);
	}
	s_state = FILM_BENCHMARK_WAITING;
}

int XwaFilmBenchmark_IsRequested(void) { return s_state != FILM_BENCHMARK_OFF; }

int XwaFilmBenchmark_IsPlaying(void) { return s_state == FILM_BENCHMARK_PLAYING; }

static void FilmBenchmark_Fail(const char* reason) {
	Aeron_LogError("xwa.benchmark", "film benchmark: %s: %s", s_filmPath, reason);
	s_state = FILM_BENCHMARK_FAILED;
}

/* Does what FilmRoom_Update does for a selected film. */
int XwaFilmBenchmark_StartScreen(int frameCounter) {
	XwaFile* filmFile;
	int16_t requiredDisk;

	if (frameCounter != 0 || s_state != FILM_BENCHMARK_WAITING) {
		return 0;
	}
	if (!g_pilotData.name[0]) {
		FilmBenchmark_Fail("there is no active pilot");
		return 0;
	}
	filmFile = File_Open(AERON_VFS_ROOT_USER, s_filmPath, "rb");
	if (filmFile == NULL) {
		FilmBenchmark_Fail("the film doesn't exist");
		return 0;
	}
	File_ReadWord(filmFile, &requiredDisk);
	File_Close(filmFile);

	strcpy(g_filmFilePath, s_filmPath);
	if ((uint16_t)requiredDisk < 2) {
		g_currentCdDisk = (uint16_t)requiredDisk;
	}
	strcpy(g_mpRoster[0].name, g_pilotData.name);
	g_mpRoster[0].playerId = 1;
	g_mpRoster[0].rating = g_pilotData.pilotRating;
	g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_SINGLEPLAYER;
	g_pilotData.campaignMode = 0;
	g_unusedFrontendMissionLaunchPrepared = 1;
	s_state = FILM_BENCHMARK_PLAYING;
	Aeron_LogInfo("xwa.benchmark", "film benchmark: playing %s", s_filmPath);
	FrontendScreen_SetCallbacks(FlightLoading_GetReadyScreen, NULL);
	return 0;
}

void XwaFilmBenchmark_NoteFlight(int flightFrameRan, uint64_t flightUs) {
	s_pendingFlightUs = flightFrameRan ? FilmBenchmark_Clamp(flightUs) : FILM_BENCHMARK_NONE;
}

void XwaFilmBenchmark_NoteCapture(uint64_t captureUs) { s_pendingCaptureUs = FilmBenchmark_Clamp(captureUs); }

void XwaFilmBenchmark_NoteHostFrame(uint64_t remasterUs, uint64_t hostFrameUs) {
	FilmBenchmarkRecord* record;
	const int fastForwardFrame = s_fastForwardFrame;

	s_fastForwardFrame = 0;
	if (s_frozen && s_frozenFramesPresented < UINT32_MAX) {
		++s_frozenFramesPresented;
	}
	/* Frozen frames and fast-forward frames aren't film playback. */
	if (s_frozen || fastForwardFrame) {
		s_pendingFlightUs = FILM_BENCHMARK_NONE;
		s_pendingCaptureUs = FILM_BENCHMARK_NONE;
		return;
	}
	/* Only the frames of the film itself, not its loading. */
	if (s_state != FILM_BENCHMARK_PLAYING || !XwaFlightTask_IsActive() || g_filmPlaybackMode != 2) {
		s_pendingFlightUs = FILM_BENCHMARK_NONE;
		s_pendingCaptureUs = FILM_BENCHMARK_NONE;
		return;
	}
	if (s_recordCount == s_recordCapacity) {
		const size_t capacity = s_recordCapacity != 0 ? s_recordCapacity * 2 : 4096;
		FilmBenchmarkRecord* records = realloc(s_records, capacity * sizeof *records);

		if (records == NULL) {
			return;
		}
		s_records = records;
		s_recordCapacity = capacity;
	}
	record = &s_records[s_recordCount++];
	record->filmTime = g_filmStepInputTimestamp;
	record->flightUs = s_pendingFlightUs;
	record->captureUs = s_pendingCaptureUs;
	record->remasterUs = FilmBenchmark_Clamp(remasterUs);
	record->hostFrameUs = FilmBenchmark_Clamp(hostFrameUs);
	s_pendingFlightUs = FILM_BENCHMARK_NONE;
	s_pendingCaptureUs = FILM_BENCHMARK_NONE;
}

static int FilmBenchmark_CompareU32(const void* a, const void* b) {
	const uint32_t x = *(const uint32_t*)a;
	const uint32_t y = *(const uint32_t*)b;

	return x < y ? -1 : x > y;
}

/* Logs the frame count, median, 95th percentile and worst case of one
   measure, `offset` in the records. */
static void FilmBenchmark_LogMeasure(const char* name, size_t offset) {
	uint32_t* values = malloc((s_recordCount != 0 ? s_recordCount : 1) * sizeof *values);
	size_t count = 0;
	size_t i;

	if (values == NULL) {
		return;
	}
	for (i = 0; i < s_recordCount; ++i) {
		uint32_t value;

		memcpy(&value, (const uint8_t*)&s_records[i] + offset, sizeof value);
		if (value != FILM_BENCHMARK_NONE) {
			values[count++] = value;
		}
	}
	if (count == 0) {
		Aeron_LogInfo("xwa.benchmark", "film benchmark: %s: no frames", name);
	} else {
		qsort(values, count, sizeof *values, FilmBenchmark_CompareU32);
		Aeron_LogInfo(
			"xwa.benchmark",
			"film benchmark: %s: %zu frames, median %.3f ms, 95th percentile %.3f ms, worst %.3f ms", name,
			count, values[count / 2] / 1000.0, values[(count - 1) * 95 / 100] / 1000.0,
			values[count - 1] / 1000.0);
	}
	free(values);
}

static void FilmBenchmark_AppendValue(char* line, size_t size, uint32_t value) {
	const size_t used = strlen(line);

	if (value == FILM_BENCHMARK_NONE) {
		snprintf(line + used, size - used, ",");
	} else {
		snprintf(line + used, size - used, ",%u", (unsigned int)value);
	}
}

static void FilmBenchmark_WriteCsv(void) {
	static const char header[] = "frame,film_time,flight_us,capture_us,remaster_us,host_frame_us\n";
	char* text;
	size_t used;
	size_t i;

	/* At most 11 characters per value, 6 values, separators and a newline. */
	text = malloc(sizeof header + s_recordCount * 80);
	if (text == NULL) {
		Aeron_LogError("xwa.benchmark", "film benchmark: %s: out of memory", s_csvPath);
		return;
	}
	memcpy(text, header, sizeof header);
	used = sizeof header - 1;
	for (i = 0; i < s_recordCount; ++i) {
		const FilmBenchmarkRecord* record = &s_records[i];
		char line[80];

		snprintf(line, sizeof line, "%zu,%d", i, (int)record->filmTime);
		FilmBenchmark_AppendValue(line, sizeof line, record->flightUs);
		FilmBenchmark_AppendValue(line, sizeof line, record->captureUs);
		FilmBenchmark_AppendValue(line, sizeof line, record->remasterUs);
		FilmBenchmark_AppendValue(line, sizeof line, record->hostFrameUs);
		used += (size_t)snprintf(text + used, 80, "%s\n", line);
	}
	if (AeronVfs_WriteAllAtomic(Aeron_GetVfs(), AERON_VFS_ROOT_USER, s_csvPath, text, used)) {
		Aeron_LogInfo("xwa.benchmark", "film benchmark: wrote %s", s_csvPath);
	} else {
		Aeron_LogError("xwa.benchmark", "film benchmark: could not write %s", s_csvPath);
	}
	free(text);
}

void XwaFilmBenchmark_FlightEnded(void) {
	s_frozen = 0;
	s_freezeRequested = 0;
	s_flightCommits = 0;
	s_lastCommitValid = 0;
	if (s_state != FILM_BENCHMARK_PLAYING) {
		return;
	}
	s_state = FILM_BENCHMARK_DONE;
	if (s_pauseAt >= 0) {
		Aeron_LogError("xwa.benchmark", "film benchmark: %s: the film ended before film time %d", s_filmPath,
					   (int)s_pauseAt);
		s_state = FILM_BENCHMARK_FAILED;
	}
	if (s_recordCount != 0) {
		Aeron_LogInfo("xwa.benchmark", "film benchmark: %s: film time %d to %d", s_filmPath,
					  (int)s_records[0].filmTime, (int)s_records[s_recordCount - 1].filmTime);
	}
	FilmBenchmark_LogMeasure("flight frame", offsetof(FilmBenchmarkRecord, flightUs));
	FilmBenchmark_LogMeasure("snapshot capture", offsetof(FilmBenchmarkRecord, captureUs));
	FilmBenchmark_LogMeasure("remaster frame", offsetof(FilmBenchmarkRecord, remasterUs));
	FilmBenchmark_LogMeasure("host frame", offsetof(FilmBenchmarkRecord, hostFrameUs));
	if (s_csvPath[0]) {
		FilmBenchmark_WriteCsv();
	}
	free(s_records);
	s_records = NULL;
	s_recordCount = 0;
	s_recordCapacity = 0;
}

int XwaFilmBenchmark_ShouldQuit(void) {
	return s_state == FILM_BENCHMARK_DONE || s_state == FILM_BENCHMARK_FAILED;
}

int XwaFilmBenchmark_ExitCode(void) { return s_state == FILM_BENCHMARK_FAILED ? 1 : 0; }

int XwaFilmBenchmark_FilmPlaying(void) {
	return XwaFlightTask_IsActive() && !XwaFlightTask_IsComplete() && g_filmPlaybackMode == 2;
}

int32_t XwaFilmBenchmark_FilmTime(void) { return g_filmStepInputTimestamp; }

void XwaFilmBenchmark_FilmName(char* out, size_t size) {
	const char* name = g_filmFilePath;
	const char* slash;
	size_t length;

	if (out == NULL || size == 0) {
		return;
	}
	slash = strrchr(name, '\\');
	if (slash != NULL) {
		name = slash + 1;
	}
	length = strlen(name);
	if (length >= 4 && (strcmp(name + length - 4, ".flm") == 0 || strcmp(name + length - 4, ".FLM") == 0)) {
		length -= 4;
	}
	if (length >= size) {
		length = size - 1;
	}
	memcpy(out, name, length);
	out[length] = '\0';
}

int XwaFilmBenchmark_IsFrozen(void) { return s_frozen; }

void XwaFilmBenchmark_RequestFreeze(void) {
	if (!s_frozen && XwaFilmBenchmark_FilmPlaying()) {
		s_freezeRequested = 1;
	}
}

void XwaFilmBenchmark_Resume(void) {
	if (s_frozen) {
		Aeron_LogInfo("xwa.benchmark", "film benchmark: resumed at %d", (int)g_filmStepInputTimestamp);
	}
	s_frozen = 0;
	s_freezeRequested = 0;
}

void XwaFilmBenchmark_NoteFlightCommit(void) {
	const XwaSnapshot* previous = XwaSnapshot_Previous();
	const int32_t filmTime = g_filmStepInputTimestamp;

	if (!XwaFilmBenchmark_FilmPlaying()) {
		s_flightCommits = 0;
		s_lastCommitValid = 0;
		return;
	}
	if (s_flightCommits < 2) {
		++s_flightCommits;
	}
	/* The renderer takes the motion vectors from the previous snapshot, so it
	   must hold the film step before the current one. */
	if (!s_frozen && (s_freezeRequested || (s_pauseAt >= 0 && filmTime >= s_pauseAt)) && s_lastCommitValid &&
		filmTime != s_lastCommitFilmTime && previous != NULL && previous->scene_kind == XWA_SCENE_FLIGHT &&
		previous->flight_camera_valid) {
		s_frozen = 1;
		s_freezeRequested = 0;
		s_pauseAt = -1;
		s_frozenFramesPresented = 0;
		Aeron_LogInfo("xwa.benchmark", "film benchmark: frozen at %d", (int)filmTime);
	}
	s_lastCommitFilmTime = filmTime;
	s_lastCommitValid = 1;
}

/* The fast-forward starts once the HD renderer drew the film's first flight
   frames, so that classic presentation is already suppressed. */
static int FilmBenchmark_FastForwardWanted(void) {
	return s_pauseAt >= 0 && !s_frozen && s_flightCommits >= 2 && XwaFilmBenchmark_FilmPlaying() &&
		   g_filmStepInputTimestamp < s_pauseAt - FILM_FAST_FORWARD_LEAD_TICKS;
}

int XwaFilmBenchmark_FastForward(void) {
	if (FilmBenchmark_FastForwardWanted()) {
		const uint64_t endUs = Aeron_NowUs() + FILM_FAST_FORWARD_SLICE_US;

		if (!s_fastForwarding) {
			s_fastForwarding = 1;
			/* Silent, but the mixer keeps running: the sounds of the skipped
			   steps end and free their slots instead of all playing at the end. */
			Aeron_AudioSetMasterGain(0.0f);
			Aeron_LogInfo("xwa.benchmark", "film benchmark: fast-forwarding from %d to %d",
						  (int)g_filmStepInputTimestamp, (int)(s_pauseAt - FILM_FAST_FORWARD_LEAD_TICKS));
		}
		while (Aeron_NowUs() < endUs && FilmBenchmark_FastForwardWanted() && XwaFlightTask_RunFrameNow()) {
		}
	}
	if (s_fastForwarding) {
		s_fastForwardFrame = 1;
		if (!FilmBenchmark_FastForwardWanted()) {
			s_fastForwarding = 0;
			if (XwaFilmBenchmark_FilmPlaying()) {
				XwaFlightTask_EndFilmFastForward();
			}
			Sound_StopAllInstances();
			while (!fsfx_IsVoiceQueueEmpty()) {
				fsfx_RemoveVoiceQueueEntryChain(0);
			}
			Aeron_AudioSetMasterGain(1.0f);
			Aeron_LogInfo("xwa.benchmark", "film benchmark: fast-forward ended at %d",
						  (int)g_filmStepInputTimestamp);
		}
	}
	return s_fastForwarding;
}

int XwaFilmBenchmark_MeasurementDue(void) {
	return s_measure && !s_measured && s_frozen && s_frozenFramesPresented != 0;
}

void XwaFilmBenchmark_MeasurementDone(int ok) {
	s_measured = 1;
	if (s_state != FILM_BENCHMARK_PLAYING) {
		return;
	}
	if (ok) {
		s_state = FILM_BENCHMARK_DONE;
	} else {
		FilmBenchmark_Fail("the measurement failed");
	}
}
