#pragma once

#include <XLib.h>

namespace XEngine::Render::HAL::ObjectFormat
{
	static constexpr uint32 PipelineLayoutSignature = 0;
	static constexpr uint32 PipelineLayoutCurrentVerstion = 0;

	struct PipelineBindPointRecord
	{
		uint32 bindPointNameHash;
		uint32 bindPointDesc;
	};

	struct PipelineLayoutHeader
	{
		uint32 signature;
		uint32 version;
		uint32 objectSize;
		uint32 objectHash;
		uint32 sourceHash;
		uint8 bindPointCount;
		uint16 _padding0;
	};

	struct PipelineBytecodeHeader
	{

	};
}
