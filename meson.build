project('desk', 'c',
	default_options: [
		'optimization=2',
		'buildtype=debugoptimized',
	],
)

wlroots = subproject('wlroots', default_options: ['examples=false'])
wlroots_lib = wlroots.get_variable('wlr_inc')
wlroots_deps = wlroots.get_variable('wlroots')

# Generate layer-shell protocol header (needed by wlr_layer_shell_v1.h)
wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(wayland_scanner_dep.get_variable('wayland_scanner'), native: true)

layer_shell_protocol_h = custom_target(
  'wlr-layer-shell-unstable-v1-protocol_h',
  input: 'protocols/wlr-layer-shell-unstable-v1.xml',
  output: 'wlr-layer-shell-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)



deps = [
     wlroots_deps,
     dependency('egl'),
     dependency('gl'),
     dependency('cairo'),
     dependency('cglm'),
]

inc = [
    include_directories('protocols'),
]

src = []
subdir('src')

desk = executable(
  'desk',
  'src/desk.c',
  layer_shell_protocol_h,
  dependencies: deps,
  sources: src,
  include_directories: [inc, wlroots_lib],
  c_args: ['-DWLR_USE_UNSTABLE -O'],
  )
