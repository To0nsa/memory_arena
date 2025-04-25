/**
 * @file arena_realloc.c
 * @author Toonsa
 * @date 2025
 *
 * @brief Reallocation logic for arena memory blocks.
 *
 * @details
 * This file provides the implementation of `arena_realloc_last()`, a function
 * that resizes the most recently allocated memory block within an arena.
 * If the block to reallocate is indeed the last allocation, resizing is
 * attempted in place. If not, a fallback strategy is used: a new block is
 * allocated and data is copied from the old one.
 *
 * Features:
 * - In-place reallocation for the last allocation.
 * - Fallback reallocation with copy for earlier blocks.
 * - Memory poisoning of the original block after fallback.
 * - Statistic updates and debug hooks support.
 *
 * @note
 * `arena_realloc_last()` is optimized for reallocating the most recent
 * allocation. Using it on earlier allocations will allocate a new block and
 * copy the contents. The original block is left poisoned (in debug mode) but
 * remains valid. This can lead to increased memory usage and should be avoided
 * in performance-critical contexts.
 *
 * @warning
 * Only the last allocation can be resized in place. Passing stale or invalid
 * pointers leads to undefined behavior.
 *
 * @ingroup arena_alloc
 */

#include "arena.h"
#include <string.h>

/*
 * INTERNAL HELPERS DECLARATIONS
 */

static inline bool arena_realloc_validate(t_arena* arena, void* old_ptr, size_t new_size);
static inline bool is_last_allocation(t_arena* arena, void* old_ptr, size_t old_size);
static inline void update_realloc_stats(t_arena* arena, void* ptr, size_t new_size, size_t old_size, const char* label);
static inline void* realloc_in_place(t_arena* arena, void* old_ptr, size_t old_size, size_t new_size);
static inline void* realloc_fallback(t_arena* arena, void* old_ptr, size_t old_size, size_t new_size);

/*
 * PUBLIC API
 */

void* arena_realloc_last(t_arena* arena, void* old_ptr, size_t old_size, size_t new_size)
{
	if (!arena_realloc_validate(arena, old_ptr, new_size))
		return NULL;

	ARENA_LOCK(arena);
	ARENA_CHECK(arena);

	bool  is_last = is_last_allocation(arena, old_ptr, old_size);
	void* result  = is_last ? realloc_in_place(arena, old_ptr, old_size, new_size) : NULL;

	ARENA_UNLOCK(arena);

	return is_last ? result : realloc_fallback(arena, old_ptr, old_size, new_size);
}

/*
 * INTERNAL HELPERS
 */

/**
 * @brief
 * Validate input arguments for `arena_realloc_last`.
 *
 * @details
 * This internal helper checks that the inputs to `arena_realloc_last`
 * are valid before proceeding with reallocation logic. It ensures:
 *
 * - The `arena` pointer is not `NULL`.
 * - The `old_ptr` is not `NULL`.
 * - The `new_size` is greater than zero.
 *
 * If any check fails, a descriptive error is reported using `arena_report_error()`.
 *
 * @param arena     Pointer to the arena where the reallocation is performed.
 * @param old_ptr   Pointer to the previously allocated memory.
 * @param new_size  New size requested for the reallocation (in bytes).
 *
 * @return `true` if inputs are valid, `false` otherwise.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_realloc_last
 * @see arena_report_error
 */
static inline bool arena_realloc_validate(t_arena* arena, void* old_ptr, size_t new_size)
{
	if (!arena)
		return arena_report_error(NULL, "arena_realloc_last failed: NULL arena"), false;
	if (!old_ptr)
		return arena_report_error(arena, "arena_realloc_last failed: NULL old_ptr"), false;
	if (new_size == 0)
		return arena_report_error(arena, "arena_realloc_last failed: zero-size reallocation"), false;
	return true;
}

/**
 * @brief
 * Check if a given pointer corresponds to the most recent allocation.
 *
 * @details
 * This helper determines whether `old_ptr` is the last allocation made
 * by the arena. It does so by comparing the pointer to the expected
 * end of the buffer based on `arena->offset` and the known `old_size`.
 *
 * This check is necessary before attempting an in-place reallocation,
 * as only the last allocation can be safely resized in place.
 *
 * @param arena     Pointer to the arena.
 * @param old_ptr   Pointer to the memory block to check.
 * @param old_size  Size of the previously allocated memory block.
 *
 * @return `true` if `old_ptr` is the last allocation, `false` otherwise.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_realloc_last
 * @see realloc_in_place
 */
static inline bool is_last_allocation(t_arena* arena, void* old_ptr, size_t old_size)
{
	uint8_t* expected = arena->buffer + (arena->offset - old_size);
	return (uint8_t*) old_ptr == expected;
}

/**
 * @brief
 * Update arena statistics after a successful reallocation.
 *
 * @details
 * This internal helper updates the arena's internal state to reflect
 * a successful in-place reallocation. It performs the following:
 * - Adjusts the `offset` to account for the new allocation size.
 * - Updates peak usage via `arena_update_peak()`.
 * - Increments allocation-related counters.
 * - Records metadata about the last allocation (size, offset).
 * - Triggers the allocation hook if one is registered.
 *
 * This function is used exclusively during in-place reallocation,
 * where the existing memory block remains valid and grows or shrinks.
 *
 * @param arena     Pointer to the arena being updated.
 * @param ptr       Pointer to the reallocated memory block.
 * @param new_size  New size of the allocation (in bytes).
 * @param old_size  Previous size of the allocation (in bytes).
 * @param label     Descriptive label for logging and tracking.
 *
 * @ingroup arena_alloc_internal
 *
 * @see realloc_in_place
 * @see realloc_fallback
 * @see arena_realloc_last
 * @see arena_update_peak
 */
static inline void update_realloc_stats(t_arena* arena, void* ptr, size_t new_size, size_t old_size, const char* label)
{
	arena->offset = (uint8_t*) ptr + new_size - arena->buffer;
	arena_update_peak(arena);

	arena->stats.reallocations++;
	arena->stats.live_allocations++;
	arena->stats.bytes_allocated += (new_size - old_size);
	arena->stats.last_alloc_size   = new_size;
	arena->stats.last_alloc_offset = (size_t) ((uint8_t*) ptr - arena->buffer);
	arena->stats.alloc_id_counter++;

	if (arena->hooks.hook_cb)
		arena->hooks.hook_cb(arena, (int) arena->stats.alloc_id_counter, ptr, new_size, arena->stats.last_alloc_offset,
		                     0, label);
}

/**
 * @brief
 * Attempt to resize the last allocation in place.
 *
 * @details
 * This internal function resizes the most recent allocation made by the arena,
 * assuming that `old_ptr` points to the last allocated block.
 *
 * If the new size extends beyond the current buffer capacity, it attempts to grow
 * the arena using `arena_grow()`. If growth fails, the function reports an error
 * and returns `NULL`.
 *
 * If the new size is smaller than the original, the unused tail of the allocation
 * is poisoned to help catch accidental use of stale memory in debug mode.
 *
 * The function updates internal statistics and triggers the allocation hook with
 * the `"arena_realloc_last (in-place)"` label to reflect the successful resize.
 *
 * @param arena     Pointer to the arena managing the memory.
 * @param old_ptr   Pointer to the memory block being reallocated.
 * @param old_size  Size of the current allocation in bytes.
 * @param new_size  Desired new size in bytes.
 *
 * @return `old_ptr` if the resize succeeded in place, or `NULL` on failure.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_grow
 * @see update_realloc_stats
 * @see arena_realloc_last
 */
static inline void* realloc_in_place(t_arena* arena, void* old_ptr, size_t old_size, size_t new_size)
{
	size_t new_end = arena->offset - old_size + new_size;

	if (new_end > arena->size && !arena_grow(arena, new_size - old_size))
		return arena_report_error(arena, "arena_realloc_last failed: growth failed (needed %zu bytes)",
		                          new_size - old_size),
		       NULL;

	if (new_size < old_size)
		arena_poison_memory((uint8_t*) old_ptr + new_size, old_size - new_size);

	update_realloc_stats(arena, old_ptr, new_size, old_size, "arena_realloc_last (in-place)");
	return old_ptr;
}

/**
 * @brief
 * Perform a fallback reallocation by allocating new memory and copying data.
 *
 * @details
 * This internal function is used when `arena_realloc_last()` cannot perform
 * an in-place reallocation (e.g., when the old pointer is not the last allocation).
 *
 * It performs the following steps:
 * - Allocates a new memory block of `new_size` bytes.
 * - Copies `min(old_size, new_size)` bytes from the old block to the new block.
 * - Poisons the old memory region for debugging purposes.
 * - Updates internal reallocation statistics and invokes the allocation hook, if set.
 *
 * This approach ensures consistent tracking of memory usage even when in-place
 * reallocation is not possible.
 *
 * @param arena     Pointer to the arena from which to allocate new memory.
 * @param old_ptr   Pointer to the original memory block.
 * @param old_size  Size of the original allocation.
 * @param new_size  Desired size of the new allocation.
 *
 * @return Pointer to the newly allocated memory block, or `NULL` on failure.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_realloc_last
 * @see realloc_in_place
 * @see update_realloc_stats
 */
static inline void* realloc_fallback(t_arena* arena, void* old_ptr, size_t old_size, size_t new_size)
{
	void* new_ptr = arena_alloc(arena, new_size);
	if (!new_ptr)
		return NULL;

	memcpy(new_ptr, old_ptr, old_size < new_size ? old_size : new_size);

	ARENA_LOCK(arena);
	arena_poison_memory(old_ptr, old_size);
	ARENA_UNLOCK(arena);

	update_realloc_stats(arena, new_ptr, new_size, old_size, "arena_realloc_last (fallback)");
	return new_ptr;
}