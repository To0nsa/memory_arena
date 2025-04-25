
#include "arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_generate_id_unique(void)
{
	t_arena a1, a2;
	memset(&a1, 0, sizeof(a1));
	memset(&a2, 0, sizeof(a2));

	arena_generate_id(&a1);
	arena_generate_id(&a2);

	assert(strncmp(a1.debug.id, "A#", 2) == 0);
	assert(strncmp(a2.debug.id, "A#", 2) == 0);
	assert(strcmp(a1.debug.id, a2.debug.id) != 0);
	printf("✅ test_generate_id_unique passed\n");
}

static void test_set_debug_label(void)
{
	t_arena a;
	memset(&a, 0, sizeof(a));
	arena_set_debug_label(&a, "my_label");
	assert(strcmp(a.debug.label, "my_label") == 0);

	arena_set_debug_label(NULL, "should not crash");
	arena_set_debug_label(&a, NULL);
	assert(a.debug.label == NULL);
	printf("✅ test_set_debug_label passed\n");
}

static char g_last_error[512];
static void mock_error_cb(const char* msg, void* ctx)
{
	(void) ctx;
	strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
}

static void test_error_callback(void)
{
	t_arena a;
	memset(&a, 0, sizeof(a));

	arena_set_error_callback(&a, mock_error_cb, NULL);
	arena_report_error(&a, "error %d", 42);
	assert(strstr(g_last_error, "error 42"));
	printf("✅ test_error_callback passed\n");

	arena_set_error_callback(&a, NULL, NULL);
	assert(a.debug.error_cb == arena_default_error_callback);
	printf("✅ test_error_callback_reset passed\n");
}

static void test_default_error_print(void)
{
	// This test just ensures arena_report_error doesn't crash
	t_arena a = {0};
	arena_set_debug_label(&a, "test_label");
	arena_report_error(&a, "some message");
	printf("✅ test_default_error_print passed (manual stderr check)\n");
}

#ifdef ARENA_POISON_MEMORY
static void test_poison_memory(void)
{
	uint8_t buffer[9];
	memset(buffer, 0, sizeof(buffer));
	arena_poison_memory(buffer, sizeof(buffer));
	uint32_t* w = (uint32_t*) buffer;
	assert(w[0] == ARENA_POISON_PATTERN);
	assert(buffer[8] == 0xEF);
	printf("✅ test_poison_memory passed\n");
}
#endif

#ifdef ARENA_DEBUG_CHECKS
static void test_arena_integrity_check(void)
{
	t_arena arena = {0};
	arena_init(&arena, 64, false);
	arena.offset = 100;

	memset(g_last_error, 0, sizeof(g_last_error));
	arena_set_error_callback(&arena, mock_error_cb, NULL);

	arena_integrity_check(&arena, __FILE__, __LINE__, __func__);

	arena_destroy(&arena);
	printf("✅ test_arena_integrity_check passed\n");
}
#endif

int main(void)
{
	test_generate_id_unique();
	test_set_debug_label();
	test_error_callback();
	test_default_error_print();
#ifdef ARENA_POISON_MEMORY
	test_poison_memory();
#endif
#ifdef ARENA_DEBUG_CHECKS
	test_arena_integrity_check();
#endif
	printf("🎉 All arena_debug tests passed.\n");
	return 0;
}
