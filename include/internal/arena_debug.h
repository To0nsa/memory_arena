/**
 * @file arena_debug.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Debugging utilities and hooks for the arena memory allocator.
 *
 * @details
 * This header defines debug-related features for arena memory systems, including:
 * - Debug labels and unique arena identifiers.
 * - Custom error reporting via callbacks.
 * - Optional memory poisoning to detect use-after-reset/pop.
 * - Internal consistency checks with assertions.
 * - Logging macros for diagnostics.
 *
 * These features are controlled via compile-time flags in `arena_config.h`:
 * - `ARENA_DEBUG_LOG`
 * - `ARENA_POISON_MEMORY`
 * - `ARENA_DEBUG_CHECKS`
 *
 * This module is optional and designed to aid development, debugging,
 * diagnostics, and testing of arena-based systems.
 *
 * @ingroup arena_debug
 */

#ifndef ARENA_DEBUG_H
#define ARENA_DEBUG_H

#include "arena_config.h"
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

	// ─────────────────────────────────────────────────────────────
	// Error Reporting & Debug Metadata
	// ─────────────────────────────────────────────────────────────

	/**
	 * @brief Arena structure forward declaration.
	 */
	typedef struct s_arena t_arena;

	/**
	 * @brief Type for error callback functions.
	 *
	 * @param message The error message string.
	 * @param context Optional user-defined context pointer.
	 */
	typedef void (*arena_error_callback)(const char* message, void* context);

	/**
	 * @brief
	 * Debug metadata associated with an arena.
	 */
	typedef struct s_arena_debug
	{
		char                 id[ARENA_ID_LEN]; ///< Unique identifier string (e.g., "A#0001").
		const char*          label;            ///< Optional user-provided label for logging/debug.
		arena_error_callback error_cb;         ///< Callback function for reporting errors.
		void*                error_context;    ///< Optional context passed to the error callback.
		atomic_int           subarena_counter; ///< Internal counter for sub-arenas.
	} t_arena_debug;

	/**
	 * @brief
	 * Assign a human-readable label to an arena for debugging and introspection.
	 *
	 * @details
	 * This function sets the `debug.label` field of the specified arena to the given
	 * string. The label can later be used for:
	 * - Logging and error messages
	 * - Visualizers or diagnostics tools
	 * - Identifying specific arenas during profiling or debug output
	 *
	 * The function does not copy or allocate memory for the label; it simply stores
	 * the pointer. Make sure the label string remains valid for the lifetime of the arena.
	 *
	 * @param arena Pointer to the arena to label.
	 * @param label Null-terminated string to assign as the arena’s label.
	 *
	 * @return void
	 *
	 * @ingroup arena_debug
	 *
	 * @note
	 * If `arena` is `NULL`, the function does nothing.
	 * If `label` is `NULL`, any existing label is cleared.
	 *
	 * @see arena_generate_id
	 * @see arena_report_error
	 *
	 * @example
	 * @code
	 * t_arena* arena = arena_create(4096, true);
	 * arena_set_debug_label(arena, "Frame Temp Arena");
	 *
	 * void* data = arena_alloc(arena, 256);
	 * // On error, logs may show: [ARENA ERROR] (Frame Temp Arena) ...
	 * @endcode
	 */
	void arena_set_debug_label(t_arena* arena, const char* label);

	/**
	 * @brief
	 * Generate and assign a unique identifier string for an arena.
	 *
	 * @details
	 * This function sets the `debug.id` field of the specified arena with a unique,
	 * human-readable string identifier in the format `"A#XXXX"`, where `XXXX` is a
	 * zero-padded incremental number.
	 *
	 * This ID can be used for debugging, profiling, or visualizing multiple arena
	 * instances, especially in tools that track memory usage across components.
	 *
	 * Internally, the function uses an atomic counter (`g_arena_id_counter`) to ensure
	 * that each arena receives a unique ID, even in multithreaded environments.
	 *
	 * @param arena Pointer to the `t_arena` instance to assign an ID to.
	 *
	 * @return void
	 *
	 * @ingroup arena_debug
	 *
	 * @note
	 * If the arena is `NULL`, the function does nothing.
	 *
	 * @see arena_set_debug_label
	 * @see g_arena_id_counter
	 *
	 * @example
	 * @code
	 * t_arena* arena = arena_create(1024, true);
	 * arena_generate_id(arena);
	 * printf("Arena ID: %s\n", arena->debug.id); // Output: A#0001, A#0002, ...
	 * @endcode
	 */
	void arena_generate_id(t_arena* arena);

	/**
	 * @brief
	 * Set a custom error callback for reporting arena-related errors.
	 *
	 * @details
	 * This function assigns a user-defined callback to be invoked whenever the arena
	 * encounters an internal error (e.g., invalid marker, buffer overflow, or corruption).
	 *
	 * The callback receives a formatted error message and an optional user-provided context
	 * pointer, which can be used for logging, diagnostics, or integration with custom tools.
	 *
	 * If `cb` is `NULL`, the arena reverts to using `arena_default_error_callback()`, which
	 * prints messages to `stderr`. The `context` parameter is ignored in that case.
	 *
	 * @param arena   Pointer to the arena to configure.
	 * @param cb      Function pointer to a custom error handler, or `NULL` to restore default.
	 * @param context Optional pointer passed to the error callback on invocation.
	 *
	 * @return void
	 *
	 * @ingroup arena_debug
	 *
	 * @note
	 * This function does nothing if `arena` is `NULL`.
	 *
	 * @warning
	 * The callback must not attempt to allocate from the same arena,
	 * or it may cause recursion or deadlocks.
	 *
	 * @see arena_report_error
	 * @see arena_default_error_callback
	 *
	 * @example
	 * @code
	 * void my_error_logger(const char* msg, void* ctx) {
	 *     FILE* log = (FILE*)ctx;
	 *     fprintf(log, "ARENA ERROR: %s\n", msg);
	 * }
	 *
	 * t_arena* arena = arena_create(1024, true);
	 * FILE* logfile = fopen("arena_errors.log", "w");
	 * arena_set_error_callback(arena, my_error_logger, logfile);
	 *
	 * // Misuse that will trigger error
	 * arena_pop(arena, 9999);  // Logs to arena_errors.log
	 * fclose(logfile);
	 * @endcode
	 */
	void arena_set_error_callback(t_arena* arena, arena_error_callback cb, void* context);

	/**
	 * @brief
	 * Report an arena-related error using the configured error callback or fallback to stderr.
	 *
	 * @details
	 * This function formats an error message using `printf`-style arguments and dispatches it
	 * to the arena's configured error handler (`arena->debug.error_cb`). If no custom handler
	 * is set, it prints the message to `stderr` with a prefix and optional debug label.
	 *
	 * The error message is limited to 512 characters and includes contextual metadata
	 * if available (e.g., debug label).
	 *
	 * This is used internally by various arena functions to signal misuse, invalid states,
	 * or configuration issues (e.g., invalid markers, size mismatches, buffer overflows).
	 *
	 * @param arena Pointer to the arena reporting the error. Can be `NULL`.
	 * @param fmt   `printf`-style format string describing the error.
	 * @param ...   Additional arguments corresponding to the format string.
	 *
	 * @return void
	 *
	 * @ingroup arena_debug
	 *
	 * @note
	 * This function is safe to call with a `NULL` arena. In that case, the error is printed
	 * to `stderr` using the default formatting.
	 *
	 * @warning
	 * The message is not dynamically allocated. Ensure the final output fits within 512 bytes.
	 *
	 * @see arena_set_error_callback
	 * @see arena_default_error_callback
	 *
	 * @example
	 * @code
	 * t_arena* arena = arena_create(256, true);
	 * if (!arena)
	 *     arena_report_error(NULL, "Failed to create arena: out of memory");
	 *
	 * // Triggering a validation error manually
	 * arena_report_error(arena, "Custom debug message: offset=%zu, size=%zu", arena->offset, arena->size);
	 * @endcode
	 */
	void arena_report_error(t_arena* arena, const char* fmt, ...);

	/**
	 * @brief
	 * Default error handler for arena-related errors.
	 *
	 * @details
	 * This function is used as the fallback error callback when no user-defined
	 * handler is installed via `arena_set_error_callback()`. It simply prints
	 * the error message to `stderr` prefixed with `[ARENA ERROR]`.
	 *
	 * This ensures that all critical failures or misuses in the arena system
	 * are visible even when no custom logging or error-reporting system is configured.
	 *
	 * @param msg  The error message string to display.
	 * @param ctx  Unused context pointer (included for compatibility with callback signature).
	 *
	 * @return void
	 *
	 * @ingroup arena_debug
	 *
	 * @note
	 * This function is safe to call from any thread. The output is directed
	 * to standard error without formatting beyond the prefix.
	 *
	 * @see arena_set_error_callback
	 * @see arena_report_error
	 *
	 * @example
	 * @code
	 * // Let the arena system use the default error handler:
	 * arena_set_error_callback(my_arena, NULL, NULL);
	 *
	 * // This will invoke arena_default_error_callback internally:
	 * arena_report_error(my_arena, "Something went wrong!");
	 * @endcode
	 */
	void arena_default_error_callback(const char* msg, void* ctx);

// ─────────────────────────────────────────────────────────────
// Memory Poisoning
// ─────────────────────────────────────────────────────────────

/**
 * @def ARENA_POISON_PATTERN
 * @brief Poison value used when overwriting memory in debug mode.
 */
#define ARENA_POISON_PATTERN 0xDEADBEEF

#ifdef ARENA_POISON_MEMORY
	/**
	 * @brief
	 * Overwrite memory with a poison pattern to detect use-after-free or invalid access.
	 *
	 * @details
	 * This function is used in debug builds to overwrite a memory region with a known
	 * invalid pattern (`0xDEADBEEF` for full words, `0xEF` for remaining bytes).
	 * This helps detect improper memory use after a region has been released or reset
	 * by making bugs more visible during debugging or testing (e.g., via sanitizer tools).
	 *
	 * - The memory is split into 32-bit words.
	 * - Full words are filled with `ARENA_POISON_PATTERN` (`0xDEADBEEF`).
	 * - Remaining bytes are filled with `0xEF`.
	 *
	 * This function is conditionally compiled only when `ARENA_POISON_MEMORY` is defined.
	 * In production builds, it is typically disabled for performance.
	 *
	 * @param ptr   Pointer to the memory region to poison.
	 * @param size  Number of bytes to overwrite.
	 *
	 * @return void
	 *
	 * @ingroup arena_debug
	 *
	 * @note
	 * This is a no-op in release builds unless `ARENA_POISON_MEMORY` is defined.
	 *
	 * @see ARENA_POISON_PATTERN
	 * @see arena_reset
	 * @see arena_pop
	 *
	 * @example
	 * @code
	 * void* temp = arena_alloc(arena, 128);
	 * // Use temp...
	 * arena_poison_memory(temp, 128); // Fill memory with poison for safety
	 * @endcode
	 */
	void arena_poison_memory(void* ptr, size_t size);
#else
/**
 * @brief No-op poison macro when poisoning is disabled.
 */
#define arena_poison_memory(ptr, size) ((void) (ptr), (void) (size))
#endif

	// ─────────────────────────────────────────────────────────────
	// Debug Assertions and Internal Checks
	// ─────────────────────────────────────────────────────────────

#ifdef ARENA_DEBUG_CHECKS
#include <assert.h>

/**
 * @brief Perform an assertion.
 */
#define ARENA_ASSERT(cond) assert(cond)

/**
 * @brief
 * Perform an internal consistency check on an arena.
 *
 * @details
 * This function validates the internal state of an arena and reports detailed
 * debug errors if inconsistencies are detected. It is designed to be used
 * in debug builds when `ARENA_DEBUG_CHECKS` is enabled.
 *
 * The following checks are performed:
 * - `arena != NULL`
 * - If the arena has a size > 0, the `buffer` must not be `NULL`.
 * - `offset` must not exceed `size`.
 * - `peak_usage` must not exceed `size`.
 * - `reallocations` must not exceed `allocations`.
 * - `offset` must not exceed `peak_usage`.
 * - `wasted_alignment_bytes` must not exceed `bytes_allocated`.
 * - If `growth_history_count > 0`, the `growth_history` pointer must not be `NULL`.
 *
 * In multithreaded configurations (`ARENA_ENABLE_THREAD_SAFE`), the function:
 * - Returns early if the arena is being destroyed or locking is disabled.
 * - Attempts a non-blocking lock (`pthread_mutex_trylock`) to avoid deadlocks.
 *
 * If any issue is found, it reports a detailed error via `arena_report_error`,
 * including the file, line, and function from which it was called.
 *
 * @param arena Pointer to the arena to validate.
 * @param file  Source filename (usually `__FILE__`).
 * @param line  Source line number (usually `__LINE__`).
 * @param func  Function name (usually `__func__`).
 *
 * @return void
 *
 * @ingroup arena_debug
 *
 * @note
 * This function should be invoked via the `ARENA_CHECK(arena)` macro,
 * which automatically fills in file/line/func information.
 *
 * @warning
 * This is a debug utility and should not be used in performance-critical code.
 *
 * @see arena_report_error
 * @see ARENA_CHECK
 * @see ARENA_ASSERT_VALID
 *
 * @example
 * @code
 * void some_function(t_arena* arena) {
 *     ARENA_CHECK(arena); // Expands to a call to arena_integrity_check(...)
 *     // Proceed with safe operations...
 * }
 * @endcode
 */
#define ARENA_CHECK(arena) arena_integrity_check((arena), __FILE__, __LINE__, __func__)

/**
 * @brief Assert that the arena is valid.
 * @ingroup arena_debug
 */
#define ARENA_ASSERT_VALID(arena) ARENA_ASSERT(arena_is_valid(arena))

	/**
	 * @brief Implementation of `ARENA_CHECK`: validate internal arena state.
	 * @ingroup arena_debug
	 */
	void arena_integrity_check(t_arena* arena, const char* file, int line, const char* func);

#else
#define ARENA_ASSERT(cond) ((void) 0)
#define ARENA_CHECK(arena) ((void) 0)
#define ARENA_ASSERT_VALID(arena) ((void) 0)
#endif

// ─────────────────────────────────────────────────────────────
// Logging
// ─────────────────────────────────────────────────────────────

/**
 * @def ALOG
 * @brief Debug logging macro (enabled only if `ARENA_DEBUG_LOG` is set).
 */
#if ARENA_DEBUG_LOG
#define ALOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define ALOG(...) ((void) 0)
#endif

#ifdef __cplusplus
}
#endif

#endif // ARENA_DEBUG_H
