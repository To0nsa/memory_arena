/**
 * @file arena_resize.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Arena memory resizing logic — dynamic growth and shrinking.
 *
 * @details
 * This file implements the dynamic memory resizing features of the arena allocator,
 * allowing it to grow when allocations exceed current capacity, and shrink when
 * memory usage drops significantly.
 *
 * Features covered in this file:
 * - Manual growth (`arena_grow`) based on requested size.
 * - Automatic resizing policy through user-defined or default callbacks.
 * - Shrinking (`arena_shrink`) of underused memory regions.
 * - Heuristic-based auto-shrinking (`arena_might_shrink`) with safe thresholds.
 * - Internal helpers for validation, computation, and buffer reallocation.
 *
 * All operations are thread-safe and acquire internal locks on the arena during
 * mutation of memory size or related metadata.
 *
 * This module is especially useful for long-lived arenas in high-uptime applications,
 * where memory usage patterns may fluctuate and reclaiming unused memory is desirable.
 *
 * @note
 * The resizing logic assumes the arena owns its buffer and is marked as growable.
 * Shrinking operations are designed to be conservative to prevent thrashing.
 *
 * @ingroup arena_resize
 */

#include "arena.h"
#include <math.h>

/*
 * INTERNAL HELPERS DEFINITIONS
 */

static inline bool   arena_should_grow(const t_arena* arena);
static inline void   arena_record_growth(t_arena* arena, size_t old_size);
static inline bool   arena_grow_validate(t_arena* arena, size_t required_size);
static inline size_t arena_grow_compute_new_size(t_arena* arena, size_t required_size);
static inline bool   arena_grow_realloc_buffer(t_arena* arena, size_t new_size, size_t old_size);

static inline bool arena_can_shrink(t_arena* arena, size_t new_size);
static inline bool arena_shrink_validate(t_arena* arena, size_t new_size);
static inline bool arena_shrink_apply(t_arena* arena, size_t new_size);

static inline bool   should_attempt_shrink(t_arena* arena);
static inline bool   arena_should_maybe_shrink(size_t used, size_t size);
static inline size_t arena_shrink_target(size_t used);

/*
 * PUBLIC API
 */

/**
 * @brief
 * Dynamically grow the arena's memory buffer to accommodate more data.
 *
 * @details
 * This function expands the size of the arena’s backing buffer to ensure
 * that at least `required_size` bytes can be allocated on top of the
 * current usage. It follows a growth policy defined by `arena->grow_cb`,
 * or defaults to `default_grow_cb` if none is provided.
 *
 * Growth is allowed only if:
 * - The arena is not `NULL`.
 * - `arena->owns_buffer` and `arena->can_grow` are `true`.
 * - The resulting size does not overflow `SIZE_MAX`.
 *
 * On success:
 * - The arena buffer is reallocated to a larger size.
 * - Internal stats are updated, including the `reallocations` counter and
 *   `growth_history`.
 * - The function returns `true`.
 *
 * On failure:
 * - An appropriate error is reported via `arena_report_error`.
 * - The function returns `false` without modifying the buffer.
 *
 * This function is thread-safe and acquires the arena lock during growth.
 *
 * @param arena          Pointer to the `t_arena` to grow.
 * @param required_size  Additional bytes needed beyond current usage.
 *
 * @return `true` if growth succeeded, `false` otherwise.
 *
 * @ingroup arena_resize
 *
 * @note
 * If `required_size` is `0`, the function returns `true` without making changes.
 *
 * @note
 * This function is normally called internally by `arena_alloc()` or `arena_realloc_last()`.
 * Users may call it directly to reserve memory in advance, but doing so is optional
 * and rarely necessary.
 *
 * @see default_grow_cb
 * @see arena_grow_validate
 * @see arena_grow_compute_new_size
 * @see arena_grow_realloc_buffer
 *
 * @example
 * @code
 * #include "arena.h"
 * #include <stdio.h>
 *
 * int main(void)
 * {
 *     // Create an arena with an initial size of 128 bytes
 *     t_arena* arena = arena_create(128, true);
 *     if (!arena)
 *     {
 *         fprintf(stderr, "Failed to create arena\n");
 *         return 1;
 *     }
 *
 *     // Manually grow the arena to ensure space for an additional 1024 bytes
 *     if (!arena_grow(arena, 1024))
 *     {
 *         fprintf(stderr, "Arena growth failed\n");
 *         arena_delete(&arena);
 *         return 1;
 *     }
 *
 *     printf("Arena successfully grown to %zu bytes\n", arena->size);
 *
 *     // Proceed with allocations as needed
 *     void* ptr = arena_alloc(arena, 512);
 *     if (!ptr)
 *         fprintf(stderr, "Allocation failed\n");
 *
 *     // Cleanup
 *     arena_delete(&arena);
 *     return 0;
 * }
 * @endcode
 */
bool arena_grow(t_arena* arena, size_t required_size)
{
	if (!arena)
		return false;

	if (required_size == 0)
		return true;

	ARENA_LOCK(arena);

	if (!arena_grow_validate(arena, required_size))
	{
		ARENA_UNLOCK(arena);
		return false;
	}

	size_t old_size = arena->size;
	size_t new_size = arena_grow_compute_new_size(arena, required_size);
	if (new_size == 0)
	{
		arena_report_error(arena, "arena_grow failed: computed size invalid");
		ARENA_UNLOCK(arena);
		return false;
	}

	if (new_size > (size_t)(1ULL << 32)) // 4GiB
	{
		arena_report_error(arena, "arena_grow rejected size: %zu", new_size);
		return false;
	}
	bool success = arena_grow_realloc_buffer(arena, new_size, old_size);
	ARENA_UNLOCK(arena);
	return success;
}

/**
 * @brief
 * Shrink the arena’s memory buffer to a smaller size if conditions allow.
 *
 * @details
 * This function attempts to reduce the size of the arena's internal buffer
 * to `new_size` bytes. It performs the following steps:
 *
 * - Acquires a lock for thread safety.
 * - Validates whether shrinking is allowed using `arena_shrink_validate()`.
 * - If valid, applies the shrink via `arena_shrink_apply()`.
 * - Releases the lock regardless of the result.
 *
 * Shrinking is permitted only if:
 * - The arena owns its buffer and growth is enabled.
 * - The new size is not smaller than the current usage (`offset`).
 * - The ratio of new size to old size meets the configured shrink threshold.
 *
 * This function is useful in long-running applications or memory-constrained
 * environments where:
 * - A large arena was temporarily expanded to handle peak load.
 * - That memory is no longer in active use.
 * - You want to proactively release unused memory back to the system.
 *
 * For automatic shrink decisions, consider using `arena_might_shrink()`.
 *
 * @param arena     Pointer to the arena to shrink.
 * @param new_size  Desired size of the buffer after shrinking (in bytes).
 *
 * @return void
 *
 * @ingroup arena_resize
 *
 * @note
 * The arena is only shrunk if all validation conditions are satisfied.
 * Otherwise, the operation is silently ignored.
 *
 * @see arena_can_shrink
 * @see arena_shrink_validate
 * @see arena_shrink_apply
 *
 * @example
 * @code
 * #include "arena.h"
 * #include <stdio.h>
 *
 * // Simulate a large temporary operation (e.g., file parsing)
 * void parse_file_simulation(t_arena* arena)
 * {
 *     // Simulate loading a large file into memory
 *     void* tmp = arena_alloc(arena, 4096);
 *     if (!tmp)
 *     {
 *         fprintf(stderr, "Failed to allocate temporary buffer\n");
 *         return;
 *     }

 *     // ... parse and extract relevant data ...

 *     // After parsing, we only need the first 512 bytes for indexing
 *     arena->offset = 512;  // Keep only what’s needed
 * }
 *
 * int main(void)
 * {
 *     t_arena* arena = arena_create(8192, true);
 *     if (!arena)
 *     {
 *         fprintf(stderr, "Arena initialization failed\n");
 *         return 1;
 *     }

 *     // Temporary workload
 *     parse_file_simulation(arena);

 *     // Shrink the arena to free up unused memory
 *     arena_shrink(arena, 512);

 *     printf("Shrunk arena to %zu bytes after parsing\n", arena->size);

 *     // Continue using the arena efficiently...
 *     void* data = arena_alloc(arena, 128);
 *     if (!data)
 *         fprintf(stderr, "Follow-up allocation failed\n");

 *     arena_delete(&arena);
 *     return 0;
 * }
 * @endcode
 */
void arena_shrink(t_arena* arena, size_t new_size)
{
	if (!arena)
		return;

	ARENA_LOCK(arena);
	ARENA_CHECK(arena);

	bool ok = arena_shrink_validate(arena, new_size) && arena_shrink_apply(arena, new_size);

	(void) ok;
	ARENA_UNLOCK(arena);
}

/**
 * @brief
 * Attempt to shrink the arena's buffer if underutilized.
 *
 * @details
 * This function evaluates whether the current memory usage of the arena
 * is significantly lower than its allocated size. If so, and if the arena
 * allows dynamic resizing, it attempts to shrink the buffer to reduce
 * memory footprint.
 *
 * The shrink target is computed as the current used size plus a padding
 * (`ARENA_SHRINK_PADDING`) to avoid immediate re-expansion.
 *
 * Conditions for shrinking:
 * - `arena != NULL`
 * - `can_grow` is true
 * - The usage ratio (`offset / size`) is below `ARENA_MIN_SHRINK_RATIO`
 * - The computed target size is smaller than the current buffer size
 *
 * This function is thread-safe and acquires the arena lock internally.
 *
 * @param arena Pointer to the arena to check for shrinking opportunity.
 *
 * @ingroup arena_resize
 *
 * @note
 * This is a non-destructive optimization. It has no effect if the arena
 * does not support resizing, is already compact, or if shrinking would
 * not release significant memory.
 *
 * @see arena_shrink
 * @see arena_should_maybe_shrink
 * @see arena_shrink_target
 *
 * @example arena_shrink_monitor.c
 * @brief
 * Example of running a background thread to monitor and shrink an arena.
 *
 * @details
 * This example creates a shared arena that is used for allocations,
 * while a background thread periodically checks whether the arena
 * should be shrunk using `arena_might_shrink()`.
 *
 * This pattern is useful in applications that experience temporary
 * spikes in memory usage but want to reclaim memory afterward
 * without destroying the arena.
 *
 * @code
 * #include "arena.h"
 * #include <pthread.h>
 * #include <stdio.h>
 * #include <unistd.h>
 * #include <stdatomic.h>
 *
 * static atomic_bool keep_running = true;
 *
 * void* shrink_monitor_thread(void* arg)
 * {
 *     t_arena* arena = (t_arena*) arg;
 *
 *     while (atomic_load(&keep_running))
 *     {
 *         arena_might_shrink(arena);
 *         sleep(1); // Check every second
 *     }
 *
 *     return NULL;
 * }
 *
 * int main(void)
 * {
 *     t_arena* arena = arena_create(1024, true); // Growable arena
 *     if (!arena)
 *     {
 *         fprintf(stderr, "Failed to create arena.\n");
 *         return 1;
 *     }
 *
 *     pthread_t thread;
 *     if (pthread_create(&thread, NULL, shrink_monitor_thread, arena) != 0)
 *     {
 *         fprintf(stderr, "Failed to create shrink monitor thread.\n");
 *         arena_delete(&arena);
 *         return 1;
 *     }
 *
 *     // Simulate allocations and deallocations
 *     for (int i = 0; i < 10; ++i)
 *     {
 *         arena_alloc(arena, 1024 * 32); // allocate 32 KB
 *         usleep(200 * 1000);            // wait 200ms
 *         arena_reset(arena);            // simulate freeing all
 *     }
 *
 *     // Stop background thread
 *     atomic_store(&keep_running, false);
 *     pthread_join(thread, NULL);
 *
 *     arena_delete(&arena);
 *     return 0;
 * }
 * @endcode
 */
bool arena_might_shrink(t_arena* arena)
{
	if (!should_attempt_shrink(arena))
		return false;

	ARENA_LOCK(arena);
	ARENA_CHECK(arena);

	const size_t used = arena->offset;
	const size_t size = arena->size;

	if (arena_should_maybe_shrink(used, size))
	{
		const size_t target = arena_shrink_target(used);

		if (target < size)
		{
			arena_shrink(arena, target);
			ARENA_UNLOCK(arena);
			return true;
		}
	}

	ARENA_UNLOCK(arena);
	return false;
}

/*
 * INTERNAL HELPERS IMPLEMENTATION
 */

/**
 * @brief
 * Check whether the arena is allowed to grow.
 *
 * @details
 * This internal helper verifies if the arena is configured to support
 * dynamic growth. It performs the following checks:
 * - The arena must own its memory buffer (`owns_buffer == true`).
 * - The arena must have growth enabled (`can_grow == true`).
 *
 * If both conditions are met, the arena can attempt to grow
 * when additional memory is needed.
 *
 * @param arena Pointer to the `t_arena` structure.
 *
 * @return `true` if the arena can grow, `false` otherwise.
 *
 * @ingroup arena_resize_internal
 *
 * @note
 * This function assumes `arena` is non-NULL. It is called by a function that
 * validates the pointer before calling.
 *
 * @see arena_grow
 */
static inline bool arena_should_grow(const t_arena* arena)
{
	bool owns = atomic_load_explicit(&arena->owns_buffer, memory_order_acquire);
	bool grow = atomic_load_explicit(&arena->can_grow, memory_order_acquire);
	return owns && grow;
}

/**
 * @brief
 * Record the previous size of the arena in the growth history.
 *
 * @details
 * This internal helper appends the `old_size` of the arena to its
 * `growth_history` array. This array keeps track of all prior sizes
 * before each successful call to `arena_grow()`, allowing for optional
 * introspection, debugging, or visualization of growth patterns.
 *
 * It reallocates the history buffer with enough room for the new entry.
 * If `realloc` fails, the history is not updated, but the function fails
 * silently to avoid interrupting allocation logic.
 *
 * @param arena     Pointer to the arena whose growth is being tracked.
 * @param old_size  The size of the arena before a successful growth.
 *
 * @ingroup arena_resize_internal
 *
 * @note
 * This function is called from within
 * a section that already holds the arena lock.
 *
 * @see arena_grow
 */
static inline void arena_record_growth(t_arena* arena, size_t old_size)
{
	size_t* temp = realloc(arena->stats.growth_history, sizeof(size_t) * (arena->stats.growth_history_count + 1));
	if (temp)
	{
		arena->stats.growth_history                                      = temp;
		arena->stats.growth_history[arena->stats.growth_history_count++] = old_size;
	}
}

/**
 * @brief
 * Validate whether the arena is allowed to grow for a given request.
 *
 * @details
 * This internal helper checks if the arena meets the conditions
 * necessary to perform a growth operation. It verifies:
 *
 * - That the arena is marked as growable (`can_grow == true`).
 * - That the arena owns its buffer (`owns_buffer == true`).
 * - That the requested growth does not overflow `size_t` when
 *   added to the current offset.
 *
 * If any condition fails, an error is reported using `arena_report_error()`
 * and the function returns `false`.
 *
 * @param arena          Pointer to the arena to validate.
 * @param required_size  The number of additional bytes needed.
 *
 * @return `true` if the arena is eligible for growth, `false` otherwise.
 *
 * @ingroup arena_resize_internal
 *
 * @note
 * This function is called from within a section where the arena
 * is already locked for thread safety.
 *
 * @see arena_grow
 * @see arena_should_grow
 */
static inline bool arena_grow_validate(t_arena* arena, size_t required_size)
{
	if (!arena_should_grow(arena))
		return arena_report_error(arena, "arena_grow failed: growth not allowed"), false;

	if (required_size > SIZE_MAX - arena->offset)
		return arena_report_error(arena, "arena_grow failed: size overflow"), false;

	return true;
}

/**
 * @brief
 * Compute the new size for the arena based on the growth policy.
 *
 * @details
 * This internal function calculates an appropriate new buffer size
 * that satisfies the `required_size` request, starting from the current
 * size and offset. It uses the user-defined growth callback if set,
 * or defaults to `default_grow_cb` otherwise.
 *
 * The result is validated to ensure it is:
 * - Large enough to satisfy the requested offset + size.
 * - Not exceeding `SIZE_MAX`.
 *
 * If the computed size is invalid (too small or overflowing), the function
 * returns `0` to indicate failure.
 *
 * @param arena          Pointer to the arena requesting growth.
 * @param required_size  The number of additional bytes needed.
 *
 * @return The computed new size in bytes, or `0` if the value is invalid.
 *
 * @ingroup arena_resize_internal
 *
 * @see arena_grow
 * @see arena_grow_validate
 * @see arena_grow_callback
 * @see default_grow_cb
 */
static inline size_t arena_grow_compute_new_size(t_arena* arena, size_t required_size)
{
	arena_grow_callback cb        = arena->grow_cb ? arena->grow_cb : default_grow_cb;
	size_t              requested = arena->offset + required_size;
	size_t              new_size  = cb(arena->size, required_size);

	if (new_size < requested)
		return 0;
	if (new_size > SIZE_MAX)
		return 0;

	return new_size;
}

/**
 * @brief
 * Reallocate the arena's buffer to a new size.
 *
 * @details
 * This internal function performs the actual `realloc()` of the arena's buffer
 * during a grow operation. It attempts to allocate a new buffer of `new_size`
 * bytes, and if successful:
 * - Updates the buffer pointer and size.
 * - Increments the reallocation counter.
 * - Records the previous size in the arena's growth history.
 * - Emits a debug log message indicating the resize.
 *
 * If reallocation fails, an error is reported and `false` is returned.
 *
 * @param arena     Pointer to the arena being resized.
 * @param new_size  Target buffer size after reallocation (in bytes).
 * @param old_size  Original size of the buffer before reallocation.
 *
 * @return `true` if reallocation succeeded, `false` otherwise.
 *
 * @ingroup arena_resize_internal
 *
 * @see arena_grow
 * @see arena_record_growth
 * @see realloc
 */
static inline bool arena_grow_realloc_buffer(t_arena* arena, size_t new_size, size_t old_size)
{
	void* new_buf = realloc(arena->buffer, new_size);
	if (!new_buf)
		return arena_report_error(arena, "arena_grow failed: realloc failed"), false;

	arena->buffer = (uint8_t*) new_buf;
	arena->size   = new_size;
	arena->stats.reallocations++;

	arena_record_growth(arena, old_size);

	ALOG("[arena_grow] Arena %p grown from %zu to %zu bytes\n", (void*) arena, old_size, new_size);
	return true;
}

/**
 * @brief
 * Determine whether the arena is eligible for shrinking to a smaller size.
 *
 * @details
 * This internal helper checks if the arena can be safely shrunk to `new_size`
 * based on ownership, growth policy, and current memory usage.
 *
 * Shrinking is allowed only if:
 * - The arena is not `NULL`.
 * - The arena owns its buffer and is allowed to grow/shrink.
 * - The proposed `new_size` is not smaller than the current offset.
 * - The ratio of `new_size / current_size` is less than or equal to the
 *   configured threshold (`ARENA_MIN_SHRINK_RATIO`), unless it matches the offset.
 *
 * This function is used as a precondition check before applying memory reduction
 * via `arena_shrink()`.
 *
 * @param arena     Pointer to the `t_arena` instance.
 * @param new_size  Proposed size for shrinking the buffer.
 *
 * @return `true` if the arena is eligible to shrink, `false` otherwise.
 *
 * @ingroup arena_resize_internal
 *
 * @note
 * If `new_size == arena->offset`, shrinking is always allowed (tight fit).
 *
 * @see arena_shrink
 */
static inline bool arena_can_shrink(t_arena* arena, size_t new_size)
{
	if (!arena)
		return false;

	bool owns = atomic_load_explicit(&arena->owns_buffer, memory_order_acquire);
	bool grow = atomic_load_explicit(&arena->can_grow, memory_order_acquire);

	if (!owns || !grow)
		return false;

	if (new_size < arena->offset)
		return false;

	if (new_size == arena->offset)
		return true;

	double shrink_ratio = (double) new_size / (double) arena->size;
	return shrink_ratio <= ARENA_MIN_SHRINK_RATIO;
}

/**
 * @brief
 * Validate whether an arena can be safely shrunk to a new size.
 *
 * @details
 * This internal helper wraps the logic of `arena_can_shrink()` with
 * an initial null-check on the arena itself. It is used to guard
 * shrink operations such as `arena_shrink()` and `arena_might_shrink()`.
 *
 * @param arena     Pointer to the arena to validate.
 * @param new_size  Proposed size to shrink the arena to (in bytes).
 *
 * @return `true` if the arena is not `NULL` and is allowed to shrink,
 *         `false` otherwise.
 *
 * @ingroup arena_resize_internal
 *
 * @see arena_can_shrink
 * @see arena_shrink
 */
static inline bool arena_shrink_validate(t_arena* arena, size_t new_size)
{
	if (!arena)
		return false;

	if (!arena_can_shrink(arena, new_size))
		return false;

	return true;
}

/**
 * @brief
 * Apply the memory shrink operation to an arena's buffer.
 *
 * @details
 * This internal function performs the actual memory reallocation
 * to reduce the arena's backing buffer to `new_size` bytes.
 *
 * On success:
 * - The arena's `buffer` pointer and `size` field are updated.
 * - The `shrinks` counter in arena statistics is incremented.
 * - A debug log message is printed.
 *
 * If `realloc()` fails, the arena remains unchanged and the function returns `false`.
 *
 * @param arena     Pointer to the arena to shrink.
 * @param new_size  New desired size of the arena buffer (in bytes).
 *
 * @return `true` on successful shrink, `false` on failure.
 *
 * @ingroup arena_resize_internal
 *
 * @note
 * This function is only called after validating the shrink operation.
 *
 * @see arena_can_shrink
 * @see arena_shrink
 */
static inline bool arena_shrink_apply(t_arena* arena, size_t new_size)
{
	void* new_buf = realloc(arena->buffer, new_size);
	if (!new_buf)
		return false;

	arena->buffer = (uint8_t*) new_buf;
	arena->size   = new_size;
	arena->stats.shrinks++;

	ALOG("[arena_shrink] Arena %p shrunk to %zu bytes\n", (void*) arena, new_size);
	return true;
}

/**
 * @brief
 * Determine whether the arena is eligible for dynamic shrinking.
 *
 * @details
 * This helper checks if the arena is in a state that allows shrinking.
 * Shrinking is only permitted if:
 * - The `arena` pointer is not `NULL`.
 * - The arena has dynamic growth enabled (`can_grow` is `true`).
 *
 * This function is typically used before triggering automatic shrinking logic,
 * such as in `arena_might_shrink()`, to avoid unnecessary operations.
 *
 * @param arena Pointer to the arena to check.
 *
 * @return `true` if the arena can potentially be shrunk, `false` otherwise.
 *
 * @ingroup arena_resize_internal
 *
 * @see arena_might_shrink
 */
static inline bool should_attempt_shrink(t_arena* arena)
{
	if (!arena)
		return false;

	return atomic_load_explicit(&arena->can_grow, memory_order_acquire);
}

/**
 * @brief
 * Decide whether the arena buffer should be shrunk based on usage ratio.
 *
 * @details
 * This helper compares the ratio of used bytes to total allocated buffer size.
 * If the usage ratio falls below `ARENA_MIN_SHRINK_RATIO`, the arena is
 * considered underutilized and may be eligible for shrinking.
 *
 * Typical usage scenarios include:
 * - Periodic memory trimming in long-running programs.
 * - Adaptive memory management after large but short-lived allocations.
 *
 * @param used Number of bytes currently used in the arena.
 * @param size Total capacity of the arena buffer.
 *
 * @return `true` if the usage is low enough to consider shrinking, `false` otherwise.
 *
 * @ingroup arena_resize_internal
 *
 * @note
 * If `size` is 0, this function returns `false` to avoid division-by-zero.
 *
 * @see arena_might_shrink
 */
static inline bool arena_should_maybe_shrink(size_t used, size_t size)
{
	if (size == 0)
		return false;

	double ratio = (double) used / (double) size;
	return ratio < ARENA_MIN_SHRINK_RATIO;
}

/**
 * @brief
 * Compute the target buffer size for a shrink operation.
 *
 * @details
 * This helper calculates a reasonable new size for the arena buffer
 * when shrinking is considered. It adds a small safety margin (`ARENA_SHRINK_PADDING`)
 * to the current usage (`used`) to avoid immediate re-growth if a small allocation
 * occurs after shrinking.
 *
 * The padding ensures that the buffer remains slightly larger than strictly needed,
 * providing better performance and avoiding fragmentation.
 *
 * @param used Number of bytes currently used in the arena.
 *
 * @return Suggested new size for the buffer after shrinking.
 *
 * @ingroup arena_resize_internal
 *
 * @see arena_might_shrink
 * @see arena_shrink
 */
static inline size_t arena_shrink_target(size_t used)
{
	return used + ARENA_SHRINK_PADDING;
}