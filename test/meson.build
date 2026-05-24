gtest_proj = subproject('gtest')
gtest_main_dep = gtest_proj.get_variable('gtest_main_dep')
thread_dep = dependency('threads')

redisdal_test_env_lib = static_library('redisdal_test_env',
  'test_env.cpp',
  dependencies: redisdal_dep
)

redisdal_test_env_dep = declare_dependency(
  link_with: redisdal_test_env_lib
)

redisdal_test_libs = [
  redisdal_test_env_dep,
  gtest_main_dep,
  thread_dep
]

test_sources = [
  'hash_ops_test.cpp',
  'key_ops_test.cpp',
  'list_ops_test.cpp',
  'lua_and_raw_cmd_test.cpp',
  'redis_connection_test.cpp',
  'redis_tpl_test.cpp',
  'serializer_test.cpp',
  'set_ops_test.cpp',
  'string_ops_test.cpp',
  'zset_ops_test.cpp',
]

foreach f : test_sources
  test_name = f.split('.')[0]
  
  exe = executable(test_name,
    f,
    include_directories: '../include',
    dependencies: redisdal_test_libs,
  )
  
  test(test_name, exe, timeout: 10)
endforeach
