Texture2D<float3> t_inputTexture : register(t0);
SamplerState s_linearSampler : register(s0);

cbuffer Constants : register(b0)
{
	float level;
	float2 offset;
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
	const float2 texcoord0 = input.texcoord + offset * float2(-1.0f, -1.0f);
	const float2 texcoord1 = input.texcoord + offset * float2(-1.0f, +1.0f);
	const float2 texcoord2 = input.texcoord + offset * float2(+1.0f, -1.0f);
	const float2 texcoord3 = input.texcoord + offset * float2(+1.0f, +1.0f);

	const float3 sample0 = t_inputTexture.SampleLevel(s_linearSampler, texcoord0, level);
	const float3 sample1 = t_inputTexture.SampleLevel(s_linearSampler, texcoord1, level);
	const float3 sample2 = t_inputTexture.SampleLevel(s_linearSampler, texcoord2, level);
	const float3 sample3 = t_inputTexture.SampleLevel(s_linearSampler, texcoord3, level);

	return (sample0 + sample1 + sample2 + sample3) * 0.25f;
}