project(
  'showmethekey',
  'c',
  version: '1.19.0',
  license: 'Apache-2.0',
  default_options: ['c_std=gnu11']
)

if get_option('debug') == true
  warning_level = 3
  add_project_arguments('-D__DEBUG__', language: ['c'])
endif
add_project_arguments('-DG_LOG_DOMAIN="showmethekey"', language: ['c'])

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
libdir = prefix / get_option('libdir')
datadir = prefix / get_option('datadir')
localedir = prefix / get_option('localedir')
pkexec_path = get_option('pkexec_path')

conf_data = configuration_data()
conf_data.set('project_version', meson.project_version())
conf_data.set('gettext_package', meson.project_name())
conf_data.set('package_bindir', bindir)
conf_data.set('package_localedir', localedir)
conf_data.set('pkexec_path', pkexec_path)

configure_file(
  input: 'common' / 'config.h.in',
  output: 'config.h',
  configuration: conf_data
)

subdir('dists')
subdir('showmethekey-cli')
subdir('showmethekey-gtk')

install_data(
  'LICENSE',
  install_dir: get_option('datadir') / 'licenses' / meson.project_name()
)

summary({
  'buildtype': get_option('buildtype'),
  'prefix': prefix,
  'bindir': bindir,
  'libdir': libdir,
  'datadir': datadir,
  'localedir': localedir,
  'pkexec_path': pkexec_path,
}, section: 'Confiuration')

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
    'gtk4-update-icon-cache',
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
