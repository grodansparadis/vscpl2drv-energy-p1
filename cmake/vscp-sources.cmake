# vscp-sources.cmake
#
# Fetches the VSCP sources that the driver compiles in, then the header-only
# dependencies (nlohmann/json, spdlog, mustache) at the versions VSCP itself
# pins in its cmake/VSCPDependencies.cmake.
#
# Sets:
#   VSCP_ROOT                  Root of the VSCP source tree
#   VSCP_NLOHMANN_INCLUDE_DIR  nlohmann/json include directory
#   VSCP_SPDLOG_INCLUDE_DIR    spdlog include directory
#   VSCP_MUSTACHE_INCLUDE_DIR  mustache include directory
#
# To build against a local vscp checkout instead of the pinned commit:
#   cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_VSCP_SOURCES=/path/to/vscp
# The other dependencies can be overridden the same way, for example with
# FETCHCONTENT_SOURCE_DIR_VSCP_SPDLOG. FETCHCONTENT_FULLY_DISCONNECTED=ON
# reuses already downloaded sources without network access.
#

include_guard(GLOBAL)
include(FetchContent)

# vscp commit to build against. Update both lines together; the hash is the
# SHA256 of the downloaded tarball.
set(VSCP_SOURCES_COMMIT d75d4f5c6c097a1dca841b63727c79c522b701ce)
set(VSCP_SOURCES_SHA256 709763c16c995b65787e72947f9c3f45a03a404c708286c7cad00b53dcaedbb5)

set(_vscp_sources_options)
if(NOT CMAKE_VERSION VERSION_LESS "3.18")
    # Populate the sources without adding the VSCP CMake project.
    list(APPEND _vscp_sources_options SOURCE_SUBDIR "__vscp_sources_only__")
endif()
if(NOT CMAKE_VERSION VERSION_LESS "3.24")
    list(APPEND _vscp_sources_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

FetchContent_Declare(
    vscp_sources
    URL https://codeload.github.com/grodansparadis/vscp/tar.gz/${VSCP_SOURCES_COMMIT}
    URL_HASH SHA256=${VSCP_SOURCES_SHA256}
    ${_vscp_sources_options}
)

if(CMAKE_VERSION VERSION_LESS "3.18")
    FetchContent_GetProperties(vscp_sources)
    if(NOT vscp_sources_POPULATED)
        FetchContent_Populate(vscp_sources)
    endif()
else()
    FetchContent_MakeAvailable(vscp_sources)
endif()

set(VSCP_ROOT "${vscp_sources_SOURCE_DIR}")
if(NOT EXISTS "${VSCP_ROOT}/src/vscp/common/vscp.h")
    message(FATAL_ERROR "VSCP sources not found in ${VSCP_ROOT}")
endif()

# nlohmann/json, spdlog and mustache (and maddy) at VSCP's pinned versions
include("${VSCP_ROOT}/cmake/VSCPDependencies.cmake")

# The fmt library bundled with spdlog requires UTF-8 source handling on MSVC
# (VSCP sets the same options).
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
