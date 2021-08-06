#define WAVE_SIZE_LOG2 5

// Compilation defines:
//	WAVE_SIZE_LOG2;

#include "HostAndShadersShared.h"
#include "SceneGeometryCommon.hlsli"

#define WAVE_SIZE (1 << WAVE_SIZE_LOG2)

ByteAddressBuffer globalBuffer : register(t0);
StructuredBuffer<float4x3> transformsBuffer : register(t1);
StructuredBuffer<uint4> sectionRecords : register(t2);

groupshared SceneGeometryASPayload payload;

void DecodeSectionRecord(in const uint2 record,
	out uint clustersDataOffset,
	out uint baseTransformIndex,
	out uint materialIndex)
{
	clustersDataOffset = (record.x & 0xFFFFFF) << 8;
	baseTransformIndex = (record.x >> 24) | ((record.y & 0x7FFF) << 8);
	materialIndex = record.y >> 15;
}

// Single wave group
[numthreads(WAVE_SIZE, 1, 1)]
void main(
	const uint globalThreadIdx : SV_DispatchThreadID,
	const uint groupIdx : SV_GroupID,
	const uint groupThreadIdx : SV_GroupIndex)
{
	//uint2 clustersVisibilityMask = 0;

	if (WaveIsFirstLane())
	{
		const uint4 inputSectionRecord = sectionRecords.Load(groupIdx);

		uint clustersDataOffset;
		uint baseTransformIndex;
		uint materialIndex;
		DecodeSectionRecord(inputSectionRecord.xy, clustersDataOffset, baseTransformIndex, materialIndex);

		payload.clustersDataOffset = clustersDataOffset;
		payload.baseTransformIndex = baseTransformIndex;
		payload.materialIndex = materialIndex;

		payload.clustersIndicies[0] = 0;

		//clustersVisibilityMask = inputSectionRecord.xy;
	}

	/*clustersVisibilityMask = WaveReadLaneFirst(clustersVisibilityMask);

	// For now we use just first 32 bits of visibility mask
	bool currentClusterActive = (clustersVisibilityMask.x >> groupThreadIdx) & 1;
	uint currentClusterOffset = WavePrefixCountBits(currentClusterActive);
	uint activeClusterCount = WaveActiveCountBits(currentClusterActive);

	payload.clustersIndicies[currentClusterOffset] = groupThreadIdx;

	DispatchMesh(activeClusterCount, 1, 1, payload);*/

	DispatchMesh(1, 1, 1, payload);
}
