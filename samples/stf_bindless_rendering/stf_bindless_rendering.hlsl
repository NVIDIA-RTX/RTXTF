/***************************************************************************
 # Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#pragma pack_matrix(row_major)

#include <donut/shaders/bindless.h>
#include <donut/shaders/utils.hlsli>
#include <donut/shaders/binding_helpers.hlsli>
#include <donut/shaders/packing.hlsli>
#include <donut/shaders/surface.hlsli>
#include <donut/shaders/lighting.hlsli>
#include <donut/shaders/scene_material.hlsli>
#include "lighting_cb.h"
#include "rng.hlsli"

#include "../../libraries/RTXTF-Library/STFSamplerState.hlsli"

struct RayPayload
{
    float committedRayT;
    uint instanceID;
    uint primitiveIndex;
    uint geometryIndex;
    float2 barycentrics;
    uint2 pixelPosition;
};

ConstantBuffer<LightingConstants> g_Const : register(b0);

RWTexture2D<float4> u_Output : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);
StructuredBuffer<InstanceData> t_InstanceData : register(t1);
StructuredBuffer<GeometryData> t_GeometryData : register(t2);
StructuredBuffer<MaterialConstants> t_MaterialConstants : register(t3);
Texture2D<float4> STBN2DTexture : register(t4);

SamplerState s_MaterialSampler : register(s0);

VK_BINDING(0, 1) ByteAddressBuffer t_BindlessBuffers[] : register(t0, space1);
VK_BINDING(1, 1) Texture2D t_BindlessTextures[] : register(t0, space2);

RayDesc setupPrimaryRay(uint2 pixelPosition, PlanarViewConstants view)
{
    float2 uv = (float2(pixelPosition) + 0.5) * view.viewportSizeInv;
    float4 clipPos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.5, 1);
    float4 worldPos = mul(clipPos, view.matClipToWorld);
    worldPos.xyz /= worldPos.w;

    RayDesc ray;
    ray.Origin = view.cameraDirectionOrPosition.xyz;
    ray.Direction = normalize(worldPos.xyz - ray.Origin);
    ray.TMin = 0;
    ray.TMax = 1000;
    return ray;
}

RayDesc setupShadowRay(float3 surfacePos, float3 viewIncident)
{
    RayDesc ray;
    ray.Origin = surfacePos - viewIncident * 0.001;
    ray.Direction = -g_Const.light.direction;
    ray.TMin = 0;
    ray.TMax = 1000;
    return ray;
}

enum GeometryAttributes
{
    GeomAttr_Position   = 0x01,
    GeomAttr_TexCoord   = 0x02,
    GeomAttr_Normal     = 0x04,
    GeomAttr_Tangents   = 0x08,

    GeomAttr_All        = 0x0F
};

struct GeometrySample
{
    InstanceData instance;
    GeometryData geometry;
    MaterialConstants material;

    float3 vertexPositions[3];
    float2 vertexTexcoords[3];

    float3 objectSpacePosition;
    float2 texcoord;
    float3 flatNormal;
    float3 geometryNormal;
    float4 tangent;
};

GeometrySample getGeometryFromHit(
    uint instanceIndex,
    uint triangleIndex,
    uint geometryIndex,
    float2 rayBarycentrics,
    GeometryAttributes attributes,
    StructuredBuffer<InstanceData> instanceBuffer,
    StructuredBuffer<GeometryData> geometryBuffer,
    StructuredBuffer<MaterialConstants> materialBuffer)
{
    GeometrySample gs = (GeometrySample)0;

    gs.instance = instanceBuffer[instanceIndex];
    gs.geometry = geometryBuffer[gs.instance.firstGeometryIndex + geometryIndex];
    gs.material = materialBuffer[gs.geometry.materialIndex];

    ByteAddressBuffer indexBuffer = t_BindlessBuffers[NonUniformResourceIndex(gs.geometry.indexBufferIndex)];
    ByteAddressBuffer vertexBuffer = t_BindlessBuffers[NonUniformResourceIndex(gs.geometry.vertexBufferIndex)];

    float3 barycentrics;
    barycentrics.yz = rayBarycentrics;
    barycentrics.x = 1.0 - (barycentrics.y + barycentrics.z);

    uint3 indices = indexBuffer.Load3(gs.geometry.indexOffset + triangleIndex * c_SizeOfTriangleIndices);

    if (attributes & GeomAttr_Position)
    {
        gs.vertexPositions[0] = asfloat(vertexBuffer.Load3(gs.geometry.positionOffset + indices[0] * c_SizeOfPosition));
        gs.vertexPositions[1] = asfloat(vertexBuffer.Load3(gs.geometry.positionOffset + indices[1] * c_SizeOfPosition));
        gs.vertexPositions[2] = asfloat(vertexBuffer.Load3(gs.geometry.positionOffset + indices[2] * c_SizeOfPosition));
        gs.objectSpacePosition = interpolate(gs.vertexPositions, barycentrics);
    }

    if ((attributes & GeomAttr_TexCoord) && gs.geometry.texCoord1Offset != ~0u)
    {
        gs.vertexTexcoords[0] = asfloat(vertexBuffer.Load2(gs.geometry.texCoord1Offset + indices[0] * c_SizeOfTexcoord));
        gs.vertexTexcoords[1] = asfloat(vertexBuffer.Load2(gs.geometry.texCoord1Offset + indices[1] * c_SizeOfTexcoord));
        gs.vertexTexcoords[2] = asfloat(vertexBuffer.Load2(gs.geometry.texCoord1Offset + indices[2] * c_SizeOfTexcoord));
        gs.texcoord = interpolate(gs.vertexTexcoords, barycentrics);
    }

    if ((attributes & GeomAttr_Normal) && gs.geometry.normalOffset != ~0u)
    {
        float3 normals[3];
        normals[0] = Unpack_RGB8_SNORM(vertexBuffer.Load(gs.geometry.normalOffset + indices[0] * c_SizeOfNormal));
        normals[1] = Unpack_RGB8_SNORM(vertexBuffer.Load(gs.geometry.normalOffset + indices[1] * c_SizeOfNormal));
        normals[2] = Unpack_RGB8_SNORM(vertexBuffer.Load(gs.geometry.normalOffset + indices[2] * c_SizeOfNormal));
        gs.geometryNormal = interpolate(normals, barycentrics);
        gs.geometryNormal = mul(gs.instance.transform, float4(gs.geometryNormal, 0.0)).xyz;
        gs.geometryNormal = normalize(gs.geometryNormal);
    }

    if ((attributes & GeomAttr_Tangents) && gs.geometry.tangentOffset != ~0u)
    {
        float4 tangents[3];
        tangents[0] = Unpack_RGBA8_SNORM(vertexBuffer.Load(gs.geometry.tangentOffset + indices[0] * c_SizeOfNormal));
        tangents[1] = Unpack_RGBA8_SNORM(vertexBuffer.Load(gs.geometry.tangentOffset + indices[1] * c_SizeOfNormal));
        tangents[2] = Unpack_RGBA8_SNORM(vertexBuffer.Load(gs.geometry.tangentOffset + indices[2] * c_SizeOfNormal));
        gs.tangent.xyz = interpolate(tangents, barycentrics).xyz;
        gs.tangent.xyz = mul(gs.instance.transform, float4(gs.tangent.xyz, 0.0)).xyz;
        gs.tangent.xyz = normalize(gs.tangent.xyz);
        gs.tangent.w = tangents[0].w;
    }

    float3 objectSpaceFlatNormal = normalize(cross(
        gs.vertexPositions[1] - gs.vertexPositions[0],
        gs.vertexPositions[2] - gs.vertexPositions[0]));

    gs.flatNormal = normalize(mul(gs.instance.transform, float4(objectSpaceFlatNormal, 0.0)).xyz);

    return gs;
}

enum MaterialAttributes
{
    MatAttr_BaseColor    = 0x01,
    MatAttr_Emissive     = 0x02,
    MatAttr_Normal       = 0x04,
    MatAttr_MetalRough   = 0x08,
    MatAttr_Transmission = 0x10,

    MatAttr_All          = 0x1F
};

float4 SampleTexture(inout STF_SamplerState stfSamplerState, bool stfEnabled, Texture2D texture, SamplerState materialSampler, float2 texCoord, float mipLevel)
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

float4 SampleTextureGrad(inout STF_SamplerState stfSamplerState, bool stfEnabled, Texture2D texture, SamplerState materialSampler, float2 texCoord, float2 texGrad_x, float2 texGrad_y)
{
#if STF_ENABLED
    if (stfEnabled)
    {
        #if STF_LOAD
            return stfSamplerState.Texture2DLoadGrad(texture, texCoord, texGrad_x, texGrad_y);
        #else
            return stfSamplerState.Texture2DSampleGrad(texture, materialSampler, texCoord, texGrad_x, texGrad_y);
        #endif
    }
#endif
    {
        return texture.SampleGrad(materialSampler, texCoord, texGrad_x, texGrad_y);
    }
}

MaterialSample sampleGeometryMaterial(
    GeometrySample gs, 
    float2 texGrad_x, 
    float2 texGrad_y, 
    bool forceMipLevel, // false - aniso, true - explicit mip level
    float mipLevel,
    MaterialAttributes attributes, 
    SamplerState materialSampler,
    uint2 pixelPosition)
{
    MaterialTextureSample textures = DefaultMaterialTextures();

    float3 u = RNG::SpatioTemporalBlueNoise2D(pixelPosition, g_Const.stfFrameIndex, STBN2DTexture);
    if (g_Const.stfUseWhiteNoise)
        u = RNG::SpatioTemporalWhiteNoise3D(pixelPosition, g_Const.stfFrameIndex);
    
    STF_SamplerState samplerState = STF_SamplerState::Create(float4(u.x, u.y, 0 /*slice - unused*/, u.z));
    samplerState.SetFilterType(g_Const.stfFilterMode);
    samplerState.SetFrameIndex(g_Const.stfFrameIndex);
    samplerState.SetMagMethod(g_Const.stfMagnificationMethod);
    samplerState.SetAddressingModes(g_Const.stfAddressMode.xxx);
    samplerState.SetSigma(g_Const.stfSigma);
    samplerState.SetAnisoMethod(g_Const.stfMinificationMethod);
    samplerState.SetReseedOnSample(g_Const.stfReseedOnSample);
    
#if STF_ENABLED
    bool stfEnabled = true;
    if (g_Const.stfSplitScreen && pixelPosition.x > (g_Const.view.viewportSize.x / 2.f))
    {
        stfEnabled = false;
    }
#else
    const bool stfEnabled = false;
#endif

    if ((attributes & MatAttr_BaseColor) && (gs.material.baseOrDiffuseTextureIndex >= 0) && (gs.material.flags & MaterialFlags_UseBaseOrDiffuseTexture) != 0)
    {
        Texture2D diffuseTexture = t_BindlessTextures[NonUniformResourceIndex(gs.material.baseOrDiffuseTextureIndex)];

        if (forceMipLevel)
        {
            textures.baseOrDiffuse = SampleTexture(samplerState, stfEnabled, diffuseTexture, materialSampler, gs.texcoord, mipLevel);
        }
        else
        {
            textures.baseOrDiffuse = SampleTextureGrad(samplerState, stfEnabled, diffuseTexture, materialSampler, gs.texcoord, texGrad_x, texGrad_y);
        }
    }

    if ((attributes & MatAttr_Emissive) && (gs.material.emissiveTextureIndex >= 0) && (gs.material.flags & MaterialFlags_UseEmissiveTexture) != 0)
    {
        Texture2D emissiveTexture = t_BindlessTextures[NonUniformResourceIndex(gs.material.emissiveTextureIndex)];

        if (forceMipLevel)
        {
            textures.emissive = SampleTexture(samplerState, stfEnabled, emissiveTexture, materialSampler, gs.texcoord, mipLevel);
        }
        else
        {
            textures.emissive = SampleTextureGrad(samplerState, stfEnabled, emissiveTexture, materialSampler, gs.texcoord, texGrad_x, texGrad_y);
        }
    }
    
    if ((attributes & MatAttr_Normal) && (gs.material.normalTextureIndex >= 0) && (gs.material.flags & MaterialFlags_UseNormalTexture) != 0)
    {
        Texture2D normalsTexture = t_BindlessTextures[NonUniformResourceIndex(gs.material.normalTextureIndex)];

        if (forceMipLevel)
        {
            textures.normal = SampleTexture(samplerState, stfEnabled, normalsTexture, materialSampler, gs.texcoord, mipLevel);
        }
        else
        {
            textures.normal = SampleTextureGrad(samplerState, stfEnabled, normalsTexture, materialSampler, gs.texcoord, texGrad_x, texGrad_y);
        }
    }

    if ((attributes & MatAttr_MetalRough) && (gs.material.metalRoughOrSpecularTextureIndex >= 0) && (gs.material.flags & MaterialFlags_UseMetalRoughOrSpecularTexture) != 0)
    {
        Texture2D specularTexture = t_BindlessTextures[NonUniformResourceIndex(gs.material.metalRoughOrSpecularTextureIndex)];

        if (forceMipLevel)
        {
            textures.metalRoughOrSpecular = SampleTexture(samplerState, stfEnabled, specularTexture, materialSampler, gs.texcoord, mipLevel);
        }
        else
        {
            textures.metalRoughOrSpecular = SampleTextureGrad(samplerState, stfEnabled, specularTexture, materialSampler, gs.texcoord, texGrad_x, texGrad_y);
        }
    }

    if ((attributes & MatAttr_Transmission) && (gs.material.transmissionTextureIndex >= 0) && (gs.material.flags & MaterialFlags_UseTransmissionTexture) != 0)
    {
        Texture2D transmissionTexture = t_BindlessTextures[NonUniformResourceIndex(gs.material.transmissionTextureIndex)];

        if (forceMipLevel)
        {
            textures.transmission = SampleTexture(samplerState, stfEnabled, transmissionTexture, materialSampler, gs.texcoord, mipLevel);
        }
        else
        {
            textures.transmission = SampleTextureGrad(samplerState, stfEnabled, transmissionTexture, materialSampler, gs.texcoord, texGrad_x, texGrad_y);
        }
    }

    return EvaluateSceneMaterial(gs.geometryNormal, gs.tangent, gs.material, textures);
}

bool considerTransparentMaterial(uint instanceIndex, uint triangleIndex, uint geometryIndex, float2 rayBarycentrics, uint2 pixelPosition)
{
    GeometrySample gs = getGeometryFromHit(instanceIndex, triangleIndex, geometryIndex, rayBarycentrics,
        GeomAttr_TexCoord, t_InstanceData, t_GeometryData, t_MaterialConstants);

    if (gs.material.domain != MaterialDomain_AlphaTested)
        return true;

    const bool forceMipLevel = true;
    const int mipLevel = 0;
    MaterialSample ms = sampleGeometryMaterial(gs, 0, 0, forceMipLevel, mipLevel, MatAttr_BaseColor | MatAttr_Transmission, s_MaterialSampler, pixelPosition);

    bool alphaMask = ms.opacity >= gs.material.alphaCutoff;

    return alphaMask;
}

void traceRay(RayDesc ray, inout RayPayload payload, uint2 pixelPosition)
{
    payload.instanceID = ~0u;
    payload.pixelPosition = pixelPosition;

#if USE_RAY_QUERY

    RayQuery<RAY_FLAG_NONE> rayQuery;
    rayQuery.TraceRayInline(SceneBVH, RAY_FLAG_NONE, 0xff, ray);

    while (rayQuery.Proceed())
    {
        if (rayQuery.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        {
            if (considerTransparentMaterial(
                rayQuery.CandidateInstanceID(),
                rayQuery.CandidatePrimitiveIndex(),
                rayQuery.CandidateGeometryIndex(),
                rayQuery.CandidateTriangleBarycentrics(),
                pixelPosition))
            {
                rayQuery.CommitNonOpaqueTriangleHit();
            }
        }
    }

    if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        payload.instanceID = rayQuery.CommittedInstanceID();
        payload.primitiveIndex = rayQuery.CommittedPrimitiveIndex();
        payload.geometryIndex = rayQuery.CommittedGeometryIndex();
        payload.barycentrics = rayQuery.CommittedTriangleBarycentrics();
        payload.committedRayT = rayQuery.CommittedRayT();
    }

#else // !USE_RAY_QUERY

    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);

#endif
}

float3 shadeSurface(
    uint2 pixelPosition,
    RayPayload payload,
    float3 viewDirection)
{
    GeometrySample gs = getGeometryFromHit(payload.instanceID, payload.primitiveIndex, payload.geometryIndex, payload.barycentrics, 
        GeomAttr_All, t_InstanceData, t_GeometryData, t_MaterialConstants);
    
    RayDesc ray_0 = setupPrimaryRay(pixelPosition, g_Const.view);
    RayDesc ray_x = setupPrimaryRay(pixelPosition + uint2(1, 0), g_Const.view);
    RayDesc ray_y = setupPrimaryRay(pixelPosition + uint2(0, 1), g_Const.view);
    float3 worldSpacePositions[3];
    worldSpacePositions[0] = mul(gs.instance.transform, float4(gs.vertexPositions[0], 1.0)).xyz;
    worldSpacePositions[1] = mul(gs.instance.transform, float4(gs.vertexPositions[1], 1.0)).xyz;
    worldSpacePositions[2] = mul(gs.instance.transform, float4(gs.vertexPositions[2], 1.0)).xyz;
    float3 bary_0 = computeRayIntersectionBarycentrics(worldSpacePositions, ray_0.Origin, ray_0.Direction);
    float3 bary_x = computeRayIntersectionBarycentrics(worldSpacePositions, ray_x.Origin, ray_x.Direction);
    float3 bary_y = computeRayIntersectionBarycentrics(worldSpacePositions, ray_y.Origin, ray_y.Direction);
    float2 texcoord_0 = interpolate(gs.vertexTexcoords, bary_0);
    float2 texcoord_x = interpolate(gs.vertexTexcoords, bary_x);
    float2 texcoord_y = interpolate(gs.vertexTexcoords, bary_y);
    float2 texGrad_x = texcoord_x - texcoord_0;
    float2 texGrad_y = texcoord_y - texcoord_0;
    
    
    const bool forceMipLevel = g_Const.stfUseMipLevelOverride == 1;
    const float mipLevel = g_Const.stfMipLevelOverride;

    MaterialSample ms = sampleGeometryMaterial(gs, texGrad_x, texGrad_y, forceMipLevel, mipLevel, MatAttr_All, s_MaterialSampler, pixelPosition);

    ms.shadingNormal = getBentNormal(gs.flatNormal, ms.shadingNormal, viewDirection);

    float3 worldPos = mul(gs.instance.transform, float4(gs.objectSpacePosition, 1.0)).xyz;

    float3 diffuseTerm = 0, specularTerm = 0;

    RayDesc shadowRay = setupShadowRay(worldPos, viewDirection);

    payload.instanceID = ~0u;
    traceRay(shadowRay, payload, pixelPosition);
    if (payload.instanceID == ~0u)
    {
        ShadeSurface(g_Const.light, ms, worldPos, viewDirection, diffuseTerm, specularTerm);
    }

    return diffuseTerm + specularTerm + (ms.diffuseAlbedo + ms.specularF0) * g_Const.ambientColor.rgb;
}

float4 GetCividis(int index, int min, int max)
{
    const float t = (float)index / (max - min);

    const float t2 = t * t;
    const float t3 = t2 * t;
    
    float4 color;
    color.r = 0.8688 * t3 - 1.5484 * t2 + 0.0081 * t + 0.2536;
    color.g = 0.8353 * t3 - 1.6375 * t2 + 0.2351 * t + 0.8669;
    color.b = 0.6812 * t3 - 1.0197 * t2 + 0.3935 * t + 0.8815;
    color.a = 0.f;
    
    return color;
}

// Row-linear warp lane shuffle.
// the global pixelPosition (SV_DispatchThreadID or DispatchRaysIndex())
// will be adjusted so that the threads within a warp are laid out the following way:
//  ----------------------------------------------------------------
// |  0  1 |  2  3 |  4   5 | 6   7 | 8   9 | 10 11 | 12 13 | 14 15 | 
// | 16 17 | 18 19 | 20  21 | 22 23 | 24 25 | 26 28 | 28 29 | 30 31 | 
//  ----------------------------------------------------------------

uint2 MakeRowLinearLanes_16x2(uint2 pixelPosition)
{
    const uint2 pixelPositionBase = uint2(pixelPosition.x & 0xFFFFFFF0, pixelPosition.y & 0xFFFFFFFE); // i.e warps map to 16x2
   
    const uint waveIndex = WaveGetLaneIndex();
    const uint waveOffsetQuadX = waveIndex & 0xF;
    const uint waveOffsetQuadY = waveIndex >> 4u;

    const uint2 waveOffset = uint2(waveOffsetQuadX, waveOffsetQuadY);
    
    return pixelPositionBase + waveOffset;
}

// Quad-swizzled lane shuffle.
// the global pixelPosition (SV_DispatchThreadID or DispatchRaysIndex())
// will be adjusted so that the threads within a warp are laid out the following way:
//  -----------------------------------------------------------
// | 0 1 | 4 5 | 8   9 | 12 13 | 16 17 | 20 21 | 24 25 | 28 30 | 
// | 2 3 | 6 7 | 10 11 | 14 15 | 18 19 | 22 23 | 26 27 | 29 31 |
//  -----------------------------------------------------------

uint2 MakeQuadZLanes_16x2(uint2 pixelPosition)
{
    const uint2 pixelPositionBase = uint2(pixelPosition.x & 0xFFFFFFF0, pixelPosition.y & 0xFFFFFFFE); // i.e warps map to 16x2
   
    const uint waveIndex = WaveGetLaneIndex();
    const uint waveOffsetQuadX  = (waveIndex >> 2u) << 1u;
    const uint quadOffsetX      = (waveIndex & 1u);
    const uint waveOffsetQuadY  = (waveIndex >> 1u) & 1u;

    const uint2 waveOffset      = uint2(waveOffsetQuadX + quadOffsetX, waveOffsetQuadY);
    
    return pixelPositionBase + waveOffset;
}

// main entrypoint, called either from CS or RGS.
void main(uint2 pixelPosition)
{
#if STF_ENABLED
    if (g_Const.stfSplitScreen)
    {
        const float2 pixelPos = pixelPosition * g_Const.view.viewportSizeInv;
        if (pixelPos.x > 0.499f && pixelPos.x < 0.501f)
        {
            u_Output[pixelPosition] = float4(1, 0, 0, 0);
            return;
        }
    }
#endif

    if (g_Const.stfWaveLaneLayoutOverride == 0 /*None*/)
    {
        // noop - let driver / implementation decide.
    }
    else if (g_Const.stfWaveLaneLayoutOverride == 1)
    {
        pixelPosition = MakeRowLinearLanes_16x2(pixelPosition);
    }
    else if (g_Const.stfWaveLaneLayoutOverride == 2)
    {
        pixelPosition = MakeQuadZLanes_16x2(pixelPosition);
    }
    
    RayDesc ray = setupPrimaryRay(pixelPosition, g_Const.view);

    RayPayload payload = (RayPayload)0;
    traceRay(ray, payload, pixelPosition);

    if (payload.instanceID == ~0u)
    {
        u_Output[pixelPosition] = 0;
        return;
    }

    u_Output[pixelPosition] = float4(shadeSurface(pixelPosition, payload, ray.Direction), 0);
    
    if (g_Const.stfDebugVisualizeLanes)
    {
        u_Output[pixelPosition] = GetCividis(WaveGetLaneIndex(), 0, WaveGetLaneCount());
    }
}
