cmake_minimum_required(VERSION 3.30)

project(testbcs2s)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)

# Find required packages
find_package(CURL CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)

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
target_link_libraries(testbcs2s PRIVATE brainCloudS2S CURL::libcurl mbedtls mbedx509 mbedcrypto)
if (WIN32)
    find_package(PThreads4W CONFIG REQUIRED)
    target_link_libraries(testbcs2s PRIVATE PThreads4W::PThreads4W)
elseif(UNIX)
	# Compile flags
	#target_compile_options(testbcs2s PRIVATE -luv -ldl -lrt -lpthread)
    find_package(Threads REQUIRED)
    target_link_libraries(brainCloudS2S PUBLIC libuv::uv dl rt ${CMAKE_THREAD_LIBS_INIT})
endif()

