#pragma once

namespace D3D12Helpers
{
	inline D3D12_COMMAND_QUEUE_DESC CommandQueueDesc(D3D12_COMMAND_LIST_TYPE type,
		INT priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		D3D12_COMMAND_QUEUE_FLAGS flags = D3D12_COMMAND_QUEUE_FLAG_NONE)
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = type;
		desc.Priority = priority;
		desc.Flags = flags;
		return desc;
	}

	inline D3D12_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDescForTexture2D(
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, UINT mip = 0, D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Format = format;
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		desc.Flags = flags;
		desc.Texture2D.MipSlice = mip;
		return desc;
	}

	inline D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE type,
		UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = type;
		desc.NumDescriptors = numDescriptors;
		desc.Flags = flags;
		return desc;
	}

	inline D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type)
	{
		D3D12_HEAP_PROPERTIES props = {};
		props.Type = type;
		return props;
	}

	inline D3D12_RENDER_TARGET_VIEW_DESC RenderTargetViewDescForTexture2D(
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, UINT mip = 0, UINT plane = 0)
	{
		D3D12_RENDER_TARGET_VIEW_DESC desc = {};
		desc.Format = format;
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mip;
		desc.Texture2D.PlaneSlice = plane;
		return desc;
	}

	inline D3D12_RESOURCE_DESC ResourceDescForBuffer(UINT64 size,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = size;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 0;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = flags;
		return desc;
	}

	inline D3D12_RESOURCE_DESC ResourceDescForTexture2D(UINT width, UINT heigth, UINT16 mipLevels,
		DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = width;
		desc.Height = heigth;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = mipLevels;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = flags;
		return desc;
	}
}
