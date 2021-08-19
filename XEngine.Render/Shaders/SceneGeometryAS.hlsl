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
void main(const uint groupIdx : SV_GroupID)
{
	const uint waveThreadIdx = WaveGetLaneIndex();

#if MAX_CLUSTERS_PER_SECTION != 64
	#error this code implies `MAX_CLUSTERS_PER_SECTION == 64`
#endif

#if (MAX_CLUSTERS_PER_SECTION / WAVE_SIZE == 0 || MAX_CLUSTERS_PER_SECTION % WAVE_SIZE != 0)
	#error this code implies `MAX_CLUSTERS_PER_SECTION = WAVE_SIZE * n`
#endif

	uint64_t clustersVisibilityMask = 0;

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

		clustersVisibilityMask = uint64_t(inputSectionRecord.z) | (uint64_t(inputSectionRecord.w) << 32);
	}

	clustersVisibilityMask = WaveReadLaneFirst(clustersVisibilityMask);

	uint interWaveClusterCompactIdxAccum = 0;

	[unroll]
	for (uint i = 0; i < MAX_CLUSTERS_PER_SECTION / WAVE_SIZE; i++)
	{
		const uint clusterIdx = i * WAVE_SIZE + waveThreadIdx;
		const bool clusterIsActive = ((clustersVisibilityMask >> clusterIdx) & 1) != 0;
		const uint waveClusterCompactIdx = WavePrefixCountBits(clusterIsActive);
		const uint waveActiveClusterCount = WaveActiveCountBits(clusterIsActive);

		const uint clusterCompactIdx = interWaveClusterCompactIdxAccum + waveClusterCompactIdx;
		payload.clustersIndicies[clusterCompactIdx] = clusterIdx;

		interWaveClusterCompactIdxAccum += waveActiveClusterCount;
	}

	const uint activeClusterCount = interWaveClusterCompactIdxAccum;
	DispatchMesh(activeClusterCount, 1, 1, payload);
}
