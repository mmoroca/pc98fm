#include "core.h"
#include "archive_zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void action_failed(const CoreCallbacks *callbacks) {
  if (callbacks && callbacks->beep)
    callbacks->beep(callbacks->context);
}

static void action_progress(const CoreCallbacks *callbacks,
                            const char *message) {
  if (callbacks && callbacks->progress)
    callbacks->progress(callbacks->context, message);
}

static int ascii_casecmp(const char *a, const char *b) {
  while (*a && *b) {
    unsigned char ca = (unsigned char)*a;
    unsigned char cb = (unsigned char)*b;

    if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + ('a' - 'A'));
    if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + ('a' - 'A'));
    if (ca != cb) return ca < cb ? -1 : 1;

    a++;
    b++;
  }

  return *a == *b ? 0 : (*a ? 1 : -1);
}

static SortMode compare_mode = SORT_NAME;
static int compare_descending;

static const char *file_extension(const char *name) {
  const char *dot = strrchr(name, '.');

  if (!dot || dot == name || dot[1] == '\0') return "";
  return dot + 1;
}

static int special_entry_rank(const char *name) {
  if (strcmp(name, ".") == 0) return 0;
  if (strcmp(name, "..") == 0) return 1;
  return 2;
}

static int compare_entries(const void *a, const void *b) {
  const FsEntry *first = (const FsEntry *)a;
  const FsEntry *second = (const FsEntry *)b;
  int first_rank = special_entry_rank(first->name);
  int second_rank = special_entry_rank(second->name);
  int result;

  /* Keep . and .. at the top, independently of the selected sort mode. */
  if (first_rank != second_rank) return first_rank < second_rank ? -1 : 1;
  if (first_rank < 2) return 0;

  /* Keep directories before files for every sorting criterion. */
  if (first->is_dir != second->is_dir)
    return second->is_dir - first->is_dir;

  switch (compare_mode) {
    case SORT_EXTENSION:
      result = ascii_casecmp(file_extension(first->name),
                             file_extension(second->name));
      if (result == 0) result = ascii_casecmp(first->name, second->name);
      break;

    case SORT_SIZE:
      if (first->size < second->size)
        result = -1;
      else if (first->size > second->size)
        result = 1;
      else
        result = ascii_casecmp(first->name, second->name);
      break;

    case SORT_DATE:
      if (first->mtime < second->mtime)
        result = -1;
      else if (first->mtime > second->mtime)
        result = 1;
      else
        result = ascii_casecmp(first->name, second->name);
      break;

    case SORT_NAME:
    default:
      result = ascii_casecmp(first->name, second->name);
      break;
  }

  return compare_descending ? -result : result;
}

static void sort_panel(Panel *panel) {
  compare_mode = panel->sort_mode;
  compare_descending = panel->sort_descending;
  qsort(panel->directory.entries, panel->directory.count,
        sizeof(panel->directory.entries[0]), compare_entries);
}

static int valid_name(const char *name) {
  if (name[0] == '\0' || strcmp(name, ".") == 0 ||
      strcmp(name, "..") == 0)
    return 0;

  return strchr(name, '/') == NULL && strchr(name, '\\') == NULL;
}

static int panel_contains_name(const Panel *panel, const char *name) {
  for (size_t i = 0; i < panel->directory.count; i++) {
    if (strcmp(panel->directory.entries[i].name, name) == 0) return 1;
  }
  return 0;
}

static int select_name(Panel *panel, const char *name) {
  for (size_t i = 0; i < panel->directory.count; i++) {
    if (strcmp(panel->directory.entries[i].name, name) == 0) {
      panel->selected = i;
      panel->scroll = 0;
      return 1;
    }
  }

  return 0;
}

int core_select_name(Panel *panel, const char *name) {
  return select_name(panel, name);
}

int core_reload_panel(Panel *panel, int preserve_selection) {
  char selected_name[FS_NAME_MAX];
  int had_selection = preserve_selection &&
                      panel->selected < panel->directory.count;

  selected_name[0] = '\0';
  if (had_selection) {
    strncpy(selected_name,
            panel->directory.entries[panel->selected].name,
            sizeof(selected_name) - 1);
    selected_name[sizeof(selected_name) - 1] = '\0';
  }

  if (!fs_load_directory(panel->path, &panel->directory)) return 0;

  sort_panel(panel);

  if (panel->directory.count == 0 || !preserve_selection) {
    panel->selected = 0;
    panel->scroll = 0;
  } else if (!had_selection || !select_name(panel, selected_name)) {
    if (panel->selected >= panel->directory.count)
      panel->selected = panel->directory.count - 1;
    if (panel->scroll >= panel->directory.count)
      panel->scroll = panel->directory.count - 1;
  }

  return 1;
}

int core_init(AppState *app, const char *path) {
  memset(app, 0, sizeof(*app));
  strncpy(app->left.path, path, sizeof(app->left.path) - 1);
  app->left.path[sizeof(app->left.path) - 1] = '\0';
  strncpy(app->right.path, path, sizeof(app->right.path) - 1);
  app->right.path[sizeof(app->right.path) - 1] = '\0';

  return core_reload_panel(&app->left, 0) &&
         core_reload_panel(&app->right, 0);
}

Panel *core_active_panel(AppState *app) {
  return app->active ? &app->right : &app->left;
}

Panel *core_other_panel(AppState *app) {
  return app->active ? &app->left : &app->right;
}

void core_toggle_panel(AppState *app) {
  app->active = !app->active;
}

void core_ensure_selection_visible(Panel *panel, int visible_rows) {
  size_t maximum_scroll;

  if (visible_rows <= 0 || panel->directory.count == 0) {
    panel->selected = 0;
    panel->scroll = 0;
    return;
  }

  if (panel->selected >= panel->directory.count)
    panel->selected = panel->directory.count - 1;

  if (panel->selected < panel->scroll)
    panel->scroll = panel->selected;
  else if (panel->selected >= panel->scroll + (size_t)visible_rows)
    panel->scroll = panel->selected - (size_t)visible_rows + 1;

  maximum_scroll = panel->directory.count > (size_t)visible_rows
                     ? panel->directory.count - (size_t)visible_rows
                     : 0;
  if (panel->scroll > maximum_scroll) panel->scroll = maximum_scroll;
}

void core_move_selection(Panel *panel, int amount, int visible_rows) {
  size_t distance;
  size_t last;

  if (panel->directory.count == 0) return;

  if (amount < 0) {
    distance = (size_t)(-amount);
    panel->selected = panel->selected > distance
                       ? panel->selected - distance
                       : 0;
  } else {
    distance = (size_t)amount;
    last = panel->directory.count - 1;
    panel->selected = panel->selected + distance < panel->selected
                       ? last
                       : panel->selected + distance;
    if (panel->selected > last) panel->selected = last;
  }

  core_ensure_selection_visible(panel, visible_rows);
}

int core_reload_panels(AppState *app, const CoreCallbacks *callbacks) {
  int success = 1;

  if (!core_reload_panel(&app->left, 1)) success = 0;
  if (!core_reload_panel(&app->right, 1)) success = 0;
  if (!success) action_failed(callbacks);
  return success;
}

int core_select_drive(AppState *app, const CoreCallbacks *callbacks) {
  FsDrive drives[FS_MAX_DRIVES];
  Panel *panel = core_active_panel(app);
  char oldpath[FS_PATH_MAX];
  size_t count = 0;
  size_t selected = 0;
  int result;

  if (!fs_list_drives(drives, FS_MAX_DRIVES, &count) || count == 0 ||
      !callbacks || !callbacks->drive) {
    action_failed(callbacks);
    return 0;
  }

  result = callbacks->drive(callbacks->context, drives, count, &selected);
  if (result == 0) return 1;
  if (result < 0 || selected >= count) {
    action_failed(callbacks);
    return 0;
  }

  strncpy(oldpath, panel->path, sizeof(oldpath) - 1);
  oldpath[sizeof(oldpath) - 1] = '\0';
  strncpy(panel->path, drives[selected].path, sizeof(panel->path) - 1);
  panel->path[sizeof(panel->path) - 1] = '\0';

  action_progress(callbacks, "Loading...");
  if (!core_reload_panel(panel, 0)) {
    strcpy(panel->path, oldpath);
    action_failed(callbacks);
    return 0;
  }

  return 1;
}

int core_enter_directory(AppState *app, const CoreCallbacks *callbacks) {
  Panel *panel = core_active_panel(app);
  FsEntry *entry;
  char oldpath[FS_PATH_MAX];
  char newpath[FS_PATH_MAX];
  char parent_name[FS_NAME_MAX];

  if (panel->directory.count == 0 ||
      panel->selected >= panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &panel->directory.entries[panel->selected];
  parent_name[0] = '\0';

  if (strcmp(entry->name, ".") == 0) {
    action_progress(callbacks, "Loading...");
    if (!core_reload_panel(panel, 1)) {
      action_failed(callbacks);
      return 0;
    }
    return 1;
  }

  if (!entry->is_dir) {
    action_failed(callbacks);
    return 0;
  }

  strncpy(oldpath, panel->path, sizeof(oldpath) - 1);
  oldpath[sizeof(oldpath) - 1] = '\0';

  if (strcmp(entry->name, "..") == 0) {
    strncpy(newpath, panel->path, sizeof(newpath) - 1);
    newpath[sizeof(newpath) - 1] = '\0';

    if (!fs_path_basename(newpath, parent_name,
                          sizeof(parent_name)) ||
        !fs_parent_path(newpath, sizeof(newpath))) {
      action_failed(callbacks);
      return 0;
    }
  } else if (!fs_join_path(newpath, sizeof(newpath), panel->path,
                          entry->name)) {
    action_failed(callbacks);
    return 0;
  }

  strcpy(panel->path, newpath);
  action_progress(callbacks, "Loading...");
  if (!core_reload_panel(panel, 0)) {
    strcpy(panel->path, oldpath);
    action_failed(callbacks);
    return 0;
  }

  if (parent_name[0]) select_name(panel, parent_name);
  return 1;
}

static int request_input(const CoreCallbacks *callbacks, const char *prompt,
                         char *buffer, size_t size) {
  return callbacks && callbacks->input &&
         callbacks->input(callbacks->context, prompt, buffer, size);
}

static int request_confirmation(const CoreCallbacks *callbacks,
                                const char *prompt) {
  if (!callbacks || !callbacks->confirm) return -1;
  return callbacks->confirm(callbacks->context, prompt);
}

int core_sort(AppState *app, const CoreCallbacks *callbacks) {
  Panel *panel = core_active_panel(app);
  char answer[16] = "";
  char selected_name[FS_NAME_MAX];
  SortMode mode;
  int had_selection = panel->selected < panel->directory.count;
  char choice;

  if (!request_input(callbacks,
                     "Sort by [N]ame, [E]xt, [S]ize or [D]ate:",
                     answer, sizeof(answer)))
    return 1;

  choice = answer[0];
  if (choice >= 'a' && choice <= 'z')
    choice = (char)(choice - ('a' - 'A'));

  switch (choice) {
    case 'N': mode = SORT_NAME; break;
    case 'E': mode = SORT_EXTENSION; break;
    case 'S': mode = SORT_SIZE; break;
    case 'D': mode = SORT_DATE; break;
    default:
      action_failed(callbacks);
      return 0;
  }

  selected_name[0] = '\0';
  if (had_selection) {
    strncpy(selected_name,
            panel->directory.entries[panel->selected].name,
            sizeof(selected_name) - 1);
    selected_name[sizeof(selected_name) - 1] = '\0';
  }

  if (panel->sort_mode == mode)
    panel->sort_descending = !panel->sort_descending;
  else {
    panel->sort_mode = mode;
    panel->sort_descending = 0;
  }

  sort_panel(panel);
  if (had_selection) select_name(panel, selected_name);
  return 1;
}

int core_execute(AppState *app, const CoreCallbacks *callbacks) {
  Panel *panel = core_active_panel(app);
  FsEntry *entry;
  char path[FS_PATH_MAX];

  if (panel->directory.count == 0 ||
      panel->selected >= panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &panel->directory.entries[panel->selected];
  if (entry->is_dir || !entry->is_executable ||
      !fs_join_path(path, sizeof(path), panel->path, entry->name) ||
      !callbacks || !callbacks->execute) {
    action_failed(callbacks);
    return 0;
  }

  action_progress(callbacks, "Executing...");
  if (!callbacks->execute(callbacks->context, path, panel->path)) {
    action_failed(callbacks);
    return 0;
  }

  action_progress(callbacks, "Loading...");
  if (!core_reload_panels(app, callbacks)) return 0;
  return 1;
}

static void clear_viewer(TextViewer *viewer) {
  for (size_t i = 0; i < viewer->count; i++) free(viewer->lines[i]);
  free(viewer->lines);
  memset(viewer, 0, sizeof(*viewer));
}

static int read_text_line(FILE *file, char **line) {
  char *buffer = NULL;
  size_t length = 0;
  size_t capacity = 0;
  int character;

  while ((character = fgetc(file)) != EOF) {
    if (character == '\n') break;
    if (character == '\r') continue;

    if (length + 1 >= capacity) {
      size_t new_capacity = capacity == 0 ? 128 : capacity * 2;
      char *new_buffer = realloc(buffer, new_capacity);
      if (!new_buffer) {
        free(buffer);
        return 0;
      }
      buffer = new_buffer;
      capacity = new_capacity;
    }

    buffer[length++] = (char)character;
  }

  if (character == EOF && length == 0) {
    free(buffer);
    return 0;
  }

  if (!buffer) {
    buffer = malloc(1);
    if (!buffer) return 0;
  }

  buffer[length] = '\0';
  *line = buffer;
  return 1;
}

static void free_text_lines(char **lines, size_t count) {
  for (size_t i = 0; i < count; i++) free(lines[i]);
  free(lines);
}

static int load_text_lines(const char *path, char ***lines, size_t *count) {
  FILE *file = fopen(path, "rb");
  char **loaded_lines = NULL;
  size_t loaded_count = 0;

  if (!file) return 0;

  while (1) {
    char *line;
    char **new_lines;

    if (!read_text_line(file, &line)) break;

    new_lines = realloc(loaded_lines,
                        (loaded_count + 1) * sizeof(*loaded_lines));
    if (!new_lines) {
      free(line);
      fclose(file);
      free_text_lines(loaded_lines, loaded_count);
      return 0;
    }

    loaded_lines = new_lines;
    loaded_lines[loaded_count++] = line;
  }

  if (ferror(file)) {
    fclose(file);
    free_text_lines(loaded_lines, loaded_count);
    return 0;
  }

  fclose(file);
  *lines = loaded_lines;
  *count = loaded_count;
  return 1;
}

static int load_text_viewer(TextViewer *viewer, const char *path) {
  size_t path_length;

  if (!load_text_lines(path, &viewer->lines, &viewer->count)) return 0;

  path_length = strlen(path);
  if (path_length >= sizeof(viewer->path))
    path_length = sizeof(viewer->path) - 1;
  memcpy(viewer->path, path, path_length);
  viewer->path[path_length] = '\0';
  viewer->active = 1;
  return 1;
}

int core_view_file(AppState *app, const CoreCallbacks *callbacks) {
  Panel *panel = core_active_panel(app);
  FsEntry *entry;
  TextViewer loaded;
  char path[FS_PATH_MAX];

  if (panel->directory.count == 0 ||
      panel->selected >= panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &panel->directory.entries[panel->selected];
  if (entry->is_dir || !fs_join_path(path, sizeof(path), panel->path,
                                     entry->name)) {
    action_failed(callbacks);
    return 0;
  }

  memset(&loaded, 0, sizeof(loaded));
  action_progress(callbacks, "Opening...");
  if (!load_text_viewer(&loaded, path)) {
    action_failed(callbacks);
    return 0;
  }

  clear_viewer(&app->viewer);
  app->viewer = loaded;
  return 1;
}

void core_close_viewer(AppState *app) {
  clear_viewer(&app->viewer);
}

void core_ensure_viewer_visible(TextViewer *viewer, int visible_rows) {
  size_t maximum_top;

  if (visible_rows <= 0 || viewer->count == 0) {
    viewer->top_line = 0;
    return;
  }

  maximum_top = viewer->count > (size_t)visible_rows
                  ? viewer->count - (size_t)visible_rows
                  : 0;
  if (viewer->top_line > maximum_top) viewer->top_line = maximum_top;
}

void core_move_viewer(TextViewer *viewer, int amount, int visible_rows) {
  size_t distance;

  if (viewer->count == 0) return;

  if (amount < 0) {
    distance = (size_t)(-amount);
    viewer->top_line = viewer->top_line > distance
                         ? viewer->top_line - distance
                         : 0;
  } else {
    size_t maximum_top = viewer->count > (size_t)visible_rows
                           ? viewer->count - (size_t)visible_rows
                           : 0;
    distance = (size_t)amount;
    if (viewer->top_line + distance < viewer->top_line ||
        viewer->top_line + distance > maximum_top)
      viewer->top_line = maximum_top;
    else
      viewer->top_line += distance;
  }

  core_ensure_viewer_visible(viewer, visible_rows);
}

static void clear_editor(TextEditor *editor) {
  free_text_lines(editor->lines, editor->count);
  memset(editor, 0, sizeof(*editor));
}

static size_t editor_line_rows(const TextEditor *editor, size_t line,
                               size_t columns) {
  size_t length;

  if (!editor->wrap || columns == 0) return 1;
  length = strlen(editor->lines[line]);
  return length == 0 ? 1 : (length + columns - 1) / columns;
}

static size_t editor_total_rows(const TextEditor *editor, size_t columns) {
  size_t total = 0;
  for (size_t i = 0; i < editor->count; i++)
    total += editor_line_rows(editor, i, columns);
  return total;
}

size_t core_editor_cursor_row(const TextEditor *editor, size_t columns) {
  size_t row = 0;

  if (columns == 0) columns = 1;
  for (size_t i = 0; i < editor->cursor_line && i < editor->count; i++)
    row += editor_line_rows(editor, i, columns);

  if (editor->wrap && editor->cursor_column > 0)
    row += (editor->cursor_column - 1) / columns;
  return row;
}

int core_editor_row_at(const TextEditor *editor, size_t row, size_t columns,
                       size_t *line, size_t *offset) {
  size_t current = 0;

  if (columns == 0) columns = 1;
  for (size_t i = 0; i < editor->count; i++) {
    size_t rows = editor_line_rows(editor, i, columns);
    if (row < current + rows) {
      size_t position = editor->wrap
                          ? (row - current) * columns
                          : 0;
      size_t length = strlen(editor->lines[i]);
      if (position > length) position = length;
      *line = i;
      *offset = position;
      return 1;
    }
    current += rows;
  }

  return 0;
}

void core_editor_ensure_visible(TextEditor *editor, int columns,
                                int visible_rows) {
  size_t cursor_row;
  size_t total_rows;
  size_t maximum_top;

  if (columns < 1) columns = 1;
  if (visible_rows < 1) visible_rows = 1;
  if (editor->count == 0) return;

  cursor_row = core_editor_cursor_row(editor, (size_t)columns);
  total_rows = editor_total_rows(editor, (size_t)columns);

  if (cursor_row < editor->top_row)
    editor->top_row = cursor_row;
  else if (cursor_row >= editor->top_row + (size_t)visible_rows)
    editor->top_row = cursor_row - (size_t)visible_rows + 1;

  maximum_top = total_rows > (size_t)visible_rows
                  ? total_rows - (size_t)visible_rows
                  : 0;
  if (editor->top_row > maximum_top) editor->top_row = maximum_top;

  if (editor->wrap) {
    editor->horizontal_scroll = 0;
  } else if (editor->cursor_column < editor->horizontal_scroll) {
    editor->horizontal_scroll = editor->cursor_column;
  } else if (editor->cursor_column >=
             editor->horizontal_scroll + (size_t)columns) {
    editor->horizontal_scroll = editor->cursor_column -
                                (size_t)columns + 1;
  }
}

static int utf8_continuation(unsigned char character);
static size_t previous_character(const char *line, size_t column);
static size_t next_character(const char *line, size_t length, size_t column);

static void editor_set_position(TextEditor *editor, size_t line,
                                size_t column, int columns, int rows) {
  size_t length;

  if (editor->count == 0) return;
  if (line >= editor->count) line = editor->count - 1;
  length = strlen(editor->lines[line]);
  if (column > length) column = length;
  if (column < length && utf8_continuation(
        (unsigned char)editor->lines[line][column]))
    column = previous_character(editor->lines[line], column);
  editor->cursor_line = line;
  editor->cursor_column = column;
  core_editor_ensure_visible(editor, columns, rows);
}

static void editor_move_vertical(TextEditor *editor, int direction,
                                 int columns, int visible_rows) {
  size_t desired;
  size_t line;
  size_t offset;

  if (editor->count == 0) return;
  if (columns < 1) columns = 1;

  desired = editor->preferred_valid
              ? editor->preferred_column
              : (editor->wrap ? editor->cursor_column % (size_t)columns
                               : editor->cursor_column);

  if (editor->wrap) {
    size_t current = core_editor_cursor_row(editor, (size_t)columns);
    size_t total = editor_total_rows(editor, (size_t)columns);
    size_t target;

    if (direction < 0) target = current > 0 ? current - 1 : 0;
    else target = current + 1 < total ? current + 1 : total - 1;

    if (core_editor_row_at(editor, target, (size_t)columns, &line, &offset)) {
      size_t length = strlen(editor->lines[line]);
      editor_set_position(editor, line,
                          offset + (desired < length - offset
                                      ? desired : length - offset),
                          columns, visible_rows);
    }
  } else {
    line = editor->cursor_line;
    if (direction < 0) {
      if (line > 0) line--;
    } else if (line + 1 < editor->count) {
      line++;
    }
    editor_set_position(editor, line, desired, columns, visible_rows);
  }

  editor->preferred_column = desired;
  editor->preferred_valid = 1;
}

static int utf8_continuation(unsigned char character) {
  return (character & 0xC0) == 0x80;
}

static size_t previous_character(const char *line, size_t column) {
  if (column == 0) return 0;
  column--;
  while (column > 0 && utf8_continuation((unsigned char)line[column]))
    column--;
  return column;
}

static size_t next_character(const char *line, size_t length, size_t column) {
  if (column >= length) return length;
  column++;
  while (column < length && utf8_continuation((unsigned char)line[column]))
    column++;
  return column;
}

void core_editor_move_left(TextEditor *editor, int columns, int rows) {
  if (editor->cursor_column > 0) {
    editor->cursor_column = previous_character(
      editor->lines[editor->cursor_line], editor->cursor_column);
  } else if (editor->cursor_line > 0) {
    editor->cursor_line--;
    editor->cursor_column = strlen(editor->lines[editor->cursor_line]);
  }
  editor->preferred_valid = 0;
  core_editor_ensure_visible(editor, columns, rows);
}

void core_editor_move_right(TextEditor *editor, int columns, int rows) {
  size_t length = strlen(editor->lines[editor->cursor_line]);

  if (editor->cursor_column < length) {
    editor->cursor_column = next_character(
      editor->lines[editor->cursor_line], length, editor->cursor_column);
  } else if (editor->cursor_line + 1 < editor->count) {
    editor->cursor_line++;
    editor->cursor_column = 0;
  }
  editor->preferred_valid = 0;
  core_editor_ensure_visible(editor, columns, rows);
}

void core_editor_move_up(TextEditor *editor, int columns, int rows) {
  editor_move_vertical(editor, -1, columns, rows);
}

void core_editor_move_down(TextEditor *editor, int columns, int rows) {
  editor_move_vertical(editor, 1, columns, rows);
}

void core_editor_home(TextEditor *editor, int columns, int rows) {
  if (editor->wrap) {
    editor->cursor_column = (editor->cursor_column / (size_t)columns) *
                            (size_t)columns;
  } else {
    editor->cursor_column = 0;
  }
  editor->preferred_valid = 0;
  core_editor_ensure_visible(editor, columns, rows);
}

void core_editor_end(TextEditor *editor, int columns, int rows) {
  size_t length = strlen(editor->lines[editor->cursor_line]);

  if (editor->wrap && columns > 0) {
    size_t start = (editor->cursor_column / (size_t)columns) *
                   (size_t)columns;
    editor->cursor_column = start +
      (length - start < (size_t)columns ? length - start : (size_t)columns);
  } else {
    editor->cursor_column = length;
  }
  editor->preferred_valid = 0;
  core_editor_ensure_visible(editor, columns, rows);
}

void core_editor_insert(TextEditor *editor, int character,
                        int columns, int visible_rows) {
  char *line = editor->lines[editor->cursor_line];
  size_t length = strlen(line);
  char *new_line = realloc(line, length + 2);

  if (!new_line) return;
  memmove(new_line + editor->cursor_column + 1,
          new_line + editor->cursor_column,
          length - editor->cursor_column + 1);
  new_line[editor->cursor_column++] = (char)character;
  editor->lines[editor->cursor_line] = new_line;
  editor->modified = 1;
  editor->preferred_valid = 0;
  core_editor_ensure_visible(editor, columns, visible_rows);
}

void core_editor_newline(TextEditor *editor, int columns, int visible_rows) {
  char *line = editor->lines[editor->cursor_line];
  size_t length = strlen(line);
  char *right = malloc(length - editor->cursor_column + 1);
  char **new_lines;

  if (!right) return;
  memcpy(right, line + editor->cursor_column,
         length - editor->cursor_column + 1);
  new_lines = realloc(editor->lines,
                      (editor->count + 1) * sizeof(*editor->lines));
  if (!new_lines) {
    free(right);
    return;
  }

  editor->lines = new_lines;
  line[editor->cursor_column] = '\0';
  memmove(&editor->lines[editor->cursor_line + 2],
          &editor->lines[editor->cursor_line + 1],
          (editor->count - editor->cursor_line - 1) *
          sizeof(*editor->lines));
  editor->lines[editor->cursor_line + 1] = right;
  editor->count++;
  editor->cursor_line++;
  editor->cursor_column = 0;
  editor->modified = 1;
  editor->preferred_valid = 0;
  core_editor_ensure_visible(editor, columns, visible_rows);
}

void core_editor_backspace(TextEditor *editor, int columns, int visible_rows) {
  char *line = editor->lines[editor->cursor_line];
  size_t length = strlen(line);

  if (editor->cursor_column > 0) {
    size_t start = previous_character(line, editor->cursor_column);
    memmove(line + start,
            line + editor->cursor_column,
            length - editor->cursor_column + 1);
    editor->cursor_column = start;
  } else if (editor->cursor_line > 0) {
    size_t previous = editor->cursor_line - 1;
    size_t previous_length = strlen(editor->lines[previous]);
    char *merged = realloc(editor->lines[previous],
                           previous_length + length + 1);
    if (!merged) return;
    memcpy(merged + previous_length, line, length + 1);
    free(line);
    editor->lines[previous] = merged;
    memmove(&editor->lines[editor->cursor_line],
            &editor->lines[editor->cursor_line + 1],
            (editor->count - editor->cursor_line - 1) *
            sizeof(*editor->lines));
    editor->count--;
    editor->cursor_line = previous;
    editor->cursor_column = previous_length;
  } else {
    return;
  }

  editor->modified = 1;
  editor->preferred_valid = 0;
  core_editor_ensure_visible(editor, columns, visible_rows);
}

void core_editor_delete(TextEditor *editor, int columns, int visible_rows) {
  char *line = editor->lines[editor->cursor_line];
  size_t length = strlen(line);

  if (editor->cursor_column < length) {
    size_t end = next_character(line, length, editor->cursor_column);
    memmove(line + editor->cursor_column,
            line + end,
            length - end + 1);
  } else if (editor->cursor_line + 1 < editor->count) {
    char *next = editor->lines[editor->cursor_line + 1];
    size_t next_length = strlen(next);
    char *merged = realloc(line, length + next_length + 1);
    if (!merged) return;
    memcpy(merged + length, next, next_length + 1);
    free(next);
    editor->lines[editor->cursor_line] = merged;
    memmove(&editor->lines[editor->cursor_line + 1],
            &editor->lines[editor->cursor_line + 2],
            (editor->count - editor->cursor_line - 2) *
            sizeof(*editor->lines));
    editor->count--;
  } else {
    return;
  }

  editor->modified = 1;
  editor->preferred_valid = 0;
  core_editor_ensure_visible(editor, columns, visible_rows);
}

void core_toggle_editor_wrap(AppState *app, int columns, int visible_rows) {
  app->editor.wrap = !app->editor.wrap;
  if (app->editor.wrap) app->editor.horizontal_scroll = 0;
  core_editor_ensure_visible(&app->editor, columns, visible_rows);
}

int core_edit_file(AppState *app, const CoreCallbacks *callbacks) {
  Panel *panel = core_active_panel(app);
  FsEntry *entry;
  TextEditor loaded;
  char path[FS_PATH_MAX];

  if (panel->directory.count == 0 ||
      panel->selected >= panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &panel->directory.entries[panel->selected];
  if (entry->is_dir || !fs_join_path(path, sizeof(path), panel->path,
                                     entry->name)) {
    action_failed(callbacks);
    return 0;
  }

  memset(&loaded, 0, sizeof(loaded));
  action_progress(callbacks, "Opening...");
  if (!load_text_lines(path, &loaded.lines, &loaded.count)) {
    action_failed(callbacks);
    return 0;
  }

  if (loaded.count == 0) {
    loaded.lines = malloc(sizeof(*loaded.lines));
    if (!loaded.lines) {
      free_text_lines(loaded.lines, loaded.count);
      action_failed(callbacks);
      return 0;
    }
    loaded.lines[0] = malloc(1);
    if (!loaded.lines[0]) {
      free_text_lines(loaded.lines, loaded.count);
      action_failed(callbacks);
      return 0;
    }
    loaded.lines[0][0] = '\0';
    loaded.count = 1;
  }

  {
    size_t path_length = strlen(path);
    if (path_length >= sizeof(loaded.path))
      path_length = sizeof(loaded.path) - 1;
    memcpy(loaded.path, path, path_length);
    loaded.path[path_length] = '\0';
  }
  loaded.active = 1;
  clear_editor(&app->editor);
  app->editor = loaded;
  return 1;
}

int core_save_editor(AppState *app, const CoreCallbacks *callbacks) {
  TextEditor *editor = &app->editor;
  FILE *file;

  if (!editor->active) return 0;
  action_progress(callbacks, "Saving...");
  file = fopen(editor->path, "wb");
  if (!file) {
    action_failed(callbacks);
    return 0;
  }

  for (size_t i = 0; i < editor->count; i++) {
    if (fputs(editor->lines[i], file) == EOF || fputc('\n', file) == EOF) {
      fclose(file);
      action_failed(callbacks);
      return 0;
    }
  }

  if (fclose(file) != 0) {
    action_failed(callbacks);
    return 0;
  }

  editor->modified = 0;
  return 1;
}

int core_close_editor(AppState *app, const CoreCallbacks *callbacks) {
  int decision;

  if (!app->editor.active) return 1;

  if (app->editor.modified) {
    decision = request_confirmation(callbacks, "Save changes? Y/N");
    if (decision < 0) return 0;
    if (decision == 1 && !core_save_editor(app, callbacks)) return 0;
  }

  clear_editor(&app->editor);
  return 1;
}

int core_copy(AppState *app, const CoreCallbacks *callbacks) {
  Panel *source_panel = core_active_panel(app);
  Panel *target_panel = core_other_panel(app);
  FsEntry *entry;
  char source[FS_PATH_MAX];
  char destination[FS_PATH_MAX];
  char prompt[FS_PATH_MAX + 32];

  if (source_panel->directory.count == 0 ||
      source_panel->selected >= source_panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &source_panel->directory.entries[source_panel->selected];
  if (!valid_name(entry->name) ||
      !fs_join_path(source, sizeof(source), source_panel->path, entry->name) ||
      !fs_join_path(destination, sizeof(destination), target_panel->path,
                     entry->name)) {
    action_failed(callbacks);
    return 0;
  }

  if (fs_path_exists(destination) ||
      panel_contains_name(target_panel, entry->name)) {
    snprintf(prompt, sizeof(prompt), "Overwrite %s? Y/N", entry->name);
    if (request_confirmation(callbacks, prompt) != 1) return 1;
  }

  /* Copying a file onto its own path is a safe no-op after confirmation. */
  if (strcmp(source, destination) == 0) return 1;

  action_progress(callbacks, "Copying...");
  if (!fs_copy(source, destination)) {
    action_failed(callbacks);
    return 0;
  }

  return core_reload_panels(app, callbacks);
}

static int has_zip_extension(const char *name) {
  size_t length = strlen(name);

  return length >= 4 && ascii_casecmp(name + length - 4, ".zip") == 0;
}

int core_pack(AppState *app, const CoreCallbacks *callbacks) {
  Panel *source_panel = core_active_panel(app);
  Panel *target_panel = core_other_panel(app);
  FsEntry *entry;
  char source[FS_PATH_MAX];
  char archive_name[FS_NAME_MAX];
  char destination[FS_PATH_MAX];
  char prompt[FS_PATH_MAX + 32];
  int written;

  if (source_panel->directory.count == 0 ||
      source_panel->selected >= source_panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &source_panel->directory.entries[source_panel->selected];
  if (!valid_name(entry->name) ||
      !fs_join_path(source, sizeof(source), source_panel->path, entry->name)) {
    action_failed(callbacks);
    return 0;
  }

  written = snprintf(archive_name, sizeof(archive_name), "%s.zip",
                     entry->name);
  if (written < 0 || (size_t)written >= sizeof(archive_name) ||
      !fs_join_path(destination, sizeof(destination), target_panel->path,
                    archive_name)) {
    action_failed(callbacks);
    return 0;
  }

  if (fs_path_exists(destination)) {
    snprintf(prompt, sizeof(prompt), "Overwrite %s? Y/N", archive_name);
    if (request_confirmation(callbacks, prompt) != 1) return 1;
  }

  action_progress(callbacks, "Packing...");
  if (!archive_pack(source, destination)) {
    action_failed(callbacks);
    return 0;
  }

  if (!core_reload_panels(app, callbacks)) return 0;
  core_select_name(target_panel, archive_name);
  return 1;
}

int core_unpack(AppState *app, const CoreCallbacks *callbacks) {
  Panel *source_panel = core_active_panel(app);
  Panel *target_panel = core_other_panel(app);
  FsEntry *entry;
  char archive_path[FS_PATH_MAX];
  char conflict[FS_NAME_MAX];
  char conflict_path[FS_PATH_MAX];
  char prompt[FS_PATH_MAX + 32];
  int conflict_result;

  if (source_panel->directory.count == 0 ||
      source_panel->selected >= source_panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &source_panel->directory.entries[source_panel->selected];
  if (entry->is_dir || !has_zip_extension(entry->name) ||
      !valid_name(entry->name) ||
      !fs_join_path(archive_path, sizeof(archive_path), source_panel->path,
                    entry->name)) {
    action_failed(callbacks);
    return 0;
  }

  /* Confirm each top-level replacement before extraction begins. */
  while ((conflict_result = archive_find_conflict(
              archive_path, target_panel->path, conflict, sizeof(conflict))) > 0) {
    snprintf(prompt, sizeof(prompt), "Overwrite %s? Y/N", conflict);
    if (request_confirmation(callbacks, prompt) != 1) return 1;

    if (!fs_join_path(conflict_path, sizeof(conflict_path),
                      target_panel->path, conflict) ||
        !fs_remove(conflict_path)) {
      action_failed(callbacks);
      return 0;
    }
  }

  if (conflict_result < 0) {
    action_failed(callbacks);
    return 0;
  }

  action_progress(callbacks, "Unpacking...");
  if (!archive_unpack(archive_path, target_panel->path)) {
    action_failed(callbacks);
    return 0;
  }

  return core_reload_panels(app, callbacks);
}

int core_rename(AppState *app, const CoreCallbacks *callbacks) {
  Panel *panel = core_active_panel(app);
  FsEntry *entry;
  char source[FS_PATH_MAX];
  char destination[FS_PATH_MAX];
  char name[FS_NAME_MAX];

  if (panel->directory.count == 0 ||
      panel->selected >= panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &panel->directory.entries[panel->selected];
  if (!valid_name(entry->name) ||
      !fs_join_path(source, sizeof(source), panel->path, entry->name)) {
    action_failed(callbacks);
    return 0;
  }

  strncpy(name, entry->name, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  if (!request_input(callbacks, "Rename:", name, sizeof(name))) return 1;

  if (!valid_name(name) ||
      !fs_join_path(destination, sizeof(destination), panel->path, name) ||
      (strcmp(source, destination) != 0 && fs_path_exists(destination))) {
    action_failed(callbacks);
    return 0;
  }

  action_progress(callbacks, "Renaming...");
  if (!fs_rename(source, destination)) {
    action_failed(callbacks);
    return 0;
  }

  if (!core_reload_panels(app, callbacks)) return 0;
  core_select_name(panel, name);
  return 1;
}

int core_new_directory(AppState *app, const CoreCallbacks *callbacks) {
  Panel *panel = core_active_panel(app);
  char name[FS_NAME_MAX] = "";
  char path[FS_PATH_MAX];

  if (!request_input(callbacks, "New directory:", name, sizeof(name))) return 1;

  if (!valid_name(name) ||
      !fs_join_path(path, sizeof(path), panel->path, name) ||
      fs_path_exists(path)) {
    action_failed(callbacks);
    return 0;
  }

  action_progress(callbacks, "Creating directory...");
  if (!fs_make_directory(path)) {
    action_failed(callbacks);
    return 0;
  }

  if (!core_reload_panels(app, callbacks)) return 0;
  core_select_name(panel, name);
  return 1;
}

int core_delete(AppState *app, const CoreCallbacks *callbacks) {
  Panel *panel = core_active_panel(app);
  FsEntry *entry;
  char path[FS_PATH_MAX];
  char prompt[FS_PATH_MAX + 32];

  if (panel->directory.count == 0 ||
      panel->selected >= panel->directory.count) {
    action_failed(callbacks);
    return 0;
  }

  entry = &panel->directory.entries[panel->selected];
  if (!valid_name(entry->name) ||
      !fs_join_path(path, sizeof(path), panel->path, entry->name)) {
    action_failed(callbacks);
    return 0;
  }

  snprintf(prompt, sizeof(prompt), "Delete %s? Y/N", entry->name);
  if (request_confirmation(callbacks, prompt) != 1) return 1;

  action_progress(callbacks, "Deleting...");
  if (!fs_remove(path)) {
    action_failed(callbacks);
    return 0;
  }

  return core_reload_panels(app, callbacks);
}
