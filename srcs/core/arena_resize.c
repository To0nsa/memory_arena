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