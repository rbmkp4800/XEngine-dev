// Compilation defines:
//	WAVE_SIZE_LOG2;
//	PREINDEXED;

#include "HostAndShadersShared.h"

#define WAVE_SIZE		(1 << WAVE_SIZE_LOG2)
#define WAVE_TIDX_MASK	(WAVE_SIZE - 1)

#define GROUP_SIZE		SCENE_GEOMETRY_AS_GROUP_SIZE
#define WAVES_PER_GROUP (GROUP_SIZE / WAVE_SIZE)

// TODO: ASSERT(WAVES_PER_GROUP <= WAVE_SIZE)

Buffer<uint> globalBuffer : register(t0);
Buffer<uint4> sectionsRecords : register(t1);

struct GroupShared
{
	uint inputSectionRecords0[MAX_SECTIONS_PER_AS_GROUP];
	uint inputSectionRecords1[MAX_SECTIONS_PER_AS_GROUP];
	uint inputSectionRecords2[MAX_SECTIONS_PER_AS_GROUP];
	uint inputSectionRecords3[MAX_SECTIONS_PER_AS_GROUP];

	uint interWaveClustersSumBuffer[WAVES_PER_GROUP + 1]; // Last element contains total sum
	uint perWaveSectionsToProcess_firstSectionIdx[WAVES_PER_GROUP];
	uint perWaveSectionsToProcess_firstSectionClustersOffset[WAVES_PER_GROUP];
};

struct ASPayload
{
	uint clustersDataU16[MAX_CLUSTERS_PER_AS_GROUP / 2];
	uint2 sectionsData[SECTIONS_PER_AS_GROUP];

	// Clusters data entry (uint16)
	//	section index : 7-8
	//	cluster index : 6

	// Sections data entry
	//	clusters begin address
	//	material index
	//	base transform index
};

groupshared GroupShared groupShared;
groupshared ASPayload payload;

[numthreads(GROUP_SIZE, 1, 1)]
void main(
	const uint globalThreadIdx : SV_DispatchThreadID,
	const uint groupIdx : SV_GroupID,
	const uint groupThreadIdx : SV_GroupIndex)
{
	const uint waveThreadIdx = WaveGetLaneIndex();
	const uint waveIdx = groupThreadIdx / WAVE_SIZE;


	if (groupThreadIdx < WAVES_PER_GROUP + 1)
		groupShared.interWaveClustersSumBuffer[groupThreadIdx] = 0

	GroupMemoryBarrierWithGroupSync();

	uint4 inputSectionRecord;
	uint inputSectionClusterCount;
	uint inputSectionClustersOffset;

	[branch]
	if (groupThreadIdx < SECTIONS_PER_AS_GPOUP)
	{
		inputSectionRecord = sectionsRecords.Load(groupIdx * SECTIONS_PER_AS_GPOUP + groupThreadIdx);
		inputSectionClusterCount = ...;

		inputSectionRecords0[groupThreadIdx] = inputSectionRecord.x;
		inputSectionRecords1[groupThreadIdx] = inputSectionRecord.y;
		inputSectionRecords2[groupThreadIdx] = inputSectionRecord.z;
		inputSectionRecords3[groupThreadIdx] = inputSectionRecord.w;

		// For now just in-wave sum. Inter-wave sum will be applied later
		inputSectionClustersOffset = WavePrefixSum(inputSectionClusterCount);

		const uint inWaveTotalClusterCount = WaveActiveSum(clusterCount);
		if (WaveIsFirstLane())
			groupShared.interWaveClustersSumBuffer[waveIdx] = waveTotalClusterCount;
	}

	GroupMemoryBarrierWithGroupSync();

	// Inter wave prefix sum
	[branch]
	if (groupThreadIdx < WAVES_PER_GROUP + 1)
	{
		const uint interWaveIthClustersCount = groupShared.interWaveClustersSumBuffer[groupThreadIdx];
		const uint interWaveIthPrefixSum = WavePrefixSum(interWaveIthClustersCount);
		groupShared.interWaveClustersSumBuffer[groupThreadIdx] = interWaveIthPrefixSum;
	}

	GroupMemoryBarrierWithGroupSync();

	[branch]
	if (groupThreadIdx < SECTIONS_PER_AS_GPOUP)
	{
		// Apply inter-wave sum and compute in-group clusters offset
		if (waveIdx > 0)
			inputSectionClustersOffset += groupShared.interWaveClustersSumBuffer[waveIdx];

		const uint totalClusterCount = groupShared.interWaveClustersSumBuffer[WAVES_PER_GROUP];

		// Coarsely divide sections between waves:
		//	1. Divide clusters in wave-sized portions
		//	2. Divide those portions uniformly between waves of this group

		// Each wave should have at least one portion (even if it is empty)
		const uint totalPortionCount =
			max((totalClusterCount + WAVE_SIZE - 1) >> WAVE_SIZE_LOG2, WAVES_PER_GROUP);

		// For each input section:
		const uint firstClusterIdx = inputSectionClustersOffset;
		const uint lastClusterIdx  = inputSectionClustersOffset + inputSectionClusterCount - 1;

		const uint firstPortionIdx = firstClusterIdx >> WAVE_SIZE_LOG2;
		const uint lastPortionIdx  = lastClusterIdx  >> WAVE_SIZE_LOG2;

		const float portionIdxToWaveIdxMul = float(WAVES_PER_GROUP) / float(totalPortionCount) * 1.001f;
		const float waveIdxToPortionIdxMul = float(totalPortionCount) / float(WAVES_PER_GROUP) * 1.001f;

		const uint firstWave = uint(float(firstPortionIdx) * portionIdxToWaveIdxMul);
		const uint lastWave  = uint(float(lastPortionIdx)  * portionIdxToWaveIdxMul);

		[loop]
		for (uint i = firstWave + 1; i <= lastWave; i++)
		{
			const uint globalPortionIdx = totalPortionCount - uint(float(WAVES_PER_GROUP - i) * waveIdxToPortionIdxMul);
			const uint sectionPortionIdx = portionIdx - firstPortionIdx;

			const uint waveFirstSectionIdx = groupThreadIdx;
			const uint waveFirstSectionClustersOffset =
				(sectionPortionIdx << WAVE_SIZE_LOG2) - (sectionClustersFirstIdx & WAVE_TIDX_MASK);

			groupShared.perWaveSectionsToProcess_firstSectionIdx[i] = waveFirstSectionIdx;
			groupShared.perWaveSectionsToProcess_firstSectionClustersOffset[i] = waveFirstSectionClustersOffset;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	// Each wave processes its own set of clusters
	{
		const uint waveLeadSectionIdx = groupShared.perWaveSectionsToProcess_firstSectionIdx[waveIdx];
		const uint waveLeadSectionClustersOffset = groupShared.perWaveSectionsToProcess_firstSectionClustersOffset[waveIdx];

		[loop]
		for (;;)
		{
			const uint thisSectionStartMask = 1 << clustersOffset;
			const uint waveCombinedSectionsStartMask = WaveActiveBitOr(thisSectionStartMask);

			const uint precSectionsStartAndMask = (1 << (waveThreadIdx + 1)) - 1;
			const uint precSectionsStartMask = waveCombinedSectionsStartMask & precSectionsStartAndMask;

			const uint targetSectionStartThreadIdx = 31 - firstbithigh(combinedpPecSectionsStartMask);
			const uint targetSectionIndex = WaveReadLaneAt(, targetSectionStartThreadIdx);
			const uint targetClusterIndex = waveThreadIdx - targetSectionStartThreadIdx;
			


		}
	}

	DispatchMesh();
}
