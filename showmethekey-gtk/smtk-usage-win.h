#ifndef __SMTK_USAGE_WIN_H__
#define __SMTK_USAGE_WIN_H__

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define SMTK_TYPE_USAGE_WIN smtk_usage_win_get_type()
G_DECLARE_FINAL_TYPE(SmtkUsageWin, smtk_usage_win, SMTK, USAGE_WIN, AdwWindow)

GtkWidget *smtk_usage_win_new(void);

G_END_DECLS

#endif /* __SMTK_USAGE_WIN_H__ */
