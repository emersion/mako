systemd = dependency('systemd', required: get_option('systemd'))

if systemd.found()
  user_units_dir = systemd.get_pkgconfig_variable('systemduserunitdir')

  configure_file(
    configuration: conf_data,
    input: 'mako.service.in',
    output: '@BASENAME@',
    install_dir: user_units_dir
  )
endif
