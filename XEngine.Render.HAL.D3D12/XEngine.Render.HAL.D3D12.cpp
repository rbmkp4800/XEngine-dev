#include <d3d12.h>
#include <dxgi1_6.h>
#include "D3D12Helpers.h"

#include <XLib.ByteStream.h>

#include <XEngine.Render.HAL.ObjectFormat.h>

#include "XEngine.Render.HAL.D3D12.h"

using namespace XLib::Platform;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::HAL::Internal;

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

static inline DXGI_FORMAT TranslateTexelFormatToDXGIFormat(TexelFormat format)
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

static inline bool ValidateGenericObjectHeader(const ObjectFormat::GenericObjectHeader* header,
	uint64 signature, uint32 objectSize)
{
	XEMasterAssert(header->signature == signature);
	XEMasterAssert(header->objectSize == objectSize);
	// TODO: Check object CRC
}

struct XEngine::Render::HAL::Internal::PipelineBindPointsLUTEntry
{
	uint32 data;

	inline bool isNameCRCMatching(uint32 nameCRC) const;
	inline PipelineBindPointType getType() const;

	inline uint8 getRootParameterIndex() const;
	inline uint8 getConstantsSize32bitValues() const;
};

// CommandList /////////////////////////////////////////////////////////////////////////////////////

enum class CommandList::State : uint8
{
	Undefined = 0,
	Idle,
	Recording,
	Executing,
};

inline PipelineBindPointsLUTEntry CommandList::lookupBindPointsLUT(uint32 bindPointNameCRC) const
{
	const uint32 lutIndex = (bindPointNameHash >> bindPointsLUTKeyShift) & bindPointsLUTKeyAndMask;
	const PipelineBindPointsLUTEntry lutEntry = bindPointsLUT[lutIndex];
	XEAssert(lutEntry.isNameCRCMatching(bindPointNameCRC));
	return lutEntry;
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

void CommandList::setPipelineType(PipelineType pipelineType)
{
	XEAssert(state == State::Recording);
	if (currentPipelineType == pipelineType)
		return;

	currentPipelineLayoutHandle = ZeroPipelineLayoutHandle;
}

void CommandList::setPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle)
{
	XEAssert(state == State::Recording);

	if (currentPipelineLayoutHandle == pipelineLayoutHandle)
		return;

	const Device::PipelineLayout& pipelineLayout =
		device->pipelineLayoutsTable[device->resolvePipelineLayoutHandle(pipelineLayoutHandle)];

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRootSignature(pipelineLayout.d3dRootSignature);
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRootSignature(pipelineLayout.d3dRootSignature);
	else
		XEAssertUnreachableCode();

	currentPipelineLayoutHandle = pipelineLayoutHandle;
}

void CommandList::setPipeline(PipelineHandle pipelineHandle)
{
	XEAssert(state == State::Recording);

	const Device::Pipeline& pipeline = device->pipelinesTable[device->resolvePipelineHandle(pipelineHandle)];
	XEAssert(pipeline.type == currentPipelineType);
	XEAssert(pipeline.pipelineLayoutHandle == currentPipelineLayoutHandle);

	XEAssert(pipeline.d3dPipelineState);
	d3dCommandList->SetPipelineState(pipeline.d3dPipelineState);
}

void CommandList::bindConstants(PipelineBindPointId bindPointId,
	const void* data, uint32 size32bitValues, uint32 offset32bitValues = 0)
{
	XEAssert(state == State::Recording);

	const uint32 rootParameterIndex = ...;

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, size32bitValues, data, offset32bitValues);
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRoot32BitConstants(rootParameterIndex, size32bitValues, data, offset32bitValues);
	else
		XEAssertUnreachableCode();
}

void CommandList::bindConstants(uint32 bindPointNameCRC,
	const void* data, uint32 size32bitValues, uint32 offset32bitValues)
{
	XEAssert(state == State::Recording);

	const PipelineBindPointsLUTEntry bindPointsLUTEntry = lookupBindPointsLUT(bindPointNameCRC);
	XEAssert(bindPointsLUTEntry.getType() == PipelineBindPointType::Constants);
	XEAssert(offset32bitValues + size32bitValues <= bindPointsLUTEntry.getConstantsSize32bitValues());
	const uint32 rootParameterIndex = bindPointsLUTEntry.getRootParameterIndex();

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

	const PipelineBindPointsLUTEntry bindPointsLUTEntry = lookupBindPointsLUT(bindPointNameCRC);
	XEAssertImply(bindType == BufferBindType::Constant, bindPointsLUTEntry.getType() == PipelineBindPointType::ConstantBuffer);
	XEAssertImply(bindType == BufferBindType::ReadOnly, bindPointsLUTEntry.getType() == PipelineBindPointType::ReadOnlyBuffer);
	XEAssertImply(bindType == BufferBindType::ReadWrite, bindPointsLUTEntry.getType() == PipelineBindPointType::ReadWriteBuffer);
	const uint32 rootParameterIndex = bindPointsLUTEntry.getRootParameterIndex();

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

void CommandList::copyFromBufferToBuffer(ResourceHandle srcBufferHandle, uint64 srcOffset,
	ResourceHandle destBufferHandle, uint64 destOffset, uint64 size)
{
	XEAssert(state == State::Recording);

	const Device::Resource& srcBuffer = device->resourcesTable[device->resolveResourceHandle(srcBufferHandle)];
	const Device::Resource& destBuffer = device->resourcesTable[device->resolveResourceHandle(destBufferHandle)];
	XEAssert(srcBuffer.type == ResourceType::Buffer && srcBuffer.d3dResource);
	XEAssert(destBuffer.type == ResourceType::Buffer && destBuffer.d3dResource);

	d3dCommandList->CopyBufferRegion(destBuffer.d3dResource, destOffset, srcBuffer.d3dResource, srcOffset, size);
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
	uint32 sourceHash;
	uint8 handleGeneration;

	uint32 bindPointsLUT[MaxPipelineBindPointCount];
};

struct Device::Pipeline
{
	ID3D12PipelineState* d3dPipelineState;
	PipelineLayoutHandle pipelineLayoutHandle;
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

ResourceHandle Device::createBuffer(uint64 size, BufferMemoryType memoryType, BufferCreationFlags flags)
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

	const DXGI_FORMAT dxgiFormat = TranslateTexelFormatToDXGIFormat(format);
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
	XEAssert(shaderResourceView.type == ShaderResourceViewType::Undefined);
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

PipelineLayoutHandle Device::createPipelineLayout(ObjectDataView objectData)
{
	using namespace ObjectFormat;

	const sint32 pipelineLayoutIndex = pipelineLayoutsTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineLayoutIndex >= 0);

	PipelineLayout& pipelineLayout = pipelineLayoutsTable[pipelineLayoutIndex];

	const byte* objectDataBytes = (const byte*)objectData.data;

	XEMasterAssert(objectData.size > sizeof(PipelineLayoutObjectHeader));
	const PipelineLayoutObjectHeader& header = *(const PipelineLayoutObjectHeader*)objectDataBytes;
	XEMasterAssert(ValidateGenericObjectHeader(&header.generic, PipelineLayoutObjectSignature, objectData.size));

	XEMasterAssert(header.bindPointCount > 0);
	XEMasterAssert(header.bindPointCount <= MaxPipelineBindPointCount);
	const uint8 bindPointsLUTSizeLog2 = uint8(32 - countLeadingZeros32(header.bindPointCount));
	const uint8 bindPointsLUTSize = 1 << bindPointsLUTSizeLog2;

	XEMasterAssert(header.bindPointsLUTKeyShift + bindPointsLUTSizeLog2 < 32);
	const uint8 bindPointsLUTKeyShift = header.bindPointsLUTKeyShift;
	const uint8 bindPointsLUTKeyAndMask = bindPointsLUTSize - 1;

	pipelineLayout.bindPointsLUTKeyShift = bindPointsLUTKeyShift;
	pipelineLayout.bindPointsLUTKeyAndMask = bindPointsLUTKeyAndMask;

	const PipelineBindPointRecord* bindPoints =
		(const PipelineBindPointRecord*)(objectDataBytes + sizeof(PipelineLayoutObjectHeader));

	for (uint8 i = 0; i < header.bindPointCount; i++)
	{
		const PipelineBindPointRecord& bindPoint = bindPoints[i];
		XEMasterAssert(bindPoint.nameCRC);
		const uint8 lutIndex = (bindPoint.nameCRC >> bindPointsLUTKeyShift) & bindPointsLUTKeyAndMask;

		XEMasterAssert(!pipelineLayout.bindPointsLUT[lutIndex]); // Check collision
		pipelineLayout.bindPointsLUT[lutIndex] = ...;
	}

	const uint32 headerAndBindPointsLength =
		sizeof(PipelineLayoutObjectHeader) + sizeof(PipelineBindPointRecord) * header.bindPointCount;
	XEMasterAssert(objectData.size > headerAndBindPointsLength);

	const void* d3dRootSignatureData = objectDataBytes + headerAndBindPointsLength;
	const uint32 d3dRootSignatureSize = objectData.size - headerAndBindPointsLength;

	XEAssert(!pipelineLayout.d3dRootSignature);
	d3dDevice->CreateRootSignature(0, d3dRootSignatureData, d3dRootSignatureSize,
		IID_PPV_ARGS(&pipelineLayout.d3dRootSignature));

	return composePipelineLayoutHandle(pipelineLayoutIndex);
}

PipelineHandle Device::createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle,
	ObjectDataView baseObjectData, const ObjectDataView* bytecodeObjectsData, uint8 bytecodeObjectCount,
	const RasterizerDesc& rasterizerDesc, const DepthStencilDesc& depthStencilDesc, const BlendDesc& blendDesc)
{
	using namespace ObjectFormat;

	const Device::PipelineLayout& pipelineLayout = pipelineLayoutsTable[resolvePipelineLayoutHandle(pipelineLayoutHandle)];
	XEAssert(pipelineLayout.d3dRootSignature);

	const sint32 pipelineIndex = pipelineLayoutsTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineIndex >= 0);

	Pipeline& pipeline = pipelinesTable[pipelineIndex];
	pipeline.pipelineLayoutHandle = pipelineLayoutHandle;
	pipeline.type = PipelineType::Graphics;

	XEMasterAssert(baseObjectData.size == sizeof(GraphicsPipelineBaseObject));
	const GraphicsPipelineBaseObject& baseObject = *(const GraphicsPipelineBaseObject*)baseObjectData.data;

	XEMasterAssert(ValidateGenericObjectHeader(&baseObject.generic, GraphicsPipelineBaseObjectSignature, baseObjectData.size));

	XEMasterAssert(baseObject.pipelineLayoutSourceHash == pipelineLayout.sourceHash);

	// Validate enabled shader stages combination
	XEMasterAssert(baseObject.enabledShaderStages.vertex ^ baseObject.enabledShaderStages.mesh);
	XEMasterAssertImply(baseObject.enabledShaderStages.vertex, !baseObject.enabledShaderStages.amplification);

	// Calculate expected bytecode objects number and their types

	GraphicsPipelineBytecodeObjectType expectedBytecodeObjectTypes[MaxGraphicsPipelineBytecodeObjectCount] = {};
	uint8 expectedBytecodeObjectCount = 0;

	if (baseObject.enabledShaderStages.vertex)
		expectedBytecodeObjectTypes[expectedBytecodeObjectCount++] = GraphicsPipelineBytecodeObjectType::VertexShader;
	if (baseObject.enabledShaderStages.amplification)
		expectedBytecodeObjectTypes[expectedBytecodeObjectCount++] = GraphicsPipelineBytecodeObjectType::AmplificationShader;
	if (baseObject.enabledShaderStages.mesh)
		expectedBytecodeObjectTypes[expectedBytecodeObjectCount++] = GraphicsPipelineBytecodeObjectType::MeshShader;
	if (baseObject.enabledShaderStages.pixel)
		expectedBytecodeObjectTypes[expectedBytecodeObjectCount++] = GraphicsPipelineBytecodeObjectType::PixelShader;

	XEAssert(expectedBytecodeObjectCount <= MaxGraphicsPipelineBytecodeObjectCount); // TODO: UB here :(
	XEMasterAssert(expectedBytecodeObjectCount == bytecodeObjectCount);

	// Go through bytecode objects and fill collect D3D12 shader bytecodes

	D3D12_SHADER_BYTECODE d3dVS = {};
	D3D12_SHADER_BYTECODE d3dAS = {};
	D3D12_SHADER_BYTECODE d3dMS = {};
	D3D12_SHADER_BYTECODE d3dPS = {};

	for (uint8 i = 0; i < expectedBytecodeObjectCount; i++)
	{
		const ObjectDataView& bytecodeObjectData = bytecodeObjectsData[i];
		const byte* bytecodeObjectDataBytes = (const byte*)bytecodeObjectData.data;

		XEAssert(bytecodeObjectData.size > sizeof(GraphicsPipelineBytecodeObjectHeader));
		const GraphicsPipelineBytecodeObjectHeader& bytecodeObjectHeader =
			*(const GraphicsPipelineBytecodeObjectHeader*)bytecodeObjectDataBytes;

		XEMasterAssert(ValidateGenericObjectHeader(&bytecodeObjectHeader.generic,
			GraphicsPipelineBytecodeObjectSignature, bytecodeObjectData.size));

		XEMasterAssert(baseObject.bytecodeObjectsCRCs[i] == bytecodeObjectHeader.generic.objectCRC);
		XEMasterAssert(baseObject.generic.objectCRC == bytecodeObjectHeader.baseObjectCRC);
		XEMasterAssert(expectedBytecodeObjectTypes[i] == bytecodeObjectHeader.objectType);

		D3D12_SHADER_BYTECODE *d3dBytecodeStorePtr = nullptr;
		switch (bytecodeObjectHeader.objectType)
		{
			case GraphicsPipelineBytecodeObjectType::VertexShader:			d3dBytecodeStorePtr = &d3dVS; break;
			case GraphicsPipelineBytecodeObjectType::AmplificationShader:	d3dBytecodeStorePtr = &d3dAS; break;
			case GraphicsPipelineBytecodeObjectType::MeshShader:			d3dBytecodeStorePtr = &d3dMS; break;
			case GraphicsPipelineBytecodeObjectType::PixelShader:			d3dBytecodeStorePtr = &d3dPS; break;
		}
		XEAssert(d3dBytecodeStorePtr);

		XEMasterAssert(!d3dBytecodeStorePtr->pShaderBytecode);
		d3dBytecodeStorePtr->pShaderBytecode = bytecodeObjectDataBytes + sizeof(GraphicsPipelineBytecodeObjectHeader);
		d3dBytecodeStorePtr->BytecodeLength = bytecodeObjectData.size - sizeof(GraphicsPipelineBytecodeObjectHeader);
	}

	// Count render targets in base object
	uint8 renderTargetCount = 0;
	for (TexelFormat renderTargetFormat : baseObject.renderTargetFormats)
	{
		if (renderTargetFormat == TexelFormat::Undefined)
			break;
		renderTargetCount++;
	}
	for (uint8 i = renderTargetCount; i < countof(baseObject.renderTargetFormats); i++)
		XEMasterAssert(baseObject.renderTargetFormats[i] == TexelFormat::Undefined);

	// Compose D3D12 Pipeline State stream

	byte d3dPSOStreamBuffer[256]; // TODO: Proper size
	XLib::ByteStreamWriter d3dPSOStreamWriter(d3dPSOStreamBuffer, sizeof(d3dPSOStreamBuffer));

	d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, sizeof(void*));
	d3dPSOStreamWriter.writeAligned(pipelineLayout.d3dRootSignature, sizeof(void*));

	if (d3dVS.pShaderBytecode)
	{
		d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, sizeof(void*));
		d3dPSOStreamWriter.writeAligned(d3dVS, sizeof(void*));
	}
	if (d3dAS.pShaderBytecode)
	{
		d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, sizeof(void*));
		d3dPSOStreamWriter.writeAligned(d3dAS, sizeof(void*));
	}
	if (d3dMS.pShaderBytecode)
	{
		d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, sizeof(void*));
		d3dPSOStreamWriter.writeAligned(d3dMS, sizeof(void*));
	}
	if (d3dPS.pShaderBytecode)
	{
		d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, sizeof(void*));
		d3dPSOStreamWriter.writeAligned(d3dPS, sizeof(void*));
	}

	{
		// TODO: ...
		// D3D12_BLEND_DESC d3dBlendDesc = {};
		// d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, sizeof(void*));
		// d3dPSOStreamWriter.writeAligned(d3dBlendDesc, sizeof(void*));
	}

	{
		// TODO: ...
		// D3D12_RASTERIZER_DESC d3dRasterizerDesc = {};
		// d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, sizeof(void*));
		// d3dPSOStreamWriter.writeAligned(d3dRasterizerDesc, sizeof(void*));
	}

	{
		D3D12_DEPTH_STENCIL_DESC d3dDepthStencilDesc = {};
		d3dDepthStencilDesc.DepthEnable = depthStencilDesc.enableDepthRead;
		d3dDepthStencilDesc.DepthWriteMask = depthStencilDesc.enableDepthWrite ?
			D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // TODO: ...

		d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, sizeof(void*));
		d3dPSOStreamWriter.writeAligned(d3dDepthStencilDesc, sizeof(void*));
	}

	if (renderTargetCount > 0)
	{
		D3D12_RT_FORMAT_ARRAY d3dRTFormatArray = {};
		for (uint8 i = 0; i < renderTargetCount; i++)
			d3dRTFormatArray.RTFormats[i] = TranslateTexelFormatToDXGIFormat(baseObject.renderTargetFormats[i]);
		d3dRTFormatArray.NumRenderTargets = renderTargetCount;

		d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, sizeof(void*));
		d3dPSOStreamWriter.writeAligned(d3dRTFormatArray, sizeof(void*));
	}

	if (baseObject.depthStencilFormat != TexelFormat::Undefined)
	{
		d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, sizeof(void*));
		d3dPSOStreamWriter.writeAligned(TranslateTexelFormatToDXGIFormat(baseObject.depthStencilFormat), sizeof(void*));
	}

	D3D12_PIPELINE_STATE_STREAM_DESC d3dPSOStreamDesc = {};
	d3dPSOStreamDesc.SizeInBytes = d3dPSOStreamWriter.getLength();
	d3dPSOStreamDesc.pPipelineStateSubobjectStream = d3dPSOStreamWriter.getData();

	XEAssert(!pipeline.d3dPipelineState);
	d3dDevice->CreatePipelineState(&d3dPSOStreamDesc, IID_PPV_ARGS(&pipeline.d3dPipelineState));

	return composePipelineHandle(pipelineIndex);
}

PipelineHandle Device::createComputePipeline(PipelineLayoutHandle pipelineLayoutHandle, ObjectDataView bytecodeObjectData)
{
	using namespace ObjectFormat;

	const Device::PipelineLayout& pipelineLayout = pipelineLayoutsTable[resolvePipelineLayoutHandle(pipelineLayoutHandle)];
	XEAssert(pipelineLayout.d3dRootSignature);

	const sint32 pipelineIndex = pipelineLayoutsTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineIndex >= 0);

	Pipeline& pipeline = pipelinesTable[pipelineIndex];
	pipeline.pipelineLayoutHandle = pipelineLayoutHandle;
	pipeline.type = PipelineType::Compute;

	const byte* objectDataBytes = (const byte*)bytecodeObjectData.data;

	XEMasterAssert(bytecodeObjectData.size > sizeof(ComputePipelineBytecodeObjectHeader));
	const ComputePipelineBytecodeObjectHeader& header = *(const ComputePipelineBytecodeObjectHeader*)objectDataBytes;

	XEMasterAssert(ValidateGenericObjectHeader(&header.generic, ComputePipelineBaseObjectSignature, baseObjectData.size));

	XEMasterAssert(header.pipelineLayoutSourceHash == pipelineLayout.sourceHash);

	D3D12_SHADER_BYTECODE d3dCS = {};
	d3dCS.pShaderBytecode = objectDataBytes + sizeof(ComputePipelineBytecodeObjectHeader);
	d3dCS.BytecodeLength = bytecodeObjectData.size - sizeof(ComputePipelineBytecodeObjectHeader);

	// Compose D3D12 Pipeline State stream

	byte d3dPSOStreamBuffer[64]; // TODO: Proper size
	XLib::ByteStreamWriter d3dPSOStreamWriter(d3dPSOStreamBuffer, sizeof(d3dPSOStreamBuffer));

	d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, sizeof(void*));
	d3dPSOStreamWriter.writeAligned(pipelineLayout.d3dRootSignature, sizeof(void*));

	d3dPSOStreamWriter.writeAligned(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, sizeof(void*));
	d3dPSOStreamWriter.writeAligned(d3dCS, sizeof(void*));

	D3D12_PIPELINE_STATE_STREAM_DESC d3dPSOStreamDesc = {};
	d3dPSOStreamDesc.SizeInBytes = d3dPSOStreamWriter.getLength();
	d3dPSOStreamDesc.pPipelineStateSubobjectStream = d3dPSOStreamWriter.getData();

	XEAssert(!pipeline.d3dPipelineState);
	d3dDevice->CreatePipelineState(&d3dPSOStreamDesc, IID_PPV_ARGS(&pipeline.d3dPipelineState));

	return composePipelineHandle(pipelineIndex);
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
