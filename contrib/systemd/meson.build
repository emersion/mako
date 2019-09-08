systemd = dependency('systemd', required: get_option('systemd'))

if systemd.found()
  user_units_dir = systemd.get_pkgconfig_variable('systemduserunitdir')
  install_data('mako.service',
    install_dir: user_units_dir)
endif
