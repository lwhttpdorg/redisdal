# Fetch GoogleTest using FetchContent for consistent test builds.
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
set(INSTALL_GTEST OFF)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Find the platform-specific thread library
find_package(Threads REQUIRED)

# Create a static library for shared test code (e.g., test_env.cpp)
# to avoid recompiling it for every test executable.
set(COMMON_TEST_FILES test_env.cpp)
add_library(janus_test_env STATIC ${COMMON_TEST_FILES})
target_link_libraries(janus_test_env PRIVATE janus)

# Define a common set of libraries required by all test executables.
set(JANUS_TEST_LIBS
    janus_test_env
    gtest_main
    Threads::Threads
)

# Auto-discover all test source files in the current directory.
file(GLOB TEST_SOURCE_FILES "*_test.cpp")

set(DEFAULT_TEST_TIMEOUT 10)
message(STATUS "Setting default test timeout to ${DEFAULT_TEST_TIMEOUT} seconds.")

# Iterate through each discovered test file and create a dedicated executable.
foreach (TEST_FILE ${TEST_SOURCE_FILES})

    # Extract a valid target name (e.g., "hash_ops_test") from the file path.
    get_filename_component(FILENAME_ONLY ${TEST_FILE} NAME)
    string(REGEX REPLACE "\\.cpp$" "" TEST_NAME "${FILENAME_ONLY}")

    message(STATUS "Processing independent test executable: ${TEST_NAME}")

    set(CURRENT_TIMEOUT ${DEFAULT_TEST_TIMEOUT})

    add_executable(${TEST_NAME} ${TEST_FILE})

    target_link_libraries(
        ${TEST_NAME}
        PRIVATE
        ${JANUS_TEST_LIBS}
    )

    # Add the project's root include directory for headers like "janus/janus.hpp".
    target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

    # Discover all GTest cases in the executable and register them with CTest.
    gtest_discover_tests(
        ${TEST_NAME}
        TIMEOUT ${CURRENT_TIMEOUT}
        DISCOVERY_MODE PRE_TEST
    )

endforeach ()
