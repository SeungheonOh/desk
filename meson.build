project('desk', 'c')

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

sources = files('src/events.c', 'src/server.c')

executable(
  'testa',
  'src/test.c',
  dependencies: deps,
  include_directories: [inc, wlroots_lib],
  c_args: ['-DWLR_USE_UNSTABLE'],
  )


executable(
  'tiny',
  'src/tiny.c',
  dependencies: deps,
  include_directories: [inc, wlroots_lib],
  c_args: ['-DWLR_USE_UNSTABLE'],
  )

executable(
  'desk',
  'src/desk.c',
  dependencies: deps,
  sources: sources,
  include_directories: [inc, wlroots_lib],
  c_args: ['-DWLR_USE_UNSTABLE'],
  )

