
#include "arena.h"
#include "arena_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void test_save_and_load_snapshot(void)
{
	t_arena* arena = arena_create(512, false);
	assert(arena);

	char* msg = arena_alloc(arena, 6);
	strcpy(msg, "hello");

	const char* path = "/tmp/arena_test.snap";
	assert(arena_save_to_file(arena, path));

	t_arena* loaded = arena_create(512, false);
	assert(loaded);

	assert(arena_load_from_file(loaded, path));
	assert(loaded->offset == arena->offset);
	assert(memcmp(loaded->buffer, arena->buffer, arena->offset) == 0);

	unlink(path);
	arena_delete(&arena);
	arena_delete(&loaded);
	printf("✅ test_save_and_load_snapshot passed\n");
}

static void test_save_error_cases(void)
{
	assert(!arena_save_to_file(NULL, "/tmp/x.snap"));
	t_arena dummy = {0};
	assert(!arena_save_to_file(&dummy, NULL));
	atomic_store_explicit(&dummy.owns_buffer, false, memory_order_release);
	assert(!arena_save_to_file(&dummy, "/tmp/x.snap"));
	printf("✅ test_save_error_cases passed\n");
}

static void test_load_error_cases(void)
{
	assert(!arena_load_from_file(NULL, "/tmp/x.snap"));
	t_arena dummy = {0};
	assert(!arena_load_from_file(&dummy, NULL));
	atomic_store_explicit(&dummy.owns_buffer, false, memory_order_release);
	assert(!arena_load_from_file(&dummy, "/tmp/x.snap"));
	printf("✅ test_load_error_cases passed\n");
}

static void test_load_invalid_magic(void)
{
	const char* path = "/tmp/arena_test_invalid.snap";
	FILE*       f    = fopen(path, "wb");
	assert(f);
	fwrite("BADMAGIC", 1, 8, f);
	fclose(f);

	t_arena* arena = arena_create(512, false);
	assert(arena);
	assert(!arena_load_from_file(arena, path));
	unlink(path);
	arena_delete(&arena);
	printf("✅ test_load_invalid_magic passed\n");
}

int main(void)
{
	test_save_and_load_snapshot();
	test_save_error_cases();
	test_load_error_cases();
	test_load_invalid_magic();
	printf("🎉 All arena_io tests passed.\n");
	return 0;
}
