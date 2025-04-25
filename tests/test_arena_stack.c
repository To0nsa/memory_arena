#include "arena.h"
#include "arena_stack.h"
#include <assert.h>
#include <stdio.h>

static void test_stack_init_basic(void)
{
	t_arena arena;
	arena_init(&arena, 1024, false);

	t_arena_stack stack;
	arena_stack_init(&stack, &arena);

	assert(stack.arena == &arena);
	assert(stack.top == NULL);

	arena_destroy(&arena);
	printf("✅ test_stack_init_basic passed\n");
}

static void test_stack_push_pop_basic(void)
{
	t_arena arena;
	arena_init(&arena, 1024, false);

	t_arena_stack stack;
	arena_stack_init(&stack, &arena);

	arena_stack_push(&stack);
	size_t after_push = arena.offset;

	char* tmp = arena_alloc(&arena, 100);
	assert(tmp != NULL);
	assert(arena.offset > after_push);

	arena_stack_pop(&stack);
	assert(arena.offset == after_push);
	assert(stack.top == NULL);

	arena_destroy(&arena);
	printf("✅ test_stack_push_pop_basic passed\n");
}

static void test_stack_multiple_push_pop(void)
{
	t_arena arena;
	arena_init(&arena, 2048, false);

	t_arena_stack stack;
	arena_stack_init(&stack, &arena);

	arena_stack_push(&stack);
	char* a = arena_alloc(&arena, 100);
	assert(a != NULL);

	arena_stack_push(&stack);
	char* b = arena_alloc(&arena, 200);
	assert(b != NULL);

	arena_stack_pop(&stack);
	assert(stack.top != NULL);

	arena_stack_pop(&stack);
	assert(stack.top == NULL);

	arena_destroy(&arena);
	printf("✅ test_stack_multiple_push_pop passed\n");
}

static void test_stack_clear(void)
{
	t_arena arena;
	arena_init(&arena, 512, false);

	t_arena_stack stack;
	arena_stack_init(&stack, &arena);

	arena_stack_push(&stack);
	arena_stack_push(&stack);
	assert(stack.top != NULL);

	arena_stack_clear(&stack);
	assert(stack.top == NULL);

	arena_stack_pop(&stack);
	assert(stack.top == NULL);

	arena_destroy(&arena);
	printf("✅ test_stack_clear passed\n");
}

static void test_stack_edge_cases(void)
{
	arena_stack_init(NULL, NULL);
	arena_stack_push(NULL);
	arena_stack_pop(NULL);
	arena_stack_clear(NULL);

	t_arena_stack stack = {0};
	arena_stack_push(&stack);
	arena_stack_pop(&stack);

	t_arena arena;
	arena_init(&arena, 128, false);
	arena_stack_init(&stack, &arena);

	arena.size = 0;
	arena_stack_push(&stack);
	assert(stack.top == NULL);

	arena_destroy(&arena);
	printf("✅ test_stack_edge_cases passed\n");
}

int main(void)
{
	test_stack_init_basic();
	test_stack_push_pop_basic();
	test_stack_multiple_push_pop();
	test_stack_clear();
	test_stack_edge_cases();
	printf("🎉 All arena_stack tests passed.\n");
	return 0;
}
