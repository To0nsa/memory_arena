#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREAD_COUNT 8
#define INITIAL_ALLOC 64
#define REALLOC_LARGE 128
#define REALLOC_SMALL 32

static t_arena*   shared_arena;
static atomic_int realloc_count = 0;

static void hook_callback(t_arena* arena, int id, void* ptr, size_t size, size_t offset, size_t wasted,
	const char* label)
{
(void) arena;
(void) id;
(void) ptr;
(void) size;
(void) offset;
(void) wasted;

if (label && strcmp(label, "arena_realloc_last (in-place)") == 0)
	atomic_fetch_add(&realloc_count, 1);
}

static void* thread_func(void* arg)
{
	int     tid     = *(int*) arg;
	uint8_t pattern = (uint8_t) tid;

	void* ptr = arena_alloc(shared_arena, INITIAL_ALLOC);
	assert(ptr);

	memset(ptr, pattern, INITIAL_ALLOC);

	void* new_ptr = arena_realloc_last(shared_arena, ptr, INITIAL_ALLOC, REALLOC_LARGE);
	assert(new_ptr);
	for (size_t i = 0; i < INITIAL_ALLOC; ++i)
		assert(((uint8_t*) new_ptr)[i] == pattern);

	void* shrunk = arena_realloc_last(shared_arena, new_ptr, REALLOC_LARGE, REALLOC_SMALL);
	assert(shrunk != NULL);

	for (size_t i = 0; i < REALLOC_SMALL; ++i)
		assert(((uint8_t*) shrunk)[i] == pattern);

	assert(arena_realloc_last(NULL, ptr, INITIAL_ALLOC, REALLOC_LARGE) == NULL);
	assert(arena_realloc_last(shared_arena, NULL, INITIAL_ALLOC, REALLOC_LARGE) == NULL);
	assert(arena_realloc_last(shared_arena, ptr, INITIAL_ALLOC, 0) == NULL);

	return NULL;
}

int main(void)
{
	shared_arena = arena_create(1024 * 1024, true);
	assert(shared_arena);

	shared_arena->hooks.hook_cb = hook_callback;

	pthread_t threads[THREAD_COUNT];
	int       ids[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		ids[i] = i;
		pthread_create(&threads[i], NULL, thread_func, &ids[i]);
	}

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);

	size_t expected_valid_reallocs = THREAD_COUNT * 2;

	printf("✅ Reallocation hook calls:   %d (expected %zu)\n", atomic_load(&realloc_count), expected_valid_reallocs);
	printf("✅ Arena reallocations stat:  %zu\n", shared_arena->stats.reallocations);
	printf("✅ Arena bytes allocated:     %zu\n", shared_arena->stats.bytes_allocated);
	printf("✅ Arena size:                %zu\n", shared_arena->size);
	printf("✅ Arena offset:              %zu\n", shared_arena->offset);

	assert(shared_arena->offset <= shared_arena->size);
	assert(shared_arena->stats.reallocations == expected_valid_reallocs);
	assert(shared_arena->stats.live_allocations >= THREAD_COUNT);
	assert(shared_arena->stats.bytes_allocated > 0);

	arena_delete(&shared_arena);
	puts("🎉 All multithreaded realloc_last tests passed.");
	return 0;
}
