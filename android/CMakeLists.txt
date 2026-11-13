cmake_minimum_required(VERSION 3.19)
project(MeanNSuperResolution)

# invariant: opencv-mobile v38 static libs reference libc++/OpenMP symbols that
# only NDK r30+ provides (r27/r29 fail at link with undefined __hash_memory);
# fail at configure with an actionable message instead.
set(SR_MIN_NDK_MAJOR 30)
set(_sr_ndk_dir "${CMAKE_ANDROID_NDK}")
if(NOT _sr_ndk_dir)
  set(_sr_ndk_dir "${ANDROID_NDK}")
endif()
file(STRINGS "${_sr_ndk_dir}/source.properties" _sr_ndk_rev REGEX "^Pkg[.]Revision")
string(REGEX MATCH "= *([0-9]+)" _sr_ndk_match "${_sr_ndk_rev}")
if(NOT CMAKE_MATCH_1 OR CMAKE_MATCH_1 LESS SR_MIN_NDK_MAJOR)
  message(FATAL_ERROR
    "react-native-mean-n-super-resolution requires Android NDK r${SR_MIN_NDK_MAJOR}+ "
    "(found '${_sr_ndk_rev}' in ${_sr_ndk_dir}). Set ndkVersion = \"30.0.14904198\" "
    "(or newer) in your app's android/build.gradle ext block.")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/OpenCVMobile.cmake)
opencv_mobile_find_package(android)

add_library(
    react-native-mean-n-super-resolution    STATIC
    ../cpp/MeanNSuperResolutionImpl.cpp
    ../cpp/sr/Align.cpp
    ../cpp/sr/ErrorMapping.cpp
    ../cpp/sr/Paths.cpp
    ../cpp/sr/Pipeline.cpp
    ../cpp/sr/Validate.cpp
    src/TempDir.cpp
)

set_target_properties(
    react-native-mean-n-super-resolution PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# invariant: the worker's catch-all and SrError matching need exceptions + RTTI,
# even though the linked opencv-mobile itself is built without them.
target_compile_options(
    react-native-mean-n-super-resolution    PRIVATE
    -fexceptions
    -frtti
)

target_include_directories(
    react-native-mean-n-super-resolution    PUBLIC
    ../cpp
)

target_link_libraries(
    react-native-mean-n-super-resolution    jsi
    reactnative
    react_codegen_MeanNSuperResolutionSpec
    fbjni
    ${OpenCV_LIBS}
    log
)
