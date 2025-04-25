/**
 * @file arena_io.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Arena snapshot input/output operations (save/load).
 *
 * @details
 * This header declares functions to serialize and deserialize the memory contents
 * of an arena. These operations allow arenas to be saved to disk and restored later,
 * enabling features such as state persistence, debugging snapshots, or offline replay.
 *
 * The snapshot includes a compact header (`t_arena_snapshot_header`) followed by the
 * raw used memory buffer of the arena.
 *
 * - `arena_save_to_file()` writes the arena's used memory to a file.
 * - `arena_load_from_file()` restores the memory contents from a file into an existing arena.
 *
 * @note
 * These functions are meant for single-threaded or externally synchronized use.
 * They assume the arena is not being mutated during I/O.
 *
 * @ingroup arena_io
 */

#ifndef ARENA_IO_H
#define ARENA_IO_H

#include "arena.h"
#include <stdbool.h>

/// Magic string identifying a valid arena snapshot file.
#define ARENA_SNAPSHOT_MAGIC "ARENASNAP"

/// Version number of the snapshot file format.
#define ARENA_SNAPSHOT_VERSION 1

/**
 * @brief
 * Header structure prepended to saved arena snapshots.
 *
 * @details
 * This packed structure contains metadata about a saved arena buffer, including:
 * - A magic string for format verification.
 * - A version field for compatibility checks.
 * - The number of bytes used in the saved arena.
 *
 * This header is written/read before the actual memory contents of the arena.
 *
 * @ingroup arena_io
 */
typedef struct s_arena_snapshot_header
{
	char     magic[9]; ///< Magic string ("ARENASNAP\0")
	uint32_t version;  ///< Snapshot file format version
	size_t   used;     ///< Number of bytes used in the arena buffer
} __attribute__((packed)) t_arena_snapshot_header;

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief
	 * Save the contents of an arena to a binary file.
	 *
	 * @details
	 * This function writes a snapshot of the arena’s current state to the file
	 * specified by `path`. The snapshot includes:
	 * - A fixed header with a magic identifier, version, and used size.
	 * - The active contents of the arena buffer up to `arena->offset`.
	 *
	 * Only arenas that **own** their buffer can be saved. If the arena does not own
	 * its buffer (e.g., sub-arenas), the function returns `false` without writing.
	 *
	 * The output file format consists of:
	 * 1. A `t_arena_snapshot_header` struct.
	 * 2. Raw buffer bytes up to the used offset.
	 *
	 * This function acquires the arena lock to ensure thread-safe access to the buffer.
	 *
	 * @param arena Pointer to the arena to serialize.
	 * @param path  Filesystem path to write the snapshot file.
	 *
	 * @return `true` on success, or `false` on failure.
	 *
	 * @ingroup arena_io
	 *
	 * @note
	 * This function only saves the memory content and usage, not the full arena state
	 * (e.g., labels, parent references, or hooks).
	 *
	 * @see arena_load_from_file
	 *
	 * @example
	 * @code
	 * #include "arena.h"
	 * #include <stdio.h>
	 *
	 * int main(void)
	 * {
	 *     t_arena arena;
	 *     if (!arena_init(&arena, 1024, true))
	 *     {
	 *         fprintf(stderr, "Failed to initialize arena.\n");
	 *         return 1;
	 *     }
	 *
	 *     // Allocate some data
	 *     int* numbers = (int*) arena_alloc(&arena, 10 * sizeof(int));
	 *     if (!numbers)
	 *     {
	 *         fprintf(stderr, "Allocation failed.\n");
	 *         arena_destroy(&arena);
	 *         return 1;
	 *     }
	 *     for (int i = 0; i < 10; ++i)
	 *         numbers[i] = i * 10;
	 *
	 *     // Save the arena state to a file
	 *     if (!arena_save_to_file(&arena, "snapshot.bin"))
	 *     {
	 *         fprintf(stderr, "Failed to save arena to file.\n");
	 *     }
	 *     else
	 *     {
	 *         printf("Arena snapshot saved to snapshot.bin\n");
	 *     }
	 *
	 *     arena_destroy(&arena);
	 *     return 0;
	 * }
	 * @endcode
	 */
	bool arena_save_to_file(const t_arena* arena, const char* path);

	/**
	 * @brief
	 * Load arena memory contents from a binary snapshot file.
	 *
	 * @details
	 * This function restores a previously saved arena memory region from a file
	 * created with `arena_save_to_file()`. It only restores the raw buffer content
	 * and the current `offset`, not the full internal state (e.g., labels, hooks, stats).
	 *
	 * The file must contain a valid `t_arena_snapshot_header` followed by raw memory
	 * data. The header is validated using a magic string and version number.
	 *
	 * This function performs:
	 * - Basic argument validation (`arena` and `path` must be non-NULL).
	 * - Ownership check (only arenas that own their buffer can be restored).
	 * - File open and header verification (magic string and version).
	 * - Safe size checks to prevent buffer overflows.
	 * - Memory restoration using `fread()`.
	 * - `arena->offset` is updated only on success.
	 *
	 * @param arena Pointer to the `t_arena` structure to populate.
	 * @param path  Path to the snapshot file.
	 *
	 * @return `true` if the file was successfully loaded, `false` otherwise.
	 *
	 * @ingroup arena_internal
	 *
	 * @note
	 * This does **not** restore debug labels, allocation stats, hooks, or metadata.
	 * Only arenas with internally-owned buffers are supported (`owns_buffer == true`).
	 *
	 * @warning
	 * If the snapshot file was generated on a different platform or architecture,
	 * compatibility is not guaranteed.
	 *
	 * @see arena_save_to_file
	 * @see t_arena_snapshot_header
	 *
	 * @example
	 * @code
	 * #include "arena.h"
	 * #include <stdio.h>
	 *
	 * int main(void)
	 * {
	 *     t_arena arena;
	 *     if (!arena_init(&arena, 1024, false))
	 *     {
	 *         fprintf(stderr, "Failed to initialize arena.\n");
	 *         return 1;
	 *     }
	 *
	 *     // Load a previously saved snapshot
	 *     if (!arena_load_from_file(&arena, "snapshot.bin"))
	 *     {
	 *         fprintf(stderr, "Failed to load arena from file.\n");
	 *         arena_destroy(&arena);
	 *         return 1;
	 *     }
	 *
	 *     // Access memory loaded from snapshot
	 *     int* numbers = (int*) arena.buffer;
	 *     printf("First value: %d\n", numbers[0]);  // Should print whatever was saved
	 *
	 *     arena_destroy(&arena);
	 *     return 0;
	 * }
	 * @endcode
	 */
	bool arena_load_from_file(t_arena* arena, const char* path);

#ifdef __cplusplus
}
#endif

#endif // ARENA_IO_H
