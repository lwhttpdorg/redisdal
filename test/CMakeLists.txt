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

# Redis Operations Test
add_janus_test(redis_operations_test redis_operations_test.cpp)
# String Operations Test
add_janus_test(string_operations_test string_test.cpp)
# Hash Operations Test
add_janus_test(hash_operations_test hash_test.cpp)
# List Operations Test
add_janus_test(list_operations_test list_test.cpp)
# Set Operations Test
add_janus_test(set_operations_test set_test.cpp)
# Sorted Set (ZSet) Operations Test
add_janus_test(zset_operations_test zset_test.cpp)
