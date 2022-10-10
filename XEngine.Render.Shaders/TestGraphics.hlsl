float4 MainVS(uint vertexId : SV_VertexID) : SV_Position
{
	float4 position;
	if (vertexId == 0)
		position.xy = float2(0.6f, -0.4f);
	else if (vertexId == 1)
		position.xy = float2(-0.6f, -0.4f);
	else
		position.xy = float2(0.0f, 0.6f);

	position.z = 0.5f;
	position.w = 1.0f;
	return position;
}

float4 MainPS() : SV_Target0
{
	return float4(1.0f, 1.0f, 0.0f, 1.0f);
}
