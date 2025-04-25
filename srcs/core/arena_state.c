/**
 * @file arena_state.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Arena state inspection and control utilities.
 *
 * @details
 * This module provides utility functions for inspecting and manipulating the
 * state of a memory arena without modifying its underlying buffer directly.
 * These functions support read-only queries (e.g., used, remaining, peak usage),
 * as well as efficient bulk memory rollback via markers (`arena_mark()` and
 * `arena_pop()`) and full arena reset.
 *
 * Features:
 * - Query memory usage (`arena_used`, `arena_remaining`, `arena_peak`).
 * - Save and restore allocation state using `arena_mark()` and `arena_pop()`.
 * - Fully reset the arena for reuse using `arena_reset()`.
 *
 * These utilities are thread-safe and lock the arena internally before
 * accessing or modifying its state.
 *
 * @note
 * These functions are designed for convenience and performance in scenarios like:
 * - Frame-based memory reuse (e.g., in games or simulations).
 * - Temporary scopes for allocation and rollback.
 * - Profiling and diagnostics (e.g., monitoring peak usage).
 *
 * @ingroup arena_state
 */

#include "arena.h"

/**
 * @brief
 * Get the number of bytes currently used in the arena.
 *
 * @details
 * This function returns the current offset of the arena, which represents
 * the total number of bytes that have been allocated (but not necessarily in use).
 * It does not include alignment padding or internal fragmentation.
 *
 * This is useful for:
 * - Measuring memory usage.
 * - Debugging or profiling arena-based systems.
 * - Saving the state for later rollback via `arena_mark()` and `arena_pop()`.
 *
 * Thread-safe: this function locks the arena before accessing internal state.
 *
 * @param arena Pointer to the arena.
 *
 * @return The number of bytes currently used, or `0` if `arena` is `NULL`.
 *
 * @ingroup arena_state
 *
 * @see arena_remaining
 * @see arena_peak
 * @see arena_mark
 *
 * @example
 * @code
 * t_arena* arena = arena_create(1024, true);
 * arena_alloc(arena, 128);
 * arena_alloc(arena, 64);
 *
 * size_t used = arena_used(arena);
 * printf("Used bytes: %zu\n", used); // Output: 192 (if no padding), or more if aligned
 *
 * arena_delete(&arena);
 * @endcode
 */
size_t arena_used(t_arena* arena)
{
	if (!arena)
		return 0;

	ARENA_LOCK(arena);
	size_t used = arena->offset;
	ARENA_UNLOCK(arena);
	return used;
}

/**
 * @brief
 * Get the number of remaining (free) bytes in the arena.
 *
 * @details
 * This function returns the number of bytes left for allocation in the arena.
 * It computes `arena->size - arena->offset`, giving you insight into how much
 * space is still available before the arena must grow (if growable) or fail
 * subsequent allocations.
 *
 * This is useful for:
 * - Monitoring arena capacity.
 * - Deciding whether to grow the arena or switch to a fallback allocator.
 * - Debugging memory exhaustion or fragmentation issues.
 *
 * Thread-safe: this function acquires the arena lock before accessing internal state.
 *
 * @param arena Pointer to the arena.
 *
 * @return The number of free bytes available, or `0` if `arena` is `NULL`.
 *
 * @ingroup arena_state
 *
 * @see arena_used
 * @see arena_peak
 * @see arena_grow
 *
 * @example
 * @code
 * t_arena* arena = arena_create(256, true);
 * arena_alloc(arena, 64);
 *
 * size_t free_bytes = arena_remaining(arena);
 * printf("Remaining space: %zu bytes\n", free_bytes); // Likely: 192 (or less with padding)
 *
 * arena_delete(&arena);
 * @endcode
 */
size_t arena_remaining(t_arena* arena)
{
	if (!arena)
		return 0;

	ARENA_LOCK(arena);
	size_t remaining = arena->size - arena->offset;
	ARENA_UNLOCK(arena);
	return remaining;
}

/**
 * @brief
 * Return the peak memory usage observed in the arena.
 *
 * @details
 * This function returns the highest number of bytes ever allocated
 * in the arena since it was created or last reset. It is useful
 * for monitoring memory pressure, tuning arena sizing, or profiling
 * high-water marks in long-running systems.
 *
 * This metric is updated internally by the allocator whenever a new
 * allocation pushes the `offset` beyond the current `peak_usage`.
 *
 * Thread-safe: this function acquires the arena lock to ensure
 * accurate and safe access in concurrent environments.
 *
 * @param arena Pointer to the arena to inspect.
 *
 * @return The peak number of bytes used at any point in the arena's lifetime.
 *         Returns `0` if `arena` is `NULL`.
 *
 * @ingroup arena_state
 *
 * @see arena_used
 * @see arena_remaining
 * @see arena_update_peak
 *
 * @example
 * @code
 * t_arena* arena = arena_create(1024, true);
 * arena_alloc(arena, 256);
 * arena_alloc(arena, 128);
 *
 * size_t peak = arena_peak(arena);
 * printf("Peak usage: %zu bytes\n", peak); // Should print 384
 *
 * arena_reset(arena);
 * printf("Peak after reset: %zu bytes\n", arena_peak(arena)); // Still 384 (reset does not affect peak)
 *
 * arena_delete(&arena);
 * @endcode
 */
size_t arena_peak(t_arena* arena)
{
	if (!arena)
		return 0;

	ARENA_LOCK(arena);
	size_t peak = arena->stats.peak_usage;
	ARENA_UNLOCK(arena);
	return peak;
}

/**
 * @brief
 * Capture the current allocation offset as a marker.
 *
 * @details
 * This function returns a marker representing the current position
 * in the arena’s allocation buffer. The marker can be used later with
 * `arena_pop()` to roll back the arena's state to this exact point,
 * effectively freeing all allocations made after the marker.
 *
 * This is useful for implementing temporary memory scopes where multiple
 * allocations are made and discarded in bulk after a certain operation.
 *
 * Thread-safe: this function acquires the arena lock.
 *
 * @param arena Pointer to the arena to mark.
 *
 * @return A marker (offset) that can later be passed to `arena_pop()`.
 *         Returns `0` if `arena` is `NULL`.
 *
 * @ingroup arena_state
 *
 * @note
 * A marker is only valid for the arena it was created from.
 *
 * @see arena_pop
 *
 * @example
 * @code
 * t_arena* arena = arena_create(1024, true);
 * t_arena_marker mark = arena_mark(arena);
 *
 * char* buffer = arena_alloc(arena, 256);
 * // Use buffer...
 *
 * // Roll back to previous state, freeing the last allocation
 * arena_pop(arena, mark);
 *
 * arena_delete(&arena);
 * @endcode
 */
t_arena_marker arena_mark(t_arena* arena)
{
	if (!arena)
		return 0;

	ARENA_LOCK(arena);
	size_t offset = arena->offset;
	ARENA_UNLOCK(arena);
	return offset;
}

/**
 * @brief
 * Revert the arena’s allocation state to a previously saved marker.
 *
 * @details
 * This function restores the arena's offset to a prior state recorded
 * using `arena_mark()`. All memory allocated after the marker is considered
 * discarded. This is a fast and efficient way to bulk-free temporary allocations.
 *
 * - If the provided `marker` is greater than the current offset, the function
 *   logs an error and does nothing.
 * - If the marker is valid, it poisons (optionally overwrites) the discarded
 *   region and rewinds the offset in debug mode.
 *
 * Thread-safe: this function acquires the arena lock.
 *
 * @param arena  Pointer to the arena to rewind.
 * @param marker Marker previously obtained from `arena_mark()`.
 *
 * @return void
 *
 * @ingroup arena_state
 *
 * @note
 * The `marker` must originate from the same arena. Using an invalid marker
 * results in no change and logs an error.
 *
 * @see arena_mark
 * @see arena_reset
 *
 * @example
 * @code
 * t_arena* arena = arena_create(4096, true);
 * t_arena_marker mark = arena_mark(arena);
 *
 * // Temporary memory region
 * char* buffer = arena_alloc(arena, 1024);
 * // Use the buffer...
 *
 * // Discard the buffer and any other allocations made after the marker
 * arena_pop(arena, mark);
 *
 * arena_delete(&arena);
 * @endcode
 */
void arena_pop(t_arena* arena, t_arena_marker marker)
{
	if (!arena)
		return;

	ARENA_LOCK(arena);
	if (marker > arena->offset)
	{
		arena_report_error(arena, "arena_pop failed: invalid marker %zu (offset: %zu)", marker, arena->offset);
		ARENA_UNLOCK(arena);
		return;
	}

	arena_poison_memory(arena->buffer + marker, arena->offset - marker);
	arena->offset = marker;
	ARENA_UNLOCK(arena);
}

/**
 * @brief
 * Reset the arena to an empty state by clearing all allocations.
 *
 * @details
 * This function sets the arena’s internal offset to zero, effectively
 * discarding all previous allocations. The underlying memory buffer is not
 * freed but reused for future allocations.
 *
 * - All memory is overwritten using `arena_poison_memory()` to catch
 *   use-after-reset bugs (only in debug or poison-enabled builds).
 * - Statistics like `peak_usage` and `live_allocations` are not reset.
 *
 * Thread-safe: this function locks the arena during the reset.
 *
 * @param arena Pointer to the arena to reset.
 *
 * @return void
 *
 * @ingroup arena_state
 *
 * @note
 * This is an extremely fast alternative to freeing each allocation individually.
 * Useful in scenarios like frame-based memory usage or scratch allocations.
 *
 * @see arena_pop
 * @see arena_mark
 *
 * @example
 * @code
 * t_arena* arena = arena_create(4096, true);
 *
 * for (int frame = 0; frame < 10; ++frame)
 * {
 *     char* frame_buffer = arena_alloc(arena, 1024);
 *     // Use memory for current frame...
 *
 *     // Reuse arena next frame
 *     arena_reset(arena);
 * }
 *
 * arena_delete(&arena);
 * @endcode
 */
void arena_reset(t_arena* arena)
{
	if (!arena)
		return;

	ARENA_LOCK(arena);
	ARENA_ASSERT_VALID(arena);

	arena_poison_memory(arena->buffer, arena->size);
	arena->offset = 0;

	ARENA_UNLOCK(arena);
}
