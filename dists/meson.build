install_data(
  'icons' / '128x128' / 'one.alynx.showmethekey.png',
  install_dir: datadir / 'icons' / 'hicolor' / '128x128' / 'apps'
)
install_data(
  'icons' / '64x64' / 'one.alynx.showmethekey.png',
  install_dir: datadir / 'icons' / 'hicolor' / '64x64' / 'apps'
)
install_data(
  'icons' / 'scalable' / 'one.alynx.showmethekey.svg',
  install_dir: datadir / 'icons' / 'hicolor' / 'scalable' / 'apps'
)
install_data(
  'one.alynx.showmethekey.gschema.xml',
  install_dir: datadir / 'glib-2.0' / 'schemas'
)
install_data(
  'one.alynx.showmethekey.metainfo.xml',
  install_dir: datadir / 'metainfo'
)
install_data(
  'one.alynx.showmethekey.rules',
  install_dir: datadir / 'polkit-1' / 'rules.d'
)
configure_file(
  input: 'one.alynx.showmethekey.policy.in',
  output: 'one.alynx.showmethekey.policy',
  configuration: conf_data,
  install: true,
  install_dir: datadir / 'polkit-1' / 'actions'
)
configure_file(
  input: 'one.alynx.showmethekey.desktop.in',
  output: 'one.alynx.showmethekey.desktop',
  configuration: conf_data,
  install: true,
  install_dir: datadir / 'applications'
)
