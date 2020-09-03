# This will build only on linux systems
cmake_minimum_required(VERSION 3.0.0)

option(BUILD_TEST "Build the test" OFF)

project(brainclouds2s)

# Setup modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# We want to use C++11 flag on the compiler for std::thread
set(CMAKE_CXX_STANDARD 11)

# Application source files
file(GLOB src_files ./src/*.*)

# Relay Server
add_library(brainclouds2s ${src_files})

# CURL
find_package(CURL REQUIRED)
list(APPEND includes PUBLIC ${CURL_INCLUDE_DIRS})
list(APPEND libs PUBLIC ${CURL_LIBRARIES})

# JsonCpp
find_package(JsonCpp REQUIRED)
list(APPEND includes PUBLIC ${JSONCPP_INCLUDE_DIRS})
list(APPEND libs PUBLIC ${JSONCPP_LIBRARY})

# pthreads
find_package(Threads)
list(APPEND libs PUBLIC ${CMAKE_THREAD_LIBS_INIT})

# Setup includes and linked libraries
target_include_directories(brainclouds2s PUBLIC "./src" "./include" ${includes})
target_link_libraries(brainclouds2s PUBLIC ${libs})

if (BUILD_TEST)
    add_subdirectory(test)
endif()
