#pragma once

#include <XLib.h>

namespace XEngine::Gfx::HAL
{
	constexpr uint8 MaxDescriptorSetBindingCount = 32;
	constexpr uint8 MaxPipelineBindingCount = 32;
	constexpr uint8 MaxVertexBufferCount = 8;
	constexpr uint8 MaxVertexBindingCount = 32;
	constexpr uint16 MaxVertexBufferElementSize = 8192;
	constexpr uint8 MaxColorRenderTargetCount = 8;

	enum class ShaderType : uint8
	{
		Undefined = 0,
		Compute,
		Vertex,
		Amplification,
		Mesh,
		Pixel,
	};

	enum class PipelineBindingType : uint8
	{
		Undefined = 0,
		InplaceConstants,
		ConstantBuffer,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		DescriptorSet,
		DescriptorArray,

		ValueCount,
	};

	enum class DescriptorType : uint8
	{
		Undefined = 0,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		ReadOnlyTexture,
		ReadWriteTexture,
		RaytracingAccelerationStructure,

		ValueCount,
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

		ValueCount,
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
		R32_UINT,
		R32_SINT,
		R32G32_FLOAT,
		R32G32_UINT,
		R32G32_SINT,
		R32G32B32_FLOAT,
		R32G32B32_UINT,
		R32G32B32_SINT,
		R32G32B32A32_FLOAT,
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

	enum class SamplerFilterMode : uint8
	{
		Undefined = 0,
		MinPnt_MagPnt_MipPnt,
		MinPnt_MagPnt_MipLin,
		MinPnt_MagLin_MipPnt,
		MinPnt_MagLin_MipLin,
		MinLin_MagPnt_MipPnt,
		MinLin_MagPnt_MipLin,
		MinLin_MagLin_MipPnt,
		MinLin_MagLin_MipLin,
		Anisotropic,
	};

	enum class SamplerReductionMode : uint8
	{
		Undefined = 0,
		WeightedAverage,
		WeightedAverageOfComparisonResult,
		Min,
		Max,
	};

	enum class SamplerAddressMode : uint8
	{
		Undefined = 0,
		Wrap,
		Mirror,
		Clamp,
		BorderZero,
	};

	struct SamplerDesc
	{
		SamplerFilterMode filterMode;
		SamplerReductionMode reductionMode;
		uint8 maxAnisotropy;
		SamplerAddressMode addressModeU;
		SamplerAddressMode addressModeV;
		SamplerAddressMode addressModeW;
		float32 lodBias;
		float32 lodMin;
		float32 lodMax;
	};
	
	class TextureFormatUtils abstract final
	{
	public:
		static inline bool IsValid(TextureFormat format) { return format < TextureFormat::ValueCount && format != TextureFormat::Undefined; }
		static bool SupportsColorRTUsage(TextureFormat format);
		static bool SupportsDepthStencilRTUsage(TextureFormat format);
		static DepthStencilFormat TranslateToDepthStencilFormat(TextureFormat format);
	};

	class TexelViewFormatUtils abstract final
	{
	public:
		static inline bool IsValid(TexelViewFormat format) { return format < TexelViewFormat::ValueCount && format != TexelViewFormat::Undefined; }
		static bool SupportsColorRTUsage(TexelViewFormat format);
		static bool SupportsVertexInputUsage(TexelViewFormat format);
		static uint8 GetByteSize(TexelViewFormat format); // Provided formats should support linear storage.
	};
}

namespace XEngine
{
	namespace GfxHAL = Gfx::HAL;
}
