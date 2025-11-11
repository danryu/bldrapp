cmake_minimum_required(VERSION 3.16)

project(qnujitsi VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set paths
set(GINST "${CMAKE_CURRENT_SOURCE_DIR}/../install" CACHE PATH "GStreamer installation prefix")
# Convert GINST to absolute path for use in PKG_CONFIG_PATH
get_filename_component(GINST_ABS "${GINST}" ABSOLUTE)

# Codec libraries are installed at the workspace root, not under GINST
set(WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
get_filename_component(WORKSPACE_ROOT_ABS "${WORKSPACE_ROOT}" ABSOLUTE)

set(QT_INSTALL_PREFIX "/Users/dan/qt6-static-build" CACHE PATH "Qt6 installation prefix")

# Add to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH "${QT_INSTALL_PREFIX}")

# Build PKG_CONFIG_PATH to find GStreamer (including plugin pkgconfig files)
set(PKG_CONFIG_PATH_DIRS
    "${WORKSPACE_ROOT_ABS}/vpx-install/lib/pkgconfig"
    "${WORKSPACE_ROOT_ABS}/openssl-install/lib/pkgconfig"
    "${WORKSPACE_ROOT_ABS}/libwebsockets-install/lib/pkgconfig"
    "/Users/dan/code/gstjitsimeet/deps/coop-install/lib/pkgconfig"
    "${GINST_ABS}/lib/pkgconfig"
    "${GINST_ABS}/lib/gstreamer-1.0/pkgconfig"
    "${QT_INSTALL_PREFIX}/lib/pkgconfig"
)

# Join with colons and set environment
string(REPLACE ";" ":" PKG_CONFIG_PATH_STR "${PKG_CONFIG_PATH_DIRS}")
set(ENV{PKG_CONFIG_PATH} "${PKG_CONFIG_PATH_STR}:$ENV{PKG_CONFIG_PATH}")

# Debug: Print the constructed PKG_CONFIG_PATH
message(STATUS "PKG_CONFIG_PATH_STR = ${PKG_CONFIG_PATH_STR}")
message(STATUS "ENV{PKG_CONFIG_PATH} = $ENV{PKG_CONFIG_PATH}")

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

# Find GStreamer via pkg-config
find_package(PkgConfig REQUIRED)

# Set PKG_CONFIG_PATH for pkg-config to find all dependencies
set(ENV{PKG_CONFIG_PATH} "${PKG_CONFIG_PATH_STR}:$ENV{PKG_CONFIG_PATH}")

# Debug: Print PKG_CONFIG_PATH right before pkg_check_modules
message(STATUS "About to run pkg_check_modules with PKG_CONFIG_PATH:")
message(STATUS "  $ENV{PKG_CONFIG_PATH}")

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-full-1.0)

# Source files
set(SOURCES main.cpp participant_manager.cpp send_pipeline.cpp conference.cpp)
# When statically linking libraries with embedded Qt resources, always use unique QRC filenames to avoid qInitResources_* function name collisions
# ie specifically don't use filename "resources.qrc", ever. Use a unique filename like "qnujitsi.qrc"
# Otherwise you'll get runtime errors like Failed to find shader ":/org/freedesktop/gstreamer/qml6/vertex.vert.qsb
set(RESOURCES qnujitsi.qrc)

# Create executable
add_executable(qnujitsi ${SOURCES} ${RESOURCES})

# Target properties
set_target_properties(qnujitsi PROPERTIES
    MACOSX_BUNDLE FALSE
    WIN32_EXECUTABLE TRUE
)

# Enable automatic QML plugin imports for static builds
set_target_properties(qnujitsi PROPERTIES
    QT_QML_MODULE_URI "QtQuick"
    QT_QML_IMPORT_PATH "${QT_INSTALL_PREFIX}/qml"
)

# Use qt_import_qml_plugins to automatically link required QML plugins
qt_import_qml_plugins(qnujitsi)

# Link libraries
target_link_libraries(qnujitsi PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    PkgConfig::GSTREAMER
    # include the libgstqml6.a library here to overcome missing symbols: qInitResources_resources(), _gst_plugin_qml6_register
    ${GINST_ABS}/lib/gstreamer-1.0/libgstqml6.a
    # ${GINST}/lib/libintl.a
)

# Information messages
message(STATUS "GStreamer installation: ${GINST}")
message(STATUS "Qt6 installation: ${QT_INSTALL_PREFIX}")
message(STATUS "")
