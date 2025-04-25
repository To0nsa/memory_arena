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
	 * Save the current contents of an arena to a file.
	 *
	 * @details
	 * This function writes a snapshot of the arena to the specified file path.
	 * The snapshot includes a header and all memory up to `arena->offset`.
	 *
	 * @param arena Pointer to the arena to save.
	 * @param path  Path to the output file.
	 * @return `true` on success, `false` on failure.
	 *
	 * @ingroup arena_io
	 */
	bool arena_save_to_file(const t_arena* arena, const char* path);

	/**
	 * @brief
	 * Load arena contents from a snapshot file.
	 *
	 * @details
	 * This function reads a snapshot file created with `arena_save_to_file()`
	 * and writes the contents into the provided arena buffer.
	 *
	 * The arena must have been initialized with a buffer large enough to hold
	 * the restored data. The arena's internal offset will be updated accordingly.
	 *
	 * @param arena Pointer to the arena to restore.
	 * @param path  Path to the snapshot file.
	 * @return `true` on success, `false` on failure.
	 *
	 * @ingroup arena_io
	 */
	bool arena_load_from_file(t_arena* arena, const char* path);

#ifdef __cplusplus
}
#endif

#endif // ARENA_IO_H
