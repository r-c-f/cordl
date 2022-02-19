/* cursutil -- useful routines for working with curses
 *
 * Version 1.4
 *
 * Copyright 2022 Ryan Farley <ryan.farley@gmx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#pragma once
#include <curses.h>
#include <stdarg.h>

/* Clear a border created by box() from a window */
#define unbox(w) wborder(w, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ')

/* Control+$KEY calculation */
#define CTRL_(c) ((c) - 'a' + 1)

/* Extended color constants (mostly) */
#define BRIGHT(color) ((color) + 8)

/* Case label for all backspace possibilities */
#define CASE_ALL_BACKSPACE case KEY_BACKSPACE: case CTRL_('h'): case 127

/* move and vw_printw */
static void vw_mvprintw(WINDOW *w, int y, int x,  const char *fmt, va_list args)
{
	wmove(w, y, x);
       	vw_printw(w, fmt, args);
}

/* an optional status line window */
static WINDOW *cu_stat_win;
/* helper for status line initialization */
static int cu_stat_init_(WINDOW *w, int cols)
{
	cu_stat_win = w;
	return OK;
}
/* initialize status line BEFORE INITSCR() */
enum cu_stat_pos {
	CU_STAT_BOTTOM = -1,
	CU_STAT_TOP = 1,
};
static int cu_stat_init(enum cu_stat_pos pos)
{
	return ripoffline(pos, cu_stat_init_);
}

/* pretty self-explanatory status line macros/functions */
#define cu_stat_clear() do { \
	wmove(cu_stat_win, 0, 0); \
	wclrtoeol(cu_stat_win); \
} while (0)
#define cu_stat_move(y, x) wmove(cu_stat_win, y, x)
/* printw for status line, but set attributes */
static void cu_stat_aprintw(int attr, char *fmt, ...)
{
	va_list ap;
	attr_t oldattr;
	short oldpair;

	if (attr != -1) {
		wattr_get(cu_stat_win, &oldattr, &oldpair, NULL);
		wattrset(cu_stat_win, attr);
	}
	va_start(ap, fmt);
	vw_printw(cu_stat_win, fmt, ap);
	va_end(ap);

	if (attr != -1) {
		wattr_set(cu_stat_win, oldattr, oldpair, NULL);
	}
	wnoutrefresh(cu_stat_win);
}

/* print an entirely new message to the status line */
static void cu_stat_setw(char *fmt, ...)
{
	va_list ap;

	cu_stat_clear();
	va_start(ap, fmt);
	vw_mvprintw(cu_stat_win, 0, 0, fmt, ap);
	va_end(ap);

	wnoutrefresh(cu_stat_win);
}
