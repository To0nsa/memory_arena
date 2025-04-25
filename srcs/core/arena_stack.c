/**
 * @file arena_stack.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Stack-based scoped memory management for arenas.
 *
 * @details
 * This module implements a lightweight stack system on top of arena-based memory.
 * It allows users to push and pop memory states using marker frames allocated
 * within the arena itself. This pattern enables efficient and structured
 * management of temporary memory regions.
 *
 * Core functionality includes:
 * - Tracking memory states using `arena_mark()` saved as stack frames.
 * - Reverting to a previous memory state via `arena_pop()`.
 * - Grouping allocations into push/pop memory scopes (similar to RAII or region-based memory).
 * - Optional stack clearing without altering the arena state.
 *
 * All stack frames are stored inside the arena, meaning no additional heap
 * allocations or deallocations are required.
 *
 * Typical use cases:
 * - Nested parsing stages or recursive algorithms.
 * - Temporary memory for intermediate steps in complex logic.
 * - Systems requiring fast and deterministic memory rollback.
 *
 * @note
 * Stack operations (`push`, `pop`, `clear`) are local to the arena and safe to use
 * as long as the arena is not destroyed or reset externally.
 *
 * @ingroup arena_state
 *
 * @example arena_stack_parser.c
 * @brief
 * Example: Nested memory scopes using arena stack frames.
 *
 * @details
 * This example demonstrates how to manage temporary memory in nested scopes
 * using `t_arena_stack`. Each "code block" pushes a new frame and allocates
 * temporary memory for simulated tokens. When the block ends, the memory is
 * discarded by popping the frame.
 *
 * This pattern is useful in:
 * - Recursive parsers or interpreters.
 * - Tree traversal or scene graphs.
 * - Temporary memory for UI elements or entity components.
 *
 * @code
 * #include "arena_stack.h"
 * #include <stdio.h>
 * #include <string.h>
 *
 * void parse_block(t_arena_stack* stack, const char* label, int depth)
 * {
 *     printf("-> Entering block: %s\n", label);
 *     arena_stack_push(stack);
 *
 *     // Simulate allocation of temporary tokens
 *     char* token1 = arena_alloc(stack->arena, 64);
 *     char* token2 = arena_alloc(stack->arena, 64);
 *     if (token1 && token2)
 *     {
 *         snprintf(token1, 64, "[%s] token1", label);
 *         snprintf(token2, 64, "[%s] token2", label);
 *         printf("   Allocated: %s | %s\n", token1, token2);
 *     }
 *
 *     // Recursively parse child blocks
 *     if (depth < 2)
 *     {
 *         char child_label[32];
 *         snprintf(child_label, sizeof(child_label), "%s.child", label);
 *         parse_block(stack, child_label, depth + 1);
 *     }
 *
 *     printf("<- Exiting block: %s\n", label);
 *     arena_stack_pop(stack);
 * }
 *
 * int main(void)
 * {
 *     t_arena* arena = arena_create(2048, true);
 *     if (!arena)
 *     {
 *         fprintf(stderr, "Failed to create arena.\n");
 *         return 1;
 *     }
 *
 *     t_arena_stack stack;
 *     arena_stack_init(&stack, arena);
 *
 *     parse_block(&stack, "root", 0);
 *
 *     arena_stack_clear(&stack);  // Optional: logical reset
 *     arena_delete(&arena);       // Free memory
 *     return 0;
 * }
 * @endcode
 */

#include "arena_stack.h"
#include <stdlib.h>

void arena_stack_init(t_arena_stack* stack, t_arena* arena)
{
	if (!stack || !arena)
		return;
	stack->arena = arena;
	stack->top   = NULL;
}

void arena_stack_push(t_arena_stack* stack)
{
	if (!stack || !stack->arena)
		return;
	t_arena_stack_frame* frame = (t_arena_stack_frame*) arena_alloc(stack->arena, sizeof(t_arena_stack_frame));
	if (!frame)
		return;
	frame->marker = arena_mark(stack->arena);
	frame->prev   = stack->top;
	stack->top    = frame;
}

void arena_stack_pop(t_arena_stack* stack)
{
	if (!stack || !stack->arena || !stack->top)
		return;
	arena_pop(stack->arena, stack->top->marker);
	stack->top = stack->top->prev;
}

void arena_stack_clear(t_arena_stack* stack)
{
	if (!stack)
		return;
	stack->top = NULL;
}
