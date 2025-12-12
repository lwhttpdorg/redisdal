# This configuration file automates the creation and registration of GTest executables.
# It ensures each test source file creates an independent executable for fast debugging.
# It uses 'gtest_discover_tests' for CTest integration and sets a default timeout.

# --- 1. Fetch External Dependencies ---
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

# Tell googletest's build system not to install itself.
set(INSTALL_GTEST OFF)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# --- 2. Define Common Test Libraries ---
set(JANUS_TEST_LIBS
    janus           # For interfaces and include directories
    gtest_main      # For GTest entry point (provides main() and links gtest/gtest_main)
    hiredis::hiredis # For underlying Redis connection
    pthread         # For threading support
)

# --- 3. Auto-discover Test Source Files ---
# Find all files ending with '_test.cpp' in the current CMake directory (may return full paths).
file(GLOB TEST_SOURCE_FILES "*_test.cpp")

# Define the common environment file required by all tests
set(COMMON_TEST_FILE test_env.cpp)

# --- 4. Set Default Timeout ---
# Set a default timeout for all tests (in seconds)
set(DEFAULT_TEST_TIMEOUT 10)
message(STATUS "Setting default test timeout to ${DEFAULT_TEST_TIMEOUT} seconds.")

# --- 5. Iterate and Configure Independent Test Executables ---
# Loop through each discovered test source file
foreach (TEST_FILE ${TEST_SOURCE_FILES})

    # CRITICAL FIX: Extract the pure filename from the full path first.
    # This prevents the target name from containing slashes (e.g., /home/...).
    get_filename_component(FILENAME_ONLY ${TEST_FILE} NAME)

    # Extract the target name by removing the .cpp extension.
    string(REGEX REPLACE "\\.cpp$" "" TEST_NAME "${FILENAME_ONLY}")

    message(STATUS "Processing independent test executable: ${TEST_NAME}")

    # Set current timeout to default
    set(CURRENT_TIMEOUT ${DEFAULT_TEST_TIMEOUT})

    # 1. Define the independent executable target
    # TEST_NAME is now a valid target name (e.g., hash_ops_test).
    # We use the full ${TEST_FILE} path as the source for the executable.
    add_executable(${TEST_NAME} ${TEST_FILE} ${COMMON_TEST_FILE})

    # 2. Link common dependencies
    target_link_libraries(
        ${TEST_NAME}
        PRIVATE
        ${JANUS_TEST_LIBS}
    ) 

    # 3. Automatic Test Discovery and CTest Registration
    # The TIMEOUT applies to every single GTest case discovered within this executable.
    gtest_discover_tests(
        ${TEST_NAME}
        TIMEOUT ${CURRENT_TIMEOUT}
    )

endforeach ()

# --- Build Comprehensive Example ---
# This builds the main example from the README as a standalone executable.
# It is not a CTest test but serves as a runnable demonstration of the library's features.
add_executable(janus_example janus_example.cpp)
target_link_libraries(janus_example PRIVATE ${JANUS_TEST_LIBS})
gtest_discover_tests(janus_example TIMEOUT ${DEFAULT_TEST_TIMEOUT})
