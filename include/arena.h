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

	void* arena_alloc(t_arena* arena, size_t size);
	void* arena_alloc_aligned(t_arena* arena, size_t size, size_t alignment);
	void* arena_alloc_labeled(t_arena* arena, size_t size, const char* label);
	void* arena_alloc_aligned_labeled(t_arena* arena, size_t size, size_t alignment, const char* label);

	void* arena_calloc(t_arena* arena, size_t count, size_t size);
	void* arena_calloc_aligned(t_arena* arena, size_t count, size_t size, size_t alignment);
	void* arena_calloc_labeled(t_arena* arena, size_t count, size_t size, const char* label);
	void* arena_calloc_aligned_labeled(t_arena* arena, size_t count, size_t size, size_t alignment, const char* label);

	bool arena_alloc_sub(t_arena* parent, t_arena* child, size_t size);
	bool arena_alloc_sub_aligned(t_arena* parent, t_arena* child, size_t size, size_t alignment);
	bool arena_alloc_sub_labeled(t_arena* parent, t_arena* child, size_t size, const char* label);
	bool arena_alloc_sub_labeled_aligned(t_arena* parent, t_arena* child, size_t size, size_t alignment,
	                                     const char* label);

	void* arena_realloc_last(t_arena* arena, void* old_ptr, size_t old_size, size_t new_size);

	bool arena_grow(t_arena* arena, size_t required_size);
	void arena_shrink(t_arena* arena, size_t new_size);
	bool arena_might_shrink(t_arena* arena);

	size_t         arena_used(t_arena* arena);
	size_t         arena_remaining(t_arena* arena);
	size_t         arena_peak(t_arena* arena);
	t_arena_marker arena_mark(t_arena* arena);
	void           arena_pop(t_arena* arena, t_arena_marker marker);

#ifdef __cplusplus
}
#endif

#endif // ARENA_H
