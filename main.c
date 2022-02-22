#define _GNU_SOURCE
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include "xmem.h"
#include "sopt.h"
#include "cursutil.h"

#define RND_IMPLEMENTATION
#include "rnd.h"

#ifdef ANCIENT
#include "getline.h"
#endif


enum cell_type {
	CELL_CHAR = 1,
	CELL_RIGHT,
	CELL_BLANK,
	CELL_WRONG,
	CELL__COUNT,
};

int cell_attr[CELL__COUNT] = {0};
int color_count = -1;

#define ROW_COUNT 6
#define WORD_LEN 5
#define CHARSET "abcdefghijklmnopqrstuvwxyz"
#define QWERTY  "qwertyuiopasdfghjklzxcvbnm"
#define CHARSET_LEN 26

int char_stat[CHARSET_LEN];
char **wordlist;
size_t wordcount;

#define PRINT_HELP_BOLD_DESC(bold, desc) do { \
	cu_stat_aprintw(A_BOLD, "%s", bold); \
	cu_stat_aprintw(A_NORMAL, ": %s; ", desc); \
} while (0)
#define PRINT_HELP_CELL(cell, desc) do { \
	cu_stat_aprintw(cell_attr[cell], "XXX"); \
	cu_stat_aprintw(A_NORMAL, ": %s; ", desc); \
} while (0)
void print_help(void)
{
	cu_stat_clear();
	PRINT_HELP_CELL(CELL_BLANK, "unused");
	PRINT_HELP_CELL(CELL_WRONG, "wrong");
	PRINT_HELP_CELL(CELL_CHAR, "misplaced");
	PRINT_HELP_CELL(CELL_RIGHT, "right");
	PRINT_HELP_BOLD_DESC("^C", "quit");
	PRINT_HELP_BOLD_DESC("^D", "new");
	refresh();
}

void qwerty_status(void)
{
	int i;
	int color;
	/* first row */
	for (i = 0; i < 10; ++i) {
		attron(cell_attr[char_stat[QWERTY[i] - 'a']]);
		mvaddch(8, 25 + (i * 2), CHARSET[QWERTY[i] - 'a']);
		attroff(cell_attr[char_stat[QWERTY[i] - 'a']]);
	}
	for (i = 10; i < 19; ++i) {
		attron(cell_attr[char_stat[QWERTY[i] - 'a']]);
		mvaddch(10, 7 + (i * 2), CHARSET[QWERTY[i] - 'a']);
		attroff(cell_attr[char_stat[QWERTY[i] - 'a']]);
	}
	for (i = 19; i < CHARSET_LEN; ++i) {
		attron(cell_attr[char_stat[QWERTY[i] - 'a']]);
		mvaddch(12, (i * 2) - 9, CHARSET[QWERTY[i] - 'a']);
		attroff(cell_attr[char_stat[QWERTY[i] - 'a']]);
	}
}

bool valid_word(char *s)
{
	size_t i;
	for (i = 0; i < wordcount; ++i) {
		if (!strcmp(s, wordlist[i]))
			return true;
	}
	return false;
}

void draw_cell(enum cell_type type, char c, int x, int y)
{
	int i, j;
	attrset(cell_attr[type] & ~A_UNDERLINE);
	x *= 4;
	y *= 4;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j) {
			if (i == 1 && j == 1) {
				mvaddch(y + j, x + i, c | cell_attr[type]);
			} else {
				mvaddch(y + j, x + i, ' ');
			}
		}
	}
}

void clear_row(int row)
{
	int i;
	for (i = 0; i < 3; ++i) {
		move((row * 4) + i, 0);
		clrtoeol();
	}
}
void draw_row(int row, char *word, char *txt)
{
	int i;
	enum cell_type type;
	clear_row(row);
	for (i = 0; i < WORD_LEN; ++i) {
		if (!word) {
			draw_cell(CELL_BLANK, ' ', i, row);
		} else {
			if (txt[i] == word[i]) {
				type = CELL_RIGHT;
			} else if (strchr(word, txt[i])) {
				type = CELL_CHAR;
			} else {
				type = CELL_WRONG;
			}
			char_stat[txt[i] - 'a'] = type;
			draw_cell(type, txt[i], i, row);
		}
	}
}

bool input_row(int row, char *dst)
{
	int c;
	int pos;
	draw_row(row, NULL, NULL);
	qwerty_status();
	refresh();
	pos = 0;
	attron(cell_attr[CELL_BLANK]);
	while (1) {
		if (pos > WORD_LEN) {
			pos = 0;
			memset(dst, 0, WORD_LEN + 1);
			cu_stat_setw("Word too long");
			draw_row(row, NULL, NULL);
			qwerty_status();
			attron(cell_attr[CELL_BLANK]);
			refresh();
			continue;
		}
		c = mvgetch(1 + (row * 4), 1 + (pos * 4));
		switch (c) {
			CASE_ALL_BACKSPACE:
				if (pos)
					--pos;
				continue;
			CASE_ALL_RETURN:
				if (pos != WORD_LEN) {
					cu_stat_setw("Word too short");
					refresh();
					continue;
				} else {
					if (valid_word(dst)) {
						return true;
					} else {
						pos = 0;
						draw_row(row, NULL, NULL);
						qwerty_status();
						attron(cell_attr[CELL_BLANK]);
						cu_stat_setw("'%s' isn't a word", dst);
						refresh();
						continue;
					}
				}
			case CTRL_('c'):
				endwin();
				exit(0);
			case CTRL_('d'):
				return false;
			default:
				if (!islower(c)) {
					beep();
					print_help();
					refresh();
					continue;
				}
				dst[pos] = c;
				mvaddch(1 + (row * 4), 1 + (pos++ * 4), c);
				refresh();
		}
	}
	return true;
}

bool is_valid_charset_len(char *str, char *charset)
{
	int i;
	for (i = 0; str[i]; ++i) {
		if (i == WORD_LEN)
			return false;
		if (!strchr(charset, str[i]))
			return false;
	}
	if (i != WORD_LEN)
		return false;
	return true;
}

char **read_all_lines(FILE *f, char *charset)
{
        size_t len = 24;
        size_t pos = 0;
        size_t n;
        ssize_t line_len;
        char **line;

        line = xcalloc(sizeof(*line), len);
        for (pos = 0; ;++pos) {
                if (pos == len) {
                        len *= 3;
                        len /= 2;
                        line = xreallocarray(line, len, sizeof(*line));
                        memset(line + pos, 0, sizeof(*line) * (len - pos));
                }
                n = 0;
                if ((line_len = getline(line + pos, &n, f)) == -1)
                        break;
                line[pos][line_len - 1] = '\0'; // strip newline
                //validate charset & length
                if (!is_valid_charset_len(line[pos], charset)) {
                        --pos; //we'll be redoing this one.
                }
        }
        line[pos] = NULL;
        return xreallocarray(line, sizeof(*line), pos + 1);
}

struct sopt optspec[] = {
	SOPT_INIT_ARGL('w', "wordlist", "dict", "List of words (one per line) to use as dictionary"),
	SOPT_INITL('m', "monochrome", "Force monochrome mode"),
	SOPT_INITL('l', "lowcolor", "Force 8 color mode"),
	SOPT_INITL('H', "highcolor", "Force 16-color mode"),
	SOPT_INITL('h', "help", "Help message"),
	SOPT_INIT_END
};

int main(int argc, char **argv)
{
	/* sopt things*/
	int opt, cpos = 0, optind = 0;
	char *optarg = NULL;

	char *dictpath = "/usr/share/dict/words";
	FILE *words;
	int i, row, col;
	size_t word;
	char **rows;
	rnd_pcg_t pcg;
	bool force_mono = false;

	if (!(dictpath = getenv("CORDL_WORDS"))) {
		dictpath = "/usr/share/dict/words";
	}

	sopt_usage_set(optspec, argv[0], "wordle-like game for the terminal");

	while ((opt = sopt_getopt(argc, argv, optspec, &cpos, &optind, &optarg)) != -1) {
		switch (opt) {
			case 'w':
				dictpath = optarg;
				break;
			case 'l':
				color_count = 8;
				break;
			case 'h':
				sopt_usage_s();
				return 0;
			case 'H':
				color_count = 17;
				break;
			case 'm':
				force_mono = true;
				break;
			default:
				sopt_usage_s();
				return 1;
		}
	}

	rows = xcalloc(ROW_COUNT, sizeof(*rows));
	for (i = 0; i < ROW_COUNT; ++i) {
		rows[i] = xcalloc(1, WORD_LEN + 1);
	}

	if (!(words = fopen(dictpath, "r"))) {
		perror("fopen wordlist");
		return 1;
	}
	wordlist = read_all_lines(words, CHARSET);
	fclose(words);
	for (wordcount = 0; wordlist[wordcount]; ++wordcount);

	rnd_pcg_seed(&pcg, time(NULL) + getpid());

	setlocale(LC_ALL, "");

	cu_stat_init(CU_STAT_BOTTOM);
	initscr();
	raw();
	noecho();
	keypad(stdscr, true);

	if (has_colors() && !force_mono) {
		start_color();
		if (color_count == -1) {
			color_count = COLORS;
		}

		if (color_count >= 16) {
			init_pair(CELL_BLANK, BRIGHT(COLOR_WHITE), BRIGHT(COLOR_BLACK));
			init_pair(CELL_WRONG, COLOR_WHITE, BRIGHT(COLOR_BLACK));
			init_pair(CELL_CHAR, BRIGHT(COLOR_WHITE), COLOR_YELLOW);
			init_pair(CELL_RIGHT, BRIGHT(COLOR_WHITE), COLOR_GREEN);
		} else {
			init_pair(CELL_BLANK, COLOR_BLACK, COLOR_WHITE);
			init_pair(CELL_WRONG, COLOR_WHITE, COLOR_BLACK);
			init_pair(CELL_CHAR, COLOR_WHITE, COLOR_YELLOW);
			init_pair(CELL_RIGHT, COLOR_WHITE, COLOR_GREEN);
		}
		cell_attr[CELL_BLANK] = COLOR_PAIR(CELL_BLANK);
		cell_attr[CELL_WRONG] = COLOR_PAIR(CELL_WRONG);
		cell_attr[CELL_CHAR] = COLOR_PAIR(CELL_CHAR) | A_BOLD;
		cell_attr[CELL_RIGHT] = COLOR_PAIR(CELL_RIGHT) | A_BOLD;
	} else {
		cell_attr[CELL_BLANK] = A_REVERSE;
		cell_attr[CELL_WRONG] = A_DIM;
		cell_attr[CELL_CHAR] = A_BOLD;
		cell_attr[CELL_RIGHT] = A_BOLD | A_UNDERLINE;
	}

	print_help();
	refresh();

	while (1) {
		for (i = 0; i < CHARSET_LEN; ++i) {
			char_stat[i] = CELL_BLANK;
		}

		word = rnd_pcg_range(&pcg, 0, wordcount - 1);

		qwerty_status();

		for (i = 0; i < ROW_COUNT; ++i) {
			if (!input_row(i, rows[i]))
				break;
			draw_row(i, wordlist[word], rows[i]);
			qwerty_status();
			refresh();
			if (!strcmp(rows[i], wordlist[word])) {
				break;
			}
		}

		cu_stat_setw("Word was: %s\n", wordlist[word]);
		refresh();
		getch();

		clear();
		refresh();
	}
	return 0;
}


