# Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

cmake_minimum_required(VERSION 3.10)

project(stf-sample)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Detect the Windows *target* architecture. On an ARM64 host, CMAKE_SYSTEM_PROCESSOR
# is still ARM64 even when configuring -A x64, so honor CMAKE_GENERATOR_PLATFORM first.
# DX12 works on ARM64; DLSS/NGX has no Windows ARM64 support yet and is left off in DLSS.cmake.
if (WIN32)
    if (CMAKE_GENERATOR_PLATFORM MATCHES "^([Aa][Rr][Mm]64)$")
        set(STF_ARM64 ON)
    elseif (CMAKE_GENERATOR_PLATFORM MATCHES "^(x64|X64|AMD64|x86_64)$")
        set(STF_ARM64 OFF)
    elseif (CMAKE_GENERATOR_PLATFORM STREQUAL "" AND
            CMAKE_SYSTEM_PROCESSOR MATCHES "^([Aa][Rr][Mm]64|[Aa][Aa][Rr][Cc][Hh]64)$")
        set(STF_ARM64 ON)
    else()
        set(STF_ARM64 OFF)
    endif()
else()
    set(STF_ARM64 OFF)
endif()

if (STF_ARM64)
    set(STF_WINDOWS_ARCH "arm64")
    message(STATUS "RTXTF: Configuring for Windows ARM64")
else()
    set(STF_WINDOWS_ARCH "x64")
endif()

# Helper to download and unzip a package from a URL
# Uses a zero-length file to identify the version of the package
function(CheckAndDownloadPackage NAME VERSION LOCAL_PATH URL)
    # Do we already have the correct version?
    if(NOT EXISTS ${LOCAL_PATH}/${VERSION}.ver)
        # Was there a previous version that we need to delete?
        if(EXISTS ${LOCAL_PATH})
            message(STATUS "Deleting old " ${NAME})
            file(REMOVE_RECURSE ${LOCAL_PATH})
        endif()
        message(STATUS "Obtaining " ${NAME} " " ${VERSION})
        file(DOWNLOAD ${URL} ${LOCAL_PATH}.zip)
        message(STATUS "Extracting ${NAME}: ${LOCAL_PATH}.zip -> ${LOCAL_PATH}")
        file(ARCHIVE_EXTRACT INPUT ${LOCAL_PATH}.zip DESTINATION ${LOCAL_PATH})
        file(REMOVE ${LOCAL_PATH}.zip)
        # Create an empty file so we know which version we have
        file(WRITE ${LOCAL_PATH}/${VERSION}.ver)
    endif()
endfunction()

CheckAndDownloadPackage("DXC" "v2025_02_20" ${CMAKE_SOURCE_DIR}/external/dxc https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2502/dxc_2025_02_20.zip)

find_program (DXC_PATH "${CMAKE_SOURCE_DIR}/external/dxc/bin/${STF_WINDOWS_ARCH}/dxc")
if (NOT DXC_PATH)
    message (FATAL_ERROR "Can't find DXC (${STF_WINDOWS_ARCH}) under ${CMAKE_SOURCE_DIR}/external/dxc/bin/${STF_WINDOWS_ARCH}")
endif ()

include(DLSS.cmake)

option(STF_SHADER_COMPILE_TESTS "Enable compilation tests" ON)

if (MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_ITERATOR_DEBUG_LEVEL=1")
endif()

option(DONUT_WITH_ASSIMP "" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(DONUT_SHADERS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/framework")

set(DONUT_WITH_DX11 OFF CACHE BOOL "Force DX11 to be disabled" FORCE)

# TODO: JUST TEMPORARY!
set(DONUT_WITH_VULKAN OFF CACHE BOOL "Force DX11 to be disabled" FORCE)

add_subdirectory(external/donut)

if (NVRHI_WITH_VULKAN OR NVRHI_WITH_DX12)
	add_subdirectory(samples/stf_bindless_rendering)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT stf_bindless_rendering)
endif()

if (STF_SHADER_COMPILE_TESTS)
	add_subdirectory(support)
endif()