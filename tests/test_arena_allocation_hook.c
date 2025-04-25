#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int         g_hook_called  = 0;
static size_t      g_hook_size    = 0;
static void*       g_hook_ptr     = NULL;
static size_t      g_hook_offset  = 0;
static size_t      g_hook_wasted  = 0;
static const char* g_hook_label   = NULL;
static t_arena*    g_hook_arena   = NULL;
static int         g_hook_slot    = -1;
static void*       g_hook_context = NULL;

static void test_hook(t_arena* arena, int slot, void* ptr, size_t size, size_t offset, size_t wasted, const char* label)
{
	g_hook_called  = 1;
	g_hook_arena   = arena;
	g_hook_slot    = slot;
	g_hook_ptr     = ptr;
	g_hook_size    = size;
	g_hook_offset  = offset;
	g_hook_wasted  = wasted;
	g_hook_label   = label;
	g_hook_context = arena ? arena->hooks.context : NULL;
}

static void reset_hook_state(void)
{
	g_hook_called  = 0;
	g_hook_size    = 0;
	g_hook_ptr     = NULL;
	g_hook_offset  = 0;
	g_hook_wasted  = 0;
	g_hook_label   = NULL;
	g_hook_arena   = NULL;
	g_hook_slot    = -1;
	g_hook_context = NULL;
}

static void test_allocation_hook_basic(void)
{
	t_arena* arena = arena_create(512, false);
	assert(arena);

	reset_hook_state();
	const char* my_label   = "hook_test";
	int         my_context = 42;

	arena_set_allocation_hook(arena, test_hook, &my_context);
	assert(arena->hooks.hook_cb == test_hook);
	assert(arena->hooks.context == &my_context);

	void* mem = arena_alloc_labeled(arena, 64, my_label);
	assert(mem);
	assert(g_hook_called);
	assert(g_hook_ptr == mem);
	assert(g_hook_size == 64);
	assert(g_hook_label != NULL && strcmp(g_hook_label, my_label) == 0);
	assert(g_hook_context == &my_context);
	assert(g_hook_arena == arena);

	arena_delete(&arena);
	printf("✅ test_allocation_hook_basic passed\n");
}

static void test_allocation_hook_null_arena(void)
{
	arena_set_allocation_hook(NULL, test_hook, NULL);
	printf("✅ test_allocation_hook_null_arena passed (check stderr manually)\n");
}

static void test_allocation_hook_disable(void)
{
	t_arena* arena = arena_create(256, false);
	assert(arena);

	arena_set_allocation_hook(arena, test_hook, NULL);
	void* mem = arena_alloc(arena, 32);
	assert(mem);
	assert(g_hook_called);

	reset_hook_state();
	arena_set_allocation_hook(arena, NULL, NULL);

	mem = arena_alloc(arena, 32);
	assert(mem);
	assert(!g_hook_called);

	arena_delete(&arena);
	printf("✅ test_allocation_hook_disable passed\n");
}

int main(void)
{
	test_allocation_hook_basic();
	test_allocation_hook_null_arena();
	test_allocation_hook_disable();
	printf("🎉 All allocation hook tests passed.\n");
	return 0;
}
