project(
	'mako',
	'c',
	version: '1.2.0',
	license: 'MIT',
	meson_version: '>=0.47.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-missing-braces', language: 'c')

datadir = get_option('datadir')

mako_inc = include_directories('include')

cc = meson.get_compiler('c')

cairo = dependency('cairo')
pango = dependency('pango')
pangocairo = dependency('pangocairo')
glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
realtime = cc.find_library('rt')
wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')

logind = dependency('libsystemd', required: false)
if logind.found()
	add_project_arguments('-DHAVE_SYSTEMD=1', language : 'c')
else
	logind = dependency('libelogind')
	add_project_arguments('-DHAVE_ELOGIND=1', language : 'c')
endif

gdk = dependency('gdk-3.0', required: get_option('icons'))
gdk_pixbuf = dependency('gdk-pixbuf-2.0', required: get_option('icons'))
if gdk.found() and gdk_pixbuf.found()
	add_global_arguments('-DHAVE_ICONS=1', language : 'c')
endif

subdir('contrib/apparmor')
subdir('contrib/completions')
subdir('protocol')

src_files = [
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
	'criteria.c',
	'types.c',
	'icon.c',
	'string-util.c',
]

executable(
	'mako',
	files(src_files),
	dependencies: [
		cairo,
		client_protos,
		gdk,
		gdk_pixbuf,
		logind,
		pango,
		pangocairo,
		glib,
		gobject,
		realtime,
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

scdoc = find_program('scdoc', required: get_option('man-pages'))

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
