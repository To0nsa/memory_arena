#include "arena.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 16
#define SUBARENA_SIZE 128
#define LOOP_COUNT 1000

typedef struct
{
	t_arena* parent;
	int      index;
	int      successful_allocs;
} ThreadContext;

void* thread_alloc_sub(void* arg)
{
	ThreadContext* ctx    = (ThreadContext*) arg;
	t_arena*       parent = ctx->parent;
	char           label[32];
	snprintf(label, sizeof(label), "thread_%d", ctx->index);

	for (int i = 0; i < LOOP_COUNT; ++i)
	{
		t_arena child;
		memset(&child, 0, sizeof(child));

		if (i % 2 == 0)
		{
			if (!arena_alloc_sub(parent, &child, SUBARENA_SIZE))
				continue;
		}
		else
		{
			if (!arena_alloc_sub_labeled(parent, &child, SUBARENA_SIZE, label))
				continue;
		}

		assert(child.buffer != NULL);
		assert(child.size == SUBARENA_SIZE);
		assert(child.offset == 0);
		assert(child.parent_ref == parent);
		assert(child.debug.label != NULL);
		assert(strncmp(child.debug.label, "subarena", 8) == 0 || strstr(child.debug.label, "thread_") != NULL);

		memset(child.buffer, ctx->index, SUBARENA_SIZE);
		ctx->successful_allocs++;
	}

	return NULL;
}

void test_multithreaded_subarena_alloc()
{
	t_arena* parent = arena_create(THREAD_COUNT * LOOP_COUNT * SUBARENA_SIZE, true);
	assert(parent);
	parent->use_lock = true;

	pthread_t     threads[THREAD_COUNT];
	ThreadContext ctxs[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		ctxs[i].parent            = parent;
		ctxs[i].index             = i;
		ctxs[i].successful_allocs = 0;
		pthread_create(&threads[i], NULL, thread_alloc_sub, &ctxs[i]);
	}

	int total_success = 0;
	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		pthread_join(threads[i], NULL);
		total_success += ctxs[i].successful_allocs;
	}

	printf("✅ test_multithreaded_subarena_alloc passed: %d successful subarena allocs\n", total_success);
	assert(parent->stats.allocations >= (size_t) total_success);

	arena_destroy(parent);
	free(parent);
}

int main(void)
{
	test_multithreaded_subarena_alloc();
	printf("🎉 All multithreaded subarena tests passed.\n");
	return 0;
}
