cmake_minimum_required(VERSION 3.30)

project(testbcs2s)

set(CMAKE_CXX_STANDARD 11)

# Find required packages (CURL and PThreads4W)
find_package(CURL CONFIG REQUIRED)
find_package(PThreads4W CONFIG REQUIRED)

include_directories("${BC_DIR}/lib/jsoncpp-1.0.0")
include_directories("src")

# Add the test source files
file(GLOB_RECURSE INCS "src/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE BC_INCS "${BC_DIR}/include/*.h")

source_group(bcs2s FILES ${BC_INCS})
source_group(headers FILES ${INCS})
source_group(src FILES ${SOURCES})

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_executable(testbcs2s ${BC_INCS} ${INCS} ${SOURCES})

# Link against brainCloudS2S and its dependencies
target_link_libraries(testbcs2s PUBLIC brainCloudS2S CURL::libcurl PThreads4W::PThreads4W mbedtls mbedx509 mbedcrypto)

