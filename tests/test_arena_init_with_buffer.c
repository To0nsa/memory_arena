#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_init_with_valid_buffer(void)
{
	uint8_t buffer[128];
	t_arena arena = {0};
	arena_init_with_buffer(&arena, buffer, sizeof(buffer), false);

	assert(arena.buffer == buffer);
	assert(arena.size == sizeof(buffer));
	assert(atomic_load_explicit(&arena.owns_buffer, memory_order_acquire) == false);
	assert(atomic_load_explicit(&arena.can_grow, memory_order_acquire) == false);

	arena_destroy(&arena);
	printf("✅ test_init_with_valid_buffer passed\n");
}

static void test_init_with_malloc_buffer(void)
{
	t_arena arena = {0};
	arena_init_with_buffer(&arena, NULL, 64, true);

	assert(arena.buffer != NULL);
	assert(arena.size == 64);
	assert(atomic_load_explicit(&arena.owns_buffer, memory_order_acquire) == true);
	assert(atomic_load_explicit(&arena.can_grow, memory_order_acquire) == true);

	arena_destroy(&arena);
	printf("✅ test_init_with_malloc_buffer passed\n");
}

static void test_init_with_zero_size_and_null_buffer(void)
{
	t_arena arena = {0};
	arena_init_with_buffer(&arena, NULL, 0, false);
	assert(arena.buffer == NULL);
	assert(arena.size == 0);
	arena_destroy(&arena);
	printf("✅ test_init_with_zero_size_and_null_buffer passed\n");
}

static void test_reinit_with_buffer_reuses_struct(void)
{
	t_arena arena = {0};
	arena_init_with_buffer(&arena, NULL, 64, false);
	void* new_buffer = malloc(64);
	assert(new_buffer);
	arena_reinit_with_buffer(&arena, new_buffer, 64, false);
	assert(arena.buffer == new_buffer);
	assert(arena.offset == 0);
	assert(arena.stats.allocations == 0);
	assert(arena.size == 64);
	arena_destroy(&arena);
	free(new_buffer);
	printf("✅ test_reinit_with_buffer_reuses_struct passed\n");
}

int main(void)
{
	test_init_with_valid_buffer();
	test_init_with_malloc_buffer();
	test_init_with_zero_size_and_null_buffer();
	test_reinit_with_buffer_reuses_struct();
	printf("🎉 All arena_init_with_buffer tests passed.\n");
	return 0;
}
