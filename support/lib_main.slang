
#ifdef COMPILER_SLANG
#define STF_SHADER_STAGE STF_SHADER_STAGE_LIBRARY
#define STF_SHADER_MODEL_MAJOR SHADER_MODEL_MAJOR
#define STF_SHADER_MODEL_MINOR SHADER_MODEL_MINOR

#if !__SLANG__
    #error "what"
#endif
#endif

#include <libraries/RTXTF-Library/STFSamplerState.hlsli>

#if defined(COMPILER_DXC)
#if STF_ALLOW_QUAD_COMM
        #error "STF_ALLOW_QUAD_COMM should be automatically disabled for DXC"
#endif

#if !STF_ALLOW_WAVE_READ
        #error "STF_ALLOW_WAVE_READ should be automatically enabled for DXC"
#endif
#endif

[shader("raygeneration")]
void lib_main()
{
}
