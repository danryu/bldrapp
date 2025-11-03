cmake_minimum_required(VERSION 3.16)

project(qjsink VERSION 1.0.0 LANGUAGES CXX C)

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
set(ENV{PKG_CONFIG_PATH} "/Users/dan/code/gstreamer-build/aom-install/lib/pkgconfig:/Users/dan/code/gstreamer-build/libwebsockets-install/lib/pkgconfig:/Users/dan/code/gstjitsimeet/deps/coop-install/lib/pkgconfig:${GSTREAMER_INSTALL_PREFIX}/lib/pkgconfig:${GSTREAMER_INSTALL_PREFIX}/lib/gstreamer-1.0/pkgconfig:${QT_INSTALL_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

# Find GStreamer via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-full-1.0)

# Source files
set(SOURCES main.cpp)
# When statically linking libraries with embedded Qt resources, always use unique QRC filenames to avoid qInitResources_* function name collisions
# ie specifically don't use filename "resources.qrc", ever. Use a unique filename like "qjsink.qrc"
# Otherwise you'll get runtime errors like Failed to find shader ":/org/freedesktop/gstreamer/qml6/vertex.vert.qsb
set(RESOURCES qjsink.qrc)

# Create executable
add_executable(qjsink ${SOURCES} ${RESOURCES})

# Target properties
set_target_properties(qjsink PROPERTIES
    MACOSX_BUNDLE FALSE
    WIN32_EXECUTABLE TRUE
)

# Enable automatic QML plugin imports for static builds
set_target_properties(qjsink PROPERTIES
    QT_QML_MODULE_URI "QtQuick"
    QT_QML_IMPORT_PATH "${QT_INSTALL_PREFIX}/qml"
)

# Use qt_import_qml_plugins to automatically link required QML plugins
qt_import_qml_plugins(qjsink)

# Link libraries
target_link_libraries(qjsink PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    PkgConfig::GSTREAMER
    # include the libgstqml6.a library here to overcome missing symbols: qInitResources_resources(), _gst_plugin_qml6_register
    ${GSTREAMER_INSTALL_PREFIX}/lib/gstreamer-1.0/libgstqml6.a
    # ${GSTREAMER_INSTALL_PREFIX}/lib/libintl.a
)

# Information messages
message(STATUS "GStreamer installation: ${GSTREAMER_INSTALL_PREFIX}")
message(STATUS "Qt6 installation: ${QT_INSTALL_PREFIX}")
message(STATUS "")
