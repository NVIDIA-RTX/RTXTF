/***************************************************************************
 # Copyright (c) 2025, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#include "stf_bindless_rendering.hlsl"

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
}

[shader("raygeneration")]
void RayGen()
{
    uint2 pixelPosition = DispatchRaysIndex().xy;
    main(pixelPosition);
}
