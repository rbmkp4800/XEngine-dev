struct /*[[xe::export_cb_layout(XEngine::Render::Shaders::TestCB)]]*/ TestCB
{
	float4x4 transform;
	float4x4 view;
	float4x4 viewProjection;
};

[[xe::binding(SOME_CONSTANT_BUFFER)]] ConstantBuffer<TestCB> testCB;

struct VSInput
{
	uint vertexId : SV_VertexID;
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texcoord : TEXCOORD;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 texcoord : TEXCOORD;
};

VSOutput MainVS(VSInput input)
{
	const float3 worldSpacePosition = mul(float4(input.position, 1.0f), testCB.transform).xyz;
	const float3 worldSpaceNormal = mul(input.normal, (float3x3) testCB.transform);
	
	VSOutput output;
	output.position = mul(float4(worldSpacePosition, 1.0f), testCB.viewProjection);
	output.normal = worldSpaceNormal;
	output.texcoord = input.texcoord;
	return output;
}

float4 MainPS(VSOutput input) : SV_Target0
{
	float a = saturate(dot(input.normal, normalize(float3(-1, 1, 1))));
	return float4(a.xxx, 1.0f);
}

/////////////////////////////////////////////////////////////////////////

float4 TestTriangleVS(uint vertexId : SV_VertexID) : SV_Position
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

float4 TestTrianglePS() : SV_Target0
{
	return float4(1.0f, 1.0f, 0.0f, 0.0f);
}
