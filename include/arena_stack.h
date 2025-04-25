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
	 * Initialize an arena stack with a given arena.
	 *
	 * @param stack Pointer to the arena stack to initialize.
	 * @param arena Pointer to the arena the stack will manage.
	 *
	 * @ingroup arena_state
	 *
	 * @note
	 * Does not allocate any memory until `arena_stack_push()` is called.
	 *
	 * @see arena_stack_push
	 * @see arena_stack_clear
	 */
	void arena_stack_init(t_arena_stack* stack, t_arena* arena);

	/**
	 * @brief
	 * Push the current arena state onto the stack.
	 *
	 * @details
	 * Saves the current allocation offset into a new stack frame.
	 * All allocations made after the push can later be discarded with `arena_stack_pop()`.
	 *
	 * @param stack Pointer to the arena stack to push onto.
	 *
	 * @ingroup arena_state
	 *
	 * @note
	 * The stack frame is allocated from the same arena being managed.
	 *
	 * @warning
	 * If the arena is out of memory, the push will silently fail.
	 *
	 * @see arena_stack_pop
	 */
	void arena_stack_push(t_arena_stack* stack);

	/**
	 * @brief
	 * Pop and restore the last saved state from the stack.
	 *
	 * @details
	 * Restores the arena to the previous state (marker) and discards all allocations made since
	 * the last `arena_stack_push()` call.
	 *
	 * @param stack Pointer to the arena stack to pop from.
	 *
	 * @ingroup arena_state
	 *
	 * @note
	 * Does nothing if the stack is empty or uninitialized.
	 *
	 * @see arena_stack_push
	 */
	void arena_stack_pop(t_arena_stack* stack);

	/**
	 * @brief
	 * Clear all stack frames without modifying arena state.
	 *
	 * @param stack Pointer to the arena stack to clear.
	 *
	 * @ingroup arena_state
	 *
	 * @note
	 * This does not rewind or pop memory. It only resets the internal frame pointer.
	 */
	void arena_stack_clear(t_arena_stack* stack);

#ifdef __cplusplus
}
#endif

#endif // ARENA_STACK_H
