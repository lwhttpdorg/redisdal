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

# Find the platform-specific thread library
find_package(Threads REQUIRED)

# --- 2. Create a common library for shared test code ---
# This avoids recompiling test_env.cpp for every single test executable.
set(COMMON_TEST_FILES test_env.cpp)
add_library(janus_test_env STATIC ${COMMON_TEST_FILES})

# Link the main janus library to our test environment library.
# This makes the janus library's headers and symbols available to test_env.cpp.
target_link_libraries(janus_test_env PRIVATE janus)

# --- 3. Define Common Test Libraries ---
set(JANUS_TEST_LIBS
    janus_test_env   # Our common test code (which already includes the 'janus' library).
    gtest            # The GTest framework (without a main function, as we provide our own).
    Threads::Threads # Platform-agnostic threading support.
)

# --- 4. Auto-discover Test Source Files ---
# Find all files ending with '_test.cpp' in the current CMake directory (may return full paths).
file(GLOB TEST_SOURCE_FILES "*_test.cpp")

# --- 4. Set Default Timeout ---
# Set a default timeout for all tests (in seconds)
set(DEFAULT_TEST_TIMEOUT 10)
message(STATUS "Setting default test timeout to ${DEFAULT_TEST_TIMEOUT} seconds.")

# --- 5. Iterate and Configure Independent Test Executables ---
# Loop through each discovered test source file to create a dedicated executable.
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
    # Each test executable is built from its own source file.
    add_executable(${TEST_NAME} ${TEST_FILE})

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
