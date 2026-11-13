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

cmake_minimum_required(VERSION 3.16)
project(test LANGUAGES CXX C)
set(PACKAGE_AUTHOR "Ake Hedman, the VSCP Project")

# VSCP sources, nlohmann/json, spdlog and mustache (shared with the driver)
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/vscp-sources.cmake)

# The test doesn't link vscp_util, so leave out vscphelper call tracing
add_definitions(-DVSCP_DISABLE_HELPER_LOGGING)

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
        
        ${VSCP_ROOT}/src/vscp/common/vscp.h
        ${VSCP_ROOT}/src/vscp/common/vscpdatetime.h
        ${VSCP_ROOT}/src/vscp/common/vscpdatetime.cpp
        ${VSCP_ROOT}/src/vscp/common/guid.h
        ${VSCP_ROOT}/src/vscp/common/guid.cpp
        ${VSCP_ROOT}/src/vscp/common/vscphelper.h
        ${VSCP_ROOT}/src/vscp/common/vscphelper.cpp
        ${VSCP_ROOT}/src/common/vscphelper-common.cpp
        ${VSCP_ROOT}/src/common/windows/getopt.c
        ${VSCP_ROOT}/src/common/configfile.h
        ${VSCP_ROOT}/src/common/configfile.cpp
        ${VSCP_ROOT}/src/common/vscpbase64.h
        ${VSCP_ROOT}/src/common/vscpbase64.c
        ${VSCP_ROOT}/src/common/vscp-aes.h
        ${VSCP_ROOT}/src/common/vscp-aes.c
        ${VSCP_ROOT}/src/common/crc.h
        ${VSCP_ROOT}/src/common/crc.c
        ${VSCP_ROOT}/src/common/crc8.h
        ${VSCP_ROOT}/src/common/crc8.c
        ${VSCP_ROOT}/src/common/vscpmd5.h
        ${VSCP_ROOT}/src/common/vscpmd5.c
        ${VSCP_ROOT}/src/common/com-win32.h
        ${VSCP_ROOT}/src/common/com-win32.cpp
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
        ${VSCP_ROOT}/src/vscp/common/vscp.h
        ${VSCP_ROOT}/src/vscp/common/vscpdatetime.h
        ${VSCP_ROOT}/src/vscp/common/vscpdatetime.cpp
        ${VSCP_ROOT}/src/vscp/common/guid.h
        ${VSCP_ROOT}/src/vscp/common/guid.cpp
        ${VSCP_ROOT}/src/vscp/common/vscphelper.h
        ${VSCP_ROOT}/src/vscp/common/vscphelper.cpp
        ${VSCP_ROOT}/src/common/vscphelper-common.cpp
        ${VSCP_ROOT}/src/vscp/common/canal.h        
        ${VSCP_ROOT}/src/common/configfile.h
        ${VSCP_ROOT}/src/common/configfile.cpp
        ${VSCP_ROOT}/src/common/vscpbase64.h
        ${VSCP_ROOT}/src/common/vscpbase64.c
        ${VSCP_ROOT}/src/common/vscp-aes.h
        ${VSCP_ROOT}/src/common/vscp-aes.c
        ${VSCP_ROOT}/src/common/crc.h
        ${VSCP_ROOT}/src/common/crc.c
        ${VSCP_ROOT}/src/common/crc8.h
        ${VSCP_ROOT}/src/common/crc8.c
        ${VSCP_ROOT}/src/common/vscpmd5.h
        ${VSCP_ROOT}/src/common/vscpmd5.c        
    )
 endif() 

#     * * * Includes * * *
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (MSVC)
    include_directories( PUBLIC ${VSCP_ROOT}/src/common 
                            ${VSCP_ROOT}/src/vscp/common/  
                            ${VSCP_NLOHMANN_INCLUDE_DIR}
                            ${VSCP_SPDLOG_INCLUDE_DIR}
                            ${VSCP_MUSTACHE_INCLUDE_DIR}
                            ${VSCP_ROOT}/src/common/windows                            
                            ${PThreads4W_INCLUDE_DIR}
                            ${CMAKE_BINARY_DIR}
                            ${DLFCN_INCLUDES}
                            ${LOCAL_WIN_INCLUDE} )
else()
    include_directories( PUBLIC ${VSCP_ROOT}/src/common 
                            ${VSCP_ROOT}/src/vscp/common/  
                            ${VSCP_NLOHMANN_INCLUDE_DIR}
                            ${VSCP_SPDLOG_INCLUDE_DIR}
                            ${VSCP_MUSTACHE_INCLUDE_DIR}
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
        OpenSSL::SSL 
        OpenSSL::Crypto
        expat::expat
        #${EXPAT_LIBRARIES}
    )
endif()

