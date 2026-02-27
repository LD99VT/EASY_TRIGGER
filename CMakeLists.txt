cmake_minimum_required(VERSION 3.22)

project(EasyTrigger_JUCE VERSION 2.4.13 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(EasyTrigger
  PRODUCT_NAME "Easy Trigger v2"
)
juce_generate_juce_header(EasyTrigger)

target_sources(EasyTrigger PRIVATE
  Source/Main.cpp
  Source/App.cpp
  Source/App.h
  Source/TriggerMainWindow.cpp
  Source/TriggerMainWindow.h
  Source/engine/ResolumeClipCollector.cpp
  Source/engine/ResolumeClipCollector.h
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

target_compile_definitions(EasyTrigger PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

target_link_libraries(EasyTrigger PRIVATE
  juce::juce_gui_extra
  juce::juce_audio_utils
  juce::juce_osc
)

if(WIN32)
  target_link_libraries(EasyTrigger PRIVATE ws2_32)
endif()
