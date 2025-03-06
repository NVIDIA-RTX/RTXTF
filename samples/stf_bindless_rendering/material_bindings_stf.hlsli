/*
* Copyright (c) 2014-2024, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#ifndef MATERIAL_BINDINGS_HLSLI
#define MATERIAL_BINDINGS_HLSLI

#include <donut/shaders/binding_helpers.hlsli>
#include "lighting_cb.h"
#include "rng.hlsli"

#include "../../libraries/RTXTF-Library/STFSamplerState.hlsli"


// Bindings - can be overriden before including this file if necessary

#ifndef MATERIAL_CB_SLOT 
#define MATERIAL_CB_SLOT 0
#endif

#ifndef LIGHTING_CB_SLOT 
#define LIGHTING_CB_SLOT 1
#endif

#ifndef MATERIAL_DIFFUSE_SLOT 
#define MATERIAL_DIFFUSE_SLOT 0
#endif

#ifndef MATERIAL_SPECULAR_SLOT 
#define MATERIAL_SPECULAR_SLOT 1
#endif

#ifndef MATERIAL_NORMALS_SLOT 
#define MATERIAL_NORMALS_SLOT 2
#endif

#ifndef MATERIAL_EMISSIVE_SLOT 
#define MATERIAL_EMISSIVE_SLOT 3
#endif

#ifndef MATERIAL_OCCLUSION_SLOT 
#define MATERIAL_OCCLUSION_SLOT 4
#endif

#ifndef MATERIAL_TRANSMISSION_SLOT 
#define MATERIAL_TRANSMISSION_SLOT 5
#endif

#ifndef MATERIAL_OPACITY_SLOT 
#define MATERIAL_OPACITY_SLOT 6
#endif

#ifndef MATERIAL_SAMPLER_SLOT 
#define MATERIAL_SAMPLER_SLOT 0
#endif

#ifndef MATERIAL_REGISTER_SPACE
#define MATERIAL_REGISTER_SPACE 0
#endif

#ifndef MATERIAL_SAMPLER_REGISTER_SPACE
#define MATERIAL_SAMPLER_REGISTER_SPACE 0
#endif

#ifndef MATERIAL_BLUE_NOISE_SLOT
#define MATERIAL_BLUE_NOISE_SLOT 0
#endif

cbuffer c_Material : REGISTER_CBUFFER(MATERIAL_CB_SLOT, MATERIAL_REGISTER_SPACE)
{
    MaterialConstants g_Material;
};

DECLARE_CBUFFER(LightingConstants, g_Const, GBUFFER_BINDING_VIEW_CONSTANTS, GBUFFER_SPACE_VIEW);

Texture2D t_BaseOrDiffuse        : REGISTER_SRV(MATERIAL_DIFFUSE_SLOT,       MATERIAL_REGISTER_SPACE);
Texture2D t_MetalRoughOrSpecular : REGISTER_SRV(MATERIAL_SPECULAR_SLOT,      MATERIAL_REGISTER_SPACE);
Texture2D t_Normal               : REGISTER_SRV(MATERIAL_NORMALS_SLOT,       MATERIAL_REGISTER_SPACE);
Texture2D t_Emissive             : REGISTER_SRV(MATERIAL_EMISSIVE_SLOT,      MATERIAL_REGISTER_SPACE);
Texture2D t_Occlusion            : REGISTER_SRV(MATERIAL_OCCLUSION_SLOT,     MATERIAL_REGISTER_SPACE);
Texture2D t_Transmission         : REGISTER_SRV(MATERIAL_TRANSMISSION_SLOT,  MATERIAL_REGISTER_SPACE);
Texture2D t_Opacity              : REGISTER_SRV(MATERIAL_OPACITY_SLOT,       MATERIAL_REGISTER_SPACE);
SamplerState s_MaterialSampler   : REGISTER_SAMPLER(MATERIAL_SAMPLER_SLOT,   MATERIAL_SAMPLER_REGISTER_SPACE);
Texture2D STBN2DTexture          : REGISTER_SRV(MATERIAL_BLUE_NOISE_SLOT,    GBUFFER_SPACE_VIEW);

float4 SampleTexture(inout STF_SamplerState stfSamplerState, bool stfEnabled, Texture2D texture, SamplerState materialSampler, float2 texCoord)
{
#if STF_ENABLED
    if (stfEnabled)
    {
        #if STF_LOAD
            return stfSamplerState.Texture2DLoad(texture, texCoord);
        #else
            return stfSamplerState.Texture2DSample(texture, materialSampler, texCoord);
        #endif
    } else
#endif
    {
        return texture.Sample(materialSampler, texCoord);
    }
}

float4 SampleTextureLevel(inout STF_SamplerState stfSamplerState, bool stfEnabled, Texture2D texture, SamplerState materialSampler, float2 texCoord, float mipLevel)
{
#if STF_ENABLED
    if (stfEnabled)
    {
        #if STF_LOAD
            return stfSamplerState.Texture2DLoadLevel(texture, texCoord, mipLevel);
        #else
            return stfSamplerState.Texture2DSampleLevel(texture, materialSampler, texCoord, mipLevel);
        #endif
    } else
#endif
    {
        return texture.SampleLevel(materialSampler, texCoord, mipLevel);
    }
}

float4 SampleTextureGrad(inout STF_SamplerState stfSamplerState, bool stfEnabled, Texture2D texture, SamplerState materialSampler, float2 texCoord, float2 dx, float2 dy)
{
#if STF_ENABLED
    if (stfEnabled)
    {
        #if STF_LOAD
            return stfSamplerState.Texture2DLoadGrad(texture, texCoord, dx, dy);
        #else
            return stfSamplerState.Texture2DSampleGrad(texture, materialSampler, texCoord, dx, dy);
        #endif
    } else
#endif
    {
        return texture.SampleGrad(materialSampler, texCoord, dx, dy);
    }
}

void InitSTF(inout STF_SamplerState stfSamplerState, uint2 pixelPosition)
{
    float3 u = RNG::SpatioTemporalBlueNoise2D(pixelPosition, g_Const.stfFrameIndex, STBN2DTexture);
    if (g_Const.stfUseWhiteNoise)
        u = RNG::SpatioTemporalWhiteNoise3D(pixelPosition, g_Const.stfFrameIndex);

    stfSamplerState = STF_SamplerState::Create(float4(u.x, u.y, 0 /*slice - unused*/, u.z));
    stfSamplerState.SetFilterType(g_Const.stfFilterMode);
    stfSamplerState.SetFrameIndex(g_Const.stfFrameIndex);
    stfSamplerState.SetMagMethod(g_Const.stfMagnificationMethod);
    stfSamplerState.SetAddressingModes(g_Const.stfAddressMode.xxx);
    stfSamplerState.SetSigma(g_Const.stfSigma);
    stfSamplerState.SetAnisoMethod(g_Const.stfMinificationMethod);
    stfSamplerState.SetReseedOnSample(g_Const.stfReseedOnSample);
}

MaterialTextureSample SampleMaterialTextures(float2 texCoord, float2 normalTexCoordScale, float2 pixelPosition)
{
    MaterialTextureSample values = DefaultMaterialTextures();

    STF_SamplerState stfSamplerState;
    InitSTF(stfSamplerState, pixelPosition);

#if STF_ENABLED
    bool stfEnabled = true;
    if (g_Const.stfSplitScreen && pixelPosition.x > (g_Const.view.viewportSize.x / 2.f))
    {
        stfEnabled = false;
    }
#else
    const bool stfEnabled = false;
#endif

    if ((g_Material.flags & MaterialFlags_UseBaseOrDiffuseTexture) != 0)
    {
        values.baseOrDiffuse = SampleTexture(stfSamplerState, stfEnabled, t_BaseOrDiffuse, s_MaterialSampler, texCoord);
    }

    if ((g_Material.flags & MaterialFlags_UseMetalRoughOrSpecularTexture) != 0)
    {
        values.metalRoughOrSpecular = SampleTexture(stfSamplerState, stfEnabled, t_MetalRoughOrSpecular, s_MaterialSampler, texCoord);
    }

    if ((g_Material.flags & MaterialFlags_UseEmissiveTexture) != 0)
    {
        values.emissive = SampleTexture(stfSamplerState, stfEnabled, t_Emissive, s_MaterialSampler, texCoord);
    }

    if ((g_Material.flags & MaterialFlags_UseNormalTexture) != 0)
    {
        values.normal = SampleTexture(stfSamplerState, stfEnabled, t_Normal, s_MaterialSampler, texCoord);
    }

    if ((g_Material.flags & MaterialFlags_UseOcclusionTexture) != 0)
    {
        values.occlusion = SampleTexture(stfSamplerState, stfEnabled, t_Occlusion, s_MaterialSampler, texCoord);
    }

    if ((g_Material.flags & MaterialFlags_UseTransmissionTexture) != 0)
    {
        values.transmission = SampleTexture(stfSamplerState, stfEnabled, t_Transmission, s_MaterialSampler, texCoord);
    }

    if ((g_Material.flags & MaterialFlags_UseOpacityTexture) != 0)
    {
        values.opacity = SampleTexture(stfSamplerState, stfEnabled, t_Opacity, s_MaterialSampler, texCoord).x;
    }

    return values;
}

MaterialTextureSample SampleMaterialTexturesLevel(float2 texCoord, float lod, float2 pixelPosition)
{
    MaterialTextureSample values = DefaultMaterialTextures();

    STF_SamplerState stfSamplerState;
    InitSTF(stfSamplerState, pixelPosition);
    
#if STF_ENABLED
    bool stfEnabled = true;
    if (g_Const.stfSplitScreen && pixelPosition.x > (g_Const.view.viewportSize.x / 2.f))
    {
        stfEnabled = false;
    }
#else
    const bool stfEnabled = false;
#endif

    if ((g_Material.flags & MaterialFlags_UseBaseOrDiffuseTexture) != 0)
    {
        values.baseOrDiffuse = SampleTextureLevel(stfSamplerState, stfEnabled, t_BaseOrDiffuse, s_MaterialSampler, texCoord, lod);
    }

    if ((g_Material.flags & MaterialFlags_UseMetalRoughOrSpecularTexture) != 0)
    {
        values.metalRoughOrSpecular = SampleTextureLevel(stfSamplerState, stfEnabled, t_MetalRoughOrSpecular, s_MaterialSampler, texCoord, lod);
    }

    if ((g_Material.flags & MaterialFlags_UseEmissiveTexture) != 0)
    {
        values.emissive = SampleTextureLevel(stfSamplerState, stfEnabled, t_Emissive, s_MaterialSampler, texCoord, lod);
    }

    if ((g_Material.flags & MaterialFlags_UseNormalTexture) != 0)
    {
        values.normal = SampleTextureLevel(stfSamplerState, stfEnabled, t_Normal, s_MaterialSampler, texCoord, lod);
    }

    if ((g_Material.flags & MaterialFlags_UseOcclusionTexture) != 0)
    {
        values.occlusion = SampleTextureLevel(stfSamplerState, stfEnabled, t_Occlusion, s_MaterialSampler, texCoord, lod);
    }

    if ((g_Material.flags & MaterialFlags_UseTransmissionTexture) != 0)
    {
        values.transmission = SampleTextureLevel(stfSamplerState, stfEnabled, t_Transmission, s_MaterialSampler, texCoord, lod);
    }

    if ((g_Material.flags & MaterialFlags_UseOpacityTexture) != 0)
    {
        values.opacity = SampleTextureLevel(stfSamplerState, stfEnabled, t_Opacity, s_MaterialSampler, texCoord, lod).x;
    }

    return values;
}

MaterialTextureSample SampleMaterialTexturesGrad(float2 texCoord, float2 ddx, float2 ddy, float2 pixelPosition)
{
    MaterialTextureSample values = DefaultMaterialTextures();

    STF_SamplerState stfSamplerState;
    InitSTF(stfSamplerState, pixelPosition);
    
#if STF_ENABLED
    bool stfEnabled = true;
    if (g_Const.stfSplitScreen && pixelPosition.x > (g_Const.view.viewportSize.x / 2.f))
    {
        stfEnabled = false;
    }
#else
    const bool stfEnabled = false;
#endif

    if ((g_Material.flags & MaterialFlags_UseBaseOrDiffuseTexture) != 0)
    {
        values.baseOrDiffuse = SampleTextureGrad(stfSamplerState, stfEnabled, t_BaseOrDiffuse, s_MaterialSampler, texCoord, ddx, ddy);
    }

    if ((g_Material.flags & MaterialFlags_UseMetalRoughOrSpecularTexture) != 0)
    {
        values.metalRoughOrSpecular = SampleTextureGrad(stfSamplerState, stfEnabled, t_MetalRoughOrSpecular, s_MaterialSampler, texCoord, ddx, ddy);
    }

    if ((g_Material.flags & MaterialFlags_UseEmissiveTexture) != 0)
    {
        values.emissive = SampleTextureGrad(stfSamplerState, stfEnabled, t_Emissive, s_MaterialSampler, texCoord, ddx, ddy);
    }

    if ((g_Material.flags & MaterialFlags_UseNormalTexture) != 0)
    {
        values.normal = SampleTextureGrad(stfSamplerState, stfEnabled, t_Normal, s_MaterialSampler, texCoord, ddx, ddy);
    }

    if ((g_Material.flags & MaterialFlags_UseOcclusionTexture) != 0)
    {
        values.occlusion = SampleTextureGrad(stfSamplerState, stfEnabled, t_Occlusion, s_MaterialSampler, texCoord, ddx, ddy);
    }

    if ((g_Material.flags & MaterialFlags_UseTransmissionTexture) != 0)
    {
        values.transmission = SampleTextureGrad(stfSamplerState, stfEnabled, t_Transmission, s_MaterialSampler, texCoord, ddx, ddy);
    }

    if ((g_Material.flags & MaterialFlags_UseOpacityTexture) != 0)
    {
        values.opacity = SampleTextureGrad(stfSamplerState, stfEnabled, t_Opacity, s_MaterialSampler, texCoord, ddx, ddy).x;
    }

    return values;
}

#endif