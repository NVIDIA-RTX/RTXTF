# Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.


include(../../external/donut/compileshaders.cmake)
file(GLOB shaders "*.hlsl" "*.hlsli")
file(GLOB sources "*.cpp" "*.h")

set(project stf_bindless_rendering)
set(folder "Samples/STF Bindless Rendering")

donut_compile_shaders(
    TARGET ${project}_shaders
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/shaders.cfg
	SHADERMAKE_OPTIONS_DXIL "--hlsl2021 --shaderModel=6_7"
    SHADERMAKE_OPTIONS_SPIRV "--hlsl2021 --shaderModel=6_7"
    SOURCES ${shaders}
    FOLDER ${folder}
    DXIL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}/dxil
    SPIRV_DXC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${project}/spirv
	VULKAN_VERSION 1.3
)

add_executable(${project} WIN32 ${sources})
target_link_libraries(${project} donut_render donut_app donut_engine)
add_dependencies(${project} ${project}_shaders)
set_target_properties(${project} PROPERTIES FOLDER ${folder})

set(DONUT_D3D_AGILITY_SDK_URL "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.616.1")
include("${PROJECT_SOURCE_DIR}/external/donut/cmake/FetchAgilitySDK.cmake")

if(DONUT_D3D_AGILITY_SDK_ENABLED)
    target_compile_definitions(${project} PUBLIC DONUT_D3D_AGILITY_SDK_ENABLED=1)
    target_compile_definitions(${project} PUBLIC DONUT_D3D_AGILITY_SDK_VERSION=${DONUT_D3D_AGILITY_SDK_VERSION})
    add_custom_command(TARGET ${project} POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${project}>/D3D12/"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different ${DONUT_D3D_AGILITY_SDK_CORE_DLL} "$<TARGET_FILE_DIR:${project}>/D3D12/"
    )
endif()

if (TARGET DLSS)
	target_compile_definitions(${project} PRIVATE 
		ENABLE_DLSS=1 
		DLSS_WITH_DX12=$<IF:$<BOOL:${DONUT_WITH_DX12}>,1,0> 
		DLSS_WITH_VK=$<IF:$<BOOL:${DONUT_WITH_VULKAN}>,1,0>)
		
	target_link_libraries(${project} DLSS)
	add_custom_command(TARGET ${project} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DLSS_SHARED_LIBRARY_PATH}"
		"$<TARGET_FILE_DIR:${project}>/")
else()
	target_compile_definitions(${project} PRIVATE 
		ENABLE_DLSS=0
		DLSS_WITH_DX12=0 
		DLSS_WITH_VK=0)
endif()
