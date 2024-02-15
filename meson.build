project('desk', 'c',
	default_options: [
		'optimization=2',
		'buildtype=debugoptimized',
	],
)

wlroots = subproject('wlroots', default_options: ['examples=false'])
wlroots_lib = wlroots.get_variable('wlr_inc')
wlroots_deps = wlroots.get_variable('wlroots')

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

# executable(
#   'testa',
#   'temp/test.c',
#   dependencies: deps,
#   include_directories: [inc, wlroots_lib],
#   c_args: ['-DWLR_USE_UNSTABLE -O'],
#   )

# executable(
#   'tiny',
#   'temp/tiny.c',
#   dependencies: deps,
#   include_directories: [inc, wlroots_lib],
#   c_args: ['-DWLR_USE_UNSTABLE -O'],
#   )

executable(
  'main',
  'temp/main.c',
  dependencies: deps,
  sources: src,
  include_directories: [inc, wlroots_lib],
  c_args: ['-DWLR_USE_UNSTABLE -O'],
  )

desk = executable(
  'desk',
  'src/desk.c',
  dependencies: deps,
  sources: src,
  include_directories: [inc, wlroots_lib],
  c_args: ['-DWLR_USE_UNSTABLE -O'],
  )
