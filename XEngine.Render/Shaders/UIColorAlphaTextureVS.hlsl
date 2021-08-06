struct VSInput
{
	float2 position : POSITION;
	float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};

struct VSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 texcoord : TEXCOORD;
};

VSOutput main(VSInput input)
{
	VSOutput output;
	output.position = float4(input.position, 0.0f, 1.0f);
	output.color = input.color;
    output.texcoord = input.texcoord;
	return output;
}