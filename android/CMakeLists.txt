cmake_minimum_required(VERSION 3.19)
project(MeanNSuperResolution)

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
