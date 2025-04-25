/**
 * @file arena_create.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Arena creation and initialization API.
 *
 * @details
 * This header declares the primary API functions for allocating and initializing arenas
 * using either dynamically or statically allocated structures and memory buffers.
 *
 * The API includes four main entry points:
 *
 * - `arena_create()` – Allocates both the arena and its buffer dynamically.
 * - `arena_init()` – Initializes a stack- or statically-allocated arena with an internal buffer.
 * - `arena_init_with_buffer()` – Initializes a pre-allocated arena with a user-provided or malloc'ed buffer.
 * - `arena_reinit_with_buffer()` – Destroys and reinitializes an existing arena.
 *
 * These functions serve various use cases, such as:
 * - Full dynamic arena creation and lifetime management.
 * - Reusing arena structs in embedded or performance-sensitive systems.
 * - Managing arenas over externally controlled memory regions.
 *
 * All APIs are safe to use in multi-threaded environments when `ARENA_ENABLE_THREAD_SAFE` is defined.
 *
 * @note
 * Most functions rely on shared internal helpers (e.g. `arena_finish_init`, `arena_reset_metadata`).
 * These are defined as `static inline` and reside in the implementation file.
 *
 * @ingroup arena_core
 */

#include "arena.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARENA_ENABLE_THREAD_SAFE
#include <pthread.h>
#endif

/*
 * INTERNAL FUNCTION DECLARATIONS
 */

static inline void     arena_set_default_label(t_arena* arena, const char* fallback);
static inline t_arena* arena_alloc_struct(void);
static inline void     arena_log_and_teardown(t_arena** arena, const char* msg);
static inline void*    arena_alloc_buffer(size_t size);
static inline void     arena_reset_metadata(t_arena* arena);
static inline bool     arena_init_mutex(t_arena* arena);
static inline bool     arena_finish_init(t_arena* arena, void* buffer, size_t size, bool allow_grow);
static inline bool     arena_set_allocated_buffer(t_arena* arena, size_t size);
static inline void     arena_set_user_buffer(t_arena* arena, void* buffer, size_t size);
static inline bool     arena_set_or_alloc_buffer(t_arena* arena, void* buffer, size_t size);

/*
 * PUBLIC API
 */

t_arena* arena_create(size_t size, bool allow_grow)
{
	if (size == 0)
	{
		arena_report_error(NULL, "arena_create failed: zero size requested");
		return NULL;
	}

	t_arena* arena = arena_alloc_struct();
	if (!arena)
		return NULL;

	void* buffer = arena_alloc_buffer(size);
	if (!buffer)
	{
		arena_log_and_teardown(&arena, "arena_create: buffer allocation failed");
		return NULL;
	}

	if (!arena_finish_init(arena, buffer, size, allow_grow))
	{
		arena_log_and_teardown(&arena, "arena_create: init failure");
		return NULL;
	}

	arena_set_default_label(arena, "arena_heap");
	arena_generate_id(arena);
	return arena;
}

bool arena_init(t_arena* arena, size_t size, bool allow_grow)
{
	if (!arena || size == 0)
	{
		arena_report_error(NULL, "arena_init failed: invalid arena or size");
		return false;
	}

	void* buffer = arena_alloc_buffer(size);
	if (!buffer)
	{
		arena_log_and_teardown(&arena, "arena_init: buffer allocation failed");
		return NULL;
	}

	if (!arena_finish_init(arena, buffer, size, allow_grow))
	{
		arena_report_error(arena, "arena_init: arena_finish_init failed");
		free(buffer);
		return false;
	}

	arena_set_default_label(arena, "arena_stack");
	arena_generate_id(arena);
	return true;
}

void arena_init_with_buffer(t_arena* arena, void* buffer, size_t size, bool allow_grow)
{
	if (!arena)
	{
		arena_report_error(NULL, "arena_init_with_buffer: NULL arena");
		return;
	}

	arena_reset_metadata(arena);
	atomic_store_explicit(&arena->can_grow, allow_grow, memory_order_release);
	arena_init_mutex(arena);

	if (!arena_set_or_alloc_buffer(arena, buffer, size))
		return;

	arena_set_default_label(arena, "arena_from_buffer");
	arena_generate_id(arena);
}

void arena_reinit_with_buffer(t_arena* arena, void* buffer, size_t size, bool allow_grow)
{
	arena_destroy(arena);
	arena_init_with_buffer(arena, buffer, size, allow_grow);
}

/*
 * INTERNAL HELPERS (static inline)
 */

/**
 * @brief
 * Set a fallback debug label if none is set.
 *
 * @details
 * This internal utility ensures that an arena has a human-readable label
 * for debugging and error reporting. If the `arena->debug.label` is `NULL`,
 * it assigns the provided fallback label.
 *
 * Labels are used in logs and diagnostic tools to identify arenas more easily,
 * especially in multi-arena systems or when visualizing memory usage.
 *
 * This function is used during initialization, after clearing metadata.
 *
 * @param arena    Pointer to the arena structure to label.
 * @param fallback The default label to assign if no label is already set.
 *
 * @ingroup arena_internal
 *
 * @note
 * This does not copy the string; it simply sets the pointer.
 * The fallback string must remain valid for the arena’s lifetime.
 *
 * @see arena_create
 * @see arena_init
 * @see arena_init_with_buffer
 */
static inline void arena_set_default_label(t_arena* arena, const char* fallback)
{
	if (!arena->debug.label)
		arena->debug.label = fallback;
}

/**
 * @brief
 * Allocate and zero-initialize a new arena struct.
 *
 * @details
 * This internal helper allocates memory for a `t_arena` structure using `calloc`,
 * which ensures all fields are zero-initialized. This guarantees a clean slate
 * before any initialization logic is applied, helping avoid undefined behavior
 * from stale data.
 *
 * On failure, the function returns `NULL`. The calling function is responsible
 * for handling the failure and cleaning up if needed.
 *
 * This function does **not** allocate the memory buffer used for allocations—
 * it only handles the control structure itself.
 *
 * @return Pointer to a newly allocated `t_arena`, or `NULL` on failure.
 *
 * @ingroup arena_internal
 *
 * @see arena_create
 */
static inline t_arena* arena_alloc_struct(void)
{
	return (t_arena*) calloc(1, sizeof(t_arena));
}

/**
 * @brief
 * Report an error, destroy the arena, and free its memory.
 *
 * @details
 * This helper is used to cleanly handle failure paths in arena creation or
 * initialization. It prints a formatted error message using `arena_report_error`,
 * then proceeds to safely destroy the arena and free the `t_arena` structure via
 * `arena_destroy()` and `arena_delete()`.
 *
 * If the `arena` pointer is `NULL` or points to `NULL`, only the error is logged
 * and no cleanup is performed. This guards against double-frees or null dereferences.
 *
 * @param arena A pointer to a pointer to a `t_arena` structure. May be `NULL`.
 * @param msg   A formatted error message passed to `arena_report_error`.
 *
 * @ingroup arena_internal
 *
 * @note
 * This function assumes ownership of the `arena` and will leave the pointer invalid.
 *
 * @see arena_create
 * @see arena_init
 * @see arena_report_error
 * @see arena_destroy
 * @see arena_delete
 */
static inline void arena_log_and_teardown(t_arena** arena, const char* msg)
{
	if (!arena || !*arena)
	{
		arena_report_error(NULL, "%s", msg);
		return;
	}
	arena_report_error(*arena, "%s", msg);
	arena_destroy(*arena);
	arena_delete(arena);
}

/**
 * @brief
 * Allocate a zero-initialized memory buffer for the arena.
 *
 * @details
 * This function wraps `calloc` to allocate a contiguous zeroed memory region
 * of `size` bytes. It is used when the arena needs to allocate its own internal
 * memory buffer (rather than relying on an external one provided by the user).
 *
 * If allocation fails, the function reports the failure using `arena_report_error`.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory buffer, or `NULL` if allocation fails.
 *
 * @ingroup arena_internal
 *
 * @note
 * The buffer must be freed manually if the initialization of the arena fails.
 *
 * @see arena_create
 * @see arena_init
 */
static inline void* arena_alloc_buffer(size_t size)
{
	void* buffer = calloc(1, size);
	if (!buffer)
		arena_report_error(NULL, "arena_create failed: buffer allocation of %zu bytes failed", size);
	return buffer;
}

/**
 * @brief
 * Reset all arena metadata fields to their default initial state.
 *
 * @details
 * This function clears the entire `t_arena` structure's internal metadata,
 * ensuring a clean and consistent base before the arena is initialized.
 *
 * It performs the following actions:
 * - Nullifies the memory buffer and resets size/offset tracking.
 * - Clears the marker stack and parent reference.
 * - Sets the default grow callback and debug error handler.
 * - Clears all debug metadata and hook pointers.
 * - Resets all internal statistics via `arena_stats_reset`.
 * - Initializes atomic flags like `owns_buffer`, `can_grow`, and `is_destroying`.
 * - Disables locking if thread safety is enabled.
 *
 * This function must be called before using or reinitializing the arena.
 * It is designed to avoid stale data and ensure safe concurrent visibility.
 *
 * @param arena Pointer to the arena to reset.
 *
 * @ingroup arena_internal
 *
 * @note
 * This function does **not** free or allocate memory. It only resets metadata.
 *
 * @see arena_finish_init
 * @see arena_init_with_buffer
 */
static inline void arena_reset_metadata(t_arena* arena)
{
	arena->buffer           = NULL;
	arena->size             = 0;
	arena->offset           = 0;
	arena->marker_stack_top = 0;
	memset(arena->marker_stack, 0, sizeof(arena->marker_stack));
	arena->parent_ref = NULL;

	arena->grow_cb             = default_grow_cb;
	arena->debug.error_cb      = arena_default_error_callback;
	arena->debug.error_context = NULL;
	arena->debug.label         = NULL;

	arena->hooks.hook_cb = NULL;
	arena->hooks.context = NULL;

	arena_stats_reset(&arena->stats);

	atomic_store_explicit(&arena->owns_buffer, false, memory_order_release);
	atomic_store_explicit(&arena->can_grow, false, memory_order_release);
	atomic_store_explicit(&arena->is_destroying, false, memory_order_release);

#ifdef ARENA_ENABLE_THREAD_SAFE
	arena->use_lock = false;
#endif
}

/**
 * @brief
 * Initialize the mutex used for thread-safe arena operations.
 *
 * @details
 * If `ARENA_ENABLE_THREAD_SAFE` is defined, this function sets up a
 * recursive `pthread_mutex_t` for the arena, enabling safe concurrent access
 * to mutable fields like `offset` or marker stack state.
 *
 * It uses `PTHREAD_MUTEX_RECURSIVE` to allow nested locks within the same thread,
 * which is necessary for safe usage patterns like reentrant allocators or hooks.
 *
 * On failure during any part of mutex attribute or lock initialization, it logs
 * a detailed error and returns `false`. The caller is expected to clean up on failure.
 *
 * If thread safety is not enabled at compile time, the function is a no-op that
 * always returns `true`.
 *
 * @param arena Pointer to the arena for which the mutex should be initialized.
 *
 * @return `true` on success, `false` if mutex setup failed.
 *
 * @ingroup arena_internal
 *
 * @see arena_finish_init
 * @see arena_init_with_buffer
 */
static inline bool arena_init_mutex(t_arena* arena)
{
#ifdef ARENA_ENABLE_THREAD_SAFE
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0)
		goto fail;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		goto fail_attr;
	if (pthread_mutex_init(&arena->lock, &attr) != 0)
		goto fail_attr;
	pthread_mutexattr_destroy(&attr);
	arena->use_lock = true;
	return true;
fail_attr:
	pthread_mutexattr_destroy(&attr);
fail:
	arena_report_error(NULL, "arena_create failed: mutex setup failed");
	return false;
#else
	(void) arena;
	return true;
#endif
}

/**
 * @brief
 * Finalize the initialization of an arena with a provided buffer.
 *
 * @details
 * This internal helper is used to complete the setup of an arena
 * after a memory buffer has been successfully allocated.
 *
 * The function performs the following:
 * - Resets the arena's internal state via `arena_reset_metadata`.
 * - Sets the buffer, size, and ownership flag.
 * - Initializes flags for growth and destruction status.
 * - Initializes the mutex (if thread safety is enabled).
 * - Generates a unique debug ID for the arena.
 *
 * If mutex initialization fails, the buffer is freed immediately
 * to avoid memory leaks and the function returns `false`.
 * Ownership of the arena structure itself remains with the caller.
 *
 * @param arena       Pointer to the arena being initialized.
 * @param buffer      Pre-allocated memory buffer for arena use.
 * @param size        Size of the memory buffer in bytes.
 * @param allow_grow  Whether the arena may grow dynamically if full.
 *
 * @return `true` on success, `false` if mutex setup failed.
 *
 * @ingroup arena_internal
 *
 * @see arena_create
 * @see arena_init
 */
static inline bool arena_finish_init(t_arena* arena, void* buffer, size_t size, bool allow_grow)
{
	arena_reset_metadata(arena);
	arena->buffer = (uint8_t*) buffer;
	arena->size   = size;
	atomic_store_explicit(&arena->owns_buffer, true, memory_order_release);
	atomic_store_explicit(&arena->can_grow, allow_grow, memory_order_release);
	atomic_store_explicit(&arena->is_destroying, false, memory_order_release);

	if (!arena_init_mutex(arena))
	{
		free(buffer);
		return false;
	}
	arena_generate_id(arena);
	return true;
}

/**
 * @brief
 * Allocate and assign a zero-initialized buffer to an arena.
 *
 * @details
 * This function is used internally when no user-supplied buffer is provided
 * during `arena_init_with_buffer`. It dynamically allocates a memory region
 * of the requested size using `calloc`, stores the resulting buffer and size
 * into the arena, and marks the arena as the owner of the buffer so it can
 * be freed on destruction.
 *
 * If the memory allocation fails, the function logs a formatted error using
 * `arena_report_error` and returns `false`.
 *
 * @param arena  Pointer to the arena receiving the allocated buffer.
 * @param size   Number of bytes to allocate.
 *
 * @return `true` if allocation and assignment succeeded, `false` on failure.
 *
 * @ingroup arena_internal
 *
 * @see arena_set_or_alloc_buffer
 * @see arena_init_with_buffer
 */
static inline bool arena_set_allocated_buffer(t_arena* arena, size_t size)
{
	void* buffer = calloc(1, size);
	if (!buffer)
	{
		arena_report_error(NULL, "arena_init_with_buffer: malloc for %zu bytes failed", size);
		return false;
	}
	arena->buffer = (uint8_t*) buffer;
	arena->size   = size;
	atomic_store_explicit(&arena->owns_buffer, true, memory_order_release);
	return true;
}

/**
 * @brief
 * Assign a user-provided buffer to an arena without taking ownership.
 *
 * @details
 * This function sets up the arena's buffer and size fields using a buffer
 * provided by the caller. It does not allocate or copy memory — it simply
 * stores the pointer and size, and marks the arena as not owning the buffer.
 * This means the buffer will not be freed when the arena is destroyed.
 *
 * It is typically used when initializing or reinitializing an arena that
 * operates over pre-allocated memory regions, such as stack memory or
 * memory-mapped regions.
 *
 * @param arena   Pointer to the arena being configured.
 * @param buffer  Pointer to the caller-supplied buffer.
 * @param size    Size (in bytes) of the buffer.
 *
 * @ingroup arena_internal
 *
 * @see arena_set_or_alloc_buffer
 * @see arena_init_with_buffer
 */
static inline void arena_set_user_buffer(t_arena* arena, void* buffer, size_t size)
{
	arena->buffer = (uint8_t*) buffer;
	arena->size   = size;
	atomic_store_explicit(&arena->owns_buffer, false, memory_order_release);
}

/**
 * @brief
 * Configure an arena's buffer by either assigning a user-provided region or allocating one internally.
 *
 * @details
 * This helper determines whether to allocate memory or use an existing buffer based
 * on the parameters:
 * - If `buffer` is `NULL` and `size > 0`, it allocates a new zeroed buffer using `calloc`.
 * - Otherwise, it sets the arena's buffer to the provided memory region.
 *
 * In both cases, it updates the `owns_buffer` flag accordingly to ensure proper cleanup
 * behavior in `arena_destroy()`. This function helps unify buffer configuration logic
 * in `arena_init_with_buffer`, reducing duplication and clarifying intent.
 *
 * @param arena   Pointer to the arena to configure.
 * @param buffer  Pointer to the caller-supplied memory buffer (can be `NULL`).
 * @param size    Size (in bytes) of the buffer.
 *
 * @return `true` on success, `false` if memory allocation failed.
 *
 * @ingroup arena_internal
 *
 * @see arena_init_with_buffer
 */
static inline bool arena_set_or_alloc_buffer(t_arena* arena, void* buffer, size_t size)
{
	if (!buffer && size > 0)
		return arena_set_allocated_buffer(arena, size);

	arena_set_user_buffer(arena, buffer, size);
	return true;
}
