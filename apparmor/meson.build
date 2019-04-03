if get_option('apparmor')
	apparmordir = join_paths(get_option('sysconfdir'), 'apparmor.d')
	install_data(
		'fr.emersion.Mako',
		install_dir: apparmordir,
		install_mode: 'rw-r-----',
	)
endif
