struct TonemappingConstantBuffer
{
	float exposureScaler;
};

[[xe::binding( tonemapping_constant_buffer )]] ConstantBuffer<TonemappingConstantBuffer> bnd_TonemappingConstantBuffer;
[[xe::binding( tonemapping_input_descriptors::luminance )]] Texture2D<float4> bnd_InputLuminanceTexture;

struct PSIntput
{
	float4 position : SV_Position;
	float2 uv : UV;
};

float4 MainPS(PSIntput input) : SV_Target0
{
	const uint2 pixelCoords = uint2(input.position.xy);

	float3 luminance = bnd_InputLuminanceTexture.Load(uint3(pixelCoords, 0)).xyz;
	luminance *= bnd_TonemappingConstantBuffer.exposureScaler;
	return float4(luminance, 0.0f);
}
