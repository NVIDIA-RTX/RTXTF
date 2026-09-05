# RTXTF Integration Guide

## API functions

### Noise Functions:
[Random Number Generation Header File](../samples/stf_bindless_rendering/rng.hlsli)

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

// Alternatively, if not using a SamplerState (for custom buffer loads like Neural Texture Compression or other texture representations), use a combination of *GetSamplePos* variants together with STF_ApplyAddressingMode
```

### Using Collaborative Texture Filtering

Collaborative Texture Filtering is a technique for magnifying sampled texture values utilizing waves intrinsics. The implementation requires that all texture channels in a material are the same resolution. A concrete example is the function MagnifyTexturesCTF in the sample, in file [stf_bindless_rendering.hlsl](../samples/stf_bindless_rendering/stf_bindless_rendering.hlsl). The general usage pattern for CTF is displayed below:

```C++
// Create and initialize the STF_SamplerState as usual
STF_SamplerState stf = ...

...

// Sample texture values without STF texture sample/load functions (they already apply magnification)
float4 colorA = textureA.rgba;
float4 colorB = textureB.rgba;
float4 colorC = textureC.rgba;

...

// Set up magnification for the material texture
STF_WaveLaneData waveData = stf.Tex2DSetupMagTexels(uv, samplePos, mipSizeX, mipSizeY);
// Use the returned waveData struct to magnify texture values across different channels of the same material
float4 magnifiedColorA = stf.Texture2DMagnifyTexels(colorA, waveData);
float4 magnifiedColorB = stf.Texture2DMagnifyTexels(colorB, waveData);
float4 magnifiedColorC = stf.Texture2DMagnifyTexels(colorC, waveData);
```


The struct STF_WaveLaneData is defined in [STFSamplerStateImpl.hlsli](../libraries/RTXTF-Library/STFSamplerStateImpl.hlsli).

For cases of materials with multiple channels of the same size (diffuse, metallic/roughness, normal, etc) only a single call to Tex2DSetupMagTexels is needed. The returned STF_WaveLaneData struct is reusable across multiple subsequent Texture2DMagnifyTexels calls.

There's a potential for neighboring lanes within the same wave to bleed texels from different materials. However, in practice CTF should get different magnification UVs for different materials. To further protect against potential texel bleeding a material ID check could be employed.

### Filter After Shading

RTXTF provides a noisy output signal that must be filtered with a post shading accumulation pass.  We recommend DLSS-SR for low order filters such as Bilinear but recommend DLSS-RR for higher order filters such as Gaussian.
