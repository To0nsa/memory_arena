#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 4
#define ALLOC_SIZE 128
#define ALLOC_CYCLES 10

static t_arena* shared_arena = NULL;

void* thread_alloc_and_metrics(void* arg)
{
	int id = *(int*) arg;
	for (int i = 0; i < ALLOC_CYCLES; ++i)
	{
		void* ptr = NULL;
		ARENA_LOCK(shared_arena);
		ptr = arena_alloc(shared_arena, ALLOC_SIZE);
		if (ptr)
			memset(ptr, id, ALLOC_SIZE);
		size_t used      = arena_used(shared_arena);
		size_t remaining = arena_remaining(shared_arena);
		size_t peak      = arena_peak(shared_arena);
		assert(used + remaining == shared_arena->size);
		assert(peak >= used);
		ARENA_UNLOCK(shared_arena);
		usleep(200);
	}
	return NULL;
}

void* thread_marker_pop(void* arg)
{
	int id = *(int*) arg;
	for (int i = 0; i < ALLOC_CYCLES / 2; ++i)
	{
		t_arena_marker m;
		ARENA_LOCK(shared_arena);
		m          = arena_mark(shared_arena);
		void* ptr1 = arena_alloc(shared_arena, ALLOC_SIZE);
		void* ptr2 = arena_alloc(shared_arena, ALLOC_SIZE);
		if (ptr1)
			memset(ptr1, id, ALLOC_SIZE);
		if (ptr2)
			memset(ptr2, id, ALLOC_SIZE);
		arena_pop(shared_arena, m);
		ARENA_UNLOCK(shared_arena);
		usleep(200);
	}
	return NULL;
}

void* thread_reset_loop(void* arg)
{
	(void) arg;
	for (int i = 0; i < ALLOC_CYCLES / 2; ++i)
	{
		ARENA_LOCK(shared_arena);
		arena_reset(shared_arena);
		assert(arena_used(shared_arena) == 0);
		ARENA_UNLOCK(shared_arena);
		usleep(250);
	}
	return NULL;
}

int main(void)
{
	printf("[TEST] arena metric + control test (multithreaded)\n");
	shared_arena = arena_create(8192, true);
	assert(shared_arena);

	pthread_t threads[THREAD_COUNT];
	int       ids[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
		ids[i] = i + 1;

	pthread_create(&threads[0], NULL, thread_alloc_and_metrics, &ids[0]);
	pthread_create(&threads[1], NULL, thread_marker_pop, &ids[1]);
	pthread_create(&threads[2], NULL, thread_alloc_and_metrics, &ids[2]);
	pthread_create(&threads[3], NULL, thread_reset_loop, NULL);

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);

	arena_destroy(shared_arena);
	arena_delete(&shared_arena);
	return 0;
}