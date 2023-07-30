#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>
#include <XLib.Containers.BitArray.h>

#include <XEngine.Gfx.HAL.Shared.h>

// TODO: Check that viewport and scissor are set when rendering.
// TODO: Check that scissor rect is not larget than render target.
// TODO: Replace DepthStencil with DepthRenderTarget.
// TODO: Probably replace RenderTargetView with RenderTarget, DepthRenderTargetView with DepthRenderTarget.
// TODO: Probably we can state that Texture2D is equivalent to Texture2DArray[1].
// TODO: `TextureDesc` can be packed into 8 bytes.
// TODO: Handle should be 18+10+4 or 17+11+4 instead of 20+8+4.
// TODO: Probably move all the `Host` stuff to `Device` itself.
// TODO: Setting pipeline type should not clear pipeline layout as one layout could work for graphics and compute.
// TODO: Depth-stencil state, rasterizer state, blend state should be provided in runtime. We assume that these settings have nothing to do with shader bytecode.

// NOTE: `CreateGraphicsPipeline` / `CreateComputePipeline` will be replaced with `CreateShader` / `CompileShader`, when
// D3D12 GPU work graphs (including collection state objects) and VK_EXT_shader_object are ready. At that point we should
// also rename `PipelineLayout` to something like `ShaderInputLayout`.

#define XEAssert(cond) XAssert(cond)
#define XEAssertUnreachableCode() XAssertUnreachableCode()
#define XEMasterAssert(cond) XAssert(cond)
#define XEMasterAssertUnreachableCode() XAssertUnreachableCode()

struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList7;
struct ID3D12Device10;
struct ID3D12Fence;

namespace XEngine::Gfx::HAL
{
	class Device;
	class Host;

	enum class DeviceMemoryAllocationHandle	: uint32 { Zero = 0 };
	enum class BufferHandle					: uint32 { Zero = 0 };
	enum class TextureHandle				: uint32 { Zero = 0 };
	enum class ResourceViewHandle			: uint32 { Zero = 0 }; // ReferenceDescriptorHandle
	enum class RenderTargetViewHandle		: uint32 { Zero = 0 }; // RenderTargetHandle
	enum class DepthStencilViewHandle		: uint32 { Zero = 0 }; // DepthRenderTargetViewHandle (DepthRenderTargetHandle)
	enum class DescriptorSetLayoutHandle	: uint32 { Zero = 0 };
	enum class PipelineLayoutHandle			: uint32 { Zero = 0 };
	enum class PipelineHandle				: uint32 { Zero = 0 };
	enum class FenceHandle					: uint32 { Zero = 0 };
	enum class SwapChainHandle				: uint32 { Zero = 0 };

	enum class DescriptorSetReference		: uint64 { Zero = 0 };
	enum class DeviceQueueSyncPoint			: uint64 { Zero = 0 };

	using DescriptorAddress = uint32;

	enum class DeviceQueue : uint8
	{
		Undefined = 0,
		Main,
		AsyncCompute,
		AsyncCopy,
	};

	struct ResourceAllocationInfo
	{
		uint64 size;
		uint64 alignment;
	};

	enum class ResourceType
	{
		Undefined = 0,
		Buffer,
		Texture,
	};

	enum class BufferMemoryType : uint8
	{
		Undefined = 0,
		DeviceLocal,
		Upload,
		Readback,
	};

	enum class TextureDimension : uint8
	{
		Undefined = 0,
		Texture1D,
		Texture2D,
		Texture3D,
	};

	struct TextureDesc
	{
		uint16x3 size;
		TextureDimension dimension;
		TextureFormat format;
		uint8 mipLevelCount;
		bool allowRenderTarget;
		bool allowShaderWrite;
	};

	enum class PipelineType : uint8
	{
		Undefined = 0,
		Graphics,
		Compute,
	};

	enum class CommandListType : uint8
	{
		Undefined = 0,
		Graphics,
		Compute,
		Copy,
	};

	enum class BufferBindType : uint8
	{
		Undefined = 0,
		Constant,
		ReadOnly,
		ReadWrite,
	};

	struct BufferPointer
	{
		BufferHandle buffer;
		uint32 offset;
	};

	enum class TextureAspect : uint8
	{
		Color = 0,
		Depth,
		Stencil,
	};

	struct TextureSubresource
	{
		uint8 mipLevel;
		TextureAspect aspect;
		uint16 arrayIndex;
	};

	struct TextureSubresourceRange
	{
		uint8 baseMipLevel;
		uint8 mipLevelCount;
		uint16 baseArrayIndex;
		uint16 arraySize;
		// TextureAspects aspects;
	};

	enum class BarrierSync : uint16
	{
		None = 0,
		Copy									= 0x0001,
		ComputeShading							= 0x0002,
		GraphicsGeometryShading					= 0x0004,
		GraphicsPixelShading					= 0x0008,
		GraphicsRenderTarget					= 0x0010,
		GraphicsDepthStencil					= 0x0020,
		RaytracingAccelerationStructureBuild	= 0x0040,
		RaytracingAccelerationStructureCopy		= 0x0080,
		// Resolve,
		// IndirectArgument,

		AllShading = ComputeShading | GraphicsGeometryShading | GraphicsPixelShading,
		AllGraphics = GraphicsGeometryShading | GraphicsPixelShading | GraphicsRenderTarget | GraphicsDepthStencil,
		All = Copy | ComputeShading | AllGraphics | RaytracingAccelerationStructureBuild | RaytracingAccelerationStructureCopy,
	};

	XDefineEnumFlagOperators(BarrierSync, uint16)

	enum class BarrierAccess : uint16
	{
		None = 0,
		Any										= 0x0001, // This flag can't be combined with others.
		CopySource								= 0x0002,
		CopyDest								= 0x0004,
		GeometryInput							= 0x0008,
		ConstantBuffer							= 0x0010,
		ShaderRead								= 0x0020,
		ShaderReadWrite							= 0x0040,
		RenderTarget							= 0x0080,
		DepthStencilRead						= 0x0100,
		DepthStencilReadWrite					= 0x0200,
		RaytracingAccelerationStructureRead		= 0x0400, // RTAS accesses are exclusive for RT-enabled buffers (for now).
		RaytracingAccelerationStructureWrite	= 0x0800,
		// Resolve,
		// IndirectArgument,
	};

	XDefineEnumFlagOperators(BarrierAccess, uint16)

	enum class TextureLayout : uint8
	{
		Undefined = 0,
		Present,
		CopySource,
		CopyDest,
		ShaderReadAndCopySource,
		ShaderReadAndCopySourceDest,
		ShaderRead,
		ShaderReadWrite,
		RenderTarget,
		DepthStencilRead,
		DepthStencilReadWrite,
	};

	enum class CopyBufferTextureDirection : uint8
	{
		BufferToTexture,
		TextureToBuffer,
	};

	struct TextureRegion
	{
		uint16x3 offset;
		uint16x3 size;
	};

	class CommandList : public XLib::NonCopyable
	{
		friend Device;

	private:
		enum class State : uint8;

		struct BindingResolveResult;

		static inline BindingResolveResult ResolveBindingByNameXSH(
			Device* device, PipelineLayoutHandle pipleineLayoutHandle, uint64 bindingNameXSH);

	private:
		Device* device = nullptr;
		ID3D12GraphicsCommandList7* d3dCommandList = nullptr;
		ID3D12CommandAllocator* d3dCommandAllocator = nullptr;
		CommandListType type = CommandListType::Undefined;

		State state = State(0);
		DeviceQueueSyncPoint executionEndSyncPoint = DeviceQueueSyncPoint::Zero;
		PipelineType currentPipelineType = PipelineType::Undefined;
		PipelineLayoutHandle currentPipelineLayoutHandle = PipelineLayoutHandle::Zero;

	public:
		CommandList() = default;
		~CommandList();

		void open();
		//void close(); // TODO: Do we need it at all?

		void clearRenderTarget(RenderTargetViewHandle rtv, const float32* color);
		void clearDepthStencil(DepthStencilViewHandle dsv, bool clearDepth, bool clearStencil, float32 depth, uint8 stencil);

		void setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv = DepthStencilViewHandle::Zero);
		void setViewport(float32 left, float32 top, float32 right, float32 bottom, float32 minDepth = 0.0f, float32 maxDepth = 1.0f);
		void setScissor(uint32 left, uint32 top, uint32 right, uint32 bottom);

		inline void setRenderTarget(RenderTargetViewHandle rtv, DepthStencilViewHandle dsv = DepthStencilViewHandle::Zero) { setRenderTargets(1, &rtv, dsv); }

		void setPipelineType(PipelineType pipelineType);
		void setPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle);
		void setPipeline(PipelineHandle pipelineHandle);

		void bindConstants(uint64 bindingNameXSH, const void* data, uint32 size32bitValues, uint32 offset32bitValues = 0);
		void bindBuffer(uint64 bindingNameXSH, BufferBindType bindType, BufferPointer bufferPointer);
		void bindDescriptorSet(uint64 bindingNameXSH, DescriptorSetReference descriptorSetReference);
		void bindDescriptorArray(uint64 bindingNameXSH, DescriptorAddress arrayStartAddress);

		void draw(uint32 vertexCount, uint32 vertexOffset = 0);
		void drawIndexed(uint32 indexCount, uint32 indexOffset = 0, uint32 vertexOffset = 0);
		void dispatchMesh();
		void dispatch(uint32 groupCountX, uint32 groupCountY = 1, uint32 groupCountZ = 1);

		void bufferMemoryBarrier(BufferHandle bufferHandle,
			BarrierSync syncBefore, BarrierSync syncAfter,
			BarrierAccess accessBefore, BarrierAccess accessAfter);
		void textureMemoryBarrier(TextureHandle textureHandle,
			BarrierSync syncBefore, BarrierSync syncAfter,
			BarrierAccess accessBefore, BarrierAccess accessAfter,
			TextureLayout layoutBefore, TextureLayout layoutAfter,
			const TextureSubresourceRange* subresourceRange = nullptr);
		//void globalMemoryBarrier();

		void copyBuffer(BufferHandle dstBufferHandle, uint64 dstOffset, BufferHandle srcBufferHandle, uint64 srcOffset, uint64 size);
		void copyTexture(TextureHandle dstTextureHandle, TextureSubresource dstSubresource, uint16x3 dstOffset,
			TextureHandle srcTextureHandle, TextureSubresource srcSubresource, const TextureRegion* srcRegion = nullptr);
		void copyBufferTexture(CopyBufferTextureDirection direction,
			BufferHandle bufferHandle, uint64 bufferOffset, uint32 bufferRowPitch,
			TextureHandle textureHandle, TextureSubresource textureSubresource, const TextureRegion* textureRegion = nullptr);

		void initializeTextureMetadata(TextureHandle textureHandle, const TextureSubresourceRange* subresourceRange = nullptr);

		//void pushDebugMarker(const char* name);
		//void popDebugMarker();
	};

	struct BlobDataView
	{
		const void* data;
		uint32 size;
	};

	struct GraphicsPipelineBlobs
	{
		BlobDataView state;
		BlobDataView vs;
		BlobDataView as;
		BlobDataView ms;
		BlobDataView ps;
	};

	class Device : public XLib::NonCopyable
	{
		friend CommandList;
		friend Host;

	private:
		static constexpr uint32 MaxMemoryAllocationCount = 1024;
		static constexpr uint32 MaxResourceCount = 1024;
		static constexpr uint32 MaxResourceViewCount = 1024;
		static constexpr uint32 MaxResourceDescriptorCount = 4096;
		static constexpr uint32 MaxRenderTargetViewCount = 64;
		static constexpr uint32 MaxDepthStencilViewCount = 64;
		static constexpr uint32 MaxDescriptorSetLayoutCount = 64;
		static constexpr uint32 MaxPipelineLayoutCount = 64;
		static constexpr uint32 MaxPipelineCount = 1024;
		static constexpr uint32 MaxFenceCount = 64;
		static constexpr uint32 MaxSwapChainCount = 4;
		static constexpr uint32 SwapChainBackBufferCount = 2;

		struct MemoryAllocation;
		struct Resource;
		struct ResourceView;
		struct DescriptorSetLayout;
		struct PipelineLayout;
		struct Pipeline;
		struct Fence;
		struct SwapChain;

		struct DecomposedDescriptorSetReference;

	private:
		XLib::Platform::COMPtr<ID3D12Device10> d3dDevice;

		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dReferenceSRVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dShaderVisbileSRVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dRTVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dDSVHeap;

		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dGraphicsQueue;
		//XLib::Platform::COMPtr<ID3D12CommandQueue> d3dAsyncComputeQueue;
		//XLib::Platform::COMPtr<ID3D12CommandQueue> d3dAsyncCopyQueue;

		XLib::Platform::COMPtr<ID3D12Fence> d3dGraphicsQueueSyncPointFence;
		//XLib::Platform::COMPtr<ID3D12Fence> d3dAsyncComputeQueueFence;
		//XLib::Platform::COMPtr<ID3D12Fence> d3dAsyncCopyQueueFence;
		uint64 graphicsQueueSyncPointFenceValue = 0;
		//uint64 asyncComputeQueueFenceValue = 0;
		//uint64 asyncCopyQueueFenceValue = 0;

		MemoryAllocation* memoryAllocationTable = nullptr;
		Resource* resourceTable = nullptr;
		ResourceView* resourceViewTable = nullptr;
		DescriptorSetLayout* descriptorSetLayoutTable = nullptr;
		PipelineLayout* pipelineLayoutTable = nullptr;
		Pipeline* pipelineTable = nullptr;
		Fence* fenceTable = nullptr;
		SwapChain* swapChainTable = nullptr;

		XLib::InplaceBitArray<MaxMemoryAllocationCount> memoryAllocationTableAllocationMask;
		XLib::InplaceBitArray<MaxResourceCount> resourceTableAllocationMask;
		XLib::InplaceBitArray<MaxResourceViewCount> resourceViewTableAllocationMask;
		XLib::InplaceBitArray<MaxRenderTargetViewCount> renderTargetViewTableAllocationMask;
		XLib::InplaceBitArray<MaxDepthStencilViewCount> depthStencilViewTableAllocationMask;
		XLib::InplaceBitArray<MaxDescriptorSetLayoutCount> descriptorSetLayoutTableAllocationMask;
		XLib::InplaceBitArray<MaxPipelineLayoutCount> pipelineLayoutTableAllocationMask;
		XLib::InplaceBitArray<MaxPipelineCount> pipelineTableAllocationMask;
		XLib::InplaceBitArray<MaxFenceCount> fenceTableAllocationMask;
		XLib::InplaceBitArray<MaxSwapChainCount> swapChainTableAllocationMask;

		uint64 referenceSRVHeapPtr = 0;
		uint64 shaderVisbileSRVHeapCPUPtr = 0;
		uint64 shaderVisbileSRVHeapGPUPtr = 0;
		uint64 rtvHeapPtr = 0;
		uint64 dsvHeapPtr = 0;

		uint16 srvDescriptorSize = 0;
		uint16 rtvDescriptorSize = 0;
		uint16 dsvDescriptorSize = 0;

		uint32 descriptorSetReferenceGenerationCounter = 0;

	private:
		DeviceMemoryAllocationHandle composeMemoryAllocationHandle(uint32 memoryAllocationIndex) const;
		BufferHandle composeBufferHandle(uint32 resourceIndex) const;
		TextureHandle composeTextureHandle(uint32 resourceIndex) const;
		ResourceViewHandle composeResourceViewHandle(uint32 resourceViewIndex) const;
		RenderTargetViewHandle composeRenderTargetViewHandle(uint32 renderTargetIndex) const;
		DepthStencilViewHandle composeDepthStencilViewHandle(uint32 depthStencilIndex) const;
		DescriptorSetLayoutHandle composeDescriptorSetLayoutHandle(uint32 descriptorSetLayoutIndex) const;
		PipelineLayoutHandle composePipelineLayoutHandle(uint32 pipelineLayoutIndex) const;
		PipelineHandle composePipelineHandle(uint32 pipelineIndex) const;
		FenceHandle composeFenceHandle(uint32 fenceIndex) const;
		SwapChainHandle composeSwapChainHandle(uint32 swapChainIndex) const;

		uint32 resolveMemoryAllocationHandle(DeviceMemoryAllocationHandle handle) const;
		uint32 resolveBufferHandle(BufferHandle handle) const;
		uint32 resolveTextureHandle(TextureHandle handle) const;
		uint32 resolveResourceViewHandle(ResourceViewHandle handle) const;
		uint32 resolveRenderTargetViewHandle(RenderTargetViewHandle handle) const;
		uint32 resolveDepthStencilViewHandle(DepthStencilViewHandle handle) const;
		uint32 resolveDescriptorSetLayoutHandle(DescriptorSetLayoutHandle handle) const;
		uint32 resolvePipelineLayoutHandle(PipelineLayoutHandle handle) const;
		uint32 resolvePipelineHandle(PipelineHandle handle) const;
		uint32 resolveFenceHandle(FenceHandle handle) const;
		uint32 resolveSwapChainHandle(SwapChainHandle handle) const;

		MemoryAllocation& getMemoryAllocationByHandle(DeviceMemoryAllocationHandle handle);
		Resource& getResourceByBufferHandle(BufferHandle handle);
		Resource& getResourceByTextureHandle(TextureHandle handle);
		ResourceView& getResourceViewByHandle(ResourceViewHandle handle);
		const DescriptorSetLayout& getDescriptorSetLayoutByHandle(DescriptorSetLayoutHandle handle) const;
		PipelineLayout& getPipelineLayoutByHandle(PipelineLayoutHandle handle);
		Pipeline& getPipelineByHandle(PipelineHandle handle);
		Fence& getFenceByHandle(FenceHandle handle);
		const Fence& getFenceByHandle(FenceHandle handle) const;
		SwapChain& getSwapChainByHandle(SwapChainHandle handle);
		const SwapChain& getSwapChainByHandle(SwapChainHandle handle) const;

		DescriptorSetReference composeDescriptorSetReference(DescriptorSetLayoutHandle descriptorSetLayoutHandle,
			uint32 baseDescriptorIndex, uint32 descriptorSetGeneration) const;
		DecomposedDescriptorSetReference decomposeDescriptorSetReference(DescriptorSetReference descriptorSetReference) const;

	private:
		static uint32 CalculateTextureSubresourceIndex(const Resource& resource, const TextureSubresource& subresource);

	private:
		void initialize(ID3D12Device10* d3dDevice);

	public:
		static constexpr uint32 ConstantBufferSizeLimit = 64 * 1024;
		static constexpr uint16 ConstantBufferBindAlignmentLog2 = 8;
		static constexpr uint16 ConstantBufferBindAlignment = 1 << ConstantBufferBindAlignmentLog2;
		static constexpr uint16 TextureArraySizeLimit = 2048;

	public:
		Device() = default;
		~Device() = default;

		DeviceMemoryAllocationHandle allocateMemory(uint64 size);
		void releaseMemory(DeviceMemoryAllocationHandle memory);

		ResourceAllocationInfo getTextureAllocationInfo(const TextureDesc& textureDesc) const;

		BufferHandle createBuffer(uint64 size, bool allowShaderWrite, BufferMemoryType memoryType = BufferMemoryType::DeviceLocal,
			DeviceMemoryAllocationHandle memoryHandle = DeviceMemoryAllocationHandle::Zero, uint64 memoryOffset = 0);
		TextureHandle createTexture(const TextureDesc& desc,
			DeviceMemoryAllocationHandle memoryHandle = DeviceMemoryAllocationHandle::Zero, uint64 memoryOffset = 0);

		BufferHandle createPartiallyResidentBuffer(uint64 size, bool allowShaderWrite);
		TextureHandle createPartiallyResidentTexture(const TextureDesc& textureDesc);

		void destroyBuffer(BufferHandle bufferHandle);
		void destroyTexture(TextureHandle textureHandle);

		ResourceViewHandle createBufferView(BufferHandle bufferHandle, TexelViewFormat format, bool writable);
		ResourceViewHandle createTextureView(TextureHandle textureHandle,
			TexelViewFormat format, bool writable, const TextureSubresourceRange& subresourceRange);
		ResourceViewHandle createTextureCubeView(TextureHandle textureHandle, TexelViewFormat format,
			uint8 baseMipLevel = 0, uint8 mipLevelCount = 0, uint16 baseArrayIndex = 0, uint16 arraySize = 0);
		ResourceViewHandle createRaytracingAccelerationStructureView(...);

		// ReferenceDescriptorHandle createBufferDescriptor(...);
		// ReferenceDescriptorHandle createTextureDescriptor(...);
		// ReferenceDescriptorHandle createTextureCubeDescriptor(...);
		// ReferenceDescriptorHandle createRaytracingAccelerationStructureDescriptor(...);

		void destroyResourceView(ResourceViewHandle handle);

		RenderTargetViewHandle createRenderTargetView(TextureHandle textureHandle,
			TexelViewFormat format, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		void destroyRenderTargetView(RenderTargetViewHandle handle);

		DepthStencilViewHandle createDepthStencilView(TextureHandle textureHandle,
			bool writableDepth, bool writableStencil, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		void destroyDepthStencilView(DepthStencilViewHandle handle);

		DescriptorSetLayoutHandle createDescriptorSetLayout(BlobDataView blob);
		void destroyDescriptorSetLayout(DescriptorSetLayoutHandle handle);

		PipelineLayoutHandle createPipelineLayout(BlobDataView blob);
		void destroyPipelineLayout(PipelineLayoutHandle handle);

		PipelineHandle createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle, const GraphicsPipelineBlobs& blobs);
		PipelineHandle createComputePipeline(PipelineLayoutHandle pipelineLayoutHandle, BlobDataView csBlob);
		void destroyPipeline(PipelineHandle handle);

		FenceHandle createFence(uint64 initialValue);
		void destroyFence(FenceHandle handle);

		SwapChainHandle createSwapChain(uint16 width, uint16 height, void* hWnd);
		void destroySwapChain(SwapChainHandle handle);

		void createCommandList(CommandList& commandList, CommandListType type);
		void destroyCommandList(CommandList& commandList);

		DescriptorSetReference createDescriptorSetReference(DescriptorSetLayoutHandle descriptorSetLayoutHandle, DescriptorAddress address);

		void writeDescriptor(DescriptorAddress descriptorAddress, ResourceViewHandle resourceViewHandle);
		void writeDescriptor(DescriptorSetReference descriptorSetReference, uint64 bindingNameXSH, ResourceViewHandle resourceViewHandle);

		void submitWorkload(DeviceQueue queue, CommandList& commandList);
		void submitSyncPointWait(DeviceQueue queue, DeviceQueueSyncPoint syncPoint);
		void submitFenceSignal(DeviceQueue queue, FenceHandle fenceHandle, uint64 value);
		void submitFenceWait(DeviceQueue queue, FenceHandle fenceHandle, uint64 value);
		void submitFlip(SwapChainHandle swapChainHandle);
		void submitPartiallyResidentResourcesRemap();

		DeviceQueueSyncPoint getEndOfQueueSyncPoint(DeviceQueue queue) const;
		bool isQueueSyncPointReached(DeviceQueueSyncPoint syncPoint) const;

		void* mapBuffer(BufferHandle bufferHandle);
		void unmapBuffer(BufferHandle bufferHandle);

		uint16 getDescriptorSetLayoutDescriptorCount(DescriptorSetLayoutHandle descriptorSetLayoutHandle) const;

		uint64 getFenceValue(FenceHandle fenceHandle) const;

		TextureHandle getSwapChainBackBuffer(SwapChainHandle swapChainHandle, uint32 backBufferIndex) const;
		uint32 getSwapChainCurrentBackBufferIndex(SwapChainHandle swapChainHandle) const;

		const char* getName() const;
	};

	class Host abstract final
	{
	public:
		static void CreateDevice(Device& device);
		static uint16x3 CalculateMipLevelSize(uint16x3 srcSize, uint8 mipLevel);
	};
}
