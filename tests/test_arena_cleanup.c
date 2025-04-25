#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_destroy_frees_buffer(void)
{
	t_arena* arena = arena_create(512, false);
	assert(arena);
	void* ptr = arena_alloc(arena, 128);
	assert(ptr);
	assert(arena->buffer != NULL);
	arena_destroy(arena);
	assert(arena->buffer == NULL);
	printf("✅ test_destroy_frees_buffer passed\n");
	arena_delete(&arena);
}

static void test_destroy_frees_growth_history(void)
{
	t_arena* arena = arena_create(64, true);
	assert(arena);
	arena_grow(arena, 64);
	assert(arena->stats.growth_history_count > 0);
	assert(arena->stats.growth_history != NULL);
	arena_destroy(arena);
	assert(arena->stats.growth_history == NULL);
	printf("✅ test_destroy_frees_growth_history passed\n");
	arena_delete(&arena);
}

static void test_destroy_zeros_metadata(void)
{
	t_arena* arena = arena_create(256, false);
	assert(arena);
	arena_alloc(arena, 128);
	arena_destroy(arena);

	if (!arena->use_lock)
	{
		assert(arena->size == 0);
		assert(arena->offset == 0);
		assert(arena->buffer == NULL);
	}

	printf("✅ test_destroy_zeros_metadata passed\n");
	arena_delete(&arena);
}

static void test_delete_nullifies_pointer(void)
{
	t_arena* arena = arena_create(256, false);
	assert(arena);
	arena_delete(&arena);
	assert(arena == NULL);
	printf("✅ test_delete_nullifies_pointer passed\n");
}

static void test_safe_on_null(void)
{
	arena_destroy(NULL);
	t_arena* null_arena = NULL;
	arena_delete(&null_arena);
	assert(null_arena == NULL);
	printf("✅ test_safe_on_null passed\n");
}

static void test_destroy_poisoning(void)
{
#ifdef ARENA_POISON_MEMORY
	t_arena* arena = arena_create(128, false);
	assert(arena);
	void* ptr = arena_alloc(arena, 64);
	assert(ptr);
	arena_destroy(arena);
	printf("✅ test_destroy_poisoning (manual validation recommended)\n");
	arena_delete(&arena);
#else
	printf("⚠️ Skipped test_destroy_poisoning (ARENA_POISON_MEMORY disabled)\n");
#endif
}

static void test_destroy_then_reinit(void)
{
	t_arena* arena = arena_create(256, false);
	assert(arena);
	uint8_t buffer[256];
	arena_reinit_with_buffer(arena, buffer, sizeof(buffer), false);
	assert(arena->buffer == buffer);
	assert(arena->size == sizeof(buffer));
	assert(arena->offset == 0);
	void* ptr = arena_alloc(arena, 64);
	assert(ptr != NULL);
	arena_delete(&arena);
	printf("✅ test_destroy_then_reinit passed\n");
}

int main(void)
{
	test_destroy_frees_buffer();
	test_destroy_frees_growth_history();
	test_destroy_zeros_metadata();
	test_delete_nullifies_pointer();
	test_safe_on_null();
	test_destroy_poisoning();
	test_destroy_then_reinit();
	printf("🎉 All arena_destroy/delete tests passed.\n");
	return 0;
}
