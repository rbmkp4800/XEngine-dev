#include <d3d12.h>
#include <dxgi1_6.h>
#include "D3D12Helpers.h"

#include <XLib.ByteStream.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.HAL.ObjectFormat.h>

#include "XEngine.Render.HAL.D3D12.h"

#include "XEngine.Render.HAL.D3D12.Translation.h"

// Microsoft.Direct3D.D3D12 v1.700.10
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 700; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

using namespace XLib;
using namespace XLib::Platform;
using namespace XEngine::Render::HAL;
//using namespace XEngine::Render::HAL::Internal;

static COMPtr<IDXGIFactory7> dxgiFactory;
static COMPtr<ID3D12Debug3> d3dDebug;

static inline bool ValidateGenericObjectHeader(const ObjectFormat::GenericObjectHeader* header,
	uint64 signature, uint32 objectSize)
{
	if (header->signature != signature)
		return false;
	if (header->objectSize != objectSize)
		return false;
	// TODO: Check object CRC

	return true;
}

//struct XEngine::Render::HAL::Internal::PipelineBindPointsLUTEntry
//{
//	uint32 data;
//
//	inline bool isNameCRCMatching(uint32 nameCRC) const;
//	inline PipelineBindPointType getType() const;
//
//	inline uint8 getRootParameterIndex() const;
//	inline uint8 getConstantsSize32bitValues() const;
//};


struct Device::MemoryBlock
{
	ID3D12Heap* d3dHeap;
	uint8 handleGeneration;
};

struct Device::Resource
{
	ID3D12Resource2* d3dResource;
	ResourceType type;
	uint8 handleGeneration;
	bool internalOwnership; // For example swap chain. User can't release it.

	union
	{
		struct
		{
			uint16x3 size;
			TextureDimension dimension;
			TextureFormat format;
			uint8 mipLevelCount;
		} texture;

		struct
		{
			uint64 size;
		} buffer;
	};
};

struct Device::ResourceView
{
	ResourceHandle resourceHandle;
	ResourceViewType type;
	uint8 handleGeneration;
};

struct Device::PipelineLayout
{
	ID3D12RootSignature* d3dRootSignature;
	uint32 sourceHash;
	uint8 handleGeneration;

	//uint32 bindPointsLUT[MaxPipelineBindPointCount];
	ObjectFormat::PipelineBindPointRecord bindPoints[MaxPipelineBindPointCount];
	uint8 bindPointCount;
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
	ResourceHandle textures[SwapChainTextureCount];
};


// CommandList /////////////////////////////////////////////////////////////////////////////////////

enum class CommandList::State : uint8
{
	Undefined = 0,
	Idle,
	Recording,
	Executing,
};

//inline PipelineBindPointsLUTEntry CommandList::lookupBindPointsLUT(uint32 bindPointNameCRC) const
//{
	//const uint32 lutIndex = (bindPointNameHash >> bindPointsLUTKeyShift) & bindPointsLUTKeyAndMask;
	//const PipelineBindPointsLUTEntry lutEntry = bindPointsLUT[lutIndex];
	//XEAssert(lutEntry.isNameCRCMatching(bindPointNameCRC));
	//return lutEntry;
//}

CommandList::~CommandList()
{
	XEAssertUnreachableCode(); // Not implemented.
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

	const Device::PipelineLayout& pipelineLayout = device->getPipelineLayoutByHandle(pipelineLayoutHandle);

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

	const Device::Pipeline& pipeline = device->getPipelineByHandle(pipelineHandle);
	XEAssert(pipeline.type == currentPipelineType);
	XEAssert(pipeline.pipelineLayoutHandle == currentPipelineLayoutHandle);

	XEAssert(pipeline.d3dPipelineState);
	d3dCommandList->SetPipelineState(pipeline.d3dPipelineState);
}

void CommandList::bindConstants(uint32 bindPointNameCRC,
	const void* data, uint32 size32bitValues, uint32 offset32bitValues)
{
	XEAssert(state == State::Recording);
	XEAssert(currentPipelineLayoutHandle != ZeroPipelineLayoutHandle);

	const Device::PipelineLayout& pipelineLayout = device->getPipelineLayoutByHandle(currentPipelineLayoutHandle);

	const ObjectFormat::PipelineBindPointRecord* bindPointRecord = nullptr;
	for (uint8 i = 0; i < pipelineLayout.bindPointCount; i++)
	{
		if (pipelineLayout.bindPoints[i].nameCRC == bindPointNameCRC)
		{
			bindPointRecord = &pipelineLayout.bindPoints[i];
			break;
		}
	}

	XEAssert(bindPointRecord);

	//const PipelineBindPointsLUTEntry bindPointsLUTEntry = lookupBindPointsLUT(bindPointNameCRC);
	//XEAssert(bindPointsLUTEntry.getType() == PipelineBindPointType::Constants);
	//XEAssert(offset32bitValues + size32bitValues <= bindPointsLUTEntry.getConstantsSize32bitValues());
	const uint32 rootParameterIndex = bindPointRecord->rootParameterIndex; //bindPointsLUTEntry.getRootParameterIndex();

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, size32bitValues, data, offset32bitValues);
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRoot32BitConstants(rootParameterIndex, size32bitValues, data, offset32bitValues);
	else
		XEAssertUnreachableCode();
}

void CommandList::bindBuffer(uint32 bindPointNameCRC,
	BufferBindType bindType, ResourceHandle bufferHandle, uint32 offset)
{
	XEAssert(state == State::Recording);

	//const PipelineBindPointsLUTEntry bindPointsLUTEntry = lookupBindPointsLUT(bindPointNameCRC);
	//XEAssertImply(bindType == BufferBindType::Constant, bindPointsLUTEntry.getType() == PipelineBindPointType::ConstantBuffer);
	//XEAssertImply(bindType == BufferBindType::ReadOnly, bindPointsLUTEntry.getType() == PipelineBindPointType::ReadOnlyBuffer);
	//XEAssertImply(bindType == BufferBindType::ReadWrite, bindPointsLUTEntry.getType() == PipelineBindPointType::ReadWriteBuffer);
	//const uint32 rootParameterIndex = bindPointsLUTEntry.getRootParameterIndex();
	XEAssertUnreachableCode(); // Not implemented.
	const uint32 rootParameterIndex = 0;

	const Device::Resource& resource = device->getResourceByHandle(bufferHandle);
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

void CommandList::bufferMemoryBarrier(ResourceHandle bufferHandle,
	BarrierSyncMask syncBefore, BarrierSyncMask syncAfter,
	BarrierAccessMask accessBefore, BarrierAccessMask accessAfter)
{
	XEAssert(state == State::Recording);

	const Device::Resource& buffer = device->getResourceByHandle(bufferHandle);
	XEAssert(buffer.type == ResourceType::Buffer && buffer.d3dResource);

	D3D12_BUFFER_BARRIER d3dBufferBarrier = {};
	d3dBufferBarrier.SyncBefore = TranslateBarrierSyncMaskToD3D12BarrierSync(syncBefore);
	d3dBufferBarrier.SyncAfter  = TranslateBarrierSyncMaskToD3D12BarrierSync(syncAfter);
	d3dBufferBarrier.AccessBefore = TranslateBarrierAccessMaskToD3D12BarrierAccess(accessBefore);
	d3dBufferBarrier.AccessAfter  = TranslateBarrierAccessMaskToD3D12BarrierAccess(accessAfter);
	d3dBufferBarrier.pResource = buffer.d3dResource;
	d3dBufferBarrier.Offset = 0; // D3D12 does not allow to apply barrier to buffer portion :(
	d3dBufferBarrier.Size = UINT64(-1);

	D3D12_BARRIER_GROUP d3dBarrierGroup = {};
	d3dBarrierGroup.Type = D3D12_BARRIER_TYPE_BUFFER;
	d3dBarrierGroup.NumBarriers = 1;
	d3dBarrierGroup.pBufferBarriers = &d3dBufferBarrier;

	d3dCommandList->Barrier(1, &d3dBarrierGroup);
}

void CommandList::textureMemoryBarrier(ResourceHandle textureHandle,
	BarrierSyncMask syncBefore, BarrierSyncMask syncAfter,
	BarrierAccessMask accessBefore, BarrierAccessMask accessAfter,
	TextureLayout layoutBefore, TextureLayout layoutAfter,
	const TextureSubresourceRange* subresourceRange)
{
	XEAssert(state == State::Recording);

	const Device::Resource& texture = device->getResourceByHandle(textureHandle);
	XEAssert(texture.type == ResourceType::Texture && texture.d3dResource);

	D3D12_TEXTURE_BARRIER d3dTextureBarrier = {};
	d3dTextureBarrier.SyncBefore = TranslateBarrierSyncMaskToD3D12BarrierSync(syncBefore);
	d3dTextureBarrier.SyncAfter  = TranslateBarrierSyncMaskToD3D12BarrierSync(syncAfter);
	d3dTextureBarrier.AccessBefore = TranslateBarrierAccessMaskToD3D12BarrierAccess(accessBefore);
	d3dTextureBarrier.AccessAfter  = TranslateBarrierAccessMaskToD3D12BarrierAccess(accessAfter);
	d3dTextureBarrier.LayoutBefore = TranslateTextureLayoutToD3D12BarrierLayout(layoutBefore);
	d3dTextureBarrier.LayoutAfter  = TranslateTextureLayoutToD3D12BarrierLayout(layoutAfter);
	d3dTextureBarrier.pResource = texture.d3dResource;

	if (subresourceRange)
	{
		// TODO: Validate `subresourceRange`.
		// TODO: Create shortcuts like "zero `subresourceRange->mipLevelCount` means all mips".

		XEAssert(subresourceRange->mipLevelCount > 0);
		// NOTE: `IndexOrFirstMipLevel` will be considered subresource index if `NumMipLevels` is zero.

		d3dTextureBarrier.Subresources.IndexOrFirstMipLevel = subresourceRange->baseMipLevel;
		d3dTextureBarrier.Subresources.NumMipLevels = subresourceRange->mipLevelCount;
		d3dTextureBarrier.Subresources.FirstArraySlice = subresourceRange->baseArraySlice;
		d3dTextureBarrier.Subresources.NumArraySlices = subresourceRange->arraySliceCount;
		d3dTextureBarrier.Subresources.FirstPlane = 0;
		d3dTextureBarrier.Subresources.NumPlanes = 1; // TODO: Handle this properly.
	}
	else
	{
		d3dTextureBarrier.Subresources.IndexOrFirstMipLevel = 0;
		d3dTextureBarrier.Subresources.NumMipLevels = texture.texture.mipLevelCount;
		d3dTextureBarrier.Subresources.FirstArraySlice = 0;
		d3dTextureBarrier.Subresources.NumArraySlices =
			(texture.texture.dimension == TextureDimension::Texture2DArray) ? texture.texture.size.z : 1; // TODO: Other dim arrays
		d3dTextureBarrier.Subresources.FirstPlane = 0;
		d3dTextureBarrier.Subresources.NumPlanes = 1; // TODO: Handle this properly.
	}

	D3D12_BARRIER_GROUP d3dBarrierGroup = {};
	d3dBarrierGroup.Type = D3D12_BARRIER_TYPE_TEXTURE;
	d3dBarrierGroup.NumBarriers = 1;
	d3dBarrierGroup.pTextureBarriers = &d3dTextureBarrier;

	d3dCommandList->Barrier(1, &d3dBarrierGroup);
}

void CommandList::copyBuffer(ResourceHandle dstBufferHandle, uint64 dstOffset,
	ResourceHandle srcBufferHandle, uint64 srcOffset, uint64 size)
{
	XEAssert(state == State::Recording);

	const Device::Resource& dstBuffer = device->getResourceByHandle(dstBufferHandle);
	const Device::Resource& srcBuffer = device->getResourceByHandle(srcBufferHandle);
	XEAssert(dstBuffer.type == ResourceType::Buffer && dstBuffer.d3dResource);
	XEAssert(srcBuffer.type == ResourceType::Buffer && srcBuffer.d3dResource);

	d3dCommandList->CopyBufferRegion(dstBuffer.d3dResource, dstOffset, srcBuffer.d3dResource, srcOffset, size);
}

void CommandList::copyTexture(ResourceHandle dstTextureHandle, TextureSubresource dstSubresource, uint16x3 dstOffset,
	ResourceHandle srcTextureHandle, TextureSubresource srcSubresource, const TextureRegion* srcRegion)
{
	XEAssert(state == State::Recording);

	const Device::Resource& dstTexture = device->getResourceByHandle(dstTextureHandle);
	const Device::Resource& srcTexture = device->getResourceByHandle(srcTextureHandle);
	XEAssert(dstTexture.type == ResourceType::Texture && dstTexture.d3dResource);
	XEAssert(srcTexture.type == ResourceType::Texture && srcTexture.d3dResource);

	D3D12_TEXTURE_COPY_LOCATION d3dDstLocation = {};
	d3dDstLocation.pResource = dstTexture.d3dResource;
	d3dDstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	d3dDstLocation.SubresourceIndex = Device::CalculateTextureSubresourceIndex(dstTexture, dstSubresource);

	D3D12_TEXTURE_COPY_LOCATION d3dSrcLocation = {};
	d3dSrcLocation.pResource = srcTexture.d3dResource;
	d3dSrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	d3dSrcLocation.SubresourceIndex = Device::CalculateTextureSubresourceIndex(srcTexture, srcSubresource);

	const D3D12_BOX d3dSrcBox = srcRegion ?
		D3D12BoxFromOffsetAndSize(srcRegion->offset, srcRegion->size) :
		D3D12BoxFromOffsetAndSize(uint16x3(0, 0, 0),
			Host::CalculateMipLevelSize(srcTexture.texture.size, srcSubresource.mipLevel));

	// TODO: Add texture subresource bounds validation.

	d3dCommandList->CopyTextureRegion(&d3dDstLocation, dstOffset.x, dstOffset.y, dstOffset.z, &d3dSrcLocation, &d3dSrcBox);
}

void CommandList::copyBufferTexture(CopyBufferTextureDirection direction,
	ResourceHandle bufferHandle, uint64 bufferOffset, uint32 bufferRowPitch,
	ResourceHandle textureHandle, TextureSubresource textureSubresource, const TextureRegion* textureRegion)
{
	XEAssert(state == State::Recording);

	// TODO: BC formats handling

	const Device::Resource& buffer = device->getResourceByHandle(bufferHandle);
	const Device::Resource& texture = device->getResourceByHandle(textureHandle);
	XEAssert(buffer.type == ResourceType::Buffer && buffer.d3dResource);
	XEAssert(texture.type == ResourceType::Texture && texture.d3dResource);

	uint16x3 textureRegionSize = textureRegion ?
		textureRegion->size : Host::CalculateMipLevelSize(texture.texture.size, textureSubresource.mipLevel);

	D3D12_TEXTURE_COPY_LOCATION d3dBufferLocation = {};
	d3dBufferLocation.pResource = buffer.d3dResource;
	d3dBufferLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	d3dBufferLocation.PlacedFootprint.Offset = bufferOffset;
	d3dBufferLocation.PlacedFootprint.Footprint.Format = TranslateTextureFormatToDXGIFormat(texture.texture.format);
	d3dBufferLocation.PlacedFootprint.Footprint.Width = textureRegionSize.x;
	d3dBufferLocation.PlacedFootprint.Footprint.Height = textureRegionSize.y;
	d3dBufferLocation.PlacedFootprint.Footprint.Depth = textureRegionSize.z;
	d3dBufferLocation.PlacedFootprint.Footprint.RowPitch = bufferRowPitch;

	D3D12_TEXTURE_COPY_LOCATION d3dTextureLocation = {};
	d3dTextureLocation.pResource = texture.d3dResource;
	d3dTextureLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	d3dTextureLocation.SubresourceIndex = Device::CalculateTextureSubresourceIndex(texture, textureSubresource);

	const bool copyBufferToTexture = (direction == CopyBufferTextureDirection::BufferToTexture);
	const bool copyTextureToBuffer = !copyBufferToTexture;

	const D3D12_TEXTURE_COPY_LOCATION& d3dDstLocation = copyBufferToTexture ? d3dTextureLocation : d3dBufferLocation;
	const D3D12_TEXTURE_COPY_LOCATION& d3dSrcLocation = copyBufferToTexture ? d3dBufferLocation : d3dTextureLocation;

	const uint16x3 textureRegionOffset = textureRegion ? textureRegion->offset : uint16x3(0, 0, 0);
	const uint16x3 dstOffset = copyBufferToTexture ? textureRegionOffset : uint16x3(0, 0, 0);

	D3D12_BOX d3dSrcBox = {};
	const D3D12_BOX* p_d3dSrcBox = nullptr;
	if (copyTextureToBuffer)
	{
		d3dSrcBox = textureRegion ?
			D3D12BoxFromOffsetAndSize(textureRegion->offset, textureRegion->size) :
			D3D12BoxFromOffsetAndSize(uint16x3(0, 0, 0),
				Host::CalculateMipLevelSize(texture.texture.size, textureSubresource.mipLevel));
		p_d3dSrcBox = &d3dSrcBox;
	}

	d3dCommandList->CopyTextureRegion(&d3dDstLocation, dstOffset.x, dstOffset.y, dstOffset.z,
		&d3dSrcLocation, p_d3dSrcBox);
}


// Device //////////////////////////////////////////////////////////////////////////////////////////

uint32 Device::CalculateTextureSubresourceIndex(const Resource& resource, const TextureSubresource& subresource)
{
	XEAssert(resource.type == ResourceType::Texture);
	XEAssert(subresource.mipLevel < resource.texture.mipLevelCount);
	XEAssert(imply(subresource.arraySlice > 0, resource.texture.dimension == TextureDimension::Texture2DArray)); // TODO: Other dim arrays
	XEAssert(subresource.arraySlice < resource.texture.size.z);

	const TextureFormat format = resource.texture.format;
	const bool hasStencil = (format == TextureFormat::D24S8 || format == TextureFormat::D32S8);
	const bool isDepthStencilTexture = (format == TextureFormat::D16 || format == TextureFormat::D32 || hasStencil);
	const bool isColorAspect = (subresource.aspect == TextureAspect::Color);
	const bool isStencilAspect = (subresource.aspect == TextureAspect::Stencil);
	XEAssert(isDepthStencilTexture ^ isColorAspect);
	XEAssert(imply(isStencilAspect, hasStencil));

	const uint32 planeIndex = isStencilAspect ? 1 : 0;
	const uint32 mipLevelCount = resource.texture.mipLevelCount;
	const uint32 arraySize = resource.texture.size.z;

	return subresource.mipLevel + (subresource.arraySlice * mipLevelCount) + (planeIndex * mipLevelCount * arraySize);
}

void Device::initialize(ID3D12Device10* _d3dDevice)
{
	XEMasterAssert(!d3dDevice);
	d3dDevice = _d3dDevice;

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

	// Allocate object tables.
	{
		// TODO: Take into account alignments.
		constexpr uintptr objectTablesMemorySize =
			sizeof(MemoryBlock) * MaxMemoryBlockCount +
			sizeof(Resource) * MaxResourceCount +
			sizeof(ResourceView) * MaxResourceViewCount +
			sizeof(PipelineLayout) * MaxPipelineLayoutCount +
			sizeof(Pipeline) * MaxPipelineCount +
			sizeof(Fence) * MaxFenceCount +
			sizeof(SwapChain) * MaxSwapChainCount;

		// TODO: Handle this memory in proper way.
		void* objectTablesMemory = SystemHeapAllocator::Allocate(objectTablesMemorySize);
		memorySet(objectTablesMemory, 0, objectTablesMemorySize);

		memoryBlockTable = (MemoryBlock*)objectTablesMemory;
		resourceTable = (Resource*)(memoryBlockTable + MaxMemoryBlockCount);
		resourceViewTable = (ResourceView*)(resourceTable + MaxResourceCount);
		pipelineLayoutTable = (PipelineLayout*)(resourceViewTable + MaxResourceViewCount);
		pipelineTable = (Pipeline*)(pipelineLayoutTable + MaxPipelineLayoutCount);
		fenceTable = (Fence*)(pipelineTable + MaxPipelineCount);
		swapChainTable = (SwapChain*)(fenceTable + MaxFenceCount);
		void* objectTablesMemoryEnd = swapChainTable + MaxSwapChainCount;

		XEAssert(uintptr(objectTablesMemory) + objectTablesMemorySize == uintptr(objectTablesMemoryEnd));
	}

	memoryBlockTableAllocationMask.clear();
	resourceTableAllocationMask.clear();
	resourceViewTableAllocationMask.clear();
	renderTargetViewTableAllocationMask.clear();
	depthStencilViewTableAllocationMask.clear();
	pipelineLayoutTableAllocationMask.clear();
	pipelineTableAllocationMask.clear();
	fenceTableAllocationMask.clear();
	swapChainTableAllocationMask.clear();
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

MemoryBlockHandle Device::allocateMemory(uint64 size, MemoryType memoryType)
{
	const sint32 memoryBlockIndex = memoryBlockTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(memoryBlockIndex >= 0);

	MemoryBlock& memoryBlock = memoryBlockTable[memoryBlockIndex];
	XEAssert(!memoryBlock.d3dHeap);

	const D3D12_HEAP_TYPE d3dHeapType = TranslateMemoryTypeToD3D12HeapType(memoryType);

	D3D12_HEAP_DESC d3dHeapDesc = {};
	d3dHeapDesc.SizeInBytes = size;
	d3dHeapDesc.Properties = D3D12Helpers::HeapProperties(d3dHeapType);
	d3dHeapDesc.Alignment = 0;
	d3dHeapDesc.Flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
	// TODO: Check D3D12_RESOURCE_HEAP_TIER_2 during starup. We need it for `D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES`.
	// TODO: Check if we need `D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS`.

	d3dDevice->CreateHeap(&d3dHeapDesc, IID_PPV_ARGS(&memoryBlock.d3dHeap));

	return composeMemoryBlockHandle(memoryBlockIndex);
}

ResourceHandle Device::createBuffer(uint64 size, BufferFlags flags,
	MemoryBlockHandle memoryBlockHandle, uint64 memoryBlockOffset)
{
	const MemoryBlock& memoryBlock = getMemoryBlockByHandle(memoryBlockHandle);
	XEAssert(memoryBlock.d3dHeap);

	const sint32 resourceIndex = resourceTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(resourceIndex >= 0);

	Resource& resource = resourceTable[resourceIndex];
	XEAssert(resource.type == ResourceType::Undefined);
	XEAssert(!resource.d3dResource);
	resource.type = ResourceType::Buffer;
	resource.internalOwnership = false;

	D3D12_RESOURCE_FLAGS d3dResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	if (flags.allowShaderWrite)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	const D3D12_RESOURCE_DESC1 d3dResourceDesc = D3D12Helpers::ResourceDesc1ForBuffer(size);

	// TODO: Check that resource fits into memory block.
	// TODO: Check alignment.
	d3dDevice->CreatePlacedResource2(memoryBlock.d3dHeap, memoryBlockOffset,
		&d3dResourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, 0, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));

	return composeResourceHandle(resourceIndex);
}

ResourceHandle Device::createTexture(TextureDimension dimension, uint16x3 size,
	TextureFormat format, uint8 mipLevelCount, TextureFlags flags,
	MemoryBlockHandle memoryBlockHandle, uint64 memoryBlockOffset)
{
	const MemoryBlock& memoryBlock = getMemoryBlockByHandle(memoryBlockHandle);
	XEAssert(memoryBlock.d3dHeap);

	const sint32 resourceIndex = resourceTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(resourceIndex >= 0);

	Resource& resource = resourceTable[resourceIndex];
	XEAssert(resource.type == ResourceType::Undefined);
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

	XEAssert(dimension == TextureDimension::Texture2D); // Not implemented.

	// TODO: Check if `mipLevelCount` is not greater than max possible level count for this resource.
	const D3D12_RESOURCE_DESC1 d3dResourceDesc =
		D3D12Helpers::ResourceDesc1ForTexture2D(size.x, size.y, mipLevelCount, dxgiFormat, d3dResourceFlags);

	// TODO: Check that resource fits into memory block.
	// TODO: Check alignment.
	// TODO: Handle initial layout properly.
	d3dDevice->CreatePlacedResource2(memoryBlock.d3dHeap, memoryBlockOffset,
		&d3dResourceDesc, D3D12_BARRIER_LAYOUT_COMMON, nullptr, 0, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));

	return composeResourceHandle(resourceIndex);
}

ResourceViewHandle Device::createResourceView(ResourceHandle resourceHandle, const ResourceViewDesc& viewDesc)
{
	const Resource& resource = getResourceByHandle(resourceHandle);
	XEAssert(resource.d3dResource);

	const D3D12_RESOURCE_DESC d3dResourceDesc = resource.d3dResource->GetDesc();

	const sint32 viewIndex = resourceViewTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(viewIndex >= 0);

	ResourceView& resourceView = resourceViewTable[viewIndex];
	XEAssert(resourceView.type == ResourceViewType::Undefined);
	resourceView.resourceHandle = resourceHandle;
	resourceView.type = viewDesc.type;

	bool useSRV = false;
	bool useUAV = false;
	D3D12_SHADER_RESOURCE_VIEW_DESC d3dSRVDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC d3dUAVDesc = {};

	if (viewDesc.type == ResourceViewType::ReadOnlyTexture2D)
	{
		XEAssert(resource.type == ResourceType::Texture);
		d3dSRVDesc = D3D12Helpers::ShaderResourceViewDescForTexture2D(d3dResourceDesc.Format,
			viewDesc.readOnlyTexture2D.startMipIndex,
			viewDesc.readOnlyTexture2D.mipLevelCount,
			viewDesc.readOnlyTexture2D.plane);
		useSRV = true;
	}
	else if (viewDesc.type == ResourceViewType::ReadWriteTexture2D)
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

	return composeResourceViewHandle(viewIndex);
}

RenderTargetViewHandle Device::createRenderTargetView(ResourceHandle textureHandle)
{
	const Resource& resource = getResourceByHandle(textureHandle);
	XEAssert(resource.type == ResourceType::Texture);
	XEAssert(resource.d3dResource);

	const sint32 viewIndex = renderTargetViewTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(viewIndex >= 0);

	const uint64 descriptorPtr = rtvHeapStartPtr + rtvDescriptorSize * viewIndex;
	d3dDevice->CreateRenderTargetView(resource.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return composeRenderTargetViewHandle(viewIndex);
}

DepthStencilViewHandle Device::createDepthStencilView(ResourceHandle textureHandle)
{
	const Resource& resource = getResourceByHandle(textureHandle);
	XEAssert(resource.type == ResourceType::Texture);
	XEAssert(resource.d3dResource);

	const sint32 viewIndex = depthStencilViewTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(viewIndex >= 0);

	const uint64 descriptorPtr = dsvHeapStartPtr + dsvDescriptorSize * viewIndex;
	d3dDevice->CreateDepthStencilView(resource.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorPtr });

	return composeDepthStencilViewHandle(viewIndex);
}

DescriptorAddress Device::allocateDescriptors(uint32 count)
{
	const uint32 startIndex = allocatedResourceDescriptorCount;
	allocatedResourceDescriptorCount += count;
	XEAssert(allocatedResourceDescriptorCount < MaxResourceDescriptorCount);

	return composeDescriptorAddress(startIndex);
}

PipelineLayoutHandle Device::createPipelineLayout(ObjectDataView objectData)
{
	using namespace ObjectFormat;

	const sint32 pipelineLayoutIndex = pipelineLayoutTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineLayoutIndex >= 0);

	PipelineLayout& pipelineLayout = pipelineLayoutTable[pipelineLayoutIndex];

	const byte* objectDataBytes = (const byte*)objectData.data;

	XEMasterAssert(objectData.size > sizeof(PipelineLayoutObjectHeader));
	const PipelineLayoutObjectHeader& header = *(const PipelineLayoutObjectHeader*)objectDataBytes;
	XEMasterAssert(ValidateGenericObjectHeader(&header.generic, PipelineLayoutObjectSignature, objectData.size));

	XEMasterAssert(header.bindPointCount > 0);
	XEMasterAssert(header.bindPointCount <= MaxPipelineBindPointCount);
	//const uint8 bindPointsLUTSizeLog2 = uint8(32 - countLeadingZeros32(header.bindPointCount));
	//const uint8 bindPointsLUTSize = 1 << bindPointsLUTSizeLog2;

	//XEMasterAssert(header.bindPointsLUTKeyShift + bindPointsLUTSizeLog2 < 32);
	//const uint8 bindPointsLUTKeyShift = header.bindPointsLUTKeyShift;
	//const uint8 bindPointsLUTKeyAndMask = bindPointsLUTSize - 1;

	//pipelineLayout.bindPointsLUTKeyShift = bindPointsLUTKeyShift;
	//pipelineLayout.bindPointsLUTKeyAndMask = bindPointsLUTKeyAndMask;

	pipelineLayout.bindPointCount = header.bindPointCount;

	const PipelineBindPointRecord* bindPoints =
		(const PipelineBindPointRecord*)(objectDataBytes + sizeof(PipelineLayoutObjectHeader));

	for (uint8 i = 0; i < header.bindPointCount; i++)
	{
		const PipelineBindPointRecord& bindPoint = bindPoints[i];
		XEMasterAssert(bindPoint.nameCRC);
		//const uint8 lutIndex = (bindPoint.nameCRC >> bindPointsLUTKeyShift) & bindPointsLUTKeyAndMask;

		//XEMasterAssert(!pipelineLayout.bindPointsLUT[lutIndex]); // Check collision
		//pipelineLayout.bindPointsLUT[lutIndex] = ...;

		pipelineLayout.bindPoints[i] = bindPoint;
	}

	const uint32 headerAndBindPointsLength =
		sizeof(PipelineLayoutObjectHeader) + sizeof(PipelineBindPointRecord) * header.bindPointCount;
	XEMasterAssert(objectData.size > headerAndBindPointsLength);

	const void* d3dRootSignatureData = objectDataBytes + headerAndBindPointsLength;
	const uint32 d3dRootSignatureSize = objectData.size - headerAndBindPointsLength;

	XEAssert(!pipelineLayout.d3dRootSignature);
	HRESULT hResult = d3dDevice->CreateRootSignature(0, d3dRootSignatureData, d3dRootSignatureSize,
		IID_PPV_ARGS(&pipelineLayout.d3dRootSignature));
	XEMasterAssert(hResult == S_OK);

	return composePipelineLayoutHandle(pipelineLayoutIndex);
}

PipelineHandle Device::createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle,
	ObjectDataView baseObjectData, const ObjectDataView* bytecodeObjectsData, uint8 bytecodeObjectCount,
	const RasterizerDesc& rasterizerDesc, const DepthStencilDesc& depthStencilDesc, const BlendDesc& blendDesc)
{
	using namespace ObjectFormat;

	const Device::PipelineLayout& pipelineLayout = getPipelineLayoutByHandle(pipelineLayoutHandle);
	XEAssert(pipelineLayout.d3dRootSignature);

	const sint32 pipelineIndex = pipelineTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineIndex >= 0);

	Pipeline& pipeline = pipelineTable[pipelineIndex];
	pipeline.pipelineLayoutHandle = pipelineLayoutHandle;
	pipeline.type = PipelineType::Graphics;

	XEMasterAssert(baseObjectData.size == sizeof(GraphicsPipelineBaseObject));
	const GraphicsPipelineBaseObject& baseObject = *(const GraphicsPipelineBaseObject*)baseObjectData.data;

	XEMasterAssert(ValidateGenericObjectHeader(&baseObject.generic, GraphicsPipelineBaseObjectSignature, baseObjectData.size));
	XEMasterAssert(baseObject.pipelineLayoutSourceHash == pipelineLayout.sourceHash);

	// Validate enabled shader stages combination
	XEMasterAssert(baseObject.enabledShaderStages.vertex ^ baseObject.enabledShaderStages.mesh);
	XEMasterAssert(imply(baseObject.enabledShaderStages.vertex, !baseObject.enabledShaderStages.amplification));

	// Calculate expected bytecode objects number and their types

	PipelineBytecodeObjectType expectedBytecodeObjectTypes[MaxGraphicsPipelineBytecodeObjectCount] = {};
	uint8 expectedBytecodeObjectCount = 0;

	if (baseObject.enabledShaderStages.vertex)
		expectedBytecodeObjectTypes[expectedBytecodeObjectCount++] = PipelineBytecodeObjectType::VertexShader;
	if (baseObject.enabledShaderStages.amplification)
		expectedBytecodeObjectTypes[expectedBytecodeObjectCount++] = PipelineBytecodeObjectType::AmplificationShader;
	if (baseObject.enabledShaderStages.mesh)
		expectedBytecodeObjectTypes[expectedBytecodeObjectCount++] = PipelineBytecodeObjectType::MeshShader;
	if (baseObject.enabledShaderStages.pixel)
		expectedBytecodeObjectTypes[expectedBytecodeObjectCount++] = PipelineBytecodeObjectType::PixelShader;

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

		XEMasterAssert(bytecodeObjectData.size > sizeof(PipelineBytecodeObjectHeader));
		const PipelineBytecodeObjectHeader& bytecodeObjectHeader = *(PipelineBytecodeObjectHeader*)bytecodeObjectDataBytes;

		XEMasterAssert(ValidateGenericObjectHeader(&bytecodeObjectHeader.generic,
			PipelineBytecodeObjectSignature, bytecodeObjectData.size));
		XEMasterAssert(baseObject.bytecodeObjectsCRCs[i] == bytecodeObjectHeader.generic.objectCRC);
		XEMasterAssert(expectedBytecodeObjectTypes[i] == bytecodeObjectHeader.objectType);
		XEMasterAssert(baseObject.pipelineLayoutSourceHash == bytecodeObjectHeader.pipelineLayoutSourceHash);

		D3D12_SHADER_BYTECODE* d3dBytecodeStorePtr = nullptr;
		switch (bytecodeObjectHeader.objectType)
		{
			case PipelineBytecodeObjectType::VertexShader:			d3dBytecodeStorePtr = &d3dVS; break;
			case PipelineBytecodeObjectType::AmplificationShader:	d3dBytecodeStorePtr = &d3dAS; break;
			case PipelineBytecodeObjectType::MeshShader:			d3dBytecodeStorePtr = &d3dMS; break;
			case PipelineBytecodeObjectType::PixelShader:			d3dBytecodeStorePtr = &d3dPS; break;
		}
		XEAssert(d3dBytecodeStorePtr);

		XEMasterAssert(!d3dBytecodeStorePtr->pShaderBytecode);
		d3dBytecodeStorePtr->pShaderBytecode = bytecodeObjectDataBytes + sizeof(PipelineBytecodeObjectHeader);
		d3dBytecodeStorePtr->BytecodeLength = bytecodeObjectData.size - sizeof(PipelineBytecodeObjectHeader);
	}

	// Count render targets in base object
	uint8 renderTargetCount = 0;
	for (TexelViewFormat renderTargetFormat : baseObject.renderTargetFormats)
	{
		if (renderTargetFormat == TexelViewFormat::Undefined)
			break;
		renderTargetCount++;
	}
	for (uint8 i = renderTargetCount; i < countof(baseObject.renderTargetFormats); i++)
		XEMasterAssert(baseObject.renderTargetFormats[i] == TexelViewFormat::Undefined);

	// Compose D3D12 Pipeline State stream

	byte d3dPSOStreamBuffer[1024]; // TODO: Proper size
	XLib::ByteStreamWriter d3dPSOStreamWriter(d3dPSOStreamBuffer, sizeof(d3dPSOStreamBuffer));

	{
		D3D12Helpers::PipelineStateSubobjectRootSignature& d3dSubobjectRS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectRootSignature>(sizeof(void*));
		d3dSubobjectRS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
		d3dSubobjectRS.rootSignature = pipelineLayout.d3dRootSignature;
	}

	if (d3dVS.pShaderBytecode)
	{
		D3D12Helpers::PipelineStateSubobjectShader& d3dSubobjectVS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectShader>(sizeof(void*));
		d3dSubobjectVS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
		d3dSubobjectVS.bytecode = d3dVS;
	}
	if (d3dAS.pShaderBytecode)
	{
		D3D12Helpers::PipelineStateSubobjectShader& d3dSubobjectAS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectShader>(sizeof(void*));
		d3dSubobjectAS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
		d3dSubobjectAS.bytecode = d3dAS;
	}
	if (d3dMS.pShaderBytecode)
	{
		D3D12Helpers::PipelineStateSubobjectShader& d3dSubobjectMS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectShader>(sizeof(void*));
		d3dSubobjectMS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;
		d3dSubobjectMS.bytecode = d3dMS;
	}
	if (d3dPS.pShaderBytecode)
	{
		D3D12Helpers::PipelineStateSubobjectShader& d3dSubobjectPS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectShader>(sizeof(void*));
		d3dSubobjectPS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
		d3dSubobjectPS.bytecode = d3dPS;
	}

	{
		// TODO: ...
		// D3D12_BLEND_DESC& d3dBlendDesc = ...;
	}

	{
		// TODO: ...
		// D3D12_RASTERIZER_DESC& d3dRasterizerDesc = ...;
	}

	{
		D3D12Helpers::PipelineStateSubobjectDepthStencil& d3dSubobjectDS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectDepthStencil>(sizeof(void*));
		d3dSubobjectDS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL;
		d3dSubobjectDS.desc = {};

		D3D12_DEPTH_STENCIL_DESC& d3dDepthStencilDesc = d3dSubobjectDS.desc;
		d3dDepthStencilDesc.DepthEnable = depthStencilDesc.enableDepthRead;
		d3dDepthStencilDesc.DepthWriteMask = depthStencilDesc.enableDepthWrite ?
			D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // TODO: ...
	}

	if (renderTargetCount > 0)
	{
		D3D12Helpers::PipelineStateSubobjectRenderTargetFormats& d3dSubobjectRTs =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectRenderTargetFormats>(sizeof(void*));
		d3dSubobjectRTs.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
		d3dSubobjectRTs.formats = {};

		D3D12_RT_FORMAT_ARRAY& d3dRTFormatArray = d3dSubobjectRTs.formats;
		for (uint8 i = 0; i < renderTargetCount; i++)
			d3dRTFormatArray.RTFormats[i] = TranslateTexelViewFormatToDXGIFormat(baseObject.renderTargetFormats[i]);
		d3dRTFormatArray.NumRenderTargets = renderTargetCount;
	}

	if (baseObject.depthStencilFormat != DepthStencilFormat::Undefined)
	{
		D3D12Helpers::PipelineStateSubobjectDepthStencilFormat& d3dSubobjectDSFormat =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectDepthStencilFormat>(sizeof(void*));
		d3dSubobjectDSFormat.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;
		d3dSubobjectDSFormat.format = TranslateDepthStencilFormatToDXGIFormat(baseObject.depthStencilFormat);
	}

	D3D12_PIPELINE_STATE_STREAM_DESC d3dPSOStreamDesc = {};
	d3dPSOStreamDesc.SizeInBytes = d3dPSOStreamWriter.getLength();
	d3dPSOStreamDesc.pPipelineStateSubobjectStream = d3dPSOStreamWriter.getData();

	XEAssert(!pipeline.d3dPipelineState);
	HRESULT hResult = d3dDevice->CreatePipelineState(&d3dPSOStreamDesc, IID_PPV_ARGS(&pipeline.d3dPipelineState));
	XEMasterAssert(hResult == S_OK);

	return composePipelineHandle(pipelineIndex);
}

PipelineHandle Device::createComputePipeline(PipelineLayoutHandle pipelineLayoutHandle, ObjectDataView computeShaderObjectData)
{
	using namespace ObjectFormat;

	const Device::PipelineLayout& pipelineLayout = getPipelineLayoutByHandle(pipelineLayoutHandle);
	XEAssert(pipelineLayout.d3dRootSignature);

	const sint32 pipelineIndex = pipelineTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineIndex >= 0);

	Pipeline& pipeline = pipelineTable[pipelineIndex];
	pipeline.pipelineLayoutHandle = pipelineLayoutHandle;
	pipeline.type = PipelineType::Compute;

	const byte* objectDataBytes = (const byte*)computeShaderObjectData.data;

	XEMasterAssert(computeShaderObjectData.size > sizeof(PipelineBytecodeObjectHeader));
	const PipelineBytecodeObjectHeader& header = *(const PipelineBytecodeObjectHeader*)objectDataBytes;

	XEMasterAssert(ValidateGenericObjectHeader(&header.generic, PipelineBytecodeObjectSignature, computeShaderObjectData.size));
	XEMasterAssert(header.pipelineLayoutSourceHash == pipelineLayout.sourceHash);

	D3D12_SHADER_BYTECODE d3dCS = {};
	d3dCS.pShaderBytecode = objectDataBytes + sizeof(PipelineBytecodeObjectHeader);
	d3dCS.BytecodeLength = computeShaderObjectData.size - sizeof(PipelineBytecodeObjectHeader);

	// Compose D3D12 Pipeline State stream

	byte d3dPSOStreamBuffer[64]; // TODO: Proper size
	XLib::ByteStreamWriter d3dPSOStreamWriter(d3dPSOStreamBuffer, sizeof(d3dPSOStreamBuffer));

	{
		D3D12Helpers::PipelineStateSubobjectRootSignature& d3dSubobjectRS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectRootSignature>(sizeof(void*));
		d3dSubobjectRS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
		d3dSubobjectRS.rootSignature = pipelineLayout.d3dRootSignature;
	}

	{
		D3D12Helpers::PipelineStateSubobjectShader& d3dSubobjectCS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectShader>(sizeof(void*));
		d3dSubobjectCS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
		d3dSubobjectCS.bytecode = d3dCS;
	}

	D3D12_PIPELINE_STATE_STREAM_DESC d3dPSOStreamDesc = {};
	d3dPSOStreamDesc.SizeInBytes = d3dPSOStreamWriter.getLength();
	d3dPSOStreamDesc.pPipelineStateSubobjectStream = d3dPSOStreamWriter.getData();

	XEAssert(!pipeline.d3dPipelineState);
	HRESULT hResult = d3dDevice->CreatePipelineState(&d3dPSOStreamDesc, IID_PPV_ARGS(&pipeline.d3dPipelineState));
	XEMasterAssert(hResult == S_OK);

	return composePipelineHandle(pipelineIndex);
}

FenceHandle Device::createFence(uint64 initialValue)
{
	const sint32 fenceIndex = fenceTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(fenceIndex >= 0);

	Fence& fence = fenceTable[fenceIndex];
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
	const sint32 swapChainIndex = swapChainTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(swapChainIndex >= 0);

	SwapChain& swapChain = swapChainTable[swapChainIndex];
	XEAssert(!swapChain.dxgiSwapChain);

	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc = {};
	dxgiSwapChainDesc.Width = width;
	dxgiSwapChainDesc.Height = height;
	dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.Stereo = FALSE;
	dxgiSwapChainDesc.SampleDesc.Count = 1;
	dxgiSwapChainDesc.SampleDesc.Quality = 0;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.BufferCount = SwapChainTextureCount;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	dxgiSwapChainDesc.Flags = 0;

	COMPtr<IDXGISwapChain1> dxgiSwapChain1;
	dxgiFactory->CreateSwapChainForHwnd(d3dGraphicsQueue, HWND(hWnd),
		&dxgiSwapChainDesc, nullptr, nullptr, dxgiSwapChain1.initRef());

	dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&swapChain.dxgiSwapChain));

	// Allocate resources for textures
	for (uint32 i = 0; i < SwapChainTextureCount; i++)
	{
		const sint32 resourceIndex = resourceTableAllocationMask.findFirstZeroAndSet();
		XEMasterAssert(resourceIndex >= 0);

		Resource& resource = resourceTable[resourceIndex];
		XEAssert(resource.type == ResourceType::Undefined);
		XEAssert(!resource.d3dResource);
		resource.type = ResourceType::Texture;
		resource.internalOwnership = true;

		dxgiSwapChain1->GetBuffer(i, IID_PPV_ARGS(&resource.d3dResource));
		
		swapChain.textures[i] = composeResourceHandle(resourceIndex);
	}

	return composeSwapChainHandle(swapChainIndex);
}

void Device::writeDescriptor(DescriptorAddress descriptorAddress, ResourceViewHandle srvHandle)
{
	const uint32 sourceDescriptorIndex = resolveResourceViewHandle(srvHandle);
	const uint32 destDescriptorIndex = resolveDescriptorAddress(descriptorAddress);

	const uint64 sourcePtr = referenceSRVHeapStartPtr + sourceDescriptorIndex * srvDescriptorSize;
	const uint64 destPtr = shaderVisbileSRVHeapStartPtrCPU + destDescriptorIndex * srvDescriptorSize;
	d3dDevice->CopyDescriptorsSimple(1,
		D3D12_CPU_DESCRIPTOR_HANDLE{ destPtr },
		D3D12_CPU_DESCRIPTOR_HANDLE{ sourcePtr },
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Device::submitWorkload(DeviceQueue queue, CommandList& commandList)
{
	XEAssert(commandList.state == CommandList::State::Recording);

	commandList.d3dCommandList->Close();

	commandList.state = CommandList::State::Executing;
	commandList.currentPipelineType = PipelineType::Undefined;
	commandList.currentPipelineLayoutHandle = ZeroPipelineLayoutHandle;

	ID3D12CommandList* d3dCommandListsToExecute[] = { commandList.d3dCommandList };
	d3dGraphicsQueue->ExecuteCommandLists(1, d3dCommandListsToExecute);
}

void Device::submitFenceSignal(DeviceQueue queue, FenceHandle fenceHandle, uint64 value)
{
	d3dGraphicsQueue->Signal(getFenceByHandle(fenceHandle).d3dFence, value);
}

void Device::submitFenceWait(DeviceQueue queue, FenceHandle fenceHandle, uint64 value)
{
	d3dGraphicsQueue->Wait(getFenceByHandle(fenceHandle).d3dFence, value);
}

void Device::submitFlip(SwapChainHandle swapChainHandle)
{
	getSwapChainByHandle(swapChainHandle).dxgiSwapChain->Present(1, 0);
}

uint64 Device::getFenceValue(FenceHandle fenceHandle) const
{
	return getFenceByHandle(fenceHandle).d3dFence->GetCompletedValue();
}

ResourceHandle Device::getSwapChainTexture(SwapChainHandle swapChainHandle, uint32 textureIndex) const
{
	const SwapChain& swapChain = getSwapChainByHandle(swapChainHandle);
	XEAssert(textureIndex < countof(swapChain.textures));
	return swapChain.textures[textureIndex];
}


// Host ////////////////////////////////////////////////////////////////////////////////////////////

void Host::CreateDevice(Device& device)
{
	if (!dxgiFactory.isInitialized())
		CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, dxgiFactory.uuid(), dxgiFactory.voidInitRef());

	if (!d3dDebug.isInitialized())
		D3D12GetDebugInterface(d3dDebug.uuid(), d3dDebug.voidInitRef());

	d3dDebug->EnableDebugLayer();

	COMPtr<IDXGIAdapter4> dxgiAdapter;
	dxgiFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		dxgiAdapter.uuid(), dxgiAdapter.voidInitRef());

	ID3D12Device10* d3dDevice = nullptr;

	// TODO: D3D_FEATURE_LEVEL_12_2
	D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&d3dDevice));

	device.initialize(d3dDevice);

	d3dDevice->Release();
}

uint16x3 Host::CalculateMipLevelSize(uint16x3 srcSize, uint8 mipLevel)
{
	uint16x3 size = srcSize;
	size.x >>= mipLevel;
	size.y >>= mipLevel;
	size.z >>= mipLevel;
	if (size == uint16x3(0, 0, 0))
		return uint16x3(0, 0, 0);

	size.x = max<uint16>(size.x, 1);
	size.y = max<uint16>(size.y, 1);
	size.z = max<uint16>(size.z, 1);
	return size;
}


// Handles /////////////////////////////////////////////////////////////////////////////////////////

// Handle structure:
//	bits  0..19 - entry index
//	bits 20..27 - generation
//	bits 28..32 - signature (should not be zero)

namespace
{
	constexpr uint8 MemoryBlockHandleSignature = 0x1;
	constexpr uint8 ResourceHandleSignature = 0x2;
	constexpr uint8 ResourceViewHandleSignature = 0x3;
	constexpr uint8 RenderTargetViewHandleSignature = 0x4;
	constexpr uint8 DepthStencilViewHandleSignature = 0x5;
	constexpr uint8 PipelineLayoutHandleSignature = 0x6;
	constexpr uint8 PipelineHandleSignature = 0x7;
	constexpr uint8 FenceHandleSignature = 0x8;
	constexpr uint8 SwapChainHandleSignature = 0x9;

	struct DecomposedHandle
	{
		uint8 signature;
		uint8 generation;
		uint32 entryIndex;
	};

	inline uint32 ComposeHandle(uint8 signature, uint8 generation, uint32 entryIndex)
	{
		XEAssert((entryIndex & ~0x0F'FF'FF) == 0);
		return (uint32(signature) << 28) | (uint32(generation) << 20) | entryIndex;
	}

	inline DecomposedHandle DecomposeHandle(uint32 handle)
	{
		DecomposedHandle result;
		result.signature = uint8(handle >> 28);
		result.generation = uint8(handle >> 20);
		result.entryIndex = handle & 0x0F'FF'FF;
		return result;
	}
}

MemoryBlockHandle Device::composeMemoryBlockHandle(uint32 memoryBlockIndex) const
{
	XEAssert(memoryBlockIndex < MaxMemoryBlockCount);
	return MemoryBlockHandle(ComposeHandle(
		MemoryBlockHandleSignature, memoryBlockTable[memoryBlockIndex].handleGeneration, memoryBlockIndex));
}

ResourceHandle Device::composeResourceHandle(uint32 resourceIndex) const
{
	XEAssert(resourceIndex < MaxResourceCount);
	return ResourceHandle(ComposeHandle(
		ResourceHandleSignature, resourceTable[resourceIndex].handleGeneration, resourceIndex));
}

ResourceViewHandle Device::composeResourceViewHandle(uint32 resourceViewIndex) const
{
	XEAssert(resourceViewIndex < MaxResourceViewCount);
	return ResourceViewHandle(ComposeHandle(
		ResourceViewHandleSignature, resourceViewTable[resourceViewIndex].handleGeneration, resourceViewIndex));
}

RenderTargetViewHandle Device::composeRenderTargetViewHandle(uint32 renderTargetViewIndex) const
{
	XEAssert(renderTargetViewIndex < MaxRenderTargetViewCount);
	// renderTargetViewTable[renderTargetViewIndex].handleGeneration;
	return RenderTargetViewHandle(ComposeHandle(RenderTargetViewHandleSignature, 0, renderTargetViewIndex));
}

DepthStencilViewHandle Device::composeDepthStencilViewHandle(uint32 depthStencilViewIndex) const
{
	XEAssert(depthStencilViewIndex < MaxDepthStencilViewCount);
	// depthStencilViewTable[depthStencilViewIndex].handleGeneration;
	return DepthStencilViewHandle(ComposeHandle(DepthStencilViewHandleSignature, 0, depthStencilViewIndex));
}

PipelineLayoutHandle Device::composePipelineLayoutHandle(uint32 pipelineLayoutIndex) const
{
	XEAssert(pipelineLayoutIndex < MaxPipelineLayoutCount);
	return PipelineLayoutHandle(ComposeHandle(
		PipelineLayoutHandleSignature, pipelineLayoutTable[pipelineLayoutIndex].handleGeneration, pipelineLayoutIndex));
}

PipelineHandle Device::composePipelineHandle(uint32 pipelineIndex) const
{
	XEAssert(pipelineIndex < MaxPipelineCount);
	return PipelineHandle(ComposeHandle(
		PipelineHandleSignature, pipelineTable[pipelineIndex].handleGeneration, pipelineIndex));
}

FenceHandle Device::composeFenceHandle(uint32 fenceIndex) const
{
	XEAssert(fenceIndex < MaxFenceCount);
	// fenceTable[fenceIndex].handleGeneration;
	return FenceHandle(ComposeHandle(FenceHandleSignature, 0, fenceIndex));
}

SwapChainHandle Device::composeSwapChainHandle(uint32 swapChainIndex) const
{
	XEAssert(swapChainIndex < MaxSwapChainCount);
	// swapChainTable[swapChainIndex].handleGeneration;
	return SwapChainHandle(ComposeHandle(SwapChainHandleSignature, 0, swapChainIndex));
}


uint32 Device::resolveMemoryBlockHandle(MemoryBlockHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == MemoryBlockHandleSignature);
	XEAssert(decomposed.entryIndex < MaxMemoryBlockCount);
	XEAssert(decomposed.generation == resourceTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}

uint32 Device::resolveResourceHandle(ResourceHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == ResourceHandleSignature);
	XEAssert(decomposed.entryIndex < MaxResourceCount);
	XEAssert(decomposed.generation == resourceTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}

uint32 Device::resolveResourceViewHandle(ResourceViewHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == ResourceViewHandleSignature);
	XEAssert(decomposed.entryIndex < MaxResourceViewCount);
	XEAssert(decomposed.generation == resourceViewTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}

uint32 Device::resolveRenderTargetViewHandle(RenderTargetViewHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == RenderTargetViewHandleSignature);
	XEAssert(decomposed.entryIndex < MaxRenderTargetViewCount);
	//XEAssert(decomposed.generation == renderTargetViewTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}

uint32 Device::resolveDepthStencilViewHandle(DepthStencilViewHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == DepthStencilViewHandleSignature);
	XEAssert(decomposed.entryIndex < MaxDepthStencilViewCount);
	//XEAssert(decomposed.generation == depthStencilViewTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}

uint32 Device::resolvePipelineLayoutHandle(PipelineLayoutHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == PipelineLayoutHandleSignature);
	XEAssert(decomposed.entryIndex < MaxPipelineLayoutCount);
	XEAssert(decomposed.generation == pipelineLayoutTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}

uint32 Device::resolvePipelineHandle(PipelineHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == PipelineHandleSignature);
	XEAssert(decomposed.entryIndex < MaxPipelineCount);
	XEAssert(decomposed.generation == pipelineTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}

uint32 Device::resolveFenceHandle(FenceHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == FenceHandleSignature);
	XEAssert(decomposed.entryIndex < MaxFenceCount);
	//XEAssert(decomposed.generation == fenceTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}

uint32 Device::resolveSwapChainHandle(SwapChainHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == SwapChainHandleSignature);
	XEAssert(decomposed.entryIndex < MaxSwapChainCount);
	//XEAssert(decomposed.generation == swapChainTable[decomposed.entryIndex].handleGeneration);
	return decomposed.entryIndex;
}


auto Device::getMemoryBlockByHandle(MemoryBlockHandle handle) -> MemoryBlock& { return memoryBlockTable[resolveMemoryBlockHandle(handle)]; }
auto Device::getResourceByHandle(ResourceHandle handle) -> Resource& { return resourceTable[resolveResourceHandle(handle)]; }
auto Device::getResourceViewByHandle(ResourceViewHandle handle) -> ResourceView& { return resourceViewTable[resolveResourceViewHandle(handle)]; }
auto Device::getPipelineLayoutByHandle(PipelineLayoutHandle handle) -> PipelineLayout& { return pipelineLayoutTable[resolvePipelineLayoutHandle(handle)]; }
auto Device::getPipelineByHandle(PipelineHandle handle) -> Pipeline& { return pipelineTable[resolvePipelineHandle(handle)]; }
auto Device::getFenceByHandle(FenceHandle handle) -> Fence& { return fenceTable[resolveFenceHandle(handle)]; }
auto Device::getFenceByHandle(FenceHandle handle) const -> const Fence& { return fenceTable[resolveFenceHandle(handle)]; }
auto Device::getSwapChainByHandle(SwapChainHandle handle) -> SwapChain& { return swapChainTable[resolveSwapChainHandle(handle)]; }
auto Device::getSwapChainByHandle(SwapChainHandle handle) const -> const SwapChain& { return swapChainTable[resolveSwapChainHandle(handle)]; }
