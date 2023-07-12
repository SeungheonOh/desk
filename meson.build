project('desk', 'c')

wlroots = subproject('wlroots', default_options: ['examples=false'])
wlroots_lib = wlroots.get_variable('wlr_inc')
wlroots_deps = wlroots.get_variable('wlroots')

deps = [
     wlroots_deps,
     dependency('egl'),
     dependency('gl'),     
]

inc = [
    include_directories('protocols'),
]

executable(
  'desk',
  'src/main.c',
  dependencies: deps,
  include_directories: [inc, wlroots_lib],
  c_args: ['-DWLR_USE_UNSTABLE'],
  )


executable(
  'spy',
  'src/test.c',
  dependencies: deps,
  include_directories: [inc, wlroots_lib],
  c_args: ['-DWLR_USE_UNSTABLE'],
  )

