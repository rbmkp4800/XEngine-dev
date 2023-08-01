#pragma once

// NOTE: This file should not be included directly by HAL users.
// Assumes that d3d12 and dxgi are included above.

namespace XEngine::Gfx::HAL
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
			case TexelViewFormat::R8_UNORM:				return DXGI_FORMAT_R8_UNORM;
			case TexelViewFormat::R8_SNORM:				return DXGI_FORMAT_R8_SNORM;
			case TexelViewFormat::R8_UINT:				return DXGI_FORMAT_R8_UINT;
			case TexelViewFormat::R8_SINT:				return DXGI_FORMAT_R8_SINT;
			case TexelViewFormat::R8G8_UNORM:			return DXGI_FORMAT_R8G8_UNORM;
			case TexelViewFormat::R8G8_SNORM:			return DXGI_FORMAT_R8G8_SNORM;
			case TexelViewFormat::R8G8_UINT:			return DXGI_FORMAT_R8G8_UINT;
			case TexelViewFormat::R8G8_SINT:			return DXGI_FORMAT_R8G8_SINT;
			case TexelViewFormat::R8G8B8A8_UNORM:		return DXGI_FORMAT_R8G8B8A8_UNORM;
			case TexelViewFormat::R8G8B8A8_SNORM:		return DXGI_FORMAT_R8G8B8A8_SNORM;
			case TexelViewFormat::R8G8B8A8_UINT:		return DXGI_FORMAT_R8G8B8A8_UINT;
			case TexelViewFormat::R8G8B8A8_SINT:		return DXGI_FORMAT_R8G8B8A8_SINT;
			case TexelViewFormat::R8G8B8A8_SRGB:		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case TexelViewFormat::R16_FLOAT:			return DXGI_FORMAT_R16_FLOAT;
			case TexelViewFormat::R16_UNORM:			return DXGI_FORMAT_R16_UNORM;
			case TexelViewFormat::R16_SNORM:			return DXGI_FORMAT_R16_SNORM;
			case TexelViewFormat::R16_UINT:				return DXGI_FORMAT_R16_UINT;
			case TexelViewFormat::R16_SINT:				return DXGI_FORMAT_R16_SINT;
			case TexelViewFormat::R16G16_FLOAT:			return DXGI_FORMAT_R16G16_FLOAT;
			case TexelViewFormat::R16G16_UNORM:			return DXGI_FORMAT_R16G16_UNORM;
			case TexelViewFormat::R16G16_SNORM:			return DXGI_FORMAT_R16G16_SNORM;
			case TexelViewFormat::R16G16_UINT:			return DXGI_FORMAT_R16G16_UINT;
			case TexelViewFormat::R16G16_SINT:			return DXGI_FORMAT_R16G16_SINT;
			case TexelViewFormat::R16G16B16A16_FLOAT:	return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case TexelViewFormat::R16G16B16A16_UNORM:	return DXGI_FORMAT_R16G16B16A16_UNORM;
			case TexelViewFormat::R16G16B16A16_SNORM:	return DXGI_FORMAT_R16G16B16A16_SNORM;
			case TexelViewFormat::R16G16B16A16_UINT:	return DXGI_FORMAT_R16G16B16A16_UINT;
			case TexelViewFormat::R16G16B16A16_SINT:	return DXGI_FORMAT_R16G16B16A16_SINT;
			case TexelViewFormat::R32_FLOAT:			return DXGI_FORMAT_R32_FLOAT;
			case TexelViewFormat::R32_UINT:				return DXGI_FORMAT_R32_UINT;
			case TexelViewFormat::R32_SINT:				return DXGI_FORMAT_R32_SINT;
			case TexelViewFormat::R32G32_FLOAT:			return DXGI_FORMAT_R32G32_FLOAT;
			case TexelViewFormat::R32G32_UINT:			return DXGI_FORMAT_R32G32_UINT;
			case TexelViewFormat::R32G32_SINT:			return DXGI_FORMAT_R32G32_SINT;
			case TexelViewFormat::R32G32B32_FLOAT:		return DXGI_FORMAT_R32G32B32_FLOAT;
			case TexelViewFormat::R32G32B32_UINT:		return DXGI_FORMAT_R32G32B32_UINT;
			case TexelViewFormat::R32G32B32_SINT:		return DXGI_FORMAT_R32G32B32_SINT;
			case TexelViewFormat::R32G32B32A32_FLOAT:	return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case TexelViewFormat::R32G32B32A32_UINT:	return DXGI_FORMAT_R32G32B32A32_UINT;
			case TexelViewFormat::R32G32B32A32_SINT:	return DXGI_FORMAT_R32G32B32A32_SINT;

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

	inline D3D12_HEAP_TYPE TranslateBufferMemoryTypeToD3D12HeapType(BufferMemoryType type)
	{
		switch (type)
		{
			case BufferMemoryType::DeviceLocal:	return D3D12_HEAP_TYPE_DEFAULT;
			case BufferMemoryType::Upload:		return D3D12_HEAP_TYPE_UPLOAD;
			case BufferMemoryType::Readback:	return D3D12_HEAP_TYPE_READBACK;
		}

		XEMasterAssertUnreachableCode();
		return D3D12_HEAP_TYPE(0);
	}

	// TODO: Remove when dropping legacy barriers support.
	inline D3D12_RESOURCE_STATES TranslateBufferMemoryTypeToInitialD3D12ResourceState(BufferMemoryType type)
	{
		switch (type)
		{
		case BufferMemoryType::DeviceLocal:	return D3D12_RESOURCE_STATE_COMMON;
		case BufferMemoryType::Upload:		return D3D12_RESOURCE_STATE_GENERIC_READ;
		case BufferMemoryType::Readback:	return D3D12_RESOURCE_STATE_COPY_DEST;
		}

		XEMasterAssertUnreachableCode();
		return D3D12_RESOURCE_STATES(0);
	}

#if USE_ENHANCED_BARRIERS

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

	inline D3D12_BARRIER_LAYOUT TranslateTextureLayoutToD3D12BarrierLayout(TextureLayout textureLayout)
	{
		switch (textureLayout)
		{
			case TextureLayout::Undefined:					return D3D12_BARRIER_LAYOUT_UNDEFINED;
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

#else

	inline D3D12_RESOURCE_STATES TranslateBarrierAccessToD3D12ResourceStates(BarrierAccess access)
	{
		if (access == BarrierAccess::Any)
			return D3D12_RESOURCE_STATE_COMMON;

		D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATES(0);

		// Read accesses.
		if ((access & BarrierAccess::CopySource) != BarrierAccess(0))
			d3dResourceStates |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		if ((access & BarrierAccess::GeometryInput) != BarrierAccess(0))
			d3dResourceStates |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if ((access & BarrierAccess::ConstantBuffer) != BarrierAccess(0))
			d3dResourceStates |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if ((access & BarrierAccess::ShaderRead) != BarrierAccess(0))
			d3dResourceStates |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if ((access & BarrierAccess::DepthStencilRead) != BarrierAccess(0))
			d3dResourceStates |= D3D12_RESOURCE_STATE_DEPTH_READ;

		// Write accesses.
		// NOTE: We enforce one-writer-at-a-time policy for legacy barriers.
		uint32 writeAccessCount = 0;
		if ((access & BarrierAccess::CopyDest) != BarrierAccess(0))
		{
			d3dResourceStates |= D3D12_RESOURCE_STATE_COPY_DEST;
			writeAccessCount++;
		}
		if ((access & BarrierAccess::ShaderReadWrite) != BarrierAccess(0))
		{
			d3dResourceStates |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			writeAccessCount++;
		}
		if ((access & BarrierAccess::RenderTarget) != BarrierAccess(0))
		{
			d3dResourceStates |= D3D12_RESOURCE_STATE_RENDER_TARGET;
			writeAccessCount++;
		}
		if ((access & BarrierAccess::DepthStencilReadWrite) != BarrierAccess(0))
		{
			d3dResourceStates |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
			writeAccessCount++;
		}
		XEAssert(writeAccessCount <= 1);

		if ((access & BarrierAccess::RaytracingAccelerationStructureRead) != BarrierAccess(0))
			XEAssertUnreachableCode(); // Not implemented.
		if ((access & BarrierAccess::RaytracingAccelerationStructureWrite) != BarrierAccess(0))
			XEAssertUnreachableCode(); // Not implemented.

		return d3dResourceStates;
	}

#endif

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
