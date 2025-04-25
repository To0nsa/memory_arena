#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_arena_used_remaining(void)
{
	assert(arena_used(NULL) == 0);
	assert(arena_remaining(NULL) == 0);

	t_arena arena;
	arena_init(&arena, 256, false);

	assert(arena_used(&arena) == 0);
	assert(arena_remaining(&arena) == 256);

	void* p = arena_alloc(&arena, 64);
	assert(p);
	assert(arena_used(&arena) == 64);
	assert(arena_remaining(&arena) == 192);

	arena_destroy(&arena);
	printf("✅ test_arena_used_remaining passed\n");
}

static void test_arena_peak_behavior(void)
{
	t_arena arena;
	arena_init(&arena, 256, false);

	assert(arena_peak(&arena) == 0);

	arena_alloc(&arena, 50);
	size_t after_first = arena.offset;
	assert(arena_peak(&arena) == after_first);

	arena_alloc(&arena, 30);
	size_t after_second = arena.offset;
	assert(arena_peak(&arena) == after_second);

	t_arena_marker mark = arena_mark(&arena);
	arena_alloc(&arena, 20);
	size_t after_third = arena.offset;
	assert(arena_peak(&arena) == after_third);

	arena_pop(&arena, mark);
	assert(arena.offset == mark);
	assert(arena_peak(&arena) == after_third);

	arena_reset(&arena);
	assert(arena.offset == 0);
	assert(arena_peak(&arena) == after_third);

	arena_destroy(&arena);
	printf("✅ test_arena_peak_behavior passed\n");
}

static void test_arena_mark_pop_mechanics(void)
{
	t_arena arena;
	arena_init(&arena, 128, false);

	char* block1 = arena_alloc(&arena, 32);
	assert(block1);
	strcpy(block1, "keep");

	t_arena_marker mark = arena_mark(&arena);

	char* block2 = arena_alloc(&arena, 32);
	assert(block2);
	strcpy(block2, "discard");

	arena_pop(&arena, mark);
	assert(arena_used(&arena) == mark);
	assert(strcmp(block1, "keep") == 0);

	arena_destroy(&arena);
	printf("✅ test_arena_mark_pop_mechanics passed\n");
}

static void test_arena_pop_edge_cases(void)
{
	arena_pop(NULL, 0);

	t_arena arena;
	arena_init(&arena, 64, false);
	arena_alloc(&arena, 32);

	t_arena_marker invalid = 1000;
	arena_pop(&arena, invalid);
	assert(arena_used(&arena) == 32);

	arena_destroy(&arena);
	printf("✅ test_arena_pop_edge_cases passed\n");
}

static void test_arena_reset_behavior(void)
{
	t_arena arena;
	arena_init(&arena, 128, false);

	arena_alloc(&arena, 64);
	assert(arena_used(&arena) == 64);

	arena_reset(&arena);
	assert(arena_used(&arena) == 0);
	assert(arena_remaining(&arena) == 128);

	arena_destroy(&arena);
	printf("✅ test_arena_reset_behavior passed\n");
}

int main(void)
{
	test_arena_used_remaining();
	test_arena_peak_behavior();
	test_arena_mark_pop_mechanics();
	test_arena_pop_edge_cases();
	test_arena_reset_behavior();
	printf("\n🎉 All arena utility function tests passed.\n");
	return 0;
}
