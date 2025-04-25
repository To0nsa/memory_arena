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
	 * Print formatted diagnostics about the arena to a stream.
	 *
	 * @param arena  Pointer to the arena to inspect.
	 * @param stream Stream to write the output to (e.g., `stdout`, `stderr`).
	 *
	 * @ingroup arena_stats
	 *
	 * @see arena_get_stats
	 */
	void arena_print_stats(t_arena* arena, FILE* stream);

	/**
	 * @brief
	 * Return a snapshot of the arena's internal statistics.
	 *
	 * @param arena Pointer to the arena.
	 * @return A copy of its `t_arena_stats` structure.
	 *
	 * @ingroup arena_stats
	 *
	 * @see arena_print_stats
	 * @see arena_stats_reset
	 */
	t_arena_stats arena_get_stats(const t_arena* arena);

	/**
	 * @brief
	 * Reset the given statistics structure to zero.
	 *
	 * @param stats Pointer to the stats structure to reset.
	 *
	 * @ingroup arena_stats
	 *
	 * @note
	 * This does not affect the arena’s internal stats unless you explicitly assign it back.
	 */
	void arena_stats_reset(t_arena_stats* stats);

	/**
	 * @brief
	 * Append a new growth event to the `growth_history` array.
	 *
	 * @param stats     Pointer to the stats structure to update.
	 * @param size      The new size of the arena after a growth.
	 *
	 * @ingroup arena_stats
	 *
	 * @see arena_print_stats
	 */
	void arena_stats_record_growth(t_arena_stats* stats, size_t size);

#ifdef __cplusplus
}
#endif

#endif // ARENA_STATS_H
