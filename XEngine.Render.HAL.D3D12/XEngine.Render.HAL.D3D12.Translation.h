#pragma once

// NOTE: This file should not be included directly by HAL users.

inline D3D12_HEAP_TYPE TranslateBufferMemoryTypeToD3D12HeapType(BufferMemoryType type)
{
	switch (type)
	{
		case BufferMemoryType::Local:		return D3D12_HEAP_TYPE_DEFAULT;
		case BufferMemoryType::Upload:		return D3D12_HEAP_TYPE_UPLOAD;
		case BufferMemoryType::Readback:	return D3D12_HEAP_TYPE_READBACK;
	}

	XEMasterAssertUnreachableCode();
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
}

inline D3D12_RESOURCE_STATES TranslateResourceStateToD3D12ResourceState(ResourceState state)
{
	if (state.isMutable())
	{
		switch (state.getMutable())
		{
			case ResourceMutableState::RenderTarget:	return D3D12_RESOURCE_STATE_RENDER_TARGET;
			case ResourceMutableState::DepthWrite:		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
			case ResourceMutableState::ShaderReadWrite:	return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			case ResourceMutableState::CopyDestination:	return D3D12_RESOURCE_STATE_COPY_DEST;
		}
		return D3D12_RESOURCE_STATE_COMMON;
	}
	else
	{
		const ResourceImmutableState immutableState = state.getImmutable();
		D3D12_RESOURCE_STATES d3dResultStates = D3D12_RESOURCE_STATES(0);
		if (immutableState.vertexBuffer)
			d3dResultStates |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (immutableState.indexBuffer)
			d3dResultStates |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (immutableState.constantBuffer)
			d3dResultStates |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (immutableState.pixelShaderRead)
			d3dResultStates |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (immutableState.nonPixelShaderRead)
			d3dResultStates |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (immutableState.depthRead)
			d3dResultStates |= D3D12_RESOURCE_STATE_DEPTH_READ;
		if (immutableState.indirectArgument)
			d3dResultStates |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		if (immutableState.copySource)
			d3dResultStates |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		return d3dResultStates;
	}
}

inline D3D12_BOX TranslateTextureRegionToD3D12Box(const TextureRegion& region)
{
	D3D12_BOX box = {};
	box.left = region.left;
	box.top = region.top;
	box.front = region.front;
	box.right = region.right;
	box.bottom = region.bottom;
	box.back = region.back;
	return box;
}
