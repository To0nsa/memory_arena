#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define THREAD_COUNT 4
#define ELEMENTS 128
#define ELEMENT_SIZE 32

void test_calloc_normal_usage(void)
{
	t_arena* arena = arena_create(4096, true);
	assert(arena);

	void* ptr = arena_calloc(arena, 4, 16);
	assert(ptr != NULL);
	for (size_t i = 0; i < 64; ++i)
		assert(((unsigned char*) ptr)[i] == 0);

	void* ptr2 = arena_calloc(arena, 128, 32);
	assert(ptr2 != NULL);
	for (size_t i = 0; i < 128 * 32; ++i)
		assert(((unsigned char*) ptr2)[i] == 0);

	assert(arena->stats.allocations == 2);
	assert(arena->stats.bytes_allocated >= 64 + 128 * 32);
	arena_delete(&arena);
	printf("✅ test_calloc_normal_usage passed\n");
}

void test_calloc_edge_cases(void)
{
	t_arena* arena = arena_create(1024, true);
	assert(arena);

	assert(arena_calloc(arena, 0, 64) == NULL);
	assert(arena_calloc(arena, 64, 0) == NULL);
	assert(arena_calloc(NULL, 64, 64) == NULL);

	// Force overflow
	size_t max = (size_t) -1;
	assert(arena_calloc(arena, max / 2 + 1, 2) == NULL);

	arena_delete(&arena);
	printf("✅ test_calloc_edge_cases passed\n");
}

void* thread_calloc_func(void* arg)
{
	t_arena* arena = (t_arena*) arg;
	void*    ptr   = arena_calloc(arena, ELEMENTS, ELEMENT_SIZE);
	assert(ptr != NULL);
	for (size_t i = 0; i < ELEMENTS * ELEMENT_SIZE; ++i)
		assert(((unsigned char*) ptr)[i] == 0);
	return NULL;
}

int main(void)
{
	test_calloc_normal_usage();
	test_calloc_edge_cases();
	printf("🎉 arena_calloc tests passed\n");
	return 0;
}