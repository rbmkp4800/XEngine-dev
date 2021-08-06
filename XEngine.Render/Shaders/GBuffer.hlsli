// Spheremap projection
// https://aras-p.info/texts/CompactNormalStorage.html

float2 EncodeNormal(float3 normal)
{
	return normal.xy / sqrt(normal.z * -8.0f + 8.0f) + 0.5f;
}

float3 DecodeNormal(float2 encodedNormal)
{
	const float2 fenc = encodedNormal * 4.0f - 2.0f;
	const float f = dot(fenc, fenc);
	const float g = sqrt(1.0f - f * 0.25f);
	return float3(fenc * g, f * 0.5f - 1.0f);
}
