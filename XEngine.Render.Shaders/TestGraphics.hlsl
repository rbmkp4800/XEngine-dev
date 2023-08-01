struct /*[[xe::export_cb_layout(XEngine::Render::Shaders::TestCB)]]*/ TestCB
{
	float4 c;
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
	VSOutput output;
	output.position = float4(input.position, 1.0f);
	output.normal = input.normal;
	output.texcoord = input.texcoord;
	return output;
}

float4 MainPS() : SV_Target0
{
	return testCB.c;
}
