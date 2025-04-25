/**
 * @file arena_tlscratch.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Thread-local scratch arena interface for fast temporary allocations.
 *
 * @details
 * This header declares the API for managing thread-local memory arenas (`t_arena`)
 * that are allocated and used independently by each thread. These arenas are:
 * - Initialized on first use
 * - Automatically reset before each access
 * - Isolated from other threads (via `_Thread_local`)
 *
 * This system enables low-overhead temporary memory allocations in concurrent
 * environments without requiring explicit synchronization.
 *
 * Features:
 * - One scratch arena per thread
 * - Customizable initial size via `set_thread_scratch_arena_size()`
 * - Safe teardown via `destroy_thread_scratch_arena()`
 * - Raw access for tooling via `get_thread_scratch_arena_ref()`
 *
 * @note
 * The thread-local scratch system is enabled by the `ARENA_ENABLE_THREAD_LOCAL_SCRATCH` macro.
 * It is **not** intended for use across threads and is separate from the general-purpose scratch pool.
 *
 * @ingroup arena_tlscratch
 */

#ifndef ARENA_TLSCRATCH_H
#define ARENA_TLSCRATCH_H

#include "arena.h"
#include "arena_tlscratch.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief
	 * Get the thread-local scratch arena, resetting it before use.
	 *
	 * @return Pointer to the thread-local `t_arena`, or `NULL` if disabled or failed.
	 *
	 * @ingroup arena_tlscratch
	 *
	 * @see destroy_thread_scratch_arena
	 * @see set_thread_scratch_arena_size
	 */
	t_arena* get_thread_scratch_arena(void);

	/**
	 * @brief
	 * Destroy the thread-local scratch arena for the calling thread.
	 *
	 * @ingroup arena_tlscratch
	 *
	 * @note
	 * This resets internal state and frees any allocated buffer.
	 *
	 * @see get_thread_scratch_arena
	 */
	void destroy_thread_scratch_arena(void);

	/**
	 * @brief
	 * Configure the initial size for the thread-local scratch arena.
	 *
	 * @param size Desired size in bytes.
	 *
	 * @ingroup arena_tlscratch
	 *
	 * @note
	 * Must be called **before** `get_thread_scratch_arena()` is used in the same thread.
	 */
	void set_thread_scratch_arena_size(size_t size);

	/**
	 * @brief
	 * Access the raw thread-local arena pointer without initialization.
	 *
	 * @return Raw pointer to the current thread's `t_arena`.
	 *
	 * @ingroup arena_tlscratch
	 *
	 * @warning
	 * May return an uninitialized arena if accessed before `get_thread_scratch_arena()`.
	 */
	t_arena* get_thread_scratch_arena_ref(void);

#ifdef __cplusplus
}
#endif

#endif // ARENA_TLSCRATCH_H
