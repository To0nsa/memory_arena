#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREADS 4
#define GROW_SIZE 512
#define SHRINK_TARGET 64
#define INIT_SIZE 128
#define CYCLES 10

static t_arena* shared_arena = NULL;

void* thread_grow_worker(void* arg)
{
	(void)arg;
	for (int i = 0; i < CYCLES; ++i)
	{
		ARENA_LOCK(shared_arena);
		assert(arena_grow(shared_arena, GROW_SIZE) == true);
		assert(shared_arena->size >= shared_arena->offset + GROW_SIZE);
		ARENA_UNLOCK(shared_arena);
		usleep(100);
	}
	return NULL;
}

void* thread_shrink_worker(void* arg)
{
	long tid = (long)arg;
	for (int i = 0; i < CYCLES; ++i)
	{
		void* temp = arena_alloc(shared_arena, SHRINK_TARGET / 2);
		if (!temp)
			fprintf(stderr, "[shrink %ld] Failed to simulate underuse\n", tid);

		bool did_shrink = arena_might_shrink(shared_arena);
		if (!did_shrink) {
			ARENA_LOCK((t_arena*)shared_arena);
			fprintf(stderr, "[arena] size=%zu, offset=%zu\n", shared_arena->size, shared_arena->offset);
			ARENA_UNLOCK((t_arena*)shared_arena);
		}

		usleep(1000);
	}
	return NULL;
}

void test_manual_shrink(void)
{
	size_t before = shared_arena->size;

	ARENA_LOCK(shared_arena);
	shared_arena->offset = SHRINK_TARGET;
	ARENA_UNLOCK(shared_arena);

	arena_shrink(shared_arena, SHRINK_TARGET);
	assert(shared_arena->size <= before);
}

void test_invalid_grow_shrink(void)
{
	ARENA_LOCK(shared_arena);
	atomic_store(&shared_arena->can_grow, false);
	assert(arena_grow(shared_arena, 1) == false);
	ARENA_UNLOCK(shared_arena);

	atomic_store(&shared_arena->can_grow, true);
	ARENA_LOCK(shared_arena);
	shared_arena->offset = 128;
	assert(shared_arena->offset > SHRINK_TARGET);
	arena_shrink(shared_arena, SHRINK_TARGET); // should be ignored
	ARENA_UNLOCK(shared_arena);
}

int main(void)
{
	shared_arena = arena_create(INIT_SIZE, true);
	assert(shared_arena);

	test_manual_shrink();
	test_invalid_grow_shrink();

	pthread_t growers[THREADS];
	pthread_t shrinkers[THREADS];

	for (long i = 0; i < THREADS; ++i)
		pthread_create(&growers[i], NULL, thread_grow_worker, (void*)i);

	for (long i = 0; i < THREADS; ++i)
		pthread_create(&shrinkers[i], NULL, thread_shrink_worker, (void*)i);

	for (int i = 0; i < THREADS; ++i)
	{
		pthread_join(growers[i], NULL);
		pthread_join(shrinkers[i], NULL);
	}

	assert(shared_arena->stats.reallocations > 0);
	assert(shared_arena->stats.shrinks > 0);

	printf("✅ All resize functions tested successfully.\n");

	arena_destroy(shared_arena);
	arena_delete(&shared_arena);
	return 0;
}
