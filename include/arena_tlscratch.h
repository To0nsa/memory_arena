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
	 * Retrieve a thread-local scratch arena for fast temporary allocations.
	 *
	 * @details
	 * This function returns a pointer to a thread-local memory arena (`t_arena`)
	 * that is unique to the calling thread. The arena is initialized only once
	 * per thread, using the size configured via `set_thread_scratch_arena_size()`
	 * or the default size (8192 bytes) if not set.
	 *
	 * On every call:
	 * - If the arena is not initialized for this thread, it is created and marked initialized.
	 * - The arena is then reset, clearing any previous allocations.
	 *
	 * This is designed for fast, single-threaded scratch memory that does not require
	 * synchronization. The returned arena can be reused immediately without freeing
	 * individual allocations.
	 *
	 * @return Pointer to the thread-local arena, or `NULL` on failure.
	 *
	 * @ingroup arena_tlscratch
	 *
	 * @note
	 * You should not share this arena across threads. Each thread gets its own instance.
	 *
	 * @see set_thread_scratch_arena_size
	 * @see destroy_thread_scratch_arena
	 * @see arena_reset
	 */
	t_arena* get_thread_scratch_arena(void);

	/**
	 * @brief
	 * Destroy the thread-local scratch arena for the current thread.
	 *
	 * @details
	 * This function deinitializes the thread-local scratch arena created by
	 * `get_thread_scratch_arena()` by calling `arena_destroy()` on it. It is useful
	 * when you want to release the thread’s scratch memory before the thread exits,
	 * or to reset the arena entirely with a fresh configuration.
	 *
	 * After calling this function:
	 * - The arena is marked as uninitialized.
	 * - Any subsequent call to `get_thread_scratch_arena()` will reinitialize it.
	 *
	 * @ingroup arena_tlscratch
	 *
	 * @note
	 * This function only affects the calling thread’s local arena.
	 * It does nothing if the arena has not yet been initialized.
	 *
	 * @see get_thread_scratch_arena
	 * @see set_thread_scratch_arena_size
	 */
	void destroy_thread_scratch_arena(void);

	/**
	 * @brief
	 * Set the initial size for the thread-local scratch arena.
	 *
	 * @details
	 * This function configures the initial size of the thread-local scratch arena.
	 * It must be called **before** the first call to `get_thread_scratch_arena()` in
	 * the current thread. If the arena has already been initialized, this call
	 * has no effect.
	 *
	 * This allows per-thread customization of scratch arena size without modifying
	 * global configuration or headers.
	 *
	 * @param size Desired initial size (in bytes) for the thread-local arena.
	 *
	 * @ingroup arena_tlscratch
	 *
	 * @note
	 * The new size will only take effect for threads that have not yet used
	 * `get_thread_scratch_arena()`. Once initialized, the size is fixed for that thread.
	 *
	 * @warning
	 * This function does not allocate memory. It only sets the size to be used
	 * on the first arena initialization.
	 *
	 * @see get_thread_scratch_arena
	 * @see destroy_thread_scratch_arena
	 */
	void set_thread_scratch_arena_size(size_t size);

	/**
	 * @brief
	 * Internal accessor for the thread-local scratch arena reference.
	 *
	 * @details
	 * This function returns a raw pointer to the thread-local `t_arena` instance
	 * used for scratch allocations in the current thread. Unlike `get_thread_scratch_arena()`,
	 * it does not perform initialization or reset, and may return an uninitialized arena
	 * if called before first use.
	 *
	 * This is intended for low-level tools such as debuggers, allocators,
	 * or introspection tools that need direct access to the arena's internal state.
	 *
	 * @return Pointer to the thread-local `t_arena` instance.
	 *
	 * @ingroup arena_tlscratch
	 *
	 * @note
	 * Do not use this function for normal allocations. Use `get_thread_scratch_arena()`
	 * instead to ensure proper initialization and cleanup.
	 *
	 * @warning
	 * Calling this before the arena has been initialized (via `get_thread_scratch_arena()`)
	 * may result in undefined behavior or access to uninitialized memory.
	 *
	 * @see get_thread_scratch_arena
	 * @see destroy_thread_scratch_arena
	 */
	t_arena* get_thread_scratch_arena_ref(void);

#ifdef __cplusplus
}
#endif

#endif // ARENA_TLSCRATCH_H
