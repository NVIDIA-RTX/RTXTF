# Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

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
