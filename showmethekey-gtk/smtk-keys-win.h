#ifndef __SMTK_KEYS_WIN_H__
#define __SMTK_KEYS_WIN_H__

#include <gtk/gtk.h>
#include <adwaita.h>

#include "smtk-app.h"

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_WIN smtk_keys_win_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeysWin, smtk_keys_win, SMTK, KEYS_WIN,
		     AdwApplicationWindow)

GtkWidget *smtk_keys_win_new(SmtkApp *app, bool clickable, GError **error);

G_END_DECLS

#endif
