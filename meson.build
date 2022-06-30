project(
  'showmethekey',
  'c',
  version: '1.6.3',
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
conf_data.set('package_bindir', get_option('prefix') / get_option('bindir'))
conf_data.set('package_localedir', get_option('prefix') / get_option('localedir'))
conf_data.set('pkexec_path', get_option('pkexec_path'))

configure_file(
  input: 'common' / 'config.h.in',
  output: 'config.h',
  configuration: conf_data
)

subdir('showmethekey-cli')

subdir('showmethekey-gtk')

install_data(
  'LICENSE',
  install_dir: get_option('datadir') / 'licenses' / meson.project_name()
)
install_data(
  'dists' / 'icons' / '128x128' / 'one.alynx.showmethekey.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '128x128' / 'apps'
)
install_data(
  'dists' / 'icons' / '64x64' / 'one.alynx.showmethekey.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '64x64' / 'apps'
)
install_data(
  'dists' / 'icons' / 'scalable' / 'one.alynx.showmethekey.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps'
)
install_data(
  'dists' / 'one.alynx.showmethekey.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas'
)
install_data(
  'dists' / 'one.alynx.showmethekey.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo'
)
install_data(
  'dists' / 'one.alynx.showmethekey.rules',
  install_dir: get_option('datadir') / 'polkit-1' / 'rules.d'
)
configure_file(
  input: 'dists' / 'one.alynx.showmethekey.policy.in',
  output: 'one.alynx.showmethekey.policy',
  configuration: conf_data,
  install: true,
  install_dir: get_option('datadir') / 'polkit-1' / 'actions'
)
configure_file(
  input: 'dists/one.alynx.showmethekey.desktop.in',
  output: 'one.alynx.showmethekey.desktop',
  configuration: conf_data,
  install: true,
  install_dir: get_option('datadir') / 'applications'
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
  meson.add_install_script(
    'gtk-update-icon-cache',
    '-f',
    '-t',
    get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor',
    skip_if_destdir: true
  )
else
  meson.add_install_script(
    'meson_post_install.py',
    get_option('prefix') / get_option('datadir')
  )
endif
