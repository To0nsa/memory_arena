/**
 * @file arena_calloc.c
 * @author Toonsa
 * @date 2025/04/24
 * 
 * @brief Zero-initialized memory allocation functions for arenas.
 *
 * @details
 * This file implements `calloc`-style memory allocation within a memory arena.
 * It provides functions to allocate memory blocks that are automatically
 * zero-initialized, with optional support for alignment and debug labeling.
 *
 * These functions are analogous to `calloc` in the C standard library, but
 * operate on arenas to enable scoped, fast allocations without direct use
 * of the heap.
 *
 * Supported use cases:
 * - Zero-initialized allocations using default alignment
 * - Aligned zeroed allocations for SIMD or cache alignment
 * - Allocation tracking using human-readable labels
 *
 * Internal helpers are used to validate parameters, detect overflows,
 * and track failed allocation attempts in a thread-safe manner.
 *
 * @note
 * These functions are part of the public allocation API, but depend on
 * internal helpers for safety checks and statistics updates.
 *
 * @ingroup arena_alloc
 */

#include "arena.h"
#include <string.h>

/*
 * INTERNAL FUNCTION DECLARATIONS
 */
static inline bool validate_arena_calloc_input(t_arena* arena, size_t count, size_t size);
static inline void arena_record_failed_alloc(t_arena* arena);

/*
 * PUBLIC API
 */

void* arena_calloc(t_arena* arena, size_t count, size_t size)
{
	return arena_calloc_labeled(arena, count, size, "arena_calloc_zero");
}

void* arena_calloc_aligned(t_arena* arena, size_t count, size_t size, size_t alignment)
{
	return arena_calloc_aligned_labeled(arena, count, size, alignment, "arena_calloc_zero");
}

void* arena_calloc_labeled(t_arena* arena, size_t count, size_t size, const char* label)
{
	return arena_calloc_aligned_labeled(arena, count, size, ARENA_DEFAULT_ALIGNMENT, label);
}

void* arena_calloc_aligned_labeled(t_arena* arena, size_t count, size_t size, size_t alignment, const char* label)
{
	if (!label)
		label = "arena_calloc_zero";

	if (!validate_arena_calloc_input(arena, count, size))
	{
		if (arena)
			arena_record_failed_alloc(arena);
		return NULL;
	}

	size_t total;
	if (would_overflow_mul(count, size, &total))
	{
		arena_record_failed_alloc(arena);
		arena_report_error(arena,
		                   "arena_calloc failed: multiplication overflow "
		                   "(count = %zu, size = %zu)",
		                   count, size);
		return NULL;
	}

	return arena_alloc_internal(arena, total, alignment, label);
}

/*
 * INTERNAL HELPERS
 */

/**
 * @brief
 * Validate input parameters for a calloc-style arena allocation.
 *
 * @details
 * This internal helper verifies that the parameters provided to `arena_calloc`
 * and its variants are valid before attempting memory allocation.
 *
 * Specifically, it ensures:
 * - The `arena` pointer is not `NULL`.
 * - The `count` of elements to allocate is non-zero.
 * - The `size` of each element is non-zero.
 *
 * If any condition fails, the function reports a descriptive error using
 * `arena_report_error` and returns `false`. Otherwise, it returns `true`.
 *
 * @param arena Pointer to the arena being used for allocation.
 * @param count Number of elements to allocate.
 * @param size  Size (in bytes) of each element.
 *
 * @return `true` if all inputs are valid, `false` otherwise.
 *
 * @ingroup arena_calloc_internal
 *
 * @see arena_calloc
 * @see arena_calloc_aligned
 * @see arena_calloc_labeled
 * @see arena_calloc_aligned_labeled
 */
static inline bool validate_arena_calloc_input(t_arena* arena, size_t count, size_t size)
{
	if (!arena)
	{
		arena_report_error(NULL, "arena_calloc failed: NULL arena provided");
		return false;
	}

	if (count == 0)
	{
		arena_report_error(arena, "arena_calloc failed: zero count (count = %zu)", count);
		return false;
	}

	if (size == 0)
	{
		arena_report_error(arena, "arena_calloc failed: zero element size (size = %zu)", size);
		return false;
	}

	return true;
}

/**
 * @brief
 * Increment the arena's failed allocation counter in a thread-safe manner.
 *
 * @details
 * This internal utility increments the `failed_allocations` field in the
 * arena’s statistics structure. It is used to track the number of failed
 * allocation attempts (due to invalid inputs, overflows, or out-of-memory
 * conditions).
 *
 * The function uses locking (via `ARENA_LOCK` / `ARENA_UNLOCK`) to ensure
 * thread safety when statistics are modified, if thread safety is enabled.
 *
 * If the arena pointer is `NULL`, the function exits silently.
 *
 * @param arena Pointer to the `t_arena` whose statistics should be updated.
 *
 * @ingroup arena_calloc_internal
 *
 * @note
 * This function is typically called from within allocation functions that
 * detect errors before allocating memory.
 *
 * @see validate_arena_calloc_input
 * @see arena_calloc_aligned_labeled
 */
static inline void arena_record_failed_alloc(t_arena* arena)
{
	if (!arena)
		return;
	ARENA_LOCK(arena);
	arena->stats.failed_allocations++;
	ARENA_UNLOCK(arena);
}