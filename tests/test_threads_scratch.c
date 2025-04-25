#include "internal/arena_debug.h"
#include "arena_scratch.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 32
#define SLOT_COUNT 8
#define SCRATCH_SIZE 1024
#define ALLOC_SIZE 64
#define TEST_ITERATIONS 1000

static t_scratch_arena_pool pool;
static atomic_int           successful_acquisitions = 0;
static atomic_int           failed_acquisitions     = 0;

void* thread_worker(void* arg)
{
	int thread_id = *(int*) arg;
	for (int i = 0; i < TEST_ITERATIONS; ++i)
	{
		t_arena* arena = scratch_acquire(&pool);
		if (!arena)
		{
			atomic_fetch_add(&failed_acquisitions, 1);
			usleep(100);
			continue;
		}

		assert(arena->offset == 0);

		uint8_t* buffer = arena_alloc(arena, ALLOC_SIZE);
		assert(buffer);
		memset(buffer, (uint8_t) thread_id, ALLOC_SIZE);
		usleep(50);
		for (size_t j = 0; j < ALLOC_SIZE; ++j)
			assert(buffer[j] == (uint8_t) thread_id);

		atomic_fetch_add(&successful_acquisitions, 1);
		scratch_release(&pool, arena);
		usleep(50);
	}
	return NULL;
}

int main(void)
{
	printf("[TEST] Initializing scratch pool with %d slots\n", SLOT_COUNT);
	assert(scratch_pool_init(&pool, SCRATCH_SIZE, true));

	pthread_t threads[THREAD_COUNT];
	int       thread_ids[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		thread_ids[i] = i + 1;
		pthread_create(&threads[i], NULL, thread_worker, &thread_ids[i]);
	}

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);

	printf("[TEST] Verifying final scratch slot states\n");
	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		bool in_use = atomic_load(&pool.slots[i].in_use);
		assert(!in_use);
		assert(arena_is_valid(&pool.slots[i].arena));
	}

	printf("[PASS] All slots released.\n");
	printf("[INFO] Success: %d | Failures: %d\n", atomic_load(&successful_acquisitions),
	       atomic_load(&failed_acquisitions));

	scratch_pool_destroy(&pool);
	return 0;
}