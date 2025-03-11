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

[numthreads(THREAD_SIZE_X, THREAD_SIZE_Y, 1)]
void main_cs(uint2 pixelPosition : SV_DispatchThreadID, uint2 groupID : SV_GroupThreadID )
{
	main(pixelPosition);
}
