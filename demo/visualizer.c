#include "arena.h"
#include "internal/arena_debug.h"
#include "arena_visualizer.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Helper: Wait for user interaction
int wait_for_key(t_arena_visualizer* vis);

// Helper: clearly defined steps
#define STEP(msg)               \
	do                          \
	{                           \
		step_message(vis, msg); \
		if (!wait_for_key(vis)) \
			return;             \
	} while (0)

// Test zero-size alloc (should trigger error)
void test_zero_alloc(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("🔍 Test: Zero-size allocation (expect error)");
	arena_alloc(arena, 0);
}

// Test normal allocations
void test_basic_allocations(t_arena* arena, t_arena_visualizer* vis)
{
	fprintf(stderr, "[DEBUG] about to alloc 128\n");
	STEP("🧪 Allocating 128 bytes");
	arena_alloc(arena, 128);
	fprintf(stderr, "[DEBUG] alloc 128 done\n");
}

// Test aligned allocations
void test_aligned_allocations(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("📐 Test: Aligned allocations (32, 64-byte alignments)");
	arena_alloc_aligned(arena, 200, 32);
	arena_alloc_aligned(arena, 300, 64);
}

// Test calloc
void test_calloc(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("🧹 Test: Calloc (10 * 100 bytes, zeroed)");
	void* mem = arena_calloc(arena, 10, 100);
	memset(mem, 1, 1000); // simulate usage
}

// Test realloc in-place
void test_realloc_inplace(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("♻️ Test: Realloc last block in-place");
	void* block = arena_alloc(arena, 200);
	block       = arena_realloc_last(arena, block, 200, 400);
}

// Test realloc fallback (not last block)
void test_realloc_fallback(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("🔄 Test: Realloc fallback (copy required)");
	void* block = arena_alloc(arena, 100);
	arena_alloc(arena, 50); // block no longer last
	block = arena_realloc_last(arena, block, 100, 300);
}

// Test Mark & Pop
void test_mark_pop(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("📌 Test: Mark and Pop functionality");
	t_arena_marker mark = arena_mark(arena);
	arena_alloc(arena, 100);
	arena_alloc(arena, 200);
	STEP("Marked + allocated (100, 200), popping...");
	arena_pop(arena, mark);
}

// Test sub-arena management
void test_subarena(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("🧩 Test: Sub-arena creation, use, reset");
	t_arena child;
	if (arena_alloc_sub_labeled(arena, &child, 2048, "ChildArena"))
	{
		arena_alloc(&child, 300);
		arena_alloc_aligned(&child, 256, 64);
		STEP("Sub-arena allocated (300, 256 aligned). Resetting...");
		arena_reset(&child);
	}
	else
		STEP("Sub-arena allocation failed");
}

// Test Shrink & Grow
void test_shrink_grow(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("📉 Test: Shrink arena size");
	size_t before = arena->size;
	arena_shrink(arena, arena_used(arena) + 64);
	STEP("📈 Test: Grow arena (+4KB)");
	arena_grow(arena, before + 4096);
}

// Final reset and cleanup
void test_final_cleanup(t_arena* arena, t_arena_visualizer* vis)
{
	STEP("🧽 Final cleanup: Reset entire arena");
	arena_reset(arena);
}

// The complete scenario:
void run_full_scenario(t_arena* arena, t_arena_visualizer* vis)
{
	test_zero_alloc(arena, vis);
	test_basic_allocations(arena, vis);
	test_aligned_allocations(arena, vis);
	test_calloc(arena, vis);
	test_realloc_inplace(arena, vis);
	test_realloc_fallback(arena, vis);
	test_mark_pop(arena, vis);
	test_subarena(arena, vis);
	test_shrink_grow(arena, vis);
	test_final_cleanup(arena, vis);
}

// Entry point
int main(void)
{
	t_arena* arena = arena_create(4096, true);
	if (!arena)
	{
		perror("Arena creation failed");
		return 1;
	}
	arena_set_debug_label(arena, "MainArena");

	t_arena_visualizer vis = {0};
	arena_visualizer_enable_history_hook(&vis, arena);
	arena_set_error_callback(arena, arena_visualizer_error_callback, &vis);
	arena_start_interactive_visualizer(&vis, arena);

	run_full_scenario(arena, &vis);

	arena_delete(&arena);
	return 0;
}

// Blocking function for keys (implemented in visualizer.c)
int wait_for_key(t_arena_visualizer* vis);
