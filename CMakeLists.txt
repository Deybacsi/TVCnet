# SPDX-License-Identifier: GPL-3.0-or-later
# The shared TVCnet code as a host library: freeTVC pulls it in with FetchContent; on its own it builds the tests.
cmake_minimum_required(VERSION 3.20)
project(tvcnet LANGUAGES CXX)

add_library(tvcnet_common STATIC
    common/src/codes.cpp
    common/src/frame.cpp
    common/src/hayes.cpp
    common/src/line_code.cpp
    common/src/modem.cpp)
target_include_directories(tvcnet_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_compile_features(tvcnet_common PUBLIC cxx_std_17)
set_target_properties(tvcnet_common PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_executable(tvcnet_tests tests/common_test.cpp)
    target_link_libraries(tvcnet_tests PRIVATE tvcnet_common)
    if(NOT MSVC)
        target_compile_options(tvcnet_common PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(tvcnet_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME common COMMAND tvcnet_tests)
endif()
