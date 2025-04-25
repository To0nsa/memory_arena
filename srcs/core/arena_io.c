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
