cmake_minimum_required(VERSION 3.14)
project(GstTestApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point to Qt6 static build
set(CMAKE_PREFIX_PATH "/Users/dan/qt6-static-build")

# Set the PKG_CONFIG_PATH to point to your custom GStreamer installation
set(ENV{PKG_CONFIG_PATH} "/Users/dan/code/gstreamer-build/install/lib/pkgconfig:/Users/dan/code/gstreamer-build/install/lib/gstreamer-1.0/pkgconfig:$ENV{PKG_CONFIG_PATH}")

# Find Qt6 components needed by the QML6 plugin
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml OpenGL)

find_package(PkgConfig REQUIRED)

# Get the raw pkg-config output
execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --libs --static gstreamer-full-1.0
    OUTPUT_VARIABLE GSTREAMER_RAW_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Remove problematic GIO module library directory and -l flags (we'll add the .a files directly)
string(REGEX REPLACE "-L[^ ]*lib/gio/modules" "" GSTREAMER_RAW_LIBS "${GSTREAMER_RAW_LIBS}")
string(REGEX REPLACE "-lgioopenssl" "" GSTREAMER_RAW_LIBS "${GSTREAMER_RAW_LIBS}")
string(REGEX REPLACE "-lgioenvironmentproxy" "" GSTREAMER_RAW_LIBS "${GSTREAMER_RAW_LIBS}")

# Get other pkg-config info
pkg_check_modules(GSTREAMER REQUIRED gstreamer-full-1.0>=1.20)

add_executable(gst_test main.cpp)
target_include_directories(gst_test PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_directories(gst_test PRIVATE ${GSTREAMER_LIBRARY_DIRS} /opt/homebrew/lib)

# Parse libraries and separate frameworks from regular libraries
separate_arguments(GSTREAMER_LIBS_LIST UNIX_COMMAND "${GSTREAMER_RAW_LIBS}")

set(GSTREAMER_REGULAR_LIBS "")
set(GSTREAMER_FRAMEWORKS "")
set(MACOS_FRAMEWORKS "Cocoa;Foundation;CoreFoundation;AppKit;CoreAudio;CoreVideo;CoreMedia;AVFoundation;AudioToolbox;VideoToolbox;Security;SystemConfiguration;IOKit;QuartzCore;OpenGL;Carbon;AudioUnit;CoreServices;CoreImage;IOSurface;ApplicationServices")
set(NEXT_IS_FRAMEWORK FALSE)

foreach(LIB_ITEM ${GSTREAMER_LIBS_LIST})
    if(NEXT_IS_FRAMEWORK)
        # This item is a framework name following "-framework"
        list(APPEND GSTREAMER_FRAMEWORKS "${LIB_ITEM}")
        set(NEXT_IS_FRAMEWORK FALSE)
    elseif(LIB_ITEM STREQUAL "-framework")
        # This is the -framework flag, mark next item as framework name
        set(NEXT_IS_FRAMEWORK TRUE)
    elseif(LIB_ITEM IN_LIST MACOS_FRAMEWORKS)
        # This is a bare framework name, add to frameworks list
        list(APPEND GSTREAMER_FRAMEWORKS "${LIB_ITEM}")
    else()
        # Regular library or flag
        list(APPEND GSTREAMER_REGULAR_LIBS "${LIB_ITEM}")
    endif()
endforeach()

# Add frameworks using LINKER: prefix to pass them directly to the linker
foreach(FRAMEWORK ${GSTREAMER_FRAMEWORKS})
    target_link_options(gst_test PRIVATE "LINKER:-framework,${FRAMEWORK}")
endforeach()

# Add regular libraries and link Qt6 and GStreamer components
target_link_libraries(gst_test PRIVATE
    ${GSTREAMER_REGULAR_LIBS}
    # Link against the static GIO modules (needed by static GStreamer build)
    /Users/dan/code/gstreamer-build/install/lib/gio/modules/libgioopenssl.a
    /Users/dan/code/gstreamer-build/install/lib/gio/modules/libgioenvironmentproxy.a
    # Link against the QML6 plugin (now that Qt6 static build is available)
    /Users/dan/code/gstreamer-build/install/lib/gstreamer-1.0/libgstqml6.a
    # Link Qt6 static libraries
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::OpenGL
)

target_compile_options(gst_test PRIVATE ${GSTREAMER_CFLAGS_OTHER})
