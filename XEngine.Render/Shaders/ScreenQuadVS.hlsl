struct VSOutput
{
	float4 position : SV_Position;
	float2 ndcPosition : POSITION0;
	float2 texcoord : TEXCOORD0;
};

VSOutput main(uint id : SV_VertexID)
{
	VSOutput output;
	output.position.x = (float)(id / 2) * 4.0f - 1.0f;
	output.position.y = (float)(id % 2) * 4.0f - 1.0f;
	output.position.z = 0.0f;
	output.position.w = 1.0f;
	output.ndcPosition = output.position.xy;
	output.texcoord.x = output.ndcPosition.x * 0.5f + 0.5f;
	output.texcoord.y = -output.ndcPosition.y * 0.5f + 0.5f;
	return output;
}