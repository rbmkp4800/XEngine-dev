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

	inline D3D12_HEAP_PROPERTIES HeapPropertiesForHeapType(D3D12_HEAP_TYPE type)
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

	inline D3D12_RESOURCE_DESC1 ResourceDesc1ForBuffer(UINT64 size,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
	{
		D3D12_RESOURCE_DESC1 desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = size;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = flags;
		return desc;
	}

	inline D3D12_SHADER_RESOURCE_VIEW_DESC ShaderResourceViewDescForTexture2D(DXGI_FORMAT format,
		UINT mostDetailedMip, UINT mipLevels, UINT planeSlice = 0, FLOAT resourceMinLODClamp = 0.0f,
		UINT shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = format;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Shader4ComponentMapping = shader4ComponentMapping;
		desc.Texture2D.MostDetailedMip = mostDetailedMip;
		desc.Texture2D.MipLevels = mipLevels;
		desc.Texture2D.PlaneSlice = planeSlice;
		desc.Texture2D.ResourceMinLODClamp = resourceMinLODClamp;
		return desc;
	}

	inline D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle(UINT64 ptr) { return D3D12_CPU_DESCRIPTOR_HANDLE { ptr }; }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHandle(UINT64 ptr) { return D3D12_GPU_DESCRIPTOR_HANDLE { ptr }; }
}
