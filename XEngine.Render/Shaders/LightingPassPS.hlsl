// NOTE: view-world space logic is completely broken
// TODO: fix that

#include "GBuffer.hlsli"

struct DirectionalLight
{
	float4x4 shadowTextureTransform;
	float3 viewSpaceDirection;
	float3 color;
};

struct PointLight
{
	float4x4 shadowTextureTransforms[6];
	float3 viewSpacePosition;
	float3 color;
};

cbuffer Constants : register(b0)
{
	float4x4 view;
	float4x4 projection;
	float ndcToViewDepthConversionA;
	float ndcToViewDepthConversionB;
	float aspect;
	float halfFOVTan;

	/*uint directionalLightCount;
	uint pointLightCount;

	DirectionalLight directionalLights[2];
	PointLight pointLights[4];*/
};

Texture2D<float4> albedoTexture : register(t0);
Texture2D<float4> normalRoughnessMetalnessTexture : register(t1);
Texture2D<float> depthTexture : register(t2);
//Texture2DArray<float> directionalLightsShadowMaps : register(t3);
//Texture2DArray<float> pointLightsShadowMaps : register(t4);

//SamplerComparisonState shadowSampler : register(s0);

struct PSInput
{
	float4 position : SV_Position;
	float2 ndcPosition : POSITION0;
};

float sqr(float value) { return value * value; }

float NDCDepthToViewSpaceDepth(float ndcDepth)
{
	// result = (zFar * zNear) / (zFar - ndcDepth * (zFar - zNear));

	// optimized:
	// ndcToViewDepthConversionA = (zFar * zNear) / (zFar - zNear);
	// ndcToViewDepthConversionB = zFar / (zFar - zNear);

	return ndcToViewDepthConversionA / (ndcToViewDepthConversionB - ndcDepth);
}

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

uint GetCubeFaceIndex(float3 v)
{
	// UnrealEngine/Engine/Shaders/Private/ShadowProjectionCommon.ush: CubemapHardwarePCF()

	float3 absv = abs(v);
	float maxCoordinate = max(absv.x, max(absv.y, absv.z));

	int cubeFaceIndex = 0;
	if (maxCoordinate == absv.x)
		cubeFaceIndex = absv.x == v.x ? 0 : 1;
	else if (maxCoordinate == absv.y)
		cubeFaceIndex = absv.y == v.y ? 2 : 3;
	else
		cubeFaceIndex = absv.z == v.z ? 4 : 5;

	return cubeFaceIndex;
}

/*float SampleShadowMap(Texture2DArray<float> shadowMaps, uint shadowMapIndex,
	float shadowMapDim, float2 shadowTexcoord, float comparisionDepth)
{
	// https://github.com/TheRealMJP/Shadows Optimized PCF
	// 5x5 PCF using 9 samples

    const float invShadowMapSize = rcp(shadowMapDim);
	const float2 uv = shadowTexcoord * shadowMapDim;

	const float2 alignedUV = floor(uv + float2(0.5f, 0.5f)) - float2(0.5f, 0.5f);
	const float2 offset = uv - alignedUV;
	const float2 alignedCenterTexcoord = alignedUV * invShadowMapSize;

	const float uw0 = 4.0f - 3.0f * offset.x;
	const float uw1 = 7.0f;
	const float uw2 = 1.0f + 3.0f * offset.x;

	const float vw0 = 4.0f - 3.0f * offset.y;
	const float vw1 = 7.0f;
	const float vw2 = 1.0f + 3.0f * offset.y;

	const float u0 = alignedCenterTexcoord.x + invShadowMapSize * ((3.0f - 2.0f * offset.x) / uw0 - 2.0f);
	const float u1 = alignedCenterTexcoord.x + invShadowMapSize * ((3.0f + offset.x) / uw1);
	const float u2 = alignedCenterTexcoord.x + invShadowMapSize * (offset.x / uw2 + 2.0f);

	const float v0 = alignedCenterTexcoord.y + invShadowMapSize * ((3.0f - 2.0f * offset.y) / vw0 - 2.0f);
	const float v1 = alignedCenterTexcoord.y + invShadowMapSize * ((3.0f + offset.y) / vw1);
	const float v2 = alignedCenterTexcoord.y + invShadowMapSize * (offset.y / vw2 + 2.0f);

	float sum = 0.0f;
	sum += uw0 * vw0 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u0, v0, shadowMapIndex), comparisionDepth);
	sum += uw1 * vw0 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u1, v0, shadowMapIndex), comparisionDepth);
	sum += uw2 * vw0 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u2, v0, shadowMapIndex), comparisionDepth);
	sum += uw0 * vw1 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u0, v1, shadowMapIndex), comparisionDepth);
	sum += uw1 * vw1 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u1, v1, shadowMapIndex), comparisionDepth);
	sum += uw2 * vw1 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u2, v1, shadowMapIndex), comparisionDepth);
	sum += uw0 * vw2 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u0, v2, shadowMapIndex), comparisionDepth);
	sum += uw1 * vw2 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u1, v2, shadowMapIndex), comparisionDepth);
	sum += uw2 * vw2 * shadowMaps.SampleCmpLevelZero(shadowSampler, float3(u2, v2, shadowMapIndex), comparisionDepth);

	return sum * 1.0f / 144.0f;
}*/

float4 main(PSInput input) : SV_Target
{
	int2 texPosition = int2(input.position.xy);

	float ndcDepth = depthTexture[texPosition];
	if (ndcDepth == 1.0f)
	{
		// return float4(0.0f, 0.0f, 0.0f, 1.0f);

		float3 viewRayDir = normalize(float3(input.ndcPosition / projection._m00_m11, 1.0f));
		float3 worldRayDir = mul(viewRayDir, (float3x3)view);


		float lll = saturate(pow(0.5f + 0.5f * dot(worldRayDir, normalize(float3(1.0f, 1.0f, 1.0f))), 3.0f));
		float sss = worldRayDir.z + 0.3f;

		return float4(
			sss * float3(0.1f, 0.27f, 0.9f) +
			lll * float3(0.99f, 0.9f, 0.7f) * 0.5f, 1.0f);
	}

	float viewSpaceDepth = NDCDepthToViewSpaceDepth(ndcDepth);
	float3 viewSpacePosition = float3(input.ndcPosition * viewSpaceDepth * halfFOVTan, viewSpaceDepth);
	viewSpacePosition.x *= aspect;
	float3 V = -normalize(viewSpacePosition);

	float3 albedo = pow(albedoTexture[texPosition].xyz, 2.2f);
	float4 normalRoughnessMetalness = normalRoughnessMetalnessTexture[texPosition];
	float3 normal = DecodeNormal(normalRoughnessMetalness.xy);
	float roughness = normalRoughnessMetalness.z;
	float metalness = normalRoughnessMetalness.w;

	float3 worldSpacePosition = mul(float4(viewSpacePosition, 1.0f), (float4x3) view);

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

	float3 sum = 0.0f;
	
	{
		float3 wsDir = normalize(float3(1.0f, 1.0f, 1.0f));
		float3 vsDir = mul((float3x3)view, wsDir);

		float3 lighting = ComputeLighting(diffuseColor, specularColor,
			float3(1,1,1), V, vsDir,
			normal, NdotV, alphaRoughnessSqr, k);

		sum += lighting;
	}

	// directional lights
	/* [loop]
	for (uint i = 0; i < directionalLightCount; i++)
	{
		const float3 shadowSpacePosition = mul(directionalLights[i].shadowTextureTransform, float4(worldSpacePosition, 1.0f)).xyz;
		const float shadowFactor = SampleShadowMap(directionalLightsShadowMaps, 0, 2048.0f, shadowSpacePosition.xy, shadowSpacePosition.z - 0.01f);

		float3 lighting = ComputeLighting(diffuseColor, specularColor,
			directionalLights[i].color, V, directionalLights[i].viewSpaceDirection,
			normal, NdotV, alphaRoughnessSqr, k);

		sum += lighting * sqr(shadowFactor);
	}

	// point lights
	[loop]
	for (uint i = 0; i < pointLightCount; i++) 
	{
		const float3 lightWorldSpacePosition = mul((float3x4) inverseView, float4(pointLights[i].viewSpacePosition, 1.0f));
		const float3 worldLightVector = worldSpacePosition - lightWorldSpacePosition;
		const uint cubeFaceIndex = GetCubeFaceIndex(worldLightVector);

		float4 shadowSpacePosition = mul(pointLights[i].shadowTextureTransforms[cubeFaceIndex], float4(worldSpacePosition, 1.0f));
		shadowSpacePosition.xyz *= rcp(shadowSpacePosition.w);

		const float shadowFactor = SampleShadowMap(pointLightsShadowMaps, cubeFaceIndex, 512.0f, shadowSpacePosition.xy, shadowSpacePosition.z - 0.01f / shadowSpacePosition.w);

		float3 lightVector = pointLights[i].viewSpacePosition - viewSpacePosition;
		float invLightDistance = rcp(length(lightVector));
		float3 L = lightVector * invLightDistance; // normalized light vector

		float3 lighting = ComputeLighting(diffuseColor, specularColor,
			pointLights[i].color * sqr(invLightDistance), V, L,
			normal, NdotV, alphaRoughnessSqr, k);

		sum += lighting * sqr(shadowFactor);
	}*/

	float3 ambient = 0.03f * albedo;

	return float4(ambient + sum, 1.0f);
}