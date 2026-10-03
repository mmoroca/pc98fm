#ifndef PC98FM_PDCURSES
#define _XOPEN_SOURCE 700
#endif

#include "ui.h"

#ifdef PC98FM_PDCURSES
#include <curses.h>
#else
#include <ncurses.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef PC98FM_PDCURSES
#include <wchar.h>
#endif

#define DATE_COLUMN_WIDTH 14
#define SIZE_COLUMN_WIDTH 12

static void format_size(uint64_t value, char *buffer, size_t size) {
  char temp[32];
  char result[64];
  int length;
  int separators;
  int result_length;
  int source;
  int destination;
  int group_size = 0;

  if (size == 0) return;

  snprintf(temp, sizeof(temp), "%llu", (unsigned long long)value);
  length = (int)strlen(temp);
  separators = (length - 1) / 3;
  result_length = length + separators;
  result[result_length] = '\0';
  source = length - 1;
  destination = result_length - 1;

  while (source >= 0) {
    result[destination--] = temp[source--];
    group_size++;
    if (group_size == 3 && source >= 0) {
      result[destination--] = ',';
      group_size = 0;
    }
  }

  if ((size_t)result_length >= size) {
    memcpy(buffer, result, size - 1);
    buffer[size - 1] = '\0';
  } else {
    memcpy(buffer, result, (size_t)result_length + 1);
  }
}

static void menu_item(int *x, const char *text) {
  char item[128];
  int label_width;
  int available;
  int visible;

  label_width = snprintf(item, sizeof(item), " %s ", text);
  if (label_width < 0) return;

  available = COLS - *x - 1;
  visible = available > 0
              ? (label_width < available ? label_width : available)
              : 0;

  if (visible > 0) {
    attron(COLOR_PAIR(20));
    mvaddnstr(0, *x, item, visible);
    attroff(COLOR_PAIR(20));
  }

  *x += label_width;
  if (*x < COLS - 1) {
    mvaddch(0, *x, ' ');
    (*x)++;
  } else if (*x > COLS) {
    *x = COLS;
  }
}

static void draw_menu(void) {
  int x = 1;

  menu_item(&x, "[:]DRV");
  menu_item(&x, "[N]ew dir");
  menu_item(&x, "e[X]ec");
  menu_item(&x, "[C]opy");
  menu_item(&x, "[D]el");
  menu_item(&x, "[R]en");
  menu_item(&x, "[S]ort");
  menu_item(&x, "[E]dit");
  menu_item(&x, "[V]iew");
  menu_item(&x, "[P]ack");
  menu_item(&x, "[U]npack");
  menu_item(&x, "[Q]uit");
}

static void draw_clock(void) {
  char buffer[32];
  time_t now = time(NULL);
  struct tm *local_time = localtime(&now);

  if (LINES < 2 || COLS < 4 || !local_time ||
      strftime(buffer, sizeof(buffer), "%H:%M:%S", local_time) == 0)
    return;

  attron(COLOR_PAIR(3));
  mvprintw(LINES - 2, COLS - 15, " %s ", buffer);
  attroff(COLOR_PAIR(3));
}

static void print_clipped(int y, int x, int width, const char *text) {
  if (width <= 0 || x < 0 || x >= COLS) return;
  if (width > COLS - x) width = COLS - x;
  if (width > 0) mvaddnstr(y, x, text, width);
}

static void print_tail_fitted(int y, int x, int width, const char *text) {
  size_t length;
  size_t tail_width;
  const char *start;
  const char *cursor;

  if (width <= 0 || x < 0 || x >= COLS) return;
  if (width > COLS - x) width = COLS - x;

  length = strlen(text);
  if ((size_t)width >= length) {
    mvaddnstr(y, x, text, width);
    return;
  }

  if (width <= 3) {
    mvaddnstr(y, x, text + length - (size_t)width, width);
    return;
  }

  tail_width = (size_t)width - 3;
  start = text + length - tail_width;
  for (cursor = text; cursor < text + length; cursor++) {
    if (*cursor == '/' && strlen(cursor) <= tail_width) {
      start = cursor;
      break;
    }
  }

  mvaddstr(y, x, "...");
  mvaddnstr(y, x + 3, start, (int)tail_width);
}

static void draw_panel_info(const Panel *panel, int x, int width) {
  char free_buffer[64];
  char size_buffer[64];
  char file_buffer[96];

  format_size(fs_free_space(panel->path), free_buffer, sizeof(free_buffer));
  format_size(panel->directory.total_size, size_buffer, sizeof(size_buffer));
  snprintf(file_buffer, sizeof(file_buffer), "%zu%s/%s",
           panel->directory.file_count,
           panel->directory.truncated ? "+" : "", size_buffer);

  attron(COLOR_PAIR(6));
  print_clipped(1, x, 6, "FREE:");
  attroff(COLOR_PAIR(6));
  attron(COLOR_PAIR(7));
  print_clipped(1, x + 6, width - 7, free_buffer);
  attroff(COLOR_PAIR(7));
  attron(COLOR_PAIR(6));
  print_clipped(1, x + 24, width - 25, "FILE:");
  attroff(COLOR_PAIR(6));
  attron(COLOR_PAIR(7));
  print_clipped(1, x + 30, width - 31, file_buffer);
  attroff(COLOR_PAIR(7));
  attron(COLOR_PAIR(6));
  print_clipped(2, x, 5, "PATH=");
  attroff(COLOR_PAIR(6));
  attron(COLOR_PAIR(8));
  print_tail_fitted(2, x + 5, width - 6, panel->path);
  attroff(COLOR_PAIR(8));
}

static size_t utf8_character_bytes(const char *text, size_t maximum) {
  const unsigned char *current = (const unsigned char *)text;
  size_t length = 1;

  if (maximum == 0 || *current == '\0') return 0;

  if (*current >= 0xC2 && *current <= 0xDF) length = 2;
  else if (*current >= 0xE0 && *current <= 0xEF) length = 3;
  else if (*current >= 0xF0 && *current <= 0xF4) length = 4;

  if (length > maximum) return 0;
  for (size_t i = 1; i < length; i++) {
    if ((current[i] & 0xC0) != 0x80) return 1;
  }

  return length;
}

static int utf8_character_width(const char *text, size_t bytes) {
#ifndef PC98FM_PDCURSES
  mbstate_t state = {0};
  wchar_t wide;
  size_t converted = mbrtowc(&wide, text, bytes, &state);

  if (converted != (size_t)-1 && converted != (size_t)-2) {
    int width = wcwidth(wide);
    if (width >= 0) return width;
  }
#else
  (void)text;
  (void)bytes;
#endif
  return 1;
}

static size_t utf8_prefix_bytes(const char *text, size_t maximum) {
  size_t used = 0;

  while (used < maximum) {
    size_t length = utf8_character_bytes(text + used, maximum - used);
    if (length == 0) break;
    used += length;
  }

  return used;
}

static size_t utf8_display_columns(const char *text, size_t bytes) {
  size_t columns = 0;
  size_t offset = 0;

  while (offset < bytes) {
    size_t length = utf8_character_bytes(text + offset, bytes - offset);
    if (length == 0) break;
    offset += length;
    columns += (size_t)utf8_character_width(text + offset - length, length);
  }

  return columns;
}

static void print_left_fitted(int y, int x, int width, const char *text) {
  size_t length;
  size_t bytes;
  int text_columns;

  if (width <= 0 || x < 0 || x >= COLS) return;
  if (width > COLS - x) width = COLS - x;
  length = strlen(text);

  if ((size_t)width >= length) {
    mvaddnstr(y, x, text, (int)length);
    return;
  }

  if (width <= 3) {
    bytes = utf8_prefix_bytes(text, (size_t)width);
    mvaddnstr(y, x, text, (int)bytes);
    return;
  }

  bytes = utf8_prefix_bytes(text, (size_t)(width - 3));
  text_columns = (int)utf8_display_columns(text, bytes);
  mvaddnstr(y, x, text, (int)bytes);
  mvaddstr(y, x + text_columns, "...");
}

static size_t utf8_prefix_for_columns(const char *text, int maximum,
                                      int *used_columns) {
  size_t offset = 0;
  size_t length = strlen(text);
  int columns = 0;

  if (maximum < 0) maximum = 0;

  while (offset < length) {
    size_t character = utf8_character_bytes(text + offset, length - offset);
    int character_width;

    if (character == 0) break;
    character_width = utf8_character_width(text + offset, character);
    if (character_width > 0 && columns + character_width > maximum) break;
    offset += character;
    if (character_width > 0) columns += character_width;
  }

  if (used_columns) *used_columns = columns;
  return offset;
}

static void print_editor_line(int y, int x, int width, const char *text) {
  int text_columns;
  int visible_columns;
  int has_more;
  int text_width;
  size_t bytes;

  if (width <= 0 || x < 0 || x >= COLS) return;
  if (width > COLS - x) width = COLS - x;
  if (width <= 0) return;

  has_more = utf8_display_columns(text, strlen(text)) > (size_t)width;
  text_width = has_more ? width - 1 : width;
  bytes = utf8_prefix_for_columns(text, text_width, &text_columns);
  if (bytes > 0) mvaddnstr(y, x, text, (int)bytes);

  visible_columns = text_columns;
  if (has_more && visible_columns < width)
    mvaddstr(y, x + visible_columns, "\342\200\246");
}

static void print_right_fitted(int y, int x, int width, const char *text) {
  size_t length;

  if (width <= 0 || x < 0 || x >= COLS) return;
  if (width > COLS - x) width = COLS - x;
  length = strlen(text);

  if ((size_t)width >= length) {
    mvprintw(y, x, "%*s", width, text);
  } else if (width <= 3) {
    mvaddnstr(y, x, text + length - (size_t)width, width);
  } else {
    mvaddstr(y, x, "...");
    mvaddnstr(y, x + 3, text + length - (size_t)(width - 3), width - 3);
  }
}

static void format_mtime(const FsEntry *entry, char *buffer, size_t size) {
  struct tm *local_time;

  if (size == 0) return;
  buffer[0] = '\0';
  if (!entry->stat_ok) return;

  local_time = localtime(&entry->mtime);
  if (local_time) strftime(buffer, size, "%y/%m/%d %H:%M", local_time);
}

static int entry_color_pair(const FsEntry *entry, int selected) {
  if (selected) return 5;
  if (entry->is_dir) return 9;
  if (entry->is_executable) return 10;
  return 2;
}

static void draw_panel(Panel *panel, int x, int width, int active) {
  const int top = 4;
  const int bottom = LINES - 2;
  int height = bottom - top + 1;
  int rows;

  if (width < 8 || height < 3) return;

  attron(COLOR_PAIR(1));
  mvhline(top, x + 1, ACS_HLINE, width - 2);
  mvhline(bottom, x + 1, ACS_HLINE, width - 2);
  mvvline(top + 1, x, ACS_VLINE, height - 2);
  mvvline(top + 1, x + width - 1, ACS_VLINE, height - 2);
  mvaddch(top, x, ACS_ULCORNER);
  mvaddch(top, x + width - 1, ACS_URCORNER);
  mvaddch(bottom, x, ACS_LLCORNER);
  mvaddch(bottom, x + width - 1, ACS_LRCORNER);
  attroff(COLOR_PAIR(1));

  mvprintw(top, x + 3, " *.* ");
  rows = height - 2;
  core_ensure_selection_visible(panel, rows);

  for (int i = 0; i < rows; i++) {
    size_t index = panel->scroll + (size_t)i;
    int y = top + 1 + i;
    FsEntry *entry;
    int selected;
    int name_x = x + 2;
    int name_width;
    int size_x = 0;
    int date_x = 0;
    char size_buffer[64];
    char date_buffer[DATE_COLUMN_WIDTH + 1];

    if (index >= panel->directory.count) break;

    entry = &panel->directory.entries[index];
    selected = active && index == panel->selected;

    if (selected) {
      attron(COLOR_PAIR(5));
      mvhline(y, x + 1, ' ', width - 2);
      attroff(COLOR_PAIR(5));
    }

    attron(COLOR_PAIR(entry_color_pair(entry, selected)));

    if (width >= 39) {
      int right = x + width - 2;
      date_x = right - DATE_COLUMN_WIDTH + 1;
      size_x = date_x - SIZE_COLUMN_WIDTH - 1;
      name_width = size_x - name_x - 1;
    } else {
      name_width = width - 4;
    }

    print_left_fitted(y, name_x, name_width, entry->name);

    if (width >= 39) {
      if (entry->is_dir) {
        strcpy(size_buffer, "<DIR>");
      } else if (entry->stat_ok) {
        format_size((uint64_t)entry->size, size_buffer, sizeof(size_buffer));
      } else {
        size_buffer[0] = '\0';
      }

      format_mtime(entry, date_buffer, sizeof(date_buffer));
      print_right_fitted(y, size_x, SIZE_COLUMN_WIDTH, size_buffer);
      print_left_fitted(y, date_x, DATE_COLUMN_WIDTH, date_buffer);
    }

    attroff(COLOR_PAIR(entry_color_pair(entry, selected)));
  }
}

static void draw_viewer_menu(void) {
  int x = 1;
  menu_item(&x, "[ESC]");
}

static void draw_viewer(AppState *app) {
  const int top = 4;
  const int bottom = LINES - 2;
  const int x = 1;
  const int right_x = COLS / 2;
  const int right_width = COLS / 2 - 2;
  const int width = right_x + right_width - x;
  int rows = bottom - top - 1;
  char line_info[64];

  draw_viewer_menu();
  if (width < 8) {
    draw_clock();
    return;
  }

  attron(COLOR_PAIR(6));
  print_clipped(1, x, 6, "FILE:");
  attroff(COLOR_PAIR(6));
  attron(COLOR_PAIR(8));
  print_tail_fitted(1, x + 6, width - 7, app->viewer.path);
  attroff(COLOR_PAIR(8));

  snprintf(line_info, sizeof(line_info), "%zu/%zu",
           app->viewer.count == 0 ? 0 : app->viewer.top_line + 1,
           app->viewer.count);
  attron(COLOR_PAIR(6));
  print_clipped(2, x, 5, "LINE:");
  attroff(COLOR_PAIR(6));
  attron(COLOR_PAIR(7));
  print_clipped(2, x + 6, width - 7, line_info);
  attroff(COLOR_PAIR(7));

  attron(COLOR_PAIR(1));
  mvhline(top, x + 1, ACS_HLINE, width - 2);
  mvhline(bottom, x + 1, ACS_HLINE, width - 2);
  mvvline(top + 1, x, ACS_VLINE, bottom - top - 1);
  mvvline(top + 1, x + width - 1, ACS_VLINE, bottom - top - 1);
  mvaddch(top, x, ACS_ULCORNER);
  mvaddch(top, x + width - 1, ACS_URCORNER);
  mvaddch(bottom, x, ACS_LLCORNER);
  mvaddch(bottom, x + width - 1, ACS_LRCORNER);
  attroff(COLOR_PAIR(1));

  core_ensure_viewer_visible(&app->viewer, rows);
  attron(COLOR_PAIR(7));
  for (int i = 0; i < rows; i++) {
    size_t line = app->viewer.top_line + (size_t)i;
    if (line >= app->viewer.count) break;
    print_left_fitted(top + 1 + i, x + 2, width - 4,
                      app->viewer.lines[line]);
  }
  attroff(COLOR_PAIR(7));
  draw_clock();
}

static void draw_editor_menu(void) {
  int x = 1;
  menu_item(&x, "[ESC]");
}

static void draw_editor(AppState *app) {
  const int top = 4;
  const int bottom = LINES - 2;
  const int x = 1;
  const int right_x = COLS / 2;
  const int right_width = COLS / 2 - 2;
  const int width = right_x + right_width - x;
  const int content_width = width - 4;
  const int rows = bottom - top - 1;
  TextEditor *editor = &app->editor;
  char line_info[64];
  size_t cursor_row;

  draw_editor_menu();
  if (width < 8) {
    draw_clock();
    return;
  }

  attron(COLOR_PAIR(6));
  print_clipped(1, x, 6, "FILE:");
  attroff(COLOR_PAIR(6));
  attron(COLOR_PAIR(8));
  print_tail_fitted(1, x + 6, width - 7, editor->path);
  attroff(COLOR_PAIR(8));

  snprintf(line_info, sizeof(line_info), "%zu/%zu",
           editor->cursor_line + 1, editor->count);
  attron(COLOR_PAIR(6));
  print_clipped(2, x, 5, "LINE:");
  attroff(COLOR_PAIR(6));
  attron(COLOR_PAIR(7));
  print_clipped(2, x + 6, width - 7, line_info);
  attroff(COLOR_PAIR(7));

  attron(COLOR_PAIR(1));
  mvhline(top, x + 1, ACS_HLINE, width - 2);
  mvhline(bottom, x + 1, ACS_HLINE, width - 2);
  mvvline(top + 1, x, ACS_VLINE, bottom - top - 1);
  mvvline(top + 1, x + width - 1, ACS_VLINE, bottom - top - 1);
  mvaddch(top, x, ACS_ULCORNER);
  mvaddch(top, x + width - 1, ACS_URCORNER);
  mvaddch(bottom, x, ACS_LLCORNER);
  mvaddch(bottom, x + width - 1, ACS_LRCORNER);
  attroff(COLOR_PAIR(1));

  core_editor_ensure_visible(editor, content_width - 1, rows);
  cursor_row = core_editor_cursor_row(editor, (size_t)content_width);

  attron(COLOR_PAIR(7));
  for (int i = 0; i < rows; i++) {
    size_t line;
    size_t offset;
    size_t display_start;

    if (!core_editor_row_at(editor, editor->top_row + (size_t)i,
                            (size_t)content_width, &line, &offset))
      break;

    display_start = editor->wrap ? offset : editor->horizontal_scroll;
    if (display_start > strlen(editor->lines[line]))
      display_start = strlen(editor->lines[line]);
    print_editor_line(top + 1 + i, x + 2, content_width,
                      editor->lines[line] + display_start);
  }
  attroff(COLOR_PAIR(7));

  if (cursor_row >= editor->top_row &&
      cursor_row < editor->top_row + (size_t)rows) {
    size_t cursor_offset = editor->horizontal_scroll;
    size_t cursor_line;
    int cursor_x = x + 2;
    int cursor_y = top + 1 + (int)(cursor_row - editor->top_row);

    if (editor->wrap && core_editor_row_at(
          editor, cursor_row, (size_t)content_width,
          &cursor_line, &cursor_offset) == 0)
      cursor_offset = 0;

    if (editor->cursor_column >= cursor_offset) {
      size_t cursor_bytes = editor->cursor_column - cursor_offset;
      cursor_x += (int)utf8_display_columns(
        editor->lines[editor->cursor_line] + cursor_offset,
        cursor_bytes);
    }
    if (cursor_x > x + 2 + content_width - 1)
      cursor_x = x + 2 + content_width - 1;

    /* Draw the cursor cell with the PC-98 cursor colors.  It is drawn at
       exactly the same position as the real terminal cursor, so there is
       only one visible cursor, including for UTF-8 text. */
    attron(COLOR_PAIR(21));
    if (editor->cursor_column < strlen(editor->lines[editor->cursor_line])) {
      const char *cursor_text =
        editor->lines[editor->cursor_line] + editor->cursor_column;
      size_t cursor_bytes = utf8_character_bytes(
        cursor_text,
        strlen(cursor_text));
      int cursor_width = cursor_bytes > 0
                           ? utf8_character_width(cursor_text, cursor_bytes)
                           : 0;

      if (cursor_bytes > 0 && cursor_width > 0)
        mvaddnstr(cursor_y, cursor_x, cursor_text, (int)cursor_bytes);
      else
        mvaddch(cursor_y, cursor_x, ' ');
    } else {
      mvaddch(cursor_y, cursor_x, ' ');
    }
    attroff(COLOR_PAIR(21));

    curs_set(1);
    move(cursor_y, cursor_x);
  }
}

static void set_terminal_cursor_color(int reset) {
#ifndef PC98FM_PDCURSES
  if (reset)
    fputs("\033]112\007", stdout);
  else
    fputs("\033]12;#00b000\007", stdout);
  fflush(stdout);
#else
  (void)reset;
#endif
}

static void panel_geometry(const AppState *app, int *x, int *width) {
  *x = app->active ? COLS / 2 : 1;
  *width = COLS / 2 - 2;
}

static int is_escape_key(int key) {
  if (key == 27) return 1;
#ifdef KEY_ESC
  if (key == KEY_ESC) return 1;
#endif
#ifdef KEY_CANCEL
  if (key == KEY_CANCEL) return 1;
#endif
#ifdef KEY_EXIT
  if (key == KEY_EXIT) return 1;
#endif
  return 0;
}

static int input_dialog(const AppState *app, const char *prompt,
                        char *buffer, size_t size, int confirmation) {
  int panel_x;
  int panel_width;
  int box_x;
  int box_width;
  int content_width;
  int top;
  int input_y;
  int bottom;
  size_t length;
  int result = confirmation ? -1 : 0;
  int answer = 0;

  if (size == 0 || (confirmation && size < 2)) return result;
  buffer[size - 1] = '\0';
  length = strlen(buffer);
  panel_geometry(app, &panel_x, &panel_width);

  box_x = panel_x + 1;
  box_width = panel_width - 2;
  content_width = box_width - 2;
  top = LINES - 5;
  input_y = LINES - 4;
  bottom = LINES - 3;
  if (content_width < 4 || top < 0) return result;

  timeout(-1);
  curs_set(1);

  while (1) {
    size_t display_start = length > (size_t)content_width
                             ? length - (size_t)content_width
                             : 0;
    int cursor_x = box_x + 1 + (int)(length - display_start);
    int ch;

    attron(COLOR_PAIR(1));
    mvhline(top, box_x + 1, ACS_HLINE, box_width - 2);
    mvhline(bottom, box_x + 1, ACS_HLINE, box_width - 2);
    mvvline(top + 1, box_x, ACS_VLINE, 1);
    mvvline(top + 1, box_x + box_width - 1, ACS_VLINE, 1);
    mvaddch(top, box_x, ACS_ULCORNER);
    mvaddch(top, box_x + box_width - 1, ACS_URCORNER);
    mvaddch(bottom, box_x, ACS_LLCORNER);
    mvaddch(bottom, box_x + box_width - 1, ACS_LRCORNER);
    mvaddnstr(top, box_x + 2, prompt, box_width - 4);
    attroff(COLOR_PAIR(1));

    attron(COLOR_PAIR(5));
    mvhline(input_y, box_x + 1, ' ', content_width);
    mvaddnstr(input_y, box_x + 1, buffer + display_start, content_width);
    attroff(COLOR_PAIR(5));

    if (cursor_x >= box_x + 1 + content_width)
      cursor_x = box_x + content_width;
    move(input_y, cursor_x);
    refresh();

    ch = getch();
    if (is_escape_key(ch)) {
      result = confirmation ? -1 : 0;
      break;
    }

    if (confirmation) {
      if (ch == 'y' || ch == 'Y') {
        answer = 1;
        buffer[0] = (char)ch;
        buffer[1] = '\0';
        length = 1;
      } else if (ch == 'n' || ch == 'N') {
        answer = 2;
        buffer[0] = (char)ch;
        buffer[1] = '\0';
        length = 1;
      } else if (ch == 10 || ch == KEY_ENTER) {
        if (answer != 0) {
          result = answer == 1 ? 1 : 0;
          break;
        }
      }
      continue;
    }

    if (ch == 10 || ch == KEY_ENTER) {
      result = 1;
      break;
    }
    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
      if (length > 0) buffer[--length] = '\0';
    } else if (ch >= 32 && ch <= 126 && length + 1 < size) {
      buffer[length++] = (char)ch;
      buffer[length] = '\0';
    }
  }

  timeout(250);
  curs_set(0);
  return result;
}

void ui_init(void) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
#ifdef NCURSES_VERSION
  set_escdelay(25);
#endif
  timeout(250);
  curs_set(0);

  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_BLACK, COLOR_CYAN);
    init_pair(6, COLOR_GREEN, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
    init_pair(8, COLOR_CYAN, COLOR_BLACK);
    init_pair(9, COLOR_CYAN, COLOR_BLACK);
    init_pair(10, COLOR_RED, COLOR_BLACK);
    init_pair(20, COLOR_BLACK, COLOR_WHITE);
    init_pair(21, COLOR_BLACK, COLOR_GREEN);
  }

  set_terminal_cursor_color(0);
}

void ui_shutdown(void) {
  endwin();
  set_terminal_cursor_color(1);
}

int ui_visible_rows(void) {
  return LINES - 7;
}

int ui_editor_columns(void) {
  int right_x = COLS / 2;
  int right_width = COLS / 2 - 2;
  /* Reserve one display cell for the editor's trailing horizontal ellipsis. */
  return right_x + right_width - 6;
}

void ui_force_redraw(void) {
  clearok(stdscr, TRUE);
  touchwin(stdscr);
}

void ui_show_easter_egg(void) {
  const char *message = "pc98fm by @mmoroca & arena.ai 2026";
  const char *hint = "Press any key";
  int message_length = (int)strlen(message);
  int hint_length = (int)strlen(hint);
  int box_width = message_length + 8;
  int box_height = 7;
  int left;
  int top;
  int key;

  if (box_width > COLS - 2) box_width = COLS - 2;
  if (box_width < 12) box_width = 12;
  if (box_height > LINES) box_height = LINES;
  left = (COLS - box_width) / 2;
  top = (LINES - box_height) / 2;

  erase();
  attron(COLOR_PAIR(1));
  for (int x = left + 1; x < left + box_width - 1; x++) {
    mvaddch(top, x, ACS_HLINE);
    mvaddch(top + box_height - 1, x, ACS_HLINE);
  }
  for (int y = top + 1; y < top + box_height - 1; y++) {
    mvaddch(y, left, ACS_VLINE);
    mvaddch(y, left + box_width - 1, ACS_VLINE);
  }
  mvaddch(top, left, ACS_ULCORNER);
  mvaddch(top, left + box_width - 1, ACS_URCORNER);
  mvaddch(top + box_height - 1, left, ACS_LLCORNER);
  mvaddch(top + box_height - 1, left + box_width - 1, ACS_LRCORNER);
  attroff(COLOR_PAIR(1));

  attron(COLOR_PAIR(2));
  if (message_length < box_width - 4)
    mvaddnstr(top + 2, left + (box_width - message_length) / 2,
              message, message_length);
  attroff(COLOR_PAIR(2));

  attron(COLOR_PAIR(6));
  if (hint_length < box_width - 4)
    mvaddnstr(top + box_height - 3,
              left + (box_width - hint_length) / 2, hint, hint_length);
  attroff(COLOR_PAIR(6));

  refresh();
  do {
    key = getch();
  } while (key == ERR);
  clear();
  refresh();
}

void ui_draw(AppState *app) {
  erase();

  if (LINES < 8) {
    mvprintw(0, 0, "Ventana demasiado pequena: se necesitan mas filas");
    draw_clock();
    refresh();
    return;
  }

  if (app->editor.active) {
    draw_clock();
    draw_editor(app);
    refresh();
    return;
  }

  if (app->viewer.active) {
    draw_viewer(app);
    refresh();
    return;
  }

  curs_set(0);
  draw_menu();
  draw_panel_info(&app->left, 1, COLS / 2 - 2);
  draw_panel_info(&app->right, COLS / 2, COLS / 2 - 2);
  draw_panel(&app->left, 1, COLS / 2 - 2, app->active == 0);
  draw_panel(&app->right, COLS / 2, COLS / 2 - 2, app->active == 1);
  draw_clock();
  refresh();
}

int ui_get_key(void) {
  int key = getch();

  /* Many terminals encode Ctrl+Alt+A as ESC followed by Ctrl-A. */
  if (key == 27) {
    int next = getch();
    if (next == 1) return UI_KEY_EASTER_EGG;
    if (next != ERR) ungetch(next);
    return 27;
  }

  if (key == 127 || key == 8) return UI_KEY_BACKSPACE;

  switch (key) {
    case KEY_UP: return UI_KEY_UP;
    case KEY_DOWN: return UI_KEY_DOWN;
    case KEY_LEFT: return UI_KEY_LEFT;
    case KEY_RIGHT: return UI_KEY_RIGHT;
    case KEY_BACKSPACE: return UI_KEY_BACKSPACE;
    case KEY_DC: return UI_KEY_DELETE;
    case KEY_PPAGE: return UI_KEY_PAGE_UP;
    case KEY_NPAGE: return UI_KEY_PAGE_DOWN;
    case KEY_HOME: return UI_KEY_HOME;
    case KEY_END: return UI_KEY_END;
    case KEY_ENTER: return UI_KEY_ENTER;
    case KEY_F(5): return UI_KEY_F5;
    case KEY_F(6): return UI_KEY_F6;
    case KEY_F(7): return UI_KEY_F7;
    case KEY_F(8): return UI_KEY_F8;
#ifdef KEY_RESIZE
    case KEY_RESIZE: return UI_KEY_RESIZE;
#endif
    case 10: return UI_KEY_ENTER;
    default: return key;
  }
}

int ui_input_callback(void *context, const char *prompt,
                      char *buffer, size_t size) {
  return input_dialog((const AppState *)context, prompt, buffer, size, 0);
}

int ui_confirm_callback(void *context, const char *prompt) {
  char answer[2] = "";

  return input_dialog((const AppState *)context, prompt,
                      answer, sizeof(answer), 1);
}

void ui_progress_callback(void *context, const char *message) {
  const AppState *app = (const AppState *)context;
  int panel_x;
  int panel_width;
  int box_x;
  int box_width;
  int content_width;
  int top;
  int input_y;
  int bottom;

  if (!app || !message) return;

  panel_geometry(app, &panel_x, &panel_width);
  box_x = panel_x + 1;
  box_width = panel_width - 2;
  content_width = box_width - 2;
  top = LINES - 5;
  input_y = LINES - 4;
  bottom = LINES - 3;

  if (content_width < 4 || top < 0) return;

  attron(COLOR_PAIR(1));
  mvhline(top, box_x + 1, ACS_HLINE, box_width - 2);
  mvhline(bottom, box_x + 1, ACS_HLINE, box_width - 2);
  mvvline(top + 1, box_x, ACS_VLINE, 1);
  mvvline(top + 1, box_x + box_width - 1, ACS_VLINE, 1);
  mvaddch(top, box_x, ACS_ULCORNER);
  mvaddch(top, box_x + box_width - 1, ACS_URCORNER);
  mvaddch(bottom, box_x, ACS_LLCORNER);
  mvaddch(bottom, box_x + box_width - 1, ACS_LRCORNER);
  mvaddnstr(top, box_x + 2, message, box_width - 4);
  attroff(COLOR_PAIR(1));

  attron(COLOR_PAIR(5));
  mvhline(input_y, box_x + 1, ' ', content_width);
  attroff(COLOR_PAIR(5));
  curs_set(0);
  refresh();
}

int ui_execute_callback(void *context, const char *path,
                        const char *working_directory) {
  int result;

  (void)context;
  def_prog_mode();
  endwin();
  result = fs_execute(path, working_directory);
  reset_prog_mode();
  keypad(stdscr, TRUE);
  timeout(250);
  clear();
  refresh();
  return result;
}

int ui_drive_callback(void *context, const FsDrive *drives,
                      size_t count, size_t *selected) {
  char prompt[FS_PATH_MAX + 64];
  char answer[32] = "";
  size_t used;
  char *end;
  long choice;

  used = (size_t)snprintf(prompt, sizeof(prompt), "Drive ");
  for (size_t i = 0; i < count && used < sizeof(prompt); i++) {
    int written = snprintf(prompt + used, sizeof(prompt) - used,
                           "%zu:%s ", i + 1, drives[i].name);
    if (written < 0) break;
    used += (size_t)written;
  }

  if (!input_dialog((const AppState *)context, prompt,
                    answer, sizeof(answer), 0))
    return 0;

  choice = strtol(answer, &end, 10);
  if (end == answer || *end != '\0' || choice < 1 ||
      (size_t)choice > count)
    return -1;

  *selected = (size_t)choice - 1;
  return 1;
}

void ui_beep_callback(void *context) {
  (void)context;
  beep();
}
