/***************************************************************************
 # Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#include <libraries/RTXTF-Library/STFDefinitions.h>

#ifdef COMPILER_SLANG
#define STF_SHADER_STAGE STF_SHADER_STAGE_COMPUTE
#define STF_SHADER_MODEL_MAJOR SHADER_MODEL_MAJOR
#define STF_SHADER_MODEL_MINOR SHADER_MODEL_MINOR

#if !__SLANG__
    #error "what"
#endif
#endif

#include <libraries/RTXTF-Library/STFSamplerState.hlsli>

#if defined(COMPILER_DXC)
    #if !STF_ALLOW_QUAD_COMM
        #error "STF_ALLOW_QUAD_COMM should be automatically enabled for DXC"
    #endif

    #if !STF_ALLOW_WAVE_READ
        #error "STF_ALLOW_WAVE_READ should be automatically enabled for DXC"
    #endif
#endif

[numthreads(4, 1, 1)]
void cs_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
}