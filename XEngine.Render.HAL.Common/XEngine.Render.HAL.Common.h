#pragma once

#include <XLib.h>

#include "XEngine.Render.HAL.TextureFormat.h"

namespace XEngine::Render::HAL
{
	static constexpr uint8 MaxPipelineBindPointCountLog2 = 4;
	static constexpr uint8 MaxPipelineBindPointCount = 1 << MaxPipelineBindPointCountLog2;
	static constexpr uint8 MaxRenderTargetCount = 4;
	static constexpr uint8 MaxGraphicsPipelineBytecodeObjectCount = 3;

	enum class DepthStencilFormat : uint8
	{
		Undefined = 0,
		D16,
		D32,
		D24S8,
		D32S8,
	};

	enum class PipelineBindPointType : uint8
	{
		Undefined = 0,
		Constants,
		ConstantBuffer,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		//Descriptor,
		//DescriptorBundle,
		//DescriptorArray,
	};

	enum class DescriptorType : uint8
	{
		Undefined = 0,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		ReadOnlyTexture2D,
		ReadWriteTexture2D,
		RaytracingAccelerationStructure,
	};
}
