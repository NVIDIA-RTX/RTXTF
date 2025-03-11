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

#include <donut/shaders/gbuffer_cb.h>

// Declare the constants that drive material bindings in 'material_bindings.hlsli'
// to match the bindings explicitly declared in 'gbuffer_cb.h'.

#define MATERIAL_REGISTER_SPACE     GBUFFER_SPACE_MATERIAL
#define MATERIAL_CB_SLOT            GBUFFER_BINDING_MATERIAL_CONSTANTS
#define MATERIAL_DIFFUSE_SLOT       GBUFFER_BINDING_MATERIAL_DIFFUSE_TEXTURE
#define MATERIAL_SPECULAR_SLOT      GBUFFER_BINDING_MATERIAL_SPECULAR_TEXTURE
#define MATERIAL_NORMALS_SLOT       GBUFFER_BINDING_MATERIAL_NORMAL_TEXTURE
#define MATERIAL_EMISSIVE_SLOT      GBUFFER_BINDING_MATERIAL_EMISSIVE_TEXTURE
#define MATERIAL_OCCLUSION_SLOT     GBUFFER_BINDING_MATERIAL_OCCLUSION_TEXTURE
#define MATERIAL_TRANSMISSION_SLOT  GBUFFER_BINDING_MATERIAL_TRANSMISSION_TEXTURE
#define MATERIAL_OPACITY_SLOT       GBUFFER_BINDING_MATERIAL_OPACITY_TEXTURE

#define MATERIAL_SAMPLER_REGISTER_SPACE GBUFFER_SPACE_VIEW
#define MATERIAL_SAMPLER_SLOT           GBUFFER_BINDING_MATERIAL_SAMPLER

#include <donut/shaders/scene_material.hlsli>
#include "material_bindings_stf.hlsli"
#include <donut/shaders/motion_vectors.hlsli>
#include <donut/shaders/forward_vertex.hlsli>
#include <donut/shaders/binding_helpers.hlsli>

void main(
    in float4 i_position : SV_Position,
	in SceneVertex i_vtx,
    in bool i_isFrontFace : SV_IsFrontFace,
    out float4 o_channel0 : SV_Target1,
    out float4 o_channel1 : SV_Target2,
    out float4 o_channel2 : SV_Target3,
    out float4 o_channel3 : SV_Target4
#if MOTION_VECTORS
    , out float3 o_motion : SV_Target5
#endif
)
{
    float2 pixelPosition = i_position.xy * g_Const.view.viewportSizeInv;

#if STF_ENABLED
    if (g_Const.stfSplitScreen)
    {
        if (pixelPosition.x > 0.499f && pixelPosition.x < 0.501f)
        {
            o_channel0 = float4(1, 0, 0, 0);
            o_channel1 = float4(1, 0, 0, 0);
            o_channel2 = float4(1, 0, 0, 0);
            o_channel3 = float4(1, 0, 0, 0);
            return;
        }
    }
#endif

    MaterialTextureSample textures = SampleMaterialTextures(i_vtx.texCoord, g_Material.normalTextureTransformScale, i_position.xy);

    MaterialSample surface = EvaluateSceneMaterial(i_vtx.normal, i_vtx.tangent, g_Material, textures);

#if ALPHA_TESTED
    if (g_Material.domain != MaterialDomain_Opaque)
        clip(surface.opacity - g_Material.alphaCutoff);
#endif

    if (!i_isFrontFace)
        surface.shadingNormal = -surface.shadingNormal;

    o_channel0.xyz = surface.diffuseAlbedo;
    o_channel0.w = surface.opacity;
    o_channel1.xyz = surface.specularF0;
    o_channel1.w = surface.occlusion;
    o_channel2.xyz = surface.shadingNormal;
    o_channel2.w = surface.roughness;
    o_channel3.xyz = surface.emissiveColor;
    o_channel3.w = 0;

#if MOTION_VECTORS
    o_motion = GetMotionVector(i_position.xyz, i_vtx.prevPos, g_Const.view, g_Const.viewPrev);
#endif
}
