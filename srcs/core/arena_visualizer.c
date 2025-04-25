#include "arena_visualizer.h"
#include "arena.h"
#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void draw_bar_to_plane(struct ncplane* plane, unsigned y, const char* label, size_t value, size_t max, int width)
{
	int filled = max > 0 ? (int) ((double) value / max * width) : 0;
	filled     = filled > width ? width : filled;

	ncplane_printf_yx(plane, y, 0, "%-8s [", label);
	ncplane_set_fg_rgb(plane, 0x00FF00);
	for (int i = 0; i < width; ++i)
		ncplane_putstr_yx(plane, y, 10 + i, i < filled ? "█" : "░");
	ncplane_set_fg_default(plane);
	ncplane_printf_yx(plane, y, 11 + width, "] %zu / %zu", value, max);
}

static void vis_record_event(t_arena_visualizer* vis, bool is_sub, bool is_step, bool is_error, size_t usage,
                             size_t size, size_t offset, const char* msg)
{
	if (!vis)
		return;

	size_t                 idx = atomic_fetch_add(&vis->history_index, 1) % MAX_HISTORY;
	t_arena_history_entry* e   = &vis->history[idx];
	e->is_sub                  = is_sub;
	e->is_step                 = is_step;
	e->is_error                = is_error;

	e->usage  = usage;
	e->size   = size;
	e->offset = offset;

	snprintf(e->label, MAX_LABEL_LEN, "%s", (msg ? msg : "(unnamed)"));

	size_t old_count = atomic_load(&vis->history_count);
	if (old_count < MAX_HISTORY)
	{
		atomic_fetch_add(&vis->history_count, 1);
	}
}

void arena_visualizer_allocation_hook(t_arena* arena, int id, void* ptr, size_t size, size_t offset, size_t wasted,
                                      const char* label)
{
	(void) id;
	(void) ptr;
	(void) wasted;
	if (!arena)
		return;

	t_arena_visualizer* vis = (t_arena_visualizer*) arena->hooks.context;
	if (!vis)
		return;

	bool   is_sub = (arena->parent_ref != NULL);
	size_t usage  = arena_used(arena);

	vis_record_event(vis, is_sub, false, false, usage, size, offset, label);
}

void arena_visualizer_error_callback(const char* msg, void* context)
{
	t_arena_visualizer* vis = (t_arena_visualizer*) context;
	if (!vis)
	{
		// fallback
		fprintf(stderr, "[ARENA ERROR] %s\n", msg);
		return;
	}
	vis_record_event(vis, false, false, true, 0, 0, 0, msg);
}

void step_message(t_arena_visualizer* vis, const char* msg)
{
	if (!vis)
	{
		// fallback
		fprintf(stderr, "STEP: %s\n", msg);
		return;
	}
	vis_record_event(vis, false, true, false, 0, 0, 0, msg);
}

void draw_visualizer(t_arena_visualizer* vis)
{
	if (!vis->nc || !vis->main_plane || !vis->arena)
		return;

	ncplane_erase(vis->main_plane);

	unsigned y = 0;
	int      rows, cols;
	ncplane_dim_yx(vis->main_plane, (unsigned*) &rows, (unsigned*) &cols);

	// Header
	ncplane_set_fg_rgb(vis->main_plane, 0xFFD700); // Gold
	ncplane_printf_yx(vis->main_plane, y++, (cols - 36) / 2, "=== 🚀 Interactive Arena Visualizer 🚀 ===");

	ncplane_set_fg_default(vis->main_plane);
	y++;

	// Arena Info
	ncplane_printf_yx(vis->main_plane, y++, 0, "Arena Label: %s",
	                  vis->arena->debug.label ? vis->arena->debug.label : "Unnamed");
	y++;

	// Usage bars
	const int bar_width = cols - 25;
	size_t    used      = arena_used(vis->arena);
	size_t    total     = vis->arena->size;
	size_t    remain    = arena_remaining(vis->arena);
	size_t    peak      = arena_peak(vis->arena);

	draw_bar_to_plane(vis->main_plane, y++, "Used", used, total, bar_width);
	draw_bar_to_plane(vis->main_plane, y++, "Remain", remain, total, bar_width);
	draw_bar_to_plane(vis->main_plane, y++, "Peak", peak, total, bar_width);

	y++;

	// Arena Stats
	ncplane_set_fg_rgb(vis->main_plane, 0xADD8E6); // Light Blue
	ncplane_printf_yx(vis->main_plane, y++, 0, "--- Arena Stats ---");
	ncplane_set_fg_default(vis->main_plane);

	ncplane_printf_yx(vis->main_plane, y++, 0, "Allocations:       %zu", vis->arena->stats.allocations);
	ncplane_printf_yx(vis->main_plane, y++, 0, "Reallocations:     %zu", vis->arena->stats.reallocations);
	ncplane_printf_yx(vis->main_plane, y++, 0, "Total Allocated:   %zu bytes", vis->arena->stats.bytes_allocated);
	ncplane_printf_yx(vis->main_plane, y++, 0, "Alignment Waste:   %zu bytes",
	                  vis->arena->stats.wasted_alignment_bytes);
	ncplane_printf_yx(vis->main_plane, y++, 0, "Live Allocations:  %zu", vis->arena->stats.live_allocations);
	y++;

	// Events Header
	ncplane_set_fg_rgb(vis->main_plane, 0x90EE90); // Light green
	ncplane_printf_yx(vis->main_plane, y++, 0, "--- Events (Oldest ➡️ Newest) ---");
	ncplane_set_fg_default(vis->main_plane);

	// Ring buffer rendering with smooth scrolling
	size_t count = atomic_load(&vis->history_count);
	size_t idx   = atomic_load(&vis->history_index);
	size_t hist  = (count < MAX_HISTORY) ? count : MAX_HISTORY;

	int lines_for_ring = rows - y - 2;
	if (lines_for_ring < 1)
		lines_for_ring = 1;
	int max_offset     = (int) hist - lines_for_ring;
	max_offset         = max_offset < 0 ? 0 : max_offset;
	vis->scroll_offset = vis->scroll_offset < 0 ? 0 : vis->scroll_offset;
	vis->scroll_offset = vis->scroll_offset > max_offset ? max_offset : vis->scroll_offset;

	int start = (int) hist - lines_for_ring - vis->scroll_offset;
	start     = start < 0 ? 0 : start;

	for (int i = start; i < (int) hist; i++)
	{
		t_arena_history_entry* e = &vis->history[(idx + i + (MAX_HISTORY - hist)) % MAX_HISTORY];

		if (y >= (unsigned) rows - 1)
			break;

		uint64_t color = e->is_error ? 0xFF4500 : e->is_step ? 0x00BFFF : e->is_sub ? 0x32CD32 : 0xFFFFFF;

		ncplane_set_fg_rgb(vis->main_plane, color);

		if (e->is_error)
			ncplane_printf_yx(vis->main_plane, y++, 0, "🔥 ERROR: %s", e->label);
		else if (e->is_step)
			ncplane_printf_yx(vis->main_plane, y++, 0, "🚩 STEP: %s", e->label);
		else if (e->is_sub)
			ncplane_printf_yx(vis->main_plane, y++, 0, "🧩 SUB: usage=%zu, size=%zu, offset=%zu [%s]", e->usage,
			                  e->size, e->offset, e->label);
		else
			ncplane_printf_yx(vis->main_plane, y++, 0, "📌 usage=%zu, size=%zu, offset=%zu [%s]", e->usage, e->size,
			                  e->offset, e->label);
	}

	// Footer with helpful instructions
	ncplane_set_fg_rgb(vis->main_plane, 0xFFD700);
	ncplane_printf_yx(vis->main_plane, rows - 1, 0, "📖 Arrows: Scroll ⬆️⬇️ | 'n': Next step ▶️ | 'q': Quit ❌\n");

	ncplane_set_fg_default(vis->main_plane);
	notcurses_render(vis->nc);
}

/**
 * function that blocks reading keystrokes:
 * - UP arrow => scroll_offset++
 * - DOWN arrow => scroll_offset--
 * - 'n' => next step => return 1
 * - 'q' => quit => return 0
 * Otherwise keep waiting
 */
int wait_for_key(t_arena_visualizer* vis)
{
	while (1)
	{
		draw_visualizer(vis);
		uint32_t key = notcurses_get_blocking(vis->nc, NULL);
		switch (key)
		{
		case 'q':
		case 'Q':
		case NCKEY_ESC:
			return 0; // user wants to quit
		case NCKEY_UP:
			vis->scroll_offset++;
			break;
		case NCKEY_DOWN:
			vis->scroll_offset--;
			break;
		case 'n':
		case 'N':
			return 1; // next step
		default:
			// do nothing
			break;
		}
	}
}

/**
 * The main interactive loop:
 * 1) user sees ring buffer
 * 2) user presses 'n' to run next script step
 * 3) user presses 'q' any time => done
 */
static void interactive_loop(t_arena_visualizer* vis)
{
	// We won't do an internal scenario script here — we assume user code calls
	// "step_message" + "alloc" from main. If you want the script integrated,
	// you can do it here. But let's just show how to do indefinite steps:

	while (1)
	{
		int ret = wait_for_key(vis);
		if (!ret)
		{ // user pressed q => done
			break;
		}
		// user pressed n => return to caller so the next step in main can run
		return;
	}
}

void arena_visualizer_enable_history_hook(t_arena_visualizer* vis, t_arena* arena)
{
	if (!vis || !arena)
		return;
	arena_set_allocation_hook(arena, arena_visualizer_allocation_hook, vis);
}

void arena_start_interactive_visualizer(t_arena_visualizer* vis, t_arena* arena)
{
	if (!vis || !arena)
		return;

	struct notcurses_options opts = {
	    .termtype = NULL,
	    .loglevel = NCLOGLEVEL_SILENT,
	};

	vis->nc = notcurses_init(&opts, NULL);
	if (!vis->nc)
	{
		fprintf(stderr, "[Visualizer] Failed to init notcurses\n");
		return;
	}

	vis->main_plane = notcurses_stdplane(vis->nc);
	pthread_mutex_init(&vis->lock, NULL);

	atomic_store(&vis->history_index, 0);
	atomic_store(&vis->history_count, 0);

	vis->scroll_offset = 0;
	vis->script_done   = false;
	vis->arena         = arena;

	// We won't do an infinite loop here. We'll just do a single call
	// to interactive_loop so user can press 'n' or 'q'
	// Then we return control to main. So main can do partial steps, come back, etc.

	interactive_loop(vis);
	// user pressed q or n => we return
}
