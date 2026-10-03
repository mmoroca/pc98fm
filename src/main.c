/***********************************/
/* pc98fm, an FM-like file manager */
/* by @mmoroca & arena.ai 2026     */
/***********************************/

#include "core.h"
#include "fs.h"
#include "ui.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  AppState app;

  setlocale(LC_ALL, "");
  CoreCallbacks callbacks;
  char current_path[FS_PATH_MAX];

  if (!fs_get_current_path(current_path, sizeof(current_path))) {
    fprintf(stderr, "Cannot get current directory\n");
    return EXIT_FAILURE;
  }

  if (!core_init(&app, current_path)) {
    fprintf(stderr, "Cannot open current directory\n");
    return EXIT_FAILURE;
  }

  callbacks.context = &app;
  callbacks.input = ui_input_callback;
  callbacks.confirm = ui_confirm_callback;
  callbacks.drive = ui_drive_callback;
  callbacks.progress = ui_progress_callback;
  callbacks.execute = ui_execute_callback;
  callbacks.beep = ui_beep_callback;

  ui_init();

  while (1) {
    int key;
    int rows = ui_visible_rows();

    if (rows < 1) rows = 1;
    ui_draw(&app);
    key = ui_get_key();

    if (key == UI_KEY_EASTER_EGG) {
      ui_show_easter_egg();
      ui_force_redraw();
      continue;
    }

    if (app.editor.active) {
      int columns = ui_editor_columns();
      if (columns < 1) columns = 1;

      switch (key) {
        case 27:
          if (core_close_editor(&app, &callbacks))
            ui_force_redraw();
          break;
        case UI_KEY_LEFT:
          core_editor_move_left(&app.editor, columns, rows);
          break;
        case UI_KEY_RIGHT:
          core_editor_move_right(&app.editor, columns, rows);
          break;
        case UI_KEY_UP:
          core_editor_move_up(&app.editor, columns, rows);
          break;
        case UI_KEY_DOWN:
          core_editor_move_down(&app.editor, columns, rows);
          break;
        case UI_KEY_PAGE_UP:
          for (int i = 0; i < rows; i++)
            core_editor_move_up(&app.editor, columns, rows);
          break;
        case UI_KEY_PAGE_DOWN:
          for (int i = 0; i < rows; i++)
            core_editor_move_down(&app.editor, columns, rows);
          break;
        case UI_KEY_HOME:
          core_editor_home(&app.editor, columns, rows);
          break;
        case UI_KEY_END:
          core_editor_end(&app.editor, columns, rows);
          break;
        case UI_KEY_BACKSPACE:
          core_editor_backspace(&app.editor, columns, rows);
          break;
        case UI_KEY_DELETE:
          core_editor_delete(&app.editor, columns, rows);
          break;
        case UI_KEY_ENTER:
          core_editor_newline(&app.editor, columns, rows);
          break;
        case UI_KEY_RESIZE:
          core_editor_ensure_visible(&app.editor, columns, rows);
          break;
        default:
          if (key >= 32 && key <= 126)
            core_editor_insert(&app.editor, key, columns, rows);
          break;
      }
      continue;
    }

    if (app.viewer.active) {
      switch (key) {
        case 27:
          core_close_viewer(&app);
          break;
        case UI_KEY_UP:
          core_move_viewer(&app.viewer, -1, rows);
          break;
        case UI_KEY_DOWN:
          core_move_viewer(&app.viewer, 1, rows);
          break;
        case UI_KEY_PAGE_UP:
          core_move_viewer(&app.viewer, -rows, rows);
          break;
        case UI_KEY_PAGE_DOWN:
          core_move_viewer(&app.viewer, rows, rows);
          break;
        case UI_KEY_HOME:
          app.viewer.top_line = 0;
          break;
        case UI_KEY_END:
          if (app.viewer.count > 0)
            app.viewer.top_line = app.viewer.count - 1;
          core_ensure_viewer_visible(&app.viewer, rows);
          break;
        case UI_KEY_RESIZE:
          core_ensure_viewer_visible(&app.viewer, rows);
          break;
      }
      continue;
    }

    Panel *panel = core_active_panel(&app);

    switch (key) {
      case 27:
      case 'q':
      case 'Q':
        if (ui_confirm_callback(&app, "Exit? Y/N") == 1) {
          ui_shutdown();
          return EXIT_SUCCESS;
        }
        break;

      case '\t':
        core_toggle_panel(&app);
        break;

      case ':':
        core_select_drive(&app, &callbacks);
        break;

      case UI_KEY_UP:
        core_move_selection(panel, -1, rows);
        break;

      case UI_KEY_DOWN:
        core_move_selection(panel, 1, rows);
        break;

      case UI_KEY_PAGE_UP:
        core_move_selection(panel, -rows, rows);
        break;

      case UI_KEY_PAGE_DOWN:
        core_move_selection(panel, rows, rows);
        break;

      case UI_KEY_HOME:
        panel->selected = 0;
        core_ensure_selection_visible(panel, rows);
        break;

      case UI_KEY_END:
        if (panel->directory.count > 0)
          panel->selected = panel->directory.count - 1;
        core_ensure_selection_visible(panel, rows);
        break;

      case 18: /* Ctrl-R */
        ui_progress_callback(&app, "Loading...");
        if (!core_reload_panel(panel, 1)) ui_beep_callback(&app);
        break;

      case 'v':
      case 'V':
        core_view_file(&app, &callbacks);
        break;

      case 'x':
      case 'X':
        core_execute(&app, &callbacks);
        break;

      case 'e':
      case 'E':
        core_edit_file(&app, &callbacks);
        break;

      case 's':
      case 'S':
        core_sort(&app, &callbacks);
        break;

      case 'p':
      case 'P':
        core_pack(&app, &callbacks);
        break;

      case 'u':
      case 'U':
        core_unpack(&app, &callbacks);
        break;

      case UI_KEY_F5:
      case 'c':
      case 'C':
        core_copy(&app, &callbacks);
        break;

      case UI_KEY_F6:
      case 'r':
      case 'R':
        core_rename(&app, &callbacks);
        break;

      case UI_KEY_F7:
      case 'n':
      case 'N':
        core_new_directory(&app, &callbacks);
        break;

      case UI_KEY_F8:
      case 'd':
      case 'D':
        core_delete(&app, &callbacks);
        break;

      case UI_KEY_ENTER:
        core_enter_directory(&app, &callbacks);
        break;

      case UI_KEY_RESIZE:
        core_ensure_selection_visible(&app.left, rows);
        core_ensure_selection_visible(&app.right, rows);
        break;
    }
  }
}
