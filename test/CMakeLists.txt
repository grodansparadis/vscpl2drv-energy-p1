# CMakeLists.txt
#
# Build instructions for the VSCP daemon.
#
# Copyright (C) 2000-2023 Ake Hedman the VSCP project
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

cmake_minimum_required(VERSION 3.5)
project(test LANGUAGES CXX C)
set(PACKAGE_AUTHOR "Ake Hedman, the VSCP Project")

#add_subdirectory(../third_party/spdlog/)

## --- C++14 build flags ---
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Build locations
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

# Generate position-independent code (-fPIC on UNIX)
#set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- System Libraries ---
include(GNUInstallDirs)

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_VSCP_COMPILE_FLAGS}")
SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_VSCP_LINK_FLAGS}")

if(MSVC)
	  add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DMSVC_LEAN_AND_MEANS)
    add_definitions(-DOPENSSL_API_1_1)
    add_definitions(-D_POSIX_C_SOURCE)
    add_definitions(-DVCPKG_TARGET_TRIPLET=x64-windows)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(LIBS_SYSTEM ws2_32)
    if (MSVC)
        #     # use precompiled headers
        #     set (CMAKE_AUTOMOC_MOC_OPTIONS "-bstdafx.h" "-fstdafx.h")
        set (CMAKE_AUTOMOC_MOC_OPTIONS "-bstdafx.h ")
    endif()
elseif(UNIX)
    set(LIBS_SYSTEM c stdc++)
endif()

#execute_process(
#    COMMAND ${CMAKE_COMMAND} -E environment
#)

# spdlog
#find_package(spdlog REQUIRED PATHS ../third_party/spdlog)

# pthreads
set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#openssl
find_package(OpenSSL REQUIRED)

set(OPENSSL_VER "OPENSSL_API_${OPENSSL_VERSION_MAJOR}_${OPENSSL_VERSION_MINOR}")
message(STATUS "OPENSSL Version: ${OPENSSL_VER} - ${OPENSSL_VERSION}")
#add_definitions(-D${OPENSSL_VER}) 

# 1.1.0 / 1.1.1 / 3.0.0 / 3.1.0
string(FIND "${OPENSSL_VERSION}" "1.0" SSL_VER_POS)
if(NOT ${SSL_VER_POS} EQUAL -1)
  add_definitions(-DOPENSSL_API_1_0)
  message(STATUS "Open ssl version is 1.0")
endif()    

string(FIND "${OPENSSL_VERSION}" "1.1" SSL_VER_POS)
if(NOT ${SSL_VER_POS} EQUAL -1)
  add_definitions(-DOPENSSL_API_1_1)
  message(STATUS "Open ssl version is 1.1")
endif()

string(FIND "${OPENSSL_VERSION}" "3.0" SSL_VER_POS)
if(NOT SSL_VER_POS EQUAL -1)
  add_definitions(-DOPENSSL_API_3_0)
  message(STATUS "Open ssl version is 3.0")
endif()  

string(FIND "${OPENSSL_VERSION}" "3.1" SSL_VER_POS)
if(NOT SSL_VER_POS EQUAL -1)
  add_definitions(-DOPENSSL_API_3_1)
  message(STATUS "Open ssl version is 3.1")
endif()

# Windows Libs
if (MSVC)
    # pthread
    find_package(pthread REQUIRED)

    find_package(expat CONFIG REQUIRED)

    # dlfcn
    find_package(dlfcn-win32 CONFIG REQUIRED)

else()
    find_package(expat CONFIG REQUIRED)
endif(MSVC)

if (MSVC)
    add_executable(test
        ./test.cpp
        ./com_sim.h
        ./com_sim.cpp
        ../src/p1item.h
        ../src/p1item.cpp
        ../src/alarm.h
        ../src/alarm.cpp
        ../src/energy-p1-obj.h
        ../src/energy-p1-obj.cpp
        
        $ENV{VSCP_ROOT}/src/vscp/common/vscp.h
        $ENV{VSCP_COMMON}/vscpdatetime.h
        $ENV{VSCP_COMMON}/vscpdatetime.cpp
        $ENV{VSCP_COMMON}/guid.h
        $ENV{VSCP_COMMON}/guid.cpp
        $ENV{VSCP_ROOT}/src/vscp/common/vscphelper.h
        $ENV{VSCP_ROOT}/src/vscp/common/vscphelper.cpp       
        $ENV{VSCP_ROOT}/src/common/windows/getopt.c
        $ENV{VSCP_ROOT}/src/common/configfile.h
        $ENV{VSCP_ROOT}/src/common/configfile.cpp
        $ENV{VSCP_ROOT}/src/common/vscpbase64.h
        $ENV{VSCP_ROOT}/src/common/vscpbase64.c
        $ENV{VSCP_ROOT}/src/common/vscp_aes.h
        $ENV{VSCP_ROOT}/src/common/vscp_aes.c
        $ENV{VSCP_ROOT}/src/common/crc.h
        $ENV{VSCP_ROOT}/src/common/crc.c
        $ENV{VSCP_ROOT}/src/common/crc8.h
        $ENV{VSCP_ROOT}/src/common/crc8.c
        $ENV{VSCP_ROOT}/src/common/vscpmd5.h
        $ENV{VSCP_ROOT}/src/common/vscpmd5.c
        $ENV{VSCP_ROOT}/src/common/com_win32.h
        $ENV{VSCP_ROOT}/src/common/com_win32.cpp
        ../third_party/fastpbkdf2/fastpbkdf2.h
        ../third_party/fastpbkdf2/fastpbkdf2.c
        ../third_party/mustache/mustache.hpp
    )
else()
    add_executable(test 
        ./test.cpp
        ./com_sim.h
        ./com_sim.cpp
        ../src/p1item.h
        ../src/p1item.cpp
        ../src/alarm.h
        ../src/alarm.cpp        
        ../src/energy-p1-obj.h
        ../src/energy-p1-obj.cpp
        $ENV{VSCP_ROOT}/src/vscp/common/vscp.h
        $ENV{VSCP_COMMON}/vscpdatetime.h
        $ENV{VSCP_COMMON}/vscpdatetime.cpp
        $ENV{VSCP_COMMON}/guid.h
        $ENV{VSCP_COMMON}/guid.cpp
        $ENV{VSCP_ROOT}/src/vscp/common/vscphelper.h
        $ENV{VSCP_ROOT}/src/vscp/common/vscphelper.cpp
        $ENV{VSCP_ROOT}/src/vscp/common/canal.h        
        $ENV{VSCP_ROOT}/src/common/configfile.h
        $ENV{VSCP_ROOT}/src/common/configfile.cpp
        $ENV{VSCP_ROOT}/src/common/vscpbase64.h
        $ENV{VSCP_ROOT}/src/common/vscpbase64.c
        $ENV{VSCP_ROOT}/src/common/vscp_aes.h
        $ENV{VSCP_ROOT}/src/common/vscp_aes.c
        $ENV{VSCP_ROOT}/src/common/crc.h
        $ENV{VSCP_ROOT}/src/common/crc.c
        $ENV{VSCP_ROOT}/src/common/crc8.h
        $ENV{VSCP_ROOT}/src/common/crc8.c
        $ENV{VSCP_ROOT}/src/common/vscpmd5.h
        $ENV{VSCP_ROOT}/src/common/vscpmd5.c        
        ../third_party/fastpbkdf2/fastpbkdf2.h
        ../third_party/fastpbkdf2/fastpbkdf2.c
        ../third_party/mustache/mustache.hpp
    )
 endif() 

#     * * * Includes * * *
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (MSVC)
    include_directories( PUBLIC $ENV{VSCP_ROOT}/src/common 
                            $ENV{VSCP_ROOT}/src/vscp/common/  
                            $ENV{VSCP_ROOT}/src/common/third_party
                            ../third_party/spdlog/include/  
                            ../third_party/nlohmann/include/
                            ../third_party/spdlog/include/
                            ../third_party/mustache/
                            ../third_party/fastpbkdf2/
                            $ENV{VSCP_ROOT}/src/common/windows                            
                            ${PThreads4W_INCLUDE_DIR}
                            ${CMAKE_BINARY_DIR}
                            ${DLFCN_INCLUDES}
                            ${LOCAL_WIN_INCLUDE} )
else()
    include_directories( PUBLIC $ENV{VSCP_ROOT}/src/common 
                            $ENV{VSCP_ROOT}/src/vscp/common/  
                            $ENV{VSCP_ROOT}/src/common/third_party
                            ../third_party/nlohmann/include
                            ../third_party/spdlog/include/
                            ../third_party/mustache/
                            ../third_party/fastpbkdf2/
                            ${PThreads4W_INCLUDE_DIR}
                            ${CMAKE_BINARY_DIR}
                            ${DLFCN_INCLUDES}
                            ${LOCAL_WIN_INCLUDE} )
 endif()   

if(MSVC)
    target_link_libraries(test PRIVATE 
        netapi32.lib                   
        PThreads4W::PThreads4W_CXXEXC
        dlfcn-win32::dl
        #spdlog::spdlog
        spdlog::spdlog_header_only
        OpenSSL::SSL 
        OpenSSL::Crypto
        expat::expat
    )
else()
    target_link_libraries(test PRIVATE     
        m
        dl
        systemd
        Threads::Threads
        #spdlog::spdlog
        #spdlog::spdlog_header_only
        OpenSSL::SSL 
        OpenSSL::Crypto
        expat::expat
        #${EXPAT_LIBRARIES}
    )
endif()

