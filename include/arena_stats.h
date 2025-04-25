/**
 * @file arena_stats.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Runtime statistics tracking and diagnostics for memory arenas.
 *
 * @details
 * This module defines the `t_arena_stats` structure and a set of utility
 * functions to introspect and manage arena memory usage. It enables users
 * to track allocation metrics, debug issues, and monitor arena behavior over time.
 *
 * Features include:
 * - Allocation and reallocation counters
 * - Peak memory usage tracking
 * - Wasted alignment bytes measurement
 * - Growth history recording for adaptive sizing
 * - Hook-friendly API for visualizers or debuggers
 *
 * Use this module to:
 * - Print real-time diagnostics (`arena_print_stats`)
 * - Fetch current memory usage data (`arena_get_stats`)
 * - Log or reset growth tracking (`arena_stats_record_growth`, `arena_stats_reset`)
 *
 * @note
 * These statistics are gathered automatically by the allocator. Manual updates are rarely needed.
 *
 * @ingroup arena_stats
 */

#ifndef ARENA_STATS_H
#define ARENA_STATS_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief
	 * Structure holding runtime statistics for an arena.
	 *
	 * @details
	 * Tracks all major allocation events, usage metrics, and debug counters.
	 * Fields are updated automatically by internal arena operations.
	 *
	 * @ingroup arena_stats
	 */
	typedef struct s_arena_stats
	{
		size_t  allocations;            ///< Total number of successful allocations
		size_t  reallocations;          ///< Total number of reallocations (via `arena_realloc_last`)
		size_t  bytes_allocated;        ///< Cumulative bytes allocated
		size_t  peak_usage;             ///< Highest offset reached (i.e., peak memory usage)
		size_t  wasted_alignment_bytes; ///< Total bytes wasted due to alignment padding
		size_t  shrinks;                ///< Number of times the arena was shrunk
		size_t* growth_history;         ///< Array of size values recorded during arena growth
		size_t  growth_history_count;   ///< Number of growth events recorded
		size_t  live_allocations;       ///< Number of active allocations (not yet released)
		size_t  last_alloc_size;        ///< Size of the last allocation
		size_t  last_alloc_offset;      ///< Offset of the last allocation
		size_t  last_alloc_id;          ///< Unique ID of the last allocation
		size_t  alloc_id_counter;       ///< Total allocation ID counter (used for tracking)
		size_t  failed_allocations;     ///< Number of failed allocation attempts
	} t_arena_stats;

	struct s_arena;
	typedef struct s_arena t_arena;

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
	void arena_print_stats(t_arena* arena, FILE* stream);

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
	t_arena_stats arena_get_stats(const t_arena* arena);

	/**
	 * @brief
	 * Reset all allocation statistics in an arena stats structure.
	 *
	 * @details
	 * This function clears all fields in a `t_arena_stats` structure,
	 * effectively resetting the arena's statistical tracking to an initial state.
	 *
	 * It resets:
	 * - Allocation and reallocation counters.
	 * - Total bytes allocated and wasted.
	 * - Peak usage and live allocation tracking.
	 * - Last allocation metadata (size, offset, ID).
	 * - Growth history tracking and counter.
	 *
	 * This is typically called during arena initialization or reuse.
	 *
	 * @param stats Pointer to the `t_arena_stats` structure to reset.
	 *
	 * @ingroup arena_state
	 *
	 * @note
	 * If `stats` is `NULL`, the function does nothing.
	 *
	 * @see arena_zero_metadata
	 */
	void arena_stats_reset(t_arena_stats* stats);

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
	void arena_stats_record_growth(t_arena_stats* stats, size_t size);

#ifdef __cplusplus
}
#endif

#endif // ARENA_STATS_H
