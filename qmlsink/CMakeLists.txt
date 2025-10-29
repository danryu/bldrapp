cmake_minimum_required(VERSION 3.16)

project(qml6sink VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set paths
set(GSTREAMER_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/../install" CACHE PATH "GStreamer installation prefix")
set(QT_INSTALL_PREFIX "/Users/dan/qt6-static-build" CACHE PATH "Qt6 installation prefix")

# Add to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH "${QT_INSTALL_PREFIX}")

# Set PKG_CONFIG_PATH to find GStreamer (including plugin pkgconfig files)
set(ENV{PKG_CONFIG_PATH} "${GSTREAMER_INSTALL_PREFIX}/lib/pkgconfig:${GSTREAMER_INSTALL_PREFIX}/lib/gstreamer-1.0/pkgconfig:${QT_INSTALL_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

# Find GStreamer via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-full-1.0)

# On macOS, replace .so GIO module files with .a versions
# The macOS linker doesn't support .so files, only .dylib for shared libraries
if(APPLE)
    # Get all link libraries from the imported target
    get_target_property(GSTREAMER_LINK_LIBS PkgConfig::GSTREAMER INTERFACE_LINK_LIBRARIES)
    if(GSTREAMER_LINK_LIBS)
        set(FILTERED_LIBS "")
        foreach(lib ${GSTREAMER_LINK_LIBS})
            # Replace .so files with .a equivalents
            string(REGEX REPLACE "\\.so$" ".a" lib_fixed "${lib}")
            # Only add if it doesn't end with .so (to catch any that weren't replaced)
            if(NOT lib_fixed MATCHES "\\.so$")
                list(APPEND FILTERED_LIBS "${lib_fixed}")
            endif()
        endforeach()
        
        set_target_properties(PkgConfig::GSTREAMER PROPERTIES
            INTERFACE_LINK_LIBRARIES "${FILTERED_LIBS}"
        )
    endif()
    
    # Also check INTERFACE_LINK_OPTIONS
    get_target_property(GSTREAMER_LINK_OPTIONS PkgConfig::GSTREAMER INTERFACE_LINK_OPTIONS)
    if(GSTREAMER_LINK_OPTIONS)
        set(FILTERED_OPTIONS "")
        foreach(opt ${GSTREAMER_LINK_OPTIONS})
            if(NOT opt MATCHES "\\.so")
                list(APPEND FILTERED_OPTIONS "${opt}")
            endif()
        endforeach()
        set_target_properties(PkgConfig::GSTREAMER PROPERTIES
            INTERFACE_LINK_OPTIONS "${FILTERED_OPTIONS}"
        )
    endif()
endif()

# Source files
set(SOURCES main.cpp)
set(RESOURCES qmlsink.qrc)

# Create executable
add_executable(qml6sink ${SOURCES} ${RESOURCES})

# Target properties
set_target_properties(qml6sink PROPERTIES
    MACOSX_BUNDLE FALSE
    WIN32_EXECUTABLE TRUE
)

# Enable automatic QML plugin imports for static builds
set_target_properties(qml6sink PROPERTIES
    QT_QML_MODULE_URI "QtQuick"
    QT_QML_IMPORT_PATH "${QT_INSTALL_PREFIX}/qml"
)

# Use qt_import_qml_plugins to automatically link required QML plugins
qt_import_qml_plugins(qml6sink)

# Link libraries
target_link_libraries(qml6sink PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    PkgConfig::GSTREAMER
    ${GSTREAMER_INSTALL_PREFIX}/lib/gstreamer-1.0/libgstqml6.a
    ${GSTREAMER_INSTALL_PREFIX}/lib/libintl.a
)

# Add linker flags to alias g_libintl_* symbols to libintl_* for proxy-libintl compatibility
if(APPLE)
    target_link_options(qml6sink PRIVATE
        -Wl,-alias,_g_libintl_bind_textdomain_codeset,_libintl_bind_textdomain_codeset
        -Wl,-alias,_g_libintl_bindtextdomain,_libintl_bindtextdomain
        -Wl,-alias,_g_libintl_gettext,_libintl_gettext
        -Wl,-alias,_g_libintl_dgettext,_libintl_dgettext
        -Wl,-alias,_g_libintl_dcgettext,_libintl_dcgettext
        -Wl,-alias,_g_libintl_ngettext,_libintl_ngettext
        -Wl,-alias,_g_libintl_dngettext,_libintl_dngettext
        -Wl,-alias,_g_libintl_dcngettext,_libintl_dcngettext
    )
endif()

# Platform-specific frameworks for macOS
if(APPLE)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(OPENGL_FRAMEWORK OpenGL)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(METAL_FRAMEWORK Metal)
    find_library(IOSURFACE_FRAMEWORK IOSurface)
    
    target_link_libraries(qml6sink PRIVATE
        ${FOUNDATION_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${OPENGL_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${IOSURFACE_FRAMEWORK}
    )
    
    set_target_properties(qml6sink PROPERTIES
        MACOSX_DEPLOYMENT_TARGET "11.0"
    )
endif()

# Information messages
message(STATUS "GStreamer installation: ${GSTREAMER_INSTALL_PREFIX}")
message(STATUS "Qt6 installation: ${QT_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "To run qml6sink, set these environment variables:")
message(STATUS "  export GST_PLUGIN_PATH=\"${GSTREAMER_INSTALL_PREFIX}/lib/gstreamer-1.0\"")
message(STATUS "  export DYLD_LIBRARY_PATH=\"${GSTREAMER_INSTALL_PREFIX}/lib:\$DYLD_LIBRARY_PATH\"")
message(STATUS "  export QML2_IMPORT_PATH=\"${QT_INSTALL_PREFIX}/qml:\$QML2_IMPORT_PATH\"")