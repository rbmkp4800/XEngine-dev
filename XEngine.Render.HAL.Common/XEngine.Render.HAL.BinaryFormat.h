#pragma once

#include <XLib.h>

namespace XEngine::Render::HAL::BinaryFormat
{
	static constexpr uint32 PipelineLayoutBlobSignature = 0;
	static constexpr uint32 PipelineLayoutBlobCurrentVerstion = 0;

	struct PipelineBindPointRecord
	{
		uint32 bindPointNameHash;
		uint32 bindPointDesc;
	};

	struct PipelineLayoutHeader
	{
		uint32 signature;
		uint32 version;
		uint32 thisBlobSize;
		uint32 thisBlobHash;
		uint32 sourceHash;
		uint8 bindPointCount;
		uint8 bindPointsLUTKeyShift;
		uint16 _padding0;
	};

	struct PipelinePartHeader
	{

	};
}
