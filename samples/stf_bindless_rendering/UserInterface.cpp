/***************************************************************************
 # Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

 /*
 License for Dear ImGui

 Copyright (c) 2014-2019 Omar Cornut

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#include "UserInterface.h"

#include <donut/engine/IesProfile.h>
#include <donut/app/Camera.h>
#include <donut/app/UserInterfaceUtils.h>
#include <donut/core/json.h>

using namespace donut;

UIData::UIData()
{
}

UserInterface::UserInterface(app::DeviceManager* deviceManager, vfs::IFileSystem& rootFS, UIData& ui)
    : ImGui_Renderer(deviceManager)
    , m_ui(ui)
{
    m_FontOpenSans = CreateFontFromFile(rootFS, "/assets/media/fonts/OpenSans/OpenSans-Regular.ttf", 17.f);
}

static void ShowHelpMarker(const char* desc)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(500.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

constexpr uint32_t c_ColorRegularHeader   = 0xffff8080;
constexpr uint32_t c_ColorAttentionHeader = 0xff80ffff;

struct ScopedDisabled
{
    ScopedDisabled(bool disabled)
    {
        ImGui::BeginDisabled(disabled);
    }

    ~ScopedDisabled()
    {
        ImGui::EndDisabled();
    }
};

#define IMGUI_SCOPED_DISABLE(cond) ScopedDisabled _scopedDisabled_##__LINE__(cond)

static bool ImGui_ColoredTreeNode(const char* text, uint32_t color)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    bool expanded = ImGui::TreeNode(text);
    ImGui::PopStyleColor();
    return expanded;
}
void UserInterface::PostProcessSettings()
{
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Separator();
        ImGui::Text("Post-Processing");

        AntiAliasingMode previousAAMode = m_ui.aaMode;
#if ENABLE_DLSS
        DLSSQualityModes previousQualityMode = m_ui.qualityMode;
#endif
        ImGui::RadioButton("Unfiltered RTXTF", (int*)&m_ui.aaMode, (int)AntiAliasingMode::None);
        ImGui::SameLine();
        ImGui::RadioButton("TAA", (int*)&m_ui.aaMode, (int)AntiAliasingMode::TAA);
#if ENABLE_DLSS
        if (m_ui.dlssAvailable)
        {
            ImGui::SameLine();
            ImGui::RadioButton("DLSS", (int*)&m_ui.aaMode, (int)AntiAliasingMode::DLSS);

            {
                IMGUI_SCOPED_DISABLE(m_ui.aaMode != AntiAliasingMode::DLSS);

                ImGui::Combo("DLSS-SR mode", (int*)&m_ui.qualityMode, "Max Performance\0Balanced\0Max Quality\0Ultra Performance\0DLAA");

                if (previousQualityMode != m_ui.qualityMode)
                {
                    m_ui.aaModeChanged = true;
                }
            }
        }
#endif
        if (previousAAMode != m_ui.aaMode)
        {
            m_ui.aaModeChanged = true;
        }
        ImGui::Checkbox("Enable Animations", (bool*)&m_ui.enableAnimations);

        if (ImGui::Combo("Sampler Type", (int*)&m_ui.samplerType, "Hardware Sampler\0Stochastic Texture Filtering\0Split Screen\0"))
        {
            m_ui.stfPipelineUpdate = true;
        }

        {
            IMGUI_SCOPED_DISABLE(!GetStfOn(m_ui.samplerType));
            if (ImGui::Checkbox("Enable Collaborative Texture Filtering", (bool*)&m_ui.enableCtfMagnify))
            {
                m_ui.stfPipelineUpdate = true;
            }
        }

        {
            ImGui::Text("Stochastic Texture Filtering");
            ImGui::Separator();

            {
                IMGUI_SCOPED_DISABLE(!GetStfOn(m_ui.samplerType));
                ImGui::Checkbox("Freeze Frame Index", (bool*)&m_ui.stfFreezeFrameIndex);

                if (ImGui::Checkbox("Use Load", (bool*)&m_ui.stfLoad))
                {
                    m_ui.stfPipelineUpdate = true;
                }

                {
                    IMGUI_SCOPED_DISABLE(m_ui.stfPipelineType != StfPipelineType::Raster);
                    if (ImGui::Checkbox("Allow Helper Lanes In Wave Intrinsics", (bool*)&m_ui.allowHelperLanesInWaveIntrinsics))
                    {
                        m_ui.stfPipelineUpdate = true;
                    }
                }

                ImGui::Combo("Filter Type", (int*)&m_ui.stfFilterMode, "Linear\0Cubic\0Gaussian\0");

                {
                    IMGUI_SCOPED_DISABLE(!m_ui.stfLoad);
                    ImGui::Combo("Address Mode", (int*)&m_ui.stfAddressMode, "Same as sampler\0Clamp\0Wrap\0");
                }

                {
                    static_assert((int)StfMagMethod::Count == 13);
                    static const char* items[(int)StfMagMethod::Count] =
                    {
                        "Default",
                        "Quad 2x2",
                        "Fine 2x2",
                        "Fine temporal 2x2",
                        "Fine ALU 3x3",
                        "Fine LUT 3x3",
                        "Fine 4x4",
                        "MinMax",
                        "MinMaxHelper",
                        "MinMax2",
                        "MinMax2Helper",
                        "Mask",
                        "Mask2",
                    };

                    ImGui::Combo("Magnification Method", (int*)&m_ui.stfMagnificationMethod, items, (int)StfMagMethod::Count);
                    ShowHelpMarker("Quad Comms: Enable groups of 2x2 pixel quads or threads in a CS to share texture samples. This typically improves reconstruction of high-frequency details during magnification");
                    ShowHelpMarker("Wave Read Lane: Enable groups of 8 threads in a warp to share texture samples. This typically improves reconstruction of high-frequency details during magnification");
                    ShowHelpMarker("Wave Read Lane Sliding Window: Enable groups of 8 threads in a warp to share texture samples in a sliding window to decorelate samples for DLSS. This typically improves reconstruction of high-frequency details during magnification");
                }
                
                {
                    static_assert((int)StfFallbackMethod::Count == 2);
                    static const char* items[(int)StfFallbackMethod::Count] =
                    {
                        "BL1STFILTER_FAST",
                        "Debug",
                    };

                    ImGui::Combo("Fallback Method", (int*)&m_ui.stfFallbackMethod, items, (int)StfFallbackMethod::Count);
                }

                {
                    IMGUI_SCOPED_DISABLE(m_ui.stfFilterMode != StfFilterMode::Gaussian);
                    ImGui::SliderFloat("Sigma", &m_ui.stfSigma, 0.f, 100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
                    ShowHelpMarker("Increase or decrease the width of the Gaussian filter");
                }

            }

            ImGui::Combo("Minification Method", (int*)&m_ui.stfMinificationMethod, "Aniso\0ForceNegInf\0ForcePosInf\0ForceNan\0ForceCustom\0");

            {
                IMGUI_SCOPED_DISABLE(m_ui.stfMinificationMethod != StfMinMethod::ForceCustom);
                ImGui::SliderFloat("Custom Mip Level", &m_ui.stfMipLevelOverride, -100.f, 100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
            }

            ImGui::Checkbox("Reseed on sample", (bool*)&m_ui.stfReseedOnSample);
            ImGui::Checkbox("Use White Noise", (bool*)&m_ui.stfUseWhiteNoise);

            {
                ImGui::Separator();
                ImGui::Text("Shader");

                if (ImGui::Combo("Pipeline Type", (int*)&m_ui.stfPipelineType, "RayGen(TraceRay)\0Compute(TraceRayInline)\0Raster\0"))
                {
                    m_ui.stfPipelineUpdate = true;
                }

                // Quad/Fine wave-share methods only produce correct output with an 8-wide
                // group + Row Linear 8x4 (their share offsets assume lane = x + y*8). On the
                // compute path we steer to that config and grey out everything that can't work.
                const bool laneLayoutApplicable =
                    GetStfOn(m_ui.samplerType)
                    && m_ui.stfPipelineType == StfPipelineType::Compute
                    && StfIsWaveShareMagMethod(m_ui.stfMagnificationMethod);

                if (laneLayoutApplicable)
                {
                    if (!StfIs8WideGroup(m_ui.stfGroupSize))
                    {
                        m_ui.stfGroupSize = StfThreadGroupSize::_8x8;
                        m_ui.stfPipelineUpdate = true;
                    }
                    m_ui.stfWaveLaneLayoutOverride = StfWaveLaneLayout::RowLinear8x4;
                }
                else if (!StfIsLaneLayoutValidForGroup(m_ui.stfWaveLaneLayoutOverride, m_ui.stfGroupSize))
                {
                    // Non-wave-share methods: just keep the remap bijective (no stale pixels).
                    m_ui.stfWaveLaneLayoutOverride = StfIs8WideGroup(m_ui.stfGroupSize)
                        ? StfWaveLaneLayout::RowLinear8x4
                        : StfWaveLaneLayout::None;
                }

                {
                    IMGUI_SCOPED_DISABLE(m_ui.stfPipelineType != StfPipelineType::Compute);
                    static_assert(sizeof(m_ui.stfGroupSize) == sizeof(int));
                    static const char* const kGroupNames[] = { " 8x8", " 16x8", " 8x16", " 16x16" };
                    if (ImGui::BeginCombo("Thread Group Size", kGroupNames[(int)m_ui.stfGroupSize]))
                    {
                        for (int i = 0; i < (int)(sizeof(kGroupNames) / sizeof(kGroupNames[0])); i++)
                        {
                            const StfThreadGroupSize group = (StfThreadGroupSize)i;
                            // Quad/Fine require an 8-wide group; grey the 16-wide options for them.
                            const bool usable = !laneLayoutApplicable || StfIs8WideGroup(group);
                            const bool selected = ((int)m_ui.stfGroupSize == i);

                            ImGui::BeginDisabled(!usable);
                            if (ImGui::Selectable(kGroupNames[i], selected))
                            {
                                m_ui.stfGroupSize = group;
                                m_ui.stfPipelineUpdate = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                            ImGui::EndDisabled();
                        }
                        ImGui::EndCombo();
                    }
                }

                ImGui::Checkbox("Debug Failure", (bool*)&m_ui.stfDebugOnFailure);

                {
                    IMGUI_SCOPED_DISABLE(!laneLayoutApplicable);

                    static const char* const kLaneLayoutNames[] = { "None", "Row Linear 16x2", "Quad-Z 16x2", "Row Linear 8x4" };
                    if (ImGui::BeginCombo("Lane Warp Layout", kLaneLayoutNames[(int)m_ui.stfWaveLaneLayoutOverride]))
                    {
                        for (int i = 0; i < (int)(sizeof(kLaneLayoutNames) / sizeof(kLaneLayoutNames[0])); i++)
                        {
                            const StfWaveLaneLayout layout = (StfWaveLaneLayout)i;
                            // Only the layout that produces correct output for this method is selectable.
                            const bool usable = StfIsLaneLayoutUsable(layout, m_ui.stfMagnificationMethod, m_ui.stfGroupSize);
                            const bool selected = ((int)m_ui.stfWaveLaneLayoutOverride == i);

                            ImGui::BeginDisabled(!usable);
                            if (ImGui::Selectable(kLaneLayoutNames[i], selected))
                            {
                                m_ui.stfWaveLaneLayoutOverride = layout;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                            ImGui::EndDisabled();
                        }
                        ImGui::EndCombo();
                    }
                }

                ImGui::Checkbox("Lane Debug viz", (bool*)&m_ui.stfDebugVisualizeLanes);
            }

            if (ImGui::Button("Recreate shader pipelines"))
            {
                m_ui.clearShaderCache = true;
                m_ui.stfPipelineUpdate = true;
            }
            ShowHelpMarker("Will re-create the graphics/compute/raytracing PSOs. NOTE: This will not trigger shader recompilation, that must be done separatley.");
        }
    }
    ImGui::End();
}

void UserInterface::buildUI()
{
    if (!m_ui.showUI)
        return;

    int width, height;
    GetDeviceManager()->GetWindowDimensions(width, height);

    if (m_ui.isLoading)
    {
        BeginFullScreenWindow();
        ImGui::PushFont(m_FontOpenSans->GetScaledFont());

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImColor barColor = ImColor(1.f, 1.f, 1.f);
        ImVec2 frameTopLeft = ImVec2(200.f, float(height) * 0.5f - 30.f);
        ImVec2 frameBottomRight = ImVec2(float(width) - 200.f, float(height) * 0.5f + 30.f);
        draw_list->AddRect(frameTopLeft, frameBottomRight, barColor, 0.f, 15, 3.f);

        float frameMargin = 5.f;
        float barFullWidth = frameBottomRight.x - frameTopLeft.x - frameMargin * 2.f;
        float barWidth = barFullWidth * dm::saturate(m_ui.loadingPercentage);
        ImVec2 barTopLeft = ImVec2(frameTopLeft.x + frameMargin, frameTopLeft.y + frameMargin);
        ImVec2 barBottomRight = ImVec2(frameTopLeft.x + frameMargin + barWidth, frameBottomRight.y - frameMargin);
        draw_list->AddRectFilled(barTopLeft, barBottomRight, barColor);

        ImGui::PopFont();
        EndFullScreenWindow();

        return;
    }

    PostProcessSettings();
}
