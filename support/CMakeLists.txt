# Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

include(compileshaders.cmake)
file(GLOB shaders "*.hlsl" "*.slang")

set(project shaders)
set(folder "support")

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

CheckAndDownloadPackage("Slang" "v2024.9.2" ${CMAKE_SOURCE_DIR}/external/Slang https://github.com/shader-slang/slang/releases/download/v2024.9.2/slang-2024.9.2-windows-x86_64.zip)

find_program (SLANG_PATH "${CMAKE_SOURCE_DIR}/external/Slang/bin/slangc")
if (NOT SLANG_PATH)
    message (FATAL_ERROR "Can't find Slang in repo: ${WINDOWS_SDK_BIN}")
endif ()

set(STF_SHADER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}")

test_compile_shaders(
    TARGET ${project}_dxc_2021_sm6_6
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/shaders_dxc.cfg
    SOURCES ${shaders}
    FOLDER ${folder}
    INCLUDE_DIR ${STF_SHADER_INCLUDE_DIR}
    COMPILER_OPTIONS_DXIL "--hlsl2021 --shaderModel=6_6"
    COMPILER_OPTIONS_SPIRV "--hlsl2021 --shaderModel=6_6"
    COMPILER_PATH_DXIL ${DXC_PATH}
    COMPILER_PATH_SPIRV ${DXC_SPIRV_PATH}
    DXIL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_dxc_2021_sm6_6/dxil
    SPIRV_DXC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_dxc_2021_sm6_6/spirv
)

test_compile_shaders(
    TARGET ${project}_dxc_2021_sm6_2
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/shaders_dxc_6_2.cfg
    SOURCES ${shaders}
    FOLDER ${folder}
    INCLUDE_DIR ${STF_SHADER_INCLUDE_DIR}
    COMPILER_OPTIONS_DXIL "--hlsl2021 --shaderModel=6_2"
    COMPILER_OPTIONS_SPIRV "--hlsl2021 --shaderModel=6_2"
    COMPILER_PATH_DXIL ${DXC_PATH}
    COMPILER_PATH_SPIRV ${DXC_SPIRV_PATH}
    DXIL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_dxc_2021_sm6_2/dxil
    SPIRV_DXC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_dxc_2021_sm6_2/spirv
)

test_compile_shaders(
    TARGET ${project}_dxc_2018
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/shaders_dxc.cfg
    SOURCES ${shaders}
    FOLDER ${folder}
    INCLUDE_DIR ${STF_SHADER_INCLUDE_DIR}
    COMPILER_OPTIONS_DXIL
    COMPILER_OPTIONS_SPIRV
    COMPILER_PATH_DXIL ${DXC_PATH}
    COMPILER_PATH_SPIRV ${DXC_SPIRV_PATH}
    DXIL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_dxc_2018/dxil
    SPIRV_DXC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_dxc_2018/spirv
)

test_compile_shaders(
    TARGET ${project}_fxc
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/shaders_fxc.cfg
    SOURCES ${shaders}
    FOLDER ${folder}
    INCLUDE_DIR ${STF_SHADER_INCLUDE_DIR}
    COMPILER_PATH_DXBC ${FXC_PATH}
    DXBC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_fxc/dxbc
)

test_compile_shaders(
    TARGET ${project}_slang
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/shaders_slang.cfg
    SOURCES ${shaders}
    FOLDER ${folder}
    INCLUDE_DIR ${STF_SHADER_INCLUDE_DIR}
    COMPILER_OPTIONS_DXIL "--slang"
    COMPILER_OPTIONS_SPIRV "--slang"
    COMPILER_PATH_DXIL ${SLANG_PATH}
    COMPILER_PATH_SPIRV ${SLANG_PATH}
    DXIL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_slang/dxil
    SPIRV_DXC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}_slang/spirv
)

add_custom_target(shaders_all ALL DEPENDS
        ${project}_dxc_2021_sm6_2
        ${project}_dxc_2021_sm6_6
        ${project}_dxc_2018
        ${project}_fxc
        ${project}_slang 
        )
set_target_properties(shaders_all PROPERTIES FOLDER "Tests")
