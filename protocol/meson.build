wl_protocol_dir = wayland_protos.get_variable(pkgconfig: 'pkgdatadir')

wayland_scanner = dependency('wayland-scanner', version: '>=1.14.91', native: true)
wayland_scanner_path = wayland_scanner.get_variable(pkgconfig: 'wayland_scanner')
wayland_scanner_prog = find_program(wayland_scanner_path, native: true)

wayland_scanner_code = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

protocols = [
	wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
	wl_protocol_dir / 'staging/cursor-shape/cursor-shape-v1.xml',
	wl_protocol_dir / 'staging/xdg-activation/xdg-activation-v1.xml',
	wl_protocol_dir / 'unstable/tablet/tablet-unstable-v2.xml',
	'wlr-layer-shell-unstable-v1.xml',
]

protocols_src = []
foreach p : protocols
	protocols_src += wayland_scanner_code.process(p)
	protocols_src += wayland_scanner_client.process(p)
endforeach
