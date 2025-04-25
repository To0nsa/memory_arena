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

/**
 * @brief
 * Initialize a stack-based frame system for scoped arena memory management.
 *
 * @details
 * This function sets up a `t_arena_stack` structure and associates it with a given arena.
 * The stack starts empty (`top == NULL`) and can be used to push/pop memory frames
 * using `arena_stack_push()` and `arena_stack_pop()` respectively.
 *
 * This is useful for:
 * - Managing nested scopes of temporary memory.
 * - Restoring previous arena states in reverse order (like a call stack).
 * - Structuring complex memory lifetimes in layered or recursive systems.
 *
 * @param stack Pointer to the arena stack structure to initialize.
 * @param arena Pointer to the arena that this stack will operate on.
 *
 * @ingroup arena_state
 *
 * @note
 * No memory is allocated by this function. Frames are allocated on the arena
 * itself when calling `arena_stack_push()`.
 *
 * @see arena_stack_push
 * @see arena_stack_pop
 * @see arena_stack_clear
 *
 * @example
 * @code
 * t_arena_stack stack;
 * t_arena* arena = arena_create(1024, true);
 * arena_stack_init(&stack, arena);
 * arena_stack_push(&stack);
 * // allocate temporary memory...
 * arena_stack_pop(&stack); // clean up
 * arena_delete(&arena);
 * @endcode
 */
void arena_stack_init(t_arena_stack* stack, t_arena* arena)
{
	if (!stack || !arena)
		return;
	stack->arena = arena;
	stack->top   = NULL;
}

/**
 * @brief
 * Push the current state of the arena onto the stack.
 *
 * @details
 * This function captures the current arena offset using `arena_mark()` and
 * pushes it onto the arena stack as a new frame. This allows you to later
 * restore the arena to this state using `arena_stack_pop()`.
 *
 * Internally:
 * - A new frame is allocated from the arena.
 * - It stores the current marker and links to the previous top frame.
 * - The frame becomes the new stack top.
 *
 * This mechanism enables scoped memory management, where multiple allocations
 * can be grouped and later discarded in one pop operation.
 *
 * @param stack Pointer to the initialized arena stack.
 *
 * @ingroup arena_state
 *
 * @note
 * This function allocates the stack frame from the same arena it manages.
 *
 * @warning
 * If the allocation for the frame fails, the stack is not updated.
 * Always ensure sufficient memory in the arena before pushing.
 *
 * @see arena_stack_init
 * @see arena_stack_pop
 * @see arena_mark
 *
 * @example
 * @code
 * t_arena_stack stack;
 * arena_stack_init(&stack, arena);
 *
 * arena_stack_push(&stack);
 * char* temp1 = arena_alloc(stack.arena, 128);
 * char* temp2 = arena_alloc(stack.arena, 256);
 * // Use temp1 and temp2...
 *
 * arena_stack_pop(&stack); // Frees temp1 and temp2
 * @endcode
 */
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

/**
 * @brief
 * Pop and restore the last saved arena state from the stack.
 *
 * @details
 * This function rolls back the arena to the last marker pushed via
 * `arena_stack_push()`. It uses `arena_pop()` to discard all allocations
 * made after that point. The top frame is then removed from the stack.
 *
 * Use this to efficiently revert a group of temporary allocations in
 * reverse order (LIFO), similar to popping a call stack.
 *
 * This is useful in:
 * - Temporary memory scopes
 * - Rewindable allocators
 * - Error handling where partial allocations must be discarded
 *
 * @param stack Pointer to the arena stack to operate on.
 *
 * @return void
 *
 * @ingroup arena_state
 *
 * @note
 * This function does nothing if the stack is empty or uninitialized.
 *
 * @warning
 * Only pop after a successful `arena_stack_push()`. Misuse may leave
 * the stack in an inconsistent state.
 *
 * @see arena_stack_push
 * @see arena_pop
 * @see arena_mark
 *
 * @example
 * @code
 * t_arena_stack stack;
 * arena_stack_init(&stack, arena);
 *
 * arena_stack_push(&stack);
 * char* block = arena_alloc(arena, 512);
 * // Use block...
 * arena_stack_pop(&stack); // Frees block
 * @endcode
 */
void arena_stack_pop(t_arena_stack* stack)
{
	if (!stack || !stack->arena || !stack->top)
		return;
	arena_pop(stack->arena, stack->top->marker);
	stack->top = stack->top->prev;
}

/**
 * @brief
 * Clear all frames in the arena stack without altering arena memory.
 *
 * @details
 * This function discards all frames from the arena stack by resetting
 * the `top` pointer to `NULL`. It does **not** modify the arena’s
 * allocation state or release memory. It's a logical reset of the stack
 * structure, not a memory rollback.
 *
 * Use this when:
 * - You want to abandon saved states without restoring them.
 * - You're about to destroy the arena and want to clean up the stack pointer.
 * - You're done with a scoped stack logic and want to reset for reuse.
 *
 * @param stack Pointer to the arena stack to clear.
 *
 * @return void
 *
 * @ingroup arena_state
 *
 * @note
 * This operation does not reclaim memory or modify arena contents.
 * Use `arena_stack_pop()` to actually revert memory state.
 *
 * @warning
 * Clearing the stack invalidates all saved markers in `arena_stack_push()`.
 * Use with care if frames are still expected to be restored.
 *
 * @see arena_stack_init
 * @see arena_stack_push
 * @see arena_stack_pop
 */
void arena_stack_clear(t_arena_stack* stack)
{
	if (!stack)
		return;
	stack->top = NULL;
}
