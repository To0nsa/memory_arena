
#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_realloc_in_place_expand(void)
{
	t_arena* arena = arena_create(1024, true);
	assert(arena);

	void* ptr = arena_alloc(arena, 128);
	assert(ptr);
	memset(ptr, 0xAB, 128);

	void* new_ptr = arena_realloc_last(arena, ptr, 128, 256);
	assert(new_ptr == ptr);
	assert(arena->stats.reallocations == 1);
	assert(arena->stats.last_alloc_size == 256);
	assert(arena->offset >= 256);

	arena_delete(&arena);
	printf("✅ test_realloc_in_place_expand passed\n");
}

static void test_realloc_in_place_shrink(void)
{
	t_arena* arena = arena_create(1024, true);
	assert(arena);

	void* ptr = arena_alloc(arena, 256);
	assert(ptr);
	memset(ptr, 0xCD, 256);

	void* new_ptr = arena_realloc_last(arena, ptr, 256, 128);
	assert(new_ptr == ptr);
	assert(arena->stats.reallocations == 1);
	assert(arena->stats.last_alloc_size == 128);
	assert(arena->offset >= 128);

	uint8_t* poison = (uint8_t*) ptr + 128;
	printf("ℹ️  Shrunk region may still contain old data: %02X %02X %02X\n",
		poison[0], poison[1], poison[2]);

	arena_delete(&arena);
	printf("✅ test_realloc_in_place_shrink passed\n");
}

static void test_realloc_same_size(void)
{
	t_arena* arena = arena_create(1024, true);
	assert(arena);

	void* ptr = arena_alloc(arena, 128);
	assert(ptr);

	void* new_ptr = arena_realloc_last(arena, ptr, 128, 128);
	assert(new_ptr == ptr);
	assert(arena->stats.reallocations == 1);

	arena_delete(&arena);
	printf("✅ test_realloc_same_size passed\n");
}

void test_realloc_fallback_copy(void)
{
	t_arena* arena = arena_create(512, false);
	assert(arena);

	void* a = arena_alloc(arena, 64);
	assert(a);
	memset(a, 0xAB, 64);

	void* b = arena_alloc(arena, 64);
	assert(b);

	void* result = arena_realloc_last(arena, a, 64, 128);
	assert(result != NULL);
	assert(memcmp(result, "\xAB\xAB\xAB\xAB\xAB\xAB\xAB\xAB", 8) == 0);
	assert(arena_used(arena) >= 64 + 64 + 128);

	arena_delete(&arena);
	printf("✅ test_realloc_fallback_copy passed\n");
}

static void test_invalid_inputs(void)
{
	t_arena* arena = arena_create(512, false);
	uint8_t  dummy[64];

	assert(arena_realloc_last(NULL, dummy, 64, 128) == NULL);
	assert(arena_realloc_last(arena, NULL, 64, 128) == NULL);
	assert(arena_realloc_last(arena, dummy, 64, 0) == NULL);

	arena_delete(&arena);
	printf("✅ test_invalid_inputs passed\n");
}

int main(void)
{
	test_realloc_in_place_expand();
	test_realloc_in_place_shrink();
	test_realloc_same_size();
	test_realloc_fallback_copy();
	test_invalid_inputs();
	printf("🎉 All arena_realloc_last tests passed.\n");
	return 0;
}
