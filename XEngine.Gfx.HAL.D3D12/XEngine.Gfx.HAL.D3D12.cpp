#include <d3d12.h>
#include <dxgi1_6.h>
#include "D3D12Helpers.h"

#include <XLib.ByteStream.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Gfx.HAL.BlobFormat.h>

#include "XEngine.Gfx.HAL.D3D12.h"
#include "XEngine.Gfx.HAL.D3D12.Translation.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

using namespace XLib;
using namespace XEngine::Gfx::HAL;

static_assert(ConstantBufferBindAlignment >= D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

static IDXGIFactory7* dxgiFactory = nullptr;
static ID3D12Debug6* d3dDebug = nullptr;

namespace // Barriers validation
{
	inline bool ValidateBarrierAccess(BarrierAccess access, ResourceType resourceType)
	{
		// Check if `BarrierAccess::Any` is exclusive.
		if ((access & BarrierAccess::Any) != BarrierAccess(0))
			return access == BarrierAccess::Any;

		// TODO: Decide if we need to check one-writer-at-a-time policy

		constexpr BarrierAccess bufferCompatibleAccess =
			BarrierAccess::CopySource | BarrierAccess::CopyDest |
			BarrierAccess::GeometryInput | BarrierAccess::ConstantBuffer |
			BarrierAccess::ShaderRead | BarrierAccess::ShaderReadWrite;

		constexpr BarrierAccess textureCompatibleAccess =
			BarrierAccess::CopySource | BarrierAccess::CopyDest |
			BarrierAccess::ShaderRead | BarrierAccess::ShaderReadWrite |
			BarrierAccess::RenderTarget |
			BarrierAccess::DepthStencilRead | BarrierAccess::DepthStencilReadWrite;

		const BarrierAccess compatibleAccess =
			resourceType == ResourceType::Buffer ? bufferCompatibleAccess : textureCompatibleAccess;

		return (access & ~compatibleAccess) == BarrierAccess(0);
	}

	inline bool ValidateBarrierAccessAndTextureLayoutCompatibility(BarrierAccess access, TextureLayout layout)
	{
		// Layout can be `Undefined` if access is `None`.
		const bool isAccessNone = (access == BarrierAccess::None);
		const bool isLayoutUndefined = (layout == TextureLayout::Undefined);
		if (isAccessNone || isLayoutUndefined)
			return isAccessNone;

		BarrierAccess compatibleAccess = BarrierAccess::None;
		switch (layout)
		{
			case TextureLayout::Present:				compatibleAccess = BarrierAccess::None; break;
			case TextureLayout::Common:					compatibleAccess = BarrierAccess::CopySource | BarrierAccess::CopyDest | BarrierAccess::ShaderRead | BarrierAccess::ShaderReadWrite; break;
			case TextureLayout::CopySource:				compatibleAccess = BarrierAccess::CopySource; break;
			case TextureLayout::CopyDest:				compatibleAccess = BarrierAccess::CopyDest; break;
			case TextureLayout::ShaderRead:				compatibleAccess = BarrierAccess::ShaderRead; break;
			case TextureLayout::ShaderReadWrite:		compatibleAccess = BarrierAccess::ShaderReadWrite; break;
			case TextureLayout::RenderTarget:			compatibleAccess = BarrierAccess::RenderTarget; break;
			case TextureLayout::DepthStencilRead:		compatibleAccess = BarrierAccess::DepthStencilRead; break;
			case TextureLayout::DepthStencilReadWrite:	compatibleAccess = BarrierAccess::DepthStencilReadWrite; break;
			default:
				return false;
		}

		// There should be no incompatible access.
		return (access & ~compatibleAccess) == BarrierAccess(0);
	}

	inline bool ValidateBarrierSyncAndAccessCompatibility(BarrierSync sync, BarrierAccess access)
	{
		// Sync can be `None` if and only if access is `None`.
		const bool isSyncNone = (sync == BarrierSync::None);
		const bool isAccessNone = (access == BarrierAccess::None);
		if (isSyncNone || isAccessNone)
			return isSyncNone && isAccessNone;

		auto has = [](BarrierAccess a, BarrierAccess b) -> bool { return (a & b) != BarrierAccess(0); };

		BarrierSync compatibleSync = BarrierSync(0);
		if (has(access, BarrierAccess::Any))
			compatibleSync |= BarrierSync::All;
		if (has(access, BarrierAccess::CopySource | BarrierAccess::CopySource))
			compatibleSync |= BarrierSync::Copy;
		if (has(access, BarrierAccess::GeometryInput))
			compatibleSync |= BarrierSync::GraphicsGeometryShading;
		if (has(access, BarrierAccess::ConstantBuffer | BarrierAccess::ShaderRead | BarrierAccess::ShaderReadWrite))
			compatibleSync |= BarrierSync::AllShading;
		if (has(access, BarrierAccess::RenderTarget))
			compatibleSync |= BarrierSync::GraphicsRenderTarget;
		if (has(access, BarrierAccess::DepthStencilRead | BarrierAccess::DepthStencilReadWrite))
			compatibleSync |= BarrierSync::GraphicsDepthStencil;
		// TODO: Raytracing and resolve.

		// There should be at least one compatible sync.
		return (sync & compatibleSync) != BarrierSync(0);
	}
}

// Device internal types ///////////////////////////////////////////////////////////////////////////

struct Device::PoolEntryBase
{
	uint16 freelistNextIdx;
	uint8 handleGeneration;
	bool isAllocated;
};

struct Device::CommandAllocator : PoolEntryBase
{
	ID3D12CommandAllocator* d3dCommandAllocator;
	uint32 queueExecutionFinishSignals[DeviceQueueCount];
	uint8 queueExecutionMask;

	uint16 closedUnsubmittedCommandListCount;
	bool hasOpenCommandList;
};

struct Device::CommandList : PoolEntryBase
{
	ID3D12GraphicsCommandList7* d3dCommandList;
};

struct Device::MemoryAllocation : PoolEntryBase
{
	ID3D12Heap* d3dHeap;
};

struct Device::Resource : PoolEntryBase
{
	ID3D12Resource2* d3dResource;
	ResourceType type;
	bool internalOwnership; // For example output back buffer. User can't release it.

	union
	{
		uint64 bufferSize;
		TextureDesc textureDesc;
	};
};

struct Device::DescriptorSetLayout : PoolEntryBase
{
	BlobFormat::DescriptorSetBindingInfo bindings[MaxDescriptorSetBindingCount];
	uint32 sourceHash;
	uint16 bindingCount;
};

struct Device::PipelineLayout : PoolEntryBase
{
	BlobFormat::PipelineBindingInfo bindings[MaxPipelineBindingCount];
	ID3D12RootSignature* d3dRootSignature;
	uint32 sourceHash;
	uint16 bindingCount;
};

struct Device::Pipeline : PoolEntryBase
{
	ID3D12PipelineState* d3dPipelineState;
	PipelineLayoutHandle pipelineLayoutHandle;
	PipelineType type;
};

struct Device::Output : PoolEntryBase
{
	IDXGISwapChain4* dxgiSwapChain;
	TextureHandle backBuffers[OutputBackBufferCount];
};

struct Device::DecomposedDescriptorSetReference
{
	const DescriptorSetLayout& descriptorSetLayout;
	uint32 baseDescriptorIndex;
	uint32 descriptorSetGeneration;
};


// CommandList /////////////////////////////////////////////////////////////////////////////////////

struct CommandList::BindingResolveResult
{
	PipelineBindingType type;
	uint8 d3dRootParameterIndex;

	union
	{
		uint8 inplaceConstantCount;
		uint32 descriptorSetLayoutSourceHash;
	};
};

inline CommandList::BindingResolveResult CommandList::ResolveBindingByNameXSH(
	Device* device, PipelineLayoutHandle pipleineLayoutHandle, uint64 bindingNameXSH)
{
	Device::PipelineLayout& pipelineLayout = device->pipelineLayoutPool.resolveHandle(uint32(pipleineLayoutHandle));
	for (uint16 i = 0; i < pipelineLayout.bindingCount; i++)
	{
		const BlobFormat::PipelineBindingInfo& binding = pipelineLayout.bindings[i];
		if (binding.nameXSH == bindingNameXSH)
		{
			BindingResolveResult result = {};
			result.type = binding.type;
			result.d3dRootParameterIndex = uint8(binding.d3dRootParameterIndex);
			if (binding.type == PipelineBindingType::InplaceConstants)
				result.inplaceConstantCount = binding.inplaceConstantCount;
			if (binding.type == PipelineBindingType::DescriptorSet)
				result.descriptorSetLayoutSourceHash = binding.descriptorSetLayoutSourceHash;
			return result;
		}
	}
	XEAssertUnreachableCode();
	return BindingResolveResult {};
}

void CommandList::cleanup()
{
	device = nullptr;
	d3dCommandList = nullptr;
	deviceCommandListHandle = 0;
	commandAllocatorHandle = CommandAllocatorHandle::Zero;

	type = CommandListType::Undefined;
	isOpen = false;

	currentPipelineLayoutHandle = PipelineLayoutHandle::Zero;
	currentPipelineType = PipelineType::Undefined;
	setColorRenderTargetCount = 0;
	isDepthStencilRenderTargetSet = false;
}

CommandList::~CommandList()
{
	XEMasterAssert(!device && !d3dCommandList && !isOpen); // Command list should be submitted or discarded.
}

void CommandList::setRenderTargets(uint8 colorRenderTargetCount, const ColorRenderTarget* colorRenderTargets,
	const DepthStencilRenderTarget* depthStencilRenderTarget, bool readOnlyDepth, bool readOnlyStencil)
{
	XEAssert(isOpen);
	XEAssert(colorRenderTargetCount < MaxColorRenderTargetCount);
	XEAssert((colorRenderTargetCount > 0) == (colorRenderTargets != nullptr));

	if (colorRenderTargets)
	{
		for (uint8 colorRenderTargetIndex = 0; colorRenderTargetIndex < colorRenderTargetCount; colorRenderTargetIndex++)
		{
			const ColorRenderTarget& rt = colorRenderTargets[colorRenderTargetIndex];

			const Device::Resource& resource = device->resourcePool.resolveHandle(uint32(rt.texture));
			XEAssert(resource.type == ResourceType::Texture && resource.d3dResource);
			XEAssert(resource.textureDesc.dimension == TextureDimension::Texture2D);
			XEAssert(resource.textureDesc.enableRenderTargetUsage);
			// TODO: Assert compatible texture format / texel view format.

			D3D12_RENDER_TARGET_VIEW_DESC d3dRTVDesc = {};
			d3dRTVDesc.Format = TranslateTexelViewFormatToDXGIFormat(rt.format);
			d3dRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			d3dRTVDesc.Texture2D.MipSlice = rt.mipLevel;
			d3dRTVDesc.Texture2D.PlaneSlice = 0;

			XEAssert(rt.arrayIndex == 0); // Not implemented.

			const uint32 descriptorIndex = rtvHeapOffset + colorRenderTargetIndex;
			const uint64 descriptorPtr = device->rtvHeapPtr + descriptorIndex * device->rtvDescriptorSize;
			device->d3dDevice->CreateRenderTargetView(resource.d3dResource, &d3dRTVDesc, D3D12Helpers::CPUDescriptorHandle(descriptorPtr));
		}
	}

	if (depthStencilRenderTarget)
	{
		const DepthStencilRenderTarget& rt = *depthStencilRenderTarget;

		const Device::Resource& resource = device->resourcePool.resolveHandle(uint32(rt.texture));
		XEAssert(resource.type == ResourceType::Texture && resource.d3dResource);
		XEAssert(resource.textureDesc.dimension == TextureDimension::Texture2D);
		XEAssert(resource.textureDesc.enableRenderTargetUsage);
		// TODO: Assert compatible texture format / texel view format.

		const DepthStencilFormat dsFormat = TextureFormatUtils::TranslateToDepthStencilFormat(resource.textureDesc.format);
		XEAssert(dsFormat != DepthStencilFormat::Undefined);

		D3D12_DSV_FLAGS d3dDSVFlags = D3D12_DSV_FLAG_NONE;
		if (readOnlyDepth)
			d3dDSVFlags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
		if (readOnlyStencil) // TODO: Check that texture has stencil.
			d3dDSVFlags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;

		D3D12_DEPTH_STENCIL_VIEW_DESC d3dDSVDesc = {};
		d3dDSVDesc.Format = TranslateDepthStencilFormatToDXGIFormat(dsFormat);
		d3dDSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		d3dDSVDesc.Flags = d3dDSVFlags;
		d3dDSVDesc.Texture2D.MipSlice = rt.mipLevel;

		XEAssert(rt.arrayIndex == 0); // Not implemented.

		const uint32 descriptorIndex = dsvHeapOffset;
		const uint64 descriptorPtr = device->dsvHeapPtr + descriptorIndex * device->dsvDescriptorSize;
		device->d3dDevice->CreateDepthStencilView(resource.d3dResource, &d3dDSVDesc, D3D12Helpers::CPUDescriptorHandle(descriptorPtr));
	}

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRTVDescriptorPtr = {};
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDSVDescriptorPtr = {};

	const uint64 rtvDescriptorPtr = device->rtvHeapPtr + rtvHeapOffset * device->rtvDescriptorSize;
	const uint64 dsvDescriptorPtr = device->dsvHeapPtr + dsvHeapOffset * device->dsvDescriptorSize;
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = D3D12Helpers::CPUDescriptorHandle(rtvDescriptorPtr);
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle = D3D12Helpers::CPUDescriptorHandle(dsvDescriptorPtr);

	d3dCommandList->OMSetRenderTargets(colorRenderTargetCount,
		colorRenderTargets ? &rtvDescriptorHandle : nullptr, TRUE,
		depthStencilRenderTarget ? &dsvDescriptorHandle : nullptr);

	setColorRenderTargetCount = colorRenderTargetCount;
	isDepthStencilRenderTargetSet = depthStencilRenderTarget != nullptr;
}

void CommandList::setViewport(float32 left, float32 top, float32 right, float32 bottom, float32 minDepth, float32 maxDepth)
{
	XEAssert(isOpen);

	const D3D12_VIEWPORT d3dViewport = { left, top, right - left, bottom - top, minDepth, maxDepth };
	d3dCommandList->RSSetViewports(1, &d3dViewport);
}

void CommandList::setScissor(uint32 left, uint32 top, uint32 right, uint32 bottom)
{
	XEAssert(isOpen);

	const D3D12_RECT d3dRect = { LONG(left), LONG(top), LONG(right), LONG(bottom) };
	d3dCommandList->RSSetScissorRects(1, &d3dRect);
}

void CommandList::setPipelineType(PipelineType pipelineType)
{
	XEAssert(isOpen);
	if (currentPipelineType == pipelineType)
		return;

	currentPipelineType = pipelineType;
	currentPipelineLayoutHandle = PipelineLayoutHandle::Zero;
}

void CommandList::setPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle)
{
	XEAssert(isOpen);
	XEAssert(pipelineLayoutHandle != PipelineLayoutHandle::Zero);

	if (currentPipelineLayoutHandle == pipelineLayoutHandle)
		return;

	const Device::PipelineLayout& pipelineLayout = device->pipelineLayoutPool.resolveHandle(uint32(pipelineLayoutHandle));

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
	XEAssert(isOpen);

	const Device::Pipeline& pipeline = device->pipelinePool.resolveHandle(uint32(pipelineHandle));
	XEAssert(pipeline.type == currentPipelineType);
	XEAssert(pipeline.pipelineLayoutHandle == currentPipelineLayoutHandle);

	XEAssert(pipeline.d3dPipelineState);
	d3dCommandList->SetPipelineState(pipeline.d3dPipelineState);
}

void CommandList::bindIndexBuffer(BufferPointer bufferPointer, IndexBufferFormat format, uint32 byteSize)
{
	XEAssert(format == IndexBufferFormat::U16 || format == IndexBufferFormat::U32);
	XEAssert(byteSize > 0);
	XEAssert(isOpen);

	const Device::Resource& resource = device->resourcePool.resolveHandle(uint32(bufferPointer.buffer));
	XEAssert(resource.type == ResourceType::Buffer);
	XEAssert(resource.d3dResource);

	D3D12_INDEX_BUFFER_VIEW d3dIBV = {};
	d3dIBV.BufferLocation = resource.d3dResource->GetGPUVirtualAddress() + bufferPointer.offset;
	d3dIBV.SizeInBytes = byteSize;
	d3dIBV.Format = format == IndexBufferFormat::U16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	d3dCommandList->IASetIndexBuffer(&d3dIBV);
}

void CommandList::bindVertexBuffer(uint8 bufferIndex, BufferPointer bufferPointer, uint16 stride, uint32 byteSize)
{
	XEAssert(stride > 0 && byteSize > 0);
	XEAssert(isOpen);

	const Device::Resource& resource = device->resourcePool.resolveHandle(uint32(bufferPointer.buffer));
	XEAssert(resource.type == ResourceType::Buffer);
	XEAssert(resource.d3dResource);

	D3D12_VERTEX_BUFFER_VIEW d3dVBV = {};
	d3dVBV.BufferLocation = resource.d3dResource->GetGPUVirtualAddress() + bufferPointer.offset;
	d3dVBV.SizeInBytes = byteSize;
	d3dVBV.StrideInBytes = stride;

	d3dCommandList->IASetVertexBuffers(bufferIndex, 1, &d3dVBV);
}

void CommandList::bindConstants(uint64 bindingNameXSH,
	const void* data, uint32 size32bitValues, uint32 offset32bitValues)
{
	XEAssert(isOpen);
	XEAssert(currentPipelineLayoutHandle != PipelineLayoutHandle::Zero);

	const BindingResolveResult resolvedBinding = ResolveBindingByNameXSH(device, currentPipelineLayoutHandle, bindingNameXSH);
	XEAssert(resolvedBinding.type == PipelineBindingType::InplaceConstants);

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRoot32BitConstants(resolvedBinding.d3dRootParameterIndex, size32bitValues, data, offset32bitValues);
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRoot32BitConstants(resolvedBinding.d3dRootParameterIndex, size32bitValues, data, offset32bitValues);
	else
		XEAssertUnreachableCode();
}

void CommandList::bindBuffer(uint64 bindingNameXSH, BufferBindType bindType, BufferPointer bufferPointer)
{
	XEAssert(isOpen);

	const BindingResolveResult resolvedBinding = ResolveBindingByNameXSH(device, currentPipelineLayoutHandle, bindingNameXSH);
	if (bindType == BufferBindType::Constant)
		XEAssert(resolvedBinding.type == PipelineBindingType::ConstantBuffer);
	else if (bindType == BufferBindType::ReadOnly)
		XEAssert(resolvedBinding.type == PipelineBindingType::ReadOnlyBuffer);
	else if (bindType == BufferBindType::ReadWrite)
		XEAssert(resolvedBinding.type == PipelineBindingType::ReadWriteBuffer);
	else
		XEAssertUnreachableCode();

	const Device::Resource& resource = device->resourcePool.resolveHandle(uint32(bufferPointer.buffer));
	XEAssert(resource.type == ResourceType::Buffer);
	XEAssert(resource.d3dResource);

	const uint64 bufferAddress = resource.d3dResource->GetGPUVirtualAddress() + bufferPointer.offset;
	XEAssert(imply(bindType == BufferBindType::Constant, bufferAddress % ConstantBufferBindAlignment == 0));

	if (currentPipelineType == PipelineType::Graphics)
	{
		if (bindType == BufferBindType::Constant)
			d3dCommandList->SetGraphicsRootConstantBufferView(resolvedBinding.d3dRootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadOnly)
			d3dCommandList->SetGraphicsRootShaderResourceView(resolvedBinding.d3dRootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadWrite)
			d3dCommandList->SetGraphicsRootUnorderedAccessView(resolvedBinding.d3dRootParameterIndex, bufferAddress);
	}
	else if (currentPipelineType == PipelineType::Compute)
	{
		if (bindType == BufferBindType::Constant)
			d3dCommandList->SetComputeRootConstantBufferView(resolvedBinding.d3dRootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadOnly)
			d3dCommandList->SetComputeRootShaderResourceView(resolvedBinding.d3dRootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadWrite)
			d3dCommandList->SetComputeRootUnorderedAccessView(resolvedBinding.d3dRootParameterIndex, bufferAddress);
	}
	else
		XEAssertUnreachableCode();
}

void CommandList::bindDescriptorSet(uint64 bindingNameXSH, DescriptorSetReference descriptorSetReference)
{
	XEAssert(isOpen);

	const BindingResolveResult resolvedBinding = ResolveBindingByNameXSH(device, currentPipelineLayoutHandle, bindingNameXSH);
	XEAssert(resolvedBinding.type == PipelineBindingType::DescriptorSet);

	DescriptorSetLayoutHandle descriptorSetLayoutHandle = DescriptorSetLayoutHandle::Zero;
	uint32 baseDescriptorIndex = 0;
	uint8 descriptorSetMagic = 0;
	Device::DecomposeDescriptorSetReference(descriptorSetReference, descriptorSetLayoutHandle, baseDescriptorIndex, descriptorSetMagic);
	const Device::DescriptorSetLayout& descriptorSetLayout = device->descriptorSetLayoutPool.resolveHandle(uint32(descriptorSetLayoutHandle));

	XEAssert(descriptorSetLayout.sourceHash == resolvedBinding.descriptorSetLayoutSourceHash);

	const uint64 descriptorTableGPUPtr = device->shaderVisbileSRVHeapGPUPtr + baseDescriptorIndex * device->srvDescriptorSize;

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRootDescriptorTable(resolvedBinding.d3dRootParameterIndex, D3D12Helpers::GPUDescriptorHandle(descriptorTableGPUPtr));
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRootDescriptorTable(resolvedBinding.d3dRootParameterIndex, D3D12Helpers::GPUDescriptorHandle(descriptorTableGPUPtr));
	else
		XEAssertUnreachableCode();
}

void CommandList::clearColorRenderTarget(uint8 colorRenderTargetIndex, const float32* color)
{
	XEAssert(isOpen);
	XEAssert(colorRenderTargetIndex < MaxColorRenderTargetCount);
	XEAssert(colorRenderTargetIndex < setColorRenderTargetCount);

	const uint32 descriptorIndex = rtvHeapOffset + colorRenderTargetIndex;
	const uint64 descriptorPtr = device->rtvHeapPtr + descriptorIndex * device->rtvDescriptorSize;
	d3dCommandList->ClearRenderTargetView(D3D12Helpers::CPUDescriptorHandle(descriptorPtr), color, 0, nullptr);
}

void CommandList::clearDepthStencilRenderTarget(bool clearDepth, bool clearStencil, float32 depth, uint8 stencil)
{
	XEAssert(isOpen);
	XEAssert(clearDepth || clearStencil);
	XEAssert(isDepthStencilRenderTargetSet);

	const uint32 descriptorIndex = dsvHeapOffset;
	const uint64 descriptorPtr = device->dsvHeapPtr + descriptorIndex * device->dsvDescriptorSize;

	D3D12_CLEAR_FLAGS d3dClearFlags = D3D12_CLEAR_FLAGS(0);
	if (clearDepth)
		d3dClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
	if (clearStencil)
		d3dClearFlags |= D3D12_CLEAR_FLAG_STENCIL;

	d3dCommandList->ClearDepthStencilView(D3D12Helpers::CPUDescriptorHandle(descriptorPtr), d3dClearFlags, depth, stencil, 0, nullptr);
}

void CommandList::draw(uint32 vertexCount, uint32 vertexOffset)
{
	XEAssert(isOpen);
	XEAssert(currentPipelineType == PipelineType::Graphics);
	// TODO: Check that pipeline is actually bound.
	d3dCommandList->DrawInstanced(vertexCount, 1, vertexOffset, 0);
}

void CommandList::drawIndexed(uint32 indexCount, uint32 indexOffset, uint32 vertexOffset)
{
	XEAssert(isOpen);
	XEAssert(currentPipelineType == PipelineType::Graphics);
	// TODO: Check that pipeline is actually bound.
	d3dCommandList->DrawIndexedInstanced(indexCount, 1, indexOffset, vertexOffset, 0);
}

void CommandList::dispatch(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ)
{
	XEAssert(isOpen);
	XEAssert(currentPipelineType == PipelineType::Compute);
	d3dCommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandList::bufferMemoryBarrier(BufferHandle bufferHandle,
	BarrierSync syncBefore, BarrierSync syncAfter,
	BarrierAccess accessBefore, BarrierAccess accessAfter)
{
	XEAssert(isOpen);

	XEAssert(accessBefore != BarrierAccess::None || accessAfter != BarrierAccess::None);
	XEAssert(ValidateBarrierAccess(accessBefore, ResourceType::Buffer));
	XEAssert(ValidateBarrierAccess(accessAfter, ResourceType::Buffer));
	XEAssert(ValidateBarrierSyncAndAccessCompatibility(syncBefore, accessBefore));
	XEAssert(ValidateBarrierSyncAndAccessCompatibility(syncAfter, accessAfter));

	const Device::Resource& buffer = device->resourcePool.resolveHandle(uint32(bufferHandle));
	XEAssert(buffer.type == ResourceType::Buffer && buffer.d3dResource);

	D3D12_BUFFER_BARRIER d3dBufferBarrier = {};
	d3dBufferBarrier.SyncBefore = TranslateBarrierSyncToD3D12BarrierSync(syncBefore);
	d3dBufferBarrier.SyncAfter  = TranslateBarrierSyncToD3D12BarrierSync(syncAfter);
	d3dBufferBarrier.AccessBefore = TranslateBarrierAccessToD3D12BarrierAccess(accessBefore);
	d3dBufferBarrier.AccessAfter  = TranslateBarrierAccessToD3D12BarrierAccess(accessAfter);
	d3dBufferBarrier.pResource = buffer.d3dResource;
	d3dBufferBarrier.Offset = 0; // D3D12 does not allow to apply barrier to part of a buffer :(
	d3dBufferBarrier.Size = UINT64(-1);

	D3D12_BARRIER_GROUP d3dBarrierGroup = {};
	d3dBarrierGroup.Type = D3D12_BARRIER_TYPE_BUFFER;
	d3dBarrierGroup.NumBarriers = 1;
	d3dBarrierGroup.pBufferBarriers = &d3dBufferBarrier;

	d3dCommandList->Barrier(1, &d3dBarrierGroup);
}

void CommandList::textureMemoryBarrier(TextureHandle textureHandle,
	BarrierSync syncBefore, BarrierSync syncAfter,
	BarrierAccess accessBefore, BarrierAccess accessAfter,
	TextureLayout layoutBefore, TextureLayout layoutAfter,
	const TextureSubresourceRange* subresourceRange)
{
	XEAssert(isOpen);

	XEAssert(accessBefore != BarrierAccess::None || accessAfter != BarrierAccess::None);
	XEAssert(ValidateBarrierAccess(accessBefore, ResourceType::Texture));
	XEAssert(ValidateBarrierAccess(accessAfter, ResourceType::Texture));
	XEAssert(ValidateBarrierSyncAndAccessCompatibility(syncBefore, accessBefore));
	XEAssert(ValidateBarrierSyncAndAccessCompatibility(syncAfter, accessAfter));
	XEAssert(ValidateBarrierAccessAndTextureLayoutCompatibility(accessBefore, layoutBefore));
	XEAssert(ValidateBarrierAccessAndTextureLayoutCompatibility(accessAfter, layoutAfter));

	const Device::Resource& texture = device->resourcePool.resolveHandle(uint32(textureHandle));
	XEAssert(texture.type == ResourceType::Texture && texture.d3dResource);

	D3D12_TEXTURE_BARRIER d3dTextureBarrier = {};
	d3dTextureBarrier.SyncBefore = TranslateBarrierSyncToD3D12BarrierSync(syncBefore);
	d3dTextureBarrier.SyncAfter  = TranslateBarrierSyncToD3D12BarrierSync(syncAfter);
	d3dTextureBarrier.AccessBefore = TranslateBarrierAccessToD3D12BarrierAccess(accessBefore);
	d3dTextureBarrier.AccessAfter  = TranslateBarrierAccessToD3D12BarrierAccess(accessAfter);
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
		d3dTextureBarrier.Subresources.FirstArraySlice = subresourceRange->baseArrayIndex;
		d3dTextureBarrier.Subresources.NumArraySlices = subresourceRange->arraySize;
		d3dTextureBarrier.Subresources.FirstPlane = 0;
		d3dTextureBarrier.Subresources.NumPlanes = 1; // TODO: Handle this properly.
	}
	else
	{
		d3dTextureBarrier.Subresources.IndexOrFirstMipLevel = 0;
		d3dTextureBarrier.Subresources.NumMipLevels = texture.textureDesc.mipLevelCount;
		d3dTextureBarrier.Subresources.FirstArraySlice = 0;
		d3dTextureBarrier.Subresources.NumArraySlices =
			(texture.textureDesc.dimension == TextureDimension::Texture2D) ? texture.textureDesc.size.z : 1; // TODO: Other dim arrays
		d3dTextureBarrier.Subresources.FirstPlane = 0;
		d3dTextureBarrier.Subresources.NumPlanes = 1; // TODO: Handle this properly.
	}

	D3D12_BARRIER_GROUP d3dBarrierGroup = {};
	d3dBarrierGroup.Type = D3D12_BARRIER_TYPE_TEXTURE;
	d3dBarrierGroup.NumBarriers = 1;
	d3dBarrierGroup.pTextureBarriers = &d3dTextureBarrier;

	d3dCommandList->Barrier(1, &d3dBarrierGroup);
}

void CommandList::copyBuffer(BufferHandle dstBufferHandle, uint64 dstOffset,
	BufferHandle srcBufferHandle, uint64 srcOffset, uint64 size)
{
	XEAssert(isOpen);

	const Device::Resource& dstBuffer = device->resourcePool.resolveHandle(uint32(dstBufferHandle));
	const Device::Resource& srcBuffer = device->resourcePool.resolveHandle(uint32(srcBufferHandle));
	XEAssert(dstBuffer.type == ResourceType::Buffer && dstBuffer.d3dResource);
	XEAssert(srcBuffer.type == ResourceType::Buffer && srcBuffer.d3dResource);

	d3dCommandList->CopyBufferRegion(dstBuffer.d3dResource, dstOffset, srcBuffer.d3dResource, srcOffset, size);
}

void CommandList::copyTexture(TextureHandle dstTextureHandle, TextureSubresource dstSubresource, uint16x3 dstOffset,
	TextureHandle srcTextureHandle, TextureSubresource srcSubresource, const TextureRegion* srcRegion)
{
	XEAssert(isOpen);

	const Device::Resource& dstTexture = device->resourcePool.resolveHandle(uint32(dstTextureHandle));
	const Device::Resource& srcTexture = device->resourcePool.resolveHandle(uint32(srcTextureHandle));
	XEAssert(dstTexture.type == ResourceType::Texture && dstTexture.d3dResource);
	XEAssert(srcTexture.type == ResourceType::Texture && srcTexture.d3dResource);

	D3D12_TEXTURE_COPY_LOCATION d3dDstLocation = {};
	d3dDstLocation.pResource = dstTexture.d3dResource;
	d3dDstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	d3dDstLocation.SubresourceIndex = Device::CalculateTextureSubresourceIndex(dstTexture.textureDesc, dstSubresource);

	D3D12_TEXTURE_COPY_LOCATION d3dSrcLocation = {};
	d3dSrcLocation.pResource = srcTexture.d3dResource;
	d3dSrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	d3dSrcLocation.SubresourceIndex = Device::CalculateTextureSubresourceIndex(dstTexture.textureDesc, srcSubresource);

	const D3D12_BOX d3dSrcBox = srcRegion ?
		D3D12BoxFromOffsetAndSize(srcRegion->offset, srcRegion->size) :
		D3D12BoxFromOffsetAndSize(uint16x3(0, 0, 0),
			CalculateMipLevelSize(srcTexture.textureDesc.size, srcSubresource.mipLevel));

	// TODO: Add texture subresource bounds validation.

	d3dCommandList->CopyTextureRegion(&d3dDstLocation, dstOffset.x, dstOffset.y, dstOffset.z, &d3dSrcLocation, &d3dSrcBox);
}

void CommandList::copyBufferTexture(CopyBufferTextureDirection direction,
	BufferHandle bufferHandle, uint64 bufferOffset, uint32 bufferRowPitch,
	TextureHandle textureHandle, TextureSubresource textureSubresource, const TextureRegion* textureRegion)
{
	XEAssert(isOpen);

	// TODO: BC formats handling

	const Device::Resource& buffer = device->resourcePool.resolveHandle(uint32(bufferHandle));
	const Device::Resource& texture = device->resourcePool.resolveHandle(uint32(textureHandle));
	XEAssert(buffer.type == ResourceType::Buffer && buffer.d3dResource);
	XEAssert(texture.type == ResourceType::Texture && texture.d3dResource);

	const uint16x3 textureRegionSize = textureRegion ?
		textureRegion->size : CalculateMipLevelSize(texture.textureDesc.size, textureSubresource.mipLevel);

	D3D12_TEXTURE_COPY_LOCATION d3dBufferLocation = {};
	d3dBufferLocation.pResource = buffer.d3dResource;
	d3dBufferLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	d3dBufferLocation.PlacedFootprint.Offset = bufferOffset;
	d3dBufferLocation.PlacedFootprint.Footprint.Format = TranslateTextureFormatToDXGIFormat(texture.textureDesc.format);
	d3dBufferLocation.PlacedFootprint.Footprint.Width = textureRegionSize.x;
	d3dBufferLocation.PlacedFootprint.Footprint.Height = textureRegionSize.y;
	d3dBufferLocation.PlacedFootprint.Footprint.Depth = textureRegionSize.z;
	d3dBufferLocation.PlacedFootprint.Footprint.RowPitch = bufferRowPitch;

	D3D12_TEXTURE_COPY_LOCATION d3dTextureLocation = {};
	d3dTextureLocation.pResource = texture.d3dResource;
	d3dTextureLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	d3dTextureLocation.SubresourceIndex = Device::CalculateTextureSubresourceIndex(texture.textureDesc, textureSubresource);

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
				CalculateMipLevelSize(texture.textureDesc.size, textureSubresource.mipLevel));
		p_d3dSrcBox = &d3dSrcBox;
	}

	d3dCommandList->CopyTextureRegion(&d3dDstLocation, dstOffset.x, dstOffset.y, dstOffset.z,
		&d3dSrcLocation, p_d3dSrcBox);
}


// Device::Pool ////////////////////////////////////////////////////////////////////////////////////

// Handle structure:
//		Entry index			0x ....'FFFF
//		Handle generation	0x ..FF'....
//		Device index(?)		0x .F..'....
//		Signature			0x F...'....

template<> constexpr uint8 Device::Pool<Device::CommandAllocator>			::GetHandleSignature() { return 1; }
template <> constexpr uint8 Device::Pool<Device::CommandList>				::GetHandleSignature() { return 2; }
template <> constexpr uint8 Device::Pool<Device::MemoryAllocation>			::GetHandleSignature() { return 3; }
template <> constexpr uint8 Device::Pool<Device::Resource>					::GetHandleSignature() { return 4; }
template <> constexpr uint8 Device::Pool<Device::DescriptorSetLayout>		::GetHandleSignature() { return 5; }
template <> constexpr uint8 Device::Pool<Device::PipelineLayout>			::GetHandleSignature() { return 6; }
template <> constexpr uint8 Device::Pool<Device::Pipeline>					::GetHandleSignature() { return 7; }
template <> constexpr uint8 Device::Pool<Device::Output>					::GetHandleSignature() { return 8; }

template <typename EntryType>
inline void Device::Pool<EntryType>::initialize(EntryType* buffer, uint16 capacity)
{
	this->buffer = buffer;
	this->capacity = capacity;
	this->committedEntryCount = 0;
	this->freelistHeadIdx = 0;
	this->freelistLength = 0;
}

template <typename EntryType>
inline EntryType& Device::Pool<EntryType>::allocate(uint32& outHandle, uint16* outEntryIndex)
{
	uint16 entryIndex = 0;

	if (freelistLength > 0)
	{
		XEAssert(freelistHeadIdx < committedEntryCount);
		entryIndex = freelistHeadIdx;
		freelistHeadIdx = buffer[freelistHeadIdx].freelistNextIdx;
		freelistLength--;
	}
	else
	{
		XEMasterAssert(committedEntryCount < capacity);
		entryIndex = committedEntryCount;
		committedEntryCount++;
	}

	EntryType& entry = buffer[entryIndex];
	entry.isAllocated = true;

	outHandle = ComposeHandle(entryIndex, entry.handleGeneration);
	if (outEntryIndex)
		*outEntryIndex = entryIndex;

	return entry;
}

template <typename EntryType>
inline void Device::Pool<EntryType>::release(uint32 handle)
{
	const uint16 entryIndex = resolveHandleToEntryIndex(handle);
	EntryType& entry = buffer[entryIndex];
	XAssert(entry.isAllocated);
	entry.isAllocated = false;
	entry.handleGeneration++;
	entry.freelistNextIdx = freelistHeadIdx;

	freelistHeadIdx = entryIndex;
	freelistLength++;
}

template <typename EntryType>
inline uint16 Device::Pool<EntryType>::resolveHandleToEntryIndex(uint32 handle) const
{
	XEAssert(handle);
	const uint8 signature = uint8(handle >> 28);
	const uint8 generation = GetHandleGeneration(handle);
	const uint16 entryIndex = GetHandleEntryIndex(handle);
	XEAssert(signature == GetHandleSignature());
	XEAssert(entryIndex < committedEntryCount);
	XEAssert(generation == buffer[entryIndex].handleGeneration);
	return entryIndex;
}

template <typename EntryType>
inline EntryType& Device::Pool<EntryType>::resolveHandle(uint32 handle, uint16* outEntryIndex)
{
	const uint16 entryIndex = resolveHandleToEntryIndex(handle);
	if (outEntryIndex)
		*outEntryIndex = entryIndex;
	return buffer[entryIndex];
}

template <typename EntryType>
inline const EntryType& Device::Pool<EntryType>::resolveHandle(uint32 handle, uint16* outEntryIndex) const
{
	const uint16 entryIndex = resolveHandleToEntryIndex(handle);
	if (outEntryIndex)
		*outEntryIndex = entryIndex;
	return buffer[entryIndex];
}

template <typename EntryType>
inline uint8 Device::Pool<EntryType>::GetHandleGeneration(uint32 handle) { return uint8(handle >> 16); }

template <typename EntryType>
inline uint16 Device::Pool<EntryType>::GetHandleEntryIndex(uint32 handle) { return uint16(handle); }

template <typename EntryType>
inline uint32 Device::Pool<EntryType>::ComposeHandle(uint16 entryIndex, uint8 handleGeneration)
{
	return (uint32(GetHandleSignature()) << 28) | (uint32(handleGeneration) << 16) | uint32(entryIndex);
}

// Device //////////////////////////////////////////////////////////////////////////////////////////

// DescriptorSetReference structure:
//		Base descriptor index					0x ....'....'...F'FFFF
//		Descriptor set layout index				0x ....'...F'FFF.'....
//		Descriptor set layout handle generation	0x ....'.FF.'....'....
//		Descriptor set magic					0x ...F'F...'....'....
//		Checksum								0x .FF.'....'....'....
//		Signature								0x F...'....'....'....

inline DescriptorSetReference Device::ComposeDescriptorSetReference(DescriptorSetLayoutHandle descriptorSetLayoutHandle,
	uint32 baseDescriptorIndex, uint8 descriptorSetMagic)
{
	const uint8 dslHandleGeneration = Pool<DescriptorSetLayout>::GetHandleGeneration(uint32(descriptorSetLayoutHandle));
	const uint16 dslPoolEntryIndex = Pool<DescriptorSetLayout>::GetHandleEntryIndex(uint32(descriptorSetLayoutHandle));
	const uint8 checksum = 0x69; // TODO: Proper checksum.

	XEAssert((baseDescriptorIndex & ~0x000F'FFFF) == 0);

	uint64 result = 0;
	result |= baseDescriptorIndex;
	result |= uint64(dslPoolEntryIndex) << 20;
	result |= uint64(dslHandleGeneration) << 36;
	result |= uint64(descriptorSetMagic) << 44;
	result |= uint64(checksum) << 52;
	result |= 0xD000'0000'0000'0000;

	return DescriptorSetReference(result);
}

inline void Device::DecomposeDescriptorSetReference(DescriptorSetReference descriptorSetReference,
	DescriptorSetLayoutHandle& outDescriptorSetLayoutHandle, uint32& outBaseDescriptorIndex, uint8& outDescriptorSetMagic)
{
	const uint64 refU64 = uint64(descriptorSetReference);

	const uint32 baseDescriptorIndex = uint32(refU64) & 0x000F'FFFF;
	const uint16 dslPoolEntryIndex = uint16(refU64 >> 20);
	const uint8 dslHandleGeneration = uint8(refU64 >> 36);
	const uint8 descriptorSetMagic = uint8(refU64 >> 44);
	const uint8 checksum = uint8(refU64 >> 52);
	const uint8 signature = uint8(refU64 >> 60);

	const uint8 checksumVerification = 0x69; // TODO: Proper checksum.
	XEAssert(signature == 0xD);
	XEAssert(checksum == checksumVerification);

	outDescriptorSetLayoutHandle = DescriptorSetLayoutHandle(Pool<DescriptorSetLayout>::ComposeHandle(dslPoolEntryIndex, dslHandleGeneration));
	outBaseDescriptorIndex = baseDescriptorIndex;
	outDescriptorSetMagic = descriptorSetMagic;
}


// DeviceQueueSyncPoint structure:
//		Fence counter	0x ....'FFFF'FFFF'FFFF
//		Queue index		0x ...F'....'....'....
//		Checksum		0x .FF.'....'....'....
//		Signature		0x F...'....'....'....

consteval uint8 Device::GetDeviceQueueSyncPointSignalValueBitCount()
{
	return 48;
}

inline DeviceQueueSyncPoint Device::ComposeDeviceQueueSyncPoint(uint8 queueIndex, uint64 signalValue)
{
	static_assert(GetDeviceQueueSyncPointSignalValueBitCount() == 48); // Used lower.
	static_assert(DeviceQueueCount <= 0xF); // Only 4 bits for queue index.
	XAssert(queueIndex < DeviceQueueCount);

	// TODO: This should be disabled in master.
	// TODO: Use something better.
	const uint8 checksum = uint8(signalValue) ^ queueIndex;

	uint64 result = 0;
	result |= signalValue & 0x0000'FFFF'FFFF'FFFF;
	result |= uint64(queueIndex & 0xF) << 48;
	result |= uint64(checksum) << 52;
	result |= 0xA000'0000'0000'0000;

	return DeviceQueueSyncPoint(result);
}

inline void Device::DecomposeDeviceQueueSyncPoint(DeviceQueueSyncPoint syncPoint, uint8& outQueueIndex, uint64& outSignalValue)
{
	static_assert(GetDeviceQueueSyncPointSignalValueBitCount() == 48); // Used lower.

	const uint64 syncPointU64 = uint64(syncPoint);

	const uint8 signature = uint8(syncPointU64 >> 60);
	const uint8 queueIndex = uint8(syncPointU64 >> 48) & 0xF;
	const uint8 checksum = uint8(syncPointU64 >> 52);
	const uint64 signalValue = syncPointU64 & 0x0000'FFFF'FFFF'FFFF;

	const uint8 checksumVerification = uint8(signalValue) ^ queueIndex;
	XAssert(signature == 0xA);
	XAssert(checksum == checksumVerification);
	XAssert(queueIndex < DeviceQueueCount);

	outQueueIndex = queueIndex;
	outSignalValue = signalValue;
}


uint32 Device::CalculateTextureSubresourceIndex(const TextureDesc& textureDesc, const TextureSubresource& textureSubresource)
{
	XEAssert(textureSubresource.mipLevel < textureDesc.mipLevelCount);
	XEAssert(textureSubresource.arrayIndex < textureDesc.size.z);

	const TextureFormat format = textureDesc.format;
	const bool hasStencil = (format == TextureFormat::D24S8 || format == TextureFormat::D32S8);
	const bool isDepthStencilTexture = (format == TextureFormat::D16 || format == TextureFormat::D32 || hasStencil);
	const bool isColorAspect = (textureSubresource.aspect == TextureAspect::Color);
	const bool isStencilAspect = (textureSubresource.aspect == TextureAspect::Stencil);
	XEAssert(isDepthStencilTexture ^ isColorAspect);
	XEAssert(imply(isStencilAspect, hasStencil));

	const uint32 planeIndex = isStencilAspect ? 1 : 0;
	const uint32 mipLevelCount = textureDesc.mipLevelCount;
	const uint32 arraySize = textureDesc.size.z;

	return textureSubresource.mipLevel + (textureSubresource.arrayIndex * mipLevelCount) + (planeIndex * mipLevelCount * arraySize);
}


inline bool Device::isDeviceSignalValueReached(const Queue& queue, uint64 signalValue, uint8 signalValueBitCount) const
{
	const uint64 mask = (uint64(1) << signalValueBitCount) - 1;
	const uint64 halfRange = uint64(-1) << (signalValueBitCount - 1);
	XEAssert((signalValue & ~mask) == 0);

	if ((queue.deviceSignalReachedValue & mask) - signalValue < halfRange)
		return true;
	((Queue&)queue).deviceSignalReachedValue = queue.d3dDeviceSignalFence->GetCompletedValue();
	return (queue.deviceSignalReachedValue & mask) - signalValue < halfRange;
}

void Device::updateCommandAllocatorExecutionStatus(CommandAllocator& commandAllocator)
{
	if (!commandAllocator.queueExecutionMask)
		return;

	for (uint8 queueIndex = 0; queueIndex < DeviceQueueCount; queueIndex++)
	{
		if ((commandAllocator.queueExecutionMask >> queueIndex) & 1)
		{
			constexpr uint8 signalBitCount = sizeof(commandAllocator.queueExecutionFinishSignals[queueIndex]) * 4;
			const bool executionFinished = isDeviceSignalValueReached(
				queues[queueIndex], commandAllocator.queueExecutionFinishSignals[queueIndex], signalBitCount);
			if (!executionFinished)
				return;

			commandAllocator.queueExecutionMask &= ~uint8(1 << queueIndex);
		}
	}

	XEAssert(commandAllocator.queueExecutionMask == 0);
}


void Device::initialize(/*const PhysicalDevice& physicalDevice, */const DeviceSettings& settings)
{
	if (!dxgiFactory)
		CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory));

	if (!d3dDebug)
		D3D12GetDebugInterface(IID_PPV_ARGS(&d3dDebug));

	//d3dDebug->EnableDebugLayer();
	//d3dDebug->SetEnableGPUBasedValidation(TRUE);
	//d3dDebug->SetEnableAutoName(TRUE);

	{
		IDXGIAdapter4* dxgiAdapter = nullptr;
		dxgiFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&dxgiAdapter));

		D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&d3dDevice));
		dxgiAdapter->Release();
	}

	/*{
		ID3D12InfoQueue* d3dInfoQueue = nullptr;
		d3dDevice->QueryInterface(IID_PPV_ARGS(&d3dInfoQueue));

		D3D12_MESSAGE_ID d3dMessagesToHide[] = {
			// NOTE: Disable these warnings temporarily until we come up with a solution for optimized clear value handling.
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
		};

		D3D12_INFO_QUEUE_FILTER d3dInfoQueueFilter = {};
		d3dInfoQueueFilter.DenyList.NumIDs = countof(d3dMessagesToHide);
		d3dInfoQueueFilter.DenyList.pIDList = d3dMessagesToHide;
		d3dInfoQueue->AddStorageFilterEntries(&d3dInfoQueueFilter);

		d3dInfoQueue->Release();
	}*/

	// Init queues.
	{
		ID3D12CommandQueue* d3dGraphicsQueue = nullptr;
		ID3D12CommandQueue* d3dComputeQueue = nullptr;
		ID3D12CommandQueue* d3dCopyQueue = nullptr;
		const D3D12_COMMAND_QUEUE_DESC d3dGraphicsQueueDesc = D3D12Helpers::CommandQueueDesc(D3D12_COMMAND_LIST_TYPE_DIRECT);
		const D3D12_COMMAND_QUEUE_DESC d3dComputeQueueDesc = D3D12Helpers::CommandQueueDesc(D3D12_COMMAND_LIST_TYPE_COMPUTE);
		const D3D12_COMMAND_QUEUE_DESC d3dCopyQueueDesc = D3D12Helpers::CommandQueueDesc(D3D12_COMMAND_LIST_TYPE_COPY);
		d3dDevice->CreateCommandQueue(&d3dGraphicsQueueDesc, IID_PPV_ARGS(&d3dGraphicsQueue));
		d3dDevice->CreateCommandQueue(&d3dComputeQueueDesc, IID_PPV_ARGS(&d3dComputeQueue));
		d3dDevice->CreateCommandQueue(&d3dCopyQueueDesc, IID_PPV_ARGS(&d3dCopyQueue));

		queues[uint8(DeviceQueue::Graphics)].d3dQueue = d3dGraphicsQueue;
		queues[uint8(DeviceQueue::Compute)].d3dQueue = d3dComputeQueue;
		queues[uint8(DeviceQueue::Copy)].d3dQueue = d3dCopyQueue;

		for (uint8 i = 0; i < DeviceQueueCount; i++)
		{
			XAssert(queues[i].d3dQueue);
			d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&queues[i].d3dDeviceSignalFence));
			//d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&queues[i].d3dHostSignalFence));
			queues[i].deviceSignalEmittedValue = 0;
			queues[i].deviceSignalReachedValue = 0;
		}
	}

	// Init descriptor heaps.
	{
		// Each command list has own piece of RTV/DSV heap.
		const uint16 rtvHeapSize = settings.maxCommandListCount * MaxColorRenderTargetCount;
		const uint16 dsvHeapSize = settings.maxCommandListCount * 1;

		const D3D12_DESCRIPTOR_HEAP_DESC d3dShaderVisbileSRVHeapDesc =
			D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				settings.descriptorPoolSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
		const D3D12_DESCRIPTOR_HEAP_DESC d3dRTVHeapDesc =
			D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtvHeapSize);
		const D3D12_DESCRIPTOR_HEAP_DESC d3dDSVHeapDesc =
			D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsvHeapSize);

		d3dDevice->CreateDescriptorHeap(&d3dShaderVisbileSRVHeapDesc, IID_PPV_ARGS(&d3dShaderVisbileSRVHeap));
		d3dDevice->CreateDescriptorHeap(&d3dRTVHeapDesc, IID_PPV_ARGS(&d3dRTVHeap));
		d3dDevice->CreateDescriptorHeap(&d3dDSVHeapDesc, IID_PPV_ARGS(&d3dDSVHeap));

		shaderVisbileSRVHeapCPUPtr = d3dShaderVisbileSRVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
		shaderVisbileSRVHeapGPUPtr = d3dShaderVisbileSRVHeap->GetGPUDescriptorHandleForHeapStart().ptr;
		rtvHeapPtr = d3dRTVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
		dsvHeapPtr = d3dDSVHeap->GetCPUDescriptorHandleForHeapStart().ptr;

		srvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		rtvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		dsvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	}

	// Allocate pools.
	{
		uintptr poolsMemorySizeAccum = 0;

		const uintptr commandAllocatorPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(CommandAllocator) * settings.maxMemoryAllocationCount;

		const uintptr commandListPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(CommandList) * settings.maxCommandAllocatorCount;

		const uintptr memoryAllocationPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(MemoryAllocation) * settings.maxCommandListCount;

		const uintptr resourcePoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Resource) * settings.maxResourceCount;

		const uintptr descriptorSetLayoutPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(DescriptorSetLayout) * settings.maxDescriptorSetLayoutCount;

		const uintptr pipelineLayoutPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(PipelineLayout) * settings.maxPipelineLayoutCount;

		const uintptr pipelinePoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Pipeline) * settings.maxPipelineCount;

		const uintptr outputPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Output) * settings.maxOutputCount;

		const uintptr poolsTotalMemorySize = poolsMemorySizeAccum;

		byte* poolsMemory = (byte*)SystemHeapAllocator::Allocate(poolsTotalMemorySize);
		memorySet(poolsMemory, 0, poolsTotalMemorySize);

		commandAllocatorPool.initialize		((CommandAllocator*)	(poolsMemory + commandAllocatorPoolMemOffset),		settings.maxMemoryAllocationCount);
		commandListPool.initialize			((CommandList*)			(poolsMemory + commandListPoolMemOffset),			settings.maxCommandAllocatorCount);
		memoryAllocationPool.initialize		((MemoryAllocation*)	(poolsMemory + memoryAllocationPoolMemOffset),		settings.maxCommandListCount);
		resourcePool.initialize				((Resource*)			(poolsMemory + resourcePoolMemOffset),				settings.maxResourceCount);
		descriptorSetLayoutPool.initialize	((DescriptorSetLayout*)	(poolsMemory + descriptorSetLayoutPoolMemOffset),	settings.maxDescriptorSetLayoutCount);
		pipelineLayoutPool.initialize		((PipelineLayout*)		(poolsMemory + pipelineLayoutPoolMemOffset),		settings.maxPipelineLayoutCount);
		pipelinePool.initialize				((Pipeline*)			(poolsMemory + pipelinePoolMemOffset),				settings.maxPipelineCount);
		outputPool.initialize				((Output*)				(poolsMemory + outputPoolMemOffset),				settings.maxOutputCount);
	}

	descriptorPoolSize = settings.descriptorPoolSize;

	descriptorSetReferenceMagicCounter = 0;
}

CommandAllocatorHandle Device::createCommandAllocator()
{
	CommandAllocatorHandle commandAllocatorHandle = CommandAllocatorHandle::Zero;
	CommandAllocator& commandAllocator = commandAllocatorPool.allocate((uint32&)commandAllocatorHandle);
	XEAssert(!commandAllocator.d3dCommandAllocator);

	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator.d3dCommandAllocator));
	commandAllocator.closedUnsubmittedCommandListCount = 0;
	commandAllocator.hasOpenCommandList = false;
	memorySet(&commandAllocator.queueExecutionFinishSignals, 0, sizeof(commandAllocator.queueExecutionFinishSignals));
	commandAllocator.queueExecutionMask = 0;

	return commandAllocatorHandle;
}

void Device::destroyCommandAllocator(CommandAllocatorHandle commandAllocatorHandle)
{
	CommandAllocator& commandAllocator = commandAllocatorPool.resolveHandle(uint32(commandAllocatorHandle));
	XEMasterAssert(commandAllocator.closedUnsubmittedCommandListCount == 0);
	XEMasterAssert(commandAllocator.hasOpenCommandList);

	updateCommandAllocatorExecutionStatus(commandAllocator);
	XEMasterAssert(commandAllocator.queueExecutionMask == 0);

	commandAllocator.d3dCommandAllocator->Release();

	commandAllocatorPool.release(uint32(commandAllocatorHandle));
}

DeviceMemoryAllocationHandle Device::allocateDeviceMemory(uint64 size, bool hostVisible)
{
	XEMasterAssert(!hostVisible); // `D3D12_HEAP_TYPE_GPU_UPLOAD` support not implemented yet.

	DeviceMemoryAllocationHandle memoryAllocationHandle = DeviceMemoryAllocationHandle::Zero;
	MemoryAllocation& memoryAllocation = memoryAllocationPool.allocate((uint32&)memoryAllocationHandle);
	XEAssert(!memoryAllocation.d3dHeap);

	D3D12_HEAP_DESC d3dHeapDesc = {};
	d3dHeapDesc.SizeInBytes = size;
	d3dHeapDesc.Properties = D3D12Helpers::HeapPropertiesForHeapType(D3D12_HEAP_TYPE_DEFAULT);
	d3dHeapDesc.Alignment = 0;
	d3dHeapDesc.Flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
	// TODO: Check D3D12_RESOURCE_HEAP_TIER_2 during starup. We need it for `D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES`.
	// TODO: Check if we need `D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS`.

	d3dDevice->CreateHeap(&d3dHeapDesc, IID_PPV_ARGS(&memoryAllocation.d3dHeap));

	return memoryAllocationHandle;
}

ResourceAllocationInfo Device::getTextureAllocationInfo(const TextureDesc& textureDesc) const
{
	XEMasterAssertUnreachableCode();
	return ResourceAllocationInfo{};
}

BufferHandle Device::createBuffer(uint64 size, DeviceMemoryAllocationHandle memoryHandle, uint64 memoryOffset)
{
	// TODO: Check that resource fits into memory.
	// TODO: Check alignment.

	XEAssert(memoryHandle == DeviceMemoryAllocationHandle::Zero && memoryOffset == 0); // Not implemented.

	const MemoryAllocation* memoryAllocation = nullptr;

	BufferHandle resourceHandle = BufferHandle::Zero;
	Resource& resource = resourcePool.allocate((uint32&)resourceHandle);
	XEAssert(resource.type == ResourceType::Undefined && !resource.d3dResource);
	resource.type = ResourceType::Buffer;
	resource.internalOwnership = false;
	resource.bufferSize = size;

	D3D12_RESOURCE_FLAGS d3dResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	const D3D12_RESOURCE_DESC1 d3dResourceDesc = D3D12Helpers::ResourceDesc1ForBuffer(size);

	if (memoryAllocation)
	{
		d3dDevice->CreatePlacedResource2(memoryAllocation->d3dHeap, memoryOffset,
			&d3dResourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, 0, nullptr,
			IID_PPV_ARGS(&resource.d3dResource));
	}
	else
	{
		const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapPropertiesForHeapType(D3D12_HEAP_TYPE_DEFAULT);

		d3dDevice->CreateCommittedResource3(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
			&d3dResourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr,
			IID_PPV_ARGS(&resource.d3dResource));
	}

	return resourceHandle;
}

TextureHandle Device::createTexture(const TextureDesc& desc, TextureLayout initialLayout,
	DeviceMemoryAllocationHandle memoryHandle, uint64 memoryOffset)
{
	// TODO: Check if `mipLevelCount` is not greater than max possible level count for this resource.
	// TODO: Check that resource fits into memory.
	// TODO: Check alignment.

	XEAssert(memoryHandle == DeviceMemoryAllocationHandle::Zero && memoryOffset == 0); // Not implemented.
	XEAssert(desc.dimension == TextureDimension::Texture2D); // Not implemented.

	const bool enableColorRT = desc.enableRenderTargetUsage && TextureFormatUtils::SupportsColorRTUsage(desc.format);
	const bool enableDepthStencilRT = desc.enableRenderTargetUsage && TextureFormatUtils::SupportsDepthStencilRTUsage(desc.format);

	XEAssert(enableColorRT || enableDepthStencilRT); // Format should support at least one of RT usages.
	XEAssert(!(enableColorRT && enableDepthStencilRT)); // Can't support both at once.

	const MemoryAllocation* memoryAllocation = nullptr;

	TextureHandle resourceHandle = TextureHandle::Zero;
	Resource& resource = resourcePool.allocate((uint32&)resourceHandle);
	XEAssert(resource.type == ResourceType::Undefined && !resource.d3dResource);
	resource.type = ResourceType::Texture;
	resource.internalOwnership = false;
	resource.textureDesc = desc;

	const DXGI_FORMAT dxgiFormat = TranslateTextureFormatToDXGIFormat(desc.format);

	D3D12_RESOURCE_FLAGS d3dResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	if (enableColorRT)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (enableDepthStencilRT)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (!enableDepthStencilRT)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_RESOURCE_DESC1 d3dResourceDesc = {};
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	d3dResourceDesc.Width = desc.size.x;
	d3dResourceDesc.Height = desc.size.y;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = desc.mipLevelCount;
	d3dResourceDesc.Format = dxgiFormat;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = d3dResourceFlags;

	XEAssert(initialLayout != TextureLayout::Undefined);
	XEAssert(imply(initialLayout == TextureLayout::RenderTarget, enableColorRT));
	XEAssert(imply(initialLayout == TextureLayout::DepthStencilRead || initialLayout == TextureLayout::DepthStencilReadWrite, enableDepthStencilRT));
	const D3D12_BARRIER_LAYOUT d3dInitialLayout = TranslateTextureLayoutToD3D12BarrierLayout(initialLayout);

	if (memoryAllocation)
	{
		d3dDevice->CreatePlacedResource2(memoryAllocation->d3dHeap, memoryOffset,
			&d3dResourceDesc, d3dInitialLayout, nullptr, 0, nullptr,
			IID_PPV_ARGS(&resource.d3dResource));
	}
	else
	{
		const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapPropertiesForHeapType(D3D12_HEAP_TYPE_DEFAULT);

		d3dDevice->CreateCommittedResource3(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
			&d3dResourceDesc, d3dInitialLayout, nullptr, nullptr, 0, nullptr,
			IID_PPV_ARGS(&resource.d3dResource));
	}

	return resourceHandle;
}

BufferHandle Device::createStagingBuffer(uint64 size, StagingBufferAccessMode accessMode)
{
	BufferHandle resourceHandle = BufferHandle::Zero;
	Resource& resource = resourcePool.allocate((uint32&)resourceHandle);
	XEAssert(resource.type == ResourceType::Undefined && !resource.d3dResource);
	resource.type = ResourceType::Buffer;
	resource.internalOwnership = false;
	resource.bufferSize = size;

	const D3D12_RESOURCE_DESC1 d3dResourceDesc = D3D12Helpers::ResourceDesc1ForBuffer(size);

	D3D12_HEAP_TYPE d3dHeapType = D3D12_HEAP_TYPE(0);

	if (accessMode == StagingBufferAccessMode::DeviceReadHostWrite)
		d3dHeapType = D3D12_HEAP_TYPE_UPLOAD;
	else if (accessMode == StagingBufferAccessMode::DeviceWriteHostRead)
		d3dHeapType = D3D12_HEAP_TYPE_READBACK;
	else
		XEMasterAssertUnreachableCode();

	const D3D12_HEAP_PROPERTIES d3dHeapProperties = D3D12Helpers::HeapPropertiesForHeapType(d3dHeapType);

	d3dDevice->CreateCommittedResource3(&d3dHeapProperties, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));

	return resourceHandle;
}

void Device::destroyBuffer(BufferHandle bufferHandle)
{
	XEMasterAssertUnreachableCode();
}

void Device::destroyTexture(TextureHandle textureHandle)
{
	XEMasterAssertUnreachableCode();
}

DescriptorSetLayoutHandle Device::createDescriptorSetLayout(BlobDataView blob)
{
	DescriptorSetLayoutHandle descriptorSetLayoutHandle = DescriptorSetLayoutHandle::Zero;
	DescriptorSetLayout& descriptorSetLayout = descriptorSetLayoutPool.allocate((uint32&)descriptorSetLayoutHandle);

	BlobFormat::DescriptorSetLayoutBlobReader blobReader;
	blobReader.open(blob.data, blob.size);
	// TODO: Assert on open result.

	const uint16 bindingCount = blobReader.getBindingCount();
	XEMasterAssert(bindingCount > 0 && bindingCount <= MaxDescriptorSetBindingCount);

	for (uint16 i = 0; i < bindingCount; i++)
		descriptorSetLayout.bindings[i] = blobReader.getBinding(i);

	descriptorSetLayout.sourceHash = blobReader.getSourceHash();
	descriptorSetLayout.bindingCount = bindingCount;

	return descriptorSetLayoutHandle;
}

PipelineLayoutHandle Device::createPipelineLayout(BlobDataView blob)
{
	PipelineLayoutHandle pipelineLayoutHandle = PipelineLayoutHandle::Zero;
	PipelineLayout& pipelineLayout = pipelineLayoutPool.allocate((uint32&)pipelineLayoutHandle);
	XEAssert(!pipelineLayout.d3dRootSignature);

	BlobFormat::PipelineLayoutBlobReader blobReader;
	blobReader.open(blob.data, blob.size);
	// TODO: Assert on open result.

	const uint16 bindingCount = blobReader.getPipelineBindingCount();
	XEMasterAssert(bindingCount <= MaxPipelineBindingCount);

	for (uint16 i = 0; i < bindingCount; i++)
	{
		// TODO: Validate binding (root parameter index, binding type etc).
		pipelineLayout.bindings[i] = blobReader.getPipelineBinding(i);
	}

	pipelineLayout.sourceHash = blobReader.getSourceHash();
	pipelineLayout.bindingCount = bindingCount;

	XEAssert(!pipelineLayout.d3dRootSignature);
	HRESULT hResult = d3dDevice->CreateRootSignature(0,
		blobReader.getPlatformData(), blobReader.getPlatformDataSize(),
		IID_PPV_ARGS(&pipelineLayout.d3dRootSignature));
	XEMasterAssert(hResult == S_OK);

	return pipelineLayoutHandle;
}

PipelineHandle Device::createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle, const GraphicsPipelineBlobs& blobs)
{
	const PipelineLayout& pipelineLayout = pipelineLayoutPool.resolveHandle(uint32(pipelineLayoutHandle));
	XEAssert(pipelineLayout.d3dRootSignature);

	PipelineHandle pipelineHandle = PipelineHandle::Zero;
	Pipeline& pipeline = pipelinePool.allocate((uint32&)pipelineHandle);
	XEAssert(!pipeline.d3dPipelineState);
	pipeline.pipelineLayoutHandle = pipelineLayoutHandle;
	pipeline.type = PipelineType::Graphics;

	BlobFormat::GraphicsPipelineStateBlobReader stateBlobReader;
	XEMasterAssert(stateBlobReader.open(blobs.state.data, blobs.state.size));
	XEMasterAssert(stateBlobReader.getPipelineLayoutSourceHash() == pipelineLayout.sourceHash);

	// Process shaders.
	D3D12_SHADER_BYTECODE d3dVS = {};
	D3D12_SHADER_BYTECODE d3dAS = {};
	D3D12_SHADER_BYTECODE d3dMS = {};
	D3D12_SHADER_BYTECODE d3dPS = {};

	if (blobs.vs.data)
	{
		BlobFormat::BytecodeBlobReader bytecodeBlobReader;
		XEMasterAssert(bytecodeBlobReader.open(blobs.vs.data, blobs.vs.size));
		XEMasterAssert(bytecodeBlobReader.getPipelineLayoutSourceHash() == pipelineLayout.sourceHash);
		XEMasterAssert(stateBlobReader.isVSBytecodeBlobRegistered());
		XEMasterAssert(stateBlobReader.getVSBytecodeBlobChecksum() == bytecodeBlobReader.getChecksum());
		d3dVS.pShaderBytecode = bytecodeBlobReader.getBytecodeData();
		d3dVS.BytecodeLength = bytecodeBlobReader.getBytecodeSize();
	}
	if (blobs.as.data)
	{
		BlobFormat::BytecodeBlobReader bytecodeBlobReader;
		XEMasterAssert(bytecodeBlobReader.open(blobs.as.data, blobs.as.size));
		XEMasterAssert(bytecodeBlobReader.getPipelineLayoutSourceHash() == pipelineLayout.sourceHash);
		XEMasterAssert(stateBlobReader.isASBytecodeBlobRegistered());
		XEMasterAssert(stateBlobReader.getASBytecodeBlobChecksum() == bytecodeBlobReader.getChecksum());
		d3dAS.pShaderBytecode = bytecodeBlobReader.getBytecodeData();
		d3dAS.BytecodeLength = bytecodeBlobReader.getBytecodeSize();
	}
	if (blobs.ms.data)
	{
		BlobFormat::BytecodeBlobReader bytecodeBlobReader;
		XEMasterAssert(bytecodeBlobReader.open(blobs.ms.data, blobs.ms.size));
		XEMasterAssert(bytecodeBlobReader.getPipelineLayoutSourceHash() == pipelineLayout.sourceHash);
		XEMasterAssert(stateBlobReader.isMSBytecodeBlobRegistered());
		XEMasterAssert(stateBlobReader.getMSBytecodeBlobChecksum() == bytecodeBlobReader.getChecksum());
		d3dMS.pShaderBytecode = bytecodeBlobReader.getBytecodeData();
		d3dMS.BytecodeLength = bytecodeBlobReader.getBytecodeSize();
	}
	if (blobs.ps.data)
	{
		BlobFormat::BytecodeBlobReader bytecodeBlobReader;
		XEMasterAssert(bytecodeBlobReader.open(blobs.ps.data, blobs.ps.size));
		XEMasterAssert(bytecodeBlobReader.getPipelineLayoutSourceHash() == pipelineLayout.sourceHash);
		XEMasterAssert(stateBlobReader.isPSBytecodeBlobRegistered());
		XEMasterAssert(stateBlobReader.getPSBytecodeBlobChecksum() == bytecodeBlobReader.getChecksum());
		d3dPS.pShaderBytecode = bytecodeBlobReader.getBytecodeData();
		d3dPS.BytecodeLength = bytecodeBlobReader.getBytecodeSize();
	}

	// Verify that all registered bytecode blobs are present.
	if (stateBlobReader.isVSBytecodeBlobRegistered())
		XEMasterAssert(d3dVS.pShaderBytecode);
	if (stateBlobReader.isASBytecodeBlobRegistered())
		XEMasterAssert(d3dAS.pShaderBytecode);
	if (stateBlobReader.isMSBytecodeBlobRegistered())
		XEMasterAssert(d3dMS.pShaderBytecode);
	if (stateBlobReader.isPSBytecodeBlobRegistered())
		XEMasterAssert(d3dPS.pShaderBytecode);

	// Verify enabled shader stages combination.
	if (d3dVS.pShaderBytecode)
		XEMasterAssert(!d3dMS.pShaderBytecode);
	if (d3dAS.pShaderBytecode)
		XEMasterAssert(d3dMS.pShaderBytecode);
	if (d3dMS.pShaderBytecode)
		XEMasterAssert(!d3dVS.pShaderBytecode);

	// Process vertex input bindings (input layout elements).
	D3D12_INPUT_ELEMENT_DESC d3dILElements[MaxVertexBindingCount] = {};
	if (stateBlobReader.getVertexBindingCount() > 0)
	{
		XEMasterAssert(d3dVS.pShaderBytecode);
		XEMasterAssert(stateBlobReader.getVertexBindingCount() < MaxVertexBindingCount);

		for (uint8 i = 0; i < stateBlobReader.getVertexBindingCount(); i++)
		{
			// NOTE: We need all this "inplace" stuff, so 'D3D12_INPUT_ELEMENT_DESC::SemanticName' can point directly into blob data.
			const BlobFormat::VertexBindingInfo* bindingInfo = stateBlobReader.getVertexBindingInplace(i);

			// Check zero terminator to make sure this is valid cstr.
			// All chars after first zero should also be zero, so checking only the last one is ok.
			XEMasterAssert(bindingInfo->nameCStr[countof(bindingInfo->nameCStr) - 1] == '\0');

			XEMasterAssert(bindingInfo->bufferIndex < MaxVertexBufferCount);
			XEMasterAssert(stateBlobReader.isVertexBufferUsed(bindingInfo->bufferIndex));

			const bool perIstance = stateBlobReader.isVertexBufferPerInstance(bindingInfo->bufferIndex);

			d3dILElements[i].SemanticName = bindingInfo->nameCStr;
			d3dILElements[i].SemanticIndex = 0;
			d3dILElements[i].Format = TranslateTexelViewFormatToDXGIFormat(bindingInfo->format);
			d3dILElements[i].InputSlot = bindingInfo->bufferIndex;
			d3dILElements[i].AlignedByteOffset = bindingInfo->offset;
			d3dILElements[i].InputSlotClass = perIstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			d3dILElements[i].InstanceDataStepRate = perIstance ? 1 : 0;
		}
	}

	// Compose D3D12 Pipeline State stream.

	byte d3dPSOStreamBuffer[1024]; // TODO: Proper size
	ByteStreamWriter d3dPSOStreamWriter(d3dPSOStreamBuffer, sizeof(d3dPSOStreamBuffer));

	// Root signagure
	{
		D3D12Helpers::PipelineStateSubobjectRootSignature& d3dSubobjectRS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectRootSignature>(sizeof(void*));
		d3dSubobjectRS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
		d3dSubobjectRS.rootSignature = pipelineLayout.d3dRootSignature;
	}

	// Shaders
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

	// Blend state
	// ...

	// Rasterizer state
	// ...

	// Depth stencil state
	{
		D3D12Helpers::PipelineStateSubobjectDepthStencil& d3dSubobjectDS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectDepthStencil>(sizeof(void*));
		d3dSubobjectDS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL;
		d3dSubobjectDS.desc = {};
		d3dSubobjectDS.desc.DepthEnable = TRUE;
		d3dSubobjectDS.desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		d3dSubobjectDS.desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	}

	// Input layout
	{
		D3D12Helpers::PipelineStateSubobjectInputLayout& d3dSubobjectIL =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectInputLayout>(sizeof(void*));
		d3dSubobjectIL.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT;
		d3dSubobjectIL.desc.pInputElementDescs = d3dILElements;
		d3dSubobjectIL.desc.NumElements = stateBlobReader.getVertexBindingCount();
	}

	// Primitive topology
	{
		D3D12Helpers::PipelineStateSubobjectPrimitiveTopology& d3dSubobjectPT =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectPrimitiveTopology>(sizeof(void*));
		d3dSubobjectPT.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY;
		// TODO: Primitive topologies support.
		d3dSubobjectPT.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}

	// Render target formats
	{
		bool someColorRTEnabled = false;
		for (uint8 i = 0; i < MaxColorRenderTargetCount; i++)
			someColorRTEnabled |= stateBlobReader.getColorRTFormat(i) != TexelViewFormat::Undefined;

		if (someColorRTEnabled)
		{
			D3D12Helpers::PipelineStateSubobjectRenderTargetFormats& d3dSubobjectRTs =
				*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectRenderTargetFormats>(sizeof(void*));
			d3dSubobjectRTs.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
			d3dSubobjectRTs.formats = {};

			for (uint8 i = 0; i < MaxColorRenderTargetCount; i++)
			{
				if (stateBlobReader.getColorRTFormat(i) != TexelViewFormat::Undefined)
				{
					d3dSubobjectRTs.formats.RTFormats[i] = TranslateTexelViewFormatToDXGIFormat(stateBlobReader.getColorRTFormat(i));
					d3dSubobjectRTs.formats.NumRenderTargets = i + 1;
				}
			}
			XEAssert(d3dSubobjectRTs.formats.NumRenderTargets > 0);
		}
	}

	// Depth stencil format
	if (stateBlobReader.getDepthStencilRTFormat() != DepthStencilFormat::Undefined)
	{
		D3D12Helpers::PipelineStateSubobjectDepthStencilFormat& d3dSubobjectDSFormat =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectDepthStencilFormat>(sizeof(void*));
		d3dSubobjectDSFormat.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;
		d3dSubobjectDSFormat.format = TranslateDepthStencilFormatToDXGIFormat(stateBlobReader.getDepthStencilRTFormat());
	}

	D3D12_PIPELINE_STATE_STREAM_DESC d3dPSOStreamDesc = {};
	d3dPSOStreamDesc.SizeInBytes = d3dPSOStreamWriter.getLength();
	d3dPSOStreamDesc.pPipelineStateSubobjectStream = d3dPSOStreamWriter.getData();

	XEAssert(!pipeline.d3dPipelineState);
	HRESULT hResult = d3dDevice->CreatePipelineState(&d3dPSOStreamDesc, IID_PPV_ARGS(&pipeline.d3dPipelineState));
	XEMasterAssert(hResult == S_OK);

	return pipelineHandle;
}

PipelineHandle Device::createComputePipeline(PipelineLayoutHandle pipelineLayoutHandle, BlobDataView csBlob)
{
	const PipelineLayout& pipelineLayout = pipelineLayoutPool.resolveHandle(uint32(pipelineLayoutHandle));
	XEAssert(pipelineLayout.d3dRootSignature);

	PipelineHandle pipelineHandle = PipelineHandle::Zero;
	Pipeline& pipeline = pipelinePool.allocate((uint32&)pipelineHandle);
	XEAssert(!pipeline.d3dPipelineState);
	pipeline.pipelineLayoutHandle = pipelineLayoutHandle;
	pipeline.type = PipelineType::Compute;

	BlobFormat::BytecodeBlobReader bytecodeBlobReader;
	XEMasterAssert(bytecodeBlobReader.open(csBlob.data, csBlob.size));
	XEMasterAssert(bytecodeBlobReader.getType() == ShaderType::Compute);
	XEMasterAssert(bytecodeBlobReader.getPipelineLayoutSourceHash() == pipelineLayout.sourceHash);

	D3D12_SHADER_BYTECODE d3dCS = {};
	d3dCS.pShaderBytecode = bytecodeBlobReader.getBytecodeData();
	d3dCS.BytecodeLength = bytecodeBlobReader.getBytecodeSize();

	// Compose D3D12 Pipeline State stream.

	byte d3dPSOStreamBuffer[64]; // TODO: Proper size
	ByteStreamWriter d3dPSOStreamWriter(d3dPSOStreamBuffer, sizeof(d3dPSOStreamBuffer));

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

	return pipelineHandle;
}

OutputHandle Device::createWindowOutput(uint16 width, uint16 height, void* platformWindowHandle)
{
	OutputHandle outputHandle = OutputHandle::Zero;
	Output& output = outputPool.allocate((uint32&)outputHandle);
	XEAssert(!output.dxgiSwapChain);

	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc = {};
	dxgiSwapChainDesc.Width = width;
	dxgiSwapChainDesc.Height = height;
	dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.Stereo = FALSE;
	dxgiSwapChainDesc.SampleDesc.Count = 1;
	dxgiSwapChainDesc.SampleDesc.Quality = 0;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.BufferCount = OutputBackBufferCount;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	dxgiSwapChainDesc.Flags = 0;

	{
		IDXGISwapChain1* dxgiSwapChain1;
		dxgiFactory->CreateSwapChainForHwnd(queues[0].d3dQueue, HWND(platformWindowHandle),
			&dxgiSwapChainDesc, nullptr, nullptr, &dxgiSwapChain1);
		dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&output.dxgiSwapChain));
		dxgiSwapChain1->Release();
	}

	// Allocate resources for textures
	for (uint32 i = 0; i < OutputBackBufferCount; i++)
	{
		ID3D12Resource2* d3dBackBufferTexture = nullptr;
		output.dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&d3dBackBufferTexture));

		TextureHandle backBufferTextureHandle = TextureHandle::Zero;
		Resource& backBufferTexture = resourcePool.allocate((uint32&)backBufferTextureHandle);
		XEAssert(backBufferTexture.type == ResourceType::Undefined);
		XEAssert(!backBufferTexture.d3dResource);
		backBufferTexture.d3dResource = d3dBackBufferTexture;
		backBufferTexture.type = ResourceType::Texture;
		backBufferTexture.internalOwnership = true;
		backBufferTexture.textureDesc.size = uint16x3(width, height, 1);
		backBufferTexture.textureDesc.dimension = TextureDimension::Texture2D;
		backBufferTexture.textureDesc.format = TextureFormat::R8G8B8A8;
		backBufferTexture.textureDesc.mipLevelCount = 1;
		backBufferTexture.textureDesc.enableRenderTargetUsage = true;

		output.backBuffers[i] = backBufferTextureHandle;
	}

	return outputHandle;
}

DescriptorSetReference Device::createDescriptorSetReference(
	DescriptorSetLayoutHandle descriptorSetLayoutHandle, DescriptorAddress address)
{
	descriptorSetReferenceMagicCounter++;
	return ComposeDescriptorSetReference(descriptorSetLayoutHandle, uint32(address), descriptorSetReferenceMagicCounter);
}

void Device::writeDescriptor(DescriptorAddress descriptorAddress, const ResourceView& resourceView)
{
	const Resource& resource = resourcePool.resolveHandle(resourceView.resourceHandle);
	XEAssert(resource.d3dResource);

	const uint64 descriptorPtr = shaderVisbileSRVHeapCPUPtr + uint32(descriptorAddress) * srvDescriptorSize;
	const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = D3D12Helpers::CPUDescriptorHandle(descriptorPtr);

	XTODO(__FUNCTION__ " not implemented");

	if (resourceView.type == ResourceViewType::Buffer)
	{
		XEAssert(resource.type == ResourceType::Buffer);

		// TODO: Check if format is supported to use with texel buffers.
		const DXGI_FORMAT dxgiFormat = TranslateTexelViewFormatToDXGIFormat(resourceView.buffer.format);

		XEMasterAssertUnreachableCode();
#if 0
		if (resourceView.buffer.writable)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC d3dUAVDesc = {};
			d3dUAVDesc.Format = dxgiFormat;
			d3dUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			d3dUAVDesc.Buffer.FirstElement = 0;
			d3dUAVDesc.Buffer.NumElements = ...;
			d3dUAVDesc.Buffer.StructureByteStride = ...;
			d3dUAVDesc.Buffer.CounterOffsetInBytes = 0;
			d3dUAVDesc.Buffer.Flags = ...;

			d3dDevice->CreateUnorderedAccessView(resource.d3dResource, nullptr, &d3dUAVDesc, descriptorHandle);
		}
		else
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC d3dSRVDesc = {};
			d3dSRVDesc.Format = dxgiFormat;
			d3dSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			d3dSRVDesc.Shader4ComponentMapping = ...;
			d3dSRVDesc.Buffer.FirstElement = 0;
			d3dSRVDesc.Buffer.NumElements = ...;
			d3dSRVDesc.Buffer.StructureByteStride = ...;
			d3dSRVDesc.Buffer.Flags = ...;

			d3dDevice->CreateShaderResourceView(resource.d3dResource, &d3dSRVDesc, descriptorHandle);
		}
#endif
	}
	else if (resourceView.type == ResourceViewType::Texture)
	{
		XEAssert(resource.type == ResourceType::Texture);

		// TODO: Check compatibility with TextureFormat.
		const DXGI_FORMAT dxgiFormat = TranslateTexelViewFormatToDXGIFormat(resourceView.texture.format);

		XEMasterAssertUnreachableCode();
#if 0
		if (resourceView.texture.writable)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC d3dUAVDesc = {};
			d3dUAVDesc.Format = dxgiFormat;
			// ...

			d3dDevice->CreateUnorderedAccessView(resource.d3dResource, nullptr, &d3dUAVDesc, descriptorHandle);
		}
		else
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC d3dSRVDesc = {};
			d3dSRVDesc.Format = dxgiFormat;
			// ...

			d3dDevice->CreateShaderResourceView(resource.d3dResource, &d3dSRVDesc, descriptorHandle);
		}
#endif
	}
	else
		XEAssertUnreachableCode();
}

void Device::writeDescriptor(DescriptorSetReference descriptorSetReference, uint64 bindingNameXSH, const ResourceView& resourceView)
{
	DescriptorSetLayoutHandle descriptorSetLayoutHandle = DescriptorSetLayoutHandle::Zero;
	uint32 baseDescriptorIndex = 0;
	uint8 descriptorSetMagic = 0;
	DecomposeDescriptorSetReference(descriptorSetReference, descriptorSetLayoutHandle, baseDescriptorIndex, descriptorSetMagic);
	const DescriptorSetLayout& descriptorSetLayout = descriptorSetLayoutPool.resolveHandle(uint32(descriptorSetLayoutHandle));

	for (uint16 i = 0; i < descriptorSetLayout.bindingCount; i++)
	{
		const BlobFormat::DescriptorSetBindingInfo& binding = descriptorSetLayout.bindings[i];
		if (binding.nameXSH == bindingNameXSH)
		{
			// TODO: This is very hacky. Properly calculate descriptor index and check resource view type.
			writeDescriptor(DescriptorAddress(baseDescriptorIndex + i), resourceView);
			return;
		}
	}
	XEAssertUnreachableCode();
}

void Device::openCommandList(HAL::CommandList& commandList, CommandAllocatorHandle commandAllocatorHandle, CommandListType type)
{
	XEAssert(type == CommandListType::Graphics); // Not implemented.

	XEMasterAssert(!commandList.device && !commandList.d3dCommandList);
	XEMasterAssert(!commandList.isOpen);

	CommandAllocator& commandAllocator = commandAllocatorPool.resolveHandle(uint32(commandAllocatorHandle));
	XEMasterAssert(commandAllocator.d3dCommandAllocator);
	XEMasterAssert(!commandAllocator.hasOpenCommandList);
	commandAllocator.hasOpenCommandList = true;

	uint32 deviceCommandListHandle = 0;
	uint16 deviceCommandListIndex = 0;
	CommandList& deviceCommandList = commandListPool.allocate(deviceCommandListHandle, &deviceCommandListIndex);

	if (deviceCommandList.d3dCommandList)
	{
		deviceCommandList.d3dCommandList->Reset(commandAllocator.d3dCommandAllocator, nullptr);
	}
	else
	{
		d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.d3dCommandAllocator,
			nullptr, IID_PPV_ARGS(&deviceCommandList.d3dCommandList));
	}

	commandList.device = this;
	commandList.d3dCommandList = deviceCommandList.d3dCommandList;
	commandList.deviceCommandListHandle = deviceCommandListHandle;
	commandList.commandAllocatorHandle = commandAllocatorHandle;
	commandList.rtvHeapOffset = deviceCommandListIndex * MaxColorRenderTargetCount;
	commandList.dsvHeapOffset = deviceCommandListIndex * 1;
	commandList.type = type;
	commandList.isOpen = true;
	commandList.currentPipelineLayoutHandle = PipelineLayoutHandle::Zero;
	commandList.currentPipelineType = PipelineType::Undefined;
	commandList.setColorRenderTargetCount = 0;
	commandList.isDepthStencilRenderTargetSet = false;

	commandList.d3dCommandList->SetDescriptorHeaps(1, &d3dShaderVisbileSRVHeap);
	commandList.d3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Device::closeCommandList(HAL::CommandList& commandList)
{
	XEMasterAssert(commandList.device && commandList.d3dCommandList && commandList.device == this);
	XEMasterAssert(commandList.isOpen);

	commandList.d3dCommandList->Close();

	CommandAllocator& commandAllocator = commandAllocatorPool.resolveHandle(uint32(commandList.commandAllocatorHandle));
	XAssert(commandAllocator.hasOpenCommandList);
	commandAllocator.closedUnsubmittedCommandListCount++;
	commandAllocator.hasOpenCommandList = false;

	commandList.isOpen = false;
}

void Device::discardCommandList(HAL::CommandList& commandList)
{
	XEMasterAssert(commandList.device && commandList.d3dCommandList && commandList.device == this);
	XEMasterAssert(!commandList.isOpen);

	CommandAllocator& commandAllocator = commandAllocatorPool.resolveHandle(uint32(commandList.commandAllocatorHandle));
	XEAssert(commandAllocator.closedUnsubmittedCommandListCount > 0);
	commandAllocator.closedUnsubmittedCommandListCount--;
	commandAllocator.hasOpenCommandList = false;

	commandListPool.release(commandList.deviceCommandListHandle);
	commandList.cleanup();
}

void Device::resetCommandAllocator(CommandAllocatorHandle commandAllocatorHandle)
{
	CommandAllocator& commandAllocator = commandAllocatorPool.resolveHandle(uint32(commandAllocatorHandle));
	XEMasterAssert(!commandAllocator.hasOpenCommandList && !commandAllocator.closedUnsubmittedCommandListCount);

	updateCommandAllocatorExecutionStatus(commandAllocator);
	XEMasterAssert(commandAllocator.queueExecutionMask == 0);

	commandAllocator.d3dCommandAllocator->Reset();
}

void Device::submitCommandList(DeviceQueue deviceQueue, HAL::CommandList& commandList)
{
	XEAssert(deviceQueue == DeviceQueue::Graphics); // Not implemented.
	// `TextureLayout::Commom` <-> `D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON`

	XEMasterAssert(commandList.device && commandList.d3dCommandList && commandList.device == this);
	XEMasterAssert(!commandList.isOpen);

	const uint8 queueIndex = uint8(deviceQueue);
	XEMasterAssert(queueIndex < countof(queues));
	Queue& queue = queues[queueIndex];

	ID3D12CommandList* d3dCommandListsToExecute[] = { commandList.d3dCommandList };
	queue.d3dQueue->ExecuteCommandLists(1, d3dCommandListsToExecute);

	queue.deviceSignalEmittedValue++;
	queue.d3dQueue->Signal(queue.d3dDeviceSignalFence, queue.deviceSignalEmittedValue);

	CommandAllocator& commandAllocator = commandAllocatorPool.resolveHandle(uint32(commandList.commandAllocatorHandle));
	XEAssert(commandAllocator.closedUnsubmittedCommandListCount > 0);
	commandAllocator.closedUnsubmittedCommandListCount--;

	commandAllocator.queueExecutionMask |= 1 << queueIndex;
	commandAllocator.queueExecutionFinishSignals[queueIndex] = uint32(queue.deviceSignalEmittedValue);

	commandListPool.release(commandList.deviceCommandListHandle);
	commandList.cleanup();
}

void Device::submitOutputFlip(DeviceQueue deviceQueue, OutputHandle outputHandle)
{
	XEMasterAssert(deviceQueue == DeviceQueue::Graphics);

	outputPool.resolveHandle(uint32(outputHandle)).dxgiSwapChain->Present(1, 0);

	Queue& queue = queues[uint8(DeviceQueue::Graphics)];
	queue.deviceSignalEmittedValue++;
	queue.d3dQueue->Signal(queue.d3dDeviceSignalFence, queue.deviceSignalEmittedValue);
}

void Device::submitSyncPointWait(DeviceQueue queue, DeviceQueueSyncPoint syncPoint)
{
	// TODO: Assert that syncPoint can not be for same queue.
	XAssertNotImplemented();
}

DeviceQueueSyncPoint Device::getEndOfQueueSyncPoint(DeviceQueue deviceQueue) const
{
	XEAssert(deviceQueue == DeviceQueue::Graphics); // Not implemented.

	const uint8 queueIndex = uint8(deviceQueue);
	XEMasterAssert(queueIndex < countof(queues));
	const Queue& queue = queues[queueIndex];

	return ComposeDeviceQueueSyncPoint(queueIndex, queue.deviceSignalEmittedValue);
}

bool Device::isQueueSyncPointReached(DeviceQueueSyncPoint syncPoint) const
{
	uint8 queueIndex = 0;
	uint64 signalValue = 0;
	DecomposeDeviceQueueSyncPoint(syncPoint, queueIndex, signalValue);

	XEMasterAssert(queueIndex < countof(queues));
	const Queue& queue = queues[queueIndex];

	return isDeviceSignalValueReached(queue, signalValue, GetDeviceQueueSyncPointSignalValueBitCount());
}

void* Device::getMappedBufferPtr(BufferHandle bufferHandle) const
{
	const Resource& buffer = resourcePool.resolveHandle(uint32(bufferHandle));
	XEAssert(buffer.type == ResourceType::Buffer && buffer.d3dResource);

	void* result = nullptr;
	buffer.d3dResource->Map(0, nullptr, &result);
	return result;
}

uint16 Device::getDescriptorSetLayoutDescriptorCount(DescriptorSetLayoutHandle descriptorSetLayoutHandle) const
{
	return descriptorSetLayoutPool.resolveHandle(uint32(descriptorSetLayoutHandle)).bindingCount;
}

TextureHandle Device::getOutputBackBuffer(OutputHandle outputHandle, uint32 backBufferIndex) const
{
	const Output& output = outputPool.resolveHandle(uint32(outputHandle));
	XEAssert(backBufferIndex < countof(output.backBuffers));
	return output.backBuffers[backBufferIndex];
}

uint32 Device::getOutputCurrentBackBufferIndex(OutputHandle outputHandle) const
{
	const Output& output = outputPool.resolveHandle(uint32(outputHandle));
	XEAssert(output.dxgiSwapChain);
	const uint32 backBufferIndex = output.dxgiSwapChain->GetCurrentBackBufferIndex();
	XEAssert(backBufferIndex < countof(output.backBuffers));
	return backBufferIndex;
}

uint16x3 XEngine::Gfx::HAL::CalculateMipLevelSize(uint16x3 srcSize, uint8 mipLevel)
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
