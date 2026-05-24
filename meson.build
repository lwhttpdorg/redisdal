project('redisdal', 'cpp',
  version: '1.0',
  meson_version: '>=1.1.0',
  default_options: ['cpp_std=c++17']
)

cpp = meson.get_compiler('cpp')

if get_option('buildtype') == 'debug'
  add_project_arguments('-D_DEBUG', language: 'cpp')
endif

hiredis_dep = dependency('hiredis', required: false)

if not hiredis_dep.found()
  hiredis_lib = cpp.find_library('hiredis', required: true)
  hiredis_dep = declare_dependency(dependencies: hiredis_lib)
endif

redisdal_inc = include_directories('include')

redisdal_lib = library('redisdal',
  'src/redis_connection.cpp',
  include_directories: redisdal_inc,
  dependencies: hiredis_dep,
  install: true
)

redisdal_dep = declare_dependency(
  include_directories: redisdal_inc,
  link_with: redisdal_lib,
  dependencies: hiredis_dep
)

install_subdir('include/redisdal', install_dir: 'include')

if get_option('enable_redisdal_test')
  subdir('test')
endif
