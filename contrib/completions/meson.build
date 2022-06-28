if get_option('zsh-completions')
	install_data(
		files(
			'zsh/_makoctl',
			'zsh/_mako',
		),
		install_dir: get_option('datadir') / 'zsh/site-functions',
		install_mode: 'rw-r--r--',
	)
endif

if get_option('fish-completions')
	fish_files = files('fish/mako.fish', 'fish/makoctl.fish')
	fish_comp = dependency('fish', required: false)
	if fish_comp.found()
		fish_install_dir = fish_comp.get_variable('completionsdir')
	else
		fish_install_dir = get_option('datadir') / 'fish/vendor_completions.d'
	endif
	install_data(fish_files, install_dir: fish_install_dir)

endif

if get_option('bash-completions')
	bash_files = files('bash/mako', 'bash/makoctl')
	bash_comp = dependency('bash-completion', required: false)
	if bash_comp.found()
		bash_install_dir = bash_comp.get_variable(
			pkgconfig: 'completionsdir',
			pkgconfig_define: ['datadir', get_option('datadir')]
		)
	else
		bash_install_dir = get_option('datadir') / 'bash-completion/completions'
	endif
	install_data(bash_files, install_dir: bash_install_dir)
endif
