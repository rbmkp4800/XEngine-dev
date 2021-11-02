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

	XEMasterAssertUnreachableCode();
}

static inline DXGI_FORMAT TranslateTextureFormatToDXGIFormat(TexelFormat format)
{
	switch (format)
	{
		case TexelFormat::Undefined:		return DXGI_FORMAT_UNKNOWN;
		case TexelFormat::R8G8B8A8_UNORM:	return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	XEMasterAssertUnreachableCode();
}

static inline D3D12_RESOURCE_STATES TranslateResourceStateToD3D12ResourceState(ResourceState state)
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
	const uint32 bindPointsLUTIndex = (bindPointNameHash >> bindPointsLUTKeyShift) & bindPointsLUTKeyAndMask;
	const BindPointDesc bindPoint = bindPointsLUT[bindPointsLUTIndex];
	XEAssert(bindPoint.isNameHashMatching(bindPointNameHash));
	return bindPoint;
}

void CommandList::begin()
{
	XEAssert(state == State::Idle);

	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);

	state = State::Recording;
}

void CommandList::clearRenderTarget(RenderTargetViewHandle rtv, const float32* color)
{
	XEAssert(state == State::Recording);

	const uint32 descriptorIndex = device->resolveRenderTargetViewHandle(rtv);
	const uint64 descriptorPtr = device->rtvHeapStartPtr + descriptorIndex * device->rtvDescriptorSize;
	d3dCommandList->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr }, color, 0, nullptr);
}

void CommandList::clearDepthStencil(DepthStencilViewHandle dsv,
	bool clearDepth, bool clearStencil, float32 depth, uint8 stencil)
{
	XEAssert(state == State::Recording);
	XEAssert(clearDepth || clearStencil);

	const uint32 descriptorIndex = device->resolveDepthStencilViewHandle(dsv);
	const uint64 descriptorPtr = device->dsvHeapStartPtr + descriptorIndex * device->dsvDescriptorSize;

	D3D12_CLEAR_FLAGS d3dClearFlags = D3D12_CLEAR_FLAGS(0);
	if (clearDepth)
		d3dClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
	if (clearStencil)
		d3dClearFlags |= D3D12_CLEAR_FLAG_STENCIL;

	d3dCommandList->ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr },
		d3dClearFlags, depth, stencil, 0, nullptr);
}

void CommandList::setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv)
{
	XEAssert(state == State::Recording);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRTVDescriptorPtrs[4] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDSVDescriptorPtr = {};

	const bool useRTVs = rtvCount > 0;
	if (useRTVs)
	{
		XEAssert(rtvs);
		XEAssert(rtvCount < countof(d3dRTVDescriptorPtrs));
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

void CommandList::setViewport(const Viewport& viewport)
{
	XEAssert(state == State::Recording);

	D3D12_VIEWPORT d3dViewport = {};
	d3dViewport.TopLeftX = viewport.left;
	d3dViewport.TopLeftY = viewport.top;
	d3dViewport.Width = viewport.right - viewport.left;
	d3dViewport.Height = viewport.bottom - viewport.top;
	d3dViewport.MinDepth = viewport.depthMin;
	d3dViewport.MaxDepth = viewport.depthMax;

	d3dCommandList->RSSetViewports(1, &d3dViewport);
}

void CommandList::setPipeline(PipelineType pipelineType, PipelineHandle pipelineHandle)
{
	XEAssert(state == State::Recording);
	XEAssert(pipelineType == PipelineType::Graphics || pipelineType == PipelineType::Compute);

	const Device::Pipeline& pipeline = device->pipelinesTable[device->resolvePipelineHandle(pipelineHandle)];
	XEAssert(pipeline.type == pipelineType);

	if (currentPipelineLayoutHandle != pipeline.layoutHandle)
	{
		const Device::PipelineLayout& pipelineLayout =
			device->pipelineLayoutsTable[device->resolvePipelineLayoutHandle(pipeline.layoutHandle)];

		bindPointsLUTKeyShift = pipelineLayout.bindPointsLUTKeyShift;
		bindPointsLUTKeyAndMask = pipelineLayout.bindPointsLUTKeyAndMask;
		bindPointsLUT = (const BindPointDesc*)pipelineLayout.bindPointsLUT;

		XEAssert(pipelineLayout.d3dRootSignature);
		if (pipelineType == PipelineType::Graphics)
			d3dCommandList->SetGraphicsRootSignature(pipelineLayout.d3dRootSignature);
		else if (pipelineType == PipelineType::Compute)
			d3dCommandList->SetComputeRootSignature(pipelineLayout.d3dRootSignature);
		else
			XEAssertUnreachableCode();

		currentPipelineLayoutHandle = pipeline.layoutHandle;
	}

	XEAssert(pipeline.d3dPipelineState);
	d3dCommandList->SetPipelineState(pipeline.d3dPipelineState);

	currentPipelineType = pipelineType;
}

void CommandList::bindConstants(uint32 bindPointNameCRC,
	const void* data, uint32 size32bitValues, uint32 offset32bitValues)
{
	XEAssert(state == State::Recording);

	const BindPointDesc bindPoint = lookupBindPointsLUT(bindPointNameCRC);
	XEAssert(bindPoint.isConstantsBindPoint());
	XEAssert(offset32bitValues + size32bitValues < bindPoint.getConstantCount());
	const uint32 rootParameterIndex = bindPoint.getRootParameterIndex();

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, size32bitValues, data, offset32bitValues);
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRoot32BitConstants(rootParameterIndex, size32bitValues, data, offset32bitValues);
	else
		XEAssertUnreachableCode();
}

void CommandList::bindBuffer(BufferBindType bindType,
	uint32 bindPointNameCRC, ResourceHandle bufferHandle, uint32 offset)
{
	XEAssert(state == State::Recording);

	const BindPointDesc bindPoint = lookupBindPointsLUT(bindPointNameCRC);
	XEAssertImply(bindType == BufferBindType::Constant, bindPoint.isConstantBufferBindPoint());
	XEAssertImply(bindType == BufferBindType::ReadOnly, bindPoint.isReadOnlyBufferBindPoint());
	XEAssertImply(bindType == BufferBindType::ReadWrite, bindPoint.isReadWriteBufferBindPoint());
	const uint32 rootParameterIndex = bindPoint.getRootParameterIndex();

	const Device::Resource& resource = device->resourcesTable[device->resolveResourceHandle(bufferHandle)];
	XEAssert(resource.type == ResourceType::Buffer);
	XEAssert(resource.d3dResource);

	const uint64 bufferAddress = resource.d3dResource->GetGPUVirtualAddress() + offset;

	if (currentPipelineType == PipelineType::Graphics)
	{
		if (bindType == BufferBindType::Constant)
			d3dCommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadOnly)
			d3dCommandList->SetGraphicsRootShaderResourceView(rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadWrite)
			d3dCommandList->SetGraphicsRootUnorderedAccessView(rootParameterIndex, bufferAddress);
		else
			XEAssertUnreachableCode();
	}
	else if (currentPipelineType == PipelineType::Compute)
	{
		if (bindType == BufferBindType::Constant)
			d3dCommandList->SetComputeRootConstantBufferView(rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadOnly)
			d3dCommandList->SetComputeRootShaderResourceView(rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadWrite)
			d3dCommandList->SetComputeRootUnorderedAccessView(rootParameterIndex, bufferAddress);
		else
			XEAssertUnreachableCode();
	}
	else
		XEAssertUnreachableCode();
}

void CommandList::drawNonIndexed(uint32 vertexCount, uint32 startVertexIndex)
{
	XEAssert(state == State::Recording);
	XEAssert(currentPipelineType == PipelineType::Graphics);
	d3dCommandList->DrawInstanced(vertexCount, 1, startVertexIndex, 0);
}

void CommandList::dispatch(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ)
{
	XEAssert(state == State::Recording);
	XEAssert(currentPipelineType == PipelineType::Compute);
	d3dCommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandList::resourceStateTransition(ResourceHandle resourceHandle,
	ResourceState stateBefore, ResourceState stateAfter)
{
	XEAssert(state == State::Recording);

	const Device::Resource& resource = device->resourcesTable[device->resolveResourceHandle(resourceHandle)];
	XEAssert(resource.d3dResource);

	const D3D12_RESOURCE_BARRIER d3dBarrier =
		D3D12Helpers::ResourceBarrierTransition(resource.d3dResource,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			TranslateResourceStateToD3D12ResourceState(stateBefore),
			TranslateResourceStateToD3D12ResourceState(stateAfter));
	d3dCommandList->ResourceBarrier(1, &d3dBarrier);
}

// Device //////////////////////////////////////////////////////////////////////////////////////////

struct Device::Resource
{
	ID3D12Resource2* d3dResource;
	ResourceType type;
	uint8 handleGeneration;
	bool internalOwnership; // For example swap chain. User can't release it
};

struct Device::ShaderResourceView
{
	ResourceHandle resourceHandle;
	ShaderResourceViewType type;
	uint8 handleGeneration;
};

struct Device::PipelineLayout
{
	ID3D12RootSignature* d3dRootSignature;
	uint8 bindPointsLUTKeyShift;
	uint8 bindPointsLUTKeyAndMask;
	uint8 handleGeneration;

	uint32 bindPointsLUT[MaxPipelineBindPointCount];
};

struct Device::Pipeline
{
	ID3D12PipelineState* d3dPipelineState;
	PipelineLayoutHandle layoutHandle;
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
	XEMasterAssert(!d3dDevice);

	D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_12_1, d3dDevice.uuid(), d3dDevice.voidInitRef());

	const D3D12_DESCRIPTOR_HEAP_DESC d3dReferenceSRVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxShaderResourceViewCount);
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
	shaderResourceViewsTableAllocationMask.clear();
	renderTargetViewsTableAllocationMask.clear();
	depthStencilViewsTableAllocationMask.clear();
	pipelineLayoutsTableAllocationMask.clear();
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

ResourceHandle Device::createBuffer(uint32 size, BufferMemoryType memoryType, BufferCreationFlags flags)
{
	const sint32 resourceIndex = resourcesTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(resourceIndex >= 0);

	Resource& resource = resourcesTable[resourceIndex];
	XEAssert(resource == ResourceType::Undefined);
	XEAssert(!resource.d3dResource);
	resource.type = ResourceType::Buffer;
	resource.internalOwnership = false;

	const D3D12_HEAP_TYPE d3dHeapType = TranslateBufferMemoryTypeToD3D12HeapType(memoryType);
	const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapProperties(d3dHeapType);

	D3D12_RESOURCE_FLAGS d3dResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	if (flags.allowShaderWrite)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	const D3D12_RESOURCE_DESC d3dResourceDesc = D3D12Helpers::ResourceDescForBuffer(size);

	d3dDevice->CreateCommittedResource(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));

	return composeResourceHandle(resourceIndex);
}

ResourceHandle Device::createTexture(const TextureDim& dim, TexelFormat format, TextureCreationFlags flags)
{
	const sint32 resourceIndex = resourcesTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(resourceIndex >= 0);

	Resource& resource = resourcesTable[resourceIndex];
	XEAssert(resource == ResourceType::Undefined);
	XEAssert(!resource.d3dResource);
	resource.type = ResourceType::Texture;
	resource.internalOwnership = false;

	const DXGI_FORMAT dxgiFormat = TranslateTextureFormatToDXGIFormat(format);
	const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapProperties(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_RESOURCE_FLAGS d3dResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	if (flags.allowRenderTarget)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (flags.allowDepthStencil)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (flags.allowShaderWrite)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// TODO: For now just Texture2D
	const D3D12_RESOURCE_DESC d3dResourceDesc =
		D3D12Helpers::ResourceDescForTexture2D(dim.width, dim.height, 0, dxgiFormat, d3dResourceFlags);

	d3dDevice->CreateCommittedResource(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));

	return composeResourceHandle(resourceIndex);
}

ShaderResourceViewHandle Device::createShaderResourceView(ResourceHandle resourceHandle, const ShaderResourceViewDesc& viewDesc)
{
	const Resource& resource = resourcesTable[resolveResourceHandle(resourceHandle)];
	XEAssert(resource.d3dResource);

	const D3D12_RESOURCE_DESC d3dResourceDesc = resource.d3dResource->GetDesc();

	const sint32 viewIndex = shaderResourceViewsTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(viewIndex >= 0);

	ShaderResourceView& shaderResourceView = shaderResourceViewsTable[viewIndex];
	XEAssert(shaderResourceView.type == ResourceViewType::Undefined);
	shaderResourceView.resourceHandle = resourceHandle;
	shaderResourceView.type = viewDesc.type;

	bool useSRV = false;
	bool useUAV = false;
	D3D12_SHADER_RESOURCE_VIEW_DESC d3dSRVDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC d3dUAVDesc = {};

	if (viewDesc.type == ShaderResourceViewType::ReadOnlyTexture2D)
	{
		XEAssert(resource.type == ResourceType::Texture);
		d3dSRVDesc = D3D12Helpers::ShaderResourceViewDescForTexture2D(d3dResourceDesc.Format,
			viewDesc.readOnlyTexture2D.startMipIndex,
			viewDesc.readOnlyTexture2D.mipLevelCount,
			viewDesc.readOnlyTexture2D.plane);
		useSRV = true;
	}
	else if (viewDesc.type == ShaderResourceViewType::ReadWriteTexture2D)
	{
		XEAssert(resource.type == ResourceType::Texture);
		//d3dUAVDesc = D3D12Helpers::UnorderedAccessViewDescForTexture2D(...);
		useUAV = true;
	}

	const uint64 descriptorPtr = rtvHeapStartPtr + rtvDescriptorSize * viewIndex;

	XEAssert(useSRV ^ useUAV);
	if (useUAV)
		d3dDevice->CreateShaderResourceView(resource.d3dResource, &d3dSRVDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });
	if (useSRV)
		d3dDevice->CreateUnorderedAccessView(resource.d3dResource, nullptr, &d3dUAVDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return composeShaderResourceViewHandle(viewIndex);
}

RenderTargetViewHandle Device::createRenderTargetView(ResourceHandle textureHandle)
{
	const Resource& resource = resourcesTable[resolveResourceHandle(textureHandle)];
	XEAssert(resource.type == ResourceType::Texture);
	XEAssert(resource.d3dResource);

	const sint32 viewIndex = renderTargetViewsTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(viewIndex >= 0);

	const uint64 descriptorPtr = rtvHeapStartPtr + rtvDescriptorSize * viewIndex;
	d3dDevice->CreateRenderTargetView(resource.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return composeRenderTargetViewHandle(viewIndex);
}

DepthStencilViewHandle Device::createDepthStencilView(ResourceHandle textureHandle)
{
	const Resource& resource = resourcesTable[resolveResourceHandle(textureHandle)];
	XEAssert(resource.type == ResourceType::Texture);
	XEAssert(resource.d3dResource);

	const sint32 viewIndex = depthStencilViewsTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(viewIndex >= 0);

	const uint64 descriptorPtr = dsvHeapStartPtr + dsvDescriptorSize * viewIndex;
	d3dDevice->CreateDepthStencilView(resource.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return composeDepthStencilViewHandle(viewIndex);
}

DescriptorAddress Device::allocateDescriptors(uint32 count)
{
	const uint32 startIndex = allocatedResourceDescriptorCount;
	allocatedResourceDescriptorCount += count;
	XEAssert(allocatedResourceDescriptorCount < ShaderVisibleSRVHeapSize);

	return composeDescriptorAddress(startIndex);
}

PipelineLayoutHandle Device::createPipelineLayout(const void* compiledData, uint32 compiledDataLength)
{
	const sint32 pipelineLayoutIndex = pipelineLayoutsTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineLayoutIndex >= 0);

	PipelineLayout& pipelineLayout = pipelineLayoutsTable[pipelineLayoutIndex];

	const byte* compiledDataBytes = (const byte*)compiledData;

	XEMasterAssert(compiledDataLength > sizeof(BinaryFormat::PipelineLayoutHeader));
	const BinaryFormat::PipelineLayoutHeader& header = *(const BinaryFormat::PipelineLayoutHeader*)compiledDataBytes;

	XEMasterAssert(header.signature == BinaryFormat::PipelineLayoutBlobSignature);
	XEMasterAssert(header.version == BinaryFormat::PipelineLayoutBlobCurrentVerstion);
	XEMasterAssert(header.thisBlobSize == compiledDataLength);

	XEMasterAssert(header.bindPointCount > 0);
	XEMasterAssert(header.bindPointCount <= MaxPipelineBindPointCount);
	const uint8 bindPointsLUTSizeLog2 = uint8(32 - countLeadingZeros32(header.bindPointCount));
	const uint8 bindPointsLUTSize = 1 << bindPointsLUTSizeLog2;

	XEMasterAssert(header.bindPointsLUTKeyShift + bindPointsLUTSizeLog2 < 32);
	const uint8 bindPointsLUTKeyShift = header.bindPointsLUTKeyShift;
	const uint8 bindPointsLUTKeyAndMask = bindPointsLUTSize - 1;

	pipelineLayout.bindPointsLUTKeyShift = bindPointsLUTKeyShift;
	pipelineLayout.bindPointsLUTKeyAndMask = bindPointsLUTKeyAndMask;

	const BinaryFormat::PipelineBindPointRecord* bindPoints =
		(const BinaryFormat::PipelineBindPointRecord*)(compiledDataBytes + sizeof(BinaryFormat::PipelineLayoutHeader));

	for (uint8 i = 0; i < header.bindPointCount; i++)
	{
		const BinaryFormat::PipelineBindPointRecord& bindPoint = bindPoints[i];
		XEMasterAssert(bindPoint.bindPointNameHash);
		const uint8 lutIndex = (bindPoint.bindPointNameHash >> bindPointsLUTKeyShift) & bindPointsLUTKeyAndMask;

		XEMasterAssert(!pipelineLayout.bindPointsLUT[lutIndex]); // Check collision
		pipelineLayout.bindPointsLUT[lutIndex] = ...;
	}

	const uint32 headerAndBindPointsLength =
		sizeof(BinaryFormat::PipelineLayoutHeader) +
		sizeof(BinaryFormat::PipelineBindPointRecord) * header.bindPointCount;
	XEMasterAssert(compiledDataLength > headerAndBindPointsLength);

	const void* d3dRootSignatureData = compiledDataBytes + headerAndBindPointsLength;
	const uint32 d3dRootSignatureSize = compiledDataLength - headerAndBindPointsLength;

	XEAssert(!pipelineLayout.d3dRootSignature);
	d3dDevice->CreateRootSignature(0, d3dRootSignatureData, d3dRootSignatureSize,
		IID_PPV_ARGS(&pipelineLayout.d3dRootSignature));

	return composePipelineLayoutHandle(pipelineLayoutIndex);
}

FenceHandle Device::createFence(uint64 initialValue)
{
	const sint32 fenceIndex = fencesTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(fenceIndex >= 0);

	Fence& fence = fencesTable[fenceIndex];
	XEAssert(!fence.d3dFence);
	d3dDevice->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3dFence));

	return composeFenceHandle(fenceIndex);
}

void Device::createCommandList(CommandList& commandList, CommandListType type)
{
	XEAssert(type == CommandListType::Graphics); // TODO: ...
	XEAssert(!commandList.d3dCommandList && !commandList.d3dCommandAllocator);

	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandList.d3dCommandAllocator));
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandList.d3dCommandAllocator, nullptr,
		IID_PPV_ARGS(&commandList.d3dCommandList));
	commandList.d3dCommandList->Close();

	commandList.state = CommandList::State::Idle;
}

SwapChainHandle Device::createSwapChain(uint16 width, uint16 height, void* hWnd)
{
	const sint32 swapChainIndex = swapChainsTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(swapChainIndex >= 0);

	SwapChain& swapChain = swapChainsTable[swapChainIndex];
	XEAssert(!swapChain.dxgiSwapChain);

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
		XEMasterAssert(resourceIndex >= 0);

		Resource& resource = resourcesTable[resourceIndex];
		XEAssert(resource == ResourceType::Undefined);
		XEAssert(!resource.d3dResource);
		resource.type = ResourceType::Texture;
		resource.internalOwnership = true;

		dxgiSwapChain1->GetBuffer(i, IID_PPV_ARGS(&resource.d3dResource));
		
		swapChain.planeTextures[i] = composeResourceHandle(resourceIndex);
	}

	return composeSwapChainHandle(swapChainIndex);
}

void Device::writeDescriptor(DescriptorAddress descriptorAddress, ShaderResourceViewHandle srvHandle)
{
	const uint32 sourceDescriptorIndex = resolveShaderResourceViewHandle(srvHandle);
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
	XEAssert(commandList.state != CommandList::State::Executing);

	commandList.d3dCommandList->Close();

	commandList.state = CommandList::State::Executing;
	commandList.currentPipelineType = PipelineType::Undefined;
	commandList.currentPipelineLayoutHandle = ZeroPipelineLayoutHandle;
	commandList.bindPointsLUTKeyShift = 0;
	commandList.bindPointsLUTKeyAndMask = 0;
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
	XEAssert(planeIndex < countof(swapChain.planeTextures));
	return swapChain.planeTextures[planeIndex];
}

// Host ////////////////////////////////////////////////////////////////////////////////////////////

void Host::CreateDevice(Device& device)
{

}
