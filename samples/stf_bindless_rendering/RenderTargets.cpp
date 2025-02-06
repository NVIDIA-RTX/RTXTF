/***************************************************************************
 # Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#include "RenderTargets.h"

#include <donut/engine/FramebufferFactory.h>

using namespace dm;
using namespace donut;

RenderTargets::RenderTargets(nvrhi::IDevice* device, int2 size)
    : Size(size)
{
    nvrhi::TextureDesc desc;
    desc.width = size.x;
    desc.height = size.y;
    desc.keepInitialState = true;

    // Render targets

    desc.isUAV = false;
    desc.isRenderTarget = true;
    desc.initialState = nvrhi::ResourceStates::RenderTarget;

    desc.format = nvrhi::Format::SRGBA8_UNORM;
    desc.debugName = "LdrColor";
    LdrColor = device->createTexture(desc);

    LdrFramebuffer = std::make_shared<engine::FramebufferFactory>(device);
    LdrFramebuffer->RenderTargets = { LdrColor };
    
    desc.format = nvrhi::Format::D32;
    desc.debugName = "DeviceDepth";
    desc.initialState = nvrhi::ResourceStates::DepthWrite;
    desc.clearValue = 0.f;
    desc.useClearValue = true;
    DeviceDepth = device->createTexture(desc);

    // G-buffer targets

    desc.isUAV = true;
    desc.initialState = nvrhi::ResourceStates::UnorderedAccess;

    desc.format = nvrhi::Format::R32_FLOAT;
#define BACKGROUND_DEPTH 65504.f
    desc.clearValue = BACKGROUND_DEPTH;
    desc.debugName = "DepthBuffer";
    Depth = device->createTexture(desc);
    desc.debugName = "PrevDepthBuffer";
    PrevDepth = device->createTexture(desc);

    desc.useClearValue = false;
    desc.clearValue = 0.f;

    desc.format = nvrhi::Format::R32_FLOAT;
    desc.debugName = "DeviceDepthUAV";
    DeviceDepthUAV = device->createTexture(desc);

    desc.format = nvrhi::Format::R32_UINT;
    desc.debugName = "GBufferDiffuseAlbedo";
    GBufferDiffuseAlbedo = device->createTexture(desc);
    desc.debugName = "PrevGBufferDiffuseAlbedo";
    PrevGBufferDiffuseAlbedo = device->createTexture(desc);

    desc.format = nvrhi::Format::R32_UINT;
    desc.debugName = "GBufferSpecularRough";
    GBufferSpecularRough = device->createTexture(desc);
    desc.debugName = "PrevGBufferSpecularRough";
    PrevGBufferSpecularRough = device->createTexture(desc);

    desc.format = nvrhi::Format::R32_UINT;
    desc.debugName = "GBufferNormals";
    GBufferNormals = device->createTexture(desc);
    desc.debugName = "PrevGBufferNormals";
    PrevGBufferNormals = device->createTexture(desc);
    
    desc.format = nvrhi::Format::R32_UINT;
    desc.debugName = "GBufferGeoNormals";
    GBufferGeoNormals = device->createTexture(desc);
    desc.debugName = "PrevGBufferGeoNormals";
    PrevGBufferGeoNormals = device->createTexture(desc);

    desc.format = nvrhi::Format::RGBA8_UNORM;
    desc.debugName = "NormalRoughness";
    NormalRoughness = device->createTexture(desc);

    desc.format = nvrhi::Format::RGBA16_FLOAT;
    desc.debugName = "GBufferEmissive";
    GBufferEmissive = device->createTexture(desc);

    desc.format = nvrhi::Format::RGBA16_FLOAT;
    desc.debugName = "MotionVectors";
    MotionVectors = device->createTexture(desc);

    desc.format = nvrhi::Format::RGBA16_FLOAT;
    desc.debugName = "ResolvedColor";
    ResolvedColor = device->createTexture(desc);

    desc.format = nvrhi::Format::RGBA16_FLOAT;
    desc.debugName = "ReferenceColor";
    ReferenceColor = device->createTexture(desc);

    GBufferFramebuffer = std::make_shared<engine::FramebufferFactory>(device);
    GBufferFramebuffer->DepthTarget = DeviceDepth;
    GBufferFramebuffer->RenderTargets = {
        Depth,
        GBufferDiffuseAlbedo,
        GBufferSpecularRough,
        GBufferNormals,
        GBufferGeoNormals,
        GBufferEmissive,
        MotionVectors
    };

    PrevGBufferFramebuffer = std::make_shared<engine::FramebufferFactory>(device);
    PrevGBufferFramebuffer->DepthTarget = DeviceDepth;
    PrevGBufferFramebuffer->RenderTargets = {
        PrevDepth,
        PrevGBufferDiffuseAlbedo,
        PrevGBufferSpecularRough,
        PrevGBufferNormals,
        PrevGBufferGeoNormals,
        GBufferEmissive,
        MotionVectors
    };

    ResolvedFramebuffer = std::make_shared<engine::FramebufferFactory>(device);
    ResolvedFramebuffer->RenderTargets = { ResolvedColor };

    // UAV-only textures

    desc.isRenderTarget = false;

    desc.format = nvrhi::Format::RGBA16_FLOAT;
    desc.debugName = "HdrColor";
    HdrColor = device->createTexture(desc);

    desc.format = nvrhi::Format::RGBA16_SNORM;
    desc.debugName = "TemporalFeedback1";
    TaaFeedback1 = device->createTexture(desc);
    desc.debugName = "TemporalFeedback2";
    TaaFeedback2 = device->createTexture(desc);
}

bool RenderTargets::IsUpdateRequired(int2 size)
{
    if (any(Size != size))
        return true;

    return false;
}

void RenderTargets::NextFrame()
{
    std::swap(Depth, PrevDepth);
    std::swap(GBufferDiffuseAlbedo, PrevGBufferDiffuseAlbedo);
    std::swap(GBufferSpecularRough, PrevGBufferSpecularRough);
    std::swap(GBufferNormals, PrevGBufferNormals);
    std::swap(GBufferGeoNormals, PrevGBufferGeoNormals);
    std::swap(GBufferFramebuffer, PrevGBufferFramebuffer);
    std::swap(DiffuseConfidence, PrevDiffuseConfidence);
    std::swap(SpecularConfidence, PrevSpecularConfidence);
}
