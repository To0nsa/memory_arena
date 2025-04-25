#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_normal_usage(void)
{
	t_arena* parent = arena_create(1024, true);
	assert(parent != NULL);

	t_arena child;
	assert(arena_alloc_sub(parent, &child, 256));
	assert(child.buffer != NULL);
	assert(child.size == 256);

	assert(atomic_load_explicit(&child.owns_buffer, memory_order_acquire) == false);

	assert(child.parent_ref == parent);
	assert(strncmp(child.debug.label, "subarena", 8) == 0);
	assert(strncmp(child.debug.id, parent->debug.id, 4) == 0);

	arena_destroy(&child);
	arena_delete(&parent);
	printf("✅ test_normal_usage passed\n");
}

void test_edge_cases(void)
{
	t_arena child;
	assert(!arena_alloc_sub(NULL, &child, 128));
	assert(!arena_alloc_sub_labeled(NULL, &child, 128, "fail"));
	assert(!arena_alloc_sub((t_arena*) 1, NULL, 128));
	assert(!arena_alloc_sub_labeled((t_arena*) 1, NULL, 128, "fail"));
	printf("✅ test_edge_cases passed\n");
}

void test_zero_size_allocation(void)
{
	t_arena* parent = arena_create(1024, true);
	assert(parent != NULL);

	t_arena child;
	assert(!arena_alloc_sub(parent, &child, 0));
	assert(!arena_alloc_sub_labeled(parent, &child, 0, "zero"));

	arena_delete(&parent);
	printf("✅ test_zero_size_allocation passed\n");
}

void test_labeled_subarena(void)
{
	t_arena* parent = arena_create(1024, true);
	assert(parent != NULL);

	t_arena child1;
	assert(arena_alloc_sub_labeled(parent, &child1, 128, "custom_label"));
	assert(strcmp(child1.debug.label, "custom_label") == 0);
	arena_destroy(&child1);

	t_arena child2;
	assert(arena_alloc_sub_labeled(parent, &child2, 128, NULL));
	assert(strcmp(child2.debug.label, "subarena") == 0);
	arena_destroy(&child2);

	arena_delete(&parent);
	printf("✅ test_labeled_subarena passed\n");
}

int main(void)
{
	test_normal_usage();
	test_edge_cases();
	test_zero_size_allocation();
	test_labeled_subarena();
	printf("🎉 All arena_alloc_sub tests passed.\n");
	return 0;
}
