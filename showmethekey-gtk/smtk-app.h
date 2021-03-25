#ifndef __SMTK_APP_H__
#define __SMTK_APP_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SMTK_TYPE_APP smtk_app_get_type()
G_DECLARE_FINAL_TYPE(SmtkApp, smtk_app, SMTK, APP, GtkApplication)

SmtkApp *smtk_app_new(void);

G_END_DECLS

#endif
