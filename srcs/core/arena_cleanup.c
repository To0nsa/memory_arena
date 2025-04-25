/**
 * @file arena_cleanup.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Cleanup and teardown routines for memory arenas.
 *
 * @details
 * This file provides functions to safely destroy and deallocate arena resources,
 * including internal memory buffers, metadata like growth history, and synchronization
 * primitives. It ensures proper cleanup whether the arena is heap-allocated or stack-allocated.
 *
 * API functions included:
 * - Thread-safe destruction (`arena_destroy`)
 * - Full arena deletion (`arena_delete`)
 *
 * These functions are used internally and publicly to manage arena lifecycles
 * and to ensure memory safety, thread-safety, and diagnostic cleanliness.
 *
 * @ingroup arena_cleanup
 */

#include "arena.h"

/*
 * INTERNAL FUNCTION DECLARATIONS
 */
static inline void arena_free_buffer_if_owned(t_arena* arena);
static inline void arena_free_growth_history(t_arena* arena);

/*
 * PUBLIC API
 */

void arena_destroy(t_arena* arena)
{
	if (!arena)
		return;

	bool expected = false;
	if (!atomic_compare_exchange_strong(&arena->is_destroying, &expected, true))
		return;

#ifdef ARENA_ENABLE_THREAD_SAFE
	if (arena->use_lock)
	{
		ARENA_CHECK(arena);
		ARENA_LOCK(arena);

		arena_free_buffer_if_owned(arena);
		arena_free_growth_history(arena);

		arena_zero_metadata(arena);

		ARENA_UNLOCK(arena);
		arena->use_lock = false;
		pthread_mutex_destroy(&arena->lock);
		return;
	}
#endif

	arena_free_buffer_if_owned(arena);
	arena_free_growth_history(arena);
	arena_zero_metadata(arena);
}

void arena_delete(t_arena** arena)
{
	if (!arena || !*arena)
		return;

	arena_destroy(*arena);
	free(*arena);
	*arena = NULL;
}

/*
 * INTERNAL HELPERS
 */

/**
 * @brief
 * Free the arena's internal buffer if the arena owns it.
 *
 * @details
 * This internal helper checks the `owns_buffer` atomic flag to determine whether
 * the arena is responsible for freeing its memory buffer. If ownership is confirmed
 * and the buffer is non-null, the function:
 *
 * - Applies optional memory poisoning via `arena_poison_memory()` for debugging.
 * - Frees the memory buffer using `free()`.
 * - Clears the buffer pointer.
 * - Resets the `owns_buffer` flag atomically to prevent double-free.
 *
 * This function is typically used during cleanup routines such as `arena_destroy()`
 * to safely deallocate memory only if it was dynamically allocated by the arena itself.
 *
 * @param arena Pointer to the arena whose buffer should be conditionally freed.
 *
 * @ingroup arena_cleanup_internal
 *
 * @see arena_destroy
 * @see arena_set_allocated_buffer
 */
static inline void arena_free_buffer_if_owned(t_arena* arena)
{
	bool owns = atomic_load_explicit(&arena->owns_buffer, memory_order_acquire);
	if (owns && arena->buffer)
	{
		arena_poison_memory(arena->buffer, arena->size);
		free(arena->buffer);
		arena->buffer = NULL;
		atomic_store_explicit(&arena->owns_buffer, false, memory_order_release);
	}
}

/**
 * @brief
 * Free the memory used to store the arena's growth history.
 *
 * @details
 * This internal helper checks whether the arena has recorded any
 * growth history via the `growth_history` pointer in its stats.
 * If so, it:
 *
 * - Optionally poisons the memory for debugging (if enabled).
 * - Frees the `growth_history` array.
 * - Sets the pointer to `NULL` to prevent dangling access.
 *
 * This function is called during teardown (e.g., in `arena_destroy`)
 * to release dynamically allocated metadata associated with growth tracking.
 *
 * @param arena Pointer to the arena whose growth history should be freed.
 *
 * @ingroup arena_cleanup_internal
 *
 * @see arena_stats_record_growth
 * @see arena_destroy
 */
static inline void arena_free_growth_history(t_arena* arena)
{
	if (arena->stats.growth_history)
	{
		arena_poison_memory(arena->stats.growth_history, sizeof(size_t) * arena->stats.growth_history_count);
		free(arena->stats.growth_history);
		arena->stats.growth_history = NULL;
	}
}
