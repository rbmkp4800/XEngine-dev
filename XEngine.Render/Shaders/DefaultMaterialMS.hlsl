#include "SceneGeometryCommon.hlsli"
#include "HostAndShadersShared.h"

struct CameraTransformConstants
{
	float4x4 view;
	float4x4 viewProjection;
};

ConstantBuffer<CameraTransformConstants> cameraTransformConstants : register(b0);

ByteAddressBuffer globalBuffer : register(t0);
StructuredBuffer<float4x3> transformsBuffer : register(t1);

struct ClusterDesc
{
	uint dataRelativeOffset; // in bytes
	uint vertexCount;
	uint triangleCount;
};

ClusterDesc DecodeClusterDesc(in const uint4 encoded)
{
	ClusterDesc result;
	result.dataRelativeOffset = (encoded.x & 0x3fFFFF) << 6;
	result.vertexCount = (encoded.x >> 22) & 0x7F;
	result.triangleCount = encoded.y & 0x7F;

	return result;
}

struct GroupShared
{
	float3 vertices[128];
};

groupshared GroupShared groupShared;

struct VertexAttributes
{
	float4 position : SV_Position0;
};

struct TriangleAttributes
{
	float3 normal : NORMAL;
	uint materialIdx : MATERIALIDX;
};

[OutputTopology("triangle")]
[numthreads(128, 1, 1)]
void main(
	in const uint groupIdx : SV_GroupID,
	in const uint groupThreadIdx : SV_GroupIndex,
	in payload SceneGeometryASPayload asPayload,
	out vertices VertexAttributes outVertices[MAX_VERTEX_PER_CLUSTER],
	out indices uint3 outTriangleIndices[MAX_TRIANGLES_PER_CLUSTER],
	out primitives TriangleAttributes outTriangleAttributes[MAX_TRIANGLES_PER_CLUSTER])
{
	const uint clusterIdx = asPayload.clustersIndicies[groupIdx];

	const uint clusterDescAddress = asPayload.clustersDataOffset + clusterIdx * 16;
	const uint4 encodedClusterDesc = globalBuffer.Load4(clusterDescAddress);
	const ClusterDesc clusterDesc = DecodeClusterDesc(encodedClusterDesc);

	SetMeshOutputCounts(clusterDesc.vertexCount, clusterDesc.triangleCount);

	const uint vertexStride = 16;
	const uint triangleStride = 4;

	const uint verticesBlobAddress = clusterDescAddress + clusterDesc.dataRelativeOffset;
	const uint trianglesBlobAddress = verticesBlobAddress + clusterDesc.vertexCount * vertexStride;

	if (groupThreadIdx < clusterDesc.vertexCount)
	{
		const uint vertexDataAddress = verticesBlobAddress + groupThreadIdx * vertexStride;
		const uint4 vertexData = globalBuffer.Load4(vertexDataAddress);

		const float3 position = asfloat(vertexData.xyz);

		const float4x3 transform = transformsBuffer[asPayload.baseTransformIndex];
		const float3 worldSpacePosition = mul(float4(position, 1.0f), transform);
		const float4 clipSpacePosition = mul(cameraTransformConstants.viewProjection, float4(worldSpacePosition, 1.0f));

		outVertices[groupThreadIdx].position = clipSpacePosition;
		groupShared.vertices[groupThreadIdx] = worldSpacePosition;
	}

	GroupMemoryBarrierWithGroupSync();

	if (groupThreadIdx < clusterDesc.triangleCount)
	{
		const uint triangleDataAddress = trianglesBlobAddress + groupThreadIdx * triangleStride;
		const uint triangleData = globalBuffer.Load(triangleDataAddress);

		uint3 triangleIndices = float3(
			(triangleData >> 0 ) & 0x7F,
			(triangleData >> 7 ) & 0x7F,
			(triangleData >> 14) & 0x7F);

		const float3 v0 = groupShared.vertices[triangleIndices.x];
		const float3 v1 = groupShared.vertices[triangleIndices.y];
		const float3 v2 = groupShared.vertices[triangleIndices.z];

		const float3 worldSpaceNormal = cross(v0 - v1, v0 - v2);
		const float3 viewSpaceNormal = mul((float3x3) cameraTransformConstants.view, worldSpaceNormal);

		outTriangleIndices[groupThreadIdx] = triangleIndices;
		outTriangleAttributes[groupThreadIdx].normal = viewSpaceNormal;
		outTriangleAttributes[groupThreadIdx].materialIdx = 0;
	}
}
