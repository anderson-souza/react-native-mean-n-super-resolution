# hazard: OPENCV_MOBILE_DIR (env) must point at an already-extracted package
# root for the platform being configured; it bypasses download and hash checks.

cmake_minimum_required(VERSION 3.19)

set(_OPENCV_MOBILE_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(_OPENCV_MOBILE_REPO_ROOT "${_OPENCV_MOBILE_REPO_ROOT}" ABSOLUTE)

function(_opencv_mobile_detect_platform out_var)
  if(ANDROID)
    set(${out_var} "android" PARENT_SCOPE)
  elseif(WIN32)
    set(${out_var} "windows" PARENT_SCOPE)
  elseif(APPLE)
    set(${out_var} "macos" PARENT_SCOPE)
  else()
    set(${out_var} "linux" PARENT_SCOPE)
  endif()
endfunction()

function(opencv_mobile_fetch platform out_root)
  if(DEFINED ENV{OPENCV_MOBILE_DIR} AND NOT "$ENV{OPENCV_MOBILE_DIR}" STREQUAL "")
    message(STATUS "opencv-mobile: using OPENCV_MOBILE_DIR=$ENV{OPENCV_MOBILE_DIR} (download skipped)")
    set(${out_root} "$ENV{OPENCV_MOBILE_DIR}" PARENT_SCOPE)
    return()
  endif()

  set(json_path "${_OPENCV_MOBILE_REPO_ROOT}/opencv.json")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${json_path}")
  file(READ "${json_path}" json)
  string(JSON version GET "${json}" version)
  string(JSON url GET "${json}" packages ${platform} url)
  string(JSON sha256 GET "${json}" packages ${platform} sha256)

  set(base "${_OPENCV_MOBILE_REPO_ROOT}/.opencv/${version}")
  set(dest "${base}/${platform}")
  set(stamp "${dest}/.sha256")

  # invariant: the stamp holds the hash of the archive the directory was
  # extracted from, so a changed opencv.json hash forces a fresh download.
  set(cached_sha "")
  if(EXISTS "${stamp}")
    file(READ "${stamp}" cached_sha)
  endif()

  if(NOT cached_sha STREQUAL sha256)
    file(MAKE_DIRECTORY "${base}")
    # why: Android configures all ABIs in parallel against the same cache dir.
    file(LOCK "${base}/${platform}.lock" GUARD FUNCTION TIMEOUT 1800)

    if(EXISTS "${stamp}")
      file(READ "${stamp}" cached_sha)
    endif()
    if(NOT cached_sha STREQUAL sha256)
      set(archive "${base}/${platform}.zip")
      message(STATUS "opencv-mobile: downloading ${url}")
      file(DOWNLOAD "${url}" "${archive}"
        EXPECTED_HASH SHA256=${sha256}
        TLS_VERIFY ON
        STATUS dl_status)
      list(GET dl_status 0 dl_code)
      if(NOT dl_code EQUAL 0)
        file(REMOVE "${archive}")
        message(FATAL_ERROR "opencv-mobile: download failed: ${dl_status}")
      endif()

      set(tmp "${dest}.tmp")
      file(REMOVE_RECURSE "${tmp}" "${dest}")
      file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${tmp}")
      file(RENAME "${tmp}" "${dest}")
      file(WRITE "${stamp}" "${sha256}")
      file(REMOVE "${archive}")
      message(STATUS "opencv-mobile: extracted to ${dest}")
    else()
      message(STATUS "opencv-mobile: cache hit ${dest} (download skipped)")
    endif()
  else()
    message(STATUS "opencv-mobile: cache hit ${dest} (download skipped)")
  endif()

  set(${out_root} "${dest}" PARENT_SCOPE)
endfunction()

# why: a macro, so find_package's OpenCV_* variables land in the caller's scope.
macro(opencv_mobile_find_package)
  if(${ARGC} GREATER 0)
    set(_ocvm_platform "${ARGV0}")
  else()
    _opencv_mobile_detect_platform(_ocvm_platform)
  endif()
  opencv_mobile_fetch(${_ocvm_platform} _ocvm_root)

  file(READ "${_OPENCV_MOBILE_REPO_ROOT}/opencv.json" _ocvm_json)
  string(JSON _ocvm_version GET "${_ocvm_json}" version)

  if(_ocvm_platform STREQUAL "android")
    set(_ocvm_config_dir "${_ocvm_root}/opencv-mobile-${_ocvm_version}-android/sdk/native/jni")
  elseif(_ocvm_platform STREQUAL "windows")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
      set(_ocvm_arch "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(_ocvm_arch "x86")
    else()
      set(_ocvm_arch "x64")
    endif()
    # why: the pack dispatcher guesses runtime vc14 + shared libs; only vc17 static libs ship.
    set(_ocvm_config_dir "${_ocvm_root}/opencv-mobile-${_ocvm_version}-windows-vs2022/${_ocvm_arch}/${_ocvm_arch}/vc17/staticlib")
  elseif(_ocvm_platform STREQUAL "linux")
    set(_ocvm_config_dir "${_ocvm_root}/opencv-mobile-${_ocvm_version}-ubuntu-2404/lib/cmake/opencv4")
  elseif(_ocvm_platform STREQUAL "macos")
    set(_ocvm_config_dir "")
  else()
    message(FATAL_ERROR "opencv-mobile: unsupported platform '${_ocvm_platform}'")
  endif()

  if(_ocvm_platform STREQUAL "macos")
    # why: the macOS package ships only opencv2.framework, no OpenCVConfig.cmake.
    if(NOT TARGET opencv_mobile_framework)
      add_library(opencv_mobile_framework INTERFACE IMPORTED)
      set_target_properties(opencv_mobile_framework PROPERTIES
        INTERFACE_COMPILE_OPTIONS "-F${_ocvm_root}"
        INTERFACE_LINK_OPTIONS "-F${_ocvm_root};-framework;opencv2;-framework;Accelerate")
    endif()
    set(OpenCV_FOUND TRUE)
    set(OpenCV_LIBS opencv_mobile_framework)
  else()
    # invariant: OpenCV_DIR always mirrors opencv.json; a stale cached value would win over PATHS.
    set(OpenCV_DIR "${_ocvm_config_dir}" CACHE PATH "opencv-mobile config dir" FORCE)
    find_package(OpenCV REQUIRED
      PATHS "${_ocvm_config_dir}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
  endif()
  message(STATUS "opencv-mobile: ${_ocvm_platform} OpenCV ${_ocvm_version} libs=${OpenCV_LIBS}")
endmacro()
