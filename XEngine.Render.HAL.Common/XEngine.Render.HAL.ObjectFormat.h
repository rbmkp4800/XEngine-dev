#pragma once

#include <XLib.h>

#include "XEngine.Render.HAL.Common.h"

namespace XEngine::Render::HAL::ObjectFormat
{
	static constexpr uint64 DescriptorSetLayoutObjectSignature = 0;
	static constexpr uint16 DescriptorSetLayoutObjectCurrentVerstion = 0;

	static constexpr uint64 PipelineLayoutObjectSignature = 0;
	static constexpr uint16 PipelineLayoutObjectCurrentVerstion = 0;

	static constexpr uint64 GraphicsPipelineBaseObjectSignature = 0;
	static constexpr uint16 GraphicsPipelineBaseObjectCurrentVerstion = 0;

	static constexpr uint64 PipelineBytecodeObjectSignature = 0;
	static constexpr uint16 PipelineBytecodeObjectCurrentVerstion = 0;

	struct GenericObjectHeader
	{
		uint64 signature;
		uint32 objectSize;
		uint32 objectCRC32; // CRC-32/zlib
	};

	struct DescriptorSetLayoutObjectHeader
	{
		GenericObjectHeader generic;

		uint32 sourceHash;
		uint8 bindingCount;
		uint8 _padding0;
		uint16 _padding1;
	};

	struct PipelineLayoutObjectHeader
	{
		GenericObjectHeader generic;

		uint32 sourceHash;
		uint8 bindingCount;
		uint8 _padding0;
		uint16 _padding1;
	};

	struct PipelineBindingRecord
	{
		uint32 nameXSH;
		PipelineBindingType type;
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
		uint32 bytecodeObjectsCRC32s[MaxGraphicsPipelineBytecodeObjectCount];

		TexelViewFormat renderTargetFormats[MaxRenderTargetCount];
		DepthStencilFormat depthStencilFormat;
		GraphicsPipelineEnabledShaderStages enabledShaderStages;
		uint8 _padding0;
		uint8 _padding1;
	};

	enum class PipelineBytecodeObjectType : uint8
	{
		Undefined = 0,
		ComputeShader,
		VertexShader,
		AmplificationShader,
		MeshShader,
		PixelShader,
	};

	struct PipelineBytecodeObjectHeader
	{
		GenericObjectHeader generic;

		uint32 pipelineLayoutSourceHash;
		PipelineBytecodeObjectType objectType;
	};
}
