dbus_conf = configuration_data()
dbus_conf.set('app_id', app_id)
dbus_conf.set('bindir', join_paths(prefix, bindir))
dbus_conf.set('bin', meson.project_name())

configure_file(
  input: 'io.github.moon.Nyx.service.in',
  output: 'io.github.moon.Nyx.service',
  configuration: dbus_conf,
  install: true,
  install_dir: join_paths(prefix, datadir, 'dbus-1', 'services'),
)
