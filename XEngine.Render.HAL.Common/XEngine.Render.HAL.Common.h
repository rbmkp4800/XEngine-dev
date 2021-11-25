#pragma once

#include <XLib.h>

namespace XEngine::Render::HAL
{
	static constexpr uint8 MaxPipelineBindPointCountLog2 = 4;
	static constexpr uint8 MaxPipelineBindPointCount = 1 << MaxPipelineBindPointCountLog2;
	static constexpr uint8 MaxRenderTargetCount = 4;
	static constexpr uint8 MaxGraphicsPipelineBytecodeObjectCount = 3;

	enum class TexelFormat : uint8
	{
		Undefined = 0,
		R8G8B8A8_UNORM,
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

	bool ValidateTexelFormatValue(TexelFormat format);
}
