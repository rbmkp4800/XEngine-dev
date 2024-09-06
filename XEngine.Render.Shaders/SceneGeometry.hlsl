struct ViewConstantBuffer
{
	float4x4 worldToViewTransform;
	float4x4 worldToClipTransform;
};

struct PerInstanceConstantBuffer
{
	uint transformIndex;
};


[[xe::binding( per_instance_constant_buffer )]] ConstantBuffer<PerInstanceConstantBuffer> bnd_PerInstanceConstantBuffer;
[[xe::binding( view_constant_buffer )]] ConstantBuffer<ViewConstantBuffer> bnd_ViewConstantBuffer;
[[xe::binding( scene_transforms_buffer )]] StructuredBuffer<float4x4> bnd_SceneTransformsBuffer;

[[xe::binding( default_sampler )]] SamplerState bnd_DefaultSampler;

struct VSInput
{
	uint vertexId : SV_VertexID;
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : UV;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 bitangent : BITANGENT;
	float2 uv : UV;
};

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normal : SV_Target1;
	float2 roughtnessMetalness : SV_Target2;
};

VSOutput MainVS(VSInput input)
{
	const uint tranformIndex = bnd_PerInstanceConstantBuffer.transformIndex;

	const float4x4 localToWorldSpaceTransform = bnd_SceneTransformsBuffer[tranformIndex];
	const float3 worldSpacePosition = mul(float4(input.position, 1.0f), localToWorldSpaceTransform).xyz;
	const float3 worldSpaceNormal = mul(input.normal, (float3x3) localToWorldSpaceTransform);
	float3 worldSpaceTangent = mul(input.tangent, (float3x3) localToWorldSpaceTransform);
	
	// https://learnopengl.com/Advanced-Lighting/Normal-Mapping "One last thing"
	// TODO: I copypasted this line and do not understand what is going on here.
	worldSpaceTangent = normalize(worldSpaceTangent - dot(worldSpaceTangent, worldSpaceNormal) * worldSpaceNormal);
	
	const float3 worldSpaceBitangent = cross(worldSpaceNormal, worldSpaceTangent);
	
	VSOutput output;
	output.position = mul(float4(worldSpacePosition, 1.0f), bnd_ViewConstantBuffer.worldToClipTransform);
	output.normal = worldSpaceNormal;
	output.tangent = worldSpaceTangent;
	output.bitangent = worldSpaceBitangent;

	output.uv = input.uv;
	return output;
}

PSOutput MainPS(VSOutput input)
{
	Texture2D<float4> albedoTexture = ResourceDescriptorHeap[0];
	Texture2D<float4> nrmTexture = ResourceDescriptorHeap[1];

	const float3 albedoSample = albedoTexture.Sample(bnd_DefaultSampler, input.uv).xyz;
	const float4 nrmSample = nrmTexture.Sample(bnd_DefaultSampler, input.uv);

	const float2 normalSample = nrmSample.xy * 2.0f - 1.0f;
	const float roughness = nrmSample.z;
	const float metalness = nrmSample.w;

	const float3 tangentSpaceNormal = float3(normalSample, sqrt(saturate(1.0f - dot(normalSample, normalSample))));

	const float3x3 TBN = transpose(float3x3(input.tangent, input.bitangent, input.normal));
	const float3 normal = normalize(mul(TBN, tangentSpaceNormal));

	PSOutput output;
	output.albedo = float4(albedoSample, 1.0f);
	output.normal = float4(normal, 0.0f);
	output.roughtnessMetalness = float2(roughness, metalness);
	return output;
}
