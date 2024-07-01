#
# Copyright (c) 2014-2020, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

set (NVRHI_DEFAULT_VK_REGISTER_OFFSETS
    --tRegShift 0
    --sRegShift 128
    --bRegShift 256
    --uRegShift 384)

function(test_compile_shaders)
    set(options "")
    set(oneValueArgs TARGET CONFIG FOLDER OUTPUT_FORMAT DXIL DXBC SPIRV_DXC
                     COMPILER_PATH_DXBC COMPILER_PATH_DXIL COMPILER_PATH_SPIRV
                     COMPILER_OPTIONS_DXBC COMPILER_OPTIONS_DXIL COMPILER_OPTIONS_SPIRV
                     INCLUDE_DIR
                     )
    set(multiValueArgs SOURCES BYPRODUCTS_DXBC BYPRODUCTS_DXIL BYPRODUCTS_SPIRV)
    cmake_parse_arguments(params "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT params_TARGET)
        message(FATAL_ERROR "donut_compile_shaders: TARGET argument missing")
    endif()
    if (NOT params_CONFIG)
        message(FATAL_ERROR "donut_compile_shaders: CONFIG argument missing")
    endif()

    # just add the source files to the project as documents, they are built by the script
    set_source_files_properties(${params_SOURCES} PROPERTIES VS_TOOL_OVERRIDE "None") 

    add_custom_target(${params_TARGET}
        DEPENDS ShaderMake
        SOURCES ${params_SOURCES})

    set(use_api_arg "")

    if ("${params_OUTPUT_FORMAT}" STREQUAL "HEADER")
        set(output_format_arg --headerBlob)
    elseif(("${params_OUTPUT_FORMAT}" STREQUAL "BINARY") OR ("${params_OUTPUT_FORMAT}" STREQUAL ""))
        set(output_format_arg --binaryBlob --outputExt .bin)
    else()
        message(FATAL_ERROR "donut_compile_shaders: unsupported OUTPUT_FORMAT = '${params_OUTPUT_FORMAT}'")
    endif()

    if (params_DXIL AND DONUT_WITH_DX12)
        if (NOT params_COMPILER_PATH_DXIL)
            message(FATAL_ERROR "donut_compile_shaders: DXC not found --- please set params_COMPILER_PATH_DXIL to the full path to the DXC binary")
        endif()
        
        set(compilerCommand ShaderMake
           --config ${params_CONFIG}
           --out ${params_DXIL}
           --platform DXIL
           ${output_format_arg}
           -I ${params_INCLUDE_DIR}
           --compiler ${params_COMPILER_PATH_DXIL}
           --shaderModel 6_5
           ${use_api_arg})

        separate_arguments(params_COMPILER_OPTIONS_DXIL NATIVE_COMMAND "${params_COMPILER_OPTIONS_DXIL}")

        list(APPEND compilerCommand ${params_COMPILER_OPTIONS_DXIL})

        if ("${params_BYPRODUCTS_DXIL}" STREQUAL "")
            add_custom_command(TARGET ${params_TARGET} PRE_BUILD COMMAND ${compilerCommand})
        else()
            set(byproducts_with_paths "")
            foreach(relative_path IN LISTS params_BYPRODUCTS_DXIL)
                list(APPEND byproducts_with_paths "${pasams_DXIL}/${relative_path}")
            endforeach()
            
            add_custom_command(TARGET ${params_TARGET} PRE_BUILD COMMAND ${compilerCommand} BYPRODUCTS "${byproducts_with_paths}")
        endif()
    endif()

    if (params_DXBC AND DONUT_WITH_DX11)
        if (NOT params_COMPILER_PATH_DXBC)
            message(FATAL_ERROR "donut_compile_shaders: FXC not found --- please set COMPILER_PATH_DXBC to the full path to the FXC binary")
        endif()
        
        set(compilerCommand ShaderMake
           --config ${params_CONFIG}
           --out ${params_DXBC}
           --platform DXBC
           ${output_format_arg}
           -I ${params_INCLUDE_DIR}
           --compiler ${params_COMPILER_PATH_DXBC}
           ${use_api_arg})

        separate_arguments(params_COMPILER_OPTIONS_DXBC NATIVE_COMMAND "${params_COMPILER_OPTIONS_DXBC}")

        list(APPEND compilerCommand ${params_COMPILER_OPTIONS_DXBC})

        if ("${params_BYPRODUCTS_DXBC}" STREQUAL "")
            add_custom_command(TARGET ${params_TARGET} PRE_BUILD COMMAND ${compilerCommand})
        else()
            set(byproducts_with_paths "")
            foreach(relative_path IN LISTS params_BYPRODUCTS_DXBC)
                list(APPEND byproducts_with_paths "${pasams_DXBC}/${relative_path}")
            endforeach()

            add_custom_command(TARGET ${params_TARGET} PRE_BUILD COMMAND ${compilerCommand} BYPRODUCTS "${byproducts_with_paths}")
        endif()
    endif()

    if (params_SPIRV_DXC AND DONUT_WITH_VULKAN)
        if (NOT params_COMPILER_PATH_SPIRV)
            message(FATAL_ERROR "donut_compile_shaders: DXC for SPIR-V not found --- please set params_COMPILER_PATH_SPIRV to the full path to the DXC binary")
        endif()
        
        set(compilerCommand ShaderMake
           --config ${params_CONFIG}
           --out ${params_SPIRV_DXC}
           --platform SPIRV
           ${output_format_arg}
           -I ${params_INCLUDE_DIR}
           -D SPIRV
           --compiler ${params_COMPILER_PATH_SPIRV}
           ${NVRHI_DEFAULT_VK_REGISTER_OFFSETS}
           --vulkanVersion 1.2
           ${use_api_arg})

        separate_arguments(params_COMPILER_OPTIONS_SPIRV NATIVE_COMMAND "${params_COMPILER_OPTIONS_SPIRV}")

        list(APPEND compilerCommand ${params_COMPILER_OPTIONS_SPIRV})

        if ("${params_BYPRODUCTS_SPIRV}" STREQUAL "")
            add_custom_command(TARGET ${params_TARGET} PRE_BUILD COMMAND ${compilerCommand})
        else()
            set(byproducts_with_paths "")
            foreach(relative_path IN LISTS params_BYPRODUCTS_SPIRV)
                list(APPEND byproducts_with_paths "${params_SPIRV_DXC}/${relative_path}")
            endforeach()

            add_custom_command(TARGET ${params_TARGET} PRE_BUILD COMMAND ${compilerCommand} BYPRODUCTS "${byproducts_with_paths}")
        endif()
    endif()

    if(params_FOLDER)
        set_target_properties(${params_TARGET} PROPERTIES FOLDER ${params_FOLDER})
    endif()
endfunction()
