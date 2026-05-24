cmake_minimum_required(VERSION 3.10)

project(redisdal VERSION 2.0)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please create a separate build directory.")
endif ()

# --- Library Definition ---
add_library(redisdal SHARED src/redis_connection.cpp)
target_include_directories(redisdal
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(redisdal PUBLIC cxx_std_17)

# --- Build Type Configuration ---
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

# --- hiredis Dependency ---
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
target_link_libraries(redisdal PUBLIC hiredis::hiredis)

# --- Installation Rules ---
include(GNUInstallDirs)

# Install headers
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/redisdal"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install libraries properly by referencing the TARGET, not the directory
install(
    TARGETS redisdal
    EXPORT redisdalTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # Dynamic libraries
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # Static libraries
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # Executables
)

# Export targets file for downstream consumers
install(EXPORT redisdalTargets
    FILE redisdalTargets.cmake
    NAMESPACE RedisDAL::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Redisdal)

# Configure and install pkg-config file
set(REDISDAL_VERSION ${PROJECT_VERSION})
configure_file(${CMAKE_SOURCE_DIR}/pkgconfig/redisdal.pc.in ${CMAKE_BINARY_DIR}/redisdal.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/redisdal.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Install CMake find/module file so external projects can use find_package
install(FILES ${CMAKE_SOURCE_DIR}/cmake/FindRedisdal.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Redisdal)

# --- Test Configuration ---
option(ENABLE_REDISDAL_TEST "Enable redisdal test" OFF)

if (ENABLE_REDISDAL_TEST)
    enable_testing()
    add_subdirectory(test)
endif ()
