# GenerateIconsFromPng.cmake
# From a single PNG, generate .ico (Windows) and .icns (macOS) at build time.
#
# Usage:
#   set(PNG_ICON_SOURCE "Icon/app_icon.png")   # path relative to project root
#   set(ICON_OUTPUT_BASE "Icon Trigger")      # base name for .ico/.icns (no path)
#   set(ICON_BUNDLE_NAME "Icon Trigger")      # CFBundleIconFile value on macOS (no extension)
#   include(cmake/GenerateIconsFromPng.cmake)
#
# If PNG_ICON_SOURCE exists and ImageMagick is found:
#   - GENERATED_ICO_PATH, GENERATED_ICNS_PATH (on Apple) are set.
#   - Icons are built into CMAKE_CURRENT_BINARY_DIR/generated/.
#   - USE_GENERATED_ICONS is set to TRUE.
# Otherwise USE_GENERATED_ICONS is FALSE; keep using existing .ico/.icns in source tree.

set(_png_abs "${CMAKE_CURRENT_SOURCE_DIR}/${PNG_ICON_SOURCE}")
set(USE_GENERATED_ICONS FALSE)

if(NOT EXISTS "${_png_abs}")
  return()
endif()

# ImageMagick: prefer "magick" (v7), then "convert" (v6)
find_program(_img_convert NAMES magick convert convert.exe
  DOC "ImageMagick convert for icon generation")
if(NOT _img_convert)
  message(STATUS "Icon: ${PNG_ICON_SOURCE} found but ImageMagick not found; using existing .ico/.icns")
  return()
endif()

# Sanitize base name for files (no spaces in generated filenames for simplicity)
string(REPLACE " " "_" _ico_stem "${ICON_OUTPUT_BASE}")
set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_ico_out "${_gen_dir}/${_ico_stem}.ico")

set(GENERATED_ICO_PATH "")
# ICO: for Windows only (used by AppIcon.rc)
if(WIN32)
  set(GENERATED_ICO_PATH "${_ico_out}")
  add_custom_command(OUTPUT "${_ico_out}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_gen_dir}"
    COMMAND "${_img_convert}" "${_png_abs}"
      -define icon:auto-resize=256,128,64,48,32,16
      "${_ico_out}"
    MAIN_DEPENDENCY "${_png_abs}"
    COMMENT "Generating .ico from ${PNG_ICON_SOURCE}"
    VERBATIM
  )
endif()

# ICNS: only on macOS (iconutil is macOS-only)
set(GENERATED_ICNS_PATH "")
if(APPLE)
  set(_icns_out "${_gen_dir}/${_ico_stem}.icns")
  set(GENERATED_ICNS_PATH "${_icns_out}")
  set(_iconset "${_gen_dir}/${_ico_stem}.iconset")
  set(_deplist "")
  foreach(_s 16 32 128 256 512)
    math(EXPR _s2 "${_s} * 2")
    set(_p1 "${_iconset}/icon_${_s}x${_s}.png")
    set(_p2 "${_iconset}/icon_${_s}x${_s}@2x.png")
    list(APPEND _deplist "${_p1}" "${_p2}")
    add_custom_command(OUTPUT "${_p1}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_iconset}"
      COMMAND "${_img_convert}" "${_png_abs}" -resize "${_s}x${_s}" "${_p1}"
      MAIN_DEPENDENCY "${_png_abs}"
      COMMENT "ICNS iconset ${_p1}"
      VERBATIM
    )
    add_custom_command(OUTPUT "${_p2}"
      COMMAND "${_img_convert}" "${_png_abs}" -resize "${_s2}x${_s2}" "${_p2}"
      MAIN_DEPENDENCY "${_png_abs}"
      COMMENT "ICNS iconset ${_p2}"
      VERBATIM
    )
  endforeach()
  add_custom_command(OUTPUT "${_icns_out}"
    COMMAND iconutil --convert icns --output "${_icns_out}" "${_iconset}"
    DEPENDS ${_deplist}
    MAIN_DEPENDENCY "${_png_abs}"
    COMMENT "Generating .icns from ${PNG_ICON_SOURCE}"
    VERBATIM
  )
endif()

set(USE_GENERATED_ICONS TRUE)
set(GENERATED_ICO_FILENAME "${_ico_stem}.ico")
message(STATUS "Icons will be generated from ${PNG_ICON_SOURCE} (ico + icns on macOS)")
