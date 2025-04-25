/**
 * @file arena.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Core public API for the Arena memory allocator.
 *
 * @details
 * This header defines the central data structure (`t_arena`) and exposes
 * all major functions for arena-based memory allocation, deallocation,
 * initialization, and state management.
 *
 * Features:
 * - Fast linear bump allocation
 * - Optional dynamic resizing (grow/shrink)
 * - Thread-safe support (via `ARENA_ENABLE_THREAD_SAFE`)
 * - Memory poisoning for debugging (via `ARENA_POISON_MEMORY`)
 * - Sub-arenas and marker-based rollback
 * - Allocation labels, hooks, and diagnostics
 * - Debug statistics, growth tracking, and stack frame support
 *
 * This header is the entry point to the arena system and is included
 * by all modules using arenas for custom allocation.
 *
 * Optional modules included:
 * - `arena_tlscratch.h` for thread-local scratch arenas
 * - `arena_stats.h`, `arena_debug.h`, `arena_hooks.h` for profiling and tooling.
 *
 * @ingroup arena_core
 */

#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>
#include <string.h>

#include "internal/arena_debug.h"
#include "arena_hooks.h"
#include "internal/arena_internal.h"
#include "internal/arena_math.h"
#include "arena_stats.h"

#ifdef ARENA_ENABLE_THREAD_SAFE
#include <pthread.h>
#endif

#ifdef ARENA_ENABLE_THREAD_LOCAL_SCRATCH
#include "arena_tlscratch.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @typedef arena_grow_callback
	 * @brief Function pointer type for custom arena growth strategies.
	 *
	 * @details
	 * This callback is used by dynamically growable arenas to compute the
	 * new size when an allocation exceeds current capacity.
	 *
	 * The function should return the new size (in bytes), based on:
	 * - `current_size`: the current total size of the arena buffer.
	 * - `requested_size`: the size that failed and triggered the grow.
	 *
	 * @param current_size  Current arena buffer size (in bytes).
	 * @param requested_size Size required for the failed allocation (in bytes).
	 * @return The new buffer size to grow to, or `0` to indicate failure.
	 *
	 * @ingroup arena_resize
	 *
	 * @note
	 * This function is optional. If `NULL`, the arena will not attempt to grow.
	 */
	typedef size_t (*arena_grow_callback)(size_t current_size, size_t requested_size);

	/**
	 * @typedef t_arena_marker
	 * @brief Type used for marking and rolling back arena allocation state.
	 *
	 * @details
	 * Represents an offset into the arena's buffer. Used in conjunction with
	 * `arena_mark()` and `arena_pop()` for scoped memory rollback.
	 *
	 * @ingroup arena_state
	 */
	typedef size_t t_arena_marker;

	/**
	 * @struct t_arena
	 * @brief The main memory arena structure used for fast allocation.
	 *
	 * @details
	 * This structure represents a linear memory arena. It manages a fixed-size
	 * or growable buffer from which memory is allocated using a bump-pointer strategy.
	 *
	 * Members:
	 * - `buffer`: Pointer to the start of the memory block.
	 * - `size`: Total size of the buffer in bytes.
	 * - `offset`: Current bump pointer offset (number of bytes used).
	 * - `grow_cb`: Optional callback for dynamic resizing.
	 * - `parent_ref`: If this is a sub-arena, points to the parent arena.
	 * - `marker_stack`: Stack of saved markers for scoped rollback.
	 * - `marker_stack_top`: Index of the top of the marker stack.
	 * - `owns_buffer`: Whether the arena owns the memory and should free it.
	 * - `can_grow`: Whether this arena can grow dynamically.
	 * - `is_destroying`: Flag indicating the arena is currently being destroyed.
	 *
	 * Thread Safety:
	 * - `lock`: Mutex used when `ARENA_ENABLE_THREAD_SAFE` is enabled.
	 * - `use_lock`: Whether this arena uses thread-safe locking internally.
	 *
	 * Debug and Instrumentation:
	 * - `stats`: Runtime statistics for allocations, peak usage, etc.
	 * - `debug`: Arena debug metadata (label, ID, error handler).
	 * - `hooks`: Allocation hooks for debugging, profiling, or tracking.
	 *
	 * @ingroup arena_core
	 *
	 * @note
	 * Use the API functions (`arena_alloc`, `arena_reset`) rather than
	 * manipulating this structure directly.
	 */
	typedef struct s_arena
	{
		uint8_t*            buffer;     /**< Pointer to the memory buffer. */
		size_t              size;       /**< Total size of the buffer. */
		size_t              offset;     /**< Current used offset (bump pointer). */
		arena_grow_callback grow_cb;    /**< Optional callback for dynamic resizing. */
		struct s_arena*     parent_ref; /**< Reference to parent arena if this is a sub-arena. */
		t_arena_marker      marker_stack[ARENA_MAX_STACK_DEPTH]; /**< Stack of saved markers. */
		int                 marker_stack_top;                    /**< Top index of marker stack. */
		_Atomic bool        owns_buffer;                         /**< Whether this arena owns the buffer memory. */
		_Atomic bool        can_grow;                            /**< Whether the arena supports dynamic growth. */
		_Atomic bool        is_destroying;                       /**< Indicates the arena is being destroyed. */

#ifdef ARENA_ENABLE_THREAD_SAFE
		pthread_mutex_t lock;     /**< Mutex for thread-safe operations. */
		bool            use_lock; /**< Enable or disable internal locking. */
#endif

		t_arena_stats stats; /**< Allocation and memory usage statistics. */
		t_arena_debug debug; /**< Debugging and diagnostic metadata. */
		t_arena_hooks hooks; /**< Allocation hooks for monitoring/debugging. */
	} t_arena;

	t_arena* arena_create(size_t size, bool allow_grow);
	bool     arena_init(t_arena* arena, size_t size, bool allow_grow);
	void     arena_init_with_buffer(t_arena* arena, void* buffer, size_t size, bool allow_grow);
	void     arena_reinit_with_buffer(t_arena* arena, void* buffer, size_t size, bool allow_grow);
	void     arena_destroy(t_arena* arena);
	void     arena_delete(t_arena** arena);
	void     arena_reset(t_arena* arena);

	void* arena_alloc_internal(t_arena* arena, size_t size, size_t alignment, const char* label);

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
	void* arena_alloc(t_arena* arena, size_t size);

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
	void* arena_alloc_aligned(t_arena* arena, size_t size, size_t alignment);

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
	void* arena_alloc_labeled(t_arena* arena, size_t size, const char* label);

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
	void* arena_alloc_aligned_labeled(t_arena* arena, size_t size, size_t alignment, const char* label);

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
	void* arena_calloc(t_arena* arena, size_t count, size_t size);

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
	void* arena_calloc_aligned(t_arena* arena, size_t count, size_t size, size_t alignment);

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
	void* arena_calloc_labeled(t_arena* arena, size_t count, size_t size, const char* label);

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
	void* arena_calloc_aligned_labeled(t_arena* arena, size_t count, size_t size, size_t alignment, const char* label);

	/**
	 * @brief
	 * Allocate a sub-arena from a parent arena using default alignment and label.
	 *
	 * @details
	 * This function allocates a memory region of `size` bytes from the `parent` arena,
	 * then initializes the `child` arena to use that region as its buffer. The child
	 * arena does not take ownership of the buffer and uses the default debug label `"subarena"`.
	 *
	 * Internally, this delegates to `arena_alloc_sub_labeled()` with a default label.
	 *
	 * @param parent Pointer to the parent arena to allocate memory from.
	 * @param child  Pointer to the arena structure to initialize as a sub-arena.
	 * @param size   Number of bytes to allocate for the sub-arena.
	 *
	 * @return `true` on success, `false` on failure.
	 *
	 * @ingroup arena_sub
	 *
	 * @note
	 * The sub-arena shares memory with the parent and must not be destroyed using `arena_delete()`.
	 * Only `arena_destroy()` should be used to reset it.
	 *
	 * @see arena_alloc_sub_labeled
	 * @see arena_destroy
	 * @see arena_init_with_buffer
	 *
	 * @example
	 * @code
	 * t_arena main_arena;
	 * arena_init(&main_arena, 8192, false);
	 *
	 * t_arena sub_arena;
	 * if (!arena_alloc_sub(&main_arena, &sub_arena, 2048))
	 * {
	 *     // Handle failure
	 *     return;
	 * }
	 *
	 * void* ptr = arena_alloc(&sub_arena, 128);
	 * if (!ptr)
	 * {
	 *     // Handle allocation failure in sub-arena
	 * }
	 *
	 * // Cleanup sub-arena (does not free its memory)
	 * arena_destroy(&sub_arena);
	 *
	 * // Cleanup main arena (frees both its own and sub-arena’s memory)
	 * arena_destroy(&main_arena);
	 * @endcode
	 */
	bool arena_alloc_sub(t_arena* parent, t_arena* child, size_t size);

	/**
	 * @brief
	 * Allocate a sub-arena from a parent arena with custom alignment.
	 *
	 * @details
	 * This function allocates `size` bytes from the `parent` arena using the
	 * specified `alignment`, and initializes the `child` arena with that memory.
	 * It behaves like `arena_alloc_sub_labeled_aligned()` but uses a default
	 * label `"subarena"` for tracking.
	 *
	 * Use this when:
	 * - You want to embed a sub-arena inside a parent arena with specific alignment constraints.
	 * - You don't need a custom label for the sub-arena.
	 *
	 * Internally:
	 * - Memory is allocated from the parent with the requested alignment.
	 * - The child arena is initialized using that memory (but does not own it).
	 * - A unique sub-arena ID is generated.
	 *
	 * @param parent     The parent `t_arena` to allocate memory from.
	 * @param child      The `t_arena` to initialize as a sub-arena.
	 * @param size       Number of bytes to allocate.
	 * @param alignment  Required memory alignment (must be a power of two).
	 *
	 * @return `true` if allocation and setup succeed, `false` otherwise.
	 *
	 * @ingroup arena_sub
	 *
	 * @note
	 * The child arena must be destroyed before the parent arena.
	 *
	 * @see arena_alloc_sub_labeled_aligned
	 * @see arena_init_with_buffer
	 * @see arena_destroy
	 *
	 * @example
	 * @code
	 * #include "arena.h"
	 *
	 * int main(void)
	 * {
	 *     t_arena parent;
	 *     t_arena child;
	 *
	 *     // Initialize a parent arena with 8 KB of memory
	 *     if (!arena_init(&parent, 8192, false))
	 *         return 1;
	 *
	 *     // Create a sub-arena of 1 KB aligned to 64 bytes
	 *     if (!arena_alloc_sub_aligned(&parent, &child, 1024, 64))
	 *     {
	 *         arena_destroy(&parent);
	 *         return 1;
	 *     }
	 *
	 *     // Allocate memory from the sub-arena
	 *     void* data = arena_alloc(&child, 256);
	 *
	 *     // Perform cleanup
	 *     arena_destroy(&child);  // Only destroys metadata, memory is still owned by parent
	 *     arena_destroy(&parent); // Frees memory if owned
	 *     return 0;
	 * }
	 * @endcode
	 */
	bool arena_alloc_sub_aligned(t_arena* parent, t_arena* child, size_t size, size_t alignment);

	/**
	 * @brief
	 * Allocate and initialize a sub-arena with a custom label using default alignment.
	 *
	 * @details
	 * This function creates a sub-arena from the given `parent` arena by allocating
	 * a block of memory of size `size` and initializing the `child` arena to use it.
	 * The sub-arena is assigned a debug `label` for tracking or diagnostic purposes.
	 *
	 * Internally, this delegates to `arena_alloc_sub_labeled_aligned()` using the
	 * default alignment (`ARENA_DEFAULT_ALIGNMENT`).
	 *
	 * Use this when:
	 * - You want to segment memory within a parent arena for modular usage.
	 * - You don’t need custom alignment but want to distinguish allocations logically.
	 *
	 * The resulting sub-arena:
	 * - Shares memory with its parent and does not own its buffer.
	 * - Should be destroyed with `arena_destroy()` (but not freed).
	 *
	 * @param parent Pointer to the parent arena.
	 * @param child  Pointer to the arena to initialize as a sub-arena.
	 * @param size   Size of the memory block to allocate from the parent.
	 * @param label  Optional debug label. If `NULL`, defaults to `"subarena"`.
	 *
	 * @return `true` on success, `false` if allocation or initialization fails.
	 *
	 * @ingroup arena_sub
	 *
	 * @note
	 * The parent arena must remain valid as long as the sub-arena is in use.
	 *
	 * @see arena_alloc_sub
	 * @see arena_alloc_sub_labeled_aligned
	 * @see arena_destroy
	 * @example
	 * @code
	 * #include "arena.h"
	 *
	 * int main(void)
	 * {
	 *     // Create and initialize a parent arena
	 *     t_arena parent;
	 *     arena_init(&parent, 4096, false);
	 *
	 *     // Declare a child arena
	 *     t_arena child;
	 *
	 *     // Allocate a sub-arena of 1024 bytes from the parent
	 *     if (!arena_alloc_sub_labeled(&parent, &child, 1024, "child_stack"))
	 *     {
	 *         // Handle allocation failure
	 *         return 1;
	 *     }
	 *
	 *     // Use the child arena
	 *     void* ptr = arena_alloc(&child, 128);
	 *
	 *     // Destroy both arenas (child first)
	 *     arena_destroy(&child);
	 *     arena_destroy(&parent);
	 *     return 0;
	 * }
	 * @endcode
	 */
	bool arena_alloc_sub_labeled(t_arena* parent, t_arena* child, size_t size, const char* label);

	/**
	 * @brief
	 * Allocate and initialize a sub-arena from a parent arena with custom alignment and label.
	 *
	 * @details
	 * This function allocates a memory block of `size` bytes from the `parent` arena
	 * with the specified `alignment`, and uses it to initialize the `child` arena.
	 * The `child` will reference the parent as its backing source but will not take
	 * ownership of the buffer.
	 *
	 * A debug `label` is assigned to the sub-arena for tracking and logging purposes.
	 * If `label` is `NULL`, the label defaults to `"subarena"`.
	 *
	 * Use this when:
	 * - You want to partition a region of memory within an arena for isolated use.
	 * - You need alignment control and want to track the sub-arena via a label.
	 *
	 * @param parent     Pointer to the parent `t_arena` from which to allocate memory.
	 * @param child      Pointer to the `t_arena` to initialize as a sub-arena.
	 * @param size       Number of bytes to allocate for the sub-arena.
	 * @param alignment  Alignment requirement for the memory block (must be a power of two).
	 * @param label      Optional string label for the sub-arena. If `NULL`, defaults to "subarena".
	 *
	 * @return `true` on success, `false` on failure.
	 *
	 * @ingroup arena_sub
	 *
	 * @note
	 * The child arena will not free its buffer on destruction, as it does not own it.
	 * The parent arena must outlive all sub-arenas allocated from it.
	 *
	 * @see arena_alloc_sub
	 * @see arena_setup_subarena
	 * @see arena_alloc_aligned
	 *
	 * @example
		@code
		#include "arena.h"

		int main(void)
		{
			// Create and initialize the parent arena
			t_arena parent;
			arena_init(&parent, 4096, false);

			// Define a child arena (sub-arena)
			t_arena child;

			// Allocate a 1024-byte sub-arena aligned to 64 bytes, with a custom label
			if (!arena_alloc_sub_labeled_aligned(&parent, &child, 1024, 64, "temp_scope"))
			{
				fprintf(stderr, "Failed to create sub-arena\n");
				return 1;
			}

			// Use the sub-arena
			void* data = arena_alloc(&child, 256);

			// When done, destroy the sub-arena (does not free buffer)
			arena_destroy(&child);

			// Finally, destroy the parent arena (frees the backing buffer)
			arena_destroy(&parent);
			return 0;
		}
		@endcode
	*/
	bool arena_alloc_sub_labeled_aligned(t_arena* parent, t_arena* child, size_t size, size_t alignment,
	                                     const char* label);

	/**
	 * @brief
	 * Reallocate the most recent allocation in the arena.
	 *
	 * @details
	 * This function attempts to resize the last allocated block in the arena
	 * from `old_size` to `new_size`. If the block is the most recent allocation
	 * (i.e., its pointer matches the current `arena->offset`), the reallocation
	 * is done **in place** if possible, potentially growing the arena if allowed.
	 *
	 * If the block is not the most recent one, a **fallback strategy** is used:
	 * a new block is allocated, data is copied, and the old memory is poisoned
	 * to help detect stale access.
	 *
	 * This function:
	 * - Validates the input parameters.
	 * - Locks the arena for thread safety.
	 * - Attempts in-place reallocation if the block is the most recent.
	 * - Falls back to allocating new memory and copying the old contents.
	 * - Updates allocation statistics and invokes debug hooks.
	 *
	 * @param arena     Pointer to the arena from which the block was originally allocated.
	 * @param old_ptr   Pointer to the block to reallocate.
	 * @param old_size  Current size of the block (in bytes).
	 * @param new_size  Desired new size of the block (in bytes).
	 *
	 * @return Pointer to the reallocated block, or `NULL` on failure.
	 *
	 * @ingroup arena_alloc
	 *
	 * @note
	 * This function only supports reallocating the **last** allocation in-place.
	 * For all other blocks, reallocation requires allocation + copy.
	 *
	 * @warning
	 * If reallocation fails, the original block remains valid but unchanged.
	 *
	 * @see realloc_in_place
	 * @see realloc_fallback
	 * @see arena_alloc
	 * @see arena_grow
	 *
	 *  * @example
	 * @code
	 * t_arena arena;
	 * arena_init(&arena, 4096, true);
	 *
	 * // Allocate a block of memory
	 * void* block = arena_alloc(&arena, 256);
	 *
	 * // Perform some work...
	 *
	 * // Reallocate the last block to a larger size
	 * void* resized = arena_realloc_last(&arena, block, 256, 512);
	 * if (!resized)
	 * {
	 *     // Handle reallocation failure
	 * }
	 *
	 * // Shrink the same block
	 * resized = arena_realloc_last(&arena, resized, 512, 128);
	 * if (!resized)
	 * {
	 *     // Handle reallocation failure
	 * }
	 *
	 * // Clean up
	 * arena_destroy(&arena);
	 * @endcode
	 */
	void* arena_realloc_last(t_arena* arena, void* old_ptr, size_t old_size, size_t new_size);

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
	bool arena_grow(t_arena* arena, size_t required_size);

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
	void arena_shrink(t_arena* arena, size_t new_size);

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
	bool arena_might_shrink(t_arena* arena);

	/**
	 * @brief
	 * Get the number of bytes currently used in the arena.
	 *
	 * @details
	 * This function returns the current offset of the arena, which represents
	 * the total number of bytes that have been allocated (but not necessarily in use).
	 * It does not include alignment padding or internal fragmentation.
	 *
	 * This is useful for:
	 * - Measuring memory usage.
	 * - Debugging or profiling arena-based systems.
	 * - Saving the state for later rollback via `arena_mark()` and `arena_pop()`.
	 *
	 * Thread-safe: this function locks the arena before accessing internal state.
	 *
	 * @param arena Pointer to the arena.
	 *
	 * @return The number of bytes currently used, or `0` if `arena` is `NULL`.
	 *
	 * @ingroup arena_state
	 *
	 * @see arena_remaining
	 * @see arena_peak
	 * @see arena_mark
	 *
	 * @example
	 * @code
	 * t_arena* arena = arena_create(1024, true);
	 * arena_alloc(arena, 128);
	 * arena_alloc(arena, 64);
	 *
	 * size_t used = arena_used(arena);
	 * printf("Used bytes: %zu\n", used); // Output: 192 (if no padding), or more if aligned
	 *
	 * arena_delete(&arena);
	 * @endcode
	 */
	size_t         arena_used(t_arena* arena);

	/**
	 * @brief
	 * Get the number of remaining (free) bytes in the arena.
	 *
	 * @details
	 * This function returns the number of bytes left for allocation in the arena.
	 * It computes `arena->size - arena->offset`, giving you insight into how much
	 * space is still available before the arena must grow (if growable) or fail
	 * subsequent allocations.
	 *
	 * This is useful for:
	 * - Monitoring arena capacity.
	 * - Deciding whether to grow the arena or switch to a fallback allocator.
	 * - Debugging memory exhaustion or fragmentation issues.
	 *
	 * Thread-safe: this function acquires the arena lock before accessing internal state.
	 *
	 * @param arena Pointer to the arena.
	 *
	 * @return The number of free bytes available, or `0` if `arena` is `NULL`.
	 *
	 * @ingroup arena_state
	 *
	 * @see arena_used
	 * @see arena_peak
	 * @see arena_grow
	 *
	 * @example
	 * @code
	 * t_arena* arena = arena_create(256, true);
	 * arena_alloc(arena, 64);
	 *
	 * size_t free_bytes = arena_remaining(arena);
	 * printf("Remaining space: %zu bytes\n", free_bytes); // Likely: 192 (or less with padding)
	 *
	 * arena_delete(&arena);
	 * @endcode
	 */
	size_t         arena_remaining(t_arena* arena);
	/**
	 * @brief
	 * Return the peak memory usage observed in the arena.
	 *
	 * @details
	 * This function returns the highest number of bytes ever allocated
	 * in the arena since it was created or last reset. It is useful
	 * for monitoring memory pressure, tuning arena sizing, or profiling
	 * high-water marks in long-running systems.
	 *
	 * This metric is updated internally by the allocator whenever a new
	 * allocation pushes the `offset` beyond the current `peak_usage`.
	 *
	 * Thread-safe: this function acquires the arena lock to ensure
	 * accurate and safe access in concurrent environments.
	 *
	 * @param arena Pointer to the arena to inspect.
	 *
	 * @return The peak number of bytes used at any point in the arena's lifetime.
	 *         Returns `0` if `arena` is `NULL`.
	 *
	 * @ingroup arena_state
	 *
	 * @see arena_used
	 * @see arena_remaining
	 * @see arena_update_peak
	 *
	 * @example
	 * @code
	 * t_arena* arena = arena_create(1024, true);
	 * arena_alloc(arena, 256);
	 * arena_alloc(arena, 128);
	 *
	 * size_t peak = arena_peak(arena);
	 * printf("Peak usage: %zu bytes\n", peak); // Should print 384
	 *
	 * arena_reset(arena);
	 * printf("Peak after reset: %zu bytes\n", arena_peak(arena)); // Still 384 (reset does not affect peak)
	 *
	 * arena_delete(&arena);
	 * @endcode
	 */
	size_t         arena_peak(t_arena* arena);
	/**
	 * @brief
	 * Capture the current allocation offset as a marker.
	 *
	 * @details
	 * This function returns a marker representing the current position
	 * in the arena’s allocation buffer. The marker can be used later with
	 * `arena_pop()` to roll back the arena's state to this exact point,
	 * effectively freeing all allocations made after the marker.
	 *
	 * This is useful for implementing temporary memory scopes where multiple
	 * allocations are made and discarded in bulk after a certain operation.
	 *
	 * Thread-safe: this function acquires the arena lock.
	 *
	 * @param arena Pointer to the arena to mark.
	 *
	 * @return A marker (offset) that can later be passed to `arena_pop()`.
	 *         Returns `0` if `arena` is `NULL`.
	 *
	 * @ingroup arena_state
	 *
	 * @note
	 * A marker is only valid for the arena it was created from.
	 *
	 * @see arena_pop
	 *
	 * @example
	 * @code
	 * t_arena* arena = arena_create(1024, true);
	 * t_arena_marker mark = arena_mark(arena);
	 *
	 * char* buffer = arena_alloc(arena, 256);
	 * // Use buffer...
	 *
	 * // Roll back to previous state, freeing the last allocation
	 * arena_pop(arena, mark);
	 *
	 * arena_delete(&arena);
	 * @endcode
	 */
	t_arena_marker arena_mark(t_arena* arena);
	/**
	 * @brief
	 * Revert the arena’s allocation state to a previously saved marker.
	 *
	 * @details
	 * This function restores the arena's offset to a prior state recorded
	 * using `arena_mark()`. All memory allocated after the marker is considered
	 * discarded. This is a fast and efficient way to bulk-free temporary allocations.
	 *
	 * - If the provided `marker` is greater than the current offset, the function
	 *   logs an error and does nothing.
	 * - If the marker is valid, it poisons (optionally overwrites) the discarded
	 *   region and rewinds the offset in debug mode.
	 *
	 * Thread-safe: this function acquires the arena lock.
	 *
	 * @param arena  Pointer to the arena to rewind.
	 * @param marker Marker previously obtained from `arena_mark()`.
	 *
	 * @return void
	 *
	 * @ingroup arena_state
	 *
	 * @note
	 * The `marker` must originate from the same arena. Using an invalid marker
	 * results in no change and logs an error.
	 *
	 * @see arena_mark
	 * @see arena_reset
	 *
	 * @example
	 * @code
	 * t_arena* arena = arena_create(4096, true);
	 * t_arena_marker mark = arena_mark(arena);
	 *
	 * // Temporary memory region
	 * char* buffer = arena_alloc(arena, 1024);
	 * // Use the buffer...
	 *
	 * // Discard the buffer and any other allocations made after the marker
	 * arena_pop(arena, mark);
	 *
	 * arena_delete(&arena);
	 * @endcode
	 */
	void           arena_pop(t_arena* arena, t_arena_marker marker);

#ifdef __cplusplus
}
#endif

#endif // ARENA_H
