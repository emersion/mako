project(
	'mako',
	'c',
	version: '1.10.0',
	license: 'MIT',
	meson_version: '>=0.60.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments([
	'-D_POSIX_C_SOURCE=200809L',

	'-Wundef',

	'-Wno-unused-parameter',
	'-Wno-missing-braces',
], language: 'c')

mako_inc = include_directories('include')

cc = meson.get_compiler('c')

cairo = dependency('cairo')
pango = dependency('pango')
pangocairo = dependency('pangocairo')
glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
realtime = cc.find_library('rt')
wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols', version: '>=1.32')
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
add_project_arguments('-DHAVE_' + sdbus.name().to_upper(), language: 'c')

gdk_pixbuf = dependency('gdk-pixbuf-2.0', required: get_option('icons'))
if gdk_pixbuf.found()
	add_project_arguments('-DHAVE_ICONS', language: 'c')
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
	'mode.c',
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
	files(src_files) + protocols_src,
	dependencies: [
		cairo,
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

executable(
	'makoctl',
	['makoctl.c'],
	dependencies: [sdbus],
	install: true,
)

conf_data = configuration_data()
conf_data.set('bindir', get_option('prefix') / get_option('bindir'))
conf_data.set('systemd_service', '')

systemd = dependency('systemd', required: get_option('systemd'))
if systemd.found()
	service_file = 'mako.service'
	user_unit_dir = systemd.get_variable(pkgconfig: 'systemduserunitdir',
		pkgconfig_define: ['prefix', get_option('prefix')])
	conf_data.set('systemd_service', 'SystemdService=' + service_file)

	install_data(
		'contrib' / 'systemd' / service_file,
		install_dir: user_unit_dir
	)
endif

configure_file(
	configuration: conf_data,
	input: 'fr.emersion.mako.service.in',
	output: '@BASENAME@',
	install_dir: get_option('datadir') + '/dbus-1/services',
)

subdir('doc')

summary({
	'sd-bus provider': sdbus.name(),
	'Icons': gdk_pixbuf.found(),
	'Man pages': scdoc.found(),
}, bool_yn: true)
