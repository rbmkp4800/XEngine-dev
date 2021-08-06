Texture2D<float3> t_hdrTexture : register(t0);
//Texture2D<float3> t_bloomTexture : register(t1);
SamplerState t_linearSampler : register(s0);

cbuffer Constants : register(b0)
{
	float exposure;
};

struct PSInput
{
	float4 position : SV_Position;
	// TODO: remove
	float2 ndcPosition : POSITION0;
	float2 texcoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
	const int2 texPosition = int2(input.position.xy);
	float3 luminance = t_hdrTexture[texPosition];

	//luminance += t_bloomTexture.SampleLevel(t_linearSampler, input.texcoord, 0.0f) * 0.1f;
	
	const float3 mappedColor = float3(1.0f, 1.0f, 1.0f) - exp(-luminance * exposure);
	return float4(pow(mappedColor, 1.0f / 2.2f), 1.0f);
}