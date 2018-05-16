project(
	'mako',
	'c',
	version: '0.0.0',
	license: 'MIT',
	meson_version: '>=0.43.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-missing-braces', language: 'c')

mako_inc = include_directories('include')

cairo = dependency('cairo')
libsystemd = dependency('libsystemd')
pango = dependency('pango')
pangocairo = dependency('pangocairo')
wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')

subdir('protocol')

executable(
	'mako',
	files([
		'config.c',
		'event-loop.c',
		'dbus/dbus.c',
		'dbus/mako.c',
		'dbus/xdg.c',
		'main.c',
		'notification.c',
		'pool-buffer.c',
		'render.c',
		'wayland.c',
	]),
	dependencies: [
		cairo,
		client_protos,
		libsystemd,
		pango,
		pangocairo,
		wayland_client,
	],
	include_directories: [mako_inc],
	install: true,
)

install_data(
	'makoctl',
	install_dir: get_option('bindir'),
	install_mode: 'rwxr-xr-x',
)

scdoc = find_program('scdoc', required: false)

if scdoc.found()
	sh = find_program('sh')

	man_pages = ['mako.1.scd', 'makoctl.1.scd']

	mandir = get_option('mandir')

	foreach src : man_pages
		topic = src.split('.')[0]
		section = src.split('.')[1]
		output = '@0@.@1@'.format(topic, section)

		custom_target(
			output,
			input: src,
			output: output,
			command: [
				sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.path(), output)
			],
			install: true,
			install_dir: '@0@/man@1@'.format(mandir, section)
		)
	endforeach
endif
