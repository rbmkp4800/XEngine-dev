#pragma once

#include <XLib.h>

#include "XEngine.Render.HAL.Common.h"

namespace XEngine::Render::HAL::ObjectFormat
{
	static constexpr uint64 PipelineLayoutObjectSignature = 0;
	static constexpr uint16 PipelineLayoutObjectCurrentVerstion = 0;

	static constexpr uint64 GraphicsPipelineBaseObjectSignature = 0;
	static constexpr uint16 GraphicsPipelineBaseObjectCurrentVerstion = 0;

	static constexpr uint64 GraphicsPipelineBytecodeObjectSignature = 0;
	static constexpr uint16 GraphicsPipelineBytecodeObjectCurrentVerstion = 0;

	static constexpr uint64 CopmputePipelineBytecodeObjectSignature = 0;
	static constexpr uint16 CopmputePipelineBytecodeObjectCurrentVerstion = 0;

	static constexpr uint8 MaxGraphicsPipelineBytecodeObjectCount = 3;

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

	struct GraphicsPipelineBaseObject
	{
		GenericObjectHeader generic;

		uint32 pipelineLayoutSourceHash;
		uint32 bytecodeObjectsCRCs[MaxGraphicsPipelineBytecodeObjectCount];

		TexelFormat renderTargetFormats[4];
		TexelFormat depthStencilFormat;
		GraphicsPipelineEnabledShaderStages enabledShaderStages;
		uint8 _padding0;
		uint8 _padding1;
	};

	enum class GraphicsPipelineBytecodeObjectType : uint8
	{
		Undefined = 0,
		VertexShader,
		AmplificationShader,
		MeshShader,
		PixelShader,
	};

	struct GraphicsPipelineBytecodeObjectHeader
	{
		GenericObjectHeader generic;

		uint32 baseObjectCRC;
		GraphicsPipelineBytecodeObjectType objectType;
	};

	struct ComputePipelineBytecodeObjectHeader
	{
		GenericObjectHeader generic;
		uint32 pipelineLayoutSourceHash;
	};
}
