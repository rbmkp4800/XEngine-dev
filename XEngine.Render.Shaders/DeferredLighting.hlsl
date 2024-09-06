float sqr(float a) { return a * a; }

////////////////////////////////////////////////////////////////////////////////////////////////////

// https://github.com/KhronosGroup/glTF-WebGL-PBR
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html

float DistributionGGX(float alphaRoughnessSqr, float NdotH)
{
	float f = (NdotH * alphaRoughnessSqr - NdotH) * NdotH + 1.0f;
	return alphaRoughnessSqr / (3.141592654f * sqr(f));
}

float GeometrySmithSchlick(float k, float NdotL, float NdotV)
{
	float l = NdotL * (1.0f - k) + k;
	float v = NdotV * (1.0f - k) + k;

	return rcp(v * l);
}

float3 FresnelSchlick(float3 f0, float NdotH)
{
	return f0 + (1.0f - f0) * pow(1.0f - NdotH, 5.0f);
}

float3 ComputeLighting(float3 diffuseColor, float3 specularColor, float3 lightColor,
	float3 V, float3 L, float3 N, float NdotV,
	float alphaRoughnessSqr, float k)
{
	float3 H = normalize(V + L);

	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));

	float D = DistributionGGX(alphaRoughnessSqr, NdotH);
	float G = GeometrySmithSchlick(k, NdotL, NdotV);
	float3 F = FresnelSchlick(specularColor, NdotH);

	float3 specularContribution = (D * G * F) * 0.25f; // NdotL * NdotV divistion is optimized
	float3 diffuseContribution = (1.0 - F) * diffuseColor;

	return (diffuseContribution + specularContribution) * lightColor * NdotL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct ViewConstantBuffer
{
	float4x4 worldToViewTransform;
	float4x4 worldToClipTransform;
	float4x4 viewToWorldTransform;
	float4 ndcDeprojectionCoefs;
};

struct DeferredLightingConstantBuffer
{
	float3 lightDirection;
};

[[xe::binding( view_constant_buffer )]]
ConstantBuffer<ViewConstantBuffer> bnd_ViewConstantBuffer;

[[xe::binding( deferrred_lighting_constant_buffer )]]
ConstantBuffer<DeferredLightingConstantBuffer> bnd_DeferredLightingConstantBuffer;

[[xe::binding( gbuffer_descriptors::depth )]] Texture2D<float> bnd_DepthTexture;
[[xe::binding( gbuffer_descriptors::gbuffer_a )]] Texture2D<float4> bnd_GBufferATexture;
[[xe::binding( gbuffer_descriptors::gbuffer_b )]] Texture2D<float4> bnd_GBufferBTexture;
[[xe::binding( gbuffer_descriptors::gbuffer_c )]] Texture2D<float2> bnd_GBufferCTexture;

struct PSIntput
{
	float4 position : SV_Position;
	float2 uv : UV;
};

float3 DeprojectNDCPositionToViewSpacePosition(float3 ndcPosition, float4 deprojectionCoefs)
{
	// deprojectionCoefs.x = horizontal tan(FOV/2)
	// deprojectionCoefs.y = vertical tan(FOV/2)
	// deprojectionCoefs.z = (zFar * zNear) / (zFar - zNear)
	// deprojectionCoefs.w = zFar / (zFar - zNear)
	const float linearDepth = deprojectionCoefs.z / (deprojectionCoefs.w - ndcPosition.z); // (zFar * zNear) / (zFar - ndcDepth * (zFar - zNear));
	return float3(ndcPosition.xy * linearDepth * deprojectionCoefs.xy, linearDepth);
}

float4 MainPS(PSIntput input) : SV_Target0
{
	const uint2 pixelCoords = uint2(input.position.xy);

	const float ndcDepth = bnd_DepthTexture.Load(uint3(pixelCoords, 0));

	[branch]
	if (ndcDepth >= 1.0f)
		return float4(0.0f, 0.0f, 0.0f, 0.0f);

	const float3 albedo = bnd_GBufferATexture.Load(uint3(pixelCoords, 0)).xyz;
	const float3 normal = bnd_GBufferBTexture.Load(uint3(pixelCoords, 0)).xyz;
	const float2 roughnessMetalness = bnd_GBufferCTexture.Load(uint3(pixelCoords, 0));
	const float roughness = roughnessMetalness.x;
	const float metalness = roughnessMetalness.y;

	const float3 ndcPosition = float3(input.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), ndcDepth);
	const float3 viewSpacePosition = DeprojectNDCPositionToViewSpacePosition(ndcPosition, bnd_ViewConstantBuffer.ndcDeprojectionCoefs);
	const float3 worldSpacePosition = mul(float4(viewSpacePosition, 1.0f), bnd_ViewConstantBuffer.viewToWorldTransform).xyz;

	const float3 cameraWorldSpacePosition = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), bnd_ViewConstantBuffer.viewToWorldTransform).xyz;
	const float3 V = normalize(cameraWorldSpacePosition - worldSpacePosition);

	// TODO: ...
	// Precompute values:
	//		for DistributionGGX:
	float alphaRoughnessSqr = sqr(sqr(roughness));
	//		for GeometrySmithSchlick:
	float k = sqr(roughness + 1.0f) / 8.0f;
	// float k = sqr(roughness) / 2.0f;
	// float k = sqr(roughness) * sqrt(2.0f / 3.141592654f);
	float NdotV = saturate(dot(normal, V));

	float3 diffuseColor = albedo * (1.0f - metalness) / 3.141592654f;
	float3 specularColor = lerp(0.04f, albedo, metalness);

	const float3 luminance = ComputeLighting(diffuseColor, specularColor,
		float3(1,1,1), V, bnd_DeferredLightingConstantBuffer.lightDirection,
		normal, NdotV, alphaRoughnessSqr, k);

	return float4(luminance, 0);
}
