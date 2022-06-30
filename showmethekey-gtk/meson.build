gnome = import('gnome')

gtk_sources = files(
  'main.c',
  'smtk-app.c',
  'smtk-app-win.c',
  'smtk-keys-win.c',
  'smtk-keys-emitter.c',
  'smtk-keys-mapper.c',
  'smtk-event.c'
)

gtk_headers = files(
  'smtk.h',
  'smtk-app.h',
  'smtk-app-win.h',
  'smtk-keys-emitter.h',
  'smtk-keys-win.h',
  'smtk-keys-mapper.h',
  'smtk-event.h'
)

gtk_dependencies = []
gtk_include_directories = []
gtk = dependency('gtk4', required: true)
glib = dependency('glib-2.0', required: true)
json_glib = dependency('json-glib-1.0', required: true)
gio = dependency('gio-2.0', required: true)
cairo = dependency('cairo', required: true)
pango = dependency('pango', required: true)
xkbcommon = dependency('xkbcommon', required: true)
# x11 = dependency('x11', required: true)
# xext = dependency('xext', required: true)
gtk_dependencies += [gtk, glib, json_glib, gio, cairo, pango, xkbcommon]
# For config.h.
gtk_include_directories += include_directories('../')

resources = gnome.compile_resources(
  'smtk-resources',
  'one.alynx.showmethekey.gresource.xml',
  source_dir: '.',
  c_name: 'smtk'
)

enum_types = gnome.mkenums_simple('smtk-enum-types', sources: gtk_headers)

executable(
  'showmethekey-gtk',
  sources: gtk_sources + resources + enum_types,
  c_args: c_args,
  dependencies: gtk_dependencies,
  include_directories: gtk_include_directories,
  install: true
  # By default, meson install binary to
  # `get_option('prefix') / get_option('bindir')`,
  # so we can omit `install_dir` here.
)

subdir('po')
