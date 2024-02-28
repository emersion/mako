scdoc = dependency('scdoc', required: get_option('man-pages'), version: '>= 1.9.7', native: true)
if not scdoc.found()
	subdir_done()
endif

man_pages = ['mako.1.scd', 'mako.5.scd', 'makoctl.1.scd']
mandir = get_option('mandir')

foreach src : man_pages
	topic = src.split('.')[0]
	section = src.split('.')[1]
	output = '@0@.@1@'.format(topic, section)

	custom_target(
		output,
		input: src,
		output: output,
		command: scdoc.get_variable('scdoc'),
		feed: true,
		capture: true,
		install: true,
		install_dir: '@0@/man@1@'.format(mandir, section)
	)
endforeach
