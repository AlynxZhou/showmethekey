#ifndef __SMTK_KEYS_WIN_H__
#define __SMTK_KEYS_WIN_H__

#include <gtk/gtk.h>

#include "smtk-keys-emitter.h"

G_BEGIN_DECLS

#define SMTK_TYPE_KEYS_WIN smtk_keys_win_get_type()
G_DECLARE_FINAL_TYPE(SmtkKeysWin, smtk_keys_win, SMTK, KEYS_WIN, GtkWindow)

GtkWidget *smtk_keys_win_new(gboolean show_mouse, SmtkKeyMode mode,
			     guint64 width, guint64 height, gint timeout, GError **error);
void smtk_keys_win_hide(SmtkKeysWin *win);
void smtk_keys_win_show(SmtkKeysWin *win);

G_END_DECLS

#endif
