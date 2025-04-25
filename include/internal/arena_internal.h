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
	 * Check whether the given arena is in a valid, initialized state.
	 *
	 * @param arena Pointer to the arena to check.
	 * @return `true` if the arena is valid, `false` otherwise.
	 *
	 * @ingroup arena_internal
	 */
	bool arena_is_valid(const t_arena* arena);

	/**
	 * @brief
	 * Default growth policy callback for arena resizing.
	 *
	 * @details
	 * Computes the next size for an arena that needs to grow.
	 * The result is typically a geometric increase (e.g., 2x rule),
	 * optionally adjusted to accommodate the requested size.
	 *
	 * @param current_size    The current buffer size of the arena.
	 * @param requested_size  The size that triggered the growth.
	 * @return A new size value suitable for reallocation.
	 *
	 * @ingroup arena_internal
	 */
	size_t default_grow_cb(size_t current_size, size_t requested_size);

	/**
	 * @brief
	 * Update the arena’s peak memory usage if the current usage exceeds it.
	 *
	 * @param arena Pointer to the arena to update.
	 * @return void
	 *
	 * @ingroup arena_internal
	 */
	void arena_update_peak(t_arena* arena);

	/**
	 * @brief
	 * Zero out and reset all debug/stats metadata associated with the arena.
	 *
	 * @param arena Pointer to the arena to zero.
	 * @return void
	 *
	 * @ingroup arena_internal
	 */
	void arena_zero_metadata(t_arena* arena);

#ifdef __cplusplus
}
#endif

#endif // ARENA_INTERNAL_H
