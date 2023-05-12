struct VSOutput
{
	float4 position : SV_Position;
};

VSOutput main(uint id : SV_VertexID)
{
	VSOutput output;
	output.position.xy = float2(id / 2, id % 2) * 4.0f - 1.0f;
	output.position.zw = float2(0.0f, 1.0f);
	return output;
}
