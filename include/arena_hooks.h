/**
 * @file arena_hooks.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Allocation hook system for arena-based memory tracking and introspection.
 *
 * @details
 * This module allows users to attach custom callback functions (hooks) to
 * arena allocators. These hooks are triggered after each successful allocation,
 * providing detailed metadata for logging, profiling, or visualization tools.
 *
 * Features:
 * - Install user-defined hooks per arena instance.
 * - Receive detailed information per allocation (pointer, size, offset, label).
 * - Optional user context pointer.
 * - Thread-safe access to hook callback (atomic pointer).
 *
 * Typical use cases:
 * - Logging all memory allocations for debugging.
 * - Instrumenting arena usage in profilers or visual debuggers.
 * - Tracking memory tags and usage over time.
 *
 * @note
 * The hook mechanism is lightweight and only triggers after successful allocations.
 * It must not attempt to allocate memory from the same arena.
 *
 * @ingroup arena_alloc
 */

#ifndef ARENA_HOOKS_H
#define ARENA_HOOKS_H

#include <stdatomic.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief Forward declaration of arena structure.
	 */
	typedef struct s_arena t_arena;

	/**
	 * @brief Allocation hook callback type.
	 *
	 * @param arena   The arena from which memory was allocated.
	 * @param alloc_id A unique ID for this allocation (monotonic counter).
	 * @param ptr     Pointer to the newly allocated memory.
	 * @param size    Size of the allocation in bytes.
	 * @param offset  Offset from the start of the arena buffer.
	 * @param wasted  Number of bytes wasted for alignment.
	 * @param label   Optional label associated with the allocation.
	 */
	typedef void (*arena_allocation_hook)(t_arena* arena, int alloc_id, void* ptr, size_t size, size_t offset,
	                                      size_t wasted, const char* label);

	/**
	 * @brief Internal structure for managing arena hooks.
	 *
	 * This structure stores the current allocation hook and an optional
	 * user-defined context that can be used inside the hook.
	 */
	typedef struct s_arena_hooks
	{
		_Atomic(arena_allocation_hook) hook_cb; ///< The active allocation hook callback.
		void*                          context; ///< Optional user data passed to the hook.
	} t_arena_hooks;

	/**
	 * @brief
	 * Set a custom allocation hook for the given arena.
	 *
	 * @details
	 * This function installs a user-defined allocation hook on the specified arena.
	 * The hook is a callback function that will be invoked after each successful
	 * memory allocation (`arena_alloc`, `arena_calloc`, `arena_realloc_last`), allowing
	 * custom instrumentation, logging, debugging, or tracking.
	 *
	 * The hook receives detailed metadata for each allocation, including:
	 * - The arena pointer.
	 * - A unique allocation ID.
	 * - The pointer to the allocated memory.
	 * - The size and offset of the allocation.
	 * - The number of alignment/wasted bytes.
	 * - An optional user-defined label (may be `NULL`).
	 *
	 * If `cb` is `NULL`, the current hook will be removed and no further
	 * allocation events will be triggered.
	 *
	 * @param arena   Pointer to the arena to attach the hook to.
	 * @param cb      Hook callback function, or `NULL` to disable.
	 * @param context Optional user data or context to associate with the hook (not used internally).
	 *
	 * @return void
	 *
	 * @ingroup arena_alloc
	 *
	 * @note
	 * Hooks are invoked inside the `arena_alloc_internal()` implementation.
	 * If thread safety is enabled, this function acquires the arena lock to modify internal state.
	 *
	 * @warning
	 * Hooks should not perform allocations from the same arena to avoid recursion or deadlocks.
	 *
	 * @see arena_allocation_hook
	 * @see arena_alloc
	 * @see arena_hooks.h
	 */
	void arena_set_allocation_hook(t_arena* arena, arena_allocation_hook cb, void* context);

#ifdef __cplusplus
}
#endif

#endif // ARENA_HOOKS_H
