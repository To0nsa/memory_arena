#include "arena_scratch.h"
#include "arena_stack.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 8
#define CYCLES_PER_THREAD 100
#define STACK_ALLOC_SIZE 64

typedef struct s_thread_info
{
	int                   thread_id;
	t_scratch_arena_pool* pool;
} t_thread_info;

void* thread_stack_test(void* arg)
{
	t_thread_info* info = (t_thread_info*) arg;
	int            tid  = info->thread_id;

	t_arena* arena = scratch_acquire(info->pool);
	assert(arena != NULL);

	t_arena_stack stack;
	arena_stack_init(&stack, arena);

	for (int i = 0; i < CYCLES_PER_THREAD; ++i)
	{
		arena_stack_push(&stack);

		int* data = (int*) arena_alloc(arena, STACK_ALLOC_SIZE);
		assert(data != NULL);

		for (size_t j = 0; j < STACK_ALLOC_SIZE / sizeof(int); ++j)
			data[j] = tid;

		for (size_t j = 0; j < STACK_ALLOC_SIZE / sizeof(int); ++j)
			assert(data[j] == tid);

		size_t offset_before = arena->offset;
		arena_stack_pop(&stack);
		assert(arena->offset <= offset_before);
	}

	arena_stack_pop(&stack);
	arena_stack_clear(&stack);
	arena_stack_push(&stack);
	arena_stack_pop(&stack);

	scratch_release(info->pool, arena);
	return NULL;
}

int main(void)
{
	printf("🧪 arena_stack multithread test\n");

	t_scratch_arena_pool pool;
	assert(scratch_pool_init(&pool, 4096, true));

	pthread_t     threads[THREAD_COUNT];
	t_thread_info info[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		info[i].thread_id = i;
		info[i].pool      = &pool;
		pthread_create(&threads[i], NULL, thread_stack_test, &info[i]);
	}

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);

	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		assert(atomic_load(&pool.slots[i].in_use) == false);
		assert(pool.slots[i].arena.offset <= pool.slots[i].arena.size);
	}

	scratch_pool_destroy(&pool);
	printf("✅ arena_stack multithread test passed\n");
	return 0;
}
