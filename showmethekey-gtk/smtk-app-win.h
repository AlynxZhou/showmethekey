#ifndef __SMTK_APP_WIN_H__
#define __SMTK_APP_WIN_H__

#include <gtk/gtk.h>
#include <adwaita.h>

#include "smtk-app.h"

G_BEGIN_DECLS

#define SMTK_TYPE_APP_WIN smtk_app_win_get_type()
G_DECLARE_FINAL_TYPE(SmtkAppWin, smtk_app_win, SMTK, APP_WIN,
		     AdwApplicationWindow)

GtkWidget *smtk_app_win_new(SmtkApp *app);
void smtk_app_win_activate(SmtkAppWin *win);
void smtk_app_win_toggle_clickable_switch(SmtkAppWin *win);
void smtk_app_win_toggle_pause_switch(SmtkAppWin *win);
void smtk_app_win_toggle_shift_switch(SmtkAppWin *win);
void smtk_app_win_toggle_mouse_switch(SmtkAppWin *win);
void smtk_app_win_toggle_border_switch(SmtkAppWin *win);
void smtk_app_win_toggle_hide_visible_switch(SmtkAppWin *win);
void smtk_app_win_set_size(SmtkAppWin *win, int width, int height);
void smtk_app_win_show_usage(SmtkAppWin *win);
void smtk_app_win_show_about(SmtkAppWin *win);

G_END_DECLS

#endif
