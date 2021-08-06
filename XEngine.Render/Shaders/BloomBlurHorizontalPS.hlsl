#include "BloomBlur.hlsli"

Texture2D<float3> t_inputTexture : register(t0);
SamplerState s_linearSampler : register(s0);

cbuffer Constants : register(b0)
{
	float level;
	float inverseInputSize;
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
	return BloomBlur(t_inputTexture, s_linearSampler, input.texcoord, float2(inverseInputSize, 0.0f), level);
}