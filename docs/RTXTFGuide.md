# RTXTF Integration Guide

## API functions

### Noise Functions:
[Random Number Generation Header File][RNG]

Three helper functions that generate different noise variants that return 3 random values.

```C++
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
```

### RTXTF Files
Simply include RTXTF-Library/STFSamplerState.hlsli into your shader to include all necessary RTXTF functionality.<br/>
**1.** STFDefinitions.h contains constants related to RTXTF functionality.<br/>
**2.** STFMacros.hlsli allows certain functionality to be available based on shader stages and shader language.<br/>
**3.** STFSampleStateImpl.hlsli contains the implementation details of the interface.<br/>
**4.** STFSamplerState.hlsli acts as the interface function table.<br/>
	
### RTXTF Interface functions in STFSamplerState.hlsli

```C++
// u - Uniform random number(s) Format: 2D(Array) tex : [u, v, (slice), mip], 3D tex : [u, v, w, mip]
// If 
static STF_SamplerState Create(float4 u) {
	STF_SamplerState s;
	s.m_impl = STF_SamplerStateImpl::_Create(u);
	return s;
}

STF_MUTATING void SetUniformRandom(float4 u)                 { m_impl.m_u = u; }                               /* Uniform random numbers, 2D(Array) tex : [u, v, (slice), mip], 3D tex : [u, v, w, mip]  */
STF_MUTATING void SetFilterType(uint filterType)             { m_impl.m_filterType = filterType; }             /*STF_FILTER_TYPE_*/
STF_MUTATING void SetFrameIndex(uint frameIndex)             { m_impl.m_frameIndex = frameIndex; }             /* Frame index used to calculate odd / even frames for STF_MAGNIFICATION_METHOD_2x2_FINE_TEMPORAL)*/
STF_MUTATING void SetAnisoMethod(uint anisoMethod)           { m_impl.m_anisoMethod = anisoMethod; }           /*STF_ANISO_LOD_METHOD_*/
STF_MUTATING void SetMagMethod(uint magMethod)               { m_impl.m_magMethod = magMethod; }               /*STF_MAGNIFICATION_METHOD_*/
STF_MUTATING void SetAddressingModes(uint3 addressingModes)  { m_impl.m_addressingModes = addressingModes; }   /*STF_ADDRESS_MODE_* - Only applied when using 'Load*/
STF_MUTATING void SetSigma(float sigma)                      { m_impl.m_sigma = sigma; }
STF_MUTATING void SetReseedOnSample(bool reseedOnSample)     { m_impl.m_reseedOnSample = reseedOnSample; }     /*Each sample will update the random numbers default: false*/
STF_MUTATING void SetUserData(uint4 userData)                { m_impl.m_userData = userData; }

float4 GetUniformRandom()         { return m_impl.m_u; }
uint   GetFilterType()            { return m_impl.m_filterType; }           /*STF_FILTER_TYPE_*/
uint   GetFrameIndex()            { return m_impl.m_frameIndex; }
uint   GetAnisoMethod()           { return m_impl.m_anisoMethod; }          /*STF_ANISO_LOD_METHOD_*/
uint   GetMagMethod()             { return m_impl.m_magMethod; }            /*STF_MAGNIFICATION_METHOD_*/
uint3  GetAddressingModes()       { return m_impl.m_addressingModes; }      /*STF_ADDRESS_MODE_* - Only applied when using 'Load'*/ 
float  GetSigma()                 { return m_impl.m_sigma; }
bool   GetReseedOnSample()        { return m_impl.m_reseedOnSample; }       /*Each sample will update the random numbers*/
uint4  GetUserData()              { return m_impl.m_userData; }

// Texture2D with Texture and SamplerState objects, will use 'tex.Sample' internally
STF_MUTATING float4 Texture2DSample(Texture2D tex, SamplerState s, float2 uv) { return m_impl._Texture2DSample(tex, s, uv); }
STF_MUTATING float4 Texture2DSampleGrad(Texture2D tex, SamplerState s, float2 uv, float2 ddxUV, float2 ddyUV) { return m_impl._Texture2DSampleGrad(tex, s, uv, ddxUV, ddyUV); }
STF_MUTATING float4 Texture2DSampleLevel(Texture2D tex, SamplerState s, float2 uv, float mipLevel) { return m_impl._Texture2DSampleLevel(tex, s, uv, mipLevel); }
STF_MUTATING float4 Texture2DSampleBias(Texture2D tex, SamplerState s, float2 uv, float mipBias) { return m_impl._Texture2DSampleBias(tex, s, uv, mipBias); }

// Texture2D with Texture objects, will use 'tex.Load' internally
STF_MUTATING float4 Texture2DLoad(Texture2D tex, float2 uv) { return m_impl._Texture2DLoad(tex, uv); }
STF_MUTATING float4 Texture2DLoadGrad(Texture2D tex, float2 uv, float2 ddxUV, float2 ddyUV) { return m_impl._Texture2DLoadGrad(tex, uv, ddxUV, ddyUV); }
STF_MUTATING float4 Texture2DLoadLevel(Texture2D tex, float2 uv, float mipLevel) { return m_impl._Texture2DLoadLevel(tex, uv, mipLevel); }
STF_MUTATING float4 Texture2DLoadBias(Texture2D tex, float2 uv, float mipBias) { return m_impl._Texture2DLoadBias(tex, uv, mipBias); }

// Texture2D/Texture2DArray without Texture objects.
// These functions return float3(x, y, lod) where (x, y) point at texel centers in UV space, lod is integer.
// Note: use floor(f) to convert the sample positions to integer texel coordinates, not round(f).
STF_MUTATING float3 Texture2DGetSamplePos(uint width, uint height, uint numberOfLevels, float2 uv) { return m_impl._Texture2DGetSamplePos(width, height, numberOfLevels, uv); }
STF_MUTATING float3 Texture2DGetSamplePosGrad(uint width, uint height, uint numberOfLevels, float2 uv, float2 ddxUV, float2 ddyUV) { return m_impl._Texture2DGetSamplePosGrad(width, height, numberOfLevels, uv, ddxUV, ddyUV); }
STF_MUTATING float3 Texture2DGetSamplePosLevel(uint width, uint height, uint numberOfLevels, float2 uv, float mipLevel) { return m_impl._Texture2DGetSamplePosLevel(width, height, numberOfLevels, uv, mipLevel); }
STF_MUTATING float3 Texture2DGetSamplePosBias(uint width, uint height, uint numberOfLevels, float2 uv, float mipBias) { return m_impl._Texture2DGetSamplePosBias(width, height, numberOfLevels, uv, mipBias); }

// Texture2DArray with Texture and SamplerState objects, will use 'tex.Sample' internally
STF_MUTATING float4 Texture2DArraySample(Texture2DArray tex, SamplerState s, float3 uv) { return m_impl._Texture2DArraySample(tex, s, uv); }
STF_MUTATING float4 Texture2DArraySampleGrad(Texture2DArray tex, SamplerState s, float3 uv, float3 ddxUV, float3 ddyUV) { return m_impl._Texture2DArraySampleGrad(tex, s, uv, ddxUV, ddyUV); }
STF_MUTATING float4 Texture2DArraySampleLevel(Texture2DArray tex, SamplerState s, float3 uv, float mipLevel) { return m_impl._Texture2DArraySampleLevel(tex, s, uv, mipLevel); }
STF_MUTATING float4 Texture2DArraySampleBias(Texture2DArray tex, SamplerState s, float3 uv, float mipBias) { return m_impl._Texture2DArraySampleBias(tex, s, uv, mipBias); }

// Texture2DArray with Texture objects, will use 'tex.Load' internally
STF_MUTATING float4 Texture2DArrayLoad(Texture2DArray tex, float3 uv) { return m_impl._Texture2DArrayLoad(tex, uv); }
STF_MUTATING float4 Texture2DArrayLoadGrad(Texture2DArray tex, float3 uv, float3 ddxUV, float3 ddyUV) { return m_impl._Texture2DArrayLoadGrad(tex, uv, ddxUV, ddyUV); }
STF_MUTATING float4 Texture2DArrayLoadLevel(Texture2DArray tex, float3 uv, float mipLevel) { return m_impl._Texture2DArrayLoadLevel(tex, uv, mipLevel); }
STF_MUTATING float4 Texture2DArrayLoadBias(Texture2DArray tex, float3 uv, float mipBias) { return m_impl._Texture2DArrayLoadBias(tex, uv, mipBias); }

// Texture3D with Texture and SamplerState objects, will use 'tex.Sample' internally
STF_MUTATING float4 Texture3DSample(Texture3D tex, SamplerState s, float3 uv) { return m_impl._Texture3DSample(tex, s, uv); }
STF_MUTATING float4 Texture3DSampleGrad(Texture3D tex, SamplerState s, float3 uv, float3 ddxUV, float3 ddyUV) { return m_impl._Texture3DSampleGrad(tex, s, uv, ddxUV, ddyUV); }
STF_MUTATING float4 Texture3DSampleLevel(Texture3D tex, SamplerState s, float3 uv, float mipLevel) { return m_impl._Texture3DSampleLevel(tex, s, uv, mipLevel); }
STF_MUTATING float4 Texture3DSampleBias(Texture3D tex, SamplerState s, float3 uv, float mipBias) { return m_impl._Texture3DSampleBias(tex, s, uv, mipBias); }

// Texture3D with Texture objects, will use 'tex.Load' internally
STF_MUTATING float4 Texture3DLoad(Texture3D tex, float3 uv) { return m_impl._Texture3DLoad(tex, uv); }
STF_MUTATING float4 Texture3DLoadGrad(Texture3D tex, float3 uv, float3 ddxUV, float3 ddyUV) { return m_impl._Texture3DLoadGrad(tex, uv, ddxUV, ddyUV); }
STF_MUTATING float4 Texture3DLoadLevel(Texture3D tex, float3 uv, float mipLevel) { return m_impl._Texture3DLoadLevel(tex, uv, mipLevel); }
STF_MUTATING float4 Texture3DLoadBias(Texture3D tex, float3 uv, float mipBias) { return m_impl._Texture3DLoadBias(tex, uv, mipBias); }

// Texture3D without Texture objects.
// These functions return float4(x, y, z, lod) where (x, y, z) point at texel centers in UV space, lod is integer.
STF_MUTATING float4 Texture3DGetSamplePos(uint width, uint height, uint depth, uint numberOfLevels, float3 uv) { return m_impl._Texture3DGetSamplePos(width, height, depth, numberOfLevels, uv); }
STF_MUTATING float4 Texture3DGetSamplePosGrad(uint width, uint height, uint depth, uint numberOfLevels, float3 uv, float3 ddxUV, float3 ddyUV) { return m_impl._Texture3DGetSamplePosGrad(width, height, depth, numberOfLevels, uv, ddxUV, ddyUV); }
STF_MUTATING float4 Texture3DGetSamplePosLevel(uint width, uint height, uint depth, uint numberOfLevels, float3 uv, float mipLevel) { return m_impl._Texture3DGetSamplePosLevel(width, height, depth, numberOfLevels, uv, mipLevel); }
STF_MUTATING float4 Texture3DGetSamplePosBias(uint width, uint height, uint depth, uint numberOfLevels, float3 uv, float mipBias) { return m_impl._Texture3DGetSamplePosBias(width, height, depth, numberOfLevels, uv, mipBias); }

// TextureCube with Texture and SamplerState objects, will use 'tex.Sample' internally.  Load variant isn't supported according to hlsl spec.  https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/texturecube
STF_MUTATING float4 TextureCubeSample(TextureCube tex, SamplerState s, float3 uv) { return m_impl._TextureCubeSample(tex, s, uv); }
STF_MUTATING float4 TextureCubeSampleGrad(TextureCube tex, SamplerState s, float3 uv, float3 ddxUV, float3 ddyUV) { return m_impl._TextureCubeSampleGrad(tex, s, uv, ddxUV, ddyUV); }
STF_MUTATING float4 TextureCubeSampleLevel(TextureCube tex, SamplerState s, float3 uv, float mipLevel) { return m_impl._TextureCubeSampleLevel(tex, s, uv, mipLevel); }
STF_MUTATING float4 TextureCubeSampleBias(TextureCube tex, SamplerState s, float3 uv, float mipBias) { return m_impl._TextureCubeSampleBias(tex, s, uv, mipBias); }
```
	
## Integrating RTXTF in the Sample Application

### Generate noise input

**1.**  All noise used for stochastic probabilities must be passed in by the client.  We recommend using a Spatio-Temporal Blue Noise random pattern to improve convergence of the RTXTF signal.
To learn more about Blue Noise and how it helps with signal convergence we recommend reading some of the articles linked here: https://github.com/NVIDIAGameWorks/SpatiotemporalBlueNoiseSDK.

**2.**  We provide a simple random number generator hlsl header file that requires the pixel position in screen space and current frame index to maintain a consistent noise sequence temporally.
A simple Blue Noise texture is provided in which you can pass to RNG::SpatioTemporalBlueNoise2D to provide 64 unique blue noise samples across 64 frames.

### Interface Methods

There are multiple ways to integrate RTXTF shader library into your shader framework:

```C++
------------------------------------------------------------------------------------------
							 STF_SamplerState example usage 
------------------------------------------------------------------------------------------
To enable RTXTF replace the regular texture sampling with an instance of STF_SamplerState. E.g

float4 color = tex.Sample(sampler, texCoord);

Becomes:

[1] const float2 u          = ... uniform random number(s) [0,1]
[2] STF_SamplerState stf    = STF_SamplerState::Create(u.xyy, STF_FILTER_TYPE_LINEAR);
[3] float4 color            = stf.Texture2DSample(tex, sampler, texCoord);

------------------------------------------------------------------------------------------

[1]  Generate random uniform numbers [0,1]. For 2D textures only need to set xy, last component can be left as zero. For 3D set xyz.
For primary rays or use in rasterization blue noise samping produce the best quality. STBN is recommended: https://github.com/NVIDIAGameWorks/SpatiotemporalBlueNoiseSD.
See sample application for more details.
const float2 u = stbnTexture.Load[uint3(pixel.xy % XY, frameIndex % N)];
Alternatively, use your favourite rng 
const float2 u = yourFavouriteHash( ... );

[2] Initialize the STF_SamplerState using the random number and desired filter type.
STF_SamplerState stf = STF_SamplerState::Create(u.xyy, STF_FILTER_TYPE_LINEAR);

Use the RTXTF in place of the normal texture sampler:

float4 colorA = stf.Texture2DSample(textureA, sampleState, texCoord);

// ... 
[3] It's safe to re-use the STF_SamplerState for multiple texture fetches on different textures and sampler states.
the internal state(the rng) will be updated.
Note that the sampler wrap mode will be applied.

float4 colorA = stf.Texture2DSample(textureA, sampleState, texCoord);
float4 colorB = stf.Texture2DSample(textureB, sampleState, texCoord);

// Alternatively, if not using a SamplerState (for custom buffer loads like Neural Texture Compression or other texture representations), use 
a combination of *GetSamplePos* variants together with STF_ApplyAddressingMode
```

### Filter After Shading

RTXTF provides a noisy output signal that must be filtered with a post shading accumulation pass.  We recommend DLSS-SR for low order filters such as Bilinear but recommend DLSS-RR for higher order filters such as Gaussian.

### Misc/Future Work

The RTXTF Shader Library can be extended to implement various types of stochastic sampling alogrithms including screen space techniques.  Currently that is out of scope for this simple shader library.

[RNG]: examples/stf_bindless_rendering/rng.hlsli