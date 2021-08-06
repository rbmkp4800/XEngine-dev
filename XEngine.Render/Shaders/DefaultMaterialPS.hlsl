#include "GBuffer.hlsli"
#include "SceneGeometryCommon.hlsli"

struct PSInput
{
	float4 position : SV_Position0;
	float3 normal : NORMAL;
	uint materialIdx : MATERIALIDX;
};

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normalRoughnessMetalness : SV_Target1;
};

PSOutput main(PSInput input)
{
	PSOutput output;
	output.albedo = float4(1.0f, 1.0f, 1.0f, 1.0f);
	output.normalRoughnessMetalness.xy = EncodeNormal(normalize(input.normal));
	output.normalRoughnessMetalness.zw = float2(1.0f, 0.0f);
	return output;
}