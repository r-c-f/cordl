#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <stdbool.h>
#include <stdarg.h>
#include "xmem.h"

#define CELL_CHAR 1 /* contains a valid chacater */
#define CELL_CHARPOS 2 /* contains the valid character in correct pos */
#define CELL_BLANK 3 /* incorrect */
#define CELL_WRONG 4

#define ROW_COUNT 6
#define WORD_LEN 5
#define CHARSET "abcdefghijklmnopqrstuvwxyz"
#define QWERTY  "qwertyuiopasdfghjklzxcvbnm"
#define CHARSET_LEN 26

int char_stat[CHARSET_LEN];
char **wordlist;
size_t wordcount;


void print_status(void)
{
	int i;
	int color;
	/* first row */
	for (i = 0; i < 10; ++i) {
		attron(COLOR_PAIR(char_stat[QWERTY[i] - 'a']));
		mvaddch(8, 25 + (i * 2), CHARSET[QWERTY[i] - 'a']);
		attroff(COLOR_PAIR(char_stat[QWERTY[i] - 'a']));
	}
	for (i = 10; i < 19; ++i) {
		attron(COLOR_PAIR(char_stat[QWERTY[i] - 'a']));
		mvaddch(10, 7 + (i * 2), CHARSET[QWERTY[i] - 'a']);
		attroff(COLOR_PAIR(char_stat[QWERTY[i] - 'a']));
	}
	for (i = 19; i < CHARSET_LEN; ++i) {
		attron(COLOR_PAIR(char_stat[QWERTY[i] - 'a']));
		mvaddch(12, (i * 2) - 9, CHARSET[QWERTY[i] - 'a']);
		attroff(COLOR_PAIR(char_stat[QWERTY[i] - 'a']));
	}
	refresh();
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

void draw_cell(int color, char c, int x, int y)
{
	int i, j;
	attron(COLOR_PAIR(color));
	x *= 4;
	y *= 4;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j) {
			if (i == 1 && j == 1) {
				mvaddch(y + j, x + i, c);
			} else {
				mvaddch(y + j, x + i, ' ');
			}
		}
	}
	attroff(COLOR_PAIR(color));
}

void clear_row(int row)
{
	int i;
	for (i = 0; i < 3; ++i) {
		move((row * 4) + i, 0);
		clrtoeol();
	}
	print_status();
	refresh();
}
void draw_row(int row, char *word, char *txt)
{
	int i;
	int color;
	clear_row(row);
	for (i = 0; i < WORD_LEN; ++i) {
		if (!word) {
			draw_cell(CELL_BLANK, ' ', i, row);
		} else {
			if (txt[i] == word[i]) {
				char_stat[txt[i] - 'a'] = CELL_CHARPOS;
				draw_cell(CELL_CHARPOS, txt[i], i, row);
			} else if (strchr(word, txt[i])) {
				char_stat[txt[i] - 'a'] = CELL_CHAR;
				draw_cell(CELL_CHAR, txt[i], i, row);
			} else {
				char_stat[txt[i] - 'a'] = CELL_WRONG;
				draw_cell(CELL_WRONG, txt[i], i, row);
			}
		}
	}
}

bool input_row(int row, char *dst)
{
	int c;
	int pos;
	draw_row(row, NULL, NULL);
	refresh();
	pos = 0;
	attron(COLOR_PAIR(CELL_BLANK));
	while (1) {
		if (pos > WORD_LEN) {
			pos = 0;
			memset(dst, 0, WORD_LEN + 1);
			print_msg("Word too long");
			draw_row(row, NULL, NULL);
			attron(COLOR_PAIR(CELL_BLANK));
			refresh();
			continue;
		}
		c = mvgetch(1 + (row * 4), 1 + (pos * 4));
		switch (c) {
			case KEY_BACKSPACE:
			case 127:
			case '\b':
				if (pos)
					--pos;
				continue;
			case '\n':
				if (pos != WORD_LEN) {
					print_msg("Word too short");
					continue;
				} else {
					if (valid_word(dst)) {
						return true;
					} else {
						pos = 0;
						draw_row(row, NULL, NULL);
						attron(COLOR_PAIR(CELL_BLANK));
						print_msg("'%s' isn't a word", dst);
						refresh();
						continue;
					}
				}
			case 4: // Ctrl+D
				return false;
			default:
				dst[pos] = c;
				mvaddch(1 + (row * 4), 1 + (pos++ * 4), c);
				refresh();
		}
	}
	return true;
}


char *pick_word(void)
{
	size_t i;
	FILE *urandom = fopen("/dev/urandom", "r");
	do {
		fread(&i, sizeof(i), 1, urandom);
		i = i % wordcount;
	} while (strlen(wordlist[i]) != 5);
	fclose(urandom);
	return wordlist[i];
}

bool is_valid_charset(char *str, char *charset)
{
	int i;
	for (i = 0; str[i]; ++i) {
		if (!strchr(charset, str[i]))
			return false;
	}
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
                //validate charset
                if (!is_valid_charset(line[pos], charset)) {
                        --pos; //we'll be redoing this one.
                }
        }
        line[pos] = NULL;
        return xreallocarray(line, sizeof(*line), pos + 1);
}

void print_msg(char *fmt,...)
{
	va_list ap;
	move(24, 0);
	clrtoeol();
	move(24, 0);
	va_start(ap, fmt);
	vw_printw(stdscr, fmt, ap);
	va_end(ap);
	refresh();
}


int main(int argc, char **argv)
{
	FILE *words;
	int i, row, col;
	char *word;
	char **rows = calloc(ROW_COUNT, sizeof(*rows));
	for (i = 0; i < ROW_COUNT; ++i) {
		rows[i] = calloc(1, WORD_LEN + 1);
	}

	initscr();
	cbreak();
	noecho();

	refresh();

	start_color();
	init_pair(CELL_BLANK, 15, 8);
	init_pair(CELL_WRONG, COLOR_WHITE, 8);
	init_pair(CELL_CHAR, 15, COLOR_YELLOW);
	init_pair(CELL_CHARPOS, 15, COLOR_GREEN);

	words = fopen("/usr/share/dict/words", "r");
	wordlist = read_all_lines(words, "abcdefghijklmnopqrstuvwxyz");
	for (wordcount = 0; wordlist[wordcount]; ++wordcount);

	while (1) {
		for (i = 0; i < CHARSET_LEN; ++i) {
			char_stat[i] = CELL_BLANK;
		}

		print_status();
		word = pick_word();

		for (i = 0; i < ROW_COUNT; ++i) {
			if (!input_row(i, rows[i]))
				break;
			draw_row(i, word, rows[i]);
			print_status();
			refresh();
			if (!strcmp(rows[i], word)) {
				break;
			}
		}

		print_msg("Word was: %s\n", word);
		getch();
		refresh();

		clear();
	}
	return 0;
}


