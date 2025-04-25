/**
 * @file arena_internal.c
 * @author Toonsa
 * @date 2025
 *
 * @brief Internal utilities and metadata management for the Arena memory system.
 *
 * @details
 * This file contains internal helper functions used by the arena allocator
 * to manage memory growth, statistics, state validation, and metadata resets.
 * These utilities are not intended for direct use by external applications.
 *
 * Features implemented in this file:
 * - Arena growth policy (`default_grow_cb`)
 * - Peak usage tracking (`arena_update_peak`)
 * - Arena state validation (`arena_is_valid`)
 * - Statistics reset (`arena_stats_reset`)
 * - Metadata clearing (`arena_zero_metadata`)
 *
 * These functions are used throughout the arena implementation during
 * initialization, allocation, and cleanup phases.
 *
 * @note
 * All functions in this file are considered part of the `@ingroup arena_internal`.
 * They are designed to be stable within the implementation.
 *
 * @ingroup arena_internal
 */

#include "arena.h"
#include <stdbool.h>

/**
 * @brief
 * Default growth strategy for arenas that support dynamic resizing.
 *
 * @details
 * This function computes a new arena size when a memory allocation
 * request exceeds the current capacity. The growth strategy works as follows:
 *
 * - If the current size is 0, it starts with a base size of 64 bytes.
 * - It ensures no overflow occurs when adding `current_size + requested_size`.
 * - It then doubles the size repeatedly until the new size is sufficient
 *   to satisfy the allocation.
 * - If doubling is not enough (due to near `SIZE_MAX`), it falls back
 *   to a minimal sufficient size.
 *
 * This strategy offers exponential growth (geometric progression)
 * to minimize reallocations and fragmentation.
 *
 * @param current_size     Current size of the arena in bytes.
 * @param requested_size   Additional memory needed for the new allocation.
 *
 * @return The new size in bytes, large enough to accommodate the request.
 *         Returns `SIZE_MAX` on overflow.
 *
 * @ingroup arena_internal
 *
 * @note
 * You can override this behavior by assigning a custom function to `arena->grow_cb`.
 * For example, use a fixed increment strategy instead of exponential growth
 * if your use case favors more predictable memory sizing or tighter control over memory usage:
 *
 * @code
 * arena->grow_cb = [](size_t current, size_t required) {
 *     return current + required + 128; // Fixed buffer with 128-byte padding
 * };
 * @endcode
 *
 * Reasons to use a custom strategy:
 * - You are working in a memory-constrained environment (e.g., embedded systems).
 * - You want deterministic memory usage patterns.
 * - You have prior knowledge of allocation patterns and want to tune growth granularity.
 *
 * @see arena_grow
 * @see arena_init_with_buffer
 */
size_t default_grow_cb(size_t current_size, size_t requested_size)
{
	size_t new_size = current_size > 0 ? current_size : 64;

	if (requested_size > SIZE_MAX - current_size)
		return SIZE_MAX;

	const size_t needed = current_size + requested_size;

	while (new_size < needed && new_size <= SIZE_MAX / 2)
		new_size *= 2;

	if (new_size < needed)
		new_size = needed;

	return new_size;
}

/**
 * @brief
 * Update the peak usage metric of the arena.
 *
 * @details
 * This internal function updates the `peak_usage` statistic in the arena
 * if the current offset exceeds the previously recorded peak. This metric
 * reflects the highest memory usage observed since the arena was initialized
 * or last reset.
 *
 * It uses locking to ensure thread-safe updates in concurrent environments.
 *
 * @param arena Pointer to the arena whose peak usage should be updated.
 *
 * @ingroup arena_internal
 *
 * @note
 * This function is typically called after a successful allocation or reallocation.
 *
 * @see arena_commit_allocation
 * @see update_realloc_stats
 */
void arena_update_peak(t_arena* arena)
{
	ARENA_LOCK(arena);
	if (arena->offset > arena->stats.peak_usage)
		arena->stats.peak_usage = arena->offset;
	ARENA_UNLOCK(arena);
}

/**
 * @brief
 * Check whether an arena is in a valid state.
 *
 * @details
 * This function verifies the structural integrity of a `t_arena` instance.
 * It ensures:
 * - The arena pointer is not `NULL`.
 * - The buffer pointer is initialized.
 * - The size is non-zero and the current offset does not exceed the size.
 *
 * This is useful for defensive checks before performing operations on an arena.
 *
 * @param arena Pointer to the arena to validate.
 *
 * @return `true` if the arena is valid, `false` otherwise.
 *
 * @ingroup arena_internal
 *
 * @note
 * This function does not verify internal metadata such as mutex state or hooks.
 *
 * @see arena_init
 */
bool arena_is_valid(const t_arena* arena)
{
	if (!arena)
		return false;
	if (!arena->buffer)
		return false;
	if (arena->size == 0 || arena->offset > arena->size)
		return false;
	return true;
}

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
 * @ingroup arena_internal
 *
 * @note
 * If `stats` is `NULL`, the function does nothing.
 *
 * @see arena_zero_metadata
 */
void arena_stats_reset(t_arena_stats* stats)
{
	if (!stats)
		return;

	stats->allocations            = 0;
	stats->reallocations          = 0;
	stats->failed_allocations     = 0;
	stats->live_allocations       = 0;
	stats->bytes_allocated        = 0;
	stats->wasted_alignment_bytes = 0;
	stats->shrinks                = 0;
	stats->peak_usage             = 0;
	stats->last_alloc_size        = 0;
	stats->last_alloc_offset      = 0;
	stats->last_alloc_id          = (size_t) -1;
	stats->alloc_id_counter       = 0;
	stats->growth_history         = NULL;
	stats->growth_history_count   = 0;
}

/**
 * @brief
 * Reset all metadata fields in an arena to their default values.
 *
 * @details
 * This function clears all metadata inside a `t_arena` structure, preparing
 * it for safe reinitialization. It does **not** free or allocate memory,
 * but resets all fields related to memory layout, statistics, growth policies,
 * debugging, and hooks.
 *
 * Specifically, it:
 * - Sets the buffer pointer, size, and offset to zero.
 * - Resets ownership and growth flags atomically.
 * - Clears growth callback and parent references.
 * - Resets all statistical counters via `arena_stats_reset()`.
 * - Clears debug fields including ID, label, and error context.
 * - Resets hook-related pointers.
 *
 * Use this when:
 * - Preparing an arena for reuse.
 * - Initializing a static or pre-allocated arena.
 *
 * @param arena Pointer to the `t_arena` structure to reset.
 *
 * @ingroup arena_internal
 *
 * @note
 * This does **not** destroy any allocated memory. It should typically be
 * called before initializing the arena with a new or reused buffer.
 *
 * @see arena_stats_reset
 * @see arena_init_with_buffer
 */
void arena_zero_metadata(t_arena* arena)
{
	arena->buffer = NULL;
	arena->size   = 0;
	arena->offset = 0;

	atomic_store_explicit(&arena->owns_buffer, false, memory_order_release);
	atomic_store_explicit(&arena->can_grow, false, memory_order_release);

	arena->grow_cb    = NULL;
	arena->parent_ref = NULL;

	arena_stats_reset(&arena->stats);

	memset(arena->debug.id, 0, ARENA_ID_LEN);
	arena->debug.label            = NULL;
	arena->debug.error_cb         = NULL;
	arena->debug.error_context    = NULL;
	arena->debug.subarena_counter = 0;

	arena->hooks.hook_cb = NULL;
	arena->hooks.context = NULL;
}
