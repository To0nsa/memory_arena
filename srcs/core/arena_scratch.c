/**
 * @file arena_scratch.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Implementation of scratch arena pool system for temporary, fast, and reusable allocations.
 *
 * @details
 * This module provides a lightweight pool of scratch arenas designed for short-lived,
 * high-performance memory allocations. A scratch arena pool contains a fixed number
 * of slots (`SCRATCH_MAX_SLOTS`), each backed by its own `t_arena` buffer, allowing
 * for efficient reuse without heap fragmentation or frequent allocation overhead.
 *
 * Key features:
 * - Constant-time acquire/release of temporary memory buffers.
 * - Optional thread-safe access using a configurable mutex.
 * - Designed for use in high-frequency systems (e.g., game frames, task graphs, simulations).
 *
 * When to use:
 * - You need fast, zero-setup memory for small, temporary tasks (e.g., JSON parsing, string builders).
 * - You want to avoid allocating and destroying arenas dynamically.
 * - You want to reuse the same memory region across frames, passes, or worker threads.
 *
 * When **not** to use:
 * - You require dynamic slot count or growing buffer pools.
 * - You need precise lifetime control with ownership semantics (see sub-arenas instead).
 * - You need guaranteed isolation between users (use per-thread arenas or heap).
 *
 * API overview:
 * - `scratch_pool_init()` creates a pool of pre-allocated arenas.
 * - `scratch_acquire()` gives out a reset arena ready for use.
 * - `scratch_release()` returns an arena to the pool.
 * - `scratch_pool_destroy()` frees all internal arenas and clears state.
 *
 * This implementation uses `atomic_bool` for per-slot locking and supports
 * optional `pthread_mutex_t` if `ARENA_ENABLE_THREAD_SAFE` is defined.
 *
 * @note
 * All scratch arenas are growable by default.
 * A typical use pattern is: acquire → allocate → release (no manual free/reset required).
 *
 * @ingroup arena_scratch
 *
 * @example
 * @brief
 * Example usage of the scratch arena pool API for temporary memory buffers.
 *
 * @details
 * This example demonstrates how to use `scratch_acquire()` and `scratch_release()`
 * to manage fast, disposable memory during the handling of short-lived tasks
 * such as formatting temporary strings or processing per-frame data.
 *
 * The scratch pool is initialized once and reused across multiple function calls.
 * Each scratch slot provides its own growable arena that is reset between uses,
 * making it ideal for avoiding heap fragmentation and unnecessary allocations.
 *
 * @code
 * #include "arena_scratch.h"
 * #include <stdio.h>
 * #include <string.h>
 *
 * #define MAX_NAME_LEN 64
 *
 * // Simulate a temporary task using scratch memory
 * void process_user_request(t_scratch_arena_pool* scratch_pool, const char* username)
 * {
 *     t_arena* scratch = scratch_acquire(scratch_pool);
 *     if (!scratch)
 *     {
 *         fprintf(stderr, "Failed to acquire scratch arena\n");
 *         return;
 *     }
 *
 *     // Allocate temporary buffer inside scratch arena
 *     char* message = (char*) arena_alloc(scratch, 256);
 *     if (!message)
 *     {
 *         fprintf(stderr, "Failed to allocate message buffer\n");
 *         scratch_release(scratch_pool, scratch);
 *         return;
 *     }
 *
 *     snprintf(message, 256, "Welcome back, %s!", username);
 *     printf("%s\n", message);
 *
 *     // Release scratch slot for reuse
 *     scratch_release(scratch_pool, scratch);
 * }
 *
 * int main(void)
 * {
 *     t_scratch_arena_pool scratch_pool;
 *
 *     // Create a thread-safe pool with 8 slots of 4KB each
 *     if (!scratch_pool_init(&scratch_pool, 4096, true))
 *     {
 *         fprintf(stderr, "Failed to initialize scratch pool\n");
 *         return 1;
 *     }
 *
 *     // Simulate multiple requests
 *     process_user_request(&scratch_pool, "Toonsa");
 *     process_user_request(&scratch_pool, "Linus Torvalds");
 *     process_user_request(&scratch_pool, "ArenaUser");
 *
 *     scratch_pool_destroy(&scratch_pool);
 *     return 0;
 * }
 * @endcode
 *
 * @example
 * @brief
 * Multithreaded usage of the scratch arena pool API.
 *
 * @details
 * This example demonstrates how a thread-safe scratch arena pool can be used
 * concurrently by multiple threads to allocate and reuse temporary memory.
 *
 * Each thread:
 * - Acquires a scratch slot from the pool.
 * - Allocates a temporary buffer using the arena.
 * - Writes a unique message.
 * - Releases the arena slot.
 *
 * The scratch pool must be initialized with `thread_safe = true` to enable locking.
 *
 * @code
 * #include "arena_scratch.h"
 * #include <pthread.h>
 * #include <stdio.h>
 * #include <stdlib.h>
 * #include <unistd.h>
 *
 * #define THREAD_COUNT 4
 *
 * typedef struct s_thread_args {
 *     t_scratch_arena_pool* pool;
 *     int thread_id;
 * } t_thread_args;
 *
 * void* thread_func(void* arg)
 * {
 *     t_thread_args* args = (t_thread_args*) arg;
 *     t_arena* arena = scratch_acquire(args->pool);
 *     if (!arena)
 *     {
 *         fprintf(stderr, "[Thread %d] Failed to acquire arena\n", args->thread_id);
 *         return NULL;
 *     }
 *
 *     // Use the scratch arena for temporary data
 *     char* buffer = (char*) arena_alloc(arena, 128);
 *     if (!buffer)
 *     {
 *         fprintf(stderr, "[Thread %d] Allocation failed\n", args->thread_id);
 *         scratch_release(args->pool, arena);
 *         return NULL;
 *     }
 *
 *     snprintf(buffer, 128, "[Thread %d] Hello from scratch arena!", args->thread_id);
 *     puts(buffer);
 *
 *     // Simulate work
 *     usleep(100 * 1000); // 100 ms
 *
 *     scratch_release(args->pool, arena);
 *     return NULL;
 * }
 *
 * int main(void)
 * {
 *     t_scratch_arena_pool scratch_pool;
 *     pthread_t threads[THREAD_COUNT];
 *     t_thread_args args[THREAD_COUNT];
 *
 *     if (!scratch_pool_init(&scratch_pool, 2048, true))
 *     {
 *         fprintf(stderr, "Failed to initialize scratch pool\n");
 *         return EXIT_FAILURE;
 *     }
 *
 *     for (int i = 0; i < THREAD_COUNT; ++i)
 *     {
 *         args[i].pool = &scratch_pool;
 *         args[i].thread_id = i;
 *         pthread_create(&threads[i], NULL, thread_func, &args[i]);
 *     }
 *
 *     for (int i = 0; i < THREAD_COUNT; ++i)
 *         pthread_join(threads[i], NULL);
 *
 *     scratch_pool_destroy(&scratch_pool);
 *     return EXIT_SUCCESS;
 * }
 * @endcode
 */

#include "arena_scratch.h"
#include <stdlib.h>
#include <string.h>

/*
 * INTERNAL HELPER DECLARATION
 */

static bool scratch_slot_init(t_scratch_slot* slot, size_t slot_size, int index);

/*
 * PUBLIC API
 */

bool scratch_pool_init(t_scratch_arena_pool* pool, size_t slot_size, bool thread_safe)
{
	if (!pool || slot_size == 0)
		return arena_report_error(NULL, "scratch_pool_init failed: invalid arguments"), false;

	memset(pool, 0, sizeof(*pool));
	pool->slot_size   = slot_size;
	pool->thread_safe = thread_safe;

#ifdef ARENA_ENABLE_THREAD_SAFE
	if (thread_safe)
		pthread_mutex_init(&pool->lock, NULL);
#endif

	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		if (!scratch_slot_init(&pool->slots[i], slot_size, i))
		{
			scratch_pool_destroy(pool);
			return false;
		}
	}
	return true;
}

void scratch_pool_destroy(t_scratch_arena_pool* pool)
{
	if (!pool)
		return;

	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
		arena_destroy(&pool->slots[i].arena);

#ifdef ARENA_ENABLE_THREAD_SAFE
	if (pool->thread_safe)
		pthread_mutex_destroy(&pool->lock);
#endif

	memset(pool, 0, sizeof(*pool));
}

t_arena* scratch_acquire(t_scratch_arena_pool* pool)
{
	if (!pool)
		return arena_report_error(NULL, "scratch_acquire failed: pool is NULL"), NULL;

	SCRATCH_LOCK(pool);
	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		if (!atomic_exchange(&pool->slots[i].in_use, true))
		{
			arena_reset(&pool->slots[i].arena);
			SCRATCH_UNLOCK(pool);
			return &pool->slots[i].arena;
		}
	}
	SCRATCH_UNLOCK(pool);
	return arena_report_error(NULL, "scratch_acquire failed: all slots in use"), NULL;
}

void scratch_release(t_scratch_arena_pool* pool, t_arena* arena)
{
	if (!pool || !arena)
	{
		arena_report_error(NULL, "scratch_release failed: null pool or arena");
		return;
	}

	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		if (&pool->slots[i].arena == arena)
		{
			atomic_store(&pool->slots[i].in_use, false);
			return;
		}
	}

	arena_report_error(arena, "scratch_release failed: arena not found in pool");
}

/*
 * INTERNAL HELPER
 */

/**
 * @brief
 * Initialize a single scratch slot with its own arena.
 *
 * @details
 * This internal helper sets up a `t_scratch_slot` by initializing the internal
 * arena with the given `slot_size`. The arena is marked as growable, and the
 * slot is initially marked as not in use.
 *
 * On failure, an error is reported using `arena_report_error()`, indicating
 * the index of the slot that failed to initialize.
 *
 * @param slot       Pointer to the scratch slot to initialize.
 * @param slot_size  Size of the arena to allocate for the slot.
 * @param index      Index of the slot (used for debugging/logging).
 *
 * @return `true` if the slot was successfully initialized, `false` otherwise.
 *
 * @ingroup arena_scratch_internal
 *
 * @note
 * This function is called during `scratch_pool_init()` to initialize
 * each slot in the scratch pool.
 *
 * @see scratch_pool_init
 * @see arena_init
 */
static bool scratch_slot_init(t_scratch_slot* slot, size_t slot_size, int index)
{
	if (!arena_init(&slot->arena, slot_size, true))
	{
		arena_report_error(NULL, "scratch_slot_init failed: arena_init failed for slot %d", index);
		return false;
	}
	atomic_store(&slot->in_use, false);
	return true;
}
