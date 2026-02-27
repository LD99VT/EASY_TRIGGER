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
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Easy Trigger requires audio input access for LTC input."
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
  set(ASIO_SDK_DIR "$ENV{ASIO_SDK_DIR}" CACHE PATH "Path to Steinberg ASIO SDK root")
  if(ASIO_SDK_DIR)
    if(EXISTS "${ASIO_SDK_DIR}/common/asio.h" AND EXISTS "${ASIO_SDK_DIR}/host/asiodrivers.h")
      target_compile_definitions(EasyTrigger PRIVATE JUCE_ASIO=1)
      target_include_directories(EasyTrigger PRIVATE
        "${ASIO_SDK_DIR}/common"
        "${ASIO_SDK_DIR}/host"
      )
      message(STATUS "ASIO enabled for EasyTrigger from: ${ASIO_SDK_DIR}")
    else()
      message(WARNING "ASIO_SDK_DIR is set but required headers were not found in common/ and host/. ASIO disabled.")
    endif()
  endif()
  target_link_libraries(EasyTrigger PRIVATE ws2_32)
endif()
