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

struct Attributes
{
    float2 uv;
};

[shader("closesthit")]
void ClosestHit(inout RayPayload payload : SV_RayPayload, in Attributes attrib : SV_IntersectionAttributes)
{
    payload.committedRayT = RayTCurrent();
    payload.instanceID = InstanceID();
    payload.primitiveIndex = PrimitiveIndex();
    payload.geometryIndex = GeometryIndex();
    payload.barycentrics = attrib.uv;
}


[shader("anyhit")]
void AnyHit(inout RayPayload payload : SV_RayPayload, in Attributes attrib : SV_IntersectionAttributes)
{
    if (!considerTransparentMaterial(InstanceID(), PrimitiveIndex(), GeometryIndex(), attrib.uv, payload.pixelPosition))
        IgnoreHit();
}