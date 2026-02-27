cmake_minimum_required(VERSION 3.22)

project(MTC_Bridge_JUCE VERSION 2.4.13 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(MTC_Bridge
  PRODUCT_NAME "Easy Bridge v2"
)
juce_generate_juce_header(MTC_Bridge)

target_sources(MTC_Bridge PRIVATE
  Source/Main.cpp
  Source/App.cpp
  Source/App.h
  Source/MainWindow.cpp
  Source/MainWindow.h
  Source/AppIcon.rc
  Source/engine/BridgeEngine.cpp
  Source/engine/BridgeEngine.h
  Source/engine/OscInput.cpp
  Source/engine/OscInput.h
  Source/core/BridgeVersion.h
  Source/core/Timecode.cpp
  Source/core/Timecode.h
  Source/core/ClockState.cpp
  Source/core/ClockState.h
  Source/core/ConfigStore.cpp
  Source/core/ConfigStore.h
)

target_compile_definitions(MTC_Bridge PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

target_link_libraries(MTC_Bridge PRIVATE
  juce::juce_gui_extra
  juce::juce_audio_utils
  juce::juce_osc
)

target_include_directories(MTC_Bridge PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SuperTimecodeConverter
)

if(WIN32)
  target_link_libraries(MTC_Bridge PRIVATE ws2_32)
endif()
