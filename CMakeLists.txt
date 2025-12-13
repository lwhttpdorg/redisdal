cmake_minimum_required(VERSION 3.10)

project(janus)

set(TARGET_NAME ${CMAKE_PROJECT_NAME})

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please create a separate build directory.")
endif ()

# Set output directories for build artifacts to keep the build folder clean.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # For static libraries (.a)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # For shared libraries (.so)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # For executables

add_library(${TARGET_NAME} SHARED src/redis_connection.cpp)
target_include_directories(${TARGET_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "The default is the Release version" FORCE)
endif ()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    endif ()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif ()

find_package(hiredis QUIET)

if (NOT TARGET hiredis::hiredis)
    find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
    find_library(HIREDIS_LIB hiredis)

    if (NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIB)
        message(FATAL_ERROR "hiredis not found")
    endif ()

    add_library(hiredis INTERFACE IMPORTED GLOBAL)
    target_include_directories(hiredis INTERFACE ${HIREDIS_INCLUDE_DIR})
    target_link_libraries(hiredis INTERFACE ${HIREDIS_LIB})

    add_library(hiredis::hiredis ALIAS hiredis)
endif ()

target_link_libraries(${TARGET_NAME} PUBLIC hiredis::hiredis)

# --- Installation Rules ---
# install headers
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/janus
    DESTINATION include # Installs to <prefix>/include/janus
)

# install libraries
install(
    DIRECTORY ${CMAKE_BINARY_DIR}/lib/
    DESTINATION lib
)

option(ENABLE_JANUS_TEST "Enable janus test" OFF)

if (ENABLE_JANUS_TEST)
    enable_testing()
    add_subdirectory(test)
endif ()
