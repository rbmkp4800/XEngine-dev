Texture2D<float3> t_inputTexture : register(t0);
SamplerState s_linearSampler : register(s0);

cbuffer Constants : register(b0)
{
	float2 offset;
}

struct PSInput
{
	float4 position : SV_Position;
	float2 ndcPosition : POSITION0;
	float2 texcoord : TEXCOORD0;
};

float SRGBToLuminance(float3 srgb)
{
	return dot(srgb, float3(0.212671f, 0.71516f, 0.07217f));
}

float3 main(PSInput input) : SV_Target
{
	const float2 texcoord0 = input.texcoord + offset * float2(-1.0f, -1.0f);
	const float2 texcoord1 = input.texcoord + offset * float2(-1.0f, +1.0f);
	const float2 texcoord2 = input.texcoord + offset * float2(+1.0f, -1.0f);
	const float2 texcoord3 = input.texcoord + offset * float2(+1.0f, +1.0f);

	float3 sample0 = t_inputTexture.SampleLevel(s_linearSampler, texcoord0, 0.0f);
	float3 sample1 = t_inputTexture.SampleLevel(s_linearSampler, texcoord1, 0.0f);
	float3 sample2 = t_inputTexture.SampleLevel(s_linearSampler, texcoord2, 0.0f);
	float3 sample3 = t_inputTexture.SampleLevel(s_linearSampler, texcoord3, 0.0f);

	const float luminance0 = SRGBToLuminance(sample0);
	const float luminance1 = SRGBToLuminance(sample1);
	const float luminance2 = SRGBToLuminance(sample2);
	const float luminance3 = SRGBToLuminance(sample3);
	
	static const float threshold = 1.0f;

	sample0 *= max(0.0f, luminance0 - threshold) / (luminance0 + 0.001f);
	sample1 *= max(0.0f, luminance1 - threshold) / (luminance1 + 0.001f);
	sample2 *= max(0.0f, luminance2 - threshold) / (luminance2 + 0.001f);
	sample3 *= max(0.0f, luminance3 - threshold) / (luminance3 + 0.001f);

	return (sample0 + sample1 + sample2 + sample3) * 0.25f;
}