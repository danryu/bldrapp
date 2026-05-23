cmake_minimum_required(VERSION 3.22)
project(juce_asio_mingw_probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ASIO_SDK_COMMON)
  message(FATAL_ERROR "ASIO_SDK_COMMON must point at the Steinberg SDK common/ directory")
endif()

add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)

add_executable(asio_probe main.cpp)
target_compile_definitions(asio_probe
  PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1
    JUCE_ASIO=1
    JUCE_ASIO_USE_EXTERNAL_SDK=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_include_directories(asio_probe PRIVATE "${ASIO_SDK_COMMON}")
target_link_libraries(asio_probe
  PRIVATE
    juce::juce_audio_devices
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
