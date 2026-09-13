project(
    'redisdal',
    'cpp',
    version: '2.1',
    meson_version: '>=1.1.0',
    default_options: ['cpp_std=c++17'],
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

redisdal_lib = library(
    'redisdal',
    'src/redis_connection.cpp',
    include_directories: redisdal_inc,
    dependencies: hiredis_dep,
    install: true,
)

redisdal_dep = declare_dependency(
    include_directories: redisdal_inc,
    link_with: redisdal_lib,
    dependencies: hiredis_dep,
)

install_subdir('include/redisdal', install_dir: get_option('includedir'))

if get_option('enable_redisdal_test')
    subdir('test')
endif

# Generate and install pkg-config file
conf_data = configuration_data()
conf_data.set('REDISDAL_VERSION', meson.project_version())
conf_data.set('REDISDAL_PREFIX', get_option('prefix'))
conf_data.set('REDISDAL_LIBDIR', join_paths('${prefix}', get_option('libdir')))
conf_data.set('REDISDAL_INCLUDEDIR', join_paths('${prefix}', get_option('includedir')))
redisdal_pc = configure_file(
    input: 'pkgconfig/redisdal.pc.in',
    output: 'redisdal.pc',
    configuration: conf_data,
)
install_data(
    redisdal_pc,
    install_dir: join_paths(get_option('libdir'), 'pkgconfig'),
)

# Install CMake Find module for external projects (match CMake install path)
install_data(
    'cmake/FindRedisdal.cmake',
    install_dir: join_paths(get_option('libdir'), 'cmake', 'Redisdal'),
)
