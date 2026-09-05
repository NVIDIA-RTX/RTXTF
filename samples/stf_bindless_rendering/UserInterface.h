/***************************************************************************
 # Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
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

#if ENABLE_DLSS
#include "../../external/DLSS/include/nvsdk_ngx_helpers.h"
#endif

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
    MinMax,
    MinMaxHelper,
    MinMaxV2,
    MinMaxV2Helper,
    Mask,
    Mask2,
    Count,
};

enum class StfFallbackMethod
{
    BL1STFILTER_FAST,
    Debug,
    Count
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
    Compute,
    Raster,
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
    RowLinear8x4,
};

inline static bool StfIs8WideGroup(StfThreadGroupSize group)
{
    return group == StfThreadGroupSize::_8x8 || group == StfThreadGroupSize::_8x16;
}

// The fixed-offset Quad/Fine methods are the only ones whose wave sharing depends on the
// lane->pixel layout. MinMax/Mask address lanes dynamically and Default does no sharing,
// so the lane layout does not apply to them.
inline static bool StfIsWaveShareMagMethod(StfMagMethod method)
{
    return method >= StfMagMethod::Filter2x2Quad && method <= StfMagMethod::Filter4x4Fine;
}

// A layout is only valid when its warp tiling matches the thread-group width, so the
// pixel<->lane remap stays a bijection over each 32-lane warp. Mismatched combos (e.g. an
// 8x4 remap on a 16-wide group) leave half the pixels unwritten.
inline static bool StfIsLaneLayoutValidForGroup(StfWaveLaneLayout layout, StfThreadGroupSize group)
{
    switch (layout)
    {
    case StfWaveLaneLayout::RowLinear8x4:  return StfIs8WideGroup(group);   // needs an 8-wide group
    case StfWaveLaneLayout::RowLinear16x2:
    case StfWaveLaneLayout::QuadZ16x2:     return !StfIs8WideGroup(group);  // need a 16-wide group
    case StfWaveLaneLayout::None:
    default:                               return true;
    }
}

// Whether a layout produces correct output for a given method+group. The fixed-offset
// Quad/Fine methods hard-code an 8-wide warp tiling (share offsets like {0,1,8,9} treat
// lane+8 as the vertical neighbor; 3x3/4x4 stride rows by 8 and need 3-4 rows), so the
// only layout that works for them is Row Linear 8x4 on an 8-wide group. For other methods
// the layout doesn't affect output, so any bijective (non-corrupting) layout is fine.
inline static bool StfIsLaneLayoutUsable(StfWaveLaneLayout layout, StfMagMethod method, StfThreadGroupSize group)
{
    if (StfIsWaveShareMagMethod(method))
        return layout == StfWaveLaneLayout::RowLinear8x4 && StfIs8WideGroup(group);
    return StfIsLaneLayoutValidForGroup(layout, group);
}

#if ENABLE_DLSS
// Ultra Quality is broken and is not used in the sample application for DLSS, punting for now until we get more answers for DLSS team
enum class DLSSQualityModes
{
    PerfQuality_Value_MaxPerf,
    PerfQuality_Value_Balanced,
    PerfQuality_Value_MaxQuality,
    PerfQuality_Value_UltraPerformance,
    PerfQuality_Value_DLAA,
};

static NVSDK_NGX_PerfQuality_Value GetNativeDLSSModes(DLSSQualityModes mode)
{
    switch (mode)
    {
    case DLSSQualityModes::PerfQuality_Value_MaxPerf:
        return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_MaxPerf;
    case DLSSQualityModes::PerfQuality_Value_Balanced:
        return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_Balanced;
    case DLSSQualityModes::PerfQuality_Value_MaxQuality:
        return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_MaxQuality;
    case DLSSQualityModes::PerfQuality_Value_UltraPerformance:
        return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_UltraPerformance;
    case DLSSQualityModes::PerfQuality_Value_DLAA:
        return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_DLAA;
    default:
        return NVSDK_NGX_PerfQuality_Value::NVSDK_NGX_PerfQuality_Value_DLAA;
    };
}
#endif

struct UIData
{
    bool isRasterized = true;
    bool showUI = true;
    bool isLoading = true;

    float loadingPercentage = 0.f;

    bool enableTextures = true;

    AntiAliasingMode aaMode = AntiAliasingMode::TAA;
    bool aaModeChanged = false;

    bool enableAnimations = true;
    float animationSpeed = 1.f;

#if ENABLE_DLSS
    bool dlssAvailable = false;
    float dlssExposureScale = 2.f;
    float dlssSharpness = 0.f;
    DLSSQualityModes qualityMode = DLSSQualityModes::PerfQuality_Value_DLAA;
#endif
    bool clearShaderCache = false;
    bool stfPipelineUpdate = false;
    bool stfFreezeFrameIndex = false;

    SamplerType samplerType = SamplerType::STF;
    bool stfLoad = true;
    bool allowHelperLanesInWaveIntrinsics = false;
    StfFilterMode stfFilterMode = StfFilterMode::Linear;
    StfMagMethod stfMagnificationMethod = StfMagMethod::MinMaxV2;
    StfFallbackMethod stfFallbackMethod = StfFallbackMethod::BL1STFILTER_FAST;
    StfMinMethod stfMinificationMethod = StfMinMethod::Aniso;
    float stfMipLevelOverride = 0.f;
    StfAddressMode stfAddressMode = StfAddressMode::SameAsSampler;
    float stfSigma = 0.7f;
    bool stfReseedOnSample = false;
    bool stfUseWhiteNoise = false;
    bool stfDebugOnFailure = false;
    float resolutionScale = 1.f;

    StfPipelineType stfPipelineType = StfPipelineType::Compute;
    StfThreadGroupSize stfGroupSize = StfThreadGroupSize::_8x8;
    StfWaveLaneLayout stfWaveLaneLayoutOverride = StfWaveLaneLayout::RowLinear8x4;
    int stfDebugVisualizeLanes = 0;

    bool enableFpsLimit = true;
    uint32_t fpsLimit = 60;

    donut::render::TemporalAntiAliasingJitter temporalJitter = donut::render::TemporalAntiAliasingJitter::Halton;

    bool enableCtfMagnify = false;

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
