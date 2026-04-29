# generated from ament/cmake/core/templates/nameConfig.cmake.in

# prevent multiple inclusion
if(_franbro_CONFIG_INCLUDED)
  # ensure to keep the found flag the same
  if(NOT DEFINED franbro_FOUND)
    # explicitly set it to FALSE, otherwise CMake will set it to TRUE
    set(franbro_FOUND FALSE)
  elseif(NOT franbro_FOUND)
    # use separate condition to avoid uninitialized variable warning
    set(franbro_FOUND FALSE)
  endif()
  return()
endif()
set(_franbro_CONFIG_INCLUDED TRUE)

# output package information
if(NOT franbro_FIND_QUIETLY)
  message(STATUS "Found franbro: 0.1.0 (${franbro_DIR})")
endif()

# warn when using a deprecated package
if(NOT "" STREQUAL "")
  set(_msg "Package 'franbro' is deprecated")
  # append custom deprecation text if available
  if(NOT "" STREQUAL "TRUE")
    set(_msg "${_msg} ()")
  endif()
  # optionally quiet the deprecation message
  if(NOT franbro_DEPRECATED_QUIET)
    message(DEPRECATION "${_msg}")
  endif()
endif()

# flag package as ament-based to distinguish it after being find_package()-ed
set(franbro_FOUND_AMENT_PACKAGE TRUE)

# include all config extra files
set(_extras "")
foreach(_extra ${_extras})
  include("${franbro_DIR}/${_extra}")
endforeach()
