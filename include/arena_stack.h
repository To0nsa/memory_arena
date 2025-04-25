/**
 * @file arena_stack.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Stack-based scope management for memory arenas.
 *
 * @details
 * This module defines a lightweight stack structure to manage allocation
 * scopes within a memory arena. It enables nested usage patterns similar
 * to function call stacks, where each `push` operation records a restore point,
 * and each `pop` rewinds the arena back to that state using `arena_pop()`.
 *
 * Features:
 * - Fast scoped memory management using `arena_mark()` and `arena_pop()`
 * - Dynamic stack frames stored inside the arena itself
 * - No extra heap allocations or dependencies
 *
 * Typical use case:
 * - Push the current state before a complex operation
 * - Allocate temporary data
 * - Pop to discard all of it in one go
 *
 * @note
 * Stack frames are themselves allocated from the arena. Be cautious when
 * pushing deeply in arenas with tight space.
 *
 * @ingroup arena_state
 */

#ifndef ARENA_STACK_H
#define ARENA_STACK_H

#include "arena.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief
	 * Internal frame representing a single state snapshot in the stack.
	 *
	 * @details
	 * Each frame holds a marker (offset) representing the arena state at the
	 * time of `push`, and a pointer to the previous frame for LIFO traversal.
	 *
	 * @ingroup arena_state
	 */
	typedef struct s_arena_stack_frame
	{
		size_t                      marker; ///< Marker to rewind arena to this point
		struct s_arena_stack_frame* prev;   ///< Previous frame in the stack
	} t_arena_stack_frame;

	/**
	 * @brief
	 * Stack structure for scoped arena memory control.
	 *
	 * @details
	 * Holds a pointer to an arena and a linked list of frames representing
	 * saved arena states. Frames are allocated from the arena itself.
	 *
	 * @ingroup arena_state
	 */
	typedef struct s_arena_stack
	{
		t_arena*             arena; ///< Arena associated with this stack
		t_arena_stack_frame* top;   ///< Top of the stack (most recent frame)
	} t_arena_stack;

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
	void arena_stack_init(t_arena_stack* stack, t_arena* arena);

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
	void arena_stack_push(t_arena_stack* stack);

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
	void arena_stack_pop(t_arena_stack* stack);

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
	void arena_stack_clear(t_arena_stack* stack);

#ifdef __cplusplus
}
#endif

#endif // ARENA_STACK_H
