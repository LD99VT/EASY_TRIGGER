# patch_macos_icon.cmake — post-build helper: guarantee icon in bundle
# Invoked by CMakeLists.txt add_custom_command POST_BUILD via cmake -P.
# Required -D variables:
#   BUNDLE_CONTENT  — path to <App>.app/Contents
#   ICNS_SRC        — source .icns file
#   ICON_NAME       — value for CFBundleIconFile (no extension)

cmake_minimum_required(VERSION 3.22)

set(_resources "${BUNDLE_CONTENT}/Resources")
set(_plist     "${BUNDLE_CONTENT}/Info.plist")
get_filename_component(_icns_name "${ICNS_SRC}" NAME)

# 1. Ensure Resources dir exists and icon is present
file(MAKE_DIRECTORY "${_resources}")
file(COPY_FILE "${ICNS_SRC}" "${_resources}/${_icns_name}" ONLY_IF_DIFFERENT)

# 2. Set CFBundleIconFile (try Set first; if key missing, Add)
execute_process(
  COMMAND /usr/libexec/PlistBuddy -c "Set :CFBundleIconFile ${ICON_NAME}" "${_plist}"
  RESULT_VARIABLE _r
  OUTPUT_QUIET ERROR_QUIET
)
if(NOT _r EQUAL 0)
  execute_process(
    COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string ${ICON_NAME}" "${_plist}"
    RESULT_VARIABLE _r
  )
  if(NOT _r EQUAL 0)
    message(FATAL_ERROR "patch_macos_icon: cannot set CFBundleIconFile in ${_plist}")
  endif()
endif()

# 3. Touch the bundle so Finder / Spotlight pick up the change
get_filename_component(_bundle "${BUNDLE_CONTENT}" DIRECTORY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${_bundle}")
