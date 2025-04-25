/**
 * @file arena_groups.h
 * @brief Doxygen group declarations for Arena memory system.
 *
 * @details
 * This header contains only Doxygen `@defgroup` declarations
 * for organizing public and internal API documentation.
 * It is not meant to be included in builds.
 *
 * @note
 * This file is used purely to structure documentation.
 *
 * @author Toonsa
 */

/**
 * @defgroup arena_init Arena Initialization
 * @brief Public functions for creating and initializing arenas.
 */

/**
 * @defgroup arena_init_internal Arena Initialization Internals
 * @brief Internal helpers for arena setup and configuration.
 * @ingroup arena_internal
 */

/**
 * @defgroup arena_cleanup Arena Cleanup
 * @brief Public functions for destroying and deleting arenas.
 */

/**
 * @defgroup arena_cleanup_internal Arena Cleanup Internals
 * @brief Internal helpers for teardown and memory release.
 * @ingroup arena_internal
 */

/**
 * @defgroup arena_alloc Arena Allocation
 * @brief Public functions for memory allocation from an arena.
 */

/**
 * @defgroup arena_alloc_internal Arena Allocation Internals
 * @brief Internal helpers for memory allocation logic.
 * @ingroup arena_internal
 */

/**
 * @defgroup arena_sub Sub-Arena Allocation
 * @brief Public functions for creating and managing sub-arenas.
 *
 * @details
 * Sub-arenas are memory arenas that are backed by allocations from a parent arena.
 * These functions allow modular memory segmentation without extra heap allocation.
 */

/**
 * @defgroup arena_sub_internal Sub-Arena Internals
 * @brief Internal helpers for configuring sub-arenas.
 * @ingroup arena_internal
 */

/**
 * @defgroup arena_resize Arena Resize
 * @brief Public functions for dynamic arena resizing.
 *
 * @details
 * These functions handle memory growth and optional shrinking of an arena's
 * buffer, providing adaptive memory behavior for changing workloads.
 */

/**
 * @defgroup arena_resize_internal Arena Resize Internals
 * @brief Internal helpers for growing and shrinking memory buffers.
 * @ingroup arena_internal
 */

/**
 * @defgroup arena_scratch Scratch Arena Pool
 * @brief Public API for acquiring and releasing temporary memory arenas from a pool.
 *
 * @details
 * Scratch arenas are useful for high-frequency temporary allocations in performance-critical paths.
 * These APIs support fast, reusable memory arenas from a preallocated pool with optional thread safety.
 */

/**
 * @defgroup arena_scratch_internal Scratch Arena Internals
 * @brief Internal helpers and utilities for scratch arena pool management.
 * @ingroup arena_internal
 */

/**
 * @defgroup arena_state Arena State Management
 * @brief Functions for inspecting and manipulating arena usage state.
 *
 * @details
 * These functions provide insight into memory usage (`used`, `remaining`, `peak`)
 * and allow for scoped rollback (`mark`, `pop`) and reset operations.
 */

/**
 * @defgroup arena_tlscratch Thread-Local Scratch Arena
 * @brief Lightweight, thread-local memory arenas for fast temporary allocations.
 *
 * @details
 * This group provides functions for accessing a private scratch arena that
 * is unique per thread. These arenas are lazily initialized and automatically
 * reset on each access to ensure clean, reusable memory without synchronization.
 *
 * Use cases include:
 * - Temporary string/structure building in multithreaded applications
 * - Replacing `alloca()` or short-lived heap allocations
 * - Avoiding mutex contention in high-performance code
 *
 * The thread-local scratch arena system is enabled via the `ARENA_ENABLE_THREAD_LOCAL_SCRATCH` macro.
 *
 * @note
 * This system is independent of the global `scratch_arena_pool`.
 * It is **not** safe to share the returned arena between threads.
 */

/**
 * @defgroup arena_debug Debugging and Validation
 * @brief Functions and tools for tracking, labeling, poisoning, and validating arenas.
 *
 * @details
 * This group includes optional tools to enhance runtime diagnostics:
 * - Custom error callbacks
 * - Unique arena IDs and labels
 * - Memory poisoning (e.g. `0xDEADBEEF`)
 * - Arena integrity validation for development and testing
 *
 * These are primarily intended for debug builds and are configurable via macros like `ARENA_DEBUG_CHECKS`.
 */

/**
 * @defgroup arena_stats Arena Statistics
 * @brief Functions for recording, retrieving, and printing memory usage statistics.
 *
 * @details
 * This group provides introspection into arena usage patterns:
 * - Number of allocations and reallocations
 * - Total bytes allocated and wasted
 * - Peak usage, growth history, and current state
 *
 * Useful for profiling, visualization, or custom allocator analytics.
 */

/**
 * @defgroup arena_internal Internal Implementation
 * @brief Base group for internal implementation details.
 *
 * @details
 * This group collects all static inline functions, helpers,
 * and low-level logic used internally by the arena system.
 * These are not part of the public API and may change at any time.
 */
