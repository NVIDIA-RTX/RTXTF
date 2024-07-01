

#include <libraries/RTXTF-Library/STFDefinitions.h>

#ifdef COMPILER_FXC
    #define STF_SHADER_STAGE STF_SHADER_STAGE_PIXEL
    #define STF_SHADER_MODEL_MAJOR 5
    #define STF_SHADER_MODEL_MINOR 0
#endif
#if defined(COMPILER_DXC)
    // STF_SHADER_STAGE will be defined automatically
#endif
#ifdef COMPILER_SLANG
    #define STF_SHADER_STAGE STF_SHADER_STAGE_PIXEL
    #define STF_SHADER_MODEL_MAJOR SHADER_MODEL_MAJOR
    #define STF_SHADER_MODEL_MINOR SHADER_MODEL_MINOR
#endif

#include <libraries/RTXTF-Library/STFSamplerState.hlsli>

#if defined(COMPILER_DXC)
    #if !STF_ALLOW_WAVE_READ
        #error "STF_ALLOW_WAVE_READ should be automatically enabled for DXC"
    #endif
#endif

struct PSInput
{
    float4 color : COLOR;
};

float4 ps_main(PSInput input) : SV_TARGET
{
    return input.color;
}