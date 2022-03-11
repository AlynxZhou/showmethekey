cli_sources = files(
  'main.c'
)

cli_dependencies = []
cli_include_directories = []
libevdev = dependency('libevdev', required: true)
libudev = dependency('libudev', required: true)
libinput = dependency('libinput', required: true)
threads = dependency('threads', required: true)
cli_dependencies += [libevdev, libudev, libinput, threads]
# For config.h.
cli_include_directories += include_directories('../')

executable(
  'showmethekey-cli',
  sources: cli_sources,
  c_args: c_args,
  dependencies: cli_dependencies,
  include_directories: cli_include_directories,
  install: true,
  install_dir: get_option('prefix') / get_option('bindir')
)
