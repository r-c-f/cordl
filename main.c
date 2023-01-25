#define _GNU_SOURCE
#include <assert.h>
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
#include "sassert.h"
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

bool hard_mode = false;

#define ROW_COUNT 6
#define WORD_LEN 5
#define CHARSET "abcdefghijklmnopqrstuvwxyz"
#define QWERTY  "qwertyuiopasdfghjklzxcvbnm"
#define CHARSET_LEN (sizeof(CHARSET) - 1)

static_assert(sizeof(CHARSET) == sizeof(QWERTY), "Character set does not match keyboard layout");


#define GAMESTAT_LEN (ROW_COUNT + 1)
int game_stat[GAMESTAT_LEN];
size_t game_count;

int char_stat[CHARSET_LEN];
char **wordlist;
size_t wordcount;

WINDOW *qwerty_win, *row_win, *stat_win;

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

void game_status(int won)
{
	int i;

	if (won >= 0) {
		++game_stat[won];
	}

	for (i = 0; i < GAMESTAT_LEN; ++i) {
		mvwprintw(stat_win, i, 1, "  %d  | %d", i + 1, game_stat[i]);
	}
	mvwprintw(stat_win, GAMESTAT_LEN - 1, 1, "Miss | %d", game_stat[GAMESTAT_LEN - 1]);
	wnoutrefresh(stat_win);
}

void qwerty_status(void)
{
	int i, ch;
	/* first row */
	for (i = 0; i < 10; ++i) {
		ch = cell_attr[char_stat[QWERTY[i] - 'a']] | CHARSET[QWERTY[i] - 'a'];
		mvwaddch(qwerty_win, 1, (i * 2) + 1, ch);
	}
	for (i = 10; i < 19; ++i) {
		ch = cell_attr[char_stat[QWERTY[i] - 'a']] | CHARSET[QWERTY[i] - 'a'];
		mvwaddch(qwerty_win, 3, ((i - 10) * 2) + 3, ch);
	}
	for (i = 19; i < CHARSET_LEN; ++i) {
		ch = cell_attr[char_stat[QWERTY[i] - 'a']] | CHARSET[QWERTY[i] - 'a'];
		mvwaddch(qwerty_win, 5, ((i - 19) * 2) + 5, ch);
	}
	wnoutrefresh(qwerty_win);
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
	wattrset(row_win, cell_attr[type] & ~A_UNDERLINE);
	x *= 4;
	y *= 4;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j) {
			if (i == 1 && j == 1) {
				mvwaddch(row_win, y + j, x + i, c | cell_attr[type]);
			} else {
				mvwaddch(row_win, y + j, x + i, ' ');
			}
		}
	}
}

void clear_row(int row)
{
	int i;
	for (i = 0; i < 3; ++i) {
		wmove(row_win, (row * 4) + i, 0);
		wclrtoeol(row_win);
	}
}

void letter_count(int freq[static 26], char *s)
{
	int i;
	while (*s) {
		++(freq[*(s++) - 'a']);
	}
}

void draw_row(int row, char *word, char *txt)
{
	int i;
	enum cell_type type;
	int word_letters[26] = {0};

	if (word) {
		letter_count(word_letters, word);
	}

	clear_row(row);
	for (i = 0; i < WORD_LEN; ++i) {
		if (!word) {
			draw_cell(CELL_BLANK, ' ', i, row);
		} else {
			if (txt[i] == word[i]) {
				type = CELL_RIGHT;
				--word_letters[txt[i] - 'a'];
			} else if (word_letters[txt[i] - 'a']) {
				type = CELL_CHAR;
				--word_letters[txt[i] - 'a'];
			} else {
				type = CELL_WRONG;
			}
			char_stat[txt[i] - 'a'] = type;
			draw_cell(type, txt[i], i, row);
		}
	}
	wnoutrefresh(row_win);
}

bool input_row(int row, char **rows, char *word)
{
	int i;
	int j;
	int c;
	int pos;
	draw_row(row, NULL, NULL);
	pos = 0;
	memset(rows[row], 0, WORD_LEN + 1);
	wattron(row_win, cell_attr[CELL_BLANK]);
	while (1) {
input_row_continue:
		qwerty_status();
		game_status(-1);
		refresh();
		if (pos < WORD_LEN) {
			c = mvwgetch(row_win, 1 + (row * 4), 1 + (pos * 4));
		} else {
			curs_set(0);
			c = wgetch(row_win);
			curs_set(1);
		}
		if (pos > WORD_LEN) {
			/* only backspace is allowed */
			switch (c) {
				CASE_ALL_BACKSPACE:
					--pos;
					rows[row][pos] = '\0';
					break;
				default:
					cu_stat_setw("Word too long");
					wnoutrefresh(row_win);
					continue;
			}
		}
		switch (c) {
			CASE_ALL_BACKSPACE:
				if (pos) {
					--pos;
				}
				rows[row][pos] = '\0';
				mvwaddch(row_win, 1 + (row * 4), 1 + (pos * 4), ' ');
				wnoutrefresh(row_win);
				continue;
			CASE_ALL_RETURN:
				if (pos < WORD_LEN) {
					cu_stat_setw("Word too short");
					wnoutrefresh(row_win);
					continue;
				}
				if (hard_mode) {
					for (i = 0; i < CHARSET_LEN; ++i) {
						if ((char_stat[i] == CELL_CHAR) || (char_stat[i] == CELL_RIGHT)) {
							if (!strchr(rows[row], CHARSET[i])) {
								cu_stat_setw("%c must be used in solution", CHARSET[i]);
								wnoutrefresh(row_win);
								goto input_row_continue;
							}
						}
					}
					for (i = 0; i < row; ++i) {
						for (j = 0; j < WORD_LEN; ++j) {
							if (rows[i][j] == rows[row][j]) {
								if (rows[row][j] != word[j]) { 
									cu_stat_setw("%c already tried in wrong position", rows[row][j]);
									wnoutrefresh(row_win);
									goto input_row_continue;
								}
							} else if (rows[i][j] == word[j]) {
								cu_stat_setw("%c must be used in correct position", rows[i][j]);
								wnoutrefresh(row_win);
								goto input_row_continue;
							}
							if (char_stat[rows[row][j] - 'a'] == CELL_WRONG) {
								cu_stat_setw("%c already tried", rows[row][j]);
								wnoutrefresh(row_win);
								goto input_row_continue;
							}
						}
					}
				}

				if (valid_word(rows[row])) {
					return true;
				}
				pos = 0;
				draw_row(row, NULL, NULL);
				wattron(row_win, cell_attr[CELL_BLANK]);
				cu_stat_setw("'%s' isn't a word", rows[row]);
				wnoutrefresh(row_win);
				continue;
			case CTRL_('c'):
				endwin();
				exit(0);
			case CTRL_('d'):
				return false;
			default:
				if (!islower(c)) {
					beep();
					print_help();
					wnoutrefresh(row_win);
					continue;
				}
				rows[row][pos] = c;
				mvwaddch(row_win, 1 + (row * 4), 1 + (pos++ * 4), c);
				wnoutrefresh(row_win);
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
	SOPT_INIT_ARGL('w', "wordlist", SOPT_ARGTYPE_STR, "dict", "List of words (one per line) to use as dictionary"),
	SOPT_INIT_ARGL('W', "word", SOPT_ARGTYPE_STR, "word", "Set initial word"),
	SOPT_INITL('m', "monochrome", "Force monochrome mode"),
	SOPT_INITL('l', "lowcolor", "Force 8 color mode"),
	SOPT_INITL('H', "highcolor", "Force 16-color mode"),
	SOPT_INITL('h', "help", "Help message"),
	SOPT_INITL('x', "hard", "Hard mode"),
	SOPT_INIT_END
};

int main(int argc, char **argv)
{
	/* sopt things*/
	int opt;
	union sopt_arg soptarg;

	char *dictpath = "/usr/share/dict/words";
	FILE *words;
	int i;
	size_t word;
	char *initial_word = NULL;
	char **rows;
	rnd_pcg_t pcg;
	bool force_mono = false;
	bool won;

	if (!(dictpath = getenv("CORDL_WORDS"))) {
		dictpath = "/usr/share/dict/words";
	}

	sopt_usage_set(optspec, argv[0], "wordle-like game for the terminal");

	while ((opt = sopt_getopt_s(argc, argv, optspec, NULL, NULL, &soptarg)) != -1) {
		switch (opt) {
			case 'w':
				dictpath = soptarg.str;
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
			case 'x':
				hard_mode = true;
				break;
			case 'W':
				initial_word = xstrdup(soptarg.str);
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

	qwerty_win = newwin(7, 21, 8, 23);
	row_win = newwin((ROW_COUNT * 4) - 1, WORD_LEN * 4, 0, 0);
	stat_win = newwin(GAMESTAT_LEN + 1, 21, 0, 23);

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
	wrefresh(stat_win);

	do {
		for (i = 0; i < CHARSET_LEN; ++i) {
			char_stat[i] = CELL_BLANK;
		}
		if (initial_word) {
			for (word = 0; word < wordcount; ++word) {
				if (!strcmp(wordlist[word], initial_word))
					break;
			}
			if (word == wordcount) {
				break;
			}
		} else {
			word = rnd_pcg_range(&pcg, 0, wordcount - 1);
		}

		qwerty_status();

		won = false;
		for (i = 0; i < ROW_COUNT; ++i) {
			if (!input_row(i, rows, wordlist[word]))
				break;
			draw_row(i, wordlist[word], rows[i]);
			qwerty_status();
			refresh();
			if (!strcmp(rows[i], wordlist[word])) {
				won = true;
				break;
			}
		}

		cu_stat_setw("Word was: %s\n", wordlist[word]);
		if (won) {
			game_status(i);
		} else { 
			game_status(GAMESTAT_LEN - 1);
		}
		refresh();
		getch();

		clear();
		refresh();
	} while (!initial_word); //exits if we have given a word
	return 0;
}


