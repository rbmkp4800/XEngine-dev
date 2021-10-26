#include <d3d12.h>
#include <dxgi1_6.h>

#include <XEngine.Render.HAL.BinaryFormat.h>

#include "D3D12Helpers.h"

#include "XEngine.Render.HAL.D3D12.h"

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

enum class CommandList::State : uint8
{
	Undefined = 0,
	Idle,
	Recording,
	Executing,
};

struct CommandList::BindPointDesc
{
	uint32 data;

	inline bool isNameHashMatching(uint32 nameHash) const;
	inline bool isConstantsBindPoint() const;
	inline bool isConstantBufferBindPoint() const;
	inline bool isReadOnlyBufferBindPoint() const;
	inline bool isReadWriteBufferBindPoint() const;

	inline uint32 getConstantCount() const;

	inline uint32 getRootParameterIndex() const;
};

inline auto CommandList::lookupBindPointsLUT(uint32 bindPointNameHash) const -> BindPointDesc
{
	const uint32 bindPointsLUTIndex =
		(bindPointNameHash >> bindPointNameHashToLUTIndexShift) & bindPointNameHashToLUTIndexAndMask;
	const BindPointDesc bindPoint = bindPointsLUT[bindPointsLUTIndex];
	XE_ASSERT(bindPoint.isNameHashMatching(bindPointNameHash));
	return bindPoint;
}

void CommandList::begin()
{
	XE_ASSERT(device);
	XE_ASSERT(state == State::Idle);

	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);

	state = State::Recording;
}

void CommandList::setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv)
{
	XE_ASSERT(device);
	XE_ASSERT(state == State::Recording);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRTVDescriptorPtrs[4] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDSVDescriptorPtr = {};

	const bool useRTVs = rtvCount > 0;
	if (useRTVs)
	{
		XE_ASSERT(rtvs);
		XE_ASSERT(rtvCount < countof(d3dRTVDescriptorPtrs));
		for (uint8 i = 0; i < rtvCount; i++)
		{
			const uint32 descriptorIndex = device->resolveRenderTargetViewHandle(rtvs[i]);
			d3dRTVDescriptorPtrs[i].ptr = device->rtvHeapStartPtr + descriptorIndex * device->rtvDescriptorSize;
		}
	}

	const bool useDSV = dsv != ZeroDepthStencilViewHandle;
	if (useDSV)
	{
		const uint32 descriptorIndex = device->resolveDepthStencilViewHandle(dsv);
		d3dDSVDescriptorPtr.ptr = device->dsvHeapStartPtr + uint32(dsv) * device->dsvDescriptorSize;
	}

	d3dCommandList->OMSetRenderTargets(rtvCount,
		useRTVs ? d3dRTVDescriptorPtrs : nullptr, FALSE,
		useDSV ? &d3dDSVDescriptorPtr : nullptr);
}

void CommandList::setGraphicsPipeline(PipelineHandle pipelineHandle)
{
	XE_ASSERT(device);
	XE_ASSERT(state == State::Recording);

	Device::Pipeline& pipeline = device->pipelinesTable[device->resolvePipelineHandle(pipelineHandle)];
	XE_ASSERT(pipeline.type == PipelineType::Graphics);
	XE_ASSERT(pipeline.d3dPipelineState);

	// TODO: Update binding layout and root signature

	d3dCommandList->SetPipelineState(pipeline.d3dPipelineState);

	currentPipelineType = PipelineType::Graphics;
}

void CommandList::bindConstants(uint32 bindPointNameCRC,
	const void* data, uint32 size32bitValues, uint32 offset32bitValues)
{
	XE_ASSERT(device);
	XE_ASSERT(state == State::Recording);

	const BindPointDesc bindPoint = lookupBindPointsLUT(bindPointNameCRC);
	XE_ASSERT(bindPoint.isConstantsBindPoint());
	XE_ASSERT(offset32bitValues + size32bitValues < bindPoint.getConstantCount());
	const uint32 rootParameterIndex = bindPoint.getRootParameterIndex();

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, size32bitValues, data, offset32bitValues);
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRoot32BitConstants(rootParameterIndex, size32bitValues, data, offset32bitValues);
	else
		XE_ASSERT_UNREACHABLE_CODE;
}

void CommandList::bindBuffer(BufferBindType bindType,
	uint32 bindPointNameCRC, ResourceHandle bufferHandle, uint32 offset)
{
	XE_ASSERT(device);
	XE_ASSERT(state == State::Recording);

	const BindPointDesc bindPoint = lookupBindPointsLUT(bindPointNameCRC);
	XE_ASSERT_IMPLY(bindType == BufferBindType::Constant, bindPoint.isConstantBufferBindPoint());
	XE_ASSERT_IMPLY(bindType == BufferBindType::ReadOnly, bindPoint.isReadOnlyBufferBindPoint());
	XE_ASSERT_IMPLY(bindType == BufferBindType::ReadWrite, bindPoint.isReadWriteBufferBindPoint());
	const uint32 rootParameterIndex = bindPoint.getRootParameterIndex();

	const Device::Resource& resource = device->resourcesTable[device->resolveResourceHandle(bufferHandle)];
	XE_ASSERT(resource.type == ResourceType::Buffer);
	XE_ASSERT(resource.d3dResource);

	const uint64 bufferAddress = resource.d3dResource->GetGPUVirtualAddress() + offset;

	if (currentPipelineType == PipelineType::Graphics)
	{
		if (bindType == BufferBindType::Constant)
			d3dCommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::Constant)
			d3dCommandList->SetGraphicsRootShaderResourceView(rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::Constant)
			d3dCommandList->SetGraphicsRootUnorderedAccessView(rootParameterIndex, bufferAddress);
		else
			XE_ASSERT_UNREACHABLE_CODE;
	}
	else if (currentPipelineType == PipelineType::Compute)
	{
		if (bindType == BufferBindType::Constant)
			d3dCommandList->SetComputeRootConstantBufferView(rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::Constant)
			d3dCommandList->SetComputeRootShaderResourceView(rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::Constant)
			d3dCommandList->SetComputeRootUnorderedAccessView(rootParameterIndex, bufferAddress);
		else
			XE_ASSERT_UNREACHABLE_CODE;
	}
	else
		XE_ASSERT_UNREACHABLE_CODE;
}

void CommandList::drawNonIndexed(uint32 vertexCount, uint32 baseVertexIndex)
{
	XE_ASSERT(device);
	XE_ASSERT(state == State::Recording);
	XE_ASSERT(currentPipelineType == PipelineType::Graphics);
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
	ResourceHandle resourceHandle;
	ResourceViewType type;
	uint8 handleGeneration;
};

struct Device::BindingLayout
{
	ID3D12RootSignature* d3dRootSignature;
	uint8 bindPointNameHashToLUTIndexShift;
	uint8 bindPointNameHashToLUTIndexAndMask;
	uint8 handleGeneration;

	uint32 bindPointsLUT[MaxRootBindPointCount];
};

struct Device::Pipeline
{
	ID3D12PipelineState* d3dPipelineState;
	BindingLayout bindingLayoutHandle;
	PipelineType type;
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
			MaxResourceDescriptorCount, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
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

	resourcesTableAllocationMask.clear();
	resourceViewsTableAllocationMask.clear();
	renderTargetViewsTableAllocationMask.clear();
	depthStencilViewsTableAllocationMask.clear();
	bindingLayoutsTableAllocationMask.clear();
	pipelinesTableAllocationMask.clear();
	fencesTableAllocationMask.clear();
	swapChainsTableAllocationMask.clear();
	allocatedResourceDescriptorCount = 0;

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
	const sint32 resourceIndex = resourcesTableAllocationMask.findFirstZeroAndSet();
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

	return composeResourceHandle(resourceIndex);
}

ResourceHandle Device::createTexture(const TextureDim& dim, TextureFormat format, uint32 flags)
{
	const sint32 resourceIndex = resourcesTableAllocationMask.findFirstZeroAndSet();
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

	return composeResourceHandle(resourceIndex);
}

ResourceViewHandle Device::createResourceView(ResourceHandle resourceHandle, const ResourceViewDesc& viewDesc)
{
	const Resource& resource = resourcesTable[resolveResourceHandle(resourceHandle)];
	XE_ASSERT(resource.d3dResource);

	const D3D12_RESOURCE_DESC d3dResourceDesc = resource.d3dResource->GetDesc();

	const sint32 viewIndex = resourceViewsTableAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(viewIndex >= 0);

	ResourceView& resourceView = resourceViewsTable[viewIndex];
	XE_ASSERT(resourceView.type == ResourceViewType::Undefined);
	resourceView.resourceHandle = resourceHandle;
	resourceView.type = viewDesc.type;

	bool useSRV = false;
	bool useUAV = false;
	D3D12_SHADER_RESOURCE_VIEW_DESC d3dSRVDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC d3dUAVDesc = {};

	if (viewDesc.type == ResourceViewType::ReadOnlyTexture2D)
	{
		XE_ASSERT(resource.type == ResourceType::Texture);
		d3dSRVDesc = D3D12Helpers::ShaderResourceViewDescForTexture2D(d3dResourceDesc.Format,
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

	return composeResourceViewHandle(viewIndex);
}

RenderTargetViewHandle Device::createRenderTargetView(ResourceHandle textureHandle)
{
	const Resource& resource = resourcesTable[resolveResourceHandle(textureHandle)];
	XE_ASSERT(resource.type == ResourceType::Texture);
	XE_ASSERT(resource.d3dResource);

	const sint32 viewIndex = renderTargetViewsTableAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(viewIndex >= 0);

	const uint64 descriptorPtr = rtvHeapStartPtr + rtvDescriptorSize * viewIndex;
	d3dDevice->CreateRenderTargetView(resource.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return composeRenderTargetViewHandle(viewIndex);
}

DepthStencilViewHandle Device::createDepthStencilView(ResourceHandle textureHandle)
{
	const Resource& resource = resourcesTable[resolveResourceHandle(textureHandle)];
	XE_ASSERT(resource.type == ResourceType::Texture);
	XE_ASSERT(resource.d3dResource);

	const sint32 viewIndex = depthStencilViewsTableAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(viewIndex >= 0);

	const uint64 descriptorPtr = dsvHeapStartPtr + dsvDescriptorSize * viewIndex;
	d3dDevice->CreateDepthStencilView(resource.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return composeDepthStencilViewHandle(viewIndex);
}

DescriptorAddress Device::allocateDescriptors(uint32 count)
{
	const uint32 startIndex = allocatedResourceDescriptorCount;
	allocatedResourceDescriptorCount += count;
	XE_ASSERT(allocatedResourceDescriptorCount < ShaderVisibleSRVHeapSize);

	return composeDescriptorAddress(startIndex);
}

BindingLayoutHandle Device::createBindingLayout(const void* compiledData, uint32 compiledDataLength)
{
	const byte* compiledDataBytes = (const byte*)compiledData;

	XE_MASTER_ASSERT(compiledDataLength > sizeof(BinaryFormat::BindingLayoutBlobHeader));
	const BinaryFormat::BindingLayoutBlobHeader& header =
		*(const BinaryFormat::BindingLayoutBlobHeader*)compiledDataBytes;

	XE_MASTER_ASSERT(header.signature == BinaryFormat::BindingLayoutBlobSignature);
	XE_MASTER_ASSERT(header.version == BinaryFormat::BindingLayoutBlobCurrentVerstion);
	XE_MASTER_ASSERT(header.thisBlobSize == compiledDataLength);
	XE_MASTER_ASSERT(header.bindPointCount > 0);
	XE_MASTER_ASSERT(header.bindPointCount <= MaxRootBindPointCount);

	const BinaryFormat::RootBindPointRecord* bindPoints =
		(const BinaryFormat::RootBindPointRecord*)(compiledDataBytes + sizeof(BinaryFormat::BindingLayoutBlobHeader));

	//const 

	// ...

	const uint32 headerAndBindPointsLength =
		sizeof(BinaryFormat::BindingLayoutBlobHeader) +
		sizeof(BinaryFormat::RootBindPointRecord) * header.bindPointCount;
	XE_MASTER_ASSERT(compiledDataLength > headerAndBindPointsLength);

	const void* d3dRootSignatureData = compiledDataBytes + headerAndBindPointsLength;
	const uint32 d3dRootSignatureSize = compiledDataLength - headerAndBindPointsLength;

	const sint32 bindingLayoutIndex = bindingLayoutsTableAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(bindingLayoutIndex >= 0);

	BindingLayout& bindingLayout = bindingLayoutsTable[bindingLayoutIndex];
	XE_ASSERT(!bindingLayout.d3dRootSignature);
	d3dDevice->CreateRootSignature(0, d3dRootSignatureData, d3dRootSignatureSize,
		IID_PPV_ARGS(&bindingLayout.d3dRootSignature));
}

FenceHandle Device::createFence(uint64 initialValue)
{
	const sint32 fenceIndex = fencesTableAllocationMask.findFirstZeroAndSet();
	XE_MASTER_ASSERT(fenceIndex >= 0);

	Fence& fence = fencesTable[fenceIndex];
	XE_ASSERT(!fence.d3dFence);
	d3dDevice->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3dFence));

	return composeFenceHandle(fenceIndex);
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
	const sint32 swapChainIndex = swapChainsTableAllocationMask.findFirstZeroAndSet();
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
		const sint32 resourceIndex = resourcesTableAllocationMask.findFirstZeroAndSet();
		XE_MASTER_ASSERT(resourceIndex >= 0);

		Resource& resource = resourcesTable[resourceIndex];
		XE_ASSERT(resource == ResourceType::Undefined);
		XE_ASSERT(!resource.d3dResource);
		resource.type = ResourceType::Texture;
		resource.internalOwnership = true;

		dxgiSwapChain1->GetBuffer(i, IID_PPV_ARGS(&resource.d3dResource));
		
		swapChain.planeTextures[i] = composeResourceHandle(resourceIndex);
	}

	return composeSwapChainHandle(swapChainIndex);
}

void Device::writeDescriptor(DescriptorAddress descriptorAddress, ResourceViewHandle resourceViewHandle)
{
	const uint32 sourceDescriptorIndex = resolveResourceViewHandle(resourceViewHandle);
	const uint32 destDescriptorIndex = resolveDescriptorAddress(descriptorAddress);

	const uint64 sourcePtr = referenceSRVHeapStartPtr + sourceDescriptorIndex * srvDescriptorSize;
	const uint64 destPtr = shaderVisbileSRVHeapStartPtrCPU + destDescriptorIndex * srvDescriptorSize;
	d3dDevice->CopyDescriptorsSimple(1,
		D3D12_CPU_DESCRIPTOR_HANDLE{ destPtr },
		D3D12_CPU_DESCRIPTOR_HANDLE{ sourcePtr },
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Device::submitGraphics(CommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0)
{
	XE_ASSERT(commandList.state != CommandList::State::Executing);

	commandList.d3dCommandList->Close();

	commandList.state = CommandList::State::Executing;
	commandList.currentPipelineType = PipelineType::Undefined;
	commandList.bindPointNameHashToLUTIndexShift = 0;
	commandList.bindPointNameHashToLUTIndexAndMask = 0;
	commandList.bindPointsLUT = nullptr;

	ID3D12CommandList* d3dCommandListsToExecute[] = { commandList.d3dCommandList };
	d3dGraphicsQueue->ExecuteCommandLists(1, d3dCommandListsToExecute);

	for (uint32 i = 0; i < fenceSignalCount; i++)
	{
		const Fence& fence = fencesTable[resolveFenceHandle(fenceSignals[i].fenceHandle)];
		d3dGraphicsQueue->Signal(fence.d3dFence, fenceSignals[i].value);
	}
}

void Device::submitFlip(SwapChainHandle swapChainHandle)
{
	SwapChain& swapChain = swapChainsTable[resolveSwapChainHandle(swapChainHandle)];
	swapChain.dxgiSwapChain->Present(1, 0);
}

uint64 Device::getFenceValue(FenceHandle fenceHandle) const
{
	const Fence& fence = fencesTable[resolveFenceHandle(fenceHandle)];
	return fence.d3dFence->GetCompletedValue();
}

ResourceHandle Device::getSwapChainPlaneTexture(SwapChainHandle swapChainHandle, uint32 planeIndex) const
{
	const SwapChain& swapChain = swapChainsTable[resolveSwapChainHandle(swapChainHandle)];
	XE_ASSERT(planeIndex < countof(swapChain.planeTextures));
	return swapChain.planeTextures[planeIndex];
}

// Host ////////////////////////////////////////////////////////////////////////////////////////////

void Host::CreateDevice(Device& device)
{

}
