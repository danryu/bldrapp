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

# Information messages
message(STATUS "GStreamer installation: ${GSTREAMER_INSTALL_PREFIX}")
message(STATUS "Qt6 installation: ${QT_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "To run qml6sink, set these environment variables:")
message(STATUS "  export GST_PLUGIN_PATH=\"${GSTREAMER_INSTALL_PREFIX}/lib/gstreamer-1.0\"")
message(STATUS "  export DYLD_LIBRARY_PATH=\"${GSTREAMER_INSTALL_PREFIX}/lib:\$DYLD_LIBRARY_PATH\"")
message(STATUS "  export QML2_IMPORT_PATH=\"${QT_INSTALL_PREFIX}/qml:\$QML2_IMPORT_PATH\"")