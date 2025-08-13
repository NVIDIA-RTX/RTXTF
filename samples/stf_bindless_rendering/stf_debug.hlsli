/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
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

float4 GetCividis(int index, int min, int max)
{
    const float t = (float)index / (max - min);

    const float t2 = t * t;
    const float t3 = t2 * t;
    
    float4 color;
    color.r = 0.8688 * t3 - 1.5484 * t2 + 0.0081 * t + 0.2536;
    color.g = 0.8353 * t3 - 1.6375 * t2 + 0.2351 * t + 0.8669;
    color.b = 0.6812 * t3 - 1.0197 * t2 + 0.3935 * t + 0.8815;
    color.a = 0.f;
    
    return color;
}

// https://www.shadertoy.com/view/Nl3BRN
uint hashUint2(uint2 value)
{
    uint hash = 23u;
    hash = hash * 31u + value.x;
    hash = hash * 31u + value.y;
    return hash;
}

// https://stackoverflow.com/a/3887197
uint hashRemap(uint hash)
{
    uint h = (hash << 15u) ^ 0xffffcd7du;
    h ^= (h >> 10);
    h += (h << 3);
    h ^= (h >> 6);
    h += (h << 2) + (h << 14);
    return (h ^ (h >> 16));
}

float3 uintToColor(uint x)
{
    return float3(
        float((x >> 0u) & 0xffu) / 255.0,
        float((x >> 8u) & 0xffu) / 255.0,
        float((x >> 16u) & 0xffu) / 255.0
    );
}

bool GetWaveVizMode(int stfDebugVisualizeLanes, uint2 pixel, out float4 color)
{
    color = 0.f.xxxx;

    if (stfDebugVisualizeLanes == 0)
    {
        return false;
    }
    else if (stfDebugVisualizeLanes == 1)
    {
        color = GetCividis(WaveGetLaneIndex(), 0, WaveGetLaneCount());
        return true;
    }
    else // if (stfDebugVisualizeLanes == 2)
    {
        uint hash = hashUint2(pixel);
        hash = hashRemap(hash);
        color = WaveReadLaneFirst(uintToColor(hash)).xyzz;
        return true;
    }
}