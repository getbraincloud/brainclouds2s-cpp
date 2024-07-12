# This will build only on linux systems
cmake_minimum_required(VERSION 3.1.0)

option(BUILD_TEST "Build the test" OFF)

project(brainclouds2s)

# Setup modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# We want to use C++11 flag on the compiler for std::thread
set(CMAKE_CXX_STANDARD 11)

# TODO: USE_LIBWEBSOCKETS to include or not include some src files in the build
# Application source files
file(GLOB src_files ./src/*.*)

file(GLOB_RECURSE JSONCPP_INCS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/lib/jsoncpp-1.0.0/*.h")
file(GLOB_RECURSE JSONCPP_SRCS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/lib/jsoncpp-1.0.0/*.cpp")

# Relay Server
add_library(brainclouds2s ${src_files}
        ${JSONCPP_INCS_FILES}
        ${JSONCPP_SRCS_FILES})

# CURL
find_package(CURL REQUIRED)
list(APPEND includes PUBLIC ${CURL_INCLUDE_DIRS})
list(APPEND libs PUBLIC ${CURL_LIBRARIES})

# JsonCpp
list(APPEND includes PUBLIC "lib/jsoncpp-1.0.0")

# pthreads
find_package(Threads)
list(APPEND libs PUBLIC ${CMAKE_THREAD_LIBS_INIT})

# websockets
if(DEFINED SSL_ALLOW_SELFSIGNED)
    set(SSL_ALLOW_SELFSIGNED 1)
else()
    set(SSL_ALLOW_SELFSIGNED 0)
endif()

set(BC_USE_OPENSSL 0)
set(USE_LIBWEBSOCKETS 1)

if(DEFINED USE_LIBWEBSOCKETS AND USE_LIBWEBSOCKETS)
    set(USE_LIBWEBSOCKETS 1)
    message("brainCloud using websockets for RTT comm")
else()
    set(USE_LIBWEBSOCKETS 0)
    add_definitions(-DLIBWEBSOCKETS_OFF)
endif()

# else

if (SSL_ALLOW_SELFSIGNED)
    message("brainCloud Skipping certificate verification in RTT")
    add_definitions(-DBC_SSL_ALLOW_SELFSIGNED)
else()
    message("brainCloud Building with certificates for RTT")
endif()

if(BC_USE_OPENSSL)
    message("brainCloud Building with OpenSSL TLS")
    add_definitions(-DBC_MBEDTLS_OFF)
else()
    message("brainCloud Building with MbedTLS TLS")
endif()

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
    if(WIN32)
        # v3.0 (on fork SHA1 is eaa935a8)
        set(BC_LWS_DIR lib/lwsv3/libwebsockets)
        set(BC_LWS_INC "${CMAKE_CURRENT_BINARY_DIR}/${BC_LWS_DIR}/include")
    else()
        # v4.3-stable (SHA1 is 58af7b44)
        set(BC_LWS_DIR lib/libwebsockets)
        set(BC_LWS_INC ${BC_LWS_DIR}/include)
    endif()

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

    add_subdirectory("${BC_LWS_DIR}")

    # turn off 'treat warnings as errors' for libwebsockets in msvc solution (errors on 'size_t' as 'int')
    if (MSVC)
        target_compile_options(websockets PRIVATE /WX-)
        if (NOT BC_USE_OPENSSL)
            target_compile_options(mbedtls PRIVATE /WX-)
            target_compile_options(mbedcrypto PRIVATE /WX-)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(websockets PRIVATE -Wno-unused-but-set-variable -Wno-typedef-redefinition)
        if (NOT BC_USE_OPENSSL)
            target_compile_options(mbedtls PRIVATE -Wno-unused-but-set-variable)
            target_compile_options(mbedcrypto PRIVATE -Wno-unused-but-set-variable)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(websockets PRIVATE -Wno-typedef-redefinition)
    endif()
	

    # append the libwesockets to list of libs
    list(APPEND libs websockets)
    # append mbedtls afterwards for linking without duplicate warning
    if(NOT BC_USE_OPENSSL)
        #list(APPEND libs PUBLIC mbedtls mbedx509 mbedcrypto)
    endif()
    list(APPEND includes
            PUBLIC
            ${BC_LWS_INC}
            )
endif()

# Setup includes and linked libraries
target_include_directories(brainclouds2s PUBLIC "./src" "./include" ${includes})
target_link_libraries(brainclouds2s PUBLIC ${libs})
#target_link_options(brainclouds2s PRIVATE LINKER:-no_warn_duplicate_libraries)

if (BUILD_TEST)
    add_subdirectory(test)
endif()
