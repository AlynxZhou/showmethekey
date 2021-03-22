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
cli_dependencies += [libevdev, libudev, libinput]

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
  'showmethekey-gtk/smtk-keys-win.c'
)

gtk_dependencies = []
gtk_include_directories = []
gtk = dependency('gtk+-3.0', required: true)
# x11 = dependency('x11', required: true)
# xext = dependency('xext', required: true)
gtk_dependencies += [gtk]

gresource = gnome.compile_resources(
  'showmethekey-gresource',
  'showmethekey-gtk/showmethekey.gresource.xml',
  source_dir: 'showmethekey-gtk/',
  c_name: 'smtk'
)

executable(
  'showmethekey-gtk',
  sources: gtk_sources + gresource,
  c_args: c_args,
  dependencies: gtk_dependencies,
  include_directories: gtk_include_directories,
  install: true,
  install_dir: get_option('prefix') / 'bin'
)

install_data(
  'LICENSE',
  install_dir: get_option('datadir') / 'license' / meson.project_name()
)
# install_data(
#   'dists' / 'showmethekey.png',
#   install_dir: get_option('datadir') / 'pixmaps'
# )
install_data(
  'dists' / 'showmethekey.desktop',
  install_dir: get_option('datadir') / 'applications'
)
