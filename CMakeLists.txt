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

find_program (DXC_PATH "${CMAKE_SOURCE_DIR}/external/DXC/bin/x64/dxc")
if (NOT DXC_PATH)
    message (FATAL_ERROR "Can't find dxc in repo: ${WINDOWS_SDK_BIN}")
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