#ifndef __SMTK_H__
#define __SMTK_H__

#ifdef G_LOG_DOMAIN
#	undef G_LOG_DOMAIN
#	define G_LOG_DOMAIN "showmethekey-gtk"
#endif

#include "config.h"

// Type generated in meson.build.
// See <https://developer.gnome.org/gobject/stable/glib-mkenums.html>
// and <https://mesonbuild.com/Gnome-module.html#gnomemkenums_simple>.
#include "smtk-enum-types.h"
#include "smtk-app.h"
#include "smtk-app-win.h"
#include "smtk-keys-win.h"
#include "smtk-keys-emitter.h"
#include "smtk-keys-mapper.h"
#include "smtk-event.h"

#endif
