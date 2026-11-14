#include "install_check.h"

#include <stdio.h>
#include <string.h>

static int install_check_error(char* error, size_t error_size, const char* format, const char* detail) {
	if (error && error_size) {
		snprintf(error, error_size, format, detail ? detail : "");
	}
	return 0;
}

static int install_check_required_files(AeronVfs* vfs, const char** missing) {
	/* Canonical install layout: the CD ALLIANCE directory contents merged into the
	   root, as the original installer laid them out. RESDATA.TXT marks the merged
	   ALLIANCE content; the movie/wave entries are unique to CD1 and CD2 so a
	   one-disc tree cannot pass validation. */
	static const char* required[] = {
		"RESDATA.TXT",         "FLIGHTMODELS/SPACECRAFT0.LST", "MISSIONS/MISSION.LST",
		"MOVIES/PROLOGUE.SNM", "MOVIES/BATTLE1.SNM",           "WAVE/FRONTEND/B1M1/N010101.WAV",
	};
	size_t i;

	for (i = 0; i < sizeof required / sizeof required[0]; ++i) {
		if (!AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, required[i])) {
			*missing = required[i];
			return 0;
		}
	}
	return 1;
}

int XwaInstall_ValidateGameData(AeronVfs* vfs, const char* candidate, char* normalized,
								size_t normalized_capacity, char* error, size_t error_size) {
	const char* missing = NULL;

	if (!vfs || !candidate || !candidate[0] || !normalized || strlen(candidate) >= normalized_capacity) {
		return install_check_error(error, error_size, "It can't be used: %s", candidate);
	}
	if (!AeronVfs_SetRoot(vfs, AERON_VFS_ROOT_ASSET, candidate) ||
		!AeronVfs_SetRootOptions(vfs, AERON_VFS_ROOT_ASSET, AERON_VFS_ROOT_OPTION_CASE_INSENSITIVE_LOOKUP)) {
		return install_check_error(error, error_size, "It can't be used: %s", candidate);
	}
	if (install_check_required_files(vfs, &missing)) {
		snprintf(normalized, normalized_capacity, "%s", candidate);
		return 1;
	}

	/* A raw CD copy keeps the install files under ALLIANCE; the game expects them
	   merged into the root. Detect it so the failure explains the fix. */
	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, "ALLIANCE/RESDATA.TXT")) {
		return install_check_error(
			error, error_size,
			"It looks like a copy of the CDs. Copy what is in its ALLIANCE folder into it. "
			"It has no %s.",
			missing);
	}

	return install_check_error(error, error_size, "It has no %s.", missing);
}
