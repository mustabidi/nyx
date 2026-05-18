rc_conf = configuration_data()
rc_conf.set('VER_NYXVERSION', nyx_version.replace('.', ',') + ',0')
rc_conf.set_quoted('PACKAGE', meson.project_name())
rc_conf.set_quoted('PACKAGE_VERSION', meson.project_version())
rc_conf.set_quoted('PACKAGE_ORIGIN', 'https://github.com/Rafostar/nyx')
rc_conf.set_quoted('NYX_APP_NAME', 'Nyx')
rc_conf.set_quoted('NYX_EXE', meson.project_name() + '.exe')

nyxapp_rc = configure_file(
  input: 'nyx.rc.in',
  output: 'nyx.rc',
  configuration: rc_conf,
)

windres = find_program('windres', required: true)
nyxapp_sources += custom_target('nyx.o',
  input: nyxapp_rc,
  output: 'nyx.o',
  command: [windres, '-I', '@CURRENT_SOURCE_DIR@', '-o', '@OUTPUT@', '@INPUT@']
)
