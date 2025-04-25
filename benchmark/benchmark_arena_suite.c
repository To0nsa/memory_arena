
#include "arena.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ALLOC_COUNT 100000
#define ALLOC_SIZE 64
#define THREAD_COUNT 4
#define MAX_PER_THREAD (ALLOC_COUNT / THREAD_COUNT)

double millis(clock_t start, clock_t end)
{
	return 1000.0 * (end - start) / CLOCKS_PER_SEC;
}

// ────────────────────────────── ARENA BENCHMARKS ──────────────────────────────

void benchmark_arena_alloc(void)
{
	t_arena* arena = arena_create(ALLOC_COUNT * ALLOC_SIZE, false);
	if (!arena)
	{
		fprintf(stderr, "Failed to create arena.\n");
		return;
	}

	clock_t start = clock();
	for (int i = 0; i < ALLOC_COUNT; ++i)
	{
		void* ptr = arena_alloc(arena, ALLOC_SIZE);
		memset(ptr, 0xAA, ALLOC_SIZE);
	}
	clock_t end = clock();
	printf("[arena_alloc]     %d x %d bytes: %.2f ms\n", ALLOC_COUNT, ALLOC_SIZE, millis(start, end));

	arena_destroy(arena);
	arena_delete(&arena);
}

void benchmark_arena_calloc(void)
{
	t_arena* arena = arena_create(ALLOC_COUNT * ALLOC_SIZE, false);
	if (!arena)
		return;

	clock_t start = clock();
	for (int i = 0; i < ALLOC_COUNT; ++i)
	{
		void* ptr = arena_calloc(arena, ALLOC_SIZE, 1);
		memset(ptr, 0xBB, ALLOC_SIZE);
	}
	clock_t end = clock();
	printf("[arena_calloc]    %d x %d bytes: %.2f ms\n", ALLOC_COUNT, ALLOC_SIZE, millis(start, end));

	arena_destroy(arena);
	arena_delete(&arena);
}

// ──────────────────────────────── STD BENCHMARKS ───────────────────────────────

void benchmark_malloc_free(void)
{
	clock_t start = clock();
	for (int i = 0; i < ALLOC_COUNT; ++i)
	{
		void* ptr = malloc(ALLOC_SIZE);
		memset(ptr, 0xAA, ALLOC_SIZE);
		free(ptr);
	}
	clock_t end = clock();
	printf("[malloc/free]     %d x %d bytes: %.2f ms\n", ALLOC_COUNT, ALLOC_SIZE, millis(start, end));
}

void benchmark_calloc_free(void)
{
	clock_t start = clock();
	for (int i = 0; i < ALLOC_COUNT; ++i)
	{
		void* ptr = calloc(ALLOC_SIZE, 1);
		memset(ptr, 0xBB, ALLOC_SIZE);
		free(ptr);
	}
	clock_t end = clock();
	printf("[calloc/free]     %d x %d bytes: %.2f ms\n", ALLOC_COUNT, ALLOC_SIZE, millis(start, end));
}

// ────────────────────────────── MULTITHREAD BENCHMARK ──────────────────────────────

typedef struct
{
	t_arena* arena;
	int      thread_id;
} thread_arg_t;

void* arena_alloc_threaded(void* arg)
{
	thread_arg_t* targ = (thread_arg_t*) arg;
	for (int i = 0; i < MAX_PER_THREAD; ++i)
	{
		void* ptr = arena_alloc(targ->arena, ALLOC_SIZE);
		memset(ptr, targ->thread_id, ALLOC_SIZE);
	}
	return NULL;
}

void benchmark_arena_multithreaded(void)
{
	t_arena*     arena = arena_create(ALLOC_COUNT * ALLOC_SIZE, true);
	pthread_t    threads[THREAD_COUNT];
	thread_arg_t args[THREAD_COUNT];

	clock_t start = clock();
	for (int i = 0; i < THREAD_COUNT; ++i)
	{
		args[i].arena     = arena;
		args[i].thread_id = i + 1;
		pthread_create(&threads[i], NULL, arena_alloc_threaded, &args[i]);
	}
	for (int i = 0; i < THREAD_COUNT; ++i)
		pthread_join(threads[i], NULL);
	clock_t end = clock();

	printf("[arena_multithreaded] %d threads × %d allocs: %.2f ms\n", THREAD_COUNT, MAX_PER_THREAD, millis(start, end));

	arena_destroy(arena);
	arena_delete(&arena);
}

// ───────────────────────────────────────────────

int main(void)
{
	printf("🔬 Arena vs malloc/calloc — Allocation Benchmark\n\n");

	benchmark_arena_alloc();
	benchmark_arena_calloc();

	benchmark_malloc_free();
	benchmark_calloc_free();

	printf("\n🔀 Multi-threaded Arena Benchmark\n\n");
	benchmark_arena_multithreaded();

	return 0;
}
