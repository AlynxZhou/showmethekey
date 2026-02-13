gnome = import('gnome')

gtk_sources = files(
  'main.c',
  'smtk-app.c',
  'smtk-app-win.c',
  'smtk-usage-win.c',
  'smtk-keys-win.c',
  'smtk-keys-area.c',
  'smtk-keys-emitter.c',
  'smtk-keys-mapper.c',
  'smtk-keymap-list.c',
  'smtk-event.c'
)

gtk_headers = files(
  'smtk-app.h',
  'smtk-app-win.h',
  'smtk-usage-win.h',
  'smtk-keys-win.h',
  'smtk-keys-area.h',
  'smtk-keys-emitter.h',
  'smtk-keys-mapper.h',
  'smtk-keymap-list.h',
  'smtk-event.h'
)

gtk_dependencies = []
gtk_include_directories = []
gtk = dependency('gtk4', required: true)
libadw = dependency('libadwaita-1', required: true)
x11 = dependency('x11')
glib = dependency('glib-2.0', required: true)
json_glib = dependency('json-glib-1.0', required: true)
gio = dependency('gio-2.0', required: true)
cairo = dependency('cairo', required: true)
pango = dependency('pango', required: true)
xkbcommon = dependency('xkbcommon', required: true)
xkbregistry = dependency('xkbregistry', required: true)
gtk_dependencies += [
  gtk, libadw, x11, glib, json_glib, gio, cairo, pango, xkbcommon, xkbregistry
]
# For config.h.
gtk_include_directories += include_directories('../')

resources = gnome.compile_resources(
  'smtk-resources',
  'one.alynx.showmethekey.gresource.xml',
  source_dir: '.',
  c_name: 'smtk'
)

gnome.mkenums('one.alynx.showmethekey.enums.gschema.xml',
  comments: '<!-- @comment@ -->',
  fhead: '<schemalist>',
  vhead: '  <@type@ id="one.alynx.showmethekey.@EnumName@">',
  vprod: '    <value nick="@valuenick@" value="@valuenum@" />',
  vtail: '  </@type@>',
  ftail: '</schemalist>',
  sources: gtk_headers,
  install_header: true,
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas'
)
enum_types = gnome.mkenums_simple('smtk-enum-types', sources: gtk_headers)

executable(
  'showmethekey-gtk',
  sources: gtk_sources + resources + enum_types,
  dependencies: gtk_dependencies,
  include_directories: gtk_include_directories,
  install: true
  # By default, meson install binary to
  # `get_option('prefix') / get_option('bindir')`,
  # so we can omit `install_dir` here.
)

subdir('po')
