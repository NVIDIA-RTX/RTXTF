
#include <libraries/RTXTF-Library/STFDefinitions.h>

#if defined(COMPILER_DXC)
    // STF_SHADER_STAGE will be defined automatically
    // STF_SHADER_MODEL_MAJOR will be defined automatically
    // STF_SHADER_MODEL_MINOR will be defined automatically
#endif

#ifdef COMPILER_SLANG
#define STF_SHADER_STAGE STF_SHADER_STAGE_LIBRARY
#define STF_SHADER_MODEL_MAJOR SHADER_MODEL_MAJOR
#define STF_SHADER_MODEL_MINOR SHADER_MODEL_MINOR
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
