project(
  'showmethekey',
  'c',
  version: '0.1.0',
  license: 'Apache-2.0',
  default_options: ['c_std=c11']
)

gnome = import('gnome')

cc = meson.get_compiler('c')

c_args = []
if get_option('debug') == true
  warning_level = 3
  c_args += ['-D__DEBUG__']
endif

conf_data = configuration_data()
conf_data.set('install_prefix', get_option('prefix'))
conf_data.set('project_version', meson.project_version())

configure_file(
  input: 'common/config-meson.h.in',
  output: 'config.h',
  configuration: conf_data
)

cli_sources = files(
  'showmethekey-cli/main.c'
)

cli_dependencies = []
cli_include_directories = []
libevdev = dependency('libevdev', required: true)
libudev = dependency('libudev', required: true)
libinput = dependency('libinput', required: true)
threads = dependency('threads', required: true)
cli_dependencies += [libevdev, libudev, libinput, threads]

executable(
  'showmethekey-cli',
  sources: cli_sources,
  c_args: c_args,
  dependencies: cli_dependencies,
  include_directories: cli_include_directories,
  install: true,
  install_dir: get_option('prefix') / 'bin'
)

gtk_sources = files(
  'showmethekey-gtk/main.c',
  'showmethekey-gtk/smtk-app.c',
  'showmethekey-gtk/smtk-app-win.c',
  'showmethekey-gtk/smtk-keys-win.c',
  'showmethekey-gtk/smtk-keys-emitter.c',
  'showmethekey-gtk/smtk-keys-mapper.c',
  'showmethekey-gtk/smtk-event.c'
)

gtk_headers = files(
  'showmethekey-gtk/smtk.h',
  'showmethekey-gtk/smtk-app.h',
  'showmethekey-gtk/smtk-app-win.h',
  'showmethekey-gtk/smtk-keys-emitter.h',
  'showmethekey-gtk/smtk-keys-win.h',
  'showmethekey-gtk/smtk-keys-mapper.h',
  'showmethekey-gtk/smtk-event.h'
)

gtk_dependencies = []
# Generated enum types sources need this.
gtk_include_directories = ['showmethekey-gtk/']
gtk = dependency('gtk+-3.0', required: true)
glib = dependency('glib-2.0', required: true)
json_glib = dependency('json-glib-1.0', required: true)
gio = dependency('gio-2.0', required: true)
cairo = dependency('cairo', required: true)
pango = dependency('pango', required: true)
xkbcommon = dependency('xkbcommon', required: true)
# x11 = dependency('x11', required: true)
# xext = dependency('xext', required: true)
gtk_dependencies += [gtk, glib, json_glib, gio, cairo, pango, xkbcommon]

resources = gnome.compile_resources(
  'smtk-resources',
  'showmethekey-gtk/one.alynx.showmethekey.gresource.xml',
  source_dir: 'showmethekey-gtk/',
  c_name: 'smtk'
)

enum_types = gnome.mkenums_simple('smtk-enum-types', sources: gtk_headers)

schema_dir = get_option('prefix') / get_option('datadir') / 'glib-2.0' / 'schemas'

executable(
  'showmethekey-gtk',
  sources: gtk_sources + resources + enum_types,
  c_args: c_args,
  dependencies: gtk_dependencies,
  include_directories: gtk_include_directories,
  install: true,
  install_dir: get_option('prefix') / 'bin'
)

install_data(
  'LICENSE',
  install_dir: get_option('prefix') / get_option('datadir') / 'licenses' / meson.project_name()
)
# install_data(
#   'dists' / 'showmethekey.png',
#   install_dir: get_option('datadir') / 'pixmaps'
# )
install_data(
  'dists' / 'showmethekey.desktop',
  install_dir: get_option('prefix') / get_option('datadir') / 'applications'
)
install_data(
  'dists' / 'one.alynx.showmethekey.gschema.xml',
  install_dir: get_option('prefix') / get_option('datadir') / 'glib-2.0' / 'schemas'
)

# We should not compile schemas by ourselves when packaging for distributions.
# A simple way to detect packaging is the `DISTDIR` env, but before Meson 0.57.0
# does not support this and it also does not allow us to get env,
# so typically we use a Python script here as fallback.
if meson.version().version_compare('>= 0.57.0')
  meson.add_install_script(
    'glib-compile-schemas',
    get_option('prefix') / get_option('datadir') / 'glib-2.0' / 'schemas',
    skip_if_destdir: true
  )
else
  meson.add_install_script(
    'meson_post_install.py',
    get_option('prefix') / get_option('datadir') / 'glib-2.0' / 'schemas'
  )
endif
