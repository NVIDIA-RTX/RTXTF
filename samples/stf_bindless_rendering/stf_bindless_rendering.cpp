/***************************************************************************
 # Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#include <donut/app/ApplicationBase.h>
#include <donut/app/Camera.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/render/ToneMappingPasses.h>
#include <donut/engine/TextureCache.h>
#include <donut/engine/Scene.h>
#include <donut/engine/BindingCache.h>
#include <donut/engine/View.h>
#include <donut/app/DeviceManager.h>
#include <donut/core/log.h>
#include <donut/core/vfs/VFS.h>
#include <donut/core/math/math.h>
#include <nvrhi/utils.h>
#include "RenderTargets.h"
#include "donut/engine/FramebufferFactory.h"
#include "donut/render/GBufferFillPass.h"
#include "donut/render/GBuffer.h"
#include <donut/render/DrawStrategy.h>
#include "../../libraries/RTXTF-Library/STFDefinitions.h"
#include <donut/render/DeferredLightingPass.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/MaterialBindingCache.h>
#include <donut/render/DepthPass.h>
#include <donut/render/CascadedShadowMap.h>
#include "UserInterface.h"

#if ENABLE_DLSS
#include "DLSS.h"
#endif

using namespace donut;
using namespace donut::render;
using namespace donut::engine;
using namespace donut::math;
using namespace donut::app;

#include "lighting_cb.h"
#include <donut/shaders/gbuffer_cb.h>

static const char* g_WindowTitle = "Donut Example: RTX Texture Filtering";

// Override GBuffer shaders
class GBufferFillPassWithSTF : public GBufferFillPass
{
    nvrhi::BufferHandle m_ConstantBuffer;
    std::shared_ptr<UIData> m_ui;
    std::shared_ptr<LoadedTexture> m_STBNTexture;

    using GBufferFillPass::GBufferFillPass;

public:
    void UpdateLightConstantBuffer(nvrhi::ICommandList* commandList, LightingConstants constants)
    {
        commandList->writeBuffer(m_ConstantBuffer, &constants, sizeof(constants));
    }

    GBufferFillPassWithSTF(nvrhi::IDevice* device, std::shared_ptr<CommonRenderPasses> commonPasses, std::shared_ptr<UIData> ui, std::shared_ptr<LoadedTexture> STBNTexture) :
        m_ui(ui),
        GBufferFillPass(device, commonPasses)
    {
        m_STBNTexture = STBNTexture;

        m_ConstantBuffer = device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(
            sizeof(LightingConstants), "LightingConstants", c_MaxRenderPassConstantBufferVersions));
    }

protected:

    nvrhi::ShaderHandle CreatePixelShader(ShaderFactory& shaderFactory, const CreateParameters& params, bool alphaTested) override
    {
        std::vector<ShaderMacro> PixelShaderMacros;
        PixelShaderMacros.push_back(ShaderMacro("STF_ENABLED", m_ui->samplerType >= SamplerType::STF ? "1" : "0"));
        PixelShaderMacros.push_back(ShaderMacro("STF_LOAD", m_ui->stfLoad == true ? "1" : "0"));
        PixelShaderMacros.push_back(ShaderMacro("MOTION_VECTORS", params.enableMotionVectors ? "1" : "0"));
        PixelShaderMacros.push_back(ShaderMacro("ALPHA_TESTED", alphaTested ? "1" : "0"));

        return shaderFactory.CreateAutoShader("app/gbuffer_stf_ps.hlsl", "main", DONUT_MAKE_PLATFORM_SHADER(g_gbuffer_stf_ps), &PixelShaderMacros, nvrhi::ShaderType::Pixel);
    }

    nvrhi::ShaderHandle CreateVertexShader(ShaderFactory& shaderFactory, const CreateParameters& params)
    {
        char const* sourceFileName = "app/gbuffer_stf_vs.hlsl";

        std::vector<ShaderMacro> VertexShaderMacros;
        VertexShaderMacros.push_back(ShaderMacro("MOTION_VECTORS", params.enableMotionVectors ? "1" : "0"));

        if (params.useInputAssembler)
        {
            return shaderFactory.CreateAutoShader(sourceFileName, "input_assembler",
                DONUT_MAKE_PLATFORM_SHADER(g_gbuffer_vs_input_assembler), &VertexShaderMacros, nvrhi::ShaderType::Vertex);
        }
        else
        {
            return shaderFactory.CreateAutoShader(sourceFileName, "buffer_loads",
                DONUT_MAKE_PLATFORM_SHADER(g_gbuffer_vs_buffer_loads), &VertexShaderMacros, nvrhi::ShaderType::Vertex);
        }
    }

    void CreateViewBindings(nvrhi::BindingLayoutHandle& layout, nvrhi::BindingSetHandle& set, const CreateParameters& params) override
    {
        auto bindingLayoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel)
            .setRegisterSpace(m_IsDX11 ? 0 : GBUFFER_SPACE_VIEW)
            .setRegisterSpaceIsDescriptorSet(!m_IsDX11)
            .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(GBUFFER_BINDING_VIEW_CONSTANTS))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(GBUFFER_BINDING_MATERIAL_SAMPLER));

        layout = m_Device->createBindingLayout(bindingLayoutDesc);

        auto bindingSetDesc = nvrhi::BindingSetDesc()
            .setTrackLiveness(params.trackLiveness)
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(GBUFFER_BINDING_VIEW_CONSTANTS, m_ConstantBuffer))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_STBNTexture->texture))
            .addItem(nvrhi::BindingSetItem::Sampler(GBUFFER_BINDING_MATERIAL_SAMPLER,
                m_CommonPasses->m_AnisotropicWrapSampler));

        set = m_Device->createBindingSet(bindingSetDesc, layout);
    }

    std::shared_ptr<MaterialBindingCache> CreateMaterialBindingCache(CommonRenderPasses& commonPasses) override
    {
        std::vector<MaterialResourceBinding> materialBindings = {
            { MaterialResource::ConstantBuffer,         GBUFFER_BINDING_MATERIAL_CONSTANTS },
            { MaterialResource::DiffuseTexture,         GBUFFER_BINDING_MATERIAL_DIFFUSE_TEXTURE },
            { MaterialResource::SpecularTexture,        GBUFFER_BINDING_MATERIAL_SPECULAR_TEXTURE },
            { MaterialResource::NormalTexture,          GBUFFER_BINDING_MATERIAL_NORMAL_TEXTURE },
            { MaterialResource::EmissiveTexture,        GBUFFER_BINDING_MATERIAL_EMISSIVE_TEXTURE },
            { MaterialResource::OcclusionTexture,       GBUFFER_BINDING_MATERIAL_OCCLUSION_TEXTURE },
            { MaterialResource::TransmissionTexture,    GBUFFER_BINDING_MATERIAL_TRANSMISSION_TEXTURE },
            { MaterialResource::OpacityTexture,         GBUFFER_BINDING_MATERIAL_OPACITY_TEXTURE }
        };

        return std::make_shared<MaterialBindingCache>(
            m_Device,
            nvrhi::ShaderType::Pixel,
            /* registerSpace = */ m_IsDX11 ? 0 : GBUFFER_SPACE_MATERIAL,
            /* registerSpaceIsDescriptorSet = */ !m_IsDX11,
            materialBindings,
            commonPasses.m_AnisotropicWrapSampler,
            commonPasses.m_GrayTexture,
            commonPasses.m_BlackTexture);
    }

    void SetupView(GeometryPassContext& abstractContext, nvrhi::ICommandList* commandList, const engine::IView* view, const engine::IView* viewPrev) override
    {
        auto& context = static_cast<Context&>(abstractContext);

        context.keyTemplate.bits.frontCounterClockwise = view->IsMirrored();
        context.keyTemplate.bits.reverseDepth = view->IsReverseDepth();
    }
};

class BindlessRayTracing : public app::ApplicationBase
{
private:

    std::shared_ptr<UIData> m_ui;
	std::shared_ptr<vfs::RootFileSystem> m_RootFS;

    nvrhi::ShaderLibraryHandle m_ShaderLibraryLib;
    nvrhi::ShaderLibraryHandle m_ShaderLibraryHitGroup;
    nvrhi::rt::PipelineHandle m_RayPipeline;
    nvrhi::rt::ShaderTableHandle m_ShaderTable;
    nvrhi::ShaderHandle m_ComputeShader;
    nvrhi::ShaderHandle m_PixelShader;
    nvrhi::ComputePipelineHandle m_ComputePipeline;
    nvrhi::CommandListHandle m_CommandList;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::BindingSetHandle m_BindingSet;
    nvrhi::BindingLayoutHandle m_BindlessLayout;
    nvrhi::BufferHandle m_rayTracingConstantBuffer;

    nvrhi::rt::AccelStructHandle m_TopLevelAS;
    std::shared_ptr<LoadedTexture> m_STBNTexture;

    std::shared_ptr<ShaderFactory> m_ShaderFactory;
    std::shared_ptr<DescriptorTableManager> m_DescriptorTable;
    std::unique_ptr<Scene> m_Scene;
    nvrhi::TextureHandle m_ColorBuffer;
    app::FirstPersonCamera m_Camera;
    PlanarView m_View;
    PlanarView m_ViewPrevious;
    std::shared_ptr<DirectionalLight> m_SunLight;
    std::shared_ptr<CascadedShadowMap>  m_ShadowMap;
    std::shared_ptr<FramebufferFactory> m_ShadowFramebuffer;
    std::shared_ptr<DepthPass>          m_ShadowDepthPass;
    std::shared_ptr<InstancedOpaqueDrawStrategy> m_OpaqueDrawStrategy;
    std::unique_ptr<BindingCache> m_BindingCache;

    std::unique_ptr<RenderTargets> m_RenderTargets;
    std::unique_ptr<GBufferFillPassWithSTF> m_GBufferPass;
    std::unique_ptr<DeferredLightingPass> m_DeferredLightingPass;
    std::unique_ptr<ToneMappingPass> m_ToneMappingPass;

    std::unique_ptr<TemporalAntiAliasingPass> m_TemporalPass;
    float4 m_ambientColor = float4(0.2f);

    float m_WallclockTime = 0.f;
    uint m_FrameIndex = 0;
    bool m_PreviousViewsValid = false;

#if ENABLE_DLSS
    std::unique_ptr<DLSS> m_DLSS;
#endif

public:
    using ApplicationBase::ApplicationBase;

    std::shared_ptr<vfs::RootFileSystem> GetRootFs()
    {
        return m_RootFS;
    }

    std::shared_ptr<ShaderFactory> GetShaderFactory()
    {
        return m_ShaderFactory;
    }

    std::shared_ptr<UIData> GetUI()
    {
        return m_ui;
    }

    bool Init(bool useRayQuery)
    {
        std::filesystem::path sceneFileName = app::GetDirectoryWithExecutable().parent_path() / "assets/media/sponza-plus.scene.json";
        std::filesystem::path frameworkShaderPath = app::GetDirectoryWithExecutable() / "shaders/framework" / app::GetShaderTypeName(GetDevice()->getGraphicsAPI());
        std::filesystem::path appShaderPath = app::GetDirectoryWithExecutable() / "shaders/stf_bindless_rendering" / app::GetShaderTypeName(GetDevice()->getGraphicsAPI());
        std::filesystem::path mediaPath = app::GetDirectoryWithExecutable().parent_path() / "assets/media";

		m_RootFS = std::make_shared<vfs::RootFileSystem>();
		m_RootFS->mount("/shaders/donut", frameworkShaderPath);
		m_RootFS->mount("/shaders/app", appShaderPath);
        m_RootFS->mount("/assets/media", mediaPath);

        m_ui = std::make_shared<UIData>();

		m_ShaderFactory = std::make_shared<ShaderFactory>(GetDevice(), m_RootFS, "/shaders");
		m_CommonPasses = std::make_shared<CommonRenderPasses>(GetDevice(), m_ShaderFactory);
        m_BindingCache = std::make_unique<BindingCache>(GetDevice());

        nvrhi::BindlessLayoutDesc bindlessLayoutDesc;
        bindlessLayoutDesc.visibility = nvrhi::ShaderType::All;
        bindlessLayoutDesc.firstSlot = 0;
        bindlessLayoutDesc.maxCapacity = 1024;
        bindlessLayoutDesc.registerSpaces = {
            nvrhi::BindingLayoutItem::RawBuffer_SRV(1),
            nvrhi::BindingLayoutItem::Texture_SRV(2)
        };
        m_BindlessLayout = GetDevice()->createBindlessLayout(bindlessLayoutDesc);

        nvrhi::BindingLayoutDesc globalBindingLayoutDesc;
        globalBindingLayoutDesc.visibility = nvrhi::ShaderType::All;
        globalBindingLayoutDesc.bindings = {
            nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
            nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2),
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3),
            nvrhi::BindingLayoutItem::Texture_SRV(4),
            nvrhi::BindingLayoutItem::Sampler(0),
            nvrhi::BindingLayoutItem::Texture_UAV(0)
        };
        m_BindingLayout = GetDevice()->createBindingLayout(globalBindingLayoutDesc);

        m_DescriptorTable = std::make_shared<DescriptorTableManager>(GetDevice(), m_BindlessLayout);

        auto nativeFS = std::make_shared<vfs::NativeFileSystem>();
        m_TextureCache = std::make_shared<TextureCache>(GetDevice(), nativeFS, m_DescriptorTable);
        
        SetAsynchronousLoadingEnabled(false);
        BeginLoadingScene(nativeFS, sceneFileName);

        m_SunLight = std::make_shared<DirectionalLight>();
        m_Scene->GetSceneGraph()->AttachLeafNode(m_Scene->GetSceneGraph()->GetRootNode(), m_SunLight);

        m_SunLight->SetDirection(double3(0.1f, -1.0f, -0.15f));
        m_SunLight->angularSize = 0.53f;
        m_SunLight->irradiance = 5.f;

        const nvrhi::Format shadowMapFormats[] = {
            nvrhi::Format::D24S8,
            nvrhi::Format::D32,
            nvrhi::Format::D16,
            nvrhi::Format::D32S8 };

        const nvrhi::FormatSupport shadowMapFeatures =
            nvrhi::FormatSupport::Texture |
            nvrhi::FormatSupport::DepthStencil |
            nvrhi::FormatSupport::ShaderLoad;

        nvrhi::Format shadowMapFormat = nvrhi::utils::ChooseFormat(GetDevice(), shadowMapFeatures, shadowMapFormats, std::size(shadowMapFormats));

        m_ShadowMap = std::make_shared<CascadedShadowMap>(GetDevice(), 2048, 4, 0, shadowMapFormat);
        m_ShadowMap->SetupProxyViews();

        m_ShadowFramebuffer = std::make_shared<FramebufferFactory>(GetDevice());
        m_ShadowFramebuffer->DepthTarget = m_ShadowMap->GetTexture();

        DepthPass::CreateParameters shadowDepthParams;
        shadowDepthParams.slopeScaledDepthBias = 4.f;
        shadowDepthParams.depthBias = 100;
        m_ShadowDepthPass = std::make_shared<DepthPass>(GetDevice(), m_CommonPasses);
        m_ShadowDepthPass->Init(*m_ShaderFactory, shadowDepthParams);

        m_OpaqueDrawStrategy = std::make_shared<InstancedOpaqueDrawStrategy>();

        m_Scene->FinishedLoading(GetFrameIndex());
        
        m_Camera.LookAt(float3(0.f, 1.8f, 0.f), float3(1.f, 1.8f, 0.f));
        m_Camera.SetMoveSpeed(3.f);

        if (!CreatePipeline((*m_ShaderFactory)))
            return false;

#if ENABLE_DLSS
#if DLSS_WITH_DX12
        if (GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12)
            m_DLSS = DLSS::CreateDX12(GetDevice(), *m_ShaderFactory);
#endif
#if DLSS_WITH_VK
        if (GetDevice()->getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
            m_DLSS = DLSS::CreateVK(GetDevice(), *m_ShaderFactory);
#endif
        if (m_DLSS)
        {
            m_ui->dlssAvailable = m_DLSS->IsSupported();
        }
#endif

        const std::filesystem::path stbnTexturePath = app::GetDirectoryWithExecutable().parent_path() / "assets/media/STBN/STBlueNoise_vec2_128x128x64.png";

        m_STBNTexture = m_TextureCache->LoadTextureFromFileDeferred(stbnTexturePath, false);

        if (m_TextureCache->IsTextureLoaded(m_STBNTexture))
        {
            m_TextureCache->ProcessRenderingThreadCommands(*m_CommonPasses, 0.f);
            m_TextureCache->LoadingFinished();
        }

        GBufferFillPass::CreateParameters GBufferParams;
        m_GBufferPass = std::make_unique<GBufferFillPassWithSTF>(GetDevice(), m_CommonPasses, m_ui, m_STBNTexture);
        m_GBufferPass->Init(*m_ShaderFactory, GBufferParams);

        m_rayTracingConstantBuffer = GetDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(
            sizeof(LightingConstants), "LightingConstants", c_MaxRenderPassConstantBufferVersions));

        m_CommandList = GetDevice()->createCommandList();

        m_CommandList->open();

        CreateAccelStructs(m_CommandList);

        m_CommandList->close();
        GetDevice()->executeCommandList(m_CommandList);

        GetDevice()->waitForIdle();

        return true;
    }

    bool LoadScene(std::shared_ptr<vfs::IFileSystem> fs, const std::filesystem::path& sceneFileName) override 
    {
        Scene* scene = new Scene(GetDevice(), *m_ShaderFactory, fs, m_TextureCache, m_DescriptorTable, nullptr);

        if (scene->Load(sceneFileName))
        {
            m_Scene = std::unique_ptr<Scene>(scene);
            return true;
        }

        return false;
    }

    bool KeyboardUpdate(int key, int scancode, int action, int mods) override
    {
        m_Camera.KeyboardUpdate(key, scancode, action, mods);

        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        {
            m_ui->enableAnimations = !m_ui->enableAnimations;
            return true;
        }

        return true;
    }

    bool MousePosUpdate(double xpos, double ypos) override
    {
        m_Camera.MousePosUpdate(xpos, ypos);
        return true;
    }

    bool MouseButtonUpdate(int button, int action, int mods) override
    {
        m_Camera.MouseButtonUpdate(button, action, mods);
        return true;
    }

    bool MouseScrollUpdate(double xoffset, double yoffset) override
    {
        m_Camera.MouseScrollUpdate(xoffset, yoffset);
        return true;
    }

    void Animate(float fElapsedTimeSeconds) override
    {
        m_Camera.Animate(fElapsedTimeSeconds);

        if (IsSceneLoaded() && m_ui->enableAnimations)
        {
            m_WallclockTime += fElapsedTimeSeconds;
            float offset = 0;

            for (const auto& anim : m_Scene->GetSceneGraph()->GetAnimations())
            {
                float duration = anim->GetDuration();
                float integral;
                float animationTime = std::modf((m_WallclockTime + offset) / duration, &integral) * duration;
                (void)anim->Apply(animationTime);
                offset += 1.0f;
            }
        }

        const char* extraInfo = m_ui->stfPipelineType == StfPipelineType::RayGen ? 
            "- using RayGen(TraceRay)" : m_ui->stfPipelineType == StfPipelineType::Compute ? 
            "- using Compute(TraceRayInline)" : "- using Rasterization";
        GetDeviceManager()->SetInformativeWindowTitle(g_WindowTitle, extraInfo);

        if (m_ToneMappingPass)
            m_ToneMappingPass->AdvanceFrame(fElapsedTimeSeconds);
    }

    bool CreatePipeline(ShaderFactory& shaderFactory)
    {
        if (m_ui->stfPipelineType == StfPipelineType::Compute)
        {
            return CreateComputePipeline(*m_ShaderFactory);
        }
        else if (m_ui->stfPipelineType == StfPipelineType::RayGen)
        {
            return CreateRayTracingPipeline(*m_ShaderFactory);
        }
        else
        {
            GBufferFillPass::CreateParameters GBufferParams;
            m_GBufferPass = std::make_unique<GBufferFillPassWithSTF>(GetDevice(), m_CommonPasses, m_ui, m_STBNTexture);
            m_GBufferPass->Init(*m_ShaderFactory, GBufferParams);
            return (m_GBufferPass != nullptr);
        }
    }

    uint2 GetThreadGroupSize()
    {
        if (!GetStfOn(m_ui->samplerType))
        {
            return uint2(16, 16);
        }

        if (m_ui->stfGroupSize == StfThreadGroupSize::_8x8)
        {
            return uint2(8, 8);
        }
        else if (m_ui->stfGroupSize == StfThreadGroupSize::_16x8)
        {
            return uint2(16, 8);
        }
        else if (m_ui->stfGroupSize == StfThreadGroupSize::_8x16)
        {
            return uint2(8, 16);
        }
        else // if (m_ui->stfGroupSize == StfThreadGroupSize::_8x8)
        {
            assert(m_ui->stfGroupSize == StfThreadGroupSize::_16x16);
            return uint2(16, 16);
        }
    }

    bool CreateComputePipeline(ShaderFactory& shaderFactory)
    {
        uint2 threadDim = GetThreadGroupSize();
        assert(threadDim.x == 16 || threadDim.x == 8);
        assert(threadDim.y == 16 || threadDim.y == 8);
        const char* dimX = threadDim.x == 16 ? "16" : "8";
        const char* dimY = threadDim.y == 16 ? "16" : "8";

        std::vector<ShaderMacro> defines = { 
            { "USE_RAY_QUERY", "1" },
            { "STF_ENABLED", !GetStfOn(m_ui->samplerType) ? "0" : "1" },
            { "STF_LOAD", (GetStfOn(m_ui->samplerType) && m_ui->stfLoad) ? "1" : "0" },
            { "THREAD_SIZE_X", dimX },
            { "THREAD_SIZE_Y", dimY },
        };

        m_ComputeShader = m_ShaderFactory->CreateShader("app/stf_bindless_rendering_cs.hlsl", "main_cs", &defines, nvrhi::ShaderType::Compute);

        if (!m_ComputeShader)
            return false;

        auto pipelineDesc = nvrhi::ComputePipelineDesc()
            .setComputeShader(m_ComputeShader)
            .addBindingLayout(m_BindingLayout)
            .addBindingLayout(m_BindlessLayout);

        m_ComputePipeline = GetDevice()->createComputePipeline(pipelineDesc);

        if (!m_ComputePipeline)
            return false;

        return true;
    }

    bool CreateRayTracingPipeline(ShaderFactory& shaderFactory)
    {
        std::vector<ShaderMacro> defines = { { "USE_RAY_QUERY", "0" }, { "STF_ENABLED", GetStfOn(m_ui->samplerType) ? "1" : "0" }, { "STF_LOAD", (GetStfOn(m_ui->samplerType) && m_ui->stfLoad) ? "1" : "0" } };
        m_ShaderLibraryLib = shaderFactory.CreateShaderLibrary("app/stf_bindless_rendering_lib.hlsl", &defines);
        m_ShaderLibraryHitGroup = shaderFactory.CreateShaderLibrary("app/stf_bindless_rendering_hit_group.hlsl", &defines);

        if (!m_ShaderLibraryLib)
            return false;

        if (!m_ShaderLibraryHitGroup)
            return false;

        nvrhi::rt::PipelineDesc pipelineDesc;
        pipelineDesc.globalBindingLayouts = { m_BindingLayout, m_BindlessLayout };
        pipelineDesc.shaders = {
            { "", m_ShaderLibraryLib->getShader("RayGen", nvrhi::ShaderType::RayGeneration), nullptr },
            { "", m_ShaderLibraryLib->getShader("Miss", nvrhi::ShaderType::Miss), nullptr }
        };

        pipelineDesc.hitGroups = { {
            "HitGroup",
            m_ShaderLibraryHitGroup->getShader("ClosestHit", nvrhi::ShaderType::ClosestHit),
            m_ShaderLibraryHitGroup->getShader("AnyHit", nvrhi::ShaderType::AnyHit),
            nullptr, // intersectionShader
            nullptr, // bindingLayout
            false  // isProceduralPrimitive
        } };

        pipelineDesc.maxPayloadSize = sizeof(float) * 8;

        m_RayPipeline = GetDevice()->createRayTracingPipeline(pipelineDesc);

        if (!m_RayPipeline)
            return false;

        m_ShaderTable = m_RayPipeline->createShaderTable();

        if (!m_ShaderTable)
            return false;

        m_ShaderTable->setRayGenerationShader("RayGen");
        m_ShaderTable->addHitGroup("HitGroup");
        m_ShaderTable->addMissShader("Miss");

        return true;
    }

    void GetMeshBlasDesc(MeshInfo& mesh, nvrhi::rt::AccelStructDesc& blasDesc) const
    {
        blasDesc.isTopLevel = false;
        blasDesc.debugName = mesh.name;

        for (const auto& geometry : mesh.geometries)
        {
            nvrhi::rt::GeometryDesc geometryDesc;
            auto & triangles = geometryDesc.geometryData.triangles;
            triangles.indexBuffer = mesh.buffers->indexBuffer;
            triangles.indexOffset = (mesh.indexOffset + geometry->indexOffsetInMesh) * sizeof(uint32_t);
            triangles.indexFormat = nvrhi::Format::R32_UINT;
            triangles.indexCount = geometry->numIndices;
            triangles.vertexBuffer = mesh.buffers->vertexBuffer;
            triangles.vertexOffset = (mesh.vertexOffset + geometry->vertexOffsetInMesh) * sizeof(float3) + mesh.buffers->getVertexBufferRange(VertexAttribute::Position).byteOffset;
            triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
            triangles.vertexStride = sizeof(float3);
            triangles.vertexCount = geometry->numVertices;
            geometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
            geometryDesc.flags = (geometry->material->domain == MaterialDomain::AlphaTested)
                ? nvrhi::rt::GeometryFlags::None
                : nvrhi::rt::GeometryFlags::Opaque;
            blasDesc.bottomLevelGeometries.push_back(geometryDesc);
        }

        // don't compact acceleration structures that are built per frame
        if (mesh.skinPrototype != nullptr)
        {
            blasDesc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
        }
        else
        {
            blasDesc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace | nvrhi::rt::AccelStructBuildFlags::AllowCompaction;
        }
    }

    void CreateAccelStructs(nvrhi::ICommandList* commandList)
    {
        for (const auto& mesh : m_Scene->GetSceneGraph()->GetMeshes())
        {
            if (mesh->buffers->hasAttribute(VertexAttribute::JointWeights))
                continue; // skip the skinning prototypes
            
            nvrhi::rt::AccelStructDesc blasDesc;

            GetMeshBlasDesc(*mesh, blasDesc);

            nvrhi::rt::AccelStructHandle as = GetDevice()->createAccelStruct(blasDesc);

            if (!mesh->skinPrototype)
                nvrhi::utils::BuildBottomLevelAccelStruct(commandList, as, blasDesc);

            mesh->accelStruct = as;
        }


        nvrhi::rt::AccelStructDesc tlasDesc;
        tlasDesc.isTopLevel = true;
        tlasDesc.topLevelMaxInstances = m_Scene->GetSceneGraph()->GetMeshInstances().size();
        m_TopLevelAS = GetDevice()->createAccelStruct(tlasDesc);
    }

    void BuildTLAS(nvrhi::ICommandList* commandList, uint32_t frameIndex) const
    {
        commandList->beginMarker("Skinned BLAS Updates");

        // Transition all the buffers to their necessary states before building the BLAS'es to allow BLAS batching
        for (const auto& skinnedInstance : m_Scene->GetSceneGraph()->GetSkinnedMeshInstances())
        {
            if (skinnedInstance->GetLastUpdateFrameIndex() < frameIndex)
                continue;

            commandList->setAccelStructState(skinnedInstance->GetMesh()->accelStruct, nvrhi::ResourceStates::AccelStructWrite);
            commandList->setBufferState(skinnedInstance->GetMesh()->buffers->vertexBuffer, nvrhi::ResourceStates::AccelStructBuildInput);
        }
        commandList->commitBarriers();

        // Now build the BLAS'es
        for (const auto& skinnedInstance : m_Scene->GetSceneGraph()->GetSkinnedMeshInstances())
        {
            if (skinnedInstance->GetLastUpdateFrameIndex() < frameIndex)
                continue;
            
            nvrhi::rt::AccelStructDesc blasDesc;
            GetMeshBlasDesc(*skinnedInstance->GetMesh(), blasDesc);
            
            nvrhi::utils::BuildBottomLevelAccelStruct(commandList, skinnedInstance->GetMesh()->accelStruct, blasDesc);
        }
        commandList->endMarker();

        std::vector<nvrhi::rt::InstanceDesc> instances;

        for (const auto& instance : m_Scene->GetSceneGraph()->GetMeshInstances())
        {
            nvrhi::rt::InstanceDesc instanceDesc;
            instanceDesc.bottomLevelAS = instance->GetMesh()->accelStruct;
            assert(instanceDesc.bottomLevelAS);
            instanceDesc.instanceMask = 1;
            instanceDesc.instanceID = instance->GetInstanceIndex();

            auto node = instance->GetNode();
            assert(node);
            dm::affineToColumnMajor(node->GetLocalToWorldTransformFloat(), instanceDesc.transform);

            instances.push_back(instanceDesc);
        }

        // Compact acceleration structures that are tagged for compaction and have finished executing the original build
        commandList->compactBottomLevelAccelStructs();

        commandList->beginMarker("TLAS Update");
        commandList->buildTopLevelAccelStruct(m_TopLevelAS, instances.data(), instances.size());
        commandList->endMarker();
    }


    void BackBufferResizing() override
    { 
        m_ColorBuffer = nullptr;
        m_BindingCache->Clear();

        m_RenderTargets = nullptr;

        m_PreviousViewsValid = false;
    }

    void Render(nvrhi::IFramebuffer* framebuffer) override
    {
        const auto& fbinfo = framebuffer->getFramebufferInfo();

        uint32_t inputWidth = fbinfo.width;
        uint32_t inputHeight = fbinfo.height;
        uint32_t outputWidth = fbinfo.width;
        uint32_t outputHeight = fbinfo.height;

        if (m_ui->clearShaderCache || m_ui->stfPipelineUpdate)
        {
            m_ShaderFactory->ClearCache();
            m_BindingCache->Clear();
        }

        if (m_ui->stfPipelineUpdate)
        {
            CreatePipeline(*m_ShaderFactory);
            m_ui->stfPipelineUpdate = false;
        }

        if (!m_RenderTargets || m_ui->aaModeChanged)
        {
            // Get optimal render resolution before creating render targets
            if (m_DLSS->IsSupported() && m_ui->aaMode == AntiAliasingMode::DLSS)
            {
                m_DLSS->SetRenderSize(outputWidth, outputHeight, GetNativeDLSSModes(m_ui->qualityMode));
                m_DLSS->GetRenderSize(&inputWidth, &inputHeight, &outputWidth, &outputHeight);
            }

            m_RenderTargets = std::make_unique<RenderTargets>(GetDevice(), int2(inputWidth, inputHeight), int2(outputWidth, outputHeight));

            render::TemporalAntiAliasingPass::CreateParameters taaParams;
            taaParams.sourceDepth = m_RenderTargets->DeviceDepth;
            taaParams.motionVectors = m_RenderTargets->MotionVectors;
            taaParams.unresolvedColor = m_RenderTargets->HdrColor;
            taaParams.resolvedColor = m_RenderTargets->ResolvedColor;
            taaParams.feedback1 = m_RenderTargets->TaaFeedback1;
            taaParams.feedback2 = m_RenderTargets->TaaFeedback2;
            taaParams.motionVectorStencilMask = 0x00;
            taaParams.useCatmullRomFilter = true;

            m_TemporalPass = std::make_unique<render::TemporalAntiAliasingPass>(GetDevice(), m_ShaderFactory, m_CommonPasses, m_View, taaParams);
            if (m_TemporalPass)
            {
                m_TemporalPass->SetJitter(donut::render::TemporalAntiAliasingJitter::Halton);
            }

            m_ui->aaModeChanged = false;
        }

        if (m_DLSS->IsSupported() && m_ui->aaMode == AntiAliasingMode::DLSS)
        {
            m_DLSS->GetRenderSize(&inputWidth, &inputHeight, &outputWidth, &outputHeight);
        }

        m_CommandList->open();

        m_Scene->Refresh(m_CommandList, GetFrameIndex());

#if ENABLE_DLSS
        if (m_ui->aaMode == AntiAliasingMode::DLSS)
        {
            if (!m_ToneMappingPass)
            {
                render::ToneMappingPass::CreateParameters toneMappingParams;
                m_ToneMappingPass = std::make_unique<render::ToneMappingPass>(GetDevice(), m_ShaderFactory, m_CommonPasses, m_RenderTargets->LdrFramebuffer, m_View, toneMappingParams);
            }
        }
#endif

        nvrhi::Viewport windowViewport((float)inputWidth, (float)inputHeight);
        m_View.SetViewport(windowViewport);
        m_View.SetMatrices(m_Camera.GetWorldToViewMatrix(), perspProjD3DStyleReverse(dm::PI_f * 0.25f, windowViewport.width() / windowViewport.height(), 0.1f));
        m_View.UpdateCache();

        auto GetStfRuntimeFilterMode = [](StfFilterMode mode)->int {
            switch (mode)
            {
            case StfFilterMode::Linear: return STF_FILTER_TYPE_LINEAR;
            case StfFilterMode::Cubic: return STF_FILTER_TYPE_CUBIC;
            case StfFilterMode::Gaussian: return STF_FILTER_TYPE_GAUSSIAN;
            default:
                assert(!"Not Implemented");
                return 0;
            }
            };

        auto GetStfMagMode = [](StfMagMethod mode)->int {
            switch (mode)
            {
            case StfMagMethod::Default: return STF_MAGNIFICATION_METHOD_NONE;
            case StfMagMethod::Filter2x2Quad: return STF_MAGNIFICATION_METHOD_2x2_QUAD;
            case StfMagMethod::Filter2x2Fine: return STF_MAGNIFICATION_METHOD_2x2_FINE;
            case StfMagMethod::Filter2x2FineTemporal: return STF_MAGNIFICATION_METHOD_2x2_FINE_TEMPORAL;
            case StfMagMethod::Filter3x3FineAlu: return STF_MAGNIFICATION_METHOD_3x3_FINE_ALU;
            case StfMagMethod::Filter3x3FineLut: return STF_MAGNIFICATION_METHOD_3x3_FINE_LUT;
            case StfMagMethod::Filter4x4Fine: return STF_MAGNIFICATION_METHOD_4x4_FINE;
            default:
                assert(!"Not Implemented");
                return 0;
            }
            };

        auto GetStfAddressMode = [](StfAddressMode mode)->int {
            switch (mode)
            {
            case StfAddressMode::SameAsSampler: return STF_ADDRESS_MODE_WRAP; // m_CommonPasses->m_AnisotropicWrapSampler
            case StfAddressMode::Clamp: return STF_ADDRESS_MODE_CLAMP;
            case StfAddressMode::Wrap: return STF_ADDRESS_MODE_WRAP;
            default:
                assert(!"Not Implemented");
                return 0;
            }
            };

        float mipLevelOverride = 0; // wont be read by default

        if (m_ui->stfMinificationMethod == StfMinMethod::ForceNegInf)
        {
            const uint32_t negInf = 0xFF800000;
            mipLevelOverride = reinterpret_cast<const float&>(negInf);
        }
        else if (m_ui->stfMinificationMethod == StfMinMethod::ForcePosInf)
        {
            const uint32_t posInf = 0x7F800000;
            mipLevelOverride = reinterpret_cast<const float&>(posInf);
        }
        else if (m_ui->stfMinificationMethod == StfMinMethod::ForceNan)
        {
            const uint32_t nan = 0x7FC00000;
            mipLevelOverride = reinterpret_cast<const float&>(nan);
        }
        else
        {
            mipLevelOverride = m_ui->stfMipLevelOverride;
        }

        LightingConstants constants = {};
        constants.ambientColor = m_ambientColor;
        constants.stfSplitScreen = m_ui->samplerType == SamplerType::SplitScreen ? 1 : 0;
        constants.stfFrameIndex = m_ui->stfFreezeFrameIndex ? m_FrameIndex : m_FrameIndex++;
        constants.stfFilterMode = GetStfRuntimeFilterMode(m_ui->stfFilterMode);
        constants.stfMagnificationMethod = GetStfMagMode(m_ui->stfMagnificationMethod);
        constants.stfMinificationMethod = m_ui->stfMinificationMethod == StfMinMethod::Aniso ? STF_ANISO_LOD_METHOD_DEFAULT : STF_ANISO_LOD_METHOD_NONE;
        constants.stfUseMipLevelOverride = m_ui->stfMinificationMethod != StfMinMethod::Aniso ? 1 : 0;
        constants.stfAddressMode = GetStfAddressMode(m_ui->stfAddressMode);
        constants.stfMipLevelOverride = mipLevelOverride;
        constants.stfSigma = m_ui->stfSigma;
        constants.stfWaveLaneLayoutOverride = (uint)m_ui->stfWaveLaneLayoutOverride;
        constants.stfReseedOnSample = (uint)m_ui->stfReseedOnSample;
        constants.stfUseWhiteNoise = (uint)m_ui->stfUseWhiteNoise;
        constants.stfDebugVisualizeLanes = m_ui->stfDebugVisualizeLanes ? 1 : 0;

        m_View.FillPlanarViewConstants(constants.view);
        if (m_PreviousViewsValid)
        {
            m_ViewPrevious.FillPlanarViewConstants(constants.viewPrev);
        }
        m_SunLight->FillLightConstants(constants.light);

        if (m_TemporalPass && m_PreviousViewsValid)
        {
            m_TemporalPass->RenderMotionVectors(m_CommandList, m_View, m_ViewPrevious);
        }

        if (m_ui->stfPipelineType == StfPipelineType::Raster)
        {
            if (!m_DeferredLightingPass)
            {
                m_DeferredLightingPass = std::make_unique<DeferredLightingPass>(GetDevice(), m_CommonPasses);
                m_DeferredLightingPass->Init(m_ShaderFactory);
            }

            m_GBufferPass->UpdateLightConstantBuffer(m_CommandList, constants);

            m_SunLight->shadowMap = m_ShadowMap;
            box3 sceneBounds = m_Scene->GetSceneGraph()->GetRootNode()->GetGlobalBoundingBox();

            frustum projectionFrustum = m_View.GetProjectionFrustum();
            const float maxShadowDistance = 100.f;

            dm::affine3 viewMatrixInv = m_View.GetChildView(ViewType::PLANAR, 0)->GetInverseViewMatrix();

            float zRange = length(sceneBounds.diagonal()) * 0.5f;
            m_ShadowMap->SetupForPlanarViewStable(*m_SunLight, projectionFrustum, viewMatrixInv, maxShadowDistance, zRange, zRange, /*m_ui.CsmExponent*/4.0);

            m_ShadowMap->Clear(m_CommandList);

            DepthPass::Context context;

            RenderCompositeView(m_CommandList,
                &m_ShadowMap->GetView(), nullptr,
                *m_ShadowFramebuffer,
                m_Scene->GetSceneGraph()->GetRootNode(),
                *m_OpaqueDrawStrategy,
                *m_ShadowDepthPass,
                context,
                "ShadowMap",
                /*m_ui.EnableMaterialEvents*/ false);

            m_CommandList->clearDepthStencilTexture(m_RenderTargets->DeviceDepth, nvrhi::AllSubresources, true, 0.0, true, 0);
            m_CommandList->clearTextureFloat(m_RenderTargets->Depth, nvrhi::AllSubresources, nvrhi::Color(65504.f));
            m_CommandList->clearTextureFloat(m_RenderTargets->HdrColor, nvrhi::AllSubresources, nvrhi::Color(0.f));
            m_CommandList->clearTextureFloat(m_RenderTargets->GBufferDiffuseAlbedo, nvrhi::AllSubresources, nvrhi::Color(0.f));
            m_CommandList->clearTextureFloat(m_RenderTargets->GBufferSpecularRough, nvrhi::AllSubresources, nvrhi::Color(0.f));
            m_CommandList->clearTextureFloat(m_RenderTargets->GBufferNormals, nvrhi::AllSubresources, nvrhi::Color(0.f));
            m_CommandList->clearTextureFloat(m_RenderTargets->GBufferEmissive, nvrhi::AllSubresources, nvrhi::Color(0.f));

            size_t meshCount = m_Scene->GetSceneGraph()->GetMeshInstances().size();

            std::vector<DrawItem> drawItems;
            for (int meshIndex = 0; meshIndex < meshCount; meshIndex++)
            {
               DrawItem drawItem;
                drawItem.instance = m_Scene->GetSceneGraph()->GetMeshInstances()[meshIndex].get();
                drawItem.mesh = drawItem.instance->GetMesh().get();

                size_t geomCount = drawItem.mesh->geometries.size();
                for (int geomIndex = 0; geomIndex < geomCount; geomIndex++)
                {
                    drawItem.geometry = drawItem.mesh->geometries[geomIndex].get();
                    drawItem.material = drawItem.geometry->material.get();
                    drawItem.buffers = drawItem.mesh->buffers.get();
                    drawItem.distanceToCamera = 0;
                    drawItem.cullMode = nvrhi::RasterCullMode::Back;

                    drawItems.push_back(drawItem);
                }
            }

            if (!drawItems.empty())
            {
                PassthroughDrawStrategy drawStrategy;
                drawStrategy.SetData(drawItems.data(), drawItems.size());

                GBufferFillPassWithSTF::Context context;

                RenderView(
                    m_CommandList,
                    &m_View,
                    m_PreviousViewsValid ? &m_ViewPrevious : &m_View,
                    m_RenderTargets->GBufferFramebuffer->GetFramebuffer(m_View),
                    drawStrategy,
                    *m_GBufferPass,
                    context,
                    false);
            }

            GBufferRenderTargets gbufferTargets;
            gbufferTargets.Depth = m_RenderTargets->DeviceDepth;
            gbufferTargets.GBufferDiffuse = m_RenderTargets->GBufferDiffuseAlbedo;
            gbufferTargets.GBufferSpecular = m_RenderTargets->GBufferSpecularRough;
            gbufferTargets.GBufferNormals = m_RenderTargets->GBufferNormals;
            gbufferTargets.GBufferEmissive = m_RenderTargets->GBufferEmissive;

            DeferredLightingPass::Inputs deferredInputs;
            deferredInputs.SetGBuffer(gbufferTargets);
            deferredInputs.ambientColorTop = m_ambientColor;
            deferredInputs.ambientColorBottom = deferredInputs.ambientColorTop;
            deferredInputs.lights = &m_Scene->GetSceneGraph()->GetLights();
            deferredInputs.output = m_RenderTargets->HdrColor;

            m_DeferredLightingPass->Render(m_CommandList, m_View, deferredInputs);
        }
        else if (m_ui->stfPipelineType != StfPipelineType::Raster)
        {
            m_CommandList->writeBuffer(m_rayTracingConstantBuffer, &constants, sizeof(constants));

            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_rayTracingConstantBuffer),
                nvrhi::BindingSetItem::RayTracingAccelStruct(0, m_TopLevelAS),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(1, m_Scene->GetInstanceBuffer()),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(2, m_Scene->GetGeometryBuffer()),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(3, m_Scene->GetMaterialBuffer()),
                nvrhi::BindingSetItem::Texture_SRV(4, m_STBNTexture->texture),
                nvrhi::BindingSetItem::Sampler(0, m_CommonPasses->m_AnisotropicWrapSampler),
                nvrhi::BindingSetItem::Texture_UAV(0, m_RenderTargets->HdrColor)
            };

            m_BindingSet = GetDevice()->createBindingSet(bindingSetDesc, m_BindingLayout);

            BuildTLAS(m_CommandList, GetFrameIndex());

            if (m_RayPipeline && m_ui->stfPipelineType == StfPipelineType::RayGen)
            {
                nvrhi::rt::State state;
                state.shaderTable = m_ShaderTable;
                state.bindings = { m_BindingSet, m_DescriptorTable->GetDescriptorTable() };
                m_CommandList->setRayTracingState(state);

                nvrhi::rt::DispatchRaysArguments args;
                args.width = inputWidth;
                args.height = inputHeight;
                m_CommandList->dispatchRays(args);
            }
            else if (m_ComputePipeline && m_ui->stfPipelineType == StfPipelineType::Compute)
            {
                nvrhi::ComputeState state;
                state.pipeline = m_ComputePipeline;
                state.bindings = { m_BindingSet, m_DescriptorTable->GetDescriptorTable() };
                m_CommandList->setComputeState(state);

                uint2 threadDim = GetThreadGroupSize();

                m_CommandList->dispatch(
                    dm::div_ceil(inputWidth, threadDim.x),
                    dm::div_ceil(inputHeight, threadDim.y));
            }
        }

        if (m_ui->aaMode == AntiAliasingMode::None)
        {
            m_CommonPasses->BlitTexture(m_CommandList, framebuffer, m_RenderTargets->HdrColor, m_BindingCache.get());
        }

#if ENABLE_DLSS
        if (m_DLSS->IsSupported() && m_ui->aaMode == AntiAliasingMode::DLSS)
        {
            m_DLSS->Render(m_CommandList,
                *m_RenderTargets,
                m_ToneMappingPass->GetExposureBuffer(),
                0.0,
                1.0,
                false,
                0,
                m_View,
                m_PreviousViewsValid ? m_ViewPrevious : m_View);
        }
        else
        {
#endif
            if (m_TemporalPass)
            {
                TemporalAntiAliasingParameters params = {};
                m_TemporalPass->TemporalResolve(m_CommandList, params, m_PreviousViewsValid, m_View, m_View);
            }

#if ENABLE_DLSS
        }
#endif

#if ENABLE_DLSS
        if (m_ui->aaMode == AntiAliasingMode::DLSS)
        {
            ToneMappingParameters ToneMappingParams;
            ToneMappingParams.minAdaptedLuminance = 0.002f;
            ToneMappingParams.maxAdaptedLuminance = 0.2f;
            ToneMappingParams.exposureBias = 0.0;
            ToneMappingParams.eyeAdaptationSpeedUp = 2.0f;
            ToneMappingParams.eyeAdaptationSpeedDown = 1.0f;
            ToneMappingParams.eyeAdaptationSpeedUp = 0.f;
            ToneMappingParams.eyeAdaptationSpeedDown = 0.f;

            m_ToneMappingPass->SimpleRender(m_CommandList, ToneMappingParams, m_View, m_RenderTargets->ResolvedColor);

            m_CommonPasses->BlitTexture(m_CommandList, framebuffer, m_RenderTargets->ResolvedColor, m_BindingCache.get());
        }
        else
        {
#endif
            if (m_ui->aaMode == AntiAliasingMode::TAA)
            {
                m_CommonPasses->BlitTexture(m_CommandList, framebuffer, m_RenderTargets->ResolvedColor, m_BindingCache.get());
            }
#if ENABLE_DLSS
        }
#endif
        m_ViewPrevious = m_View;
        m_PreviousViewsValid = true;

        m_CommandList->close();
        GetDevice()->executeCommandList(m_CommandList);

        if (m_TemporalPass)
        {
            m_TemporalPass->AdvanceFrame();
            m_View.SetPixelOffset(m_ui->aaMode == AntiAliasingMode::None
                ? dm::float2::zero()
                : m_TemporalPass->GetCurrentPixelOffset());
        }

        if (IsSceneLoaded())
        {
            m_ui->isLoading = false;
        }
    }
};

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(int __argc, const char** __argv)
#endif
{
    nvrhi::GraphicsAPI api = app::GetGraphicsAPIFromCommandLine(__argc, __argv);
    app::DeviceManager* deviceManager = app::DeviceManager::Create(api);

    app::DeviceCreationParameters deviceParams;
    deviceParams.enableRayTracingExtensions = true;
    deviceParams.enablePerMonitorDPI = true;
    deviceParams.supportExplicitDisplayScaling = true;
    deviceParams.backBufferWidth = 2560;
    deviceParams.backBufferHeight = 1440;

    bool useRayQuery = false;
    for (int i = 1; i < __argc; i++)
    {
        if (strcmp(__argv[i], "-rayQuery") == 0)
        {
            useRayQuery = true;
        }
        else if (strcmp(__argv[i], "-debug") == 0)
        {
            deviceParams.enableDebugRuntime = true;
            deviceParams.enableNvrhiValidationLayer = true;
        }
    }

#if ENABLE_DLSS && DLSS_WITH_VK
    if (api == nvrhi::GraphicsAPI::VULKAN)
    {
        DLSS::GetRequiredVulkanExtensions(deviceParams.optionalVulkanInstanceExtensions, deviceParams.optionalVulkanDeviceExtensions);
    }
#endif

    if (!deviceManager->CreateWindowDeviceAndSwapChain(deviceParams, g_WindowTitle))
    {
        log::fatal("Cannot initialize a graphics device with the requested parameters");
        return 1;
    }

    if (!useRayQuery && !deviceManager->GetDevice()->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline))
    {
        log::fatal("The graphics device does not support Ray Tracing Pipelines");
        return 1;
    }

    if (useRayQuery && !deviceManager->GetDevice()->queryFeatureSupport(nvrhi::Feature::RayQuery))
    {
        log::fatal("The graphics device does not support Ray Queries");
        return 1;
    }

    {
        BindlessRayTracing example(deviceManager);
        if (example.Init(useRayQuery))
        {
            UserInterface userInterface(deviceManager, *example.GetRootFs(), *example.GetUI());
            userInterface.Init(example.GetShaderFactory());

            deviceManager->AddRenderPassToBack(&example);
            deviceManager->AddRenderPassToBack(&userInterface);
            deviceManager->RunMessageLoop();
            deviceManager->RemoveRenderPass(&example);
            deviceManager->RemoveRenderPass(&userInterface);
        }
    }
    
    deviceManager->Shutdown();

    delete deviceManager;

    return 0;
}
