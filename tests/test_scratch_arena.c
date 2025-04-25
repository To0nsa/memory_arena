#include "internal/arena_internal.h"
#include "arena_scratch.h"
#include <assert.h>
#include <stdio.h>

static void test_scratch_pool_init_valid(void)
{
	t_scratch_arena_pool pool;
	bool                 ok = scratch_pool_init(&pool, 256, false);
	assert(ok);

	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		assert(!atomic_load(&pool.slots[i].in_use));
		assert(arena_is_valid(&pool.slots[i].arena));
	}

	scratch_pool_destroy(&pool);
	printf("✅ test_scratch_pool_init_valid passed\n");
}

static void test_scratch_pool_init_invalid(void)
{
	assert(!scratch_pool_init(NULL, 256, false));
	assert(!scratch_pool_init(&(t_scratch_arena_pool){0}, 0, false));
	printf("✅ test_scratch_pool_init_invalid passed\n");
}

static void test_scratch_pool_destroy_clears_state(void)
{
	t_scratch_arena_pool pool;
	scratch_pool_init(&pool, 256, false);
	scratch_pool_destroy(&pool);
	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		assert(pool.slots[i].arena.buffer == NULL);
	}
	printf("✅ test_scratch_pool_destroy_clears_state passed\n");
}

static void test_acquire_and_release(void)
{
	t_scratch_arena_pool pool;
	scratch_pool_init(&pool, 128, false);

	t_arena* acquired[SCRATCH_MAX_SLOTS];
	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		acquired[i] = scratch_acquire(&pool);
		assert(acquired[i] != NULL);
		assert(acquired[i]->offset == 0);
	}

	assert(scratch_acquire(&pool) == NULL);

	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		scratch_release(&pool, acquired[i]);
	}

	for (int i = 0; i < SCRATCH_MAX_SLOTS; ++i)
	{
		t_arena* a = scratch_acquire(&pool);
		assert(a != NULL);
		scratch_release(&pool, a);
	}

	scratch_pool_destroy(&pool);
	printf("✅ test_acquire_and_release passed\n");
}

static void test_edge_cases(void)
{
	scratch_acquire(NULL);
	scratch_release(NULL, NULL);
	t_scratch_arena_pool pool;
	scratch_pool_init(&pool, 64, false);
	scratch_release(&pool, NULL);

	scratch_pool_destroy(&pool);
	printf("✅ test_edge_cases passed\n");
}

int main(void)
{
	test_scratch_pool_init_valid();
	test_scratch_pool_init_invalid();
	test_scratch_pool_destroy_clears_state();
	test_acquire_and_release();
	test_edge_cases();
	printf("🎉 All scratch arena tests passed.\n");
	return 0;
}
