/**
 * @file arena_hooks.c
 * @author Toonsa
 * @date 2025
 * @brief
 * Allocation hook management for tracking and debugging arena memory usage.
 *
 * @details
 * This module provides support for installing user-defined callback functions
 * (hooks) that are triggered on every allocation performed within an arena.
 *
 * Hooks can be used to:
 * - Log detailed memory events (size, offset, label, wasted alignment, etc.).
 * - Visualize live allocation activity during development.
 * - Implement custom debugging or telemetry tools (e.g. leak detection).
 * - Track per-system or per-feature memory usage via labeled allocations.
 * - Build profiling dashboards or integrate with real-time allocators.
 *
 * Hooks are invoked only on successful allocations from:
 * - `arena_alloc`
 * - `arena_calloc`
 * - `arena_realloc_last`
 *
 * Arena allocation hooks allow you to observe memory behavior **without altering**
 * your allocation logic. You can:
 * - Monitor how much memory specific systems consume.
 * - Associate metadata (e.g. labels or IDs) with allocations for better insight.
 * - Track peak usage, wasted padding, or allocation spikes.
 * - Export allocation data for runtime visualization or testing reports.
 *
 * How to use:
 * 1. Define a hook function of type `arena_allocation_hook`:
 * @code
 * void my_hook(t_arena* arena, int id, void* ptr, size_t size, size_t offset, size_t wasted, const char* label) {
 *     printf("[hook] ID: %d | size: %zu | label: %s\n", id, size, label ? label : "none");
 * }
 * @endcode
 *
 * 2. Register it with the arena:
 * @code
 * arena_set_allocation_hook(my_arena, my_hook, NULL);
 * @endcode
 *
 * 3. Make allocations as usual. Your hook will be invoked on each.
 *
 * You can also unset the hook by passing `NULL` to disable tracking.
 *
 * @warning
 * The hook callback must not perform allocations from the same arena,
 * or it may cause infinite recursion or deadlocks.
 *
 * @note
 * Hooks are optional. If unset (`NULL`), no callbacks are triggered.
 * Hooks can be updated or cleared at any time via `arena_set_allocation_hook()`,
 * if `cb` is `NULL`, the current hook will be removed and no further
 * allocation events will be displayed.
 *
 * @ingroup arena_alloc
 *
 * @see arena_allocation_hook
 * @see arena_set_allocation_hook
 * @see arena_hooks.h
 * @see arena_alloc_internal
 *
 * @example
 * @brief
 * Example of using an allocation hook to log arena allocations.
 *
 * @details
 * This example installs a custom hook that prints detailed
 * allocation metadata every time memory is allocated from the arena.
 * It is useful for debugging, tracing memory patterns, or integrating
 * with external tools.
 *
 * @code
 * #include "arena.h"
 * #include <stdio.h>
 *
 * // Simple logging hook: print metadata of each allocation
 * void log_allocation_hook(t_arena* arena, int alloc_id, void* ptr, size_t size,
 *                           size_t offset, size_t wasted, const char* label)
 * {
 *     (void) arena;
 *     printf("[ALLOC] id=%d, ptr=%p, size=%zu, offset=%zu, wasted=%zu, label=%s\n",
 *            alloc_id,
 *            ptr,
 *            size,
 *            offset,
 *            wasted,
 *            label ? label : "(none)");
 * }
 *
 * int main(void)
 * {
 *     t_arena* arena = arena_create(1024, true); // Growable arena
 *     if (!arena)
 *         return 1;
 *
 *     // Install the allocation hook
 *     arena_set_allocation_hook(arena, log_allocation_hook, NULL);
 *
 *     // Perform some allocations with labels
 *     void* ptr1 = arena_alloc_labeled(arena, 64, "init");
 *     void* ptr2 = arena_alloc_labeled(arena, 128, "main_buffer");
 *
 *     // Cleanup
 *     arena_delete(&arena);
 *     return 0;
 * }
 * @endcode
 * @par Expected output:
 * @code
 * [ALLOC] id=0, ptr=0x55f3a0c00400, size=64, offset=64, wasted=0, label=init
 * [ALLOC] id=1, ptr=0x55f3a0c00440, size=128, offset=192, wasted=0, label=main_buffer
 * @endcode
 *
 * @note
 * Actual pointer addresses (`ptr`) may vary between runs.
 */

#include "arena.h"

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
void arena_set_allocation_hook(t_arena* arena, arena_allocation_hook cb, void* context)
{
	if (!arena)
	{
		arena_report_error(NULL, "arena_set_allocation_hook failed: NULL arena");
		return;
	}

	ARENA_LOCK(arena);
	arena->hooks.hook_cb = cb;
	arena->hooks.context = context;
	ARENA_UNLOCK(arena);
}