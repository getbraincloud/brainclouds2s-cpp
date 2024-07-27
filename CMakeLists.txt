cmake_minimum_required(VERSION 3.30)

project(brainCloudS2S)

# Setup modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

#=============================================================================
# CMake options
#=============================================================================
option(BUILD_TESTS "brainCloud Unit Tests" OFF)
option(USE_CURL_WIN "Force use libCurl on Win32" OFF)

if(DEFINED SSL_ALLOW_SELFSIGNED)
    set(SSL_ALLOW_SELFSIGNED 1)
else()
    set(SSL_ALLOW_SELFSIGNED 0)
endif()

#=============================================================================
# Definitions
#=============================================================================

if (SSL_ALLOW_SELFSIGNED)
    message("brainCloudS2S Skipping certificate verification in RTT")
    add_definitions(-DBC_SSL_ALLOW_SELFSIGNED)
    else()
    message("brainCloudS2S Building with certificates for RTT")
endif()

set(USE_LIBWEBSOCKETS 1)

if(BC_USE_OPENSSL)
    message("brainCloudS2S Building with OpenSSL TLS")
    add_definitions(-DBC_MBEDTLS_OFF)
else()
    message("brainCloudS2S Building with MbedTLS TLS")
endif()

#=============================================================================
# Compiler flags
#=============================================================================
# We want to use C++11 flag on the compiler
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

#=============================================================================
# BraincloudS2S client
#=============================================================================
set (BC_DIR "${PROJECT_SOURCE_DIR}")
set (OS_SPECIFIC_SRCS "")
set (OS_SPECIFIC_INCS "")

file(GLOB_RECURSE JSONCPP_INCS_FILES "${BC_DIR}/lib/jsoncpp-1.0.0/*.h")
file(GLOB_RECURSE JSONCPP_SRCS_FILES "${BC_DIR}/lib/jsoncpp-1.0.0/*.cpp")

# Useful flags
if (UNIX AND NOT APPLE AND NOT ANDROID)
    set(LINUX 1)
endif()

# Sockets
if (USE_LIBWEBSOCKETS)
    list(APPEND OS_SPECIFIC_INCS
            "include/DefaultWebSocket.h")
    list(APPEND OS_SPECIFIC_SRCS "src/DefaultWebSocket.cpp")
endif()
if (UNIX)
	add_definitions(-DUSE_TCP)
    list(APPEND OS_SPECIFIC_INCS
            "include/DefaultTCPSocket.h")
    list(APPEND OS_SPECIFIC_SRCS "src/DefaultTCPSocket.cpp")
endif()

list(APPEND includes PUBLIC "include")
list(APPEND includes PUBLIC "lib/jsoncpp-1.0.0")

#=============================================================================
# Libraries
#=============================================================================
find_package(CURL CONFIG REQUIRED)
find_package(libwebsockets CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)
# android uses openssl instead of tls (pre-built libwebsockets.a is linked above)
if (USE_LIBWEBSOCKETS)
    # TLS
    option(ENABLE_TESTING "" OFF)
    option(ENABLE_PROGRAMS "" OFF)
	if (BC_USE_OPENSSL)

		set(OPENSSL_USE_STATIC_LIBS TRUE)
		find_package(OpenSSL REQUIRED)
		set(OPENSSL_FOUND 1)
		set(BC_OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")
		set(OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")
		include_directories("${BC_OPENSSL_INCLUDE_DIRS}")
		option(LWS_SSL_CLIENT_USE_OS_CA_CERTS "SSL support should make use of the OS-installed CA root certs" ON)

        option(LWS_WITH_SSL "Include SSL support (defaults to OpenSSL)" ON)

	else()
		# TLS
		option(ENABLE_TESTING "" OFF)
		option(ENABLE_PROGRAMS "" OFF)

        # mbedtls-2.28 (SHA1 is 1963d67e)
        set(BC_MBEDTLS_DIR lib/mbedtls)

		add_subdirectory("${BC_MBEDTLS_DIR}")

		list(APPEND libs PUBLIC mbedtls mbedx509 mbedcrypto)
		list(APPEND includes
				PUBLIC ${BC_MBEDTLS_DIR}/include/)
		option(LWS_WITH_MBEDTLS "" ON)
		set(LWS_MBEDTLS_INCLUDE_DIRS "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${BC_MBEDTLS_DIR}/include>")
        if(WIN32)
            set(LWS_MBEDTLS_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/${BC_MBEDTLS_DIR}/library/${CMAKE_BUILD_TYPE}/mbedtls.lib;${CMAKE_CURRENT_BINARY_DIR}/${BC_MBEDTLS_DIR}/library/${CMAKE_BUILD_TYPE}/mbedcrypto.lib;${CMAKE_CURRENT_BINARY_DIR}/${BC_MBEDTLS_DIR}/library/${CMAKE_BUILD_TYPE}/mbedx509.lib")
        elseif(XCODE)
            set(LWS_MBEDTLS_LIBRARIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/${BC_MBEDTLS_DIR}/library/$CONFIGURATION$EFFECTIVE_PLATFORM_NAME/libmbedtls.a;$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/${BC_MBEDTLS_DIR}/library/$CONFIGURATION$EFFECTIVE_PLATFORM_NAME/libmbedcrypto.a;$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/${BC_MBEDTLS_DIR}/library/$CONFIGURATION$EFFECTIVE_PLATFORM_NAME/libmbedx509.a")
        else()
            set(LWS_MBEDTLS_LIBRARIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/${BC_MBEDTLS_DIR}/library/libmbedtls.a;$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/${BC_MBEDTLS_DIR}/library/libmbedcrypto.a;$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/${BC_MBEDTLS_DIR}/library/libmbedx509.a")
        endif()
	endif()

    # LibWebSockets
    option(LWS_WITHOUT_SERVER
            "Don't build the server part of the library" ON)
    option(LWS_WITHOUT_TESTAPPS
            "Don't build the libwebsocket-test-apps" ON)
    option(LWS_WITHOUT_TEST_SERVER
            "Don't build the test server" ON)
    option(LWS_WITHOUT_TEST_SERVER_EXTPOLL
            "Don't build the test server version that uses external poll" ON)
    option(LWS_WITHOUT_TEST_PING
            "Don't build the ping test application" ON)
    option(LWS_WITHOUT_TEST_CLIENT
            "Don't build the client test application" ON)
    option(LWS_WITH_SHARED
            "Build the shared version of the library" OFF)
    option(LWS_WITHOUT_EXTENSIONS "" ON)
    option(LWS_WITH_TLS "" ON)

    #add_subdirectory("${BC_LWS_DIR}")
    list(APPEND includes
            PUBLIC
            ${BC_LWS_INC}
            )

endif()

add_library(brainCloudS2S STATIC
        # public headers
        include/brainclouds2s.h
        include/brainclouds2s-rtt.h
        include/BrainCloudTypes.h
        include/IRTTCallback.h
        include/IRTTConnectCallback.h
        include/IServerCallback.h
        include/ISocket.h
        include/ITCPSocket.h
        include/IWebSocket.h
        include/OperationParam.h
        include/RTTComms.h
        include/ServiceName.h
        include/ServiceOperation.h
        include/TimeUtil.h
		
		src/brainclouds2s.cpp
        src/brainclouds2s-rtt.cpp
        src/RTTComms.cpp
        src/ServiceName.cpp
        src/ServiceOperation.cpp
        src/TimeUtil.cpp
        
        ${OS_SPECIFIC_INCS}
        ${OS_SPECIFIC_SRCS}
        ${JSONCPP_INCS_FILES}
        ${JSONCPP_SRCS_FILES}
        )

# Setup includes and links libraries
target_include_directories(brainCloudS2S PUBLIC "./include" ${includes})
target_include_directories(brainCloudS2S PRIVATE "./src")
target_link_libraries(brainCloudS2S PUBLIC ${libs} CURL::libcurl)

# Find pthreads based on the platform
if (WIN32)
    find_package(PThreads4W CONFIG REQUIRED)
    target_link_libraries(brainCloudS2S PRIVATE PThreads4W::PThreads4W)
	target_link_libraries(brainCloudS2S PRIVATE websockets_shared)
	
	# Compile flags
	target_compile_options(brainCloudS2S PRIVATE
            /MP     # Multi-core compilation
            )
elseif(UNIX)
	find_package(libwebsockets CONFIG REQUIRED)
	target_link_libraries(brainCloudS2S PRIVATE websockets)
	
	find_package(Threads REQUIRED)
    target_link_libraries(brainCloudS2S PUBLIC uv dl rt ${CMAKE_THREAD_LIBS_INIT})
endif()


#=============================================================================
# Unit Tests
#=============================================================================
if (DEFINED(${BUILD_TESTS}) OR ${BUILD_TESTS})
    message("brainCloudS2S Building with BUILD_TESTS ON")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tests")
endif()