/**
 * @file arena_sub.c
 * @author Toonsa
 * @date 2025
 * @brief
 * Sub-arena allocation and management using memory carved from parent arenas.
 *
 * @details
 * This module provides functions to create sub-arenas within a parent arena's
 * memory buffer. A sub-arena allows for scoped or modular memory management
 * without allocating additional heap memory.
 *
 * All sub-arenas are backed by memory allocated from their parent arena.
 * They do not own this memory and must not attempt to free it. Instead, sub-arenas
 * should be cleaned up via `arena_destroy()`, while ownership and actual memory
 * cleanup is delegated to the parent arena.
 *
 * Features:
 * - Default, aligned, labeled, and aligned-labeled sub-arena allocation.
 * - Debug ID and label assignment for each sub-arena.
 * - Shared lifetime: child arenas must not outlive their parent arena.
 *
 * Internal helpers are provided for validating inputs, setting up sub-arena metadata,
 * and generating debug identifiers for tracking and debugging.
 *
 * @note
 * Sub-arenas are suitable for managing short-lived scopes (e.g., temporary stacks,
 * frame allocators, scratch spaces) within a larger allocation region.
 *
 * Sub-arenas are ideal when you want to:
 * - Divide a larger arena into logical, reusable regions.
 * - Provide isolated memory scopes for different modules or systems.
 * - Allocate many related objects (e.g., UI widgets, ECS components) and reset them as a group.
 * - Avoid fragmentation and minimize heap allocations.
 * - Implement hierarchical memory lifetimes (parent outlives children).
 *
 * Use sub-arenas when:
 * - You need efficient memory partitioning without extra malloc calls.
 * - You want fast, structured cleanup of a block of allocations.
 * - You're implementing systems with nested or modular resource lifetimes.
 *
 * Sub-arenas are **not** suitable for:
 * - Use across threads without synchronization.
 * - Independent lifetimes beyond their parent.
 * - Scenarios requiring ownership of their backing memory (use scratch arena for that).
 *
 * @ingroup arena_sub
 *
 * @example
 * @brief
 * Using sub-arenas to isolate per-frame allocations in a game engine.
 *
 * @details
 * In this example, we use a main arena to hold long-lived memory (like game state),
 * and a sub-arena for temporary, per-frame allocations (e.g., AI buffers, particle systems,
 * collision scratch space, etc.). The sub-arena is reset every frame using `arena_destroy()`,
 * avoiding individual frees and reducing fragmentation.
 *
 * @code
 * #include "arena.h"
 * #include <stdio.h>
 *
 * typedef struct s_game_state
 * {
 *     float* player_position;
 *     int*   world_map;
 * } t_game_state;
 *
 * void simulate_frame(t_arena* frame_arena)
 * {
 *     // Allocate some per-frame temporary buffers
 *     float* temp_physics_buffer = arena_alloc(frame_arena, sizeof(float) * 1024);
 *     int*   visibility_map      = arena_calloc(frame_arena, 512, sizeof(int));
 *
 *     if (!temp_physics_buffer || !visibility_map)
 *     {
 *         fprintf(stderr, "Frame allocation failed.\n");
 *         return;
 *     }
 *
 *     // Simulate things...
 *     printf("Simulating frame with temp buffer at %p\n", (void*) temp_physics_buffer);
 * }
 *
 * int main(void)
 * {
 *     // Create the main arena for long-lived data
 *     t_arena* game_arena = arena_create(8192, true);
 *     if (!game_arena)
 *     {
 *         fprintf(stderr, "Failed to create main arena.\n");
 *         return 1;
 *     }
 *
 *     // Allocate persistent game state from the main arena
 *     t_game_state* state = arena_alloc(game_arena, sizeof(t_game_state));
 *     state->player_position = arena_alloc(game_arena, sizeof(float) * 3);
 *     state->world_map       = arena_alloc(game_arena, sizeof(int) * 1024);
 *
 *     // Simulate multiple frames
 *     for (int i = 0; i < 5; ++i)
 *     {
 *         t_arena frame_arena;
 *         if (!arena_sub_init_label(&frame_arena, game_arena, 4096, "frame_arena"))
 *         {
 *             fprintf(stderr, "Sub-arena creation failed on frame %d\n", i);
 *             break;
 *         }
 *
 *         simulate_frame(&frame_arena);
 *         arena_destroy(&frame_arena); // Free all per-frame memory at once
 *     }
 *
 *     arena_delete(&game_arena);
 *     return 0;
 * }
 * @endcode
 */

#include "arena.h"
#include <stdio.h>

/*
 * INTERNAL HELPERS DECLARATIONS
 */

static inline bool arena_alloc_sub_validate(t_arena* parent, t_arena* child);
static inline void arena_setup_subarena(t_arena* parent, t_arena* child, void* buffer, size_t size, const char* label);
static inline void arena_generate_subarena_id(t_arena* parent, t_arena* child);

/*
 * PUBLIC API
 */

bool arena_alloc_sub(t_arena* parent, t_arena* child, size_t size)
{
	return arena_alloc_sub_labeled(parent, child, size, "subarena");
}

bool arena_alloc_sub_aligned(t_arena* parent, t_arena* child, size_t size, size_t alignment)
{
	return arena_alloc_sub_labeled_aligned(parent, child, size, alignment, "subarena");
}

bool arena_alloc_sub_labeled(t_arena* parent, t_arena* child, size_t size, const char* label)
{
	return arena_alloc_sub_labeled_aligned(parent, child, size, ARENA_DEFAULT_ALIGNMENT, label);
}

bool arena_alloc_sub_labeled_aligned(t_arena* parent, t_arena* child, size_t size, size_t alignment, const char* label)
{
	if (!arena_alloc_sub_validate(parent, child))
		return false;

	void* mem = arena_alloc_aligned(parent, size, alignment);
	if (!mem)
	{
		arena_report_error(parent, "arena_alloc_sub failed: allocation from parent arena failed");
		return false;
	}

	arena_setup_subarena(parent, child, mem, size, label);

	ALOG("[arena_alloc_sub] Created sub-arena (%s) of %zu bytes from %p → %p\n", child->debug.label, size,
	     (void*) parent, (void*) child);

	return true;
}

/*
 * INTERNAL HELPERS
 */

/**
 * @brief
 * Validate the input parameters for creating a sub-arena.
 *
 * @details
 * This internal function ensures that both `parent` and `child` arena pointers
 * are valid before proceeding with a sub-arena allocation. It performs:
 * - A null check on the parent and child pointers.
 * - A basic sanity check on the `parent` pointer's address to ensure it isn't
 *   accidentally an invalid pointer (e.g., very low memory address).
 *
 * If validation fails, an appropriate error is reported via `arena_report_error()`.
 *
 * @param parent Pointer to the parent arena from which memory will be allocated.
 * @param child  Pointer to the arena struct that will be initialized as a sub-arena.
 *
 * @return `true` if both parameters are valid, `false` otherwise.
 *
 * @ingroup arena_sub_internal
 *
 * @see arena_alloc_sub
 * @see arena_setup_subarena
 */
static inline bool arena_alloc_sub_validate(t_arena* parent, t_arena* child)
{
	if (!parent || (uintptr_t) parent < 0x1000)
	{
		arena_report_error(NULL, "arena_alloc_sub failed: invalid parent arena");
		return false;
	}
	if (!child)
	{
		arena_report_error(NULL, "arena_alloc_sub failed: NULL child");
		return false;
	}
	return true;
}

/**
 * @brief
 * Initialize a child arena as a sub-arena of a parent using a provided memory block.
 *
 * @details
 * This internal helper prepares a `child` arena to operate as a sub-arena carved from
 * its `parent`. It performs the following steps:
 * - Initializes the `child` using `arena_init_with_buffer()` with the given `buffer` and `size`.
 * - Marks the buffer as not owned by the child (ownership remains with the parent).
 * - Sets a reference to the parent arena in `child->parent_ref`.
 * - Generates a unique debug ID for the child based on its parent.
 * - Sets a debug label for identification (defaults to `"subarena"` if `label == NULL`).
 * - Validates the state of both arenas using `ARENA_CHECK()`.
 *
 * This function assumes that the memory buffer was already allocated from the parent arena.
 *
 * @param parent Pointer to the parent arena.
 * @param child  Pointer to the arena to be configured as a sub-arena.
 * @param buffer Memory block allocated from the parent to back the child arena.
 * @param size   Size of the memory block in bytes.
 * @param label  Optional debug label for the sub-arena. Can be `NULL`.
 *
 * @ingroup arena_sub_internal
 *
 * @note
 * This function does not allocate memory. It only sets up the `child` to use memory
 * previously allocated from the `parent`.
 *
 * @see arena_alloc_sub_labeled_aligned
 * @see arena_init_with_buffer
 * @see arena_generate_subarena_id
 */
static inline void arena_setup_subarena(t_arena* parent, t_arena* child, void* buffer, size_t size, const char* label)
{
	arena_init_with_buffer(child, buffer, size, false);
	atomic_store_explicit(&child->owns_buffer, false, memory_order_release);
	child->parent_ref = parent;
	arena_generate_subarena_id(parent, child);
	arena_set_debug_label(child, label ? label : "subarena");

	ARENA_CHECK(parent);
	ARENA_CHECK(child);
}

/**
 * @brief
 * Generate a unique debug identifier for a sub-arena.
 *
 * @details
 * This internal helper constructs a human-readable ID string for a child (sub-arena),
 * based on its parent arena's ID. The generated ID takes the form:
 *
 *     `<PREF>.<N>`
 *
 * Where `<PREF>` is the first four characters of the parent arena's ID and `<N>` is an
 * incrementing counter (subarena index). This ID is stored in `child->debug.id` and
 * can be used in logs, debug tools, or allocation tracking.
 *
 * The parent's subarena counter is incremented after each call.
 *
 * @param parent Pointer to the parent arena.
 * @param child  Pointer to the child arena to assign the ID.
 *
 * @ingroup arena_sub_internal
 *
 * @note
 * The child’s ID is truncated to `ARENA_ID_LEN` if the formatted string exceeds the limit.
 *
 * @see arena_setup_subarena
 * @see arena_alloc_sub_labeled_aligned
 */
static inline void arena_generate_subarena_id(t_arena* parent, t_arena* child)
{
	int sub_id = parent->debug.subarena_counter++;
	snprintf(child->debug.id, ARENA_ID_LEN, "%.4s.%d", parent->debug.id, sub_id);
}
