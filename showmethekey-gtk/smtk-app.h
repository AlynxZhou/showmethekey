#ifndef __SMTK_APP_H__
#define __SMTK_APP_H__

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define SMTK_TYPE_APP smtk_app_get_type()
G_DECLARE_FINAL_TYPE(SmtkApp, smtk_app, SMTK, APP, AdwApplication)

SmtkApp *smtk_app_new(void);
void smtk_app_quit(SmtkApp *this);

G_END_DECLS

#endif
