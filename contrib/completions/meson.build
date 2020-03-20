if get_option('zsh-completions')
	install_data(
		files(
			'zsh/_makoctl',
			'zsh/_mako',
		),
		install_dir: datadir + '/zsh/site-functions',
		install_mode: 'rw-r--r--',
	)
endif

if get_option('fish-completions')
	fish_files = files('fish/mako.fish')
	if fish_comp.found()
		fish_install_dir = fish_comp.get_pkgconfig_variable('completionsdir')
	else
		fish_install_dir = datadir + '/fish/vendor_completions.d'
	endif
	install_data(fish_files, install_dir: fish_install_dir)

endif
