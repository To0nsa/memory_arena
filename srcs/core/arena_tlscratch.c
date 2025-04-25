/**
 * @file arena_tlsscratch.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Thread-local scratch arena management for fast temporary memory allocations.
 *
 * @details
 * This module provides a per-thread memory arena (`t_arena`) for efficient,
 * short-lived memory allocations that do not require synchronization.
 *
 * Each thread gets its own scratch arena, which is:
 * - Initialized on first access via `get_thread_scratch_arena()`.
 * - Automatically reset to a clean state each time it is retrieved.
 * - Optionally destroyed using `destroy_thread_scratch_arena()`.

 * Features:
 * - No locking or contention between threads.
 * - Predictable lifecycle: initialization, usage, reset, optional destruction.
 * - Configurable size before initialization via `set_thread_scratch_arena_size()`.
 * - Low overhead for frequent temporary allocations (e.g., in render loops or parsers).

 * @par When and Why to Use
 * Use thread-local scratch arenas when you need fast, isolated memory for temporary
 * operations that do not outlive a function call, frame, or short-lived scope. They are
 * ideal in multithreaded programs where:
 * - You want to avoid `malloc`/`free` overhead on hot paths.
 * - You need temporary buffers unique to each thread (e.g., rendering jobs, JSON parsing, or I/O staging).
 * - You want reusable memory that doesn't require manual deallocation.
 *
 * Unlike scratch arena pools (shared) or sub-arenas (structured hierarchies), TLS arenas are zero-contention,
 * private, and reset automatically, making them a top choice for thread-isolated tasks.
 *
 * This file is compiled only if `ARENA_ENABLE_THREAD_LOCAL_SCRATCH` is defined.
 * Otherwise, the access functions are stubbed with appropriate error reporting.
 *
 * @note
 * The thread-local arena is **not** shared between threads. Each thread owns and manages its own instance.
 *
 * @ingroup arena_tlscratch
 *
 * @example
 * @brief
 * Using thread-local scratch arenas for parallel tasks.
 *
 * @details
 * This example demonstrates how to use `get_thread_scratch_arena()` in a
 * multithreaded environment to perform fast, thread-isolated memory allocations.
 * Each thread gets its own `t_arena`, reset automatically between uses.
 * No synchronization is required between threads.
 *
 * This pattern is ideal for:
 * - Multithreaded render pipelines
 * - Parsers and serializers
 * - Job systems that need temporary buffers
 *
 * @code
 * #include "arena.h"
 * #include <pthread.h>
 * #include <stdio.h>
 * #include <string.h>
 * #include <unistd.h>
 *
 * #define NUM_THREADS 4
 *
 * void* thread_func(void* arg)
 * {
 *     int thread_id = *(int*)arg;
 *     t_arena* arena = get_thread_scratch_arena();
 *     if (!arena)
 *     {
 *         fprintf(stderr, "Thread %d: failed to get scratch arena\n", thread_id);
 *         return NULL;
 *     }
 *
 *     // Allocate a temporary string buffer
 *     char* buffer = (char*) arena_alloc(arena, 256);
 *     snprintf(buffer, 256, "Hello from thread %d!", thread_id);
 *
 *     // Simulate work
 *     printf("%s\n", buffer);
 *     usleep(100000); // simulate delay
 *
 *     // Memory is automatically reset after each call to get_thread_scratch_arena()
 *     return NULL;
 * }
 *
 * int main(void)
 * {
 *     pthread_t threads[NUM_THREADS];
 *     int ids[NUM_THREADS];
 *
 *     for (int i = 0; i < NUM_THREADS; ++i)
 *     {
 *         ids[i] = i;
 *         pthread_create(&threads[i], NULL, thread_func, &ids[i]);
 *     }
 *
 *     for (int i = 0; i < NUM_THREADS; ++i)
 *         pthread_join(threads[i], NULL);
 *
 *     return 0;
 * }
 * @endcode
 *
 * @par Expected output
 * Hello from thread 0!
 * Hello from thread 1!
 * Hello from thread 2!
 * Hello from thread 3!
 */

#include "arena.h"

#if ARENA_ENABLE_THREAD_LOCAL_SCRATCH

/**
 * @brief
 * Thread-local scratch arena and its metadata.
 *
 * @details
 * These variables define a thread-local scratch arena that can be used
 * for fast, isolated memory allocations without synchronization overhead.
 *
 * Each thread gets its own private `t_arena` instance, which is initialized
 * on first access and reset on every call to `get_thread_scratch_arena()`.
 *
 * - `thread_scratch_arena`: The actual memory arena used by the thread.
 * - `thread_scratch_initialized`: Boolean flag indicating whether the arena
 *   has been initialized for the current thread.
 * - `thread_scratch_size`: Initial size used to allocate the scratch arena
 *   when it is first initialized. Can be overridden before first use.
 *
 * These variables are defined as `static _Thread_local` to ensure that each
 * translation unit sees exactly one instance per thread.
 *
 * @ingroup arena_tlscratch
 *
 * @note
 * These variables should remain in the `.c` file and not be declared in a header.
 * Use accessors such as `get_thread_scratch_arena()` to interact with them safely.
 */
_Thread_local static t_arena thread_scratch_arena;
_Thread_local static bool    thread_scratch_initialized = false;
_Thread_local static size_t  thread_scratch_size        = 8192;

void set_thread_scratch_arena_size(size_t size)
{
	if (!thread_scratch_initialized)
		thread_scratch_size = size;
}

t_arena* get_thread_scratch_arena_ref(void)
{
	return &thread_scratch_arena;
}

t_arena* get_thread_scratch_arena(void)
{
	if (!thread_scratch_initialized)
	{
		arena_init(&thread_scratch_arena, 8192, true);
		thread_scratch_initialized = true;
	}
	arena_reset(&thread_scratch_arena);
	return &thread_scratch_arena;
}

void destroy_thread_scratch_arena(void)
{
	if (thread_scratch_initialized)
	{
		arena_destroy(&thread_scratch_arena);
		thread_scratch_initialized = false;
	}
}

#else

t_arena* get_thread_scratch_arena(void)
{
	arena_report_error(NULL, "get_thread_scratch_arena() called but ARENA_ENABLE_THREAD_LOCAL_SCRATCH is disabled");
	return NULL;
}

void destroy_thread_scratch_arena(void)
{
	arena_report_error(NULL, "destroy_thread_scratch_arena() called but ARENA_ENABLE_THREAD_LOCAL_SCRATCH is disabled");
}

void set_thread_scratch_arena_size(size_t size)
{
	(void)size;
	arena_report_error(NULL, "set_thread_scratch_arena_size() called but ARENA_ENABLE_THREAD_LOCAL_SCRATCH is "
	                         "disabled");
}

t_arena* get_thread_scratch_arena_ref(void)
{
	arena_report_error(NULL, "get_thread_scratch_arena_ref() called but ARENA_ENABLE_THREAD_LOCAL_SCRATCH is disabled");
	return NULL;
}

#endif
