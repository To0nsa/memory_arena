#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void cleanup_growth_history(t_arena_stats* stats)
{
	if (stats && stats->growth_history)
	{
		free(stats->growth_history);
		stats->growth_history       = NULL;
		stats->growth_history_count = 0;
	}
}

static void test_arena_stats_reset(void)
{
	t_arena_stats stats = {.allocations            = 5,
	                       .reallocations          = 3,
	                       .failed_allocations     = 2,
	                       .live_allocations       = 1,
	                       .bytes_allocated        = 1024,
	                       .wasted_alignment_bytes = 16,
	                       .shrinks                = 1,
	                       .peak_usage             = 900,
	                       .last_alloc_size        = 64,
	                       .last_alloc_offset      = 128,
	                       .last_alloc_id          = 1234,
	                       .alloc_id_counter       = 987,
	                       .growth_history_count   = 2};

	stats.growth_history = malloc(2 * sizeof(size_t));
	assert(stats.growth_history);

	free(stats.growth_history);
	arena_stats_reset(&stats);

	assert(stats.allocations == 0);
	assert(stats.reallocations == 0);
	assert(stats.failed_allocations == 0);
	assert(stats.live_allocations == 0);
	assert(stats.bytes_allocated == 0);
	assert(stats.wasted_alignment_bytes == 0);
	assert(stats.shrinks == 0);
	assert(stats.peak_usage == 0);
	assert(stats.last_alloc_size == 0);
	assert(stats.last_alloc_offset == 0);
	assert(stats.last_alloc_id == (size_t) -1);
	assert(stats.alloc_id_counter == 0);
	assert(stats.growth_history == NULL);
	assert(stats.growth_history_count == 0);

	printf("✅ test_arena_stats_reset passed\n");
}

static void test_arena_stats_record_growth(void)
{
	t_arena_stats stats = {0};

	arena_stats_record_growth(&stats, 128);
	assert(stats.growth_history_count == 1);
	assert(stats.growth_history[0] == 128);

	arena_stats_record_growth(&stats, 256);
	assert(stats.growth_history_count == 2);
	assert(stats.growth_history[1] == 256);

	cleanup_growth_history(&stats);
	printf("✅ test_arena_stats_record_growth passed\n");
}

static void test_arena_stats_record_growth_null_safe(void)
{
	arena_stats_record_growth(NULL, 128);
	printf("✅ test_arena_stats_record_growth_null_safe passed\n");
}

static void test_arena_get_stats_copy(void)
{
	t_arena* arena = arena_create(512, false);
	assert(arena);

	void* a = arena_alloc(arena, 64);
	assert(a);

	t_arena_stats stats = arena_get_stats(arena);
	assert(stats.allocations == 1);
	assert(stats.last_alloc_size == 64);
	assert(stats.last_alloc_offset == 0);
	assert(stats.bytes_allocated >= 64);

	stats.allocations = 9999;
	assert(arena->stats.allocations != 9999);

	arena_delete(&arena);
	printf("✅ test_arena_get_stats_copy passed\n");
}

static void test_arena_get_stats_null_safe(void)
{
	t_arena_stats stats = arena_get_stats(NULL);
	assert(memcmp(&stats, &(t_arena_stats){0}, sizeof(stats)) == 0);
	printf("✅ test_arena_get_stats_null_safe passed\n");
}

static void test_arena_print_stats_output(void)
{
	t_arena* arena = arena_create(256, true);
	assert(arena);
	arena_set_debug_label(arena, "print_test");

	arena_stats_record_growth(&arena->stats, 128);
	arena_stats_record_growth(&arena->stats, 256);

	FILE* fp = tmpfile();
	assert(fp);

	arena_print_stats(arena, fp);

	rewind(fp);
	char   buffer[1024] = {0};
	size_t _            = fread(buffer, 1, sizeof(buffer) - 1, fp);
	(void) _;
	assert(strstr(buffer, "Arena Diagnostics"));
	assert(strstr(buffer, "print_test"));
	assert(strstr(buffer, "Growth History"));
	assert(strstr(buffer, "128"));
	assert(strstr(buffer, "256"));

	fclose(fp);
	arena_delete(&arena);
	printf("✅ test_arena_print_stats_output passed\n");
}

static void test_arena_print_stats_null_safe(void)
{
	arena_print_stats(NULL, stdout);
	arena_print_stats(NULL, NULL);
	arena_print_stats((t_arena*) 0x1, NULL);
	printf("✅ test_arena_print_stats_null_safe passed\n");
}

int main(void)
{
	test_arena_stats_reset();
	test_arena_stats_record_growth();
	test_arena_stats_record_growth_null_safe();
	test_arena_get_stats_copy();
	test_arena_get_stats_null_safe();
	test_arena_print_stats_output();
	test_arena_print_stats_null_safe();
	printf("🎉 All arena stats tests passed.\n");
	return 0;
}
