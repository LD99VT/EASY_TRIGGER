cmake_minimum_required(VERSION 3.22)

# ── Version ────────────────────────────────────────────────────────────────────
# Single source of truth for the app version:
# - installer_trigger.iss and Inno Setup read it via version.iss
# - CMake reads the same version.iss here and passes it to project().
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.iss" _et_version_line REGEX "AppVersion")
if(NOT _et_version_line)
  message(FATAL_ERROR "version.iss not found or AppVersion not defined")
endif()
string(REGEX REPLACE ".*\"([0-9]+\\.[0-9]+\\.[0-9]+)\".*" "\\1" APP_VERSION "${_et_version_line}")
project(EasyTrigger_JUCE VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate Version.h from Version.h.in, substituting @PROJECT_VERSION_*@ tokens.
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Source/core/Version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
  @ONLY
)

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(EasyTrigger
  PRODUCT_NAME "Easy Trigger"
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Easy Trigger requires audio input access for LTC input."
)
juce_generate_juce_header(EasyTrigger)

# Icons: one PNG → generate .ico (Windows) and .icns (macOS) at build time if ImageMagick is available
set(PNG_ICON_SOURCE "Icon/Icon Trigger.png")
set(ICON_OUTPUT_BASE "Icon Trigger")
set(ICON_BUNDLE_NAME "Icon Trigger")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateIconsFromPng.cmake")

if(USE_GENERATED_ICONS)
  set(ICO_FILENAME "${GENERATED_ICO_FILENAME}")
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AppIcon.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/AppIcon.rc"
    @ONLY
  )
  add_custom_target(icon_gen ALL DEPENDS ${GENERATED_ICO_PATH} ${GENERATED_ICNS_PATH})
  add_dependencies(EasyTrigger icon_gen)
  set(_appicon_rc "${CMAKE_CURRENT_BINARY_DIR}/generated/AppIcon.rc")
else()
  set(_appicon_rc "${CMAKE_CURRENT_SOURCE_DIR}/Source/AppIcon.rc")
endif()

# macOS app bundle icon — copy .icns into Resources and set CFBundleIconFile
if(APPLE)
  set(_trigger_icns "${CMAKE_CURRENT_SOURCE_DIR}/Icon/Icon Trigger.icns")
  target_sources(EasyTrigger PRIVATE "${_trigger_icns}")
  set_source_files_properties("${_trigger_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  set_target_properties(EasyTrigger PROPERTIES MACOSX_BUNDLE_ICON_FILE "${ICON_BUNDLE_NAME}")
  add_custom_command(TARGET EasyTrigger POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DBUNDLE_CONTENT=$<TARGET_BUNDLE_CONTENT_DIR:EasyTrigger>"
      "-DICNS_SRC=${_trigger_icns}"
      "-DICON_NAME=${ICON_BUNDLE_NAME}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch_macos_icon.cmake"
    COMMENT "Patching macOS bundle icon"
    VERBATIM
  )
  add_custom_command(TARGET EasyTrigger POST_BUILD
    COMMAND /bin/rm -rf
      "$<TARGET_BUNDLE_CONTENT_DIR:EasyTrigger>/Icon"
    COMMAND /bin/rm -rf
      "$<TARGET_BUNDLE_CONTENT_DIR:EasyTrigger>/Icons"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/Fonts"
      "$<TARGET_BUNDLE_CONTENT_DIR:EasyTrigger>/Fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/Help"
      "$<TARGET_BUNDLE_CONTENT_DIR:EasyTrigger>/Help"
    COMMENT "Copying runtime assets into macOS bundle (Contents/)"
    VERBATIM
  )
endif()

target_sources(EasyTrigger PRIVATE
  Source/Main.cpp
  Source/App.cpp
  Source/App.h
  Source/TriggerMainWindow.cpp
  Source/trigger/TriggerCustomCRUD.cpp
  Source/ui/TriggerMenuHandlers.cpp
  Source/core/TriggerConfigIO.cpp
  Source/TriggerMainWindow.h
  Source/trigger/CustomTriggerState.h
  Source/engine/ResolumeClipCollector.cpp
  Source/engine/ResolumeClipCollector.h
  ${_appicon_rc}
  Source/engine/BridgeEngine.cpp
  Source/engine/BridgeEngine.h
  Source/engine/OscInput.cpp
  Source/engine/OscInput.h
  Source/core/Version.h.in
  Source/core/Timecode.cpp
  Source/core/Timecode.h
  Source/core/ClockState.cpp
  Source/core/ClockState.h
  Source/core/ConfigStore.cpp
  Source/core/ConfigStore.h
  Source/ui/monitoring/OscLog.h
  Source/ui/components/StatusBarComponent.h
  Source/ui/components/SettingsPanelComponent.h
  Source/ui/updates/UpdateChecker.h
  Source/ui/updates/UpdateInstaller.h
  Source/ui/windows/StatusMonitorWindow.h
  Source/ui/windows/UpdatePromptWindow.h
)

target_include_directories(EasyTrigger PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
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
  if(NOT ASIO_SDK_DIR)
    set(_asio_candidates
      "${CMAKE_CURRENT_SOURCE_DIR}/ASIOSDK"
      "${CMAKE_CURRENT_SOURCE_DIR}/asiosdk"
      "${CMAKE_CURRENT_SOURCE_DIR}/third_party/asiosdk"
      "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/asiosdk"
      "C:/ASIOSDK"
      "C:/SDK/ASIOSDK"
      "C:/SDK/asiosdk"
      "$ENV{USERPROFILE}/Downloads/asiosdk"
      "$ENV{USERPROFILE}/Downloads/ASIOSDK"
      "$ENV{USERPROFILE}/Desktop/asiosdk"
      "$ENV{USERPROFILE}/Desktop/ASIOSDK"
    )
    foreach(_cand IN LISTS _asio_candidates)
      if(EXISTS "${_cand}/common/asio.h" AND EXISTS "${_cand}/host/asiodrivers.h")
        set(ASIO_SDK_DIR "${_cand}" CACHE PATH "Path to Steinberg ASIO SDK root" FORCE)
        break()
      endif()
    endforeach()
  endif()
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
  else()
    message(STATUS "ASIO SDK not found for EasyTrigger. Build will not include ASIO/ReaRoute devices.")
  endif()
  target_link_libraries(EasyTrigger PRIVATE ws2_32)
endif()
