project(
	'mako',
	'c',
	version: '1.6',
	license: 'MIT',
	meson_version: '>=0.60.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments([
	'-Wno-unused-parameter',
	'-Wno-missing-braces',
], language: 'c')

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
wayland_protos = dependency('wayland-protocols', version: '>=1.21')
wayland_cursor = dependency('wayland-cursor')

epoll = dependency('', required: false)
if (not cc.has_function('timerfd_create', prefix: '#include <sys/timerfd.h>') or
    not cc.has_function('signalfd', prefix: '#include <sys/signalfd.h>'))
	epoll = dependency('epoll-shim')
endif

if get_option('sd-bus-provider') == 'auto'
	assert(get_option('auto_features').auto(), 'sd-bus-provider must not be set to auto since auto_features != auto')
	sdbus = dependency('libsystemd', 'libelogind', 'basu')
else
	sdbus = dependency(get_option('sd-bus-provider'))
endif
add_project_arguments('-DHAVE_' + sdbus.name().to_upper() + '=1', language: 'c')

gdk_pixbuf = dependency('gdk-pixbuf-2.0', required: get_option('icons'))
if gdk_pixbuf.found()
	add_global_arguments('-DHAVE_ICONS=1', language: 'c')
endif

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
	'surface.c',
	'icon.c',
	'string-util.c',
]

if gdk_pixbuf.found()
	src_files += 'cairo-pixbuf.c'
endif

executable(
	'mako',
	files(src_files),
	dependencies: [
		cairo,
		client_protos,
		epoll,
		gdk_pixbuf,
		sdbus,
		pango,
		pangocairo,
		glib,
		gobject,
		realtime,
		wayland_client,
		wayland_cursor,
	],
	include_directories: [mako_inc],
	install: true,
)

install_data(
	'makoctl',
	install_dir: get_option('bindir'),
	install_mode: 'rwxr-xr-x',
)

conf_data = configuration_data()
conf_data.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

configure_file(
	configuration: conf_data,
	input: 'fr.emersion.mako.service.in',
	output: '@BASENAME@',
	install_dir: datadir + '/dbus-1/services',
)

scdoc = dependency('scdoc', required: get_option('man-pages'), version: '>= 1.9.7', native: true)
if scdoc.found()
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
endif
