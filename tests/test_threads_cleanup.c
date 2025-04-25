#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define THREADS 8

static t_arena*        shared_arena                 = NULL;
static atomic_int      alloc_attempts_after_destroy = 0;
static pthread_mutex_t counter_lock                 = PTHREAD_MUTEX_INITIALIZER;

void* thread_alloc_destroy(void* arg)
{
	(void) arg;

	for (int i = 0; i < 50; ++i)
	{
		usleep(rand() % 1000);

		if (rand() % 10 == 0)
		{
			arena_destroy(shared_arena);
		}
		else
		{
			if (!atomic_load_explicit(&shared_arena->is_destroying, memory_order_acquire))
			{
				void* p = arena_alloc(shared_arena, 64);
				if (!p && atomic_load_explicit(&shared_arena->is_destroying, memory_order_acquire))
				{
					pthread_mutex_lock(&counter_lock);
					alloc_attempts_after_destroy++;
					pthread_mutex_unlock(&counter_lock);
				}
			}
		}
	}
	return NULL;
}

void test_threaded_arena_destroy(void)
{
	shared_arena = arena_create(1024, true);
	assert(shared_arena != NULL);

	pthread_t threads[THREADS];
	for (int i = 0; i < THREADS; ++i)
		pthread_create(&threads[i], NULL, thread_alloc_destroy, NULL);

	for (int i = 0; i < THREADS; ++i)
		pthread_join(threads[i], NULL);

	assert(shared_arena->buffer == NULL);
	assert(shared_arena->stats.growth_history == NULL);
	assert(shared_arena->use_lock == false);
	assert(!arena_is_valid(shared_arena));

	printf("✅ test_threaded_arena_destroy passed. %d allocs attempted after destroy\n", alloc_attempts_after_destroy);
	arena_delete(&shared_arena);
}

void test_destroy_null_arena(void)
{
	arena_destroy(NULL);
	arena_delete(NULL);
	printf("✅ test_destroy_null_arena passed\n");
}

void* thread_mass_alloc_delete(void* _)
{
	(void) _; // Unused

	t_arena* local = arena_create(128, true);
	for (int i = 0; i < 8; ++i)
		arena_alloc(local, 16);
	arena_delete(&local);
	assert(local == NULL);
	return NULL;
}

void test_massive_deletion(void)
{
	pthread_t threads[THREADS * 4];
	for (int i = 0; i < THREADS * 4; ++i)
		pthread_create(&threads[i], NULL, thread_mass_alloc_delete, NULL);
	for (int i = 0; i < THREADS * 4; ++i)
		pthread_join(threads[i], NULL);
	printf("✅ test_massive_deletion passed\n");
}

int main(void)
{
	test_threaded_arena_destroy();
	test_destroy_null_arena();
	test_massive_deletion();
	printf("🎉 test_threads_cleanup passed\n");
	return 0;
}
