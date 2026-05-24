# Findredisdal.cmake - Find redisdal library and headers
# Usage: find_package(redisdal REQUIRED)

find_path(redisdal_INCLUDE_DIR
    NAMES redisdal/redisdal.hpp
)

find_library(redisdal_LIBRARY
    NAMES redisdal
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(redisdal
    REQUIRED_VARS redisdal_LIBRARY redisdal_INCLUDE_DIR
)

if(redisdal_FOUND AND NOT TARGET redisdal::redisdal)
    add_library(redisdal::redisdal UNKNOWN IMPORTED)
    set_target_properties(redisdal::redisdal PROPERTIES
        IMPORTED_LOCATION "${redisdal_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${redisdal_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(redisdal_INCLUDE_DIR redisdal_LIBRARY)
