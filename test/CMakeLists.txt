include(FetchContent)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.2)
FetchContent_MakeAvailable(googletest)

set(JANUS_TEST_LIBS
		janus           # For interfaces and include directories
		gtest_main      # For GTest entry point
		hiredis::hiredis # For underlying Redis connection
		pthread         # For threading support (required by GTest/Redis libraries)
)

# Define a macro to reduce repetition for each test file
macro(add_janus_test TEST_NAME FILENAME)
	# 1. Define the executable target
	add_executable(${TEST_NAME} ${FILENAME} test_env.cpp)

	# 2. Link common dependencies
	target_link_libraries(
			${TEST_NAME}
			PRIVATE
			${JANUS_TEST_LIBS}
	)

	# 3. Add the test to CTest
	add_test(
			NAME ${TEST_NAME}_runner
			COMMAND ${TEST_NAME}
	)
endmacro()

# Lua and Raw Command Test
add_janus_test(lua_and_raw_cmd_test lua_and_raw_cmd_test.cpp)
# Redis Operations Test
add_janus_test(key_ops_test key_ops_test.cpp)
# String Operations Test
add_janus_test(string_ops_test string_ops_test.cpp)
# Hash Operations Test
add_janus_test(hash_ops_test hash_ops_test.cpp)
# List Operations Test
add_janus_test(list_ops_test list_ops_test.cpp)
# Set Operations Test
add_janus_test(set_ops_test set_ops_test.cpp)
# Sorted Set (ZSet) Operations Test
add_janus_test(zset_ops_test zset_ops_test.cpp)
