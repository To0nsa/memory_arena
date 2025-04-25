
#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define TEST_BUFFER_SIZE 2048

static int hook_called = 0;

void test_hook(t_arena* arena, int id, void* ptr, size_t size, size_t offset, size_t wasted, const char* label)
{
	(void) arena;
	(void) id;
	(void) ptr;
	(void) size;
	(void) offset;
	(void) wasted;
	assert(label != NULL);
	hook_called++;
}

void test_normal_allocations(void)
{
	t_arena* arena = arena_create(TEST_BUFFER_SIZE, false);
	assert(arena);

	void* ptr1 = arena_alloc(arena, 64);
	assert(ptr1);
	assert(arena_used(arena) >= 64);

	void* ptr2 = arena_alloc_aligned(arena, 32, 16);
	assert(ptr2 && ((uintptr_t) ptr2 % 16 == 0));

	void* ptr3 = arena_alloc_labeled(arena, 128, "my_label");
	assert(ptr3);

	void* ptr4 = arena_alloc_aligned_labeled(arena, 64, 64, "aligned_labeled");
	assert(ptr4 && ((uintptr_t) ptr4 % 64 == 0));

	arena_delete(&arena);
	printf("✅ test_normal_allocations passed\n");
}

void test_edge_cases(void)
{
	t_arena* arena = arena_create(TEST_BUFFER_SIZE, false);
	assert(arena);

	assert(arena_alloc(arena, 0) == NULL);
	assert(arena_alloc_aligned(arena, 32, 0) == NULL);
	assert(arena_alloc_aligned(arena, 32, 3) == NULL);
	assert(arena_alloc(NULL, 32) == NULL);

	size_t big_size = (size_t) -32;
	assert(arena_alloc(arena, big_size) == NULL);

	arena_delete(&arena);
	printf("✅ test_edge_cases passed\n");
}

void test_stats_tracking(void)
{
	t_arena* arena = arena_create(TEST_BUFFER_SIZE, false);
	assert(arena);

	size_t start_used = arena_used(arena);
	void*  ptr        = arena_alloc_aligned(arena, 64, 64);
	assert(ptr);
	assert(((uintptr_t) ptr % 64) == 0);
	assert(arena->stats.allocations == 1);
	assert(arena->stats.live_allocations == 1);
	assert(arena->stats.bytes_allocated >= 64);
	assert(arena->stats.last_alloc_size == 64);
	assert(arena_used(arena) >= start_used + 64);

	arena_delete(&arena);
	printf("✅ test_stats_tracking passed\n");
}

void test_hook_invocation(void)
{
	t_arena* arena = arena_create(TEST_BUFFER_SIZE, false);
	assert(arena);
	arena->hooks.hook_cb = test_hook;

	hook_called = 0;
	void* ptr   = arena_alloc_labeled(arena, 32, "hooked_alloc");
	assert(ptr);
	assert(hook_called == 1);

	arena_delete(&arena);
	printf("✅ test_hook_invocation passed\n");
}

int main(void)
{
	test_normal_allocations();
	test_edge_cases();
	test_stats_tracking();
	test_hook_invocation();
	printf("🎉 All arena_alloc* tests passed.\n");
	return 0;
}
