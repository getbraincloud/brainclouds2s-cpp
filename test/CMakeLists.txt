cmake_minimum_required(VERSION 3.2.0)

project(testbcs2s)

set(CMAKE_CXX_STANDARD 11)

include_directories(./src)

file(GLOB src_files ./src/*.*)
add_executable(testbcs2s ${src_files} src/tests.h)

target_link_libraries(testbcs2s brainclouds2s)

target_compile_features(testbcs2s PUBLIC c_std_99)