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
	 * @details
	 * This function sets up a reusable pool of scratch arenas, each
	 * initialized to the same `slot_size`. It prepares the internal
	 * array of slots, resets metadata, and (if enabled) configures
	 * a mutex for thread safety.
	 *
	 * For each slot:
	 * - An arena is initialized with the specified `slot_size`.
	 * - The slot is marked as not in use.
	 *
	 * If any arena initialization fails, the entire pool is destroyed
	 * via `scratch_pool_destroy()` and the function returns `false`.
	 *
	 * @param pool         Pointer to the scratch pool structure to initialize.
	 * @param slot_size    Size of each individual scratch arena (in bytes).
	 * @param thread_safe  If `true`, enables mutex-based locking for slot access.
	 *
	 * @return `true` if the pool was successfully initialized, `false` otherwise.
	 *
	 * @ingroup arena_scratch
	 *
	 * @note
	 * All arenas in the pool are marked as growable by default.
	 *
	 * @warning
	 * You must call `scratch_pool_destroy()` to clean up the pool before deallocating it.
	 *
	 * @see scratch_slot_init
	 * @see scratch_pool_destroy
	 * @see scratch_acquire
	 * @see scratch_release
	 */
	bool scratch_pool_init(t_scratch_arena_pool* pool, size_t slot_size, bool thread_safe);

	/**
	 * @brief
	 * Destroy all arenas in a scratch pool and reset its metadata.
	 *
	 * @details
	 * This function deinitializes all scratch arenas in the pool by calling
	 * `arena_destroy()` on each slot. If the pool was initialized as thread-safe,
	 * it also destroys the associated mutex.
	 *
	 * After cleanup, the pool's memory is zeroed out using `memset` to prevent
	 * accidental reuse or access to invalidated arenas.
	 *
	 * This function must be called before deallocating or discarding a scratch pool,
	 * especially in cases where initialization (`scratch_pool_init`) succeeded only
	 * partially or the pool is no longer needed.
	 *
	 * @param pool Pointer to the scratch arena pool to destroy.
	 *
	 * @return void
	 *
	 * @ingroup arena_scratch
	 *
	 * @note
	 * This function is idempotent: calling it on a `NULL` pool is safe and has no effect.
	 *
	 * @see scratch_pool_init
	 * @see arena_destroy
	 */
	void scratch_pool_destroy(t_scratch_arena_pool* pool);

	/**
	 * @brief
	 * Acquire a temporary scratch arena from the pool.
	 *
	 * @details
	 * This function returns a pointer to a scratch arena from the given pool
	 * that is currently available (i.e., not in use). Internally, it scans the
	 * pool’s slots and uses an atomic flag to claim one safely, even in multithreaded contexts.
	 *
	 * Upon successful acquisition:
	 * - The arena is marked as in-use via `atomic_exchange()`.
	 * - Its memory is cleared using `arena_reset()` before returning.
	 *
	 * If no arenas are available, the function returns `NULL` and reports an error.
	 * This may happen when all slots are concurrently in use or exhausted.
	 *
	 * @param pool Pointer to the scratch arena pool to acquire from.
	 *
	 * @return Pointer to a `t_arena` if one is available, or `NULL` on failure.
	 *
	 * @ingroup arena_scratch
	 *
	 * @note
	 * The returned arena must be released using `scratch_release()` when no longer needed.
	 *
	 * @warning
	 * The arena is not thread-safe by default. Do not use the same scratch slot
	 * concurrently without external synchronization.
	 *
	 * @see scratch_release
	 * @see arena_reset
	 */
	t_arena* scratch_acquire(t_scratch_arena_pool* pool);

	/**
	 * @brief
	 * Release a scratch arena back to the pool for reuse.
	 *
	 * @details
	 * This function marks a previously acquired scratch arena as available again.
	 * It searches the pool’s slots for a match with the provided arena pointer,
	 * and clears the `in_use` flag using `atomic_store()`.
	 *
	 * This allows the arena to be reused by future calls to `scratch_acquire()`.
	 *
	 * If the provided arena does not match any slot in the pool, the function
	 * logs an error via `arena_report_error()` and does nothing.
	 *
	 * @param pool   Pointer to the scratch pool the arena belongs to.
	 * @param arena  Pointer to the arena to release.
	 *
	 * @ingroup arena_scratch
	 *
	 * @note
	 * It is safe to call this function multiple times with the same arena,
	 * but doing so without matching `scratch_acquire()` calls may cause logic errors.
	 *
	 * @warning
	 * The arena must have been obtained from the same pool via `scratch_acquire()`.
	 * Passing an invalid or mismatched pointer results in an error.
	 *
	 * @see scratch_acquire
	 */
	void scratch_release(t_scratch_arena_pool* pool, t_arena* arena);

#ifdef __cplusplus
}
#endif

#endif // ARENA_SCRATCH_H
