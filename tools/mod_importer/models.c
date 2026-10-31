#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_filesystem.h>

#include "aeron/sync.h"
#include "aeron/time.h"
#include "bc7_codec.h"
#include "cook_model.h"
#include "import_internal.h"
#include "ktx2_writer.h"
#include "model_conversion.h"
#include "opt_gltf_write.h"

static int compare_keys(const void* a, const void* b) {
	return strcmp(((const ImportModel*)a)->key, ((const ImportModel*)b)->key);
}

static bool wanted(const XwaModImportOptions* options, const char* key) {
	if (!options->only_count)
		return true;
	for (size_t i = 0; i < options->only_count; ++i)
		if (strcmp(options->only[i], key) == 0)
			return true;
	return false;
}

static bool listed(const ImportModel* models, size_t count, const char* key) {
	for (size_t i = 0; i < count; ++i)
		if (strcmp(models[i].key, key) == 0)
			return true;
	return false;
}

/* Adds the OPTs of the installation's FlightModels folder. */
static bool list_opts(ImportContext* ctx, ImportModel** models, size_t* count) {
	char dir[IMPORT_PATH_CAP];
	if (!Import_FindNoCase(ctx->options->source, "FlightModels", true, dir, sizeof dir)) {
		Import_Failed(ctx, "%s has no FlightModels folder", ctx->options->source);
		return false;
	}
	size_t names_count = 0;
	char** names = ListDir(dir, true, &names_count);
	bool ok = true;
	for (size_t i = 0; ok && i < names_count; ++i) {
		char path[IMPORT_PATH_CAP], key[IMPORT_KEY_CAP];
		SDL_PathInfo info;
		if (!EndsWithNoCase(names[i], ".opt") || !PathFormat(path, sizeof path, "%s/%s", dir, names[i]) ||
			!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE)
			continue;
		const size_t base_length = strlen(names[i]) - 4;
		if (base_length == 0 || base_length >= sizeof key)
			continue;
		LowerCopy(key, sizeof key, names[i]);
		key[base_length] = '\0';
		if (!wanted(ctx->options, key))
			continue;
		if (listed(*models, *count, key)) {
			ToolLog_Write(TOOL_LOG_WARNING, "%s: another OPT has the same name in another case; skipped",
						  path);
			continue;
		}
		ImportModel* model = ARRAY_PUSH(*models, *count);
		ok = model && (model->opt = StrDup(path)) != NULL;
		if (ok) {
			snprintf(model->key, sizeof model->key, "%s", key);
			snprintf(model->base, sizeof model->base, "%.*s", (int)base_length, names[i]);
			model->size = (long long)info.size;
		}
	}
	FreeList(names, names_count);
	if (!ok)
		Import_Failed(ctx, "Out of memory");
	return ok;
}

/* Adds the models of the authoring folder that the installation doesn't
 * have: models/<key>/<key>.gltf. */
static bool list_authored(ImportContext* ctx, ImportModel** models, size_t* count) {
	char dir[IMPORT_PATH_CAP];
	if (!PathFormat(dir, sizeof dir, "%s/models", ctx->options->authoring))
		return false;
	size_t names_count = 0;
	char** names = ListDir(dir, true, &names_count);
	bool ok = true;
	for (size_t i = 0; ok && i < names_count; ++i) {
		char gltf[IMPORT_PATH_CAP];
		if (names[i][0] == '.' || strlen(names[i]) >= IMPORT_KEY_CAP || !wanted(ctx->options, names[i]) ||
			listed(*models, *count, names[i]) ||
			!PathFormat(gltf, sizeof gltf, "%s/%s/%s.gltf", dir, names[i], names[i]) || !IsFile(gltf))
			continue;
		ImportModel* model = ARRAY_PUSH(*models, *count);
		ok = model != NULL;
		if (ok) {
			snprintf(model->key, sizeof model->key, "%s", names[i]);
			snprintf(model->base, sizeof model->base, "%s", names[i]);
		}
	}
	FreeList(names, names_count);
	if (!ok)
		Import_Failed(ctx, "Out of memory");
	return ok;
}

bool ImportModels_List(ImportContext* ctx, ImportModel** models, size_t* count) {
	*models = NULL;
	*count = 0;
	if (!list_opts(ctx, models, count) || (ctx->options->authoring && !list_authored(ctx, models, count)))
		return false;
	if (!*count) {
		Import_Failed(ctx,
					  ctx->options->only_count ? "None of the models asked for is in %s"
											   : "%s has no .opt files in FlightModels",
					  ctx->options->source);
		return false;
	}
	qsort(*models, *count, sizeof **models, compare_keys);
	return true;
}

void ImportModels_Free(ImportModel* models, size_t count) {
	for (size_t i = 0; i < count; ++i) {
		free(models[i].opt);
		free(models[i].gltf);
		free(models[i].failed);
		cJSON_Delete(models[i].files);
	}
	free(models);
}

/* ---- The workers -------------------------------------------------------- */

typedef struct Pool {
	ImportContext* ctx;
	ImportModel** order; /* the models, largest OPT first */
	size_t count;
	volatile int next;
	volatile int done_kb;
	int total_kb;
	ModelConversion conversion;
	AeronGltfCookOptions cook;
	const ImportAuthoring* authoring;
	char glb_dir[IMPORT_PATH_CAP];
} Pool;

static void fail(ImportContext* ctx, ImportModel* model, const char* reason) {
	ToolLog_Write(TOOL_LOG_ERROR, "%s: %s", model->key, reason);
	model->failed = StrDup(reason);
	Aeron_AtomicAdd(&ctx->progress->failures, 1);
}

/* Converts the OPT into folder `dir`, as <dir>/<key>.gltf with its files. */
static bool convert(Pool* pool, ImportModel* model, const char* dir) {
	char from[IMPORT_PATH_CAP], to[IMPORT_PATH_CAP];
	if (!RemoveTree(dir) || !MakeDirs(dir)) {
		fail(pool->ctx, model, "can't create its work folder");
		return false;
	}
	if (!ModelConversion_Convert(&pool->conversion, model->opt, model->base, dir)) {
		fail(pool->ctx, model, "conversion failed");
		return false;
	}
	if (!PathFormat(from, sizeof from, "%s/%s.gltf", dir, model->base) ||
		!PathFormat(to, sizeof to, "%s/%s.gltf", dir, model->key) ||
		(strcmp(from, to) != 0 && !Import_Rename(from, to))) {
		fail(pool->ctx, model, "can't rename its .gltf");
		return false;
	}
	return true;
}

/* The probes bake of an authoring model, where the Blender add-on writes
 * it. */
static bool probes_folder(const char* model_dir, char* out, size_t cap) {
	char path[IMPORT_PATH_CAP];
	if (!PathFormat(out, cap, "%s/bake/probes", model_dir))
		return false;
	static const char* const files[] = { "bake.json", "probes.ktx2", "grid.ktx2" };
	for (size_t i = 0; i < sizeof files / sizeof files[0]; ++i)
		if (!PathFormat(path, sizeof path, "%s/%s", out, files[i]) || !IsFile(path))
			return false;
	return true;
}

/* Converts the model into the authoring folder, unless the user changed it
 * there. Sets the .gltf to cook and its probes. */
static bool place_authoring(Pool* pool, ImportModel* model, char* gltf, char* probes) {
	char rel[IMPORT_PATH_CAP], dir[IMPORT_PATH_CAP], staging[IMPORT_PATH_CAP];
	if (!PathFormat(rel, sizeof rel, "models/%s", model->key) ||
		!PathFormat(dir, sizeof dir, "%s/%s", pool->authoring->folder, rel) ||
		!PathFormat(gltf, IMPORT_PATH_CAP, "%s/%s.gltf", dir, model->key) ||
		!PathFormat(staging, sizeof staging, "%s/models/.staging.%s", pool->authoring->folder, model->key)) {
		fail(pool->ctx, model, "path too long");
		return false;
	}
	model->state = model->opt ? ImportAuthoring_State(pool->authoring, rel) : IMPORT_AUTHORING_AUTHORED;
	if (model->state == IMPORT_AUTHORING_EDITED)
		ToolLog_Write(TOOL_LOG_INFO, "%s: edited since its import, kept", rel);
	else if (model->state == IMPORT_AUTHORING_AUTHORED)
		ToolLog_Write(TOOL_LOG_INFO, "%s: not made by the importer, kept", rel);
	else {
		if (!convert(pool, model, staging)) {
			RemoveTree(staging);
			return false;
		}
		if (!(model->files = ImportAuthoring_Place(pool->authoring, staging, rel))) {
			RemoveTree(staging);
			fail(pool->ctx, model, "can't place it in the authoring folder");
			return false;
		}
	}
	if (!probes_folder(dir, probes, IMPORT_PATH_CAP))
		probes[0] = '\0';
	return true;
}

/* AeronGltfCookUriImageSource over a converted document. */
static bool document_image(void* context, const char* uri, AeronGltfCookImageView* out) {
	OptGltfImageView view;
	if (!OptGltf_ImageViewByUri((const OptGltfDocument*)context, uri, &view))
		return false;
	out->rgba = view.rgba;
	out->width = (int)view.width;
	out->height = (int)view.height;
	return true;
}

/* Converts the model in memory, writes its .gltf to `kept` for the effect
 * libraries and the material check, and cooks it into `glb`. */
static bool import_in_memory(Pool* pool, ImportModel* model, const char* kept, const char* glb) {
	ModelConversionModel* built = NULL;
	if (!ModelConversion_Build(&pool->conversion, model->opt, model->base, &built)) {
		fail(pool->ctx, model, "conversion failed");
		return false;
	}
	const OptGltfDocument* document = ModelConversion_Document(built);
	const cgltf_data* data = OptGltf_Data((OptGltfDocument*)document);
	size_t json_size = 0;
	char* json = OptGltf_Json(document, &json_size);
	bool cooked = false;
	if (!json || !WriteWholeFile(kept, json, json_size))
		fail(pool->ctx, model, "can't write its .gltf");
	else {
		const AeronGltfCookInput input = {
			.json = json,
			.json_size = json_size,
			.label = kept,
			.bin = data->buffers_count ? data->buffers[0].data : NULL,
			.bin_size = data->buffers_count ? data->buffers[0].size : 0,
			.images = document_image,
			.images_context = (void*)document,
		};
		if (!(cooked = aeron_gltf_cook_model_input(&input, glb, &pool->cook, NULL)))
			fail(pool->ctx, model, "cook failed");
	}
	free(json);
	ModelConversion_FreeModel(built);
	return cooked;
}

static void import_model(Pool* pool, ImportModel* model) {
	ImportContext* ctx = pool->ctx;
	const uint64_t start = Aeron_NowUs();
	char gltf[IMPORT_PATH_CAP], glb[IMPORT_PATH_CAP], probes[IMPORT_PATH_CAP] = "";
	if (!PathFormat(glb, sizeof glb, "%s/%s.glb", pool->glb_dir, model->key) ||
		(!pool->authoring && !PathFormat(gltf, sizeof gltf, "%s/gltf/%s.gltf", ctx->work, model->key))) {
		fail(ctx, model, "path too long");
		return;
	}
	if (pool->authoring) {
		if (!place_authoring(pool, model, gltf, probes))
			return;
		if (!(model->cooked = aeron_gltf_cook_model(gltf, glb, &pool->cook, probes[0] ? probes : NULL)))
			fail(ctx, model, "cook failed");
	} else
		model->cooked = import_in_memory(pool, model, gltf, glb);
	if (model->cooked && !(model->gltf = StrDup(gltf))) {
		fail(ctx, model, "out of memory");
		model->cooked = false;
	}
	model->seconds = (double)(Aeron_NowUs() - start) / 1e6;
	if (model->cooked)
		ToolLog_Write(TOOL_LOG_INFO, "%s: done in %.1f s", model->key, model->seconds);
}

static int worker(void* user) {
	Pool* pool = user;
	for (;;) {
		const int index = Aeron_AtomicAdd(&pool->next, 1);
		if ((size_t)index >= pool->count || Import_Cancelled(pool->ctx))
			break;
		ImportModel* model = pool->order[index];
		import_model(pool, model);
		const int kb = (int)(model->size / 1024) + 1;
		const int done = Aeron_AtomicAdd(&pool->done_kb, kb) + kb;
		Import_Advance(pool->ctx, 1, (int)((long long)done * 1000 / pool->total_kb));
	}
	return 0;
}

/* Largest OPT first, then by key. */
static int compare_sizes(const void* a, const void* b) {
	const ImportModel* ma = *(ImportModel* const*)a;
	const ImportModel* mb = *(ImportModel* const*)b;
	return ma->size != mb->size ? (ma->size < mb->size ? 1 : -1) : strcmp(ma->key, mb->key);
}

bool ImportModels_Run(ImportContext* ctx, ImportModel* models, size_t count,
					  const ImportAuthoring* authoring) {
	Pool* pool = calloc(1, sizeof *pool);
	if (!pool || !(pool->order = malloc(count * sizeof *pool->order))) {
		free(pool);
		Import_Failed(ctx, "Out of memory");
		return false;
	}
	pool->ctx = ctx;
	pool->count = count;
	pool->authoring = authoring;
	pool->total_kb = 1;
	for (size_t i = 0; i < count; ++i) {
		pool->order[i] = &models[i];
		pool->total_kb += (int)(models[i].size / 1024) + 1;
	}
	qsort(pool->order, count, sizeof *pool->order, compare_sizes);

	char gltf_dir[IMPORT_PATH_CAP];
	bool ok = PathFormat(pool->glb_dir, sizeof pool->glb_dir, "%s/flight/models", ctx->options->mod) &&
			  PathFormat(gltf_dir, sizeof gltf_dir, "%s/gltf", ctx->work) && MakeDirs(pool->glb_dir) &&
			  MakeDirs(gltf_dir);
	if (!ok)
		Import_Failed(ctx, "Can't create %s", pool->glb_dir);
	SetReportProgram("opt2gltf");
	if (ok && !ModelConversion_InitXwau(&pool->conversion, ctx->options->source)) {
		Import_Failed(ctx, "Can't read the installation's materials, missions and tables");
		ok = false;
	}

	if (ok) {
		aeron_gltf_cook_default_options(&pool->cook);
		if (ctx->options->zstd_level > 0)
			pool->cook.zstd_level = ctx->options->zstd_level;
		/* Set here once, so the cooks running at once don't set it. */
		const int zstd_level = ktx2_zstd_level();
		ktx2_set_zstd_level(pool->cook.zstd_level);

		int workers = ctx->options->jobs > 0 ? ctx->options->jobs : ctx->cores / 2;
		if (workers < 1)
			workers = 1;
		if ((size_t)workers > count)
			workers = (int)count;
		bc7_codec_set_max_threads((unsigned)(ctx->cores / workers > 1 ? ctx->cores / workers : 1));
		ToolLog_Write(TOOL_LOG_INFO, "Converting %zu models on %d threads", count, workers);

		AeronThread** threads = calloc((size_t)workers, sizeof *threads);
		int started = 0;
		for (; threads && started < workers; ++started)
			if (!(threads[started] = Aeron_ThreadCreate("model import", worker, pool)))
				break;
		if (!started) {
			Import_Failed(ctx, "Can't start the model threads");
			ok = false;
		}
		for (int i = 0; i < started; ++i)
			Aeron_ThreadJoin(threads[i]);
		free(threads);
		bc7_codec_set_max_threads(0);
		ktx2_set_zstd_level(zstd_level);
	}
	ModelConversion_Free(&pool->conversion);
	free(pool->order);
	free(pool);
	return ok;
}
