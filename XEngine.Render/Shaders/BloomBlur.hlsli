float3 BloomBlur(Texture2D<float3> inputTexture, SamplerState linearSampler, const float2 texcoord, const float2 step, const float level)
{
	static const float weight0 = 0.227027029f;
	static const float weight1 = 0.316216230f;
	static const float weight2 = 0.0702702701f;
	
	static const float offset1 = 1.38461530f;
	static const float offset2 = 3.23076916f;
	
	const float3 sample0 = inputTexture.SampleLevel(linearSampler, texcoord, level);
	const float3 sampleP1 = inputTexture.SampleLevel(linearSampler, texcoord + step * offset1, level);
	const float3 sampleP2 = inputTexture.SampleLevel(linearSampler, texcoord + step * offset2, level);
	const float3 sampleN1 = inputTexture.SampleLevel(linearSampler, texcoord - step * offset1, level);
	const float3 sampleN2 = inputTexture.SampleLevel(linearSampler, texcoord - step * offset2, level);
	
	return sample0 * weight0 + (sampleP1 + sampleN1) * weight1 + (sampleP2 + sampleN2) * weight2;
}