/***************************************************************************
 # Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#ifndef LIGHTING_CB_H
#define LIGHTING_CB_H

#include <donut/shaders/light_cb.h>
#include <donut/shaders/view_cb.h>

struct LightingConstants
{
    float4 ambientColor;

    LightConstants light;
    PlanarViewConstants view;
    PlanarViewConstants viewPrev;

    // STF Settings
    uint stfSplitScreen;
    uint stfFrameIndex;
    uint stfFilterMode;
    uint stfMagnificationMethod;

    uint stfMinificationMethod;
    uint stfUseMipLevelOverride;
    float stfMipLevelOverride;
    uint stfAddressMode;

    float stfSigma;
    uint stfWaveLaneLayoutOverride;
    uint stfReseedOnSample;
    uint stfUseWhiteNoise;

    uint stfDebugVisualizeLanes;
    uint3 pad;
};

#endif // LIGHTING_CB_H