#ifndef PC98FM_UI_H
#define PC98FM_UI_H

#include "core.h"

#define UI_KEY_UP 1001
#define UI_KEY_DOWN 1002
#define UI_KEY_PAGE_UP 1003
#define UI_KEY_PAGE_DOWN 1004
#define UI_KEY_HOME 1005
#define UI_KEY_END 1006
#define UI_KEY_ENTER 1007
#define UI_KEY_RESIZE 1008
#define UI_KEY_F5 1009
#define UI_KEY_F6 1010
#define UI_KEY_F7 1011
#define UI_KEY_F8 1012
#define UI_KEY_LEFT 1013
#define UI_KEY_RIGHT 1014
#define UI_KEY_BACKSPACE 1015
#define UI_KEY_DELETE 1016
#define UI_KEY_EASTER_EGG 1017

void ui_init(void);
void ui_shutdown(void);
void ui_draw(AppState *app);
void ui_force_redraw(void);
void ui_show_easter_egg(void);
int ui_get_key(void);
int ui_visible_rows(void);
int ui_editor_columns(void);
int ui_input_callback(void *context, const char *prompt,
                      char *buffer, size_t size);
int ui_confirm_callback(void *context, const char *prompt);
void ui_progress_callback(void *context, const char *message);
int ui_execute_callback(void *context, const char *path,
                        const char *working_directory);
int ui_drive_callback(void *context, const FsDrive *drives,
                      size_t count, size_t *selected);
void ui_beep_callback(void *context);

#endif
