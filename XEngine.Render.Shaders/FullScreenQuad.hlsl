struct VSOutput
{
	float4 position : SV_Position;
	float2 uv : UV;
};

VSOutput MainVS(uint id : SV_VertexID)
{
	const float2 c = float2(id >> 1, id & 1);

	VSOutput output;
	output.position = float4(c * 4.0f - 1.0f, 0.0f, 1.0f);
	output.uv = float2(2.0f * c.x, 1.0f - 2.0f * c.y);
	return output;
}
