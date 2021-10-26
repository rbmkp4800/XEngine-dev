#pragma once

#include <XLib.h>

namespace XEngine::Render::HAL::BinaryFormat
{
	static constexpr uint32 BindingLayoutBlobSignature = 0;
	static constexpr uint32 BindingLayoutBlobCurrentVerstion = 0;

	struct RootBindPointRecord
	{
		uint32 bindPointNameHash;
		uint32 bindPointDesc;
	};

	struct BindingLayoutBlobHeader
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
