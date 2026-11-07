/*
 * The effect libraries of an import without an authoring folder. Each
 * library is made in memory from the XWAU files and built straight into the
 * mod, so no source images are written. The libraries are built on worker
 * threads, within a memory budget.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_stdinc.h>

#include "aeron/sync.h"
#include "cJSON.h"
#include "effect_library.h"
#include "import_internal.h"
#include "xwau_lava.h"
#include "xwau_library.h"

/* The libraries built at the same time take at most this much memory, as
 * job_bytes estimates it. A larger library is built on its own. */
#define MEMORY_BUDGET ((size_t)1024 << 20)

typedef struct Job {
	size_t index; /* in the library set */
	size_t bytes;
	bool failed;
} Job;

typedef struct Pool {
	ImportContext* ctx;
	const XwauLibrarySet* set;
	const char* out;
	Job* jobs;
	size_t count;
	size_t next;   /* the next job to start */
	size_t in_use; /* the memory of the jobs being built */
	int lava_threads;
	SDL_Mutex* mutex;
	SDL_Condition* finished; /* signalled when a job ends */
} Pool;

/* The memory that building a library takes: its inputs, the images baked
 * from them, and its pages with their mip levels and BC7 blocks. */
static size_t job_bytes(const XwauLibrarySet* set, size_t index) {
	return XwauLibrary_InputBytes(set, index) * 3;
}

/* The lava library first, as it renders its frames on every core, which is
 * quickest before the other libraries take the cores. Then the largest
 * first. */
static int compare_jobs(void* user, const void* a, const void* b) {
	const XwauLibrarySet* set = user;
	const Job* ja = a;
	const Job* jb = b;
	const bool lava_a = strcmp(XwauLibrary_Name(set, ja->index), XWAU_LAVA_LIBRARY) == 0;
	const bool lava_b = strcmp(XwauLibrary_Name(set, jb->index), XWAU_LAVA_LIBRARY) == 0;
	if (lava_a != lava_b)
		return lava_a ? -1 : 1;
	return ja->bytes != jb->bytes ? (ja->bytes < jb->bytes ? 1 : -1)
								  : (ja->index > jb->index) - (ja->index < jb->index);
}

typedef struct Inputs {
	XwauLibraryInput* items;
	size_t count;
} Inputs;

/* EffectLibraryInputSource over the inputs made in memory. The builder reads
 * each input once, so its pixels are handed over instead of copied. */
static bool take_input(void* context, const char* name, uint8_t** rgba, int* width, int* height, char* error,
					   size_t error_size) {
	Inputs* inputs = context;
	for (size_t i = 0; i < inputs->count; ++i) {
		XwauLibraryInput* input = &inputs->items[i];
		if (strcmp(input->name, name) != 0 || !input->rgba)
			continue;
		*rgba = input->rgba;
		*width = input->width;
		*height = input->height;
		input->rgba = NULL;
		return true;
	}
	snprintf(error, error_size, "%s wasn't made, or is used twice", name);
	return false;
}

static bool build(Pool* pool, const Job* job) {
	cJSON* library = NULL;
	Inputs inputs = { 0 };
	const bool ok =
		XwauLibrary_Make(pool->set, job->index, pool->lava_threads, &library, &inputs.items, &inputs.count) &&
		EffectLibrary_BuildFromInputs(IMPORT_LIBRARY_TAG, XwauLibrary_Name(pool->set, job->index), library,
									  take_input, &inputs, pool->out);
	XwauLibrary_FreeInputs(inputs.items, inputs.count);
	cJSON_Delete(library);
	return ok;
}

static int worker(void* user) {
	Pool* pool = user;
	ImportContext* ctx = pool->ctx;
	SDL_LockMutex(pool->mutex);
	for (;;) {
		/* The jobs start in order, when the memory of the jobs being built
		   leaves room for them. */
		while (pool->next < pool->count && !Import_ShouldStop(ctx) && pool->in_use &&
			   pool->in_use + pool->jobs[pool->next].bytes > MEMORY_BUDGET)
			SDL_WaitCondition(pool->finished, pool->mutex);
		if (pool->next >= pool->count || Import_ShouldStop(ctx))
			break;
		Job* job = &pool->jobs[pool->next++];
		pool->in_use += job->bytes;
		SDL_UnlockMutex(pool->mutex);

		job->failed = !build(pool, job);
		if (job->failed) {
			ToolLog_Write(TOOL_LOG_ERROR, "The effect library %s couldn't be built.",
						  XwauLibrary_Name(pool->set, job->index));
			Aeron_AtomicAdd(&ctx->progress->failures, 1);
		}
		Import_Advance(ctx, 1, -1);

		SDL_LockMutex(pool->mutex);
		pool->in_use -= job->bytes;
		SDL_BroadcastCondition(pool->finished);
	}
	SDL_UnlockMutex(pool->mutex);
	return 0;
}

void ImportEffects_Build(ImportContext* ctx, const char* const* gltfs, size_t count, const char* out,
						 size_t checks) {
	XwauLibrarySet* set = XwauLibrary_Read(ctx->options->source, gltfs, count, out);
	if (!set) {
		Import_Failed(ctx, "The effect libraries can't be read.");
		return;
	}
	Pool pool = {
		.ctx = ctx,
		.set = set,
		.out = out,
		.count = XwauLibrary_Count(set),
		.lava_threads = ctx->cores,
		.mutex = SDL_CreateMutex(),
		.finished = SDL_CreateCondition(),
	};
	pool.jobs = calloc(pool.count + 1, sizeof *pool.jobs);
	if (!pool.jobs || !pool.mutex || !pool.finished) {
		Import_Failed(ctx, "The effect libraries can't be built: %s", SDL_GetError());
	} else {
		Aeron_AtomicStore(&ctx->progress->total, (int)(pool.count + checks));
		for (size_t i = 0; i < pool.count; ++i)
			pool.jobs[i] = (Job) { .index = i, .bytes = job_bytes(set, i) };
		SDL_qsort_r(pool.jobs, pool.count, sizeof *pool.jobs, compare_jobs, set);

		int workers = ctx->cores < (int)pool.count ? ctx->cores : (int)pool.count;
		if (workers < 1)
			workers = 1;
		/* The BC7 encoder isn't limited: a large library built alone uses every
		   core. */
		ToolLog_Write(TOOL_LOG_DETAIL, "Building %zu effect libraries on %d threads", pool.count, workers);
		AeronThread** threads = calloc((size_t)workers, sizeof *threads);
		int started = 0;
		for (; threads && started < workers; ++started)
			if (!(threads[started] = Aeron_ThreadCreate("effect library", worker, &pool)))
				break;
		/* Without threads, the libraries are built here. */
		if (!started)
			worker(&pool);
		for (int i = 0; i < started; ++i)
			Aeron_ThreadJoin(threads[i]);
		free(threads);
		for (size_t i = 0; i < pool.count; ++i)
			if (pool.jobs[i].failed)
				Import_ListFailed(ctx, "The effect library %s couldn't be built.",
								  XwauLibrary_Name(set, pool.jobs[i].index));
	}
	free(pool.jobs);
	SDL_DestroyCondition(pool.finished);
	SDL_DestroyMutex(pool.mutex);
	XwauLibrary_FreeSet(set);
}
