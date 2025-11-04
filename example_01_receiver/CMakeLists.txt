cmake_minimum_required(VERSION 3.16)

project(receiver VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set paths consistent with qjsink
set(GINST "${CMAKE_CURRENT_SOURCE_DIR}/../install" CACHE PATH "GStreamer installation prefix")
get_filename_component(GINST_ABS "${GINST}" ABSOLUTE)

set(WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
get_filename_component(WORKSPACE_ROOT_ABS "${WORKSPACE_ROOT}" ABSOLUTE)

# Qt install (match qjsink)
set(QT_INSTALL_PREFIX "/Users/dan/qt6-static-build" CACHE PATH "Qt6 installation prefix")
list(APPEND CMAKE_PREFIX_PATH "${QT_INSTALL_PREFIX}")

# Build PKG_CONFIG_PATH to find GStreamer and codec deps
set(PKG_CONFIG_PATH_DIRS
    "${WORKSPACE_ROOT_ABS}/openh264-install/lib/pkgconfig"
    "${WORKSPACE_ROOT_ABS}/vpx-install/lib/pkgconfig"
    "${WORKSPACE_ROOT_ABS}/aom-install/lib/pkgconfig"
    "${WORKSPACE_ROOT_ABS}/openssl-install/lib/pkgconfig"
    "${WORKSPACE_ROOT_ABS}/libwebsockets-install/lib/pkgconfig"
    "/Users/dan/code/gstjitsimeet/deps/coop-install/lib/pkgconfig"
    "${GINST_ABS}/lib/pkgconfig"
    "${GINST_ABS}/lib/gstreamer-1.0/pkgconfig"
    "${QT_INSTALL_PREFIX}/lib/pkgconfig"
)

string(REPLACE ";" ":" PKG_CONFIG_PATH_STR "${PKG_CONFIG_PATH_DIRS}")
set(ENV{PKG_CONFIG_PATH} "${PKG_CONFIG_PATH_STR}:$ENV{PKG_CONFIG_PATH}")

message(STATUS "PKG_CONFIG_PATH for receiver = ${PKG_CONFIG_PATH_STR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-full-1.0)

# Find Qt6 (to satisfy static gstqml6 plugin symbols)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

add_executable(receiver
  main.cpp
  gstutil/pipeline-helper.cpp
)

# # Include headers from gstjitsimeet sources for helpers used by the example
# target_include_directories(receiver PRIVATE
#   /Users/dan/code/gstjitsimeet/src
#   /Users/dan/code/gstjitsimeet/src/examples
# )

# Auto-import QML plugins (even though we don't use QML, this ensures static deps resolve)
qt_import_qml_plugins(receiver)

target_link_libraries(receiver PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    PkgConfig::GSTREAMER
    # include the libgstqml6.a library here to overcome missing symbols: qInitResources_resources(), _gst_plugin_qml6_register
    ${GINST_ABS}/lib/gstreamer-1.0/libgstqml6.a
    ${GINST_ABS}/lib/gstreamer-1.0/libgstjitsimeet.a
    # ${GINST}/lib/libintl.a
)

message(STATUS "GStreamer installation: ${GINST_ABS}")


