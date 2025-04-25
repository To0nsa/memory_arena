#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREAD_COUNT 8
#define ALLOCATIONS_PER_THREAD 10

static t_arena*   shared_arena;
static atomic_int successful_allocs = 0;
static atomic_int failed_allocs     = 0;
static atomic_int null_arena_fails  = 0;

static void* thread_func()
{
	for (int i = 0; i < ALLOCATIONS_PER_THREAD; ++i)
	{
		size_t count = (rand() % 64) + 1;
		size_t size  = (rand() % 128) + 1;

		if (i % 25 == 0)
			count = 0;
		if (i % 40 == 0)
			size = 0;
		if (i % 100 == 0 && size != 0)
			count = SIZE_MAX / size + 100;

		void* ptr = arena_calloc(shared_arena, count, size);
		if (!ptr)
		{
			if (count != 0 && size != 0)
				atomic_fetch_add(&failed_allocs, 1);
		}
		else
		{
			atomic_fetch_add(&successful_allocs, 1);
			size_t         total = count * size;
			unsigned char* bytes = (unsigned char*) ptr;
			for (size_t j = 0; j < total; ++j)
				assert(bytes[j] == 0);
		}

		if (i % 200 == 0)
		{
			void* null_ptr = arena_calloc(NULL, 10, 10);
			if (!null_ptr)
				atomic_fetch_add(&null_arena_fails, 1);
		}
	}
	return NULL;
}

int main(void)
{
	shared_arena = arena_create(1024 * 1024, true);
	assert(shared_arena);

	pthread_t threads[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_create(&threads[i], NULL, thread_func, NULL);

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);

	int    total_success    = atomic_load(&successful_allocs);
	int    total_failed     = atomic_load(&failed_allocs);
	int    total_null_fails = atomic_load(&null_arena_fails);
	size_t arena_failed     = shared_arena->stats.failed_allocations;

	printf("✅ Total successful allocations: %d\n", total_success);
	printf("✅ Total bytes allocated:       %zu\n", shared_arena->stats.bytes_allocated);
	printf("✅ Failed allocations:          %d\n", total_failed);
	printf("✅ Null arena failures:         %d\n", total_null_fails);
	printf("✅ Arena stats.allocations:     %zu\n", shared_arena->stats.allocations);
	printf("✅ Arena stats.failed:          %zu\n", arena_failed);
	printf("✅ Arena stats.bytes_allocated: %zu\n", shared_arena->stats.bytes_allocated);
	printf("✅ Arena size:                  %zu\n", shared_arena->size);
	printf("✅ Arena offset:                %zu\n", shared_arena->offset);
	printf("✅ Peak usage:                  %zu\n", shared_arena->stats.peak_usage);

	assert(shared_arena->stats.allocations == (size_t) total_success);
	if (arena_failed != (size_t) total_failed)
	{
		printf("⚠️ Note: arena->stats.failed_allocations (%zu) includes all internal checks (zero-size, overflow, "
		       "etc).\n",
		       arena_failed);
		printf("⚠️       local failed_allocs (%d) counts only allocs where count && size > 0.\n", total_failed);
	}
	assert(shared_arena->offset <= shared_arena->size);
	assert(shared_arena->stats.peak_usage <= shared_arena->size);

	arena_delete(&shared_arena);
	puts("🎉 All calloc multithread tests passed.");
	return 0;
}
