cmake_minimum_required(VERSION 3.19)
project(mean_n_super_resolution_native_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/OpenCVMobile.cmake)
opencv_mobile_find_package()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
  URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# why: keeps gtest on the same MSVC runtime as the rest of the test binary.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(SR_DIR ${CMAKE_CURRENT_LIST_DIR}/../sr)
add_library(sr_core STATIC
  ${SR_DIR}/Paths.cpp
  ${SR_DIR}/Validate.cpp
  TempDirHost.cpp
)
target_include_directories(sr_core PUBLIC ${SR_DIR})
target_link_libraries(sr_core PUBLIC ${OpenCV_LIBS})

add_executable(sr_tests
  smoke_test.cpp
  paths_test.cpp
  validate_test.cpp
)
target_link_libraries(sr_tests PRIVATE sr_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(sr_tests)
