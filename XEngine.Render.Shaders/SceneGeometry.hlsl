struct ViewConstantBuffer
{
	float4x4 localToWorldSpaceTransform;
	float4x4 worldToViewTransform;
	float4x4 worldToClipTransform;
};

[[xe::binding(view_constant_buffer)]] ConstantBuffer<ViewConstantBuffer> bnd_ViewConstantBuffer;
[[xe::binding(default_sampler)]] SamplerState bnd_DefaultSampler;
//[[xe::binding(transform_buffer)]] Buffer<float4> bnd_TransformBuffer;

struct VSInput
{
	uint vertexId : SV_VertexID;
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 uv : UV;
};

struct PSOutput
{
	float4 albedo : SV_Target0;
};

VSOutput MainVS(VSInput input)
{
	const float4x4 localToWorldSpaceTransform = bnd_ViewConstantBuffer.localToWorldSpaceTransform;
	const float3 worldSpacePosition = mul(float4(input.position, 1.0f), localToWorldSpaceTransform).xyz;
	const float3 worldSpaceNormal = mul(input.normal, (float3x3) localToWorldSpaceTransform);

	VSOutput output;
	output.position = mul(float4(worldSpacePosition, 1.0f), bnd_ViewConstantBuffer.worldToClipTransform);
	output.normal = worldSpaceNormal;
	output.uv = input.uv;
	return output;
}

PSOutput MainPS(VSOutput input)
{
	Texture2D<float4> myTexture = ResourceDescriptorHeap[0];
	float4 texSample = myTexture.Sample(bnd_DefaultSampler, input.uv);
	
	PSOutput output;
	output.albedo = float4(texSample.xyz, 1.0f);
	return output;
}
