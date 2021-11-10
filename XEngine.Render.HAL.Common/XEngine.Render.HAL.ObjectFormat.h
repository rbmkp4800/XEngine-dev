#pragma once

#include <XLib.h>

#include "XEngine.Render.HAL.Common.h"

namespace XEngine::Render::HAL::ObjectFormat
{
	static constexpr uint64 PipelineLayoutObjectSignature = 0;
	static constexpr uint16 PipelineLayoutObjectCurrentVerstion = 0;

	static constexpr uint64 GraphicsPipelineBaseObjectSignature = 0;
	static constexpr uint16 GraphicsPipelineBaseObjectCurrentVerstion = 0;

	static constexpr uint64 ComputePipelineBaseObjectSignature = 0;
	static constexpr uint16 ComputePipelineBaseObjectCurrentVerstion = 0;

	static constexpr uint64 PipelineBytecodeObjectSignature = 0;
	static constexpr uint16 PipelineBytecodeObjectCurrentVerstion = 0;

	struct GenericObjectHeader
	{
		uint64 signature;
		uint32 objectSize;
		uint32 objectCRC;
	};

	struct PipelineLayoutObjectHeader
	{
		GenericObjectHeader generic;

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

	struct GraphicsPipelineEnabledShaderStages
	{
		bool vertex : 1;
		bool amplification : 1;
		bool mesh : 1;
		bool pixel : 1;
		uint _padding : 4;
	};

	struct GraphicsPipelineBaseObjectHeader
	{
		GenericObjectHeader generic;

		TexelFormat renderTargetFormats[4];
		TexelFormat depthStencilFormat;
		GraphicsPipelineEnabledShaderStages enabledShaderStages;
	};

	struct ComputePipelineBaseObjectHeader
	{
		GenericObjectHeader generic;
	};

	struct PipelineBytecodeObjectHeader
	{
		GenericObjectHeader generic;
	};
}
