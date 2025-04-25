#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 16
#define ALLOC_STEP 128

static t_arena* shared_arena;

void* thread_peak_update(void* arg)
{
	(void) arg;
	for (int i = 1; i <= 10; ++i)
	{
		size_t alloc_size = i * ALLOC_STEP;
		void*  mem        = arena_alloc(shared_arena, alloc_size);
		assert(mem != NULL);
		arena_update_peak(shared_arena);
	}
	return NULL;
}

void test_arena_update_peak_concurrency()
{
	shared_arena = arena_create(4096 * 16, true);
	assert(shared_arena);

	pthread_t threads[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_create(&threads[i], NULL, thread_peak_update, NULL);
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);

	assert(shared_arena->stats.peak_usage >= shared_arena->offset);
	printf("✅ arena_update_peak: peak_usage = %zu\n", shared_arena->stats.peak_usage);

	arena_destroy(shared_arena);
	arena_zero_metadata(shared_arena);
	arena_delete(&shared_arena);
}

void* thread_is_valid_check()
{
	t_arena* a = arena_create(512, false);
	assert(a);
	for (int i = 0; i < 100; ++i)
	{
		assert(arena_is_valid(a) == true);
		usleep(100);
	}
	arena_destroy(a);
	arena_zero_metadata(a);
	arena_delete(&a);
	return NULL;
}

void test_arena_is_valid_concurrent()
{
	pthread_t threads[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_create(&threads[i], NULL, thread_is_valid_check, NULL);
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);
	printf("✅ arena_is_valid: concurrent use passed\n");
}

void* thread_zero_metadata_conflict()
{
	t_arena* a = arena_create(2048, false);
	assert(a);
	arena_alloc(a, 64);
	usleep(100);

	arena_destroy(a);
	arena_zero_metadata(a);
	arena_delete(&a);
	return NULL;
}

void test_arena_zero_metadata_conflict()
{
	pthread_t t;
	pthread_create(&t, NULL, thread_zero_metadata_conflict, NULL);
	pthread_join(t, NULL);
	printf("⚠️ arena_zero_metadata: test ran — check with TSAN for safety\n");
}

void* thread_grow_cb_test(void* arg)
{
	(void) arg;
	for (int i = 1; i <= 1000; ++i)
	{
		size_t current   = rand() % 4096 + 1;
		size_t requested = rand() % 8192 + 1;
		size_t result    = default_grow_cb(current, requested);
		assert(result >= current + requested || result == SIZE_MAX);
	}
	return NULL;
}

void test_default_grow_cb_concurrency()
{
	pthread_t threads[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_create(&threads[i], NULL, thread_grow_cb_test, NULL);
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);
	printf("✅ default_grow_cb: all threads passed validation\n");
}

int main(void)
{
	test_arena_update_peak_concurrency();
	test_arena_is_valid_concurrent();
	test_arena_zero_metadata_conflict();
	test_default_grow_cb_concurrency();
	return 0;
}
