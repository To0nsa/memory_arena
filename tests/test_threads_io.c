#include "arena.h"
#include "arena_io.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 8
#define SNAPSHOT_PREFIX "snapshot_thread_"
#define SNAPSHOT_FILE "snapshot_shared.bin"
#define BUFFER_SIZE 4096
#define TEST_STRING "HelloArenaSnapshot"

typedef struct
{
	t_arena* arena;
	int      thread_id;
	char     filename[64];
} t_save_payload;

typedef struct
{
	t_arena arena;
	int     thread_id;
} t_load_payload;

static void fill_arena(t_arena* arena)
{
	char* data = (char*) arena_alloc(arena, strlen(TEST_STRING) + 1);
	strcpy(data, TEST_STRING);
}

void* thread_save_to_file(void* arg)
{
	t_save_payload* p = (t_save_payload*) arg;
	assert(arena_save_to_file(p->arena, p->filename));
	return NULL;
}

void* thread_load_from_file(void* arg)
{
	t_load_payload* p     = (t_load_payload*) arg;
	t_arena*        arena = &p->arena;
	assert(arena_init(arena, BUFFER_SIZE, false));
	assert(arena_load_from_file(arena, SNAPSHOT_FILE));

	const char* data = (const char*) arena->buffer;
	assert(strcmp(data, TEST_STRING) == 0);

	arena_destroy(arena);
	return NULL;
}

void* thread_fault_cases(void* arg)
{
	(void) arg;

	assert(arena_save_to_file(NULL, "dummy") == false);
	assert(arena_save_to_file((t_arena*) 1, NULL) == false);

	t_arena dummy = {0};
	atomic_store_explicit(&dummy.owns_buffer, false, memory_order_relaxed);
	assert(arena_save_to_file(&dummy, "dummy") == false);

	assert(arena_load_from_file(NULL, "dummy") == false);
	assert(arena_load_from_file((t_arena*) 1, NULL) == false);
	assert(arena_load_from_file(&dummy, "dummy") == false);

	return NULL;
}

void create_corrupt_file(const char* path)
{
	FILE* f = fopen(path, "wb");
	fwrite("BADMAGIC", 1, 8, f);
	fclose(f);
}

void* thread_load_corrupt()
{
	t_arena arena;
	assert(arena_init(&arena, BUFFER_SIZE, false));
	assert(arena_load_from_file(&arena, "corrupt.bin") == false);
	arena_destroy(&arena);
	return NULL;
}

int main(void)
{
	t_arena* arena = arena_create(BUFFER_SIZE, false);
	assert(arena);
	fill_arena(arena);
	assert(arena_save_to_file(arena, SNAPSHOT_FILE));

	pthread_t      save_threads[THREAD_COUNT];
	t_save_payload payloads[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		snprintf(payloads[i].filename, sizeof(payloads[i].filename), SNAPSHOT_PREFIX "%d.bin", i);
		payloads[i].arena     = arena;
		payloads[i].thread_id = i;
		pthread_create(&save_threads[i], NULL, thread_save_to_file, &payloads[i]);
	}
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(save_threads[i], NULL);

	for (int i = 1; i < THREAD_COUNT; ++i)
	{
		FILE* f1 = fopen(payloads[0].filename, "rb");
		FILE* f2 = fopen(payloads[i].filename, "rb");

		assert(f1 && f2);
		char   buf1[BUFFER_SIZE], buf2[BUFFER_SIZE];
		size_t n1 = fread(buf1, 1, BUFFER_SIZE, f1);
		size_t n2 = fread(buf2, 1, BUFFER_SIZE, f2);
		assert(n1 == n2);
		assert(memcmp(buf1, buf2, n1) == 0);

		fclose(f1);
		fclose(f2);
	}
	printf("✅ Concurrent arena_save_to_file passed\n");

	pthread_t      load_threads[THREAD_COUNT];
	t_load_payload load_payloads[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
		load_payloads[i].thread_id = i;

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_create(&load_threads[i], NULL, thread_load_from_file, &load_payloads[i]);

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(load_threads[i], NULL);

	printf("✅ Concurrent arena_load_from_file passed\n");

	pthread_t fault, corrupt;
	create_corrupt_file("corrupt.bin");

	pthread_create(&fault, NULL, thread_fault_cases, NULL);
	pthread_create(&corrupt, NULL, thread_load_corrupt, NULL);
	pthread_join(fault, NULL);
	pthread_join(corrupt, NULL);

	printf("✅ Fault injection tests passed\n");

	arena_destroy(arena);
	arena_delete(&arena);

	unlink(SNAPSHOT_FILE);
	unlink("corrupt.bin");
	for (int i = 0; i < THREAD_COUNT; ++i)
		unlink(payloads[i].filename);

	return 0;
}
