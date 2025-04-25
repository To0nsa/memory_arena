/**
 * @file arena_internal.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Internal utilities and helpers for arena memory management.
 *
 * @details
 * This header defines functions and macros used internally by the arena system.
 * It includes synchronization macros (`ARENA_LOCK`, `ARENA_UNLOCK`), metadata utilities,
 * and default logic for peak tracking and growth behavior.
 *
 * These APIs are **not** intended for external use and may change without notice.
 *
 * @ingroup arena_internal
 */

#ifndef ARENA_INTERNAL_H
#define ARENA_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ─────────────────────────────────────────────────────────────
// Synchronization macros
// ─────────────────────────────────────────────────────────────

/**
 * @def ARENA_LOCK
 * @brief Acquire the mutex lock if thread safety is enabled.
 * @param arena Pointer to the arena.
 */
#ifdef ARENA_ENABLE_THREAD_SAFE
#define ARENA_LOCK(arena)                       \
	do                                          \
	{                                           \
		if ((arena)->use_lock)                  \
			pthread_mutex_lock(&(arena)->lock); \
	} while (0)

/**
 * @def ARENA_UNLOCK
 * @brief Release the mutex lock if thread safety is enabled.
 * @param arena Pointer to the arena.
 */
#define ARENA_UNLOCK(arena)                       \
	do                                            \
	{                                             \
		if ((arena)->use_lock)                    \
			pthread_mutex_unlock(&(arena)->lock); \
	} while (0)
#else
#define ARENA_LOCK(arena) ((void) 0)
#define ARENA_UNLOCK(arena) ((void) 0)
#endif

	// ─────────────────────────────────────────────────────────────
	// Internal helper functions
	// ─────────────────────────────────────────────────────────────

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
	bool arena_is_valid(const t_arena* arena);

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
	size_t default_grow_cb(size_t current_size, size_t requested_size);

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
	void arena_update_peak(t_arena* arena);

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
	void arena_zero_metadata(t_arena* arena);

#ifdef __cplusplus
}
#endif

#endif // ARENA_INTERNAL_H
