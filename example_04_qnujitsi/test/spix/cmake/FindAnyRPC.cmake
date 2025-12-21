# FindAnyRPC.cmake
# Simple find module for AnyRPC library (which doesn't provide cmake config files)

find_path(AnyRPC_INCLUDE_DIR
    NAMES anyrpc/anyrpc.h
    HINTS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES include
)

find_library(AnyRPC_LIBRARY
    NAMES anyrpc
    HINTS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AnyRPC
    REQUIRED_VARS AnyRPC_LIBRARY AnyRPC_INCLUDE_DIR
)

if(AnyRPC_FOUND AND NOT TARGET AnyRPC::anyrpc)
    add_library(AnyRPC::anyrpc UNKNOWN IMPORTED)
    set_target_properties(AnyRPC::anyrpc PROPERTIES
        IMPORTED_LOCATION "${AnyRPC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AnyRPC_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(AnyRPC_INCLUDE_DIR AnyRPC_LIBRARY)
