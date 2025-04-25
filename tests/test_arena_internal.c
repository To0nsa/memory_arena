
#include "arena.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void test_default_grow_cb_basic(void)
{
	assert(default_grow_cb(0, 32) >= 64);
	assert(default_grow_cb(64, 64) == 128);
	printf("✅ test_default_grow_cb_basic passed\n");
}

static void test_default_grow_cb_large_and_overflow(void)
{
	size_t half_max = SIZE_MAX / 2;
	size_t grow     = default_grow_cb(half_max, 4096);
	assert(grow >= half_max + 4096);
	assert(default_grow_cb(SIZE_MAX - 32, 64) == SIZE_MAX);
	printf("✅ test_default_grow_cb_large_and_overflow passed\n");
}

static void test_arena_update_peak(void)
{
	t_arena arena = {0};
	arena.offset  = 64;
	arena_update_peak(&arena);
	assert(arena.stats.peak_usage == 64);

	arena.offset = 32;
	arena_update_peak(&arena);
	assert(arena.stats.peak_usage == 64);
	printf("✅ test_arena_update_peak passed\n");
}

static void test_arena_is_valid(void)
{
	t_arena arena = {0};
	assert(arena_is_valid(NULL) == false);
	assert(arena_is_valid(&arena) == false);

	uint8_t buffer[128];
	arena.buffer = buffer;
	arena.size   = 128;
	arena.offset = 64;
	assert(arena_is_valid(&arena) == true);

	arena.offset = 256;
	assert(arena_is_valid(&arena) == false);
	printf("✅ test_arena_is_valid passed\n");
}

static void test_arena_zero_metadata(void)
{
	t_arena arena             = {0};
	arena.buffer              = (uint8_t*) 0x1;
	arena.size                = 128;
	arena.offset              = 64;
	arena.stats.peak_usage    = 64;
	arena.debug.label         = "test";
	arena.stats.last_alloc_id = 42;

	arena_zero_metadata(&arena);
	assert(arena.buffer == NULL);
	assert(arena.size == 0);
	assert(arena.offset == 0);
	assert(arena.stats.peak_usage == 0);
	assert(arena.stats.last_alloc_id == SIZE_MAX);
	assert(arena.debug.label == NULL);
	assert(arena_is_valid(&arena) == false);
	printf("✅ test_arena_zero_metadata passed\n");
}

int main(void)
{
	test_default_grow_cb_basic();
	test_default_grow_cb_large_and_overflow();
	test_arena_update_peak();
	test_arena_is_valid();
	test_arena_zero_metadata();
	printf("🎉 All utility tests passed.\n");
	return 0;
}
