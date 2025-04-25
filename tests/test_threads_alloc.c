
#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define THREAD_COUNT 16
#define ALLOCS_PER_THREAD 500
#define MAX_ALLOC_SIZE 256

typedef struct
{
	t_arena*       arena;
	int            thread_id;
	size_t         alloc_count;
	atomic_size_t* global_alloc_counter;
} thread_args;

typedef struct
{
	void*  ptr;
	size_t size;
	size_t offset;
	int    thread_id;
} alloc_record;

#define MAX_RECORDS (THREAD_COUNT * ALLOCS_PER_THREAD)
static alloc_record  records[MAX_RECORDS];
static atomic_size_t record_index = 0;

void allocation_hook(t_arena* arena, int id, void* ptr, size_t size, size_t offset, size_t wasted, const char* label)
{
	size_t i = atomic_fetch_add(&record_index, 1);
	if (i < MAX_RECORDS)
	{
		records[i].ptr       = ptr;
		records[i].size      = size;
		records[i].offset    = offset;
		records[i].thread_id = id;
	}
	(void) arena;
	(void) wasted;
	(void) label;
}

void* thread_func(void* arg)
{
	thread_args* args = (thread_args*) arg;
	for (size_t i = 0; i < ALLOCS_PER_THREAD; ++i)
	{
		size_t size      = (rand() % MAX_ALLOC_SIZE) + 1;
		size_t alignment = 1 << ((rand() % 5) + 3);
		int    variant   = rand() % 4;

		void* ptr = NULL;
		switch (variant)
		{
		case 0:
			ptr = arena_alloc(args->arena, size);
			break;
		case 1:
			ptr = arena_alloc_aligned(args->arena, size, alignment);
			break;
		case 2:
			ptr = arena_alloc_labeled(args->arena, size, NULL);
			break;
		case 3:
			ptr = arena_alloc_aligned_labeled(args->arena, size, alignment, NULL);
			break;
		}

		if (ptr)
			memset(ptr, args->thread_id, size); // mark memory
		atomic_fetch_add(args->global_alloc_counter, 1);
	}
	return NULL;
}

int main(void)
{
	srand((unsigned int) time(NULL));

	t_arena* arena = arena_create(4096, true);
	assert(arena);
	arena_set_allocation_hook(arena, allocation_hook, NULL);

	pthread_t     threads[THREAD_COUNT];
	thread_args   args[THREAD_COUNT];
	atomic_size_t global_allocs = 0;

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		args[i].arena                = arena;
		args[i].thread_id            = i;
		args[i].global_alloc_counter = &global_allocs;
		pthread_create(&threads[i], NULL, thread_func, &args[i]);
	}

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		pthread_join(threads[i], NULL);
	}

	printf("✅ Total allocations: %zu\n", global_allocs);
	printf("✅ Arena offset: %zu\n", arena->offset);
	printf("✅ Arena stats.allocations: %zu\n", arena->stats.allocations);
	printf("✅ Arena stats.live_allocations: %zu\n", arena->stats.live_allocations);
	printf("✅ Arena bytes allocated: %zu\n", arena->stats.bytes_allocated);
	printf("✅ Final arena size: %zu\n", arena->size);

	assert(arena->offset <= arena->size);
	assert(arena->stats.allocations == global_allocs);
	assert(arena->stats.live_allocations == global_allocs);

	// Optional: overlap check
	for (size_t i = 0; i < record_index; ++i)
	{
		for (size_t j = i + 1; j < record_index; ++j)
		{
			if ((records[i].ptr < records[j].ptr + records[j].size) &&
			    (records[j].ptr < records[i].ptr + records[i].size))
			{
				fprintf(stderr, "❌ Overlap between %zu and %zu\n", i, j);
				exit(1);
			}
		}
	}

	arena_destroy(arena);
	arena_delete(&arena);
	printf("🎉 All multithreaded allocation tests passed.\n");
	return 0;
}
