/***************************************************************************
 # Copyright (c) 2021-2024, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#pragma once


#include <donut/engine/Scene.h>
#include <donut/render/TemporalAntiAliasingPass.h>
#include <donut/app/imgui_renderer.h>

#include <optional>
#include <string>

class SampleScene;

namespace donut::engine {
    struct IesProfile;
}

namespace donut::app {
    class FirstPersonCamera;
}

enum class AntiAliasingMode : uint32_t
{
    None,
    TAA,
#if ENABLE_DLSS
    DLAA,
    DLSS
#endif
};

enum class SamplerType
{
    HW,
    STF,
    SplitScreen
};

inline static bool GetStfOn(SamplerType mode)
{
    return mode != SamplerType::HW;
}

enum class StfFilterMode
{
    Linear,
    Cubic,
    Gaussian
};

enum class StfMagMethod
{
    Default,
    Filter2x2Quad,
    Filter2x2Fine,
    Filter2x2FineTemporal,
    Filter3x3FineAlu,
    Filter3x3FineLut,
	Filter4x4Fine,
    Wave
};

enum class StfMinMethod
{
    Aniso,
    ForceNegInf,
    ForcePosInf,
    ForceNan,
    ForceCustom,
};

enum class StfAddressMode
{
    SameAsSampler,
    Clamp,
    Wrap
};

enum class StfPipelineType
{
    RayGen,
    Compute
};

enum class StfThreadGroupSize
{
    _8x8,
    _16x8,
    _8x16,
    _16x16,
};

enum class StfWaveLaneLayout
{
    None,
    RowLinear16x2,
    QuadZ16x2,
};

struct UIData
{
    bool showUI = true;
    bool isLoading = true;

    float loadingPercentage = 0.f;

    bool enableTextures = true;

#if ENABLE_DLSS
    AntiAliasingMode aaMode = AntiAliasingMode::DLSS;
#else
    AntiAliasingMode aaMode = AntiAliasingMode::None;
#endif

    bool enableAnimations = true;
    float animationSpeed = 1.f;

#if ENABLE_DLSS
    bool dlssAvailable = true;
    float dlssExposureScale = 2.f;
    float dlssSharpness = 0.f;
#endif
    bool clearShaderCache = false;
    bool stfPipelineUpdate = false;
    bool stfFreezeFrameIndex = false;

    SamplerType samplerType = SamplerType::STF;
    bool stfLoad = false;
    StfFilterMode stfFilterMode = StfFilterMode::Linear;
    StfMagMethod stfMagnificationMethod = StfMagMethod::Filter2x2Quad;
    StfMinMethod stfMinificationMethod = StfMinMethod::Aniso;
    float stfMipLevelOverride = 0.f;
    StfAddressMode stfAddressMode = StfAddressMode::SameAsSampler;
    float stfSigma = 0.7f;
    bool stfReseedOnSample = false;
    bool stfUseWhiteNoise = false;
    float resolutionScale = 1.f;

    StfPipelineType stfPipelineType = StfPipelineType::Compute;
    StfThreadGroupSize stfGroupSize = StfThreadGroupSize::_8x8;
    StfWaveLaneLayout stfWaveLaneLayoutOverride = StfWaveLaneLayout::None;
    bool stfDebugVisualizeLanes = false;

    bool enableFpsLimit = true;
    uint32_t fpsLimit = 60;

    donut::render::TemporalAntiAliasingJitter temporalJitter = donut::render::TemporalAntiAliasingJitter::Halton;

    UIData();
};


class UserInterface : public donut::app::ImGui_Renderer
{
private:
    UIData& m_ui;
    std::shared_ptr<donut::app::RegisteredFont> m_FontOpenSans;

    void PostProcessSettings();

protected:
    void buildUI(void) override;

public:
    UserInterface(donut::app::DeviceManager* deviceManager, donut::vfs::IFileSystem& rootFS, UIData& ui);

};
