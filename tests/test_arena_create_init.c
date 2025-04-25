
#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_create_normal(void)
{
	t_arena* arena = arena_create(512, true);
	assert(arena);
	assert(arena->buffer != NULL);
	assert(arena->size == 512);

	assert(atomic_load_explicit(&arena->can_grow, memory_order_acquire) == true);
	assert(atomic_load_explicit(&arena->owns_buffer, memory_order_acquire) == true);

	assert(arena->grow_cb != NULL);
	assert(strlen(arena->debug.id) > 0);

	arena_delete(&arena);
	printf("✅ test_create_normal passed\n");
}

static void test_create_zero_size(void)
{
	t_arena* arena = arena_create(0, false);
	assert(arena == NULL);
	printf("✅ test_create_zero_size passed\n");
}

static void test_init_normal(void)
{
	t_arena arena;
	bool    ok = arena_init(&arena, 512, true);
	assert(ok);
	assert(arena.buffer != NULL);
	assert(arena.size == 512);

	assert(atomic_load_explicit(&arena.can_grow, memory_order_acquire) == true);
	assert(atomic_load_explicit(&arena.owns_buffer, memory_order_acquire) == true);

	assert(arena.grow_cb != NULL);
	assert(strlen(arena.debug.id) > 0);

	arena_destroy(&arena);
	printf("✅ test_init_normal passed\n");
}

static void test_init_null_arena(void)
{
	bool ok = arena_init(NULL, 128, false);
	assert(ok == false);
	printf("✅ test_init_null_arena passed\n");
}

static void test_init_zero_size(void)
{
	t_arena arena;
	bool    ok = arena_init(&arena, 0, false);
	assert(ok == false);
	printf("✅ test_init_zero_size passed\n");
}

static void test_destroy_cleans_up(void)
{
	t_arena* arena = arena_create(256, false);
	assert(arena);
	void* ptr = arena_alloc(arena, 128);
	assert(ptr != NULL);
	arena_destroy(arena);
	assert(arena->buffer == NULL);
	arena_delete(&arena);
	printf("✅ test_destroy_cleans_up passed\n");
}

static void test_debug_id_format(void)
{
	t_arena* arena = arena_create(256, false);
	assert(arena);
	assert(strlen(arena->debug.id) > 0);
	assert(strchr(arena->debug.id, '#') != NULL);
	arena_delete(&arena);
	printf("✅ test_debug_id_format passed\n");
}

int main(void)
{
	test_create_normal();
	test_create_zero_size();
	test_init_normal();
	test_init_null_arena();
	test_init_zero_size();
	test_destroy_cleans_up();
	test_debug_id_format();
	printf("🎉 All arena_create/init tests passed.\n");
	return 0;
}
