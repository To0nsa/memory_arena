#include "arena.h"
#include "arena_tlscratch.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4

#ifdef ARENA_ENABLE_THREAD_LOCAL_SCRATCH

void test_thread_scratch_basic_usage(void)
{
	t_arena* a1 = get_thread_scratch_arena();
	assert(a1 != NULL);
	assert(arena_is_valid(a1));
	size_t offset1 = a1->offset;

	void* mem1 = arena_alloc(a1, 64);
	assert(mem1 != NULL);
	assert(a1->offset > offset1);

	t_arena* a2 = get_thread_scratch_arena();
	assert(a2 == a1);
	assert(a2->offset == 0);
	assert(arena_is_valid(a2));

	printf("✅ basic usage passed\n");
}

void test_thread_scratch_repeated_reuse(void)
{
	t_arena* arena = NULL;
	for (int i = 0; i < 10; ++i)
	{
		t_arena* a = get_thread_scratch_arena();
		assert(a != NULL);
		assert(arena_is_valid(a));
		if (arena == NULL)
			arena = a;
		else
			assert(a == arena);
		assert(a->offset == 0);
		arena_alloc(a, 64);
	}
	printf("✅ repeated reuse passed\n");
}

void test_thread_scratch_double_destroy(void)
{
	destroy_thread_scratch_arena();
	destroy_thread_scratch_arena();
	printf("✅ double destroy passed\n");
}

void* thread_scratch_entry(void* result)
{
	t_arena* a = get_thread_scratch_arena();
	assert(a != NULL);
	*(t_arena**) result = a;
	destroy_thread_scratch_arena();
	return NULL;
}

void test_thread_scratch_isolation(void)
{
	pthread_t threads[THREADS];
	t_arena*  results[THREADS] = {0};

	for (int i = 0; i < THREADS; ++i)
		pthread_create(&threads[i], NULL, thread_scratch_entry, &results[i]);

	for (int i = 0; i < THREADS; ++i)
		pthread_join(threads[i], NULL);

	for (int i = 0; i < THREADS; ++i)
		for (int j = i + 1; j < THREADS; ++j)
			assert(results[i] != results[j]);

	printf("✅ thread-local isolation passed\n");
}

void test_thread_scratch_destroy_and_recreate(void)
{
	t_arena* a1 = get_thread_scratch_arena();
	assert(a1 != NULL);
	void* m1 = arena_alloc(a1, 32);
	assert(m1 != NULL);

	destroy_thread_scratch_arena();

	t_arena* a2 = get_thread_scratch_arena();
	assert(a2 != NULL);
	assert(a2->offset == 0);
	assert(arena_is_valid(a2));

	printf("✅ destroy and recreate passed\n");
}

#else

void test_thread_scratch_basic_usage(void)
{
	t_arena* a = get_thread_scratch_arena();
	assert(a == NULL);
	printf("✅ basic usage (fallback) passed\n");
}

void test_thread_scratch_destroy_and_recreate(void)
{
	destroy_thread_scratch_arena(); // No-op
	printf("✅ destroy and recreate (fallback) passed\n");
}

void test_thread_scratch_repeated_reuse(void)
{
}
void test_thread_scratch_double_destroy(void)
{
}
void test_thread_scratch_isolation(void)
{
}

#endif // ARENA_ENABLE_THREAD_LOCAL_SCRATCH

int main(void)
{
	printf("[TEST] thread-local scratch arena\n");
	test_thread_scratch_basic_usage();
	test_thread_scratch_destroy_and_recreate();
	test_thread_scratch_repeated_reuse();
	test_thread_scratch_double_destroy();
	test_thread_scratch_isolation();
	printf("🎉 all thread-scratch tests passed\n");
	return 0;
}
