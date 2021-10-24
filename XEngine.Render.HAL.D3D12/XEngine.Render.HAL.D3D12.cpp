#include <d3d12.h>
#include <dxgi1_6.h>

#include "D3D12Helpers.h"

#include "XEngine.Render.HAL.D3D12.h"

#define XE_ASSERT(...)
#define XE_ASSERT_UNREACHABLE_CODE
#define XE_MASTER_ASSERT(...)
#define XE_MASTER_ASSERT_UNREACHABLE_CODE

using namespace XLib::Platform;
using namespace XEngine::Render::HAL;

static COMPtr<IDXGIFactory7> dxgiFactory;

static inline D3D12_HEAP_TYPE TranslateBufferMemoryTypeToD3D12HeapType(BufferMemoryType type)
{
	switch (type)
	{
		case BufferMemoryType::Local:		return D3D12_HEAP_TYPE_DEFAULT;
		case BufferMemoryType::Upload:		return D3D12_HEAP_TYPE_UPLOAD;
		case BufferMemoryType::Readback:	return D3D12_HEAP_TYPE_READBACK;
	}

	XE_MASTER_ASSERT_UNREACHABLE_CODE;
}

static inline DXGI_FORMAT TranslateTextureFormatToDXGIFormat(TextureFormat format)
{
	switch (format)
	{
		case TextureFormat::Undefined:		return DXGI_FORMAT_UNKNOWN;
		case TextureFormat::R8G8B8A8_UNORM:	return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	XE_MASTER_ASSERT_UNREACHABLE_CODE;
}

// CommandList /////////////////////////////////////////////////////////////////////////////////////

void CommandList::setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv)
{
	XE_ASSERT(device);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRTVDescriptorPtrs[4] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDSVDescriptorPtr = {};

	const bool useRTVs = rtvCount > 0;
	if (useRTVs)
	{
		XE_ASSERT(rtvs);
		XE_ASSERT(rtvCount < countof(d3dRTVDescriptorPtrs));
		for (uint8 i = 0; i < rtvCount; i++)
		{
			XE_ASSERT(rtvs[i] != ZeroRenderTargetViewHandle);
			const uint32 descriptorIndex = uint32(rtvs[i]); // TODO: Proper handle decode
			d3dRTVDescriptorPtrs[i].ptr = device->rtvHeapStartPtr + descriptorIndex * device->rtvDescriptorSize;
		}
	}

	const bool useDSVs = dsv != ZeroDepthStencilViewHandle;
	if (useDSVs)
	{
		const uint32 descriptorIndex = uint32(dsv); // TODO: Proper handle decode
		d3dDSVDescriptorPtr.ptr = device->dsvHeapStartPtr + uint32(dsv) * device->dsvDescriptorSize;
	}

	d3dCommandList->OMSetRenderTargets(rtvCount,
		useRTVs ? d3dRTVDescriptorPtrs : nullptr, FALSE,
		useDSVs ? &d3dDSVDescriptorPtr : nullptr);
}

// Device //////////////////////////////////////////////////////////////////////////////////////////

struct Device::Resource
{
	ID3D12Resource2* d3dResource;
	ResourceType type;
	uint8 handleGeneration;
	bool internalOwnership; // For example swap chain. User can't release it
};

struct Device::ResourceView
{
	ResourceHandle resource;
	ResourceViewType type;
	uint8 handleGeneration;
};

struct Device::Pipeline
{
	ID3D12PipelineState* d3dPipelineState;
	bool isCompute;
	uint8 handleGeneration;
};

struct Device::Fence
{
	ID3D12Fence* d3dFence;
};

struct Device::SwapChain
{
	IDXGISwapChain4* dxgiSwapChain;
	ResourceHandle planeTextures[SwapChainPlaneCount];
};

void Device::initialize(IDXGIAdapter4* dxgiAdapter)
{
	XE_MASTER_ASSERT(!d3dDevice);

	D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_12_1, d3dDevice.uuid(), d3dDevice.voidInitRef());

	const D3D12_DESCRIPTOR_HEAP_DESC d3dReferenceSRVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxResourceViewCount);
	const D3D12_DESCRIPTOR_HEAP_DESC d3dShaderVisbileSRVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			ShaderVisibleSRVHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
	const D3D12_DESCRIPTOR_HEAP_DESC d3dRTVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MaxRenderTargetViewCount);
	const D3D12_DESCRIPTOR_HEAP_DESC d3dDSVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MaxDepthStencilViewCount);

	d3dDevice->CreateDescriptorHeap(&d3dReferenceSRVHeapDesc, d3dReferenceSRVHeap.uuid(), d3dReferenceSRVHeap.voidInitRef());
	d3dDevice->CreateDescriptorHeap(&d3dShaderVisbileSRVHeapDesc, d3dShaderVisbileSRVHeap.uuid(), d3dShaderVisbileSRVHeap.voidInitRef());
	d3dDevice->CreateDescriptorHeap(&d3dRTVHeapDesc, d3dRTVHeap.uuid(), d3dRTVHeap.voidInitRef());
	d3dDevice->CreateDescriptorHeap(&d3dDSVHeapDesc, d3dDSVHeap.uuid(), d3dDSVHeap.voidInitRef());

	const D3D12_COMMAND_QUEUE_DESC d3dGraphicsQueueDesc = D3D12Helpers::CommandQueueDesc(D3D12_COMMAND_LIST_TYPE_DIRECT);
	d3dDevice->CreateCommandQueue(&d3dGraphicsQueueDesc, d3dGraphicsQueue.uuid(), d3dGraphicsQueue.voidInitRef());

	resourcesAllocationMask.clear();
	resourceViewsAllocationMask.clear();
	renderTargetViewsAllocationMask.clear();
	depthStencilViewsAllocationMask.clear();
	fencesAllocationMask.clear();
	swapChainsAllocationMask.clear();

	referenceSRVHeapStartPtr = d3dReferenceSRVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	shaderVisbileSRVHeapStartPtrCPU = d3dShaderVisbileSRVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	shaderVisbileSRVHeapStartPtrGPU = d3dShaderVisbileSRVHeap->GetGPUDescriptorHandleForHeapStart().ptr;
	rtvHeapStartPtr = d3dRTVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	dsvHeapStartPtr = d3dDSVHeap->GetCPUDescriptorHandleForHeapStart().ptr;

	rtvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	dsvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	srvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
}

ResourceHandle Device::createBuffer(uint32 size, BufferMemoryType memoryType)
{
	const sint32 resourceIndex = resourcesAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(resourceIndex >= 0);

	Resource& resource = resourcesTable[resourceIndex];
	XE_ASSERT(resource == ResourceType::Undefined);
	XE_ASSERT(!resource.d3dResource);
	resource.type = ResourceType::Buffer;
	resource.internalOwnership = false;

	const D3D12_HEAP_TYPE d3dHeapType = TranslateBufferMemoryTypeToD3D12HeapType(memoryType);
	const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapProperties(d3dHeapType);
	const D3D12_RESOURCE_DESC d3dResourceDesc = D3D12Helpers::ResourceDescForBuffer(size);

	d3dDevice->CreateCommittedResource(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));

	return encodeResourceHandle(resourceIndex);
}

ResourceHandle Device::createTexture(const TextureDim& dim, TextureFormat format, uint32 flags)
{
	const sint32 resourceIndex = resourcesAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(resourceIndex >= 0);

	Resource& resource = resourcesTable[resourceIndex];
	XE_ASSERT(resource == ResourceType::Undefined);
	XE_ASSERT(!resource.d3dResource);
	resource.type = ResourceType::Texture;
	resource.internalOwnership = false;

	D3D12_RESOURCE_FLAGS d3dResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	if (flags & TextureFlag::AllowRenderTarget)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (flags & TextureFlag::AllowDepthStencil)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (flags & TextureFlag::AllowShaderWrite)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	const DXGI_FORMAT dxgiFormat = TranslateTextureFormatToDXGIFormat(format);
	const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapProperties(D3D12_HEAP_TYPE_DEFAULT);

	// TODO: For now just Texture2D
	const D3D12_RESOURCE_DESC d3dResourceDesc =
		D3D12Helpers::ResourceDescForTexture2D(dim.width, dim.height, 0, dxgiFormat, d3dResourceFlags);

	d3dDevice->CreateCommittedResource(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));

	return encodeResourceHandle(resourceIndex);
}

ResourceViewHandle Device::createResourceView(ResourceHandle resourceHandle, const ResourceViewDesc& viewDesc)
{
	const Resource& resource = resourcesTable[decodeResourceHandle(resourceHandle)];
	XE_ASSERT(resource.d3dResource);

	const sint32 viewIndex = resourceViewsAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(viewIndex >= 0);

	ResourceView& resourceView = resourceViewsTable[viewIndex];
	XE_ASSERT(resourceView.type == ResourceViewType::Undefined);
	resourceView.resource = resourceHandle;
	resourceView.type = viewDesc.type;

	bool useSRV = false;
	bool useUAV = false;
	D3D12_SHADER_RESOURCE_VIEW_DESC d3dSRVDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC d3dUAVDesc = {};

	if (viewDesc.type == ResourceViewType::ReadOnlyTexture2D)
	{
		XE_ASSERT(resource.type == ResourceType::Texture);
		d3dSRVDesc = D3D12Helpers::ShaderResourceViewDescForTexture2D(format,
			viewDesc.readOnlyTexture2D.startMipIndex,
			viewDesc.readOnlyTexture2D.mipLevelCount,
			viewDesc.readOnlyTexture2D.plane);
		useSRV = true;
	}
	else if (viewDesc.type == ResourceViewType::ReadWriteTexture2D)
	{
		XE_ASSERT(resource.type == ResourceType::Texture);
		//d3dUAVDesc = D3D12Helpers::UnorderedAccessViewDescForTexture2D(...);
		useUAV = true;
	}

	const uint64 descriptorPtr = rtvHeapStartPtr + rtvDescriptorSize * viewIndex;

	XE_ASSERT(useSRV ^ useUAV);
	if (useUAV)
		d3dDevice->CreateShaderResourceView(resource.d3dResource, &d3dSRVDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });
	if (useSRV)
		d3dDevice->CreateUnorderedAccessView(resource.d3dResource, nullptr, &d3dUAVDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return encodeResourceViewHandle(viewIndex);
}

RenderTargetViewHandle Device::createRenderTargetView(ResourceHandle textureHandle)
{
	const Resource& resource = resourcesTable[decodeResourceHandle(textureHandle)];
	XE_ASSERT(resource.type == ResourceType::Texture);
	XE_ASSERT(resource.d3dResource);

	const sint32 viewIndex = renderTargetViewsAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(viewIndex >= 0);

	const uint64 descriptorPtr = rtvHeapStartPtr + rtvDescriptorSize * viewIndex;
	d3dDevice->CreateRenderTargetView(resource.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return encodeRenderTargetViewHandle(viewIndex);
}

DepthStencilViewHandle Device::createDepthStencilView(ResourceHandle textureHandle)
{
	const Resource& resource = resourcesTable[decodeResourceHandle(textureHandle)];
	XE_ASSERT(resource.type == ResourceType::Texture);
	XE_ASSERT(resource.d3dResource);

	const sint32 viewIndex = depthStencilViewsAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(viewIndex >= 0);

	const uint64 descriptorPtr = dsvHeapStartPtr + dsvDescriptorSize * viewIndex;
	d3dDevice->CreateDepthStencilView(resource.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return encodeDepthStencilViewHandle(viewIndex);
}

DescriptorAddress Device::allocateDescriptors(uint32 count)
{

}

FenceHandle Device::createFence(uint64 initialValue)
{
	const sint32 fenceIndex = fencesAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(fenceIndex >= 0);

	Fence& fence = fencesTable[fenceIndex];
	XE_ASSERT(!fence.d3dFence);
	d3dDevice->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3dFence));

	return encodeFenceHandle(fenceIndex);
}

void Device::createCommandList(CommandList& commandList, CommandListType type)
{
	XE_ASSERT(type == CommandListType::Graphics); // TODO: ...
	XE_ASSERT(!commandList.d3dCommandList && !commandList.d3dCommandAllocator);

	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandList.d3dCommandAllocator));
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandList.d3dCommandAllocator, nullptr,
		IID_PPV_ARGS(&commandList.d3dCommandList));
	commandList.d3dCommandList->Close();
}

SwapChainHandle Device::createSwapChain(uint16 width, uint16 height, void* hWnd)
{
	const sint32 swapChainIndex = swapChainsAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(swapChainIndex >= 0);

	SwapChain& swapChain = swapChainsTable[swapChainIndex];
	XE_ASSERT(!swapChain.dxgiSwapChain);

	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc = {};
	dxgiSwapChainDesc.Width = width;
	dxgiSwapChainDesc.Height = height;
	dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.Stereo = FALSE;
	dxgiSwapChainDesc.SampleDesc.Count = 1;
	dxgiSwapChainDesc.SampleDesc.Quality = 1;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.BufferCount = SwapChainPlaneCount;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	dxgiSwapChainDesc.Flags = 0;

	COMPtr<IDXGISwapChain1> dxgiSwapChain1;
	dxgiFactory->CreateSwapChainForHwnd(d3dGraphicsQueue, HWND(hWnd),
		&dxgiSwapChainDesc, nullptr, nullptr, dxgiSwapChain1.initRef());

	dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&swapChain.dxgiSwapChain));

	// Allocate resources for planes
	for (uint32 i = 0; i < SwapChainPlaneCount; i++)
	{
		const sint32 resourceIndex = resourcesAllocationMask.findFirstZeroAndSet();
		XE_MASTER_ASSERT(resourceIndex >= 0);

		Resource& resource = resourcesTable[resourceIndex];
		XE_ASSERT(resource == ResourceType::Undefined);
		XE_ASSERT(!resource.d3dResource);
		resource.type = ResourceType::Texture;
		resource.internalOwnership = true;

		dxgiSwapChain1->GetBuffer(i, IID_PPV_ARGS(&resource.d3dResource));
	}

	return encodeSwapChainHandle(swapChainIndex);
}

void Device::writeDescriptor(DescriptorAddress descriptorAddress, ResourceViewHandle resourceViewHandle)
{
	const uint32 sourceDescriptorIndex = decodeResourceViewHandle(resourceViewHandle);
	const uint32 destDescriptorIndex = decodeDescriptorAddress(descriptorAddress);

	const uint64 sourcePtr = referenceSRVHeapStartPtr + sourceDescriptorIndex * srvDescriptorSize;
	const uint64 destPtr = shaderVisbileSRVHeapStartPtrCPU + destDescriptorIndex * srvDescriptorSize;
	d3dDevice->CopyDescriptorsSimple(1,
		D3D12_CPU_DESCRIPTOR_HANDLE{ destPtr },
		D3D12_CPU_DESCRIPTOR_HANDLE{ sourcePtr },
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Device::submitGraphics(CommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0)
{
	commandList.d3dCommandList->Close();

	ID3D12CommandList* d3dCommandListsToExecute[] = { commandList.d3dCommandList };
	d3dGraphicsQueue->ExecuteCommandLists(1, d3dCommandListsToExecute);

	for (uint32 i = 0; i < fenceSignalCount; i++)
	{
		const Fence& fence = fencesTable[decodeFenceHandle(fenceSignals[i].fenceHandle)];
		d3dGraphicsQueue->Signal(fence.d3dFence, fenceSignals[i].value);
	}
}

void Device::submitFlip(SwapChainHandle swapChainHandle)
{

}

// Host ////////////////////////////////////////////////////////////////////////////////////////////

void Host::CreateDevice(Device& device)
{

}
