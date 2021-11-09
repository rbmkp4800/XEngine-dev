#pragma once

#include <XLib.h>

#include "XEngine.Render.HAL.Common.h"

namespace XEngine::Render::HAL::ObjectFormat
{
	static constexpr uint32 PipelineLayoutObjectSignature = 0;
	static constexpr uint32 PipelineLayoutObjectCurrentVerstion = 0;

	static constexpr uint32 PipelineBaseObjectSignature = 0;
	static constexpr uint32 PipelineBaseObjectCurrentVerstion = 0;

	static constexpr uint32 PipelineBytecodeObjectSignature = 0;
	static constexpr uint32 PipelineBytecodeObjectCurrentVerstion = 0;

	struct PipelineLayoutObjectHeader
	{
		uint32 signature;
		uint32 version;
		uint32 objectSize;
		uint32 objectCRC;

		uint32 sourceHash;
		uint8 bindPointCount;
		uint8 _padding0;
		uint16 _padding1;
	};

	struct PipelineBindPointRecord
	{
		uint32 nameCRC;
		PipelineBindPointType type;
		uint8 rootParameterIndex;
		uint8 constantsSize32bitValues;
	};

	struct PipelineBaseObjectHeader
	{
		uint32 signature;
		uint32 version;
		uint32 objectSize;
		uint32 objectCRC;

	};

	struct PipelineBytecodeObjectHeader
	{
		uint32 signature;
		uint32 version;
		uint32 objectSize;
		uint32 objectCRC;
	};
}
