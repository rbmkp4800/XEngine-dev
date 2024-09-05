struct ViewConstantBuffer
{
	float4x4 worldToViewTransform;
	float4x4 worldToClipTransform;
};

struct DeferredLightingConstantBuffer
{
	float3 lightDirection;
};

[[xe::binding( view_constant_buffer )]]
ConstantBuffer<ViewConstantBuffer> bnd_ViewConstantBuffer;

[[xe::binding( deferrred_lighting_constant_buffer )]]
ConstantBuffer<DeferredLightingConstantBuffer> bnd_DeferredLightingConstantBuffer;

[[xe::binding( gbuffer_descriptors::gbuffer_a )]] Texture2D<float4> bnd_GBufferA;
[[xe::binding( gbuffer_descriptors::gbuffer_b )]] Texture2D<float4> bnd_GBufferB;
[[xe::binding( gbuffer_descriptors::gbuffer_c )]] Texture2D<float2> bnd_GBufferC;

struct PSIntput
{
	float4 position : SV_Position;
	float2 uv : UV;
};

float4 MainPS(PSIntput input) : SV_Target0
{
	const uint2 pixelCoords = uint2(input.position.xy);
	const float3 albedo = bnd_GBufferA.Load(uint3(pixelCoords, 0)).xyz;
	const float3 normal = bnd_GBufferB.Load(uint3(pixelCoords, 0)).xyz;

	float l = saturate(dot(normal, bnd_DeferredLightingConstantBuffer.lightDirection));

	return float4(albedo * l, 1);
}
