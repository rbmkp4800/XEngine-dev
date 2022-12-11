#define USE_ENHANCED_BARRIERS 0

#include <d3d12.h>
#include <dxgi1_6.h>
#include "D3D12Helpers.h"

#include <XLib.ByteStream.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.HAL.BlobFormat.h>

#include "XEngine.Render.HAL.D3D12.h"

#include "XEngine.Render.HAL.D3D12.Translation.h"

// Microsoft.Direct3D.D3D12 v1.700.10
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_PREVIEW_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

using namespace XLib;
using namespace XLib::Platform;
using namespace XEngine::Render::HAL;
//using namespace XEngine::Render::HAL::Internal;

static_assert(Device::ConstantBufferBindAlignment >= D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

static COMPtr<IDXGIFactory7> dxgiFactory;
static COMPtr<ID3D12Debug3> d3dDebug;

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
			case TextureLayout::Present:						compatibleAccess = BarrierAccess::CopySource | BarrierAccess::ShaderRead; break;
			case TextureLayout::CopySource:						compatibleAccess = BarrierAccess::CopySource; break;
			case TextureLayout::CopyDest:						compatibleAccess = BarrierAccess::CopyDest; break;
			case TextureLayout::ShaderReadAndCopySource:		compatibleAccess = BarrierAccess::ShaderRead | BarrierAccess::CopySource; break;
			case TextureLayout::ShaderReadAndCopySourceDest:	compatibleAccess = BarrierAccess::ShaderRead | BarrierAccess::CopySource | BarrierAccess::CopyDest; break;
			case TextureLayout::ShaderRead:						compatibleAccess = BarrierAccess::ShaderRead; break;
			case TextureLayout::ShaderReadWrite:				compatibleAccess = BarrierAccess::ShaderReadWrite; break;
			case TextureLayout::RenderTarget:					compatibleAccess = BarrierAccess::RenderTarget; break;
			case TextureLayout::DepthStencilRead:				compatibleAccess = BarrierAccess::DepthStencilRead; break;
			case TextureLayout::DepthStencilReadWrite:			compatibleAccess = BarrierAccess::DepthStencilReadWrite; break;
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

#if !USE_ENHANCED_BARRIERS

namespace // Legacy barriers utils
{
	void ApplyLegacyD3D12Barrier(ID3D12GraphicsCommandList* d3dCommandList,
		ID3D12Resource* d3dResource, UINT subresourceIndex,
		BarrierAccess accessBefore, BarrierAccess accessAfter,
		TextureLayout layoutBefore = TextureLayout::Undefined, TextureLayout layoutAfter = TextureLayout::Undefined)
	{
		const bool isLayoutBeforeUndefined = (layoutBefore == TextureLayout::Undefined);
		const bool isLayoutAfterUndefined = (layoutAfter == TextureLayout::Undefined);

		if (isLayoutBeforeUndefined || isLayoutAfterUndefined)
		{
			// Resource aliasing barrier.
			D3D12_RESOURCE_BARRIER d3dResourceBarrier = {};
			d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			d3dResourceBarrier.Aliasing.pResourceBefore = isLayoutBeforeUndefined ? nullptr : d3dResource;
			d3dResourceBarrier.Aliasing.pResourceAfter = isLayoutAfterUndefined ? nullptr : d3dResource;
			d3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
		}

		{
			// Resource transition barrier.
			D3D12_RESOURCE_BARRIER d3dResourceBarrier = {};
			d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			d3dResourceBarrier.Transition.pResource = d3dResource;
			d3dResourceBarrier.Transition.Subresource = subresourceIndex;
			d3dResourceBarrier.Transition.StateBefore = TranslateBarrierAccessToD3D12ResourceStates(accessBefore);
			d3dResourceBarrier.Transition.StateAfter = TranslateBarrierAccessToD3D12ResourceStates(accessAfter);
			d3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
		}

		constexpr BarrierAccess uavBarrierAccessMask = BarrierAccess::ShaderReadWrite |
			BarrierAccess::RaytracingAccelerationStructureRead | BarrierAccess::RaytracingAccelerationStructureWrite;

		if ((accessBefore & uavBarrierAccessMask) != BarrierAccess(0) &&
			(accessAfter & uavBarrierAccessMask) != BarrierAccess(0))
		{
			// Resource UAV barrier.
			D3D12_RESOURCE_BARRIER d3dResourceBarrier = {};
			d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			d3dResourceBarrier.UAV.pResource = d3dResource;
			d3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
		}
	}
}

#endif

namespace // DeviceQueueSyncPoint
{
	// DeviceQueueSyncPoint structure:
	//	Fence counter	0x ....'XXXX'XXXX'XXXX
	//	Queue ID		0x ...X'....'....'....
	//	Checksum		0x .XX.'....'....'....
	//	Signature		0x X...'....'....'....

	// TODO: Finish implementation: poper queue encoding/decoding, checksums.

	constexpr uint64 DeviceQueueSyncPointSignatureMask			= 0xF000'0000'0000'0000;
	constexpr uint64 DeviceQueueSyncPointFenceCounterValueMask	= 0x0000'FFFF'FFFF'FFFF;
	constexpr uint64 DeviceQueueSyncPointSignature				= 0xA000'0000'0000'0000;

	struct DecomposedDeviceQueueSyncPoint
	{
		DeviceQueue queue;
		uint64 queueFenceCounterValue;
	};

	inline DeviceQueueSyncPoint ComposeDeviceQueueSyncPoint(DeviceQueue queue, uint64 queueFenceCounterValue)
	{
		XEAssert((queueFenceCounterValue & ~DeviceQueueSyncPointFenceCounterValueMask) == 0);
		return DeviceQueueSyncPoint(DeviceQueueSyncPointSignature | queueFenceCounterValue);
	}

	inline DecomposedDeviceQueueSyncPoint DecomposeDeviceQueueSyncPoint(DeviceQueueSyncPoint syncPoint)
	{
		XEAssert((uint64(syncPoint) & DeviceQueueSyncPointSignatureMask) == DeviceQueueSyncPointSignature);
		DecomposedDeviceQueueSyncPoint result = {};
		result.queue = DeviceQueue::Main;
		result.queueFenceCounterValue = uint64(syncPoint) & DeviceQueueSyncPointFenceCounterValueMask;
		return result;
	}

	inline uint64 GetDeviceQueueSyncPointFenceNextCounterValue(uint64 currentValue)
	{
		XEAssert((currentValue & ~DeviceQueueSyncPointFenceCounterValueMask) == 0);
		return (currentValue + 1) & DeviceQueueSyncPointFenceCounterValueMask;
	}

	inline bool IsDeviceQueueSyncPointFenceCounterValueReached(uint64 currentValue, uint64 targetValue)
	{
		XEAssert((currentValue & ~DeviceQueueSyncPointFenceCounterValueMask) == 0);
		XEAssert((targetValue & ~DeviceQueueSyncPointFenceCounterValueMask) == 0);
		return (currentValue - targetValue) < (DeviceQueueSyncPointFenceCounterValueMask >> 1);
	}
}


struct Device::MemoryBlock
{
	ID3D12Heap* d3dHeap;
	MemoryType type;
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
			uint8 mipLevelCount; // TODO: Move this to `size.z`
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

struct Device::DescriptorSetLayout
{
	BlobFormat::DescriptorSetNestedBindingInfo bindings[MaxDescriptorSetNestedBindingCount];
	uint32 sourceHash;
	uint16 bindingCount;
	uint8 handleGeneration;
};

struct Device::PipelineLayout
{
	BlobFormat::PipelineBindingInfo bindings[MaxPipelineBindingCount];
	ID3D12RootSignature* d3dRootSignature;
	uint32 sourceHash;
	uint16 bindingCount;
	uint8 handleGeneration;
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
	ResourceHandle backBuffers[SwapChainBackBufferCount];
};

struct Device::DecomposedDescriptorSetReference
{
	const DescriptorSetLayout& descriptorSetLayout;
	uint32 baseDescriptorIndex;
	uint32 descriptorSetGeneration;
};


// CommandList /////////////////////////////////////////////////////////////////////////////////////

enum class CommandList::State : uint8
{
	Undefined = 0,
	Idle,
	Recording,
	Executing,
};

struct CommandList::BindingResolveResult
{
	PipelineBindingType type;
	uint8 rootParameterIndex;

	union
	{
		uint8 constantsSize;
		uint32 descriptorSetLayoutSourceHash;
	};
};

inline CommandList::BindingResolveResult CommandList::ResolveBindingByNameXSH(
	Device* device, PipelineLayoutHandle pipleineLayoutHandle, uint64 bindingNameXSH)
{
	Device::PipelineLayout& pipelineLayout = device->getPipelineLayoutByHandle(pipleineLayoutHandle);
	for (uint16 i = 0; i < pipelineLayout.bindingCount; i++)
	{
		const BlobFormat::PipelineBindingInfo& binding = pipelineLayout.bindings[i];
		if (binding.nameXSH == bindingNameXSH)
		{
			BindingResolveResult result = {};
			result.type = binding.type;
			result.rootParameterIndex = uint8(binding.platformBindingIndex);
			if (binding.type == PipelineBindingType::Constants)
				result.constantsSize = binding.constantsSize;
			if (binding.type == PipelineBindingType::DescriptorSet)
				result.descriptorSetLayoutSourceHash = binding.descriptorSetLayoutSourceHash;
			return result;
		}
	}
	XEAssertUnreachableCode();
	return BindingResolveResult {};
}

CommandList::~CommandList()
{
	XEAssertUnreachableCode(); // Not implemented.
}

void CommandList::open()
{
	XEAssert(state == State::Idle || state == State::Executing);

	if (state == State::Executing)
	{
		XEMasterAssert(device->isQueueSyncPointReached(executionEndSyncPoint));
		state = State::Idle;
	}

	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);

	d3dCommandList->SetDescriptorHeaps(1, &device->d3dShaderVisbileSRVHeap);

	// TODO: Primitive topologies support.
	d3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	state = State::Recording;
}

void CommandList::clearRenderTarget(RenderTargetViewHandle rtv, const float32* color)
{
	XEAssert(state == State::Recording);

	const uint32 descriptorIndex = device->resolveRenderTargetViewHandle(rtv);
	const uint64 descriptorPtr = device->rtvHeapPtr + descriptorIndex * device->rtvDescriptorSize;
	d3dCommandList->ClearRenderTargetView({ descriptorPtr }, color, 0, nullptr);
}

void CommandList::clearDepthStencil(DepthStencilViewHandle dsv,
	bool clearDepth, bool clearStencil, float32 depth, uint8 stencil)
{
	XEAssert(state == State::Recording);
	XEAssert(clearDepth || clearStencil);

	const uint32 descriptorIndex = device->resolveDepthStencilViewHandle(dsv);
	const uint64 descriptorPtr = device->dsvHeapPtr + descriptorIndex * device->dsvDescriptorSize;

	D3D12_CLEAR_FLAGS d3dClearFlags = D3D12_CLEAR_FLAGS(0);
	if (clearDepth)
		d3dClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
	if (clearStencil)
		d3dClearFlags |= D3D12_CLEAR_FLAG_STENCIL;

	d3dCommandList->ClearDepthStencilView({ descriptorPtr }, d3dClearFlags, depth, stencil, 0, nullptr);
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
			d3dRTVDescriptorPtrs[i].ptr = device->rtvHeapPtr + descriptorIndex * device->rtvDescriptorSize;
		}
	}

	const bool useDSV = dsv != ZeroDepthStencilViewHandle;
	if (useDSV)
	{
		const uint32 descriptorIndex = device->resolveDepthStencilViewHandle(dsv);
		d3dDSVDescriptorPtr.ptr = device->dsvHeapPtr + uint32(dsv) * device->dsvDescriptorSize;
	}

	d3dCommandList->OMSetRenderTargets(rtvCount,
		useRTVs ? d3dRTVDescriptorPtrs : nullptr, FALSE,
		useDSV ? &d3dDSVDescriptorPtr : nullptr);
}

void CommandList::setViewport(float32 left, float32 top, float32 right, float32 bottom, float32 minDepth, float32 maxDepth)
{
	XEAssert(state == State::Recording);

	const D3D12_VIEWPORT d3dViewport = { left, top, right - left, bottom - top, minDepth, maxDepth };
	d3dCommandList->RSSetViewports(1, &d3dViewport);
}

void CommandList::setScissor(uint32 left, uint32 top, uint32 right, uint32 bottom)
{
	XEAssert(state == State::Recording);

	const D3D12_RECT d3dRect = { LONG(left), LONG(top), LONG(right), LONG(bottom) };
	d3dCommandList->RSSetScissorRects(1, &d3dRect);
}

void CommandList::setPipelineType(PipelineType pipelineType)
{
	XEAssert(state == State::Recording);
	if (currentPipelineType == pipelineType)
		return;

	currentPipelineType = pipelineType;
	currentPipelineLayoutHandle = ZeroPipelineLayoutHandle;
}

void CommandList::setPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle)
{
	XEAssert(state == State::Recording);
	XEAssert(pipelineLayoutHandle != ZeroPipelineLayoutHandle);

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

void CommandList::bindConstants(uint64 bindingNameXSH,
	const void* data, uint32 size32bitValues, uint32 offset32bitValues)
{
	XEAssert(state == State::Recording);
	XEAssert(currentPipelineLayoutHandle != ZeroPipelineLayoutHandle);

	const BindingResolveResult resolvedBinding = ResolveBindingByNameXSH(device, currentPipelineLayoutHandle, bindingNameXSH);
	XEAssert(resolvedBinding.type == PipelineBindingType::Constants);

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRoot32BitConstants(resolvedBinding.rootParameterIndex, size32bitValues, data, offset32bitValues);
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRoot32BitConstants(resolvedBinding.rootParameterIndex, size32bitValues, data, offset32bitValues);
	else
		XEAssertUnreachableCode();
}

void CommandList::bindBuffer(uint64 bindingNameXSH,
	BufferBindType bindType, ResourceHandle bufferHandle, uint32 offset)
{
	XEAssert(state == State::Recording);

	const BindingResolveResult resolvedBinding = ResolveBindingByNameXSH(device, currentPipelineLayoutHandle, bindingNameXSH);
	if (bindType == BufferBindType::Constant)
		XEAssert(resolvedBinding.type == PipelineBindingType::ConstantBuffer);
	else if (bindType == BufferBindType::ReadOnly)
		XEAssert(resolvedBinding.type == PipelineBindingType::ReadOnlyBuffer);
	else if (bindType == BufferBindType::ReadWrite)
		XEAssert(resolvedBinding.type == PipelineBindingType::ReadWriteBuffer);
	else
		XEAssertUnreachableCode();

	const Device::Resource& resource = device->getResourceByHandle(bufferHandle);
	XEAssert(resource.type == ResourceType::Buffer);
	XEAssert(resource.d3dResource);

	const uint64 bufferAddress = resource.d3dResource->GetGPUVirtualAddress() + offset;
	XEAssert(imply(bindType == BufferBindType::Constant, bufferAddress % Device::ConstantBufferBindAlignment == 0));

	if (currentPipelineType == PipelineType::Graphics)
	{
		if (bindType == BufferBindType::Constant)
			d3dCommandList->SetGraphicsRootConstantBufferView(resolvedBinding.rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadOnly)
			d3dCommandList->SetGraphicsRootShaderResourceView(resolvedBinding.rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadWrite)
			d3dCommandList->SetGraphicsRootUnorderedAccessView(resolvedBinding.rootParameterIndex, bufferAddress);
	}
	else if (currentPipelineType == PipelineType::Compute)
	{
		if (bindType == BufferBindType::Constant)
			d3dCommandList->SetComputeRootConstantBufferView(resolvedBinding.rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadOnly)
			d3dCommandList->SetComputeRootShaderResourceView(resolvedBinding.rootParameterIndex, bufferAddress);
		else if (bindType == BufferBindType::ReadWrite)
			d3dCommandList->SetComputeRootUnorderedAccessView(resolvedBinding.rootParameterIndex, bufferAddress);
	}
	else
		XEAssertUnreachableCode();
}

void CommandList::bindDescriptorSet(uint64 bindingNameXSH, DescriptorSetReference descriptorSetReference)
{
	XEAssert(state == State::Recording);

	const BindingResolveResult resolvedBinding = ResolveBindingByNameXSH(device, currentPipelineLayoutHandle, bindingNameXSH);
	XEAssert(resolvedBinding.type == PipelineBindingType::DescriptorSet);

	const Device::DecomposedDescriptorSetReference decomposedDescriptorSetReference = device->decomposeDescriptorSetReference(descriptorSetReference);

	XEAssert(decomposedDescriptorSetReference.descriptorSetLayout.sourceHash == resolvedBinding.descriptorSetLayoutSourceHash);

	const uint64 descriptorTableGPUPtr =
		device->shaderVisbileSRVHeapGPUPtr + decomposedDescriptorSetReference.baseDescriptorIndex * device->srvDescriptorSize;

	if (currentPipelineType == PipelineType::Graphics)
		d3dCommandList->SetGraphicsRootDescriptorTable(resolvedBinding.rootParameterIndex, { descriptorTableGPUPtr });
	else if (currentPipelineType == PipelineType::Compute)
		d3dCommandList->SetComputeRootDescriptorTable(resolvedBinding.rootParameterIndex, { descriptorTableGPUPtr });
	else
		XEAssertUnreachableCode();
}

void CommandList::draw(uint32 vertexCount, uint32 vertexOffset)
{
	XEAssert(state == State::Recording);
	XEAssert(currentPipelineType == PipelineType::Graphics);
	// TODO: Check that pipeline is actually bound.
	d3dCommandList->DrawInstanced(vertexCount, 1, vertexOffset, 0);
}

void CommandList::dispatch(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ)
{
	XEAssert(state == State::Recording);
	XEAssert(currentPipelineType == PipelineType::Compute);
	d3dCommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandList::bufferMemoryBarrier(ResourceHandle bufferHandle,
	BarrierSync syncBefore, BarrierSync syncAfter,
	BarrierAccess accessBefore, BarrierAccess accessAfter)
{
	XEAssert(state == State::Recording);

	XEAssert(accessBefore != BarrierAccess::None || accessAfter != BarrierAccess::None);
	XEAssert(ValidateBarrierAccess(accessBefore, ResourceType::Buffer));
	XEAssert(ValidateBarrierAccess(accessAfter, ResourceType::Buffer));
	XEAssert(ValidateBarrierSyncAndAccessCompatibility(syncBefore, accessBefore));
	XEAssert(ValidateBarrierSyncAndAccessCompatibility(syncAfter, accessAfter));

	const Device::Resource& buffer = device->getResourceByHandle(bufferHandle);
	XEAssert(buffer.type == ResourceType::Buffer && buffer.d3dResource);

#if USE_ENHANCED_BARRIERS

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

#else

	ApplyLegacyD3D12Barrier(d3dCommandList,
		buffer.d3dResource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, accessBefore, accessAfter);

#endif
}

void CommandList::textureMemoryBarrier(ResourceHandle textureHandle,
	BarrierSync syncBefore, BarrierSync syncAfter,
	BarrierAccess accessBefore, BarrierAccess accessAfter,
	TextureLayout layoutBefore, TextureLayout layoutAfter,
	const TextureSubresourceRange* subresourceRange)
{
	XEAssert(state == State::Recording);

	XEAssert(accessBefore != BarrierAccess::None || accessAfter != BarrierAccess::None);
	XEAssert(ValidateBarrierAccess(accessBefore, ResourceType::Texture));
	XEAssert(ValidateBarrierAccess(accessAfter, ResourceType::Texture));
	XEAssert(ValidateBarrierSyncAndAccessCompatibility(syncBefore, accessBefore));
	XEAssert(ValidateBarrierSyncAndAccessCompatibility(syncAfter, accessAfter));
	XEAssert(ValidateBarrierAccessAndTextureLayoutCompatibility(accessBefore, layoutBefore));
	XEAssert(ValidateBarrierAccessAndTextureLayoutCompatibility(accessAfter, layoutAfter));

	const Device::Resource& texture = device->getResourceByHandle(textureHandle);
	XEAssert(texture.type == ResourceType::Texture && texture.d3dResource);

#if USE_ENHANCED_BARRIERS

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

#else

	XEAssert(!subresourceRange); // Not implemented.

	ApplyLegacyD3D12Barrier(d3dCommandList,
		texture.d3dResource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		accessBefore, accessAfter, layoutBefore, layoutAfter);

#endif
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

	d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, d3dGraphicsQueueSyncPointFence.uuid(), d3dGraphicsQueueSyncPointFence.voidInitRef());
	graphicsQueueSyncPointFenceValue = 0;

	// Allocate object tables.
	{
		// TODO: Take into account alignments.
		constexpr uintptr objectTablesMemorySize =
			sizeof(MemoryBlock) * MaxMemoryBlockCount +
			sizeof(Resource) * MaxResourceCount +
			sizeof(ResourceView) * MaxResourceViewCount +
			sizeof(DescriptorSetLayout) * MaxDescriptorSetLayoutCount +
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
		descriptorSetLayoutTable = (DescriptorSetLayout*)(resourceViewTable + MaxResourceViewCount);
		pipelineLayoutTable = (PipelineLayout*)(descriptorSetLayoutTable + MaxDescriptorSetLayoutCount);
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
	descriptorSetLayoutTableAllocationMask.clear();
	pipelineLayoutTableAllocationMask.clear();
	pipelineTableAllocationMask.clear();
	fenceTableAllocationMask.clear();
	swapChainTableAllocationMask.clear();

	referenceSRVHeapPtr = d3dReferenceSRVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	shaderVisbileSRVHeapCPUPtr = d3dShaderVisbileSRVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	shaderVisbileSRVHeapGPUPtr = d3dShaderVisbileSRVHeap->GetGPUDescriptorHandleForHeapStart().ptr;
	rtvHeapPtr = d3dRTVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	dsvHeapPtr = d3dDSVHeap->GetCPUDescriptorHandleForHeapStart().ptr;

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

	memoryBlock.type = memoryType;

	D3D12_HEAP_DESC d3dHeapDesc = {};
	d3dHeapDesc.SizeInBytes = size;
	d3dHeapDesc.Properties = D3D12Helpers::HeapProperties(TranslateMemoryTypeToD3D12HeapType(memoryType));
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
	if ((flags & BufferFlags::AllowShaderWrite) != BufferFlags(0))
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	const D3D12_RESOURCE_DESC1 d3dResourceDesc = D3D12Helpers::ResourceDesc1ForBuffer(size);

	// TODO: Check that resource fits into memory block.
	// TODO: Check alignment.

#if USE_ENHANCED_BARRIERS
	d3dDevice->CreatePlacedResource2(memoryBlock.d3dHeap, memoryBlockOffset,
		&d3dResourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, 0, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));
#else

	D3D12_RESOURCE_STATES d3dInitialState = D3D12_RESOURCE_STATE_COMMON;
	if (memoryBlock.type == MemoryType::DeviceLocal)
		d3dInitialState = D3D12_RESOURCE_STATE_COMMON;
	else if (memoryBlock.type == MemoryType::DeviceReadHostWrite)
		d3dInitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	else if (memoryBlock.type == MemoryType::DeviceWriteHostRead)
		d3dInitialState = D3D12_RESOURCE_STATE_COPY_DEST;
	else
		XEAssertUnreachableCode();

	d3dDevice->CreatePlacedResource1(memoryBlock.d3dHeap, memoryBlockOffset,
		&d3dResourceDesc, d3dInitialState, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));
#endif

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
	if ((flags & TextureFlags::AllowRenderTarget) != TextureFlags(0))
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if ((flags & TextureFlags::AllowDepthStencil) != TextureFlags(0))
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if ((flags & TextureFlags::AllowShaderWrite) != TextureFlags(0))
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	XEAssert(dimension == TextureDimension::Texture2D); // Not implemented.

	// TODO: Check if `mipLevelCount` is not greater than max possible level count for this resource.
	const D3D12_RESOURCE_DESC1 d3dResourceDesc =
		D3D12Helpers::ResourceDesc1ForTexture2D(size.x, size.y, mipLevelCount, dxgiFormat, d3dResourceFlags);

	// TODO: Check that resource fits into memory block.
	// TODO: Check alignment.
	// TODO: Handle initial layout properly.

#if USE_ENHANCED_BARRIERS
	d3dDevice->CreatePlacedResource2(memoryBlock.d3dHeap, memoryBlockOffset,
		&d3dResourceDesc, D3D12_BARRIER_LAYOUT_COMMON, nullptr, 0, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));
#else
	d3dDevice->CreatePlacedResource1(memoryBlock.d3dHeap, memoryBlockOffset,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr,
		IID_PPV_ARGS(&resource.d3dResource));
#endif

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

	const uint64 descriptorPtr = rtvHeapPtr + rtvDescriptorSize * viewIndex;

	XEAssert(useSRV ^ useUAV);
	if (useUAV)
		d3dDevice->CreateShaderResourceView(resource.d3dResource, &d3dSRVDesc, { descriptorPtr });
	if (useSRV)
		d3dDevice->CreateUnorderedAccessView(resource.d3dResource, nullptr, &d3dUAVDesc, { descriptorPtr });

	return composeResourceViewHandle(viewIndex);
}

RenderTargetViewHandle Device::createRenderTargetView(ResourceHandle textureHandle)
{
	const Resource& resource = getResourceByHandle(textureHandle);
	XEAssert(resource.type == ResourceType::Texture);
	XEAssert(resource.d3dResource);

	const sint32 viewIndex = renderTargetViewTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(viewIndex >= 0);

	const uint64 descriptorPtr = rtvHeapPtr + rtvDescriptorSize * viewIndex;
	d3dDevice->CreateRenderTargetView(resource.d3dResource, nullptr, { descriptorPtr });

	return composeRenderTargetViewHandle(viewIndex);
}

DepthStencilViewHandle Device::createDepthStencilView(ResourceHandle textureHandle)
{
	const Resource& resource = getResourceByHandle(textureHandle);
	XEAssert(resource.type == ResourceType::Texture);
	XEAssert(resource.d3dResource);

	const sint32 viewIndex = depthStencilViewTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(viewIndex >= 0);

	const uint64 descriptorPtr = dsvHeapPtr + dsvDescriptorSize * viewIndex;
	d3dDevice->CreateDepthStencilView(resource.d3dResource, nullptr, { descriptorPtr });

	return composeDepthStencilViewHandle(viewIndex);
}

DescriptorSetLayoutHandle Device::createDescriptorSetLayout(BlobDataView blob)
{
	const sint32 descriptorSetLayoutIndex = descriptorSetLayoutTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(descriptorSetLayoutIndex >= 0);

	DescriptorSetLayout& descriptorSetLayout = descriptorSetLayoutTable[descriptorSetLayoutIndex];

	BlobFormat::DescriptorSetLayoutBlobReader blobReader;
	blobReader.open(blob.data, blob.size);

	const uint16 bindingCount = uint16(blobReader.getBindingCount()); // TODO: Clean up this.
	XEMasterAssert(bindingCount > 0 && bindingCount <= MaxDescriptorSetNestedBindingCount);

	for (uint16 i = 0; i < bindingCount; i++)
		descriptorSetLayout.bindings[i] = blobReader.getBinding(i);

	descriptorSetLayout.sourceHash = blobReader.getSourceHash();
	descriptorSetLayout.bindingCount = bindingCount;

	return composeDescriptorSetLayoutHandle(descriptorSetLayoutIndex);
}

PipelineLayoutHandle Device::createPipelineLayout(BlobDataView blob)
{
	const sint32 pipelineLayoutIndex = pipelineLayoutTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineLayoutIndex >= 0);

	PipelineLayout& pipelineLayout = pipelineLayoutTable[pipelineLayoutIndex];

	BlobFormat::PipelineLayoutBlobReader blobReader;
	blobReader.open(blob.data, blob.size);

	const uint32 bindingCount = blobReader.getPipelineBindingCount();
	XEMasterAssert(bindingCount > 0 && bindingCount <= MaxPipelineBindingCount);

	for (uint16 i = 0; i < bindingCount; i++)
	{
		// TODO: Validate binding (root parameter index, binding type etc).
		pipelineLayout.bindings[i] = blobReader.getPipelineBinding(i);
	}

	pipelineLayout.sourceHash = blobReader.getSourceHash();
	pipelineLayout.bindingCount = uint16(bindingCount);

	XEAssert(!pipelineLayout.d3dRootSignature);
	HRESULT hResult = d3dDevice->CreateRootSignature(0,
		blobReader.getPlatformData(), blobReader.getPlatformDataSize(),
		IID_PPV_ARGS(&pipelineLayout.d3dRootSignature));
	XEMasterAssert(hResult == S_OK);

	return composePipelineLayoutHandle(pipelineLayoutIndex);
}

PipelineHandle Device::createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle,
	BlobDataView baseBlob, const BlobDataView* bytecodeBlobs, uint32 bytecodeBlobCount)
{
	const Device::PipelineLayout& pipelineLayout = getPipelineLayoutByHandle(pipelineLayoutHandle);
	XEAssert(pipelineLayout.d3dRootSignature);

	const sint32 pipelineIndex = pipelineTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineIndex >= 0);

	Pipeline& pipeline = pipelineTable[pipelineIndex];
	pipeline.pipelineLayoutHandle = pipelineLayoutHandle;
	pipeline.type = PipelineType::Graphics;

	BlobFormat::GraphicsPipelineBaseBlobReader baseBlobReader;
	XEMasterAssert(baseBlobReader.open(baseBlob.data, baseBlob.size));
	XEMasterAssert(baseBlobReader.getPipelineLayoutSourceHash() == pipelineLayout.sourceHash);

	D3D12_SHADER_BYTECODE d3dVS = {};
	D3D12_SHADER_BYTECODE d3dAS = {};
	D3D12_SHADER_BYTECODE d3dMS = {};
	D3D12_SHADER_BYTECODE d3dPS = {};

	for (uint32 i = 0; i < bytecodeBlobCount; i++)
	{
		BlobFormat::BytecodeBlobReader bytecodeBlobReader;
		XEMasterAssert(bytecodeBlobReader.open(bytecodeBlobs[i].data, bytecodeBlobs[i].size));
		XEMasterAssert(bytecodeBlobReader.getPipelineLayoutSourceHash() == pipelineLayout.sourceHash);

		const BlobFormat::BytecodeBlobType blobType = bytecodeBlobReader.getType();
		XEMasterAssert(baseBlobReader.isBytecodeBlobRegistered(blobType));
		XEMasterAssert(baseBlobReader.getBytecodeBlobChecksum(blobType) == bytecodeBlobReader.getChecksum());

		D3D12_SHADER_BYTECODE* d3dBytecodeStorePtr = nullptr;
		switch (blobType)
		{
			case BlobFormat::BytecodeBlobType::VertexShader:		d3dBytecodeStorePtr = &d3dVS; break;
			case BlobFormat::BytecodeBlobType::AmplificationShader:	d3dBytecodeStorePtr = &d3dAS; break;
			case BlobFormat::BytecodeBlobType::MeshShader:			d3dBytecodeStorePtr = &d3dMS; break;
			case BlobFormat::BytecodeBlobType::PixelShader:			d3dBytecodeStorePtr = &d3dPS; break;
		}

		XEMasterAssert(d3dBytecodeStorePtr);
		XEMasterAssert(!d3dBytecodeStorePtr->pShaderBytecode);
		d3dBytecodeStorePtr->pShaderBytecode = bytecodeBlobReader.getBytecodeData();
		d3dBytecodeStorePtr->BytecodeLength = bytecodeBlobReader.getBytecodeSize();
	}

	// Verify that all registered bytecode blobs are present.
	if (baseBlobReader.isBytecodeBlobRegistered(BlobFormat::BytecodeBlobType::VertexShader))
		XEMasterAssert(d3dVS.pShaderBytecode);
	if (baseBlobReader.isBytecodeBlobRegistered(BlobFormat::BytecodeBlobType::AmplificationShader))
		XEMasterAssert(d3dAS.pShaderBytecode);
	if (baseBlobReader.isBytecodeBlobRegistered(BlobFormat::BytecodeBlobType::MeshShader))
		XEMasterAssert(d3dMS.pShaderBytecode);
	if (baseBlobReader.isBytecodeBlobRegistered(BlobFormat::BytecodeBlobType::PixelShader))
		XEMasterAssert(d3dPS.pShaderBytecode);

	// Verify enabled shader stages combination.
	if (d3dVS.pShaderBytecode)
		XEMasterAssert(!d3dMS.pShaderBytecode);
	if (d3dAS.pShaderBytecode)
		XEMasterAssert(d3dMS.pShaderBytecode);
	if (d3dMS.pShaderBytecode)
		XEMasterAssert(!d3dVS.pShaderBytecode);

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

	// Depth stencil
#if 0
	{
		D3D12Helpers::PipelineStateSubobjectDepthStencil& d3dSubobjectDS =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectDepthStencil>(sizeof(void*));
		d3dSubobjectDS.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL;
		d3dSubobjectDS.desc = {};

		D3D12_DEPTH_STENCIL_DESC& d3dDepthStencilDesc = d3dSubobjectDS.desc;
		d3dDepthStencilDesc.DepthEnable = FALSE;
		d3dDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;;
		d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	}
#endif

	// Primitive topology
	{
		D3D12Helpers::PipelineStateSubobjectPrimitiveTopology& d3dSubobjectPT =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectPrimitiveTopology>(sizeof(void*));
		d3dSubobjectPT.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY;
		// TODO: Primitive topologies support.
		d3dSubobjectPT.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}

	// Render target formats
	if (baseBlobReader.getRenderTargetCount() > 0)
	{
		D3D12Helpers::PipelineStateSubobjectRenderTargetFormats& d3dSubobjectRTs =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectRenderTargetFormats>(sizeof(void*));
		d3dSubobjectRTs.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
		d3dSubobjectRTs.formats = {};

		const uint32 renderTargetCount = baseBlobReader.getRenderTargetCount();
		XEMasterAssert(renderTargetCount < MaxRenderTargetCount);

		D3D12_RT_FORMAT_ARRAY& d3dRTFormatArray = d3dSubobjectRTs.formats;
		for (uint32 i = 0; i < renderTargetCount; i++)
			d3dRTFormatArray.RTFormats[i] = TranslateTexelViewFormatToDXGIFormat(baseBlobReader.getRenderTargetFormat(i));
		d3dRTFormatArray.NumRenderTargets = renderTargetCount;
	}

	// Depth stencil format
	if (baseBlobReader.getDepthStencilFormat() != DepthStencilFormat::Undefined)
	{
		D3D12Helpers::PipelineStateSubobjectDepthStencilFormat& d3dSubobjectDSFormat =
			*d3dPSOStreamWriter.advanceAligned<D3D12Helpers::PipelineStateSubobjectDepthStencilFormat>(sizeof(void*));
		d3dSubobjectDSFormat.type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;
		d3dSubobjectDSFormat.format = TranslateDepthStencilFormatToDXGIFormat(baseBlobReader.getDepthStencilFormat());
	}

	D3D12_PIPELINE_STATE_STREAM_DESC d3dPSOStreamDesc = {};
	d3dPSOStreamDesc.SizeInBytes = d3dPSOStreamWriter.getLength();
	d3dPSOStreamDesc.pPipelineStateSubobjectStream = d3dPSOStreamWriter.getData();

	XEAssert(!pipeline.d3dPipelineState);
	HRESULT hResult = d3dDevice->CreatePipelineState(&d3dPSOStreamDesc, IID_PPV_ARGS(&pipeline.d3dPipelineState));
	XEMasterAssert(hResult == S_OK);

	return composePipelineHandle(pipelineIndex);
}

PipelineHandle Device::createComputePipeline(PipelineLayoutHandle pipelineLayoutHandle, BlobDataView computeShaderBlob)
{
	const Device::PipelineLayout& pipelineLayout = getPipelineLayoutByHandle(pipelineLayoutHandle);
	XEAssert(pipelineLayout.d3dRootSignature);

	const sint32 pipelineIndex = pipelineTableAllocationMask.findFirstZeroAndSet();
	XEMasterAssert(pipelineIndex >= 0);

	Pipeline& pipeline = pipelineTable[pipelineIndex];
	pipeline.pipelineLayoutHandle = pipelineLayoutHandle;
	pipeline.type = PipelineType::Compute;

	BlobFormat::BytecodeBlobReader bytecodeBlobReader;
	XEMasterAssert(bytecodeBlobReader.open(computeShaderBlob.data, computeShaderBlob.size));
	XEMasterAssert(bytecodeBlobReader.getType() == BlobFormat::BytecodeBlobType::ComputeShader);
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
	dxgiSwapChainDesc.BufferCount = SwapChainBackBufferCount;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	dxgiSwapChainDesc.Flags = 0;

	COMPtr<IDXGISwapChain1> dxgiSwapChain1;
	dxgiFactory->CreateSwapChainForHwnd(d3dGraphicsQueue, HWND(hWnd),
		&dxgiSwapChainDesc, nullptr, nullptr, dxgiSwapChain1.initRef());

	dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&swapChain.dxgiSwapChain));

	// Allocate resources for textures
	for (uint32 i = 0; i < SwapChainBackBufferCount; i++)
	{
		const sint32 resourceIndex = resourceTableAllocationMask.findFirstZeroAndSet();
		XEMasterAssert(resourceIndex >= 0);

		Resource& resource = resourceTable[resourceIndex];
		XEAssert(resource.type == ResourceType::Undefined);
		XEAssert(!resource.d3dResource);
		resource.type = ResourceType::Texture;
		resource.internalOwnership = true;
		resource.texture.size = uint16x3(width, height, 1);
		resource.texture.dimension = TextureDimension::Texture2D;
		resource.texture.format = TextureFormat::R8G8B8A8;
		resource.texture.mipLevelCount = 1;

		dxgiSwapChain1->GetBuffer(i, IID_PPV_ARGS(&resource.d3dResource));
		
		swapChain.backBuffers[i] = composeResourceHandle(resourceIndex);
	}

	return composeSwapChainHandle(swapChainIndex);
}

void Device::createCommandList(CommandList& commandList, CommandListType type)
{
	XEAssert(type == CommandListType::Graphics); // TODO: ...
	XEAssert(!commandList.d3dCommandList && !commandList.d3dCommandAllocator);

	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandList.d3dCommandAllocator));
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandList.d3dCommandAllocator, nullptr,
		IID_PPV_ARGS(&commandList.d3dCommandList));
	commandList.d3dCommandList->Close();

	commandList.device = this;
	commandList.state = CommandList::State::Idle;
	commandList.executionEndSyncPoint = DeviceQueueSyncPoint::Zero;
}

DescriptorSetReference Device::createDescriptorSetReference(
	DescriptorSetLayoutHandle descriptorSetLayoutHandle, DescriptorAddress address)
{
	descriptorSetReferenceGenerationCounter++;
	return composeDescriptorSetReference(descriptorSetLayoutHandle, uint32(address), descriptorSetReferenceGenerationCounter);
}

void Device::writeDescriptor(DescriptorAddress descriptorAddress, ResourceViewHandle srvHandle)
{
	const uint32 sourceDescriptorIndex = resolveResourceViewHandle(srvHandle);
	const uint32 destDescriptorIndex = uint32(descriptorAddress);

	const uint64 sourcePtr = referenceSRVHeapPtr + sourceDescriptorIndex * srvDescriptorSize;
	const uint64 destPtr = shaderVisbileSRVHeapCPUPtr + destDescriptorIndex * srvDescriptorSize;
	d3dDevice->CopyDescriptorsSimple(1, { destPtr }, { sourcePtr }, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Device::writeDescriptor(DescriptorSetReference descriptorSetReference,
	uint32 bindingNameXSH, ResourceViewHandle resourceViewHandle)
{
	const DecomposedDescriptorSetReference decomposedDescriptorSetReference = decomposeDescriptorSetReference(descriptorSetReference);
	const DescriptorSetLayout& descriptorSetLayout = decomposedDescriptorSetReference.descriptorSetLayout;

	for (uint16 i = 0; i < descriptorSetLayout.bindingCount; i++)
	{
		const BlobFormat::DescriptorSetNestedBindingInfo& binding = descriptorSetLayout.bindings[i];
		if (uint32(binding.nameXSH) == bindingNameXSH)
		{
			// TODO: This is very hacky. Properly calculate descriptor index and check resource view type.
			writeDescriptor(DescriptorAddress(decomposedDescriptorSetReference.baseDescriptorIndex + i), resourceViewHandle);
			return;
		}
	}
	XEAssertUnreachableCode();
}

void Device::submitWorkload(DeviceQueue queue, CommandList& commandList)
{
	XEAssert(queue == DeviceQueue::Main); // Not implemented.
	XEAssert(commandList.state == CommandList::State::Recording);

	commandList.d3dCommandList->Close();

	const uint64 newGraphicsQueueSyncPointFenceValue = GetDeviceQueueSyncPointFenceNextCounterValue(graphicsQueueSyncPointFenceValue);

	commandList.state = CommandList::State::Executing;
	commandList.executionEndSyncPoint = ComposeDeviceQueueSyncPoint(queue, newGraphicsQueueSyncPointFenceValue);
	commandList.currentPipelineType = PipelineType::Undefined;
	commandList.currentPipelineLayoutHandle = ZeroPipelineLayoutHandle;

	ID3D12CommandList* d3dCommandListsToExecute[] = { commandList.d3dCommandList };
	d3dGraphicsQueue->ExecuteCommandLists(1, d3dCommandListsToExecute);

	graphicsQueueSyncPointFenceValue = newGraphicsQueueSyncPointFenceValue;
	d3dGraphicsQueue->Signal(d3dGraphicsQueueSyncPointFence, graphicsQueueSyncPointFenceValue);
}

void Device::submitFenceSignal(DeviceQueue queue, FenceHandle fenceHandle, uint64 value)
{
	XEAssert(queue == DeviceQueue::Main); // Not implemented.
	d3dGraphicsQueue->Signal(getFenceByHandle(fenceHandle).d3dFence, value);
}

void Device::submitFenceWait(DeviceQueue queue, FenceHandle fenceHandle, uint64 value)
{
	XEAssert(queue == DeviceQueue::Main); // Not implemented.
	d3dGraphicsQueue->Wait(getFenceByHandle(fenceHandle).d3dFence, value);
}

void Device::submitFlip(SwapChainHandle swapChainHandle)
{
	getSwapChainByHandle(swapChainHandle).dxgiSwapChain->Present(1, 0);

	graphicsQueueSyncPointFenceValue = GetDeviceQueueSyncPointFenceNextCounterValue(graphicsQueueSyncPointFenceValue);
	d3dGraphicsQueue->Signal(d3dGraphicsQueueSyncPointFence, graphicsQueueSyncPointFenceValue);
}

DeviceQueueSyncPoint Device::getEndOfQueueSyncPoint(DeviceQueue queue) const
{
	XEAssert(queue == DeviceQueue::Main); // Not implemented.
	return ComposeDeviceQueueSyncPoint(DeviceQueue::Main, graphicsQueueSyncPointFenceValue);
}

bool Device::isQueueSyncPointReached(DeviceQueueSyncPoint syncPoint) const
{
	DecomposedDeviceQueueSyncPoint decomposedSyncPoint = DecomposeDeviceQueueSyncPoint(syncPoint);
	XEAssert(decomposedSyncPoint.queue == DeviceQueue::Main); // Not implemented.

	const uint64 currentFenceCounterValue = d3dGraphicsQueueSyncPointFence->GetCompletedValue();
	return IsDeviceQueueSyncPointFenceCounterValueReached(currentFenceCounterValue, decomposedSyncPoint.queueFenceCounterValue);
}

void* Device::mapBuffer(ResourceHandle bufferHandle)
{
	const Device::Resource& buffer = getResourceByHandle(bufferHandle);
	XEAssert(buffer.type == ResourceType::Buffer && buffer.d3dResource);

	void* result = nullptr;
	buffer.d3dResource->Map(0, nullptr, &result);
	return result;
}

uint64 Device::getFenceValue(FenceHandle fenceHandle) const
{
	return getFenceByHandle(fenceHandle).d3dFence->GetCompletedValue();
}

ResourceHandle Device::getSwapChainBackBuffer(SwapChainHandle swapChainHandle, uint32 backBufferIndex) const
{
	const SwapChain& swapChain = getSwapChainByHandle(swapChainHandle);
	XEAssert(backBufferIndex < countof(swapChain.backBuffers));
	return swapChain.backBuffers[backBufferIndex];
}

uint32 Device::getSwapChainCurrentBackBufferIndex(SwapChainHandle swapChainHandle) const
{
	const SwapChain& swapChain = getSwapChainByHandle(swapChainHandle);
	XEAssert(swapChain.dxgiSwapChain);
	const uint32 backBufferIndex = swapChain.dxgiSwapChain->GetCurrentBackBufferIndex();
	XEAssert(backBufferIndex < countof(swapChain.backBuffers));
	return backBufferIndex;
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
	constexpr uint8 DescriptorSetLayoutHandleSignature = 0x6;
	constexpr uint8 PipelineLayoutHandleSignature = 0x7;
	constexpr uint8 PipelineHandleSignature = 0x8;
	constexpr uint8 FenceHandleSignature = 0x9;
	constexpr uint8 SwapChainHandleSignature = 0xA;

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
		DecomposedHandle result = {};
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

DescriptorSetLayoutHandle Device::composeDescriptorSetLayoutHandle(uint32 descriptorSetLayoutIndex) const
{
	XEAssert(descriptorSetLayoutIndex < MaxDescriptorSetLayoutCount);
	return DescriptorSetLayoutHandle(ComposeHandle(
		DescriptorSetLayoutHandleSignature, descriptorSetLayoutTable[descriptorSetLayoutIndex].handleGeneration, descriptorSetLayoutIndex));
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

uint32 Device::resolveDescriptorSetLayoutHandle(DescriptorSetLayoutHandle handle) const
{
	const DecomposedHandle decomposed = DecomposeHandle(uint32(handle));
	XEAssert(decomposed.signature == DescriptorSetLayoutHandleSignature);
	XEAssert(decomposed.entryIndex < MaxDescriptorSetLayoutCount);
	XEAssert(decomposed.generation == descriptorSetLayoutTable[decomposed.entryIndex].handleGeneration);
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
auto Device::getDescriptorSetLayoutByHandle(DescriptorSetLayoutHandle handle) -> DescriptorSetLayout& { return descriptorSetLayoutTable[resolveDescriptorSetLayoutHandle(handle)]; }
auto Device::getPipelineLayoutByHandle(PipelineLayoutHandle handle) -> PipelineLayout& { return pipelineLayoutTable[resolvePipelineLayoutHandle(handle)]; }
auto Device::getPipelineByHandle(PipelineHandle handle) -> Pipeline& { return pipelineTable[resolvePipelineHandle(handle)]; }
auto Device::getFenceByHandle(FenceHandle handle) -> Fence& { return fenceTable[resolveFenceHandle(handle)]; }
auto Device::getFenceByHandle(FenceHandle handle) const -> const Fence& { return fenceTable[resolveFenceHandle(handle)]; }
auto Device::getSwapChainByHandle(SwapChainHandle handle) -> SwapChain& { return swapChainTable[resolveSwapChainHandle(handle)]; }
auto Device::getSwapChainByHandle(SwapChainHandle handle) const -> const SwapChain& { return swapChainTable[resolveSwapChainHandle(handle)]; }


// DescriptorSetReference //////////////////////////////////////////////////////////////////////////

// DescriptorSetReference structure:
//	Base descriptor index					0x ....'....'...X'XXXX
//	Descriptor set layout index				0x ....'....'XXX.'....
//	Descriptor set layout handle generation	0x ....'..XX'....'....
//	Descriptor set generation				0x ...X'XX..'....'....
//	Checksum								0x .XX.'....'....'....
//	Signature								0x X...'....'....'....

namespace
{
	constexpr uint8 DescriptorSetReferenceSignature = 0xE;
}

DescriptorSetReference Device::composeDescriptorSetReference(DescriptorSetLayoutHandle descriptorSetLayoutHandle,
	uint32 baseDescriptorIndex, uint32 descriptorSetGeneration) const
{
	const DecomposedHandle decomposedDSLHandle = DecomposeHandle(uint32(descriptorSetLayoutHandle));
	XEAssert(decomposedDSLHandle.signature == DescriptorSetLayoutHandleSignature);
	XEAssert(decomposedDSLHandle.entryIndex < MaxDescriptorSetLayoutCount);
	XEAssert(decomposedDSLHandle.generation == descriptorSetLayoutTable[decomposedDSLHandle.entryIndex].handleGeneration);

	const uint8 checksum = 0x69; // TODO: Proper checksum.
	const uint8 descriptorSetLayoutHandleGeneration = decomposedDSLHandle.generation;
	const uint32 descriptorSetLayoutIndex = decomposedDSLHandle.entryIndex;

	XEAssert((baseDescriptorIndex & 0xF'FF'FF) == 0);
	XEAssert((descriptorSetLayoutIndex & 0xF'FF) == 0);
	descriptorSetGeneration &= 0xF'FF;

	uint64 result = baseDescriptorIndex;
	result |= uint64(descriptorSetLayoutIndex) << 20;
	result |= uint64(descriptorSetLayoutHandleGeneration) << 32;
	result |= uint64(descriptorSetGeneration) << 40;
	result |= uint64(checksum) << 52;
	result |= uint64(DescriptorSetReferenceSignature) << 60;

	return DescriptorSetReference(result);
}

auto Device::decomposeDescriptorSetReference(DescriptorSetReference descriptorSetReference) const -> DecomposedDescriptorSetReference
{
	const uint64 refU64 = uint64(descriptorSetReference);

	const uint32 baseDescriptorIndex = uint32(refU64) & 0xF'FF'FF;
	const uint32 descriptorSetLayoutIndex = uint32(refU64 >> 20) & 0xF'FF;
	const uint8 descriptorSetLayoutHandleGeneration = uint8(refU64 >> 32);
	const uint32 descriptorSetGeneration = uint32(refU64 >> 40) & 0xF'FF;
	const uint8 checksum = uint8(refU64 >> 52);
	const uint8 signature = uint8(refU64 >> 60);

	const uint8 checksumVerification = 0x69; // TODO: Proper checksum.

	XEAssert(signature == DescriptorSetReferenceSignature);
	XEAssert(checksum == checksumVerification);
	XEAssert(descriptorSetLayoutIndex < MaxDescriptorSetLayoutCount);
	const DescriptorSetLayout& descriptorSetLayout = descriptorSetLayoutTable[descriptorSetLayoutIndex];
	XEAssert(descriptorSetLayoutHandleGeneration == descriptorSetLayout.handleGeneration);

	return DecomposedDescriptorSetReference { descriptorSetLayout, baseDescriptorIndex, descriptorSetGeneration };
}
