/**
 * @file arena_alloc.c
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Public memory allocation functions for the arena allocator.
 *
 * @details
 * This file provides the main allocation interface for the arena memory system.
 * It defines four public functions that allow clients to allocate memory from an
 * arena with varying degrees of control over alignment and labeling:
 *
 * - `arena_alloc()`: Simple allocation with default alignment.
 * - `arena_alloc_aligned()`: Allocation with a custom alignment.
 * - `arena_alloc_labeled()`: Allocation with a debug label for tracking.
 * - `arena_alloc_aligned_labeled()`: Full control over alignment and labeling.
 *
 * Internally, all of these functions delegate to `arena_alloc_internal()`, which
 * handles alignment, capacity checks, thread safety, debug labeling, and allocation
 * hooks.
 *
 * These public functions are designed to be safe, efficient, and flexible, and
 * are the main entry points for memory allocation from a `t_arena` instance.
 *
 * @ingroup arena_alloc
 *
 * @see arena_alloc
 * @see arena_alloc_aligned
 * @see arena_alloc_labeled
 * @see arena_alloc_aligned_labeled
 * @see arena_alloc_internal
 * @see arena.h
 */

#include "arena.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * INTERNAL FUNCTION DECLARATIONS
 */
static inline bool   arena_alloc_validate_input(t_arena* arena, size_t size, size_t alignment, const char* label);
static inline bool   arena_is_being_destroyed(t_arena* arena, const char* label);
static inline bool   arena_check_overflow(t_arena* arena, size_t size, const char* label);
static inline size_t arena_calc_aligned_offset(t_arena* arena, size_t alignment);
static inline bool   arena_try_grow(t_arena* arena, size_t size, const char* label);
static inline bool   arena_ensure_capacity(t_arena* arena, size_t size, size_t alignment, const char* label,
                                           size_t* aligned_offset, size_t* wasted);
static inline void   arena_update_stats(t_arena* arena, size_t size, size_t wasted);
static inline void   arena_commit_allocation(t_arena* arena, size_t size, size_t wasted, size_t aligned_offset);
static inline void   arena_zero_if_needed(void* ptr, size_t size, const char* label);
static inline void   arena_invoke_allocation_hook(t_arena* arena, void* ptr, size_t size, size_t offset, size_t wasted,
                                                  const char* label);
void*                arena_alloc_internal(t_arena* arena, size_t size, size_t alignment, const char* label);

/*
 * Public API
 */

/**
 * @brief
 * Allocate a block of memory from the arena with default alignment.
 *
 * @details
 * This function allocates `size` bytes of memory from the given `arena` using
 * the default alignment (`ARENA_DEFAULT_ALIGNMENT`). It delegates to the internal
 * `arena_alloc_internal()` and uses the label `"arena_alloc"` for tracking and
 * debugging purposes.
 *
 * Use this when:
 * - You want a simple allocation without needing to specify alignment or label.
 * - You’re working with data that doesn’t require special alignment (e.g., byte arrays).
 *
 * On success, a pointer to the allocated memory block is returned. On failure,
 * `NULL` is returned and an error is reported via the arena’s error callback.
 *
 * @param arena Pointer to the `t_arena` from which to allocate memory.
 * @param size  Number of bytes to allocate.
 *
 * @return Pointer to the allocated memory block, or `NULL` on failure.
 *
 * @ingroup arena_alloc
 *
 * @see arena_alloc_internal
 * @see arena_alloc_aligned
 * @see arena_alloc_labeled
 *
 * @example
 * @code
 * // Example: Allocate a raw buffer for general-purpose use
 * #include <string.h>
 *
 * t_arena arena;
 * arena_init(&arena, 8192, false);
 *
 * // Allocate a 256-byte buffer
 * void* buffer = arena_alloc(&arena, 256);
 *
 * if (!buffer) {
 *     // Handle allocation failure
 * }
 *
 * // Use the buffer (e.g., zero it)
 * memset(buffer, 0, 256);
 *
 * arena_destroy(&arena);
 * @endcode
 *
 */
void* arena_alloc(t_arena* arena, size_t size)
{
	return arena_alloc_internal(arena, size, ARENA_DEFAULT_ALIGNMENT, "arena_alloc");
}

/**
 * @brief
 * Allocate a block of memory from the arena with a specified alignment.
 *
 * @details
 * This function allocates `size` bytes from the arena using the provided `alignment`.
 * It is useful when allocating data structures that require specific memory alignment
 * (e.g., SIMD types, hardware buffers).
 *
 * Internally, this delegates to `arena_alloc_internal()` and uses the label
 * `"arena_alloc_aligned"` for debugging and tracking.
 *
 * On failure, `NULL` is returned and the error is reported through the arena's
 * error reporting mechanism.
 *
 * @param arena     Pointer to the `t_arena` from which to allocate memory.
 * @param size      Number of bytes to allocate.
 * @param alignment Required alignment (must be a power of two).
 *
 * @return Pointer to aligned memory block, or `NULL` on failure.
 *
 * @ingroup arena_alloc
 *
 * @note
 * Alignment must be a power of two. Use `arena_alloc()` if default alignment is sufficient.
 *
 * @see arena_alloc
 * @see arena_alloc_internal
 * @see arena_alloc_aligned_labeled
 *
 * @example
 * @code
 * // Example: Allocate memory aligned for SIMD operations
 * #include <immintrin.h> // For __m128 or AVX types (if using SIMD)
 *
 * t_arena arena;
 * arena_init(&arena, 4096, false);
 *
 * // Allocate 128 bytes aligned to 32-byte boundary for AVX usage
 * void* simd_buffer = arena_alloc_aligned(&arena, 128, 32);
 * if (!simd_buffer) {
 *     // Handle allocation failure
 * }
 *
 * // Cast to __m256* if needed (e.g., for AVX vector operations)
 * __m256* vectors = (__m256*)simd_buffer;
 *
 * arena_destroy(&arena);
 * @endcode
 */
void* arena_alloc_aligned(t_arena* arena, size_t size, size_t alignment)
{
	return arena_alloc_internal(arena, size, alignment, "arena_alloc_aligned");
}

/**
 * @brief
 * Allocate a block of memory from the arena with a custom debug label.
 *
 * @details
 * This function allocates `size` bytes from the arena using the default alignment,
 * and tags the allocation with a custom `label` for debugging or profiling purposes.
 *
 * If `label` is `NULL`, it defaults to `"arena_alloc_labeled"`.
 * The label is passed through to internal logging and tracking hooks.
 *
 * This function is useful when you want to track or categorize allocations
 * based on usage context (e.g., "sprites", "config", "network_packet").
 *
 * @param arena Pointer to the `t_arena` from which to allocate memory.
 * @param size  Number of bytes to allocate.
 * @param label Optional string label for debugging/tracking. Can be `NULL`.
 *
 * @return Pointer to allocated memory block, or `NULL` on failure.
 *
 * @ingroup arena_alloc
 *
 * @note
 * If you also require alignment control, use `arena_alloc_aligned_labeled()`.
 *
 * @see arena_alloc
 * @see arena_alloc_aligned
 * @see arena_alloc_aligned_labeled
 * @see arena_alloc_internal
 *
 * @example
 * @code
 * // Example: Track separate categories of allocations for profiling
 * t_arena arena;
 * arena_init(&arena, 8192, false);
 *
 * // Allocate memory for a configuration block
 * void* config = arena_alloc_labeled(&arena, 512, "config_data");
 *
 * // Allocate memory for UI layout caching
 * void* ui_cache = arena_alloc_labeled(&arena, 256, "ui_layout_cache");
 *
 * arena_destroy(&arena);
 * @endcode
 */
void* arena_alloc_labeled(t_arena* arena, size_t size, const char* label)
{
	if (!label)
		label = "arena_alloc_labeled";
	return arena_alloc_internal(arena, size, ARENA_DEFAULT_ALIGNMENT, label);
}

/**
 * @brief
 * Allocate a block of memory from the arena with custom alignment and label.
 *
 * @details
 * This function allocates `size` bytes from the arena, ensuring the memory
 * block starts at an address aligned to `alignment`. It also assigns a
 * debug `label` to the allocation for logging or tracking purposes.
 *
 * This is the most flexible allocation function in the arena system:
 * - It allows specifying a custom memory alignment (must be a power of two).
 * - It allows tagging the allocation with a label for debugging or profiling.
 *
 * If `label` is `NULL`, a default label `"arena_alloc_aligned_labeled"` is used.
 *
 * @param arena     Pointer to the `t_arena` from which to allocate memory.
 * @param size      Number of bytes to allocate.
 * @param alignment Required alignment (must be power-of-two).
 * @param label     Optional debug label (can be `NULL`).
 *
 * @return Pointer to aligned, allocated memory, or `NULL` on failure.
 *
 * @ingroup arena_alloc
 *
 * @note
 * For default alignment or simpler usage, see `arena_alloc_labeled()` or `arena_alloc_aligned()`.
 *
 * @see arena_alloc
 * @see arena_alloc_aligned
 * @see arena_alloc_labeled
 * @see arena_alloc_internal
 *
 * @example
 * @code
 * // Example: Allocate aligned memory for a video frame buffer
 * t_arena arena;
 * arena_init(&arena, 16384, true);
 *
 * // Allocate 1024 bytes aligned to 64 bytes for SIMD processing
 * void* frame_buffer = arena_alloc_aligned_labeled(&arena, 1024, 64, "video_frame");
 * if (!frame_buffer) {
 *     // Handle allocation failure
 * }
 *
 * // Use the memory...
 *
 * arena_destroy(&arena);
 * @endcode
 *
 */
void* arena_alloc_aligned_labeled(t_arena* arena, size_t size, size_t alignment, const char* label)
{
	if (!label)
		label = "arena_alloc_aligned_labeled";
	return arena_alloc_internal(arena, size, alignment, label);
}

/*
 * Internal helpers
 */

/**
 * @brief
 * Validate input parameters for arena allocation.
 *
 * @details
 * This internal helper checks that all parameters passed to an arena allocation
 * function are valid before proceeding. It performs the following checks:
 *
 * - The `arena` pointer is not `NULL`.
 * - The requested `size` is greater than zero.
 * - The `alignment` is a non-zero power of two (required for valid memory alignment).
 *
 * If any of these checks fail, an appropriate error is reported using
 * `arena_report_error()` with a descriptive message and the provided label.
 *
 * @param arena     Pointer to the `t_arena` to validate.
 * @param size      Size of the memory to allocate, in bytes.
 * @param alignment Requested alignment of the memory block (must be power-of-two).
 * @param label     Name of the calling function, used in error messages.
 *
 * @return `true` if all inputs are valid, `false` otherwise.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_alloc_internal
 */
static inline bool arena_alloc_validate_input(t_arena* arena, size_t size, size_t alignment, const char* label)
{
	if (!arena)
	{
		arena_report_error(NULL, "%s failed: NULL arena", label);
		return false;
	}
	if (size == 0)
	{
		arena_report_error(arena, "%s failed: zero-size allocation", label);
		return false;
	}
	if (alignment == 0 || (alignment & (alignment - 1)) != 0)
	{
		arena_report_error(arena, "%s failed: alignment (%zu) is not a power-of-two", label, alignment);
		return false;
	}
	return true;
}

/**
 * @brief
 * Check whether the arena is currently being destroyed.
 *
 * @details
 * This internal helper inspects the `is_destroying` atomic flag to determine
 * whether the arena is in the process of being torn down (e.g., via `arena_destroy()`).
 *
 * If the flag is set, an error is reported with the provided `label`, and the
 * function returns `true` to indicate that no further operations should be performed
 * on the arena.
 *
 * This check prevents undefined behavior and race conditions by avoiding memory
 * access or mutation while destruction is in progress.
 *
 * @param arena Pointer to the arena being queried.
 * @param label Name of the calling function, used for contextual error messages.
 *
 * @return `true` if the arena is being destroyed, `false` otherwise.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_destroy
 * @see arena_alloc_internal
 */
static inline bool arena_is_being_destroyed(t_arena* arena, const char* label)
{
	(void)label;
	if (atomic_load_explicit(&arena->is_destroying, memory_order_acquire))
		return true;
	return false;
}

/**
 * @brief
 * Check whether an allocation would cause a size_t overflow.
 *
 * @details
 * This helper ensures that the requested allocation does not exceed the bounds
 * of the addressable memory space by checking for integer overflow when adding
 * `size` to the current `arena->offset`.
 *
 * If an overflow would occur, the function:
 * - Increments the `failed_allocations` counter in the arena stats.
 * - Logs an error message with the given `label`.
 * - Returns `true` to indicate failure.
 *
 * @param arena Pointer to the arena being used for allocation.
 * @param size  The requested allocation size in bytes.
 * @param label Label or name of the calling function for error context.
 *
 * @return `true` if an overflow would occur, `false` otherwise.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_alloc_internal
 */
static inline bool arena_check_overflow(t_arena* arena, size_t size, const char* label)
{
	if (size > SIZE_MAX - arena->offset)
	{
		arena->stats.failed_allocations++;
		arena_report_error(arena, "%s failed: size overflow (requested: %zu)", label, size);
		return true;
	}
	return false;
}

/**
 * @brief
 * Compute the next aligned offset within the arena.
 *
 * @details
 * This helper calculates the next available offset within the arena's buffer
 * that satisfies the specified alignment requirement. It adds the current offset
 * to the base buffer address, rounds the result up to the nearest aligned address,
 * and then subtracts the base address to return the aligned offset.
 *
 * This value is used to ensure that allocations start at properly aligned memory
 * locations, which is essential for performance and correctness on many platforms.
 *
 * @param arena     Pointer to the arena structure.
 * @param alignment The desired alignment (must be a power of two).
 *
 * @return The aligned offset (relative to the buffer base).
 *
 * @ingroup arena_alloc_internal
 *
 * @see align_up
 * @see arena_ensure_capacity
 */
static inline size_t arena_calc_aligned_offset(t_arena* arena, size_t alignment)
{
	size_t curr    = (size_t) (arena->buffer + arena->offset);
	size_t aligned = align_up(curr, alignment);
	return aligned - (size_t) arena->buffer;
}

/**
 * @brief
 * Attempt to grow the arena's buffer to satisfy an upcoming allocation.
 *
 * @details
 * This internal helper checks whether the arena is allowed to grow (`can_grow` flag)
 * and, if so, attempts to expand its memory region by calling `arena_grow()`.
 *
 * If growth is disallowed or fails, an appropriate error is reported with the
 * given label for context, and the function returns `false`.
 *
 * This function helps support dynamic allocation models where the arena is allowed
 * to resize its memory buffer when out of space.
 *
 * @param arena Pointer to the arena to grow.
 * @param size  Minimum number of bytes required to satisfy the next allocation.
 * @param label Allocation label used for context in error messages.
 *
 * @return `true` if growth succeeded or was unnecessary, `false` otherwise.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_grow
 * @see arena_ensure_capacity
 */
static inline bool arena_try_grow(t_arena* arena, size_t size, const char* label)
{
	if (!arena->can_grow)
	{
		arena_report_error(arena, "%s failed: cannot grow", label);
		return false;
	}
	if (!arena_grow(arena, size))
	{
		arena_report_error(arena, "%s failed: growth failed", label);
		return false;
	}
	return true;
}

/**
 * @brief
 * Ensure that the arena has enough space for a new allocation with alignment.
 *
 * @details
 * This function calculates the aligned offset needed to satisfy an allocation
 * of `size` bytes with the given `alignment`. If the allocation fits in the
 * current arena buffer, the function returns `true`.
 *
 * If the buffer is too small and the arena is allowed to grow, it attempts to
 * expand the buffer via `arena_try_grow()`, and then recalculates the offset
 * and checks again.
 *
 * This function returns:
 * - `true` if enough space is available (after optional growth).
 * - `false` if the arena cannot grow or is still too small.
 *
 * The final aligned offset and wasted bytes (padding due to alignment) are
 * written to the provided output pointers.
 *
 * @param arena          Pointer to the arena being used.
 * @param size           Size of the requested allocation (in bytes).
 * @param alignment      Required alignment (must be power-of-two).
 * @param label          Label for logging and error reporting.
 * @param aligned_offset Output: aligned offset where the allocation would start.
 * @param wasted         Output: number of bytes wasted due to alignment padding.
 *
 * @return `true` if capacity is sufficient, `false` otherwise.
 *
 * @ingroup arena_alloc_internal
 *
 * @see arena_calc_aligned_offset
 * @see arena_try_grow
 */
static inline bool arena_ensure_capacity(t_arena* arena, size_t size, size_t alignment, const char* label,
                                         size_t* aligned_offset, size_t* wasted)
{
	*aligned_offset = arena_calc_aligned_offset(arena, alignment);
	*wasted         = *aligned_offset - arena->offset;

	if (*aligned_offset + size <= arena->size)
		return true;

	if (!arena_try_grow(arena, size, label))
		return false;

	*aligned_offset = arena_calc_aligned_offset(arena, alignment);
	*wasted         = *aligned_offset - arena->offset;

	return *aligned_offset + size <= arena->size;
}

/**
 * @brief
 * Update allocation statistics after a successful arena allocation.
 *
 * @details
 * This function increments and records key allocation-related statistics
 * inside the `arena->stats` structure. These statistics include:
 * - Total number of allocations (`allocations`)
 * - Current number of live allocations (`live_allocations`)
 * - Total number of bytes allocated (`bytes_allocated`)
 * - Total number of wasted bytes due to alignment (`wasted_alignment_bytes`)
 * - Last allocation size (`last_alloc_size`)
 * - Offset of the last allocation (`last_alloc_offset`)
 * - Allocation ID counter (`alloc_id_counter`)
 *
 * The stats are updated inside a critical section to ensure thread safety
 * when `ARENA_ENABLE_THREAD_SAFE` is enabled.
 *
 * @param arena  Pointer to the arena whose statistics are to be updated.
 * @param size   Number of bytes allocated.
 * @param wasted Number of bytes wasted due to alignment padding.
 *
 * @ingroup arena_alloc_internal
 *
 * @note
 * This function performs locking internally (via `ARENA_LOCK` / `ARENA_UNLOCK`),
 * so it should not be called within an existing locked section to avoid deadlocks.
 *
 * @see arena_commit_allocation
 */
static inline void arena_update_stats(t_arena* arena, size_t size, size_t wasted)
{
	if (!arena)
		return;

	ARENA_LOCK(arena);
	arena->stats.allocations++;
	arena->stats.live_allocations++;
	arena->stats.bytes_allocated += size;
	arena->stats.wasted_alignment_bytes += wasted;
	arena->stats.alloc_id_counter++;
	arena->stats.last_alloc_size   = size;
	arena->stats.last_alloc_offset = arena->offset - size;
	ARENA_UNLOCK(arena);
}

/**
 * @brief
 * Update offset, peak usage, and statistics.
 *
 * @details
 * This internal helper is used after determining that an allocation
 * can safely be performed. It updates the arena's `offset` to reflect
 * the new allocation, invokes `arena_update_peak()` to track maximum
 * usage, and updates internal statistics such as allocation count,
 * total allocated bytes, and alignment waste via `arena_update_stats()`.
 *
 * @param arena          Pointer to the arena being updated.
 * @param size           Size in bytes of the allocation.
 * @param wasted         Number of alignment bytes wasted.
 * @param aligned_offset Offset where the allocation begins in the buffer.
 *
 * @ingroup arena_alloc_internal
 *
 * @note
 * This function assumes that allocation boundaries and capacity
 * have already been validated via `arena_ensure_capacity()`.
 *
 * @see arena_alloc_internal
 * @see arena_update_stats
 * @see arena_update_peak
 */
static inline void arena_commit_allocation(t_arena* arena, size_t size, size_t wasted, size_t aligned_offset)
{
	arena->offset = aligned_offset + size;
	arena_update_peak(arena);
	arena_update_stats(arena, size, wasted);
}

/**
 * @brief
 * Zero or poison a memory region depending on allocation label.
 *
 * @details
 * This internal utility determines whether a newly allocated block of memory
 * should be zero-initialized or poisoned for debugging purposes.
 *
 * If the provided `label` is exactly `"arena_calloc_zero"`, the memory is cleared
 * using `memset(ptr, 0, size)`. Otherwise, the memory is poisoned using
 * `arena_poison_memory()` to help detect uninitialized access in debugging mode.
 *
 * This is typically used in `arena_alloc_internal()` after computing the memory
 * location of a new allocation.
 *
 * @param ptr    Pointer to the memory block to initialize.
 * @param size   Number of bytes to initialize.
 * @param label  Allocation label string used to determine behavior.
 *
 * @ingroup arena_alloc_internal
 *
 * @note
 * The label comparison is string-based; other labels will trigger poisoning
 * in debug mode.
 *
 * @see arena_alloc_internal
 * @see arena_poison_memory
 */
static inline void arena_zero_if_needed(void* ptr, size_t size, const char* label)
{
	if (label && strcmp(label, "arena_calloc_zero") == 0)
		memset(ptr, 0, size);
	else
		arena_poison_memory(ptr, size);
}

/**
 * @brief
 * Invoke the user-defined allocation hook, if registered.
 *
 * @details
 * This internal utility checks whether an allocation hook has been registered
 * in the arena’s debug hook system (`arena->hooks.hook_cb`). If a hook is set,
 * it is called with detailed allocation metadata:
 *
 * - Arena pointer
 * - Allocation ID
 * - Pointer to the allocated memory
 * - Size of the allocation
 * - Offset in the arena buffer
 * - Wasted bytes due to alignment
 * - Allocation label
 *
 * The hook is loaded using `atomic_load_explicit` to ensure safe concurrent access.
 * This mechanism enables advanced users to track allocations (e.g., for profiling,
 * leak detection, or debugging).
 *
 * @param arena   Pointer to the arena that performed the allocation.
 * @param ptr     Pointer to the allocated memory block.
 * @param size    Number of bytes allocated.
 * @param offset  Offset in the arena buffer where the allocation began.
 * @param wasted  Number of wasted bytes due to alignment padding.
 * @param label   Label associated with this allocation.
 *
 * @ingroup arena_alloc_internal
 *
 * @note
 * If no hook is set, this function does nothing.
 *
 * @see arena_set_allocation_hook
 * @see arena_alloc_internal
 */
static inline void arena_invoke_allocation_hook(t_arena* arena, void* ptr, size_t size, size_t offset, size_t wasted,
                                                const char* label)
{
	arena_allocation_hook hook = atomic_load_explicit(&arena->hooks.hook_cb, memory_order_acquire);
	if (hook)
		hook(arena, (int) arena->stats.alloc_id_counter, ptr, size, offset, wasted, label);
}

/**
 * @brief
 * Core internal allocator with alignment and labeling support.
 *
 * @details
 * This function performs the complete logic for allocating memory from an arena.
 * It supports:
 * - Alignment-aware allocation.
 * - Out-of-bounds detection and safe growth if enabled.
 * - Labeling for diagnostics and debugging.
 * - Allocation hooks for instrumentation.
 *
 * The flow is:
 * 1. Validate input arguments (alignment, size, etc.).
 * 2. Acquire the arena lock and perform consistency checks.
 * 3. Ensure the arena is not being destroyed.
 * 4. Check for arithmetic overflow.
 * 5. Calculate aligned offset and check capacity.
 * 6. Optionally grow the arena.
 * 7. Commit the allocation (update offset and stats).
 * 8. Zero the memory if requested via label.
 * 9. Trigger the allocation hook if registered.
 * 10. Return a pointer to the allocated memory.
 *
 * @param arena     The arena from which to allocate.
 * @param size      The number of bytes to allocate.
 * @param alignment The alignment in bytes (must be power of two).
 * @param label     A descriptive label for logging and debugging.
 *
 * @return A pointer to the allocated memory, or `NULL` on failure.
 *
 * @ingroup arena_alloc_internal
 *
 * @note
 * Use public wrappers like `arena_alloc()` or `arena_alloc_aligned()` for convenience.
 * This function is not meant to be called directly by users.
 *
 * @see arena_alloc
 * @see arena_alloc_aligned
 * @see arena_alloc_labeled
 */
void* arena_alloc_internal(t_arena* arena, size_t size, size_t alignment, const char* label)
{
	if (!arena_alloc_validate_input(arena, size, alignment, label))
		return NULL;

	ARENA_LOCK(arena);
	ARENA_CHECK(arena);

	if (arena_is_being_destroyed(arena, label))
	{
		ARENA_UNLOCK(arena);
		return NULL;
	}

	if (arena_check_overflow(arena, size, label))
	{
		ARENA_UNLOCK(arena);
		return NULL;
	}

	size_t aligned_offset = 0;
	size_t wasted         = 0;
	if (!arena_ensure_capacity(arena, size, alignment, label, &aligned_offset, &wasted))
	{
		arena->stats.failed_allocations++;
		arena_report_error(arena, "%s failed: out of memory (requested: %zu)", label, size);
		ARENA_UNLOCK(arena);
		return NULL;
	}

	arena_commit_allocation(arena, size, wasted, aligned_offset);
	void* result = arena->buffer + aligned_offset;
	arena_zero_if_needed(result, size, label);
	arena_invoke_allocation_hook(arena, result, size, aligned_offset, wasted, label);

	ALOG("[arena] %s: Allocated %zu bytes @ offset %zu (arena %p)\n", label, size, aligned_offset, (void*) arena);

	ARENA_CHECK(arena);
	ARENA_UNLOCK(arena);
	return result;
}