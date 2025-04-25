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
bool arena_alloc_sub(t_arena* parent, t_arena* child, size_t size)
{
	return arena_alloc_sub_labeled(parent, child, size, "subarena");
}

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
bool arena_alloc_sub_aligned(t_arena* parent, t_arena* child, size_t size, size_t alignment)
{
	return arena_alloc_sub_labeled_aligned(parent, child, size, alignment, "subarena");
}

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
bool arena_alloc_sub_labeled(t_arena* parent, t_arena* child, size_t size, const char* label)
{
	return arena_alloc_sub_labeled_aligned(parent, child, size, ARENA_DEFAULT_ALIGNMENT, label);
}

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
