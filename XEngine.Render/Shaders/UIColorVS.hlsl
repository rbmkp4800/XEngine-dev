struct VSInput
{
	float2 position : POSITION;
	float4 color : COLOR;
};

struct VSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR;
};

VSOutput main(VSInput input)
{
	VSOutput output;
	output.position = float4(input.position, 0.0f, 1.0f);
	output.color = input.color;
	return output;
}