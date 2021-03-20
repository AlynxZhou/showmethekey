#ifndef __SMTK_KEYS_WIN_H__
#define __SMTK_KEYS_WIN_H__

#include <gtk/gtk.h>

#define SMTK_TYPE_KEYS_WIN smtk_keys_win_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeysWin, smtk_keys_win, SMTK, KEYS_WIN, GtkWindow)

GtkWidget *smtk_keys_win_new(void);

GtkWidget *smtk_canvas_get_keys_label(SmtkKeysWin *win);

#endif
