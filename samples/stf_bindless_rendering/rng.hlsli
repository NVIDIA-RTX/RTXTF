/*
* Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#ifndef __RNG_HLSLI__
#define __RNG_HLSLI__

struct RNG
{

    // Note: this will turn 0 into 0! if that's a problem do Hash32( x+constant ) - HashCombine does something similar already
    // This little gem is from https://nullprogram.com/blog/2018/07/31/, "Prospecting for Hash Functions" by Chris Wellons
    static uint Hash32(uint x)
    {
        x ^= x >> 17;
        x *= uint(0xed5ad4bb);
        x ^= x >> 11;
        x *= uint(0xac4c1b51);
        x ^= x >> 15;
        x *= uint(0x31848bab);
        x ^= x >> 14;
        return x;
    }

    // popular hash_combine (boost, etc.)
    static uint Hash32Combine(const uint seed, const uint value)
    {
        return seed ^ (Hash32(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }

    static float SampleNext1D(inout uint hash)
    {
        // Use upper 24 bits and divide by 2^24 to get a number u in [0,1).
        // In floating-point precision this also ensures that 1.0-u != 0.0.
        hash = Hash32(hash);
        return (hash >> 8) / float(1 << 24);
    }

    static float2 SampleNext2D(inout uint hash)
    {
        float2 sample;
        // Don't use the float2 initializer to ensure consistent order of evaluation.
        sample.x = SampleNext1D(hash);
        sample.y = SampleNext1D(hash);
        return sample;
    }

    static float3 SampleNext3D(inout uint hash)
    {
        float3 sample;
        // Don't use the float3 initializer to ensure consistent order of evaluation.
        sample.x = SampleNext1D(hash);
        sample.y = SampleNext1D(hash);
        sample.z = SampleNext1D(hash);
        return sample;
    }

    static float STBNBlueNoise1D(uint2 screenCoord, uint frameIndex, Texture2D spatioTemporalBlueNoiseTex)
    {
        uint3 WrappedCoordinate = uint3(screenCoord % 128, frameIndex % 64);
        uint3 TextureCoordinate = uint3(WrappedCoordinate.x, WrappedCoordinate.z * 128 + WrappedCoordinate.y, 0);
        return spatioTemporalBlueNoiseTex.Load(TextureCoordinate, 0).x;
    }

    static float2 STBNBlueNoise2D(uint2 screenCoord, uint frameIndex, Texture2D spatioTemporalBlueNoiseTex)
    {
        uint3 WrappedCoordinate = uint3(screenCoord % 128, frameIndex % 64);
        uint3 TextureCoordinate = uint3(WrappedCoordinate.x, WrappedCoordinate.z * 128 + WrappedCoordinate.y, 0);
        return spatioTemporalBlueNoiseTex.Load(TextureCoordinate, 0).xy;
    }

    static float STWNWhiteNoise1D(uint2 screenCoord, uint frameIndex)
    {
        uint hash = Hash32Combine(Hash32(frameIndex + 0x035F9F29), (screenCoord.x << 16) | screenCoord.y);
        return SampleNext1D(hash);
    }

    static float2 STWNWhiteNoise2D(uint2 screenCoord, uint frameIndex)
    {
        uint hash = Hash32Combine(Hash32(frameIndex + 0x035F9F29), (screenCoord.x << 16) | screenCoord.y);
        return SampleNext2D(hash);
    }

    static float3 STWNWhiteNoise3D(uint2 screenCoord, uint frameIndex)
    {
        uint hash = Hash32Combine(Hash32(frameIndex + 0x035F9F29), (screenCoord.x << 16) | screenCoord.y);
        return SampleNext3D(hash);
    }

    static float3 SpatioTemporalBlueNoise1D(float2 pixel, uint frameIndex, Texture2D spatioTemporalBlueNoiseTex)
    {
        float3 u;
        u.x = STBNBlueNoise1D(uint2(pixel.xy), frameIndex, spatioTemporalBlueNoiseTex);
        u.yz = STWNWhiteNoise2D(uint2(pixel.xy), frameIndex);
        return u;
    }

    static float3 SpatioTemporalBlueNoise2D(float2 pixel, uint frameIndex, Texture2D spatioTemporalBlueNoiseTex)
    {
        float3 u;
        u.xy = STBNBlueNoise2D(uint2(pixel.xy), frameIndex, spatioTemporalBlueNoiseTex);
        u.z  = STWNWhiteNoise1D(uint2(pixel.xy), frameIndex);
        return u;
    }

    static float3 SpatioTemporalWhiteNoise3D(float2 pixel, uint frameIndex)
    {
        float3 u;
        u = STWNWhiteNoise3D(uint2(pixel.xy), frameIndex);
        return u;
    }
};

#endif // __RNG_HLSLI__