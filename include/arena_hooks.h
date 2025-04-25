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
	 * Set or remove an allocation hook on a given arena.
	 *
	 * @details
	 * This function installs a callback that will be invoked after each
	 * successful allocation (including realloc and calloc) within the specified arena.
	 *
	 * If `cb` is `NULL`, any previously installed hook is removed.
	 *
	 * @param arena   Pointer to the target arena.
	 * @param cb      Hook callback function (or `NULL` to disable).
	 * @param context Optional user-defined context passed to the hook.
	 *
	 * @return void
	 *
	 * @ingroup arena_alloc
	 *
	 * @warning
	 * Do not perform allocations from the same arena within the hook itself.
	 * Doing so can cause recursion or deadlocks.
	 *
	 * @see arena_allocation_hook
	 */
	void arena_set_allocation_hook(t_arena* arena, arena_allocation_hook cb, void* context);

#ifdef __cplusplus
}
#endif

#endif // ARENA_HOOKS_H
