#pragma once

// NOTE: This file should not be included directly by HAL users.
// Assumes that d3d12 and dxgi are included above.

namespace XEngine::Render::HAL
{
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

	inline D3D12_BARRIER_LAYOUT TranslateTextureLayoutToD3D12BarrierLayout(TextureLayout textureLayout)
	{
		switch (textureLayout)
		{
			case TextureLayout::Present:					return D3D12_BARRIER_LAYOUT_PRESENT;
			case TextureLayout::CopySource:					return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
			case TextureLayout::CopyDest:					return D3D12_BARRIER_LAYOUT_COPY_DEST;
			case TextureLayout::ShaderReadAndCopySource:	return D3D12_BARRIER_LAYOUT_GENERIC_READ;
			case TextureLayout::ShaderReadAndCopySourceDest:return D3D12_BARRIER_LAYOUT_COMMON;
			case TextureLayout::ShaderRead:					return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
			case TextureLayout::ShaderReadWrite:			return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
			case TextureLayout::RenderTarget:				return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
			case TextureLayout::DepthStencilRead:			return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
			case TextureLayout::DepthStencilReadWrite:		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		}

		// TODO: Maybe we want to use `D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_*` and `D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_*` ?

		XEMasterAssertUnreachableCode();
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}

	inline D3D12_BARRIER_SYNC TranslateBarrierSyncToD3D12BarrierSync(BarrierSync sync)
	{
		if (sync == BarrierSync::All)
			return D3D12_BARRIER_SYNC_ALL;

		D3D12_BARRIER_SYNC d3dBarrierSync = D3D12_BARRIER_SYNC(0);

		if ((sync & BarrierSync::AllGraphics) == BarrierSync::AllGraphics)
		{
			d3dBarrierSync |= D3D12_BARRIER_SYNC_DRAW;
			sync ^= BarrierSync::AllGraphics;
		}

		if ((sync & BarrierSync::AllShading) == BarrierSync::AllShading)
		{
			d3dBarrierSync |= D3D12_BARRIER_SYNC_ALL_SHADING;
			sync ^= BarrierSync::AllShading;
		}

		if ((sync & BarrierSync::Copy) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_COPY;
		if ((sync & BarrierSync::ComputeShading) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		if ((sync & BarrierSync::GraphicsGeometryShading) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		if ((sync & BarrierSync::GraphicsPixelShading) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if ((sync & BarrierSync::GraphicsRenderTarget) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
		if ((sync & BarrierSync::GraphicsDepthStencil) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		if ((sync & BarrierSync::RaytracingAccelerationStructureBuild) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		if ((sync & BarrierSync::RaytracingAccelerationStructureCopy) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE;

		return d3dBarrierSync;
	}

	inline D3D12_BARRIER_ACCESS TranslateBarrierAccessToD3D12BarrierAccess(BarrierAccess access)
	{
		if (access == BarrierAccess::None)
			return D3D12_BARRIER_ACCESS_NO_ACCESS;
		if (access == BarrierAccess::Any)
			return D3D12_BARRIER_ACCESS_COMMON;

		D3D12_BARRIER_ACCESS d3dBarrierAccess = D3D12_BARRIER_ACCESS(0);

		if ((access & BarrierAccess::CopySource) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
		if ((access & BarrierAccess::CopyDest) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_COPY_DEST;
		if ((access & BarrierAccess::GeometryInput) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_INDEX_BUFFER | D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
		if ((access & BarrierAccess::ConstantBuffer) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
		if ((access & BarrierAccess::ShaderRead) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		if ((access & BarrierAccess::ShaderReadWrite) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if ((access & BarrierAccess::RenderTarget) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		if ((access & BarrierAccess::DepthStencilRead) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		if ((access & BarrierAccess::DepthStencilReadWrite) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		if ((access & BarrierAccess::RaytracingAccelerationStructureRead) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		if ((access & BarrierAccess::RaytracingAccelerationStructureWrite) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;

		return d3dBarrierAccess;
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
