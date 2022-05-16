project(
  'showmethekey',
  'c',
  version: '1.5.0',
  license: 'Apache-2.0',
  default_options: ['c_std=c11']
)

cc = meson.get_compiler('c')

c_args = []
if get_option('debug') == true
  warning_level = 3
  c_args += ['-D__DEBUG__']
endif

conf_data = configuration_data()
conf_data.set('project_version', meson.project_version())
conf_data.set('gettext_package', meson.project_name())
conf_data.set('package_bindir', join_paths(get_option('prefix'), get_option('bindir')))
conf_data.set('package_localedir', join_paths(get_option('prefix'), get_option('localedir')))
conf_data.set('pkexec_path', get_option('pkexec_path'))

configure_file(
  input: 'common' / 'config-meson.h.in',
  output: 'config.h',
  configuration: conf_data
)

configure_file(
  input: 'dists/showmethekey.desktop.in',
  output: 'showmethekey.desktop',
  configuration: conf_data,
  install_dir: get_option('datadir') / 'applications'
)

subdir('showmethekey-cli')

subdir('showmethekey-gtk')

install_data(
  'LICENSE',
  install_dir: get_option('prefix') / get_option('datadir') / 'licenses' / meson.project_name()
)
install_data(
  'dists' / 'showmethekey.png',
  install_dir: get_option('prefix') / get_option('datadir') / 'pixmaps'
)
install_data(
  'dists' / 'showmethekey.svg',
  install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps'
)
install_data(
  'dists' / 'one.alynx.showmethekey.gschema.xml',
  install_dir: get_option('prefix') / get_option('datadir') / 'glib-2.0' / 'schemas'
)
install_data(
  'dists' / 'one.alynx.showmethekey.rules',
  install_dir: get_option('prefix') / get_option('datadir') / 'polkit-1' / 'rules.d'
)
configure_file(
  input: 'dists' / 'one.alynx.showmethekey.policy.in',
  output: 'one.alynx.showmethekey.policy',
  configuration: conf_data,
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'polkit-1' / 'actions'
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
