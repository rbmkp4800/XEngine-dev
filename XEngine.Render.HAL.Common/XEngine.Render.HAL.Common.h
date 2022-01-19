#pragma once

#include <XLib.h>

namespace XEngine::Render::HAL
{
	static constexpr uint8 MaxPipelineBindPointCountLog2 = 4;
	static constexpr uint8 MaxPipelineBindPointCount = 1 << MaxPipelineBindPointCountLog2;
	static constexpr uint8 MaxRenderTargetCount = 4;
	static constexpr uint8 MaxGraphicsPipelineBytecodeObjectCount = 3;

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

	enum class TextureFormat : uint8
	{
		Undefined = 0,
		R8,
		R8G8,
		R8G8B8A8,
		R16,
		R16G16,
		R16G16B16A16,
		R32,
		R32G32,
		R32G32B32,
		R32G32B32A32,
		D16,
		D32,
		D24S8,
		D32S8,
	};

	enum class TexelViewFormat : uint8
	{
		Undefined = 0,

		R8_UNORM,
		R8_SNORM,
		R8_UINT,
		R8_SINT,
		R8G8_UNORM,
		R8G8_SNORM,
		R8G8_UINT,
		R8G8_SINT,
		R8G8B8A8_UNORM,
		R8G8B8A8_SNORM,
		R8G8B8A8_UINT,
		R8G8B8A8_SINT,
		R8G8B8A8_SRGB,
		R16_FLOAT,
		R16_UNORM,
		R16_SNORM,
		R16_UINT,
		R16_SINT,
		R16G16_FLOAT,
		R16G16_UNORM,
		R16G16_SNORM,
		R16G16_UINT,
		R16G16_SINT,
		R16G16B16A16_FLOAT,
		R16G16B16A16_UNORM,
		R16G16B16A16_SNORM,
		R16G16B16A16_UINT,
		R16G16B16A16_SINT,
		R32_FLOAT,
		R32_UNORM,
		R32_SNORM,
		R32_UINT,
		R32_SINT,
		R32G32_FLOAT,
		R32G32_UNORM,
		R32G32_SNORM,
		R32G32_UINT,
		R32G32_SINT,
		R32G32B32A32_FLOAT,
		R32G32B32A32_UNORM,
		R32G32B32A32_SNORM,
		R32G32B32A32_UINT,
		R32G32B32A32_SINT,

		R24_UNORM_X8,
		X24_G8_UINT,
		R32_FLOAT_X8,
		X32_G8_UINT,

		ValueCount,
	};

	enum class DepthStencilFormat : uint8
	{
		Undefined = 0,
		D16,
		D32,
		D24S8,
		D32S8,
	};

	bool ValidateTexelViewFormatValue(TexelViewFormat format);
	bool ValidateTexelViewFormatForRenderTargetUsage(TexelViewFormat format);
}
