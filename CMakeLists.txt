cmake_minimum_required(VERSION 3.11)

project(janus)

set(TARGET_NAME ${CMAKE_PROJECT_NAME})

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds not allowed. Please create a separate build directory.")
endif ()

add_library(${TARGET_NAME} INTERFACE)
target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(${TARGET_NAME} INTERFACE cxx_std_17)

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

if (TARGET hiredis::hiredis)
	target_link_libraries(${TARGET_NAME} INTERFACE hiredis::hiredis)
else ()
	find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
	find_library(HIREDIS_LIB hiredis)

	if (NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIB)
		message(FATAL_ERROR "hiredis not found")
	endif ()

	add_library(hiredis INTERFACE)
	target_include_directories(hiredis INTERFACE ${HIREDIS_INCLUDE_DIR})
	target_link_libraries(hiredis INTERFACE ${HIREDIS_LIB})

	add_library(hiredis::hiredis ALIAS hiredis)

	target_link_libraries(${TARGET_NAME} INTERFACE hiredis::hiredis)
endif ()

option(ENABLE_JANUS_TEST "Enable janus test" OFF)

if (ENABLE_JANUS_TEST)
	enable_testing()
	add_subdirectory(test)
endif ()
