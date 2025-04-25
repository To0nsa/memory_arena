#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 16
#define BUFFER_SIZE 4096

typedef struct s_payload
{
	t_arena* arena;
	int      thread_id;
	bool     use_stack_arena;
} t_payload;

static t_arena stack_arenas[THREAD_COUNT];

void* thread_create_arena(void* arg)
{
	t_payload* p = (t_payload*) arg;

	t_arena* arena = arena_create(8192, true);
	assert(arena != NULL);

	char* data = (char*) arena_alloc(arena, 64);
	assert(data != NULL);

	snprintf(data, 64, "Thread %d", p->thread_id);
	char expected[64];
	snprintf(expected, 64, "Thread %d", p->thread_id);
	assert(strcmp(data, expected) == 0);

	arena_destroy(arena);
	arena_delete(&arena);
	return NULL;
}

void* thread_stack_arena_init(void* arg)
{
	t_payload* p     = (t_payload*) arg;
	t_arena*   arena = &stack_arenas[p->thread_id];

	assert(arena_init(arena, BUFFER_SIZE, true) == true);

	char* data = (char*) arena_alloc(arena, 64);
	assert(data != NULL);

	snprintf(data, 64, "Init %d", p->thread_id);

	char expected[64];
	snprintf(expected, 64, "Init %d", p->thread_id);

	assert(strcmp(data, expected) == 0);

	arena_destroy(arena);
	return NULL;
}

void* thread_invalid_create(void* arg)
{
	(void) arg;
	t_arena* a = arena_create(0, false);
	assert(a == NULL);
	return NULL;
}

void* thread_recursive_mutex()
{
	t_arena* arena = arena_create(1024, false);
	assert(arena != NULL);

	ARENA_LOCK(arena);
	ARENA_LOCK(arena);
	ARENA_UNLOCK(arena);
	ARENA_UNLOCK(arena);

	arena_destroy(arena);
	arena_delete(&arena);
	return NULL;
}

int main(void)
{
	pthread_t threads[THREAD_COUNT];
	t_payload payloads[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		payloads[i] = (t_payload){.arena = NULL, .thread_id = i};
		pthread_create(&threads[i], NULL, thread_create_arena, &payloads[i]);
	}
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);
	printf("✅ arena_create: All threads created and destroyed arenas successfully\n");

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		pthread_create(&threads[i], NULL, thread_stack_arena_init, &payloads[i]);
	}
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);
	printf("✅ arena_init: All stack arenas initialized and destroyed safely\n");

	pthread_t fail_thread;
	pthread_create(&fail_thread, NULL, thread_invalid_create, NULL);
	pthread_join(fail_thread, NULL);
	printf("✅ Failure injection: arena_create(0, false) handled gracefully\n");

	pthread_t recursive_thread;
	pthread_create(&recursive_thread, NULL, thread_recursive_mutex, &payloads[0]);
	pthread_join(recursive_thread, NULL);
	printf("✅ Recursive mutex: nested locks succeeded on one thread\n");

	return 0;
}
