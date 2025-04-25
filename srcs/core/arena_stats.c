/**
 * @file arena_stats.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Arena statistics and diagnostics module.
 *
 * @details
 * This module provides tools for inspecting, printing, recording, and retrieving
 * memory usage statistics from a memory arena (`t_arena`). These features are
 * useful for profiling, debugging, and tracking memory behavior in applications
 * that use arenas for allocation.
 *
 * The statistics include:
 * - Number of allocations and reallocations
 * - Peak memory usage and current offset
 * - Bytes allocated and wasted due to alignment
 * - Last allocation metadata
 * - Allocation failures and growth events
 * - Growth history tracking (if dynamic resizing is enabled)
 *
 * Core functions:
 * - `arena_print_stats()`: Dump readable statistics to a `FILE*` stream.
 * - `arena_stats_record_growth()`: Track size increases during growth.
 * - `arena_get_stats()`: Return a snapshot copy of all statistics.
 * - `arena_stats_reset()`: Clear all statistics to start fresh (defined elsewhere).
 *
 * All functions are safe to use in multi-threaded environments when thread safety
 * is enabled in the arena.
 *
 * @note
 * Statistics are updated internally by the arena system. Most users will only
 * need to use the print or get functions for diagnostics.
 *
 * @ingroup arena_stats
 */

#include "arena.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief
 * Print detailed diagnostics about the given arena to a stream.
 *
 * @details
 * This function outputs a formatted summary of the current arena state to the
 * specified `FILE*` stream. It includes memory layout, allocation statistics,
 * debug metadata, and growth history (if available).
 *
 * Sections reported:
 * - Basic layout: buffer address, size, offset, remaining space
 * - Allocation stats: total allocations, reallocations, failures, alignment waste
 * - Last allocation metadata: size, offset, ID
 * - Debug info: ID, label, hook presence, thread safety
 * - Growth history: sizes recorded during dynamic expansion
 *
 * This function is useful for:
 * - Debugging memory usage patterns
 * - Verifying arena configuration at runtime
 * - Profiling custom allocators using arena-backed systems
 *
 * @param arena  Pointer to the arena to inspect.
 * @param stream Output stream to write the diagnostics to (e.g., `stdout`, `stderr`, or a log file).
 *
 * @return void
 *
 * @ingroup arena_stats
 *
 * @note
 * If `arena` or `stream` is `NULL`, the function does nothing.
 */
void arena_print_stats(t_arena* arena, FILE* stream)
{
	if (!arena || !stream)
		return;

	fprintf(stream, "──────────────────────────────\n");
	fprintf(stream, " Arena Diagnostics (%s)\n", arena->debug.label ? arena->debug.label : "unnamed");
	fprintf(stream, "──────────────────────────────\n");

	// Buffer & memory layout
	fprintf(stream, "- Buffer Address:         %p\n", (void*) arena->buffer);
	fprintf(stream, "- Buffer Size:            %zu bytes\n", arena->size);
	fprintf(stream, "- Current Offset:         %zu bytes\n", arena->offset);
	fprintf(stream, "- Remaining Space:        %zu bytes\n", arena->size - arena->offset);
	fprintf(stream, "- Peak Usage:             %zu bytes\n", arena->stats.peak_usage);
	fprintf(stream, "- Can Grow:               %s\n",
	        atomic_load_explicit(&arena->can_grow, memory_order_acquire) ? "yes" : "no");

	// Allocation tracking
	fprintf(stream, "- Allocations:            %zu\n", arena->stats.allocations);
	fprintf(stream, "- Reallocations:          %zu\n", arena->stats.reallocations);
	fprintf(stream, "- Failed Allocations:     %zu\n", arena->stats.failed_allocations);
	fprintf(stream, "- Live Allocations:       %zu\n", arena->stats.live_allocations);
	fprintf(stream, "- Bytes Allocated:        %zu bytes\n", arena->stats.bytes_allocated);
	fprintf(stream, "- Wasted Alignment Bytes: %zu bytes\n", arena->stats.wasted_alignment_bytes);
	fprintf(stream, "- Shrinks:                %zu\n", arena->stats.shrinks);

	// Last allocation details
	fprintf(stream, "- Last Alloc Size:        %zu bytes\n", arena->stats.last_alloc_size);
	fprintf(stream, "- Last Alloc Offset:      %zu\n", arena->stats.last_alloc_offset);
	fprintf(stream, "- Last Alloc ID:          %zu\n", arena->stats.last_alloc_id);

	// Debug / internal metadata
	fprintf(stream, "- Debug ID:               %s\n", arena->debug.id);
	fprintf(stream, "- Subarena Counter:       %d\n", arena->debug.subarena_counter);
	fprintf(stream, "- Hook Installed:         %s\n", arena->hooks.hook_cb ? "yes" : "no");
#ifdef ARENA_ENABLE_THREAD_SAFE
	fprintf(stream, "- Thread Safety:          %s\n", arena->use_lock ? "enabled" : "disabled");
#endif

	// Growth history
	if (arena->stats.growth_history_count > 0)
	{
		fprintf(stream, "- Growth History:         ");
		for (size_t i = 0; i < arena->stats.growth_history_count; ++i)
			fprintf(stream, "%zu ", arena->stats.growth_history[i]);
		fprintf(stream, "\n");
	}
	else
	{
		fprintf(stream, "- Growth History:         (none)\n");
	}
}

/**
 * @brief
 * Record a new arena growth event in the statistics.
 *
 * @details
 * This function appends a new size value to the `growth_history` array
 * inside the provided `t_arena_stats` structure. It tracks each time
 * the arena grows dynamically (via `arena_grow()`), enabling tools and
 * diagnostics to analyze memory usage trends over time.
 *
 * Internally:
 * - It reallocates the `growth_history` array to make room for the new entry.
 * - Appends the `new_size` to the end of the list.
 * - Increments the `growth_history_count`.
 *
 * If memory reallocation fails, the function logs a debug message via `ALOG`
 * (if debug logging is enabled) and exits silently without recording the growth.
 *
 * @param stats     Pointer to the `t_arena_stats` structure to update.
 * @param new_size  The new arena size (in bytes) after a growth operation.
 *
 * @return void
 *
 * @ingroup arena_stats
 *
 * @note
 * This function is typically called automatically by `arena_grow()`.
 * The `growth_history` buffer is dynamically managed and grows linearly.
 *
 * @warning
 * This function does nothing if `stats` is `NULL`.
 * Memory allocation failure is non-fatal but results in data loss for this event.
 *
 * @see arena_grow
 * @see arena_print_stats
 */
void arena_stats_record_growth(t_arena_stats* stats, size_t new_size)
{
	if (!stats)
		return;

	size_t  count      = stats->growth_history_count;
	size_t* new_buffer = realloc(stats->growth_history, (count + 1) * sizeof(size_t));
	if (!new_buffer)
	{
		ALOG("arena_stats_record_growth: failed to realloc for growth history\n");
		return;
	}

	stats->growth_history        = new_buffer;
	stats->growth_history[count] = new_size;
	stats->growth_history_count++;
}

/**
 * @brief
 * Retrieve a snapshot of the current arena statistics.
 *
 * @details
 * This function returns a copy of the `t_arena_stats` structure from
 * the specified arena. The statistics include information such as:
 * - Number of allocations and reallocations
 * - Bytes allocated and alignment overhead
 * - Peak usage and current allocation ID
 * - Growth history and tracking counters
 *
 * This is useful for diagnostics, profiling, logging, or exporting
 * memory usage metrics to external tools.
 *
 * The returned stats are safe to use outside the arena and will not
 * be affected by future changes to the arena state.
 *
 * Thread-safe: acquires the arena lock before reading the data.
 *
 * @param arena Pointer to the arena to inspect.
 *
 * @return A copy of the arena’s current statistics. If `arena` is `NULL`,
 *         returns a zero-initialized `t_arena_stats` structure.
 *
 * @ingroup arena_stats
 *
 * @note
 * This function performs a shallow copy. The `growth_history` pointer
 * in the returned struct still points to internal memory managed by the arena.
 * Do not modify or free it manually.
 *
 * @see arena_print_stats
 * @see arena_stats_record_growth
 * @see arena_stats_reset
 */
t_arena_stats arena_get_stats(const t_arena* arena)
{
	t_arena_stats copy;
	memset(&copy, 0, sizeof(copy));

	if (!arena)
		return copy;

	ARENA_LOCK((t_arena*) arena);

	copy = arena->stats;

	ARENA_UNLOCK((t_arena*) arena);
	return copy;
}
