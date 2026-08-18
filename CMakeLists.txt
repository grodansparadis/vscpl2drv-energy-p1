# CMakeLists.txt
#
# Build instructions for VSCP Works.
#
# Copyright (C) 2000-2023 Åke Hedman, the VSCP Project
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
# VSCP_PATH - set to path to local vscp repository. Default is "../vscp". Set with
# -DVSCP_PATH="path"
#

cmake_minimum_required(VERSION 3.16)
project(vscpl2drv-energy-p1 LANGUAGES CXX C)
set(PACKAGE_AUTHOR "Ake Hedman, the VSCP Project")


# Only spdlog::spdlog_header_only (an INTERFACE target) is used, so exclude
# the compiled spdlog library from the default build to save build time.
add_subdirectory(./third_party/spdlog/ EXCLUDE_FROM_ALL)

# Read version from version.h
file(READ "${CMAKE_SOURCE_DIR}/src/version.h" VERSION_FILE_CONTENT)
string(REGEX MATCH "#define MAJOR_VERSION[ \t]+([0-9]+)" _ ${VERSION_FILE_CONTENT})
set(MAJOR_VERSION ${CMAKE_MATCH_1})
string(REGEX MATCH "#define MINOR_VERSION[ \t]+([0-9]+)" _ ${VERSION_FILE_CONTENT})
set(MINOR_VERSION ${CMAKE_MATCH_1})
string(REGEX MATCH "#define RELEASE_VERSION[ \t]+([0-9]+)" _ ${VERSION_FILE_CONTENT})
set(RELEASE_VERSION ${CMAKE_MATCH_1})
string(REGEX MATCH "#define BUILD_VERSION[ \t]+([0-9]+)" _ ${VERSION_FILE_CONTENT})
set(BUILD_VERSION ${CMAKE_MATCH_1})

# Version settings
if(NOT DEFINED MAJOR_VERSION)
    SET(MAJOR_VERSION "15")
endif()
if(NOT DEFINED MINOR_VERSION)
    SET(MINOR_VERSION "0")
endif()
if(NOT DEFINED PATCH_VERSION)
    SET(PATCH_VERSION ${RELEASE_VERSION})
endif()

# Make version available as preprocessor definitions
add_definitions(-DMAJOR_VERSION=${MAJOR_VERSION})
add_definitions(-DMINOR_VERSION=${MINOR_VERSION})
add_definitions(-DRELEASE_VERSION=${RELEASE_VERSION})
add_definitions(-DBUILD_VERSION=${BUILD_VERSION})

# Display version information
message(STATUS "Version: ${MAJOR_VERSION}.${MINOR_VERSION}.${RELEASE_VERSION}.${BUILD_VERSION}")

SET(CPACK_PACKAGE_DESCRIPTION "VSCP driver for p1 energy meter")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VSCP level II Driver for VSCP that read p1 energy meter protocol and publish relevant events")
SET(CPACK_PACKAGE_VENDOR "the VSCP Project")
SET(CPACK_PACKAGE_CONTACT "akhe@vscp.org")

if(NOT DEFINED APP_LOW_NAME)
set(APP_LOW_NAME "vscpl2drv-energy-p1")
endif()

if(NOT DEFINED APP_NAME)
set(APP_NAME "VSCPL2DEV-ENERGY-P1")
endif()

if(NOT DEFINED APP_URL)
set(APP_URL "https://github.com/grodansparadis/vscpl2drv-energy-p1")
endif()

if(NOT DEFINED APP_VERSION)
set(APP_VERSION, "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")
endif()

# Man build and install
if(NOT DEFINED WIN32)
    add_subdirectory(man)
endif()

option(USE_SSL "Use SSL" TRUE)

## --- C++11 build flags ---
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Build locations
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Default location for VSCP level II drivers
set(VSCP_LEVEL2_DRV_DIR /var/lib/vscp/drivers/level2/)

# Generate position-independent code (-fPIC on UNIX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- System Libraries ---
include(GNUInstallDirs)

# Disable ABI warnings emitted by GCC when compiling nlohmann.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-psabi)
endif()

# --- Path to vscp --- 
if (NOT DEFINED VSCP_PATH)
set (VSCP_PATH "${PROJECT_SOURCE_DIR}/../vscp")
endif()

if(WIN32)
	  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DWIN32_LEAN_AND_MEANS)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DOPENSSL_API_1_1)
    add_definitions(-D_POSIX_C_SOURCE)    
    add_definitions(-DVCPKG_TARGET_TRIPLET=x64-windows)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(LIBS_SYSTEM ws2_32)
elseif(UNIX)
    set(LIBS_SYSTEM c stdc++)
endif()

#   ===========================  cpack  ===========================
IF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
  INCLUDE(InstallRequiredSystemLibraries)

  IF(WIN32)
  SET(CPACK_SET_DESTDIR OFF)
  ELSE()
  SET(CPACK_SET_DESTDIR ON)
  SET(CPACK_PACKAGING_INSTALL_PREFIX "/var/lib/vscp/drivers")
  ENDIF()

  IF(WIN32)
  SET(CPACK_GENERATOR "NSIS" "ZIP")
    ELSEIF(APPLE)
    SET(CPACK_GENERATOR "DragNDrop" "TGZ" "ZIP")
  ELSE()
  SET(CPACK_GENERATOR "DEB" "RPM" "TGZ" "ZIP")
  ENDIF()
  
  set(CPACK_PACKAGE_NAME ${APP_LOW_NAME})
  set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY ${APP_NAME})

  SET(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR_VERSION}")
  SET(CPACK_PACKAGE_VERSION_MINOR "${MINOR_VERSION}")
  SET(CPACK_PACKAGE_VERSION_PATCH "${PATCH_VERSION}")
  SET(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${MAJOR_VERSION}.${MINOR_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
  SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${MAJOR_VERSION}.${MINOR_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
    SET(CPACK_IGNORE_FILES "[.]psd$;/CVS/;/[.]svn/;/[.]git/;[.]swp$;/CMakeLists[.]txt[.]user;[.]#;/#;[.]tar[.]gz$;/CMakeFiles/;CMakeCache[.]txt;[.]qm$;/build/;[.]diff$;[.]DS_Store")
  SET(CPACK_SOURCE_IGNORE_FILES ${CPACK_IGNORE_FILES})

  SET(CPACK_NSIS_INSTALLED_ICON_NAME "${APP_LOW_NAME}.ico")
  SET(CPACK_NSIS_HELP_LINK ${APP_URL})
  SET(CPACK_NSIS_URL_INFO_ABOUT ${APP_URL})

  IF(NOT DEFINED WIN32)
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS " libexpat-dev, libssl-dev ")
  SET(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
  SET(CPACK_DEBIAN_PACKAGE_SECTION "network")
  SET(CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
  ENDIF()

  SET(CPACK_COMPONENTS_ALL Libraries ApplicationData)

  # Configure file with custom definitions for NSIS.
  configure_file(
    ${PROJECT_SOURCE_DIR}/resources/nsis/NSIS.definitions.nsh.in
    ${CMAKE_CURRENT_BINARY_DIR}/resources/nsis/NSIS.definitions.nsh
  )
 
    # Load packaging facilities.
    INCLUDE(CPack)

ENDIF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")


set(CMAKE_INCLUDE_CURRENT_DIR ON)

# pthreads
set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# spdlog
#find_package(spdlog REQUIRED PATHS ./third_party/spdlog)

#openssl
find_package(OpenSSL REQUIRED)

# Expat C library
find_package(EXPAT REQUIRED)

# Set sockettcp build flags
add_definitions(-DUSE_IPV6)
string(FIND "${OPENSSL_VERSION}" "1.1." SSL_VER_POS)
if(${SSL_VER_POS})
    add_definitions(-DOPENSSL_API_1_0)
else()
    add_definitions(-DOPENSSL_API_1_1)
endif()

set(RESOURCE_FILES
        README.md
        forms/index.html)

# Add files to library
add_library(vscpl2drv-energy-p1
    ${CMAKE_SOURCE_DIR}/src/vscpl2drv-energy-p1.h   
    ${CMAKE_SOURCE_DIR}/src/vscpl2drv-energy-p1.cpp
    ${CMAKE_SOURCE_DIR}/src/p1item.h 
    ${CMAKE_SOURCE_DIR}/src/p1item.cpp 
    ${CMAKE_SOURCE_DIR}/src/energy-p1-obj.h 
    ${CMAKE_SOURCE_DIR}/src/energy-p1-obj.cpp
    ${CMAKE_SOURCE_DIR}/src/alarm.h 
    ${CMAKE_SOURCE_DIR}/src/alarm.cpp
    ${CMAKE_SOURCE_DIR}/src/serialport.h
    #./third_party/mustache/mustache.hpp
    #./third_party/spdlog/include    
    third_party/vscp/src/vscp/common/vscp.h
    #third_party/vscp/src/vscp/common/vscpremotetcpif.h
    #third_party/vscp/src/vscp/common/vscpremotetcpif.cpp
    #$third_party/vscp/src/vscp/common/vscpdatetime.h
    #third_party/vscp/src/vscp/common/vscpdatetime.cpp
    third_party/vscp/src/vscp/common/guid.h
    third_party/vscp/src/vscp/common/guid.cpp
    #third_party/vscp/src/vscp/common/mdf.h
    #third_party/vscp/src/vscp/common/mdf.cpp
    third_party/vscp/src/vscp/common/vscphelper.h
    third_party/vscp/src/vscp/common/vscphelper.cpp
    third_party/vscp/src/common/crc.c
    third_party/vscp/src/common/crc8.c
    third_party/vscp/src/common/vscp-aes.c
    third_party/vscp/src/common/vscpbase64.c
    third_party/vscp/src/common/vscpmd5.c
    #third_party/vscp/src/vscp/common/canal.h
    #third_party/vscp/src/common/sockettcp.h
    #third_party/vscp/src/common/sockettcp.c
    #third_party/vscp/src/common/vscpbase64.h
    #third_party/vscp/src/common/vscpbase64.c
    #third_party/vscp/src/common/vscp_aes.h
    #third_party/vscp/src/common/vscp_aes.c
    #third_party/vscp/src/common/crc.h
    #third_party/vscp/src/common/crc.c
    #third_party/vscp/src/common/crc8.h
    #third_party/vscp/src/common/crc8.c
    #third_party/vscp/src/common/vscpmd5.h
    #third_party/vscp/src/common/vscpmd5.c
    $<$<BOOL:${WIN32}>:third_party/vscp/src/common/com-win32.h>
    $<$<BOOL:${WIN32}>:third_party/vscp/src/common/com-win32.cpp>
    $<$<NOT:$<BOOL:${WIN32}>>:third_party/vscp/src/common/com-linux.h>
    $<$<NOT:$<BOOL:${WIN32}>>:third_party/vscp/src/common/com-linux.cpp>
  ) 

if (WIN32)    
    find_package(pthreads REQUIRED)
    find_path(PTHREAD_INCLUDE_DIR pthread.h)

    include_directories( PUBLIC src/
        ./third_party/vscp//src/vscp/common/  
        ./third_party/vscp//src/common
        ./third_party/vscp//src/common/third_party/
        ./third_party/nlohmann/include/
        ./third_party/spdlog/include/
        ./third_party/fastpbkdf2/
        ./third_party/mustache/
        ./third_party/maddy/include/
        ${PThreads4W_INCLUDE_DIR}
        ${CMAKE_BINARY_DIR}
        ${OPENSSL_INCLUDE_DIR}
        ./third_party/vscp/src/vscp/daemon/
        ${LOCAL_WIN_INCLUDE}
        ${LOCAL_SYSLOG_INCLUDE} )
else()
    include_directories( PUBLIC src/
        ./third_party/vscp//src/vscp/common/
        ./third_party/vscp//src/common
        ./third_party/vscp//src/common/third_party/
        ./third_party/nlohmann/include/
        ./third_party/spdlog/include/
        ./third_party/fastpbkdf2/
        ./third_party/mustache/
        ./third_party/maddy/include/
        ${CMAKE_BINARY_DIR}
        ${OPENSSL_INCLUDE_DIR}
        ${LOCAL_SYSLOG_INCLUDE} )
 endif()   

if(WIN32)
    target_link_libraries(vscpl2drv-energy-p1 PRIVATE     
        PThreads4W::PThreads4W_CXXEXC
        #spdlog::spdlog
        spdlog::spdlog_header_only
        OpenSSL::SSL 
        OpenSSL::Crypto
        EXPAT::EXPAT
    )
        
elseif(APPLE)
    target_link_libraries(vscpl2drv-energy-p1 PRIVATE
        Threads::Threads
        spdlog::spdlog_header_only
        OpenSSL::SSL
        OpenSSL::Crypto
        EXPAT::EXPAT
    )
else()
    target_link_libraries(vscpl2drv-energy-p1 PRIVATE     
        m
        dl
        Threads::Threads
        #spdlog::spdlog
        spdlog::spdlog_header_only
        OpenSSL::SSL 
        OpenSSL::Crypto
        EXPAT::EXPAT
    )
endif()

# Install
if(WIN32)
    # Runtime files
    install(TARGETS vscpl2drv-energy-p1
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    # Dependencies
    file(GLOB deps_files "${CMAKE_BINARY_DIR}/Release/*.dll")
    install(FILES ${deps_files} DESTINATION ${CMAKE_INSTALL_BINDIR})
    # LICENSE
    install(FILES "${CMAKE_BINARY_DIR}/../LICENSE" DESTINATION "${CMAKE_INSTALL_BINDIR}/..")
    # README
    install(FILES "${CMAKE_BINARY_DIR}/../README.md" DESTINATION "${CMAKE_INSTALL_BINDIR}/..")
else()
    install(TARGETS vscpl2drv-energy-p1
            LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_}${VSCP_LEVEL2_DRV_DIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_MANDIR}            
            RESOURCE DESTINATION ${CMAKE_INSTALL_FULL_}/var/lib/vscp/vscpd) 
    install(FILES ${CMAKE_SOURCE_DIR}/resources/linux/energyp1.json
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/vscpl2drv-energy-p1/")             
endif()
