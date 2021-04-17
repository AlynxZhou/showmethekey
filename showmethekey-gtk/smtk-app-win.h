#ifndef __SMTK_APP_WIN_H__
#define __SMTK_APP_WIN_H__

#include <gtk/gtk.h>

#include "smtk-app.h"

G_BEGIN_DECLS

#define SMTK_TYPE_APP_WIN smtk_app_win_get_type()
G_DECLARE_FINAL_TYPE(SmtkAppWin, smtk_app_win, SMTK, APP_WIN,
		     GtkApplicationWindow)

GtkWidget *smtk_app_win_new(SmtkApp *app);
void smtk_app_win_toggle_hide_switch(SmtkAppWin *win);
void smtk_app_win_show_usage_dialog(SmtkAppWin *win);
void smtk_app_win_show_about_dialog(SmtkAppWin *win);

G_END_DECLS

#endif
