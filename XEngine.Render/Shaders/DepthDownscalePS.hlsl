Texture2D<float> sourceTexture : register(t0);
SamplerState defaultSampler : register(s0);

struct PSInput
{
	float4 position : SV_Position;
	// TODO: remove
	float2 ndcPosition : POSITION0;
	float2 texcoord : TEXCOORD0;
};

float main(PSInput input) : SV_Target
{
	const float4 depth = sourceTexture.GatherRed(defaultSampler, input.texcoord);
	return max(max(depth.x, depth.y), max(depth.z, depth.w));
}