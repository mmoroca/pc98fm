#ifndef PC98FM_CORE_H
#define PC98FM_CORE_H

#include "fs.h"

#include <stddef.h>

typedef enum {
  SORT_NAME,
  SORT_EXTENSION,
  SORT_SIZE,
  SORT_DATE
} SortMode;

typedef struct {
  char path[FS_PATH_MAX];
  FsDirectory directory;
  size_t selected;
  size_t scroll;
  SortMode sort_mode;
  int sort_descending;
} Panel;

typedef struct {
  int active;
  char path[FS_PATH_MAX];
  char **lines;
  size_t count;
  size_t top_line;
} TextViewer;

typedef struct {
  int active;
  char path[FS_PATH_MAX];
  char **lines;
  size_t count;
  size_t cursor_line;
  size_t cursor_column;
  size_t preferred_column;
  size_t top_row;
  size_t horizontal_scroll;
  int preferred_valid;
  int modified;
  int wrap;
} TextEditor;

typedef struct {
  Panel left;
  Panel right;
  int active;
  TextViewer viewer;
  TextEditor editor;
} AppState;

typedef struct {
  void *context;
  int (*input)(void *context, const char *prompt,
               char *buffer, size_t size);
  int (*confirm)(void *context, const char *prompt);
  int (*drive)(void *context, const FsDrive *drives,
               size_t count, size_t *selected);
  void (*progress)(void *context, const char *message);
  int (*execute)(void *context, const char *path,
                 const char *working_directory);
  void (*beep)(void *context);
} CoreCallbacks;

int core_init(AppState *app, const char *path);
Panel *core_active_panel(AppState *app);
Panel *core_other_panel(AppState *app);
void core_toggle_panel(AppState *app);
void core_ensure_selection_visible(Panel *panel, int visible_rows);
void core_move_selection(Panel *panel, int amount, int visible_rows);
int core_reload_panel(Panel *panel, int preserve_selection);
int core_reload_panels(AppState *app, const CoreCallbacks *callbacks);
int core_select_drive(AppState *app, const CoreCallbacks *callbacks);
int core_sort(AppState *app, const CoreCallbacks *callbacks);
int core_execute(AppState *app, const CoreCallbacks *callbacks);
int core_view_file(AppState *app, const CoreCallbacks *callbacks);
void core_close_viewer(AppState *app);
void core_move_viewer(TextViewer *viewer, int amount, int visible_rows);
void core_ensure_viewer_visible(TextViewer *viewer, int visible_rows);
int core_edit_file(AppState *app, const CoreCallbacks *callbacks);
int core_save_editor(AppState *app, const CoreCallbacks *callbacks);
int core_close_editor(AppState *app, const CoreCallbacks *callbacks);
void core_toggle_editor_wrap(AppState *app, int columns, int visible_rows);
void core_editor_move_left(TextEditor *editor, int columns, int visible_rows);
void core_editor_move_right(TextEditor *editor, int columns, int visible_rows);
void core_editor_move_up(TextEditor *editor, int columns, int visible_rows);
void core_editor_move_down(TextEditor *editor, int columns, int visible_rows);
void core_editor_home(TextEditor *editor, int columns, int visible_rows);
void core_editor_end(TextEditor *editor, int columns, int visible_rows);
void core_editor_backspace(TextEditor *editor, int columns, int visible_rows);
void core_editor_delete(TextEditor *editor, int columns, int visible_rows);
void core_editor_newline(TextEditor *editor, int columns, int visible_rows);
void core_editor_insert(TextEditor *editor, int character,
                        int columns, int visible_rows);
size_t core_editor_cursor_row(const TextEditor *editor, size_t columns);
int core_editor_row_at(const TextEditor *editor, size_t row, size_t columns,
                       size_t *line, size_t *offset);
void core_editor_ensure_visible(TextEditor *editor, int columns,
                                int visible_rows);
int core_enter_directory(AppState *app, const CoreCallbacks *callbacks);
int core_copy(AppState *app, const CoreCallbacks *callbacks);
int core_pack(AppState *app, const CoreCallbacks *callbacks);
int core_unpack(AppState *app, const CoreCallbacks *callbacks);
int core_rename(AppState *app, const CoreCallbacks *callbacks);
int core_new_directory(AppState *app, const CoreCallbacks *callbacks);
int core_delete(AppState *app, const CoreCallbacks *callbacks);
int core_select_name(Panel *panel, const char *name);

#endif
