#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREADS 8
#define ALLOCATIONS_PER_THREAD 32

static t_arena*   shared_arena  = NULL;
static atomic_int hook_counter  = 0;
static atomic_int failed_allocs = 0;

void test_hook_cb(t_arena* arena, int alloc_id, void* ptr, size_t size, size_t offset, size_t wasted, const char* label)
{
	(void) alloc_id;
	(void) wasted;
	assert(arena != NULL);
	assert(ptr != NULL);
	assert(size > 0);
	assert(offset < arena->size);
	assert(label != NULL);
	atomic_fetch_add(&hook_counter, 1);

	printf("[arena] %s: Allocated %zu bytes @ offset %zu (arena %p)\n", label, size, offset, (void*) arena);
}

void* thread_alloc(void* arg)
{
	int  id = *(int*) arg;
	char label[64];
	for (int i = 0; i < ALLOCATIONS_PER_THREAD; ++i)
	{
		snprintf(label, sizeof(label), "alloc-%d", id);
		void* p = arena_alloc_labeled(shared_arena, 64, label);
		if (!p)
		{
			atomic_fetch_add(&failed_allocs, 1);
		}
		usleep(100); // let other threads run
	}
	return NULL;
}

void* thread_change_hook(void* arg)
{
	(void) arg;
	for (int i = 0; i < 10; ++i)
	{
		usleep(500);
		arena_set_allocation_hook(shared_arena, test_hook_cb, NULL);
	}
	return NULL;
}

void test_allocation_hook_multithreaded(void)
{
	shared_arena = arena_create(65536, true);
	assert(shared_arena);

	// Register the hook
	arena_set_allocation_hook(shared_arena, test_hook_cb, NULL);
	__sync_synchronize(); // memory barrier

	pthread_t threads[THREADS];
	int       ids[THREADS];

	// Allocation threads
	for (int i = 0; i < THREADS; ++i)
	{
		ids[i] = i;
		pthread_create(&threads[i], NULL, thread_alloc, &ids[i]);
	}

	// One extra thread changing the hook
	pthread_t hook_changer;
	pthread_create(&hook_changer, NULL, thread_change_hook, NULL);

	for (int i = 0; i < THREADS; ++i)
		pthread_join(threads[i], NULL);

	pthread_join(hook_changer, NULL);

	int total_expected = THREADS * ALLOCATIONS_PER_THREAD;
	int total_handled  = atomic_load(&hook_counter) + atomic_load(&failed_allocs);
	printf("[hook] Observed hooks: %d / Expected: %d\n", hook_counter, total_expected);

	assert(total_handled >= total_expected);
	assert(arena_is_valid(shared_arena));

	arena_set_allocation_hook(shared_arena, NULL, NULL);
	arena_destroy(shared_arena);
	arena_delete(&shared_arena);
	printf("✅ test_allocation_hook_multithreaded passed\n");
}

void test_hook_null_inputs(void)
{
	arena_set_allocation_hook(NULL, test_hook_cb, NULL);
	arena_set_allocation_hook(NULL, NULL, NULL);
	shared_arena = arena_create(128, true);
	arena_set_allocation_hook(shared_arena, NULL, NULL);
	arena_destroy(shared_arena);
	arena_delete(&shared_arena);
	printf("✅ test_hook_null_inputs passed\n");
}

int main(void)
{
	test_allocation_hook_multithreaded();
	test_hook_null_inputs();
	printf("🎉 All hook-related tests passed.\n");
	return 0;
}