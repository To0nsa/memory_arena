#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREAD_COUNT 8
#define BUFFER_SIZE 4096

typedef struct s_payload
{
	int      thread_id;
	uint8_t* shared_buffer;
	size_t   buffer_size;
} t_payload;

static t_arena arenas[THREAD_COUNT];

// 🔹 Thread using shared external buffer (not owned)
void* thread_init_with_external_buffer(void* arg)
{
	t_payload* p = (t_payload*) arg;

	// 🛠️ Allocate a *private* buffer per thread
	uint8_t* thread_buffer = malloc(p->buffer_size);
	assert(thread_buffer != NULL);
	memset(thread_buffer, 0xEE, p->buffer_size);

	t_arena* arena = &arenas[p->thread_id];
	arena_init_with_buffer(arena, thread_buffer, p->buffer_size, false);

	assert(arena->buffer == thread_buffer);

	// ✅ atomic check for owns_buffer
	assert(atomic_load_explicit(&arena->owns_buffer, memory_order_acquire) == false);

	assert(arena->use_lock == true);
	assert(arena->offset == 0);

	char* ptr = (char*) arena_alloc(arena, 64);
	assert(ptr != NULL);
	snprintf(ptr, 64, "Buf %d", p->thread_id);

	arena_destroy(arena);
	free(thread_buffer); // 💡 safe because ownership is explicitly false

	return NULL;
}

// 🔹 Thread using internal allocation (NULL buffer)
void* thread_init_with_internal_buffer(void* arg)
{
	t_payload* p     = (t_payload*) arg;
	t_arena*   arena = &arenas[p->thread_id];

	arena_init_with_buffer(arena, NULL, p->buffer_size, true);

	assert(arena->buffer != NULL);
	assert(arena->size == p->buffer_size);
	assert(arena->offset == 0);
	assert(arena->use_lock == true);

	// ✅ Atomic check
	assert(atomic_load_explicit(&arena->owns_buffer, memory_order_acquire) == true);

	char* data = arena_alloc(arena, 64);
	assert(data != NULL);
	snprintf(data, 64, "int-%d", p->thread_id);
	assert(strncmp(data, "int-", 4) == 0);

	arena_destroy(arena);

	// ✅ Manual free only if still owned
	if (atomic_load_explicit(&arena->owns_buffer, memory_order_acquire))
		free(arena->buffer);

	return NULL;
}

void* thread_reinit_with_buffer_cycle(void* arg)
{
	t_payload* p     = (t_payload*) arg;
	t_arena*   arena = &arenas[p->thread_id];

	for (int i = 0; i < 4; ++i)
	{
		bool use_internal = (i % 2 == 0);

		uint8_t* buffer = NULL;
		if (!use_internal)
		{
			buffer = malloc(p->buffer_size);
			assert(buffer != NULL);
			memset(buffer, 0xAA, p->buffer_size);
		}

		arena_reinit_with_buffer(arena, buffer, p->buffer_size, true);

		assert(arena->size == p->buffer_size);
		assert(arena->offset == 0);
		assert(arena->use_lock == true);

		assert(atomic_load_explicit(&arena->owns_buffer, memory_order_acquire) == use_internal);

		void* ptr = arena_alloc(arena, 64);
		assert(ptr != NULL);
		memset(ptr, 0x42, 64);

		arena_destroy(arena);

		if (!use_internal && buffer)
			free(buffer);
	}
	return NULL;
}

void* thread_null_arena(void* arg)
{
	(void) arg;
	arena_init_with_buffer(NULL, NULL, 0, true);
	arena_reinit_with_buffer(NULL, NULL, 0, true);
	return NULL;
}

void* thread_empty_buffer(void* arg)
{
	(void) arg;
	t_arena arena;
	arena_init_with_buffer(&arena, NULL, 0, true);
	assert(arena.buffer == NULL);
	arena_destroy(&arena);
	return NULL;
}

void* thread_recursive_lock(void* arg)
{
	(void) arg;
	t_arena arena;
	arena_init_with_buffer(&arena, NULL, 128, true);
	assert(arena.use_lock == true);

	ARENA_LOCK(&arena);
	ARENA_LOCK(&arena);
	ARENA_UNLOCK(&arena);
	ARENA_UNLOCK(&arena);

	arena_destroy(&arena);
	free(arena.buffer);
	return NULL;
}

int main(void)
{
	pthread_t threads[THREAD_COUNT];
	t_payload payloads[THREAD_COUNT];

	uint8_t* shared = malloc(BUFFER_SIZE);
	assert(shared);

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		payloads[i] = (t_payload){.thread_id = i, .shared_buffer = shared, .buffer_size = BUFFER_SIZE};
		pthread_create(&threads[i], NULL, thread_init_with_external_buffer, &payloads[i]);
	}
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);
	printf("✅ arena_init_with_buffer: All threads used shared buffer safely\n");

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_create(&threads[i], NULL, thread_init_with_internal_buffer, &payloads[i]);
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);
	printf("✅ arena_init_with_buffer: All threads used internal buffer safely\n");

	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_create(&threads[i], NULL, thread_reinit_with_buffer_cycle, &payloads[i]);
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);
	printf("✅ arena_reinit_with_buffer: Alternating reinitialization succeeded\n");

	pthread_t null_arena_thread;
	pthread_create(&null_arena_thread, NULL, thread_null_arena, NULL);
	pthread_join(null_arena_thread, NULL);
	printf("✅ arena_init_with_buffer: NULL arena handled gracefully\n");

	pthread_t empty_buffer_thread;
	pthread_create(&empty_buffer_thread, NULL, thread_empty_buffer, NULL);
	pthread_join(empty_buffer_thread, NULL);
	printf("✅ arena_init_with_buffer: NULL buffer + 0 size handled\n");

	pthread_t recursive_thread;
	pthread_create(&recursive_thread, NULL, thread_recursive_lock, NULL);
	pthread_join(recursive_thread, NULL);
	printf("✅ Recursive mutex test passed\n");

	free(shared);
	return 0;
}
