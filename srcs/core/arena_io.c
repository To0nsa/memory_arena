/**
 * @file arena_io.c
 * @author Toonsa
 * @date 2025
 *
 * @brief Arena snapshot serialization and deserialization.
 *
 * @details
 * This file provides I/O utilities for saving and loading arena memory snapshots
 * to and from binary files. It enables serialization of the arena's buffer content
 * for persistence or debugging purposes.
 *
 * Functions included:
 * - `arena_save_to_file()`: Save the current contents of an arena to a `.bin` file.
 * - `arena_load_from_file()`: Load previously saved contents into an arena buffer.
 *
 * Features:
 * - Snapshot format includes a magic header and version check.
 * - Only arenas that own their buffer (`owns_buffer == true`) are supported.
 * - Snapshots contain only the buffer content and current offset (not debug info or stats).
 *
 * This functionality is particularly useful for:
 * - Debugging memory state across runs.
 * - Rehydrating pre-filled arenas for faster loading.
 * - Saving and restoring arena states between application sessions.
 *
 * @note
 * Compatibility is not guaranteed across platforms or architectures due to
 * possible differences in structure alignment or endianness.
 *
 * @ingroup arena_io
 *
 * @example
 * @code
 * #include "arena.h"
 * #include <stdio.h>
 * #include <string.h>
 *
 * int main(void)
 * {
 *     const char* snapshot_path = "snapshot.bin";
 *
 *     // Create and fill an arena
 *     t_arena arena;
 *     if (!arena_init(&arena, 1024, false))
 *         return 1;
 *
 *     char* message = arena_alloc(&arena, 32);
 *     if (!message)
 *     {
 *         arena_destroy(&arena);
 *         return 1;
 *     }
 *     strcpy(message, "Hello, Arena!");
 *
 *     // Save the current arena to disk
 *     if (!arena_save_to_file(&arena, snapshot_path))
 *     {
 *         fprintf(stderr, "Failed to save arena snapshot.\n");
 *         arena_destroy(&arena);
 *         return 1;
 *     }
 *     arena_destroy(&arena); // Clear the arena
 *
 *     // Recreate arena and load from file
 *     if (!arena_init(&arena, 1024, false))
 *         return 1;
 *     if (!arena_load_from_file(&arena, snapshot_path))
 *     {
 *         fprintf(stderr, "Failed to load arena snapshot.\n");
 *         arena_destroy(&arena);
 *         return 1;
 *     }
 *
 *     // Access restored content
 *     printf("Restored message: %s\n", (char*) arena.buffer);
 *
 *     arena_destroy(&arena);
 *     return 0;
 * }
 * @endcode
 */

#include "arena_io.h"
#include "arena_debug.h"
#include <stdio.h>
#include <string.h>

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
bool arena_save_to_file(const t_arena* arena, const char* path)
{
	if (!arena || !path)
		return false;

	if (!atomic_load_explicit(&arena->owns_buffer, memory_order_acquire))
		return false;

	ARENA_LOCK((t_arena*) arena);
	size_t      used = arena->offset;
	const void* buf  = arena->buffer;
	ARENA_UNLOCK((t_arena*) arena);

	t_arena_snapshot_header header = {.magic = ARENA_SNAPSHOT_MAGIC, .version = ARENA_SNAPSHOT_VERSION, .used = used};

	FILE* f = fopen(path, "wb");
	if (!f)
		return false;

	bool ok = fwrite(&header, sizeof(header), 1, f) == 1 && fwrite(buf, 1, used, f) == used;

	fclose(f);
	return ok;
}

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
bool arena_load_from_file(t_arena* arena, const char* path)
{
	if (!arena || !path)
		return false;

	if (!atomic_load_explicit(&arena->owns_buffer, memory_order_acquire))
		return false;

	FILE* f = fopen(path, "rb");
	if (!f)
		return false;

	t_arena_snapshot_header header;
	bool ok = fread(&header, sizeof(header), 1, f) == 1 && strncmp(header.magic, ARENA_SNAPSHOT_MAGIC, 9) == 0 &&
	          header.version == ARENA_SNAPSHOT_VERSION && header.used <= arena->size &&
	          fread(arena->buffer, 1, header.used, f) == header.used;

	if (ok)
		arena->offset = header.used;

	fclose(f);
	return ok;
}
