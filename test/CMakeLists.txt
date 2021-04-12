cmake_minimum_required(VERSION 3.0.0)

project(testbcs2s)

set(CMAKE_CXX_STANDARD 11)

include_directories(./src)

file(GLOB src_files ./src/*.*)
add_executable(testbcs2s ${src_files})

target_link_libraries(testbcs2s brainclouds2s)
