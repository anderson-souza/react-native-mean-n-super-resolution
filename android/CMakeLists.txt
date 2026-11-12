cmake_minimum_required(VERSION 3.13)
project(MeanNSuperResolution)

add_library(
    react-native-mean-n-super-resolution    STATIC
    ../cpp/MeanNSuperResolutionImpl.cpp
)

set_target_properties(
    react-native-mean-n-super-resolution PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(
    react-native-mean-n-super-resolution    PUBLIC
    ../cpp
)

target_link_libraries(
    react-native-mean-n-super-resolution    jsi
    reactnative
    react_codegen_MeanNSuperResolutionSpec
)
