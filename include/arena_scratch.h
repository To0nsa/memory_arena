/**
 * @file arena_scratch.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Scratch arena pool system for fast, reusable temporary allocations.
 *
 * @details
 * This module defines a pool of scratch arenas that can be acquired and released
 * on demand. Each scratch arena is a lightweight, growable `t_arena` suitable
 * for short-lived allocations in performance-critical paths such as rendering,
 * simulation steps, or parsing tasks.
 *
 * The pool supports up to `SCRATCH_MAX_SLOTS` concurrently usable slots.
 * Slots are tracked using atomic flags and can be safely acquired and reused,
 * with optional thread-safe locking.
 *
 * Features:
 * - Fixed-size pool of memory arenas
 * - Optional thread safety via mutex
 * - Simple API: `scratch_acquire()` / `scratch_release()`
 * - Suitable for high-frequency temporary allocations
 *
 * @note
 * You must call `scratch_pool_destroy()` before discarding a pool to release resources.
 *
 * @ingroup arena_scratch
 */

#ifndef ARENA_SCRATCH_H
#define ARENA_SCRATCH_H

#include "arena.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

/// Maximum number of concurrently usable scratch arenas in the pool
#define SCRATCH_MAX_SLOTS 64

/// Thread-safe locking macro (enabled only if `ARENA_ENABLE_THREAD_SAFE` is set)
#ifdef ARENA_ENABLE_THREAD_SAFE
#define SCRATCH_LOCK(pool)   \
	if ((pool)->thread_safe) \
	pthread_mutex_lock(&(pool)->lock)

/// Thread-safe unlocking macro (enabled only if `ARENA_ENABLE_THREAD_SAFE` is set)
#define SCRATCH_UNLOCK(pool) \
	if ((pool)->thread_safe) \
	pthread_mutex_unlock(&(pool)->lock)
#else
#define SCRATCH_LOCK(pool) ((void) 0)
#define SCRATCH_UNLOCK(pool) ((void) 0)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief
	 * Represents a single slot in the scratch arena pool.
	 *
	 * @details
	 * Each slot holds a memory arena (`t_arena`) and an atomic flag
	 * indicating whether the slot is currently in use.
	 *
	 * @ingroup arena_scratch_internal
	 */
	typedef struct s_scratch_slot
	{
		t_arena     arena;  ///< Memory arena associated with this slot
		atomic_bool in_use; ///< Atomic flag for acquisition tracking
	} t_scratch_slot;

	/**
	 * @brief
	 * Scratch arena pool structure holding multiple reusable memory arenas.
	 *
	 * @details
	 * This structure maintains a fixed number of scratch slots that can be
	 * individually acquired and reset for temporary memory needs.
	 *
	 * @ingroup arena_scratch
	 */
	typedef struct s_scratch_arena_pool
	{
		t_scratch_slot  slots[SCRATCH_MAX_SLOTS]; ///< Array of scratch slots
		size_t          slot_size;                ///< Size of each arena in bytes
		bool            thread_safe;              ///< Whether locking is enabled
		pthread_mutex_t lock;                     ///< Mutex for protecting slot access
	} t_scratch_arena_pool;

	/**
	 * @brief
	 * Initialize a scratch arena pool with pre-allocated slots.
	 *
	 * @param pool         Pointer to the scratch pool structure to initialize.
	 * @param slot_size    Size of each individual scratch arena (in bytes).
	 * @param thread_safe  If `true`, enables mutex-based locking for slot access.
	 *
	 * @return `true` if the pool was successfully initialized, `false` otherwise.
	 *
	 * @ingroup arena_scratch
	 *
	 * @see scratch_pool_destroy
	 */
	bool scratch_pool_init(t_scratch_arena_pool* pool, size_t slot_size, bool thread_safe);

	/**
	 * @brief
	 * Destroy all arenas in a scratch pool and reset its metadata.
	 *
	 * @param pool Pointer to the scratch arena pool to destroy.
	 *
	 * @ingroup arena_scratch
	 *
	 * @note
	 * Must be called to clean up any internally allocated memory.
	 *
	 * @see scratch_pool_init
	 */
	void scratch_pool_destroy(t_scratch_arena_pool* pool);

	/**
	 * @brief
	 * Acquire a temporary scratch arena from the pool.
	 *
	 * @param pool Pointer to the scratch arena pool to acquire from.
	 *
	 * @return Pointer to an available `t_arena`, or `NULL` if none are free.
	 *
	 * @ingroup arena_scratch
	 *
	 * @see scratch_release
	 */
	t_arena* scratch_acquire(t_scratch_arena_pool* pool);

	/**
	 * @brief
	 * Release a scratch arena back to the pool for reuse.
	 *
	 * @param pool   Pointer to the scratch pool the arena belongs to.
	 * @param arena  Pointer to the arena to release.
	 *
	 * @ingroup arena_scratch
	 *
	 * @see scratch_acquire
	 */
	void scratch_release(t_scratch_arena_pool* pool, t_arena* arena);

#ifdef __cplusplus
}
#endif

#endif // ARENA_SCRATCH_H
