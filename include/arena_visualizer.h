#ifndef ARENA_VISUALIZER_H
#define ARENA_VISUALIZER_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "arena.h"
#include "arena_hooks.h"

#define MAX_HISTORY 200
#define MAX_LABEL_LEN 128

#ifdef __cplusplus
extern "C"
{
#endif

	struct notcurses;
	struct ncplane;

	/**
	 * Each ring-buffer event: error, step, normal or sub-alloc
	 */
	typedef struct s_arena_history_entry
	{
		bool   is_sub;
		bool   is_step;
		bool   is_error;
		size_t usage;
		size_t size;
		size_t offset;
		char   label[MAX_LABEL_LEN];
	} t_arena_history_entry;

	/**
	 * The main interactive visualizer
	 */
	typedef struct s_arena_visualizer
	{
		t_arena*        arena;
		pthread_mutex_t lock;

		struct notcurses* nc;
		struct ncplane*   main_plane;

		t_arena_history_entry history[MAX_HISTORY];
		atomic_size_t         history_index;
		atomic_size_t         history_count;

		// scrolling offset for ring buffer events
		int scroll_offset;
		// if the user wants to skip the scenario script
		bool script_done;
	} t_arena_visualizer;

	int  wait_for_key(t_arena_visualizer* vis);
	void draw_visualizer(t_arena_visualizer* vis);
	/**
	 * The ring buffer hooks
	 */
	void arena_visualizer_allocation_hook(t_arena* arena, int id, void* ptr, size_t size, size_t offset, size_t wasted,
	                                      const char* label);

	void arena_visualizer_error_callback(const char* msg, void* context);
	void step_message(t_arena_visualizer* vis, const char* msg);

	/** Attach our allocation hook to an arena */
	void arena_visualizer_enable_history_hook(t_arena_visualizer* vis, t_arena* arena);

	/**
	 * Start the interactive session (blocks until user quits).
	 * The user can scroll the ring buffer with arrow keys, press 'n' to do next step,
	 * press 'q' to quit, etc.
	 */
	void arena_start_interactive_visualizer(t_arena_visualizer* vis, t_arena* arena);

#ifdef __cplusplus
}
#endif

#endif // ARENA_VISUALIZER_H
