#include "BloomBlur.hlsli"

Texture2D<float3> t_bloomTextureA : register(t0);
Texture2D<float3> t_bloomTextureB : register(t1);
SamplerState s_linearSampler : register(s0);

cbuffer Constants : register(b0)
{
	float level;
	float inverseInputSize;
	float accumulationFactor;
}

struct PSInput
{
	float4 position : SV_Position;
	// TODO: remove
	float2 ndcPosition : POSITION0;
	float2 texcoord : TEXCOORD0;
};

float3 main(PSInput input) : SV_Target
{
	const float3 accumulatedBloomSample = t_bloomTextureA.SampleLevel(s_linearSampler, input.texcoord, level + 1.0f);
	const float3 blurResult = BloomBlur(t_bloomTextureB, s_linearSampler, input.texcoord, float2(0.0f, inverseInputSize), level);

	return accumulatedBloomSample * accumulationFactor + blurResult;
}