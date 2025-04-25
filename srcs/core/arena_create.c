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

/**
 * @brief
 * Allocate and initialize a dynamic arena with an internal buffer.
 *
 * @details
 * This is the main constructor-style function for creating a new `t_arena` on the heap.
 * It performs both the allocation of the arena **structure** itself and a fresh internal
 * memory buffer of the requested size. It then initializes metadata, locking (if enabled),
 * and a unique ID.
 *
 * Use this when:
 * - You want the arena struct and its buffer to be fully heap-allocated.
 * - You want to manage the arena via a pointer and destroy/free it dynamically.
 * - You prefer a simplified interface without manually supplying memory.
 *
 * It performs:
 * - A check for invalid size (must be non-zero).
 * - A call to `arena_alloc_struct()` to create the arena.
 * - A call to `arena_alloc_buffer()` to allocate zeroed memory.
 * - Initialization via `arena_finish_init()` and setup of a default debug label.
 *
 * If any step fails, the function cleans up and returns `NULL`, with error reporting.
 *
 * @param size        Size (in bytes) of the internal memory buffer.
 * @param allow_grow  Whether the arena is allowed to grow dynamically if needed.
 *
 * @return Pointer to a fully initialized `t_arena`, or `NULL` on failure.
 *
 * @ingroup arena_core
 *
 * @note
 * The returned arena must be destroyed with `arena_destroy()` and freed with `arena_delete()`.
 * Use `arena_init()` instead if you have a statically or stack-allocated arena struct.
 *
 * @see arena_init
 * @see arena_destroy
 * @see arena_delete
 *
 * @example
 * @code
 * t_arena* heap_arena = arena_create(16384, true);
 * if (!heap_arena) {
 *     // handle error
 * }
 * void* ptr = arena_alloc(heap_arena, 512);
 * arena_destroy(heap_arena);
 * arena_delete(&heap_arena);
 * @endcode
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

/**
 * @brief
 * Initialize a pre-allocated arena struct with a newly allocated buffer.
 *
 * @details
 * This function is used when the `t_arena` structure has already been allocated
 * (typically on the stack or statically). It allocates a fresh internal buffer
 * of the given size, and then initializes the arena metadata, mutex (if thread-safe),
 * and assigns a unique ID.
 *
 * Use this when:
 * - You already have a `t_arena` instance allocated (e.g., on the stack).
 * - You want to control the lifetime of the arena struct manually.
 * - You still want the arena to manage its own buffer internally.
 *
 * It performs:
 * - Basic input validation.
 * - Allocation of a zeroed internal buffer using `arena_alloc_buffer()`.
 * - Final setup via `arena_finish_init()`.
 * - Assignment of a default debug label ("arena_stack") if none is set.
 *
 * On failure, errors are reported via `arena_report_error()` and any allocated buffer is freed.
 *
 * @param arena       Pointer to an already-allocated `t_arena` struct.
 * @param size        Size (in bytes) of the memory buffer to allocate.
 * @param allow_grow  Whether the arena is allowed to grow dynamically.
 *
 * @return `true` on success, `false` on failure.
 *
 * @ingroup arena_core
 *
 * @note
 * The memory buffer is owned by the arena and will be freed by `arena_destroy()`.
 * The struct itself is *not* freed—this is the main difference from `arena_create()`.
 *
 * @see arena_create
 * @see arena_destroy
 * @see arena_reset
 *
 * @example
 * @code
 * t_arena my_arena;
 * if (!arena_init(&my_arena, 4096, false)) {
 *     // handle failure
 * }
 * void* mem = arena_alloc(&my_arena, 256);
 * arena_destroy(&my_arena); // Frees buffer, not struct
 * @endcode
 */
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

/**
 * @brief
 * Initialize a pre-allocated arena struct using a user-supplied or dynamically allocated buffer.
 *
 * @details
 * This function is used when the `t_arena` structure has already been allocated
 * (e.g., on the stack or statically), and you want to initialize it using:
 * - a buffer you provide (you retain ownership), or
 * - a buffer that the arena allocates internally (arena takes ownership).
 *
 * Use this when:
 * - You want to supply a pre-existing memory buffer (e.g., stack, static, or pre-mapped memory).
 * - You want to avoid dynamic allocation for the arena struct itself.
 * - You want full control over memory layout or reuse of arena buffers.
 *
 * Behavior:
 * - If `buffer == NULL` and `size > 0`, a zero-initialized buffer is allocated internally.
 *   The arena takes ownership and will free it during `arena_destroy()`.
 * - If `buffer != NULL`, the arena uses the supplied buffer without taking ownership.
 *   You are responsible for freeing it yourself (if dynamically allocated).
 *
 * Initialization steps:
 * - Metadata is reset via `arena_reset_metadata()`.
 * - The `can_grow` flag is stored atomically.
 * - A mutex is initialized if thread safety is enabled.
 * - The buffer is either set directly or allocated internally.
 * - A default debug label is set if none exists.
 * - A unique arena ID is generated.
 *
 * @param arena       Pointer to the `t_arena` structure to initialize.
 * @param buffer      Optional buffer to use for arena memory. Can be `NULL`.
 * @param size        Size of the buffer in bytes. Required if `buffer == NULL`.
 * @param allow_grow  Whether the arena is allowed to grow dynamically.
 *
 * @ingroup arena_init
 *
 * @note
 * If `buffer == NULL`, the arena allocates memory internally and takes ownership of it.
 * If `buffer != NULL`, the arena does **not** take ownership — you must free it manually.
 *
 * @warning
 * Passing `buffer == NULL && size == 0` creates an arena with no usable memory.
 *
 * @see arena_create
 * @see arena_init
 * @see arena_destroy
 *
 * @example
 * @code
 * // Example 1: arena allocates its own buffer
 * t_arena arena;
 * arena_init_with_buffer(&arena, NULL, 8192, false); // Arena owns buffer
 * void* data = arena_alloc(&arena, 128);
 * arena_destroy(&arena); // Frees internal buffer
 *
 * // Example 2: user-provided buffer (you own the buffer)
 * uint8_t external_buf[4096];
 * t_arena arena2;
 * arena_init_with_buffer(&arena2, external_buf, sizeof(external_buf), false);
 * void* ptr = arena_alloc(&arena2, 64);
 * arena_destroy(&arena2); // Does not free external_buf
 *
 * // Example 3: user-allocated buffer (you must free it manually)
 * uint8_t* heap_buf = malloc(4096);
 * arena_init_with_buffer(&arena2, heap_buf, 4096, false);
 * arena_destroy(&arena2); // Arena does NOT free heap_buf
 * free(heap_buf);         // You must free it yourself
 * @endcode
 */
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

/**
 * @brief
 * Destroy and reinitialize an arena using a new buffer.
 *
 * @details
 * This function first calls `arena_destroy()` to release any resources
 * currently owned by the given arena, then reinitializes it in-place
 * with a new buffer (or a newly allocated one if `buffer == NULL`).
 *
 * Use this when:
 * - You want to recycle an existing arena instance with a new memory region.
 * - You want to reset an arena's growth policy or memory layout without allocating a new struct.
 * - You want to reassign ownership of an arena without calling `arena_delete()`.
 *
 * Behavior:
 * - If `buffer == NULL`, a new buffer of `size` bytes is allocated and **owned** by the arena.
 *   It will be freed automatically by `arena_destroy()`.
 * - If `buffer != NULL`, it is used directly without being freed by the arena.
 *   You remain responsible for releasing it manually if it was heap-allocated.
 *
 * @param arena       Pointer to an existing `t_arena` structure to reinitialize.
 * @param buffer      Optional buffer to use for the new memory region. Can be `NULL`.
 * @param size        Size of the buffer in bytes. Must be non-zero if `buffer == NULL`.
 * @param allow_grow  Whether the arena is allowed to grow dynamically.
 *
 * @ingroup arena_init
 *
 * @note
 * If `buffer == NULL`, a new zeroed buffer is allocated and owned by the arena.
 * If `buffer != NULL`, the arena does not take ownership — you must free it manually if needed.
 *
 * @warning
 * Any active pointers to memory previously allocated from the arena become invalid after this call.
 * Ensure no references remain before reinitializing.
 *
 * @see arena_destroy
 * @see arena_init_with_buffer
 *
 * @example
 * @code
 * // Example 1: recycle arena with new internal buffer
 * t_arena arena;
 * arena_init(&arena, 8192, false);
 * arena_reinit_with_buffer(&arena, NULL, 16384, false); // Arena owns new buffer
 *
 * // Example 2: reinitialize with external stack buffer
 * uint8_t buffer[4096];
 * arena_reinit_with_buffer(&arena, buffer, sizeof(buffer), false);
 *
 * // Example 3: reinitialize with heap buffer (manual free required)
 * uint8_t* heap_buffer = malloc(8192);
 * arena_reinit_with_buffer(&arena, heap_buffer, 8192, false);
 * arena_destroy(&arena);
 * free(heap_buffer); // Arena does not free this
 * @endcode
 */
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
