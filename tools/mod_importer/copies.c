#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "import_internal.h"
#include "yaml_json.h"

typedef struct Copies {
	ImportContext* ctx;
	int copied;
	int missing;
} Copies;

/* Adds to `names` each string value of a `key` key in `node`, once. */
static void collect(const cJSON* node, const char* key, cJSON* names) {
	const cJSON* child;
	cJSON_ArrayForEach(child, node) {
		if (cJSON_IsString(child) && child->string && strcmp(child->string, key) == 0) {
			bool seen = false;
			const cJSON* name;
			cJSON_ArrayForEach(name, names) seen |= strcmp(name->valuestring, child->valuestring) == 0;
			if (!seen)
				cJSON_AddItemToArray(names, cJSON_CreateString(child->valuestring));
		} else
			collect(child, key, names);
	}
}

/* The `key` values of the YAML file `path`, or NULL when it isn't there. */
static cJSON* read_names(Copies* copies, const char* path, const char* key) {
	char err[512];
	if (!IsFile(path))
		return NULL;
	cJSON* root = YamlJson_Load(path, err, sizeof err);
	if (!root) {
		Import_Failed(copies->ctx, "%s", err);
		return NULL;
	}
	cJSON* names = cJSON_CreateArray();
	collect(root, key, names);
	cJSON_Delete(root);
	return names;
}

/* Copies the file `name` of folder `from`, in any case, to <to>/<name>. */
static void copy_named(Copies* copies, const char* from, const char* name, const char* to) {
	char source[IMPORT_PATH_CAP], target[IMPORT_PATH_CAP];
	if (strchr(name, '/') || strchr(name, '\\') || !from[0] ||
		!Import_FindNoCase(from, name, false, source, sizeof source)) {
		Import_Failed(copies->ctx, "%s: not in %s", name, from[0] ? from : "the installation");
		copies->missing++;
		return;
	}
	if (PathFormat(target, sizeof target, "%s/%s", to, name) && MakeDirs(to) &&
		Import_CopyFile(source, target))
		copies->copied++;
	else
		Import_Failed(copies->ctx, "Can't copy %s to %s", source, to);
}

/* Copies the files that the `key` keys of the YAML file `yaml` name, from
 * folder `from` into folder `to`. */
static void copy_listed(Copies* copies, const char* yaml, const char* key, const char* from, const char* to) {
	cJSON* names = read_names(copies, yaml, key);
	const cJSON* name;
	cJSON_ArrayForEach(name, names) copy_named(copies, from, name->valuestring, to);
	cJSON_Delete(names);
}

/* The videos of frontend/frontres/<dir>/video/<base>.yaml, next to each
 * file. */
static void copy_frontres_videos(Copies* copies, const char* frontres, const char* resdata) {
	size_t dir_count = 0;
	char** dirs = ListDir(frontres, true, &dir_count);
	for (size_t d = 0; d < dir_count; ++d) {
		char video[IMPORT_PATH_CAP];
		size_t count = 0;
		char** files = PathFormat(video, sizeof video, "%s/%s/video", frontres, dirs[d])
						   ? ListDir(video, true, &count)
						   : NULL;
		for (size_t f = 0; f < count; ++f) {
			char yaml[IMPORT_PATH_CAP];
			if (EndsWithNoCase(files[f], ".yaml") && PathFormat(yaml, sizeof yaml, "%s/%s", video, files[f]))
				copy_listed(copies, yaml, "video", resdata, video);
		}
		FreeList(files, count);
	}
	FreeList(dirs, dir_count);
}

/* The installation's Movies folder, without INTRO_FINAL.SUB, which is the
 * original game's subtitles. */
static void copy_movies(Copies* copies, const char* to) {
	char movies[IMPORT_PATH_CAP];
	if (!Import_FindNoCase(copies->ctx->options->source, "Movies", true, movies, sizeof movies))
		return;
	size_t count = 0;
	char** names = ListDir(movies, true, &count);
	for (size_t i = 0; i < count; ++i) {
		char path[IMPORT_PATH_CAP];
		if (strcasecmp(names[i], "INTRO_FINAL.SUB") != 0 &&
			PathFormat(path, sizeof path, "%s/%s", movies, names[i]) && IsFile(path))
			copy_named(copies, movies, names[i], to);
	}
	FreeList(names, count);
}

void ImportCopies_Run(ImportContext* ctx) {
	const XwaModImportOptions* options = ctx->options;
	Copies copies = { .ctx = ctx };
	char resdata[IMPORT_PATH_CAP] = "", sfx[IMPORT_PATH_CAP] = "", fonts[IMPORT_PATH_CAP] = "";
	char path[IMPORT_PATH_CAP], dir[IMPORT_PATH_CAP];
	Import_FindNoCase(options->source, "Resdata", true, resdata, sizeof resdata);
	Import_FindNoCase(options->source, "Sfx", true, sfx, sizeof sfx);
	PathFormat(fonts, sizeof fonts, "%s/assets/fonts", options->resources);

	if (PathFormat(path, sizeof path, "%s/frontend/animations.yaml", options->mod) &&
		PathFormat(dir, sizeof dir, "%s/frontend/animations", options->mod)) {
		copy_listed(&copies, path, "video", resdata, dir);
		copy_listed(&copies, path, "sound", sfx, dir);
	}
	if (PathFormat(dir, sizeof dir, "%s/frontend/frontres", options->mod))
		copy_frontres_videos(&copies, dir, resdata);
	if (PathFormat(dir, sizeof dir, "%s/frontend/movies", options->mod))
		copy_movies(&copies, dir);
	static const char* const font_dirs[] = { "frontend/fonts", "flight/fonts" };
	for (size_t i = 0; i < sizeof font_dirs / sizeof font_dirs[0]; ++i)
		if (PathFormat(dir, sizeof dir, "%s/%s", options->mod, font_dirs[i]) &&
			PathFormat(path, sizeof path, "%s/fonts.yaml", dir))
			copy_listed(&copies, path, "file", fonts, dir);
	ToolLog_Write(TOOL_LOG_INFO, "Copied %d files, %d missing", copies.copied, copies.missing);
}
