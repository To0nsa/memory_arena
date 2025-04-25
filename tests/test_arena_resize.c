
#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_arena_grow_normal(void)
{
	t_arena* arena = arena_create(64, true);
	assert(arena);
	size_t old_size = arena->size;

	bool result = arena_grow(arena, 128);
	assert(result);
	assert(arena->size > old_size);
	assert(arena->stats.reallocations == 1);
	assert(arena->stats.growth_history_count == 1);
	assert(arena->stats.growth_history[0] == old_size);

	arena_delete(&arena);
	printf("✅ test_arena_grow_normal passed\n");
}

static size_t too_small_cb(size_t current, size_t requested)
{
	(void) requested;
	return current + 1;
}

static void test_arena_grow_edge_cases(void)
{
	assert(!arena_grow(NULL, 128));

	// growth disabled
	t_arena* arena = arena_create(64, false);
	assert(arena);
	assert(!arena_grow(arena, 128));
	arena_delete(&arena);

	arena = arena_create(64, true);
	assert(arena);
	atomic_store_explicit(&arena->owns_buffer, false, memory_order_release);
	assert(!arena_grow(arena, 128));
	uint8_t* leaked_buf = arena->buffer; // clean up manually since arena won't free it
	arena_delete(&arena);
	free(leaked_buf);

	// zero-sized grow
	arena = arena_create(64, true);
	assert(arena);
	assert(arena_grow(arena, 0));
	arena_delete(&arena);

	// overflow case
	arena = arena_create(64, true);
	assert(arena);
	arena->offset = SIZE_MAX - 4;
	assert(!arena_grow(arena, 8));
	arena_delete(&arena);

	// grow_cb returns too small
	arena = arena_create(64, true);
	assert(arena);
	arena->grow_cb = too_small_cb;
	assert(!arena_grow(arena, 128));
	arena_delete(&arena);

	printf("✅ test_arena_grow_edge_cases passed\n");
}

static void test_arena_shrink_valid(void)
{
	t_arena* arena = arena_create(256, true);
	assert(arena);
	arena_alloc(arena, 64);

	size_t old_size = arena->size;
	arena_shrink(arena, 96);

	assert(arena->size <= old_size);
	assert(arena->stats.shrinks == 1);
	arena_delete(&arena);
	printf("✅ test_arena_shrink_valid passed\n");
}

static void test_arena_shrink_edge_cases(void)
{
	arena_shrink(NULL, 128);

	t_arena* arena = arena_create(128, false);
	assert(arena);
	arena_shrink(arena, 64);
	assert(arena->size == 128);
	arena_delete(&arena);

	arena = arena_create(128, true);
	assert(arena);
	uint8_t* leaked_buf = arena->buffer;
	atomic_store_explicit(&arena->owns_buffer, false, memory_order_release);
	arena_shrink(arena, 64);
	assert(arena->size == 128);
	arena_delete(&arena);
	free(leaked_buf);

	arena = arena_create(128, true);
	assert(arena);
	arena_alloc(arena, 64);
	arena_shrink(arena, 32);
	assert(arena->size == 128);
	arena_delete(&arena);

	printf("✅ test_arena_shrink_edge_cases passed\n");
}

static void test_arena_might_shrink(void)
{
	t_arena* arena = arena_create(1024, true);
	assert(arena);
	arena_alloc(arena, 64);

	size_t original_size = arena->size;
	arena_might_shrink(arena);
	assert(arena->size < original_size);
	assert(arena->stats.shrinks == 1);

	arena_delete(&arena);
	printf("✅ test_arena_might_shrink passed\n");
}

int main(void)
{
	test_arena_grow_normal();
	test_arena_grow_edge_cases();
	test_arena_shrink_valid();
	test_arena_shrink_edge_cases();
	test_arena_might_shrink();
	printf("\n🎉 All arena resize tests passed.\n");
	return 0;
}
