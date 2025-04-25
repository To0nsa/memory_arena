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

/**
 * @brief
 * Allocate and zero-initialize memory in the arena using default alignment.
 *
 * @details
 * This function allocates a block of memory large enough to hold `count * size` bytes,
 * and initializes all bytes to zero. It behaves similarly to `calloc()` in standard C.
 *
 * It uses the default alignment and passes the label `"arena_calloc_zero"` to ensure
 * the memory is zero-initialized internally.
 *
 * Use this when:
 * - You want to allocate zeroed memory.
 * - You don’t need to specify alignment or a custom label.
 *
 * @param arena Pointer to the `t_arena` to allocate from.
 * @param count Number of elements.
 * @param size  Size of each element in bytes.
 *
 * @return Pointer to zero-initialized memory, or `NULL` on failure.
 *
 * @ingroup arena_calloc
 *
 * @see arena_calloc_labeled
 * @see arena_alloc
 *
 * @example
 * @code
 * // Example: Allocate and zero-initialize memory for 128 structures
 * typedef struct {
 *     int id;
 *     float value;
 * } MyData;
 *
 * t_arena arena;
 * arena_init(&arena, 4096, false);
 *
 * MyData* buffer = (MyData*) arena_calloc(&arena, 128, sizeof(MyData));
 * if (!buffer) {
 *     // Handle allocation failure
 * }
 *
 * // All fields are guaranteed to be zero
 * for (size_t i = 0; i < 128; ++i) {
 *     assert(buffer[i].id == 0);
 *     assert(buffer[i].value == 0.0f);
 * }
 *
 * arena_destroy(&arena);
 * @endcode
 */
void* arena_calloc(t_arena* arena, size_t count, size_t size)
{
	return arena_calloc_labeled(arena, count, size, "arena_calloc_zero");
}

/**
 * @brief
 * Allocate zero-initialized memory from the arena with a specified alignment.
 *
 * @details
 * This function allocates a block of memory large enough to hold `count` elements
 * of `size` bytes each, ensuring that the memory is aligned to `alignment` bytes.
 * The memory is zero-initialized, and the allocation is tracked under the label
 * `"arena_calloc_zero"`.
 *
 * Internally, this delegates to `arena_calloc_aligned_labeled()` with a default label.
 *
 * @param arena     Pointer to the arena from which to allocate memory.
 * @param count     Number of elements to allocate.
 * @param size      Size of each element, in bytes.
 * @param alignment Desired memory alignment (must be power of two).
 *
 * @return Pointer to aligned, zero-initialized memory, or `NULL` on failure.
 *
 * @ingroup arena_alloc
 *
 * @note
 * If `count * size` causes an overflow, the allocation will fail.
 *
 * @see arena_calloc
 * @see arena_calloc_labeled
 * @see arena_calloc_aligned_labeled
 *
 * @example
 * @code
 * // Example: Allocate a 64-byte aligned array of 100 floats
 * t_arena arena;
 * arena_init(&arena, 8192, false);
 *
 * float* data = (float*) arena_calloc_aligned(&arena, 100, sizeof(float), 64);
 * if (!data) {
 *     // Handle allocation failure
 * }
 *
 * // All 100 floats are zero-initialized
 * for (size_t i = 0; i < 100; ++i) {
 *     assert(data[i] == 0.0f);
 * }
 *
 * arena_destroy(&arena);
 * @endcode
 */
void* arena_calloc_aligned(t_arena* arena, size_t count, size_t size, size_t alignment)
{
	return arena_calloc_aligned_labeled(arena, count, size, alignment, "arena_calloc_zero");
}

/**
 * @brief
 * Allocate zero-initialized memory from the arena with a custom label.
 *
 * @details
 * This function allocates a block of memory large enough to hold `count` elements
 * of `size` bytes each. The memory is zero-initialized, and the allocation is tagged
 * with a custom `label` for tracking and debugging.
 *
 * Internally, this delegates to `arena_calloc_aligned_labeled()` with the default alignment.
 *
 * @param arena Pointer to the arena from which to allocate memory.
 * @param count Number of elements to allocate.
 * @param size  Size of each element, in bytes.
 * @param label Optional label for identifying the allocation. Can be `NULL`.
 *
 * @return Pointer to zero-initialized memory, or `NULL` on failure.
 *
 * @ingroup arena_alloc
 *
 * @note
 * If `count * size` causes an overflow, the allocation will fail.
 *
 * @see arena_calloc
 * @see arena_calloc_aligned
 * @see arena_calloc_aligned_labeled
 *
 * @example
 * @code
 * // Example: Allocate zeroed memory with a custom label for debugging
 * typedef struct {
 *     int id;
 *     float weight;
 * } Entity;
 *
 * t_arena arena;
 * arena_init(&arena, 8192, false);
 *
 * Entity* entities = (Entity*) arena_calloc_labeled(&arena, 256, sizeof(Entity), "game_entities");
 * if (!entities) {
 *     // Handle allocation failure
 * }
 *
 * // Zero-initialized: safe to access fields
 * assert(entities[0].id == 0);
 * assert(entities[0].weight == 0.0f);
 *
 * arena_destroy(&arena);
 * @endcode
 */
void* arena_calloc_labeled(t_arena* arena, size_t count, size_t size, const char* label)
{
	return arena_calloc_aligned_labeled(arena, count, size, ARENA_DEFAULT_ALIGNMENT, label);
}

/**
 * @brief
 * Allocate zero-initialized memory from the arena with custom alignment and label.
 *
 * @details
 * This function allocates a memory block large enough to hold `count * size` bytes,
 * ensures the returned pointer satisfies the specified `alignment`, and tags the
 * allocation with a custom `label` for tracking or debugging purposes.
 *
 * It validates input parameters and checks for multiplication overflow before
 * delegating the actual allocation to `arena_alloc_internal()` with the special label
 * `"arena_calloc_zero"`, which triggers zero-initialization in the allocator.
 *
 * @param arena     Pointer to the arena from which to allocate memory.
 * @param count     Number of elements.
 * @param size      Size of each element in bytes.
 * @param alignment Required memory alignment in bytes (must be a power of two).
 * @param label     Optional label string to associate with the allocation. Can be `NULL`.
 *
 * @return Pointer to the allocated and zero-initialized memory block, or `NULL` on failure.
 *
 * @ingroup arena_alloc
 *
 * @note
 * If the product `count * size` overflows `size_t`, the function returns `NULL`.
 *
 * @see arena_calloc
 * @see arena_calloc_aligned
 * @see arena_calloc_labeled
 * @see arena_alloc_internal
 *
 * @example
 * @code
 * // Example: Allocate aligned and zero-initialized memory with a custom label
 * #include <stdalign.h> // For alignof
 *
 * typedef struct {
 *     double x, y, z;
 * } Vector3D;
 *
 * t_arena arena;
 * arena_init(&arena, 16384, true);
 *
 * // Allocate 100 Vector3D structs, aligned to 64 bytes, with a debug label
 * Vector3D* vectors = (Vector3D*) arena_calloc_aligned_labeled(
 *     &arena, 100, sizeof(Vector3D), 64, "physics_vectors"
 * );
 *
 * if (!vectors) {
 *     // Handle allocation failure
 * }
 *
 * // All memory is zero-initialized
 * assert(vectors[0].x == 0.0);
 * assert(((uintptr_t)vectors % 64) == 0); // Check alignment
 *
 * arena_destroy(&arena);
 * @endcode
 */
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