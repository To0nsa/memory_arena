/**
 * @file arena_state.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Arena state inspection and control utilities.
 *
 * @details
 * This module provides utility functions for inspecting and manipulating the
 * state of a memory arena without modifying its underlying buffer directly.
 * These functions support read-only queries (e.g., used, remaining, peak usage),
 * as well as efficient bulk memory rollback via markers (`arena_mark()` and
 * `arena_pop()`) and full arena reset.
 *
 * Features:
 * - Query memory usage (`arena_used`, `arena_remaining`, `arena_peak`).
 * - Save and restore allocation state using `arena_mark()` and `arena_pop()`.
 * - Fully reset the arena for reuse using `arena_reset()`.
 *
 * These utilities are thread-safe and lock the arena internally before
 * accessing or modifying its state.
 *
 * @note
 * These functions are designed for convenience and performance in scenarios like:
 * - Frame-based memory reuse (e.g., in games or simulations).
 * - Temporary scopes for allocation and rollback.
 * - Profiling and diagnostics (e.g., monitoring peak usage).
 *
 * @ingroup arena_state
 */

#include "arena.h"

size_t arena_used(t_arena* arena)
{
	if (!arena)
		return 0;

	ARENA_LOCK(arena);
	size_t used = arena->offset;
	ARENA_UNLOCK(arena);
	return used;
}

size_t arena_remaining(t_arena* arena)
{
	if (!arena)
		return 0;

	ARENA_LOCK(arena);
	size_t remaining = arena->size - arena->offset;
	ARENA_UNLOCK(arena);
	return remaining;
}

size_t arena_peak(t_arena* arena)
{
	if (!arena)
		return 0;

	ARENA_LOCK(arena);
	size_t peak = arena->stats.peak_usage;
	ARENA_UNLOCK(arena);
	return peak;
}

t_arena_marker arena_mark(t_arena* arena)
{
	if (!arena)
		return 0;

	ARENA_LOCK(arena);
	size_t offset = arena->offset;
	ARENA_UNLOCK(arena);
	return offset;
}

void arena_pop(t_arena* arena, t_arena_marker marker)
{
	if (!arena)
		return;

	ARENA_LOCK(arena);
	if (marker > arena->offset)
	{
		arena_report_error(arena, "arena_pop failed: invalid marker %zu (offset: %zu)", marker, arena->offset);
		ARENA_UNLOCK(arena);
		return;
	}

	arena_poison_memory(arena->buffer + marker, arena->offset - marker);
	arena->offset = marker;
	ARENA_UNLOCK(arena);
}

void arena_reset(t_arena* arena)
{
	if (!arena)
		return;

	ARENA_LOCK(arena);
	ARENA_ASSERT_VALID(arena);

	arena_poison_memory(arena->buffer, arena->size);
	arena->offset = 0;

	ARENA_UNLOCK(arena);
}
