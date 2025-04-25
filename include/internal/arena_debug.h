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
	 * @brief Assign a debug label to an arena.
	 * @ingroup arena_debug
	 */
	void arena_set_debug_label(t_arena* arena, const char* label);

	/**
	 * @brief Generate a unique debug ID for an arena.
	 * @ingroup arena_debug
	 */
	void arena_generate_id(t_arena* arena);

	/**
	 * @brief Set a custom error callback function for reporting arena errors.
	 * @ingroup arena_debug
	 */
	void arena_set_error_callback(t_arena* arena, arena_error_callback cb, void* context);

	/**
	 * @brief Report an error from an arena using its configured error callback.
	 * @ingroup arena_debug
	 */
	void arena_report_error(t_arena* arena, const char* fmt, ...);

	/**
	 * @brief Default fallback error callback (prints to stderr).
	 * @ingroup arena_debug
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
	 * @brief Fill a memory region with poison values (0xDEADBEEF / 0xEF).
	 * @ingroup arena_debug
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
 * @brief Check arena integrity and report detailed errors if invalid.
 * @ingroup arena_debug
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
