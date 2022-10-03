#pragma once

// NOTE: This file should not be included directly by HAL users.
// Assumes that d3d12 and dxgi are included above.

namespace XEngine::Render::HAL
{
	inline D3D12_HEAP_TYPE TranslateMemoryTypeToD3D12HeapType(MemoryType type)
	{
		switch (type)
		{
			case MemoryType::DeviceLocal:			return D3D12_HEAP_TYPE_DEFAULT;
			case MemoryType::DeviceReadHostWrite:	return D3D12_HEAP_TYPE_UPLOAD;
			case MemoryType::DeviceWriteHostRead:	return D3D12_HEAP_TYPE_READBACK;
		}

		XEMasterAssertUnreachableCode();
		return D3D12_HEAP_TYPE_DEFAULT;
	}

	inline DXGI_FORMAT TranslateTextureFormatToDXGIFormat(TextureFormat format)
	{
		switch (format)
		{
			case TextureFormat::Undefined:		return DXGI_FORMAT_UNKNOWN;
			case TextureFormat::R8:				return DXGI_FORMAT_R8_TYPELESS;
			case TextureFormat::R8G8:			return DXGI_FORMAT_R8G8_TYPELESS;
			case TextureFormat::R8G8B8A8:		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case TextureFormat::R16:			return DXGI_FORMAT_R16_TYPELESS;
			case TextureFormat::R16G16:			return DXGI_FORMAT_R16G16_TYPELESS;
			case TextureFormat::R16G16B16A16:	return DXGI_FORMAT_R16G16B16A16_TYPELESS;
			case TextureFormat::R32:			return DXGI_FORMAT_R32_TYPELESS;
			case TextureFormat::R32G32:			return DXGI_FORMAT_R32G32_TYPELESS;
			case TextureFormat::R32G32B32:		return DXGI_FORMAT_R32G32B32_TYPELESS;
			case TextureFormat::R32G32B32A32:	return DXGI_FORMAT_R32G32B32A32_TYPELESS;
			case TextureFormat::D16:			return DXGI_FORMAT_D16_UNORM;
			case TextureFormat::D32:			return DXGI_FORMAT_D32_FLOAT;
			case TextureFormat::D24S8:			return DXGI_FORMAT_D24_UNORM_S8_UINT;
			case TextureFormat::D32S8:			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		}

		XEMasterAssertUnreachableCode();
		return DXGI_FORMAT_UNKNOWN;
	}

	inline DXGI_FORMAT TranslateTexelViewFormatToDXGIFormat(TexelViewFormat format)
	{
		switch (format)
		{
			case TexelViewFormat::R8_UNORM:			return DXGI_FORMAT_R8_UNORM;
			case TexelViewFormat::R8_SNORM:			return DXGI_FORMAT_R8_SNORM;
			case TexelViewFormat::R8_UINT:			return DXGI_FORMAT_R8_UINT;
			case TexelViewFormat::R8_SINT:			return DXGI_FORMAT_R8_SINT;
			case TexelViewFormat::R8G8_UNORM:		return DXGI_FORMAT_R8G8_UNORM;
			case TexelViewFormat::R8G8_SNORM:		return DXGI_FORMAT_R8G8_SNORM;
			case TexelViewFormat::R8G8_UINT:		return DXGI_FORMAT_R8G8_UINT;
			case TexelViewFormat::R8G8_SINT:		return DXGI_FORMAT_R8G8_SINT;
			case TexelViewFormat::R8G8B8A8_UNORM:	return DXGI_FORMAT_R8G8B8A8_UNORM;
			case TexelViewFormat::R8G8B8A8_SNORM:	return DXGI_FORMAT_R8G8B8A8_SNORM;
			case TexelViewFormat::R8G8B8A8_UINT:	return DXGI_FORMAT_R8G8B8A8_UINT;
			case TexelViewFormat::R8G8B8A8_SINT:	return DXGI_FORMAT_R8G8B8A8_SINT;
			case TexelViewFormat::R8G8B8A8_SRGB:	return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		}

		XEMasterAssertUnreachableCode();
		return DXGI_FORMAT_UNKNOWN;
	}

	inline DXGI_FORMAT TranslateDepthStencilFormatToDXGIFormat(DepthStencilFormat format)
	{
		switch (format)
		{
			case DepthStencilFormat::Undefined:	return DXGI_FORMAT_UNKNOWN;
			case DepthStencilFormat::D16:		return DXGI_FORMAT_D16_UNORM;
			case DepthStencilFormat::D32:		return DXGI_FORMAT_D16_UNORM;
			case DepthStencilFormat::D24S8:		return DXGI_FORMAT_D24_UNORM_S8_UINT;
			case DepthStencilFormat::D32S8:		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		}

		XEMasterAssertUnreachableCode();
		return DXGI_FORMAT_UNKNOWN;
	}

	inline D3D12_BOX D3D12BoxFromOffsetAndSize(uint16x3 offset, uint16x3 size)
	{
		D3D12_BOX result = {};
		result.left = offset.x;
		result.top = offset.y;
		result.front = offset.z;
		result.right = offset.x + size.x;
		result.bottom = offset.y + size.y;
		result.back = offset.z + size.z;
		return result;
	}
}
