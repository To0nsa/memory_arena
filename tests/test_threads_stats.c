
#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 8
#define ARENA_SIZE 8192
#define MAX_TOTAL_ALLOC_PER_THREAD (64 * 1024)

typedef struct
{
	int      thread_id;
	t_arena* arena;
} thread_arg_t;

void* thread_alloc_and_track(void* arg)
{
	thread_arg_t* targ  = (thread_arg_t*) arg;
	t_arena*      arena = targ->arena;
	int           id    = targ->thread_id;

	size_t total_allocated = 0;
	while (total_allocated < MAX_TOTAL_ALLOC_PER_THREAD)
	{
		size_t size = 64 + (rand() % 128);
		void*  ptr  = arena_alloc(arena, size);
		if (!ptr)
			break;

		memset(ptr, id, size);
		total_allocated += size;
		usleep(500);
	}

	return NULL;
}

int main(void)
{
	t_arena* arena = arena_create(ARENA_SIZE, true);
	assert(arena != NULL);

	pthread_t    threads[THREAD_COUNT];
	thread_arg_t args[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		args[i].thread_id = i;
		args[i].arena     = arena;
		pthread_create(&threads[i], NULL, thread_alloc_and_track, &args[i]);
	}

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);

	arena_print_stats(arena, stdout);
	arena_destroy(arena);
	arena_delete(&arena);

	printf("Threaded allocation test completed within safe bounds.\n");
	return 0;
}
