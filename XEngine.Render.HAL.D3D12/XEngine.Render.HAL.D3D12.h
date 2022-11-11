#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>
#include <XLib.Containers.BitArray.h>

#include <XEngine.Render.HAL.Common.h>

// TODO: Check that viewport and scissor are set when rendering.
// TODO: Check that scissor rect is not larget than render target.

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

namespace XEngine::Render::HAL
{
	class Device;
	class Host;

	enum class MemoryBlockHandle			: uint32 {};
	enum class ResourceHandle				: uint32 {};
	enum class ResourceViewHandle			: uint32 {};
	enum class RenderTargetViewHandle		: uint32 {};
	enum class DepthStencilViewHandle		: uint32 {};
	enum class DescriptorSetLayoutHandle	: uint32 {};
	enum class PipelineLayoutHandle			: uint32 {};
	enum class PipelineHandle				: uint32 {};
	enum class DescriptorSetHandle			: uint32 {};
	enum class FenceHandle					: uint32 {};
	enum class SwapChainHandle				: uint32 {};

	enum class DeviceQueueSyncPoint : uint64 { Zero = 0 };

	using DescriptorAddress = uint32;

	static constexpr MemoryBlockHandle ZeroMemoryBlockHandle = MemoryBlockHandle(0);
	static constexpr ResourceHandle ZeroResourceHandle = ResourceHandle(0);
	static constexpr ResourceViewHandle ZeroResourceViewHandle = ResourceViewHandle(0);
	static constexpr RenderTargetViewHandle ZeroRenderTargetViewHandle = RenderTargetViewHandle(0);
	static constexpr DepthStencilViewHandle ZeroDepthStencilViewHandle = DepthStencilViewHandle(0);
	static constexpr DescriptorSetLayoutHandle ZeroDescriptorSetLayoutHandle = DescriptorSetLayoutHandle(0);
	static constexpr PipelineLayoutHandle ZeroPipelineLayoutHandle = PipelineLayoutHandle(0);
	static constexpr PipelineHandle ZeroPipelineHandle = PipelineHandle(0);
	static constexpr FenceHandle ZeroFenceHandle = FenceHandle(0);
	static constexpr SwapChainHandle ZeroSwapChainHandle = SwapChainHandle(0);

	enum class DeviceQueue : uint8
	{
		Undefined = 0,
		Main,
		AsyncCompute,
		AsyncCopy,
	};

	enum class MemoryType : uint8
	{
		Undefined = 0,
		DeviceLocal,
		DeviceReadHostWrite,
		DeviceWriteHostRead,
	};

	struct ResourceAllocationInfo
	{
		uint64 size;
		uint64 alignment;
	};

	enum class ResourceType : uint8
	{
		Undefined = 0,
		Buffer,
		Texture,
	};

	enum class TextureDimension : uint8
	{
		Undefined = 0,
		Texture1D,
		Texture1DArray,
		Texture2D,
		Texture2DArray,
		Texture3D,
		TextureCube,
		TextureCubeArray,
	};

	enum class BufferFlags : uint8
	{
		None = 0,
		AllowShaderWrite = 0x01,
		// AllowRaytracingAccelerationStructure
	};

	XDefineEnumFlagOperators(BufferFlags, uint8)

	enum class TextureFlags : uint8
	{
		None = 0,
		AllowRenderTarget	= 0x01,
		AllowDepthStencil	= 0x02,
		AllowShaderWrite	= 0x04,
	};

	XDefineEnumFlagOperators(TextureFlags, uint8)

	enum class ResourceViewType : uint8
	{
		Undefined = 0,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		ReadOnlyTexelBuffer,
		ReadWriteTexelBuffer,
		ReadOnlyTexture1D,
		ReadWriteTexture1D,
		ReadOnlyTexture2D,
		ReadWriteTexture2D,
		ReadOnlyTexture3D,
		ReadWriteTexture3D,
		ReadOnlyTextureCube,
		RaytracingAccelerationStructure,
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
		uint16 arraySlice;
	};

	struct TextureSubresourceRange
	{
		uint8 baseMipLevel;
		uint8 mipLevelCount;
		uint16 baseArraySlice;
		uint16 arraySliceCount;
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

	struct ResourceViewDesc
	{
		ResourceViewType type;

		union
		{
			struct
			{

			} readOnlyBuffer;

			struct
			{

			} readWriteBuffer;

			struct
			{
				TexelViewFormat format;
				uint8 startMipIndex;
				uint8 mipLevelCount;
				uint8 plane;
			} readOnlyTexture2D;

			struct
			{
				TexelViewFormat format;
				uint8 mipIndex;
				uint8 plane;
			} readWriteTexture2D;
		};
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
		PipelineLayoutHandle currentPipelineLayoutHandle = ZeroPipelineLayoutHandle;

	public:
		CommandList() = default;
		~CommandList();

		void open();
		void close();

		void clearRenderTarget(RenderTargetViewHandle rtv, const float32* color);
		void clearDepthStencil(DepthStencilViewHandle dsv, bool clearDepth, bool clearStencil, float32 depth, uint8 stencil);

		void setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv = ZeroDepthStencilViewHandle);
		void setViewport(float32 left, float32 top, float32 right, float32 bottom, float32 minDepth = 0.0f, float32 maxDepth = 1.0f);
		void setScissor(uint32 left, uint32 top, uint32 right, uint32 bottom);

		inline void setRenderTarget(RenderTargetViewHandle rtv, DepthStencilViewHandle dsv = ZeroDepthStencilViewHandle) { setRenderTargets(1, &rtv, dsv); }

		void setPipelineType(PipelineType pipelineType);
		void setPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle);
		void setPipeline(PipelineHandle pipelineHandle);

		void bindConstants(uint64 bindingNameXSH, const void* data, uint32 size32bitValues, uint32 offset32bitValues = 0);
		void bindBuffer(uint64 bindingNameXSH, BufferBindType bindType, ResourceHandle bufferHandle, uint32 offset = 0);
		void bindDescriptorSet(uint64 bindingNameXSH, DescriptorSetHandle descriptorSetHandle);
		void bindDescriptorArray(uint64 bindingNameXSH, DescriptorAddress arrayStartAddress);

		void draw(uint32 vertexCount, uint32 vertexOffset = 0);
		void drawIndexed(uint32 indexCount, uint32 indexOffset = 0, uint32 vertexOffset = 0);
		void dispatchMesh();
		void dispatch(uint32 groupCountX, uint32 groupCountY = 1, uint32 groupCountZ = 1);

		void bufferMemoryBarrier(ResourceHandle bufferHandle,
			BarrierSync syncBefore, BarrierSync syncAfter,
			BarrierAccess accessBefore, BarrierAccess accessAfter);
		void textureMemoryBarrier(ResourceHandle textureHandle,
			BarrierSync syncBefore, BarrierSync syncAfter,
			BarrierAccess accessBefore, BarrierAccess accessAfter,
			TextureLayout layoutBefore, TextureLayout layoutAfter,
			const TextureSubresourceRange* subresourceRange = nullptr);
		//void globalMemoryBarrier();

		void copyBuffer(ResourceHandle dstBufferHandle, uint64 dstOffset, ResourceHandle srcBufferHandle, uint64 srcOffset, uint64 size);
		void copyTexture(ResourceHandle dstTextureHandle, TextureSubresource dstSubresource, uint16x3 dstOffset,
			ResourceHandle srcTextureHandle, TextureSubresource srcSubresource, const TextureRegion* srcRegion = nullptr);
		void copyBufferTexture(CopyBufferTextureDirection direction,
			ResourceHandle bufferHandle, uint64 bufferOffset, uint32 bufferRowPitch,
			ResourceHandle textureHandle, TextureSubresource textureSubresource, const TextureRegion* textureRegion = nullptr);

		void initializeTextureMetadata(ResourceHandle textureHandle, const TextureSubresourceRange* subresourceRange = nullptr);
	};

	struct BlobDataView
	{
		const void* data;
		uint32 size;
	};

	class Device : public XLib::NonCopyable
	{
		friend CommandList;
		friend Host;

	private:
		static constexpr uint32 MaxMemoryBlockCount = 1024;
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

		struct MemoryBlock;
		struct Resource;
		struct ResourceView;
		struct DescriptorSetLayout;
		struct PipelineLayout;
		struct Pipeline;
		struct Fence;
		struct SwapChain;

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

		MemoryBlock* memoryBlockTable = nullptr;
		Resource* resourceTable = nullptr;
		ResourceView* resourceViewTable = nullptr;
		DescriptorSetLayout* descriptorSetLayoutTable = nullptr;
		PipelineLayout* pipelineLayoutTable = nullptr;
		Pipeline* pipelineTable = nullptr;
		Fence* fenceTable = nullptr;
		SwapChain* swapChainTable = nullptr;

		XLib::InplaceBitArray<MaxMemoryBlockCount> memoryBlockTableAllocationMask;
		XLib::InplaceBitArray<MaxResourceCount> resourceTableAllocationMask;
		XLib::InplaceBitArray<MaxResourceViewCount> resourceViewTableAllocationMask;
		XLib::InplaceBitArray<MaxRenderTargetViewCount> renderTargetViewTableAllocationMask;
		XLib::InplaceBitArray<MaxDepthStencilViewCount> depthStencilViewTableAllocationMask;
		XLib::InplaceBitArray<MaxDescriptorSetLayoutCount> descriptorSetLayoutTableAllocationMask;
		XLib::InplaceBitArray<MaxPipelineLayoutCount> pipelineLayoutTableAllocationMask;
		XLib::InplaceBitArray<MaxPipelineCount> pipelineTableAllocationMask;
		XLib::InplaceBitArray<MaxFenceCount> fenceTableAllocationMask;
		XLib::InplaceBitArray<MaxSwapChainCount> swapChainTableAllocationMask;
		uint32 allocatedResourceDescriptorCount = 0;

		uint64 referenceSRVHeapStartPtr = 0;
		uint64 shaderVisbileSRVHeapStartPtrCPU = 0;
		uint64 shaderVisbileSRVHeapStartPtrGPU = 0;
		uint64 rtvHeapStartPtr = 0;
		uint64 dsvHeapStartPtr = 0;

		uint16 srvDescriptorSize = 0;
		uint16 rtvDescriptorSize = 0;
		uint16 dsvDescriptorSize = 0;

	private:
		MemoryBlockHandle composeMemoryBlockHandle(uint32 memoryBlockIndex) const;
		ResourceHandle composeResourceHandle(uint32 resourceIndex) const;
		ResourceViewHandle composeResourceViewHandle(uint32 resourceViewIndex) const;
		RenderTargetViewHandle composeRenderTargetViewHandle(uint32 renderTargetIndex) const;
		DepthStencilViewHandle composeDepthStencilViewHandle(uint32 depthStencilIndex) const;
		DescriptorSetLayoutHandle composeDescriptorSetLayoutHandle(uint32 descriptorSetLayoutIndex) const;
		PipelineLayoutHandle composePipelineLayoutHandle(uint32 pipelineLayoutIndex) const;
		PipelineHandle composePipelineHandle(uint32 pipelineIndex) const;
		FenceHandle composeFenceHandle(uint32 fenceIndex) const;
		SwapChainHandle composeSwapChainHandle(uint32 swapChainIndex) const;

		uint32 resolveMemoryBlockHandle(MemoryBlockHandle handle) const;
		uint32 resolveResourceHandle(ResourceHandle handle) const;
		uint32 resolveResourceViewHandle(ResourceViewHandle handle) const;
		uint32 resolveRenderTargetViewHandle(RenderTargetViewHandle handle) const;
		uint32 resolveDepthStencilViewHandle(DepthStencilViewHandle handle) const;
		uint32 resolveDescriptorSetLayoutHandle(DescriptorSetLayoutHandle handle) const;
		uint32 resolvePipelineLayoutHandle(PipelineLayoutHandle handle) const;
		uint32 resolvePipelineHandle(PipelineHandle handle) const;
		uint32 resolveFenceHandle(FenceHandle handle) const;
		uint32 resolveSwapChainHandle(SwapChainHandle handle) const;

		DescriptorAddress composeDescriptorAddress(uint32 srvHeapDescriptorIndex) const { return DescriptorAddress(srvHeapDescriptorIndex); }
		uint32 resolveDescriptorAddress(DescriptorAddress address) const { return uint32(address); }

		MemoryBlock& getMemoryBlockByHandle(MemoryBlockHandle handle);
		Resource& getResourceByHandle(ResourceHandle handle);
		ResourceView& getResourceViewByHandle(ResourceViewHandle handle);
		DescriptorSetLayout& getDescriptorSetLayoutByHandle(DescriptorSetLayoutHandle handle);
		PipelineLayout& getPipelineLayoutByHandle(PipelineLayoutHandle handle);
		Pipeline& getPipelineByHandle(PipelineHandle handle);
		Fence& getFenceByHandle(FenceHandle handle);
		const Fence& getFenceByHandle(FenceHandle handle) const;
		SwapChain& getSwapChainByHandle(SwapChainHandle handle);
		const SwapChain& getSwapChainByHandle(SwapChainHandle handle) const;

	private:
		static uint32 CalculateTextureSubresourceIndex(const Resource& resource, const TextureSubresource& subresource);

	private:
		void initialize(ID3D12Device10* d3dDevice);

	public:
		static constexpr uint32 ConstantBufferSizeLimit = 0x10000;
		static constexpr uint32 ConstantBufferBindAlignment = 0x100;

	public:
		Device() = default;
		~Device() = default;

		MemoryBlockHandle allocateMemory(uint64 size, MemoryType memoryType = MemoryType::DeviceLocal);
		void releaseMemory(MemoryBlockHandle memory);

		ResourceAllocationInfo getTextureAllocationInfo(TextureDimension dimension, uint16x3 size,
			TextureFormat format, uint8 mipLevelCount, TextureFlags flags);

		ResourceHandle createBuffer(uint64 size, BufferFlags flags,
			MemoryBlockHandle memoryBlockHandle, uint64 memoryBlockOffset);
		ResourceHandle createTexture(TextureDimension dimension, uint16x3 size,
			TextureFormat format, uint8 mipLevelCount, TextureFlags flags,
			MemoryBlockHandle memoryBlockHandle, uint64 memoryBlockOffset);

		ResourceHandle createPartiallyResidentBuffer(uint64 size, BufferFlags flags);
		ResourceHandle createPartiallyResidentTexture(TextureDimension dimension, uint16x3 size,
			TextureFormat format, uint8 mipLevelCount, TextureFlags flags);

		void destroyResource(ResourceHandle handle);

		ResourceViewHandle createResourceView(ResourceHandle resourceHandle, const ResourceViewDesc& viewDesc);
		void destroyResourceView(ResourceViewHandle handle);

		RenderTargetViewHandle createRenderTargetView(ResourceHandle textureHandle);
		void destroyRenderTargetView(RenderTargetViewHandle handle);

		DepthStencilViewHandle createDepthStencilView(ResourceHandle textureHandle);
		void destroyDepthStencilView(DepthStencilViewHandle handle);

		DescriptorSetLayoutHandle createDescriptorSetLayout(BlobDataView blob);
		void destroyDescriptorSetLayout(DescriptorSetLayoutHandle handle);

		PipelineLayoutHandle createPipelineLayout(BlobDataView blob);
		void destroyPipelineLayout(PipelineLayoutHandle handle);

		PipelineHandle createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle,
			BlobDataView baseBlob, const BlobDataView* bytecodeBlobs, uint32 bytecodeBlobCount);
		PipelineHandle createComputePipeline(PipelineLayoutHandle pipelineLayoutHandle, BlobDataView computeShaderBlob);
		void destroyPipeline(PipelineHandle handle);

		DescriptorSetHandle createDescriptorSet(DescriptorSetLayoutHandle layoutHandle);
		void destroyDescriptorSet(DescriptorSetHandle handle);

		DescriptorAddress allocateDescriptors(uint32 count = 1);
		void releaseDescriptors(DescriptorAddress address);

		FenceHandle createFence(uint64 initialValue);
		void destroyFence(FenceHandle handle);

		SwapChainHandle createSwapChain(uint16 width, uint16 height, void* hWnd);
		void destroySwapChain(SwapChainHandle handle);

		void createCommandList(CommandList& commandList, CommandListType type);
		void destroyCommandList(CommandList& commandList);

		void writeDescriptorSet(DescriptorSetHandle descriptorSet, uint32 bindingNameXSH, ResourceViewHandle resourceViewHandle);
		void writeDescriptor(DescriptorAddress descriptorAddress, ResourceViewHandle resourceViewHandle);

		void submitWorkload(DeviceQueue queue, CommandList& commandList);
		void submitSyncPointWait(DeviceQueue queue, DeviceQueueSyncPoint syncPoint);
		void submitFenceSignal(DeviceQueue queue, FenceHandle fenceHandle, uint64 value);
		void submitFenceWait(DeviceQueue queue, FenceHandle fenceHandle, uint64 value);
		void submitFlip(SwapChainHandle swapChainHandle);
		void submitPartiallyResidentResourcesRemap();

		DeviceQueueSyncPoint getEndOfQueueSyncPoint(DeviceQueue queue) const;
		bool isQueueSyncPointReached(DeviceQueueSyncPoint syncPoint) const;

		void* mapHostVisibleMemoryBlock(MemoryBlockHandle hostVisibleMemoryBlock);
		void unmapHostVisibleMemoryBlock(MemoryBlockHandle hostVisibleMemoryBlock);

		// TODO: Remove. Temporary solution.
		void* mapBuffer(ResourceHandle bufferHandle);
		void unmapBuffer(ResourceHandle bufferHandle);

		uint64 getFenceValue(FenceHandle fenceHandle) const;

		ResourceHandle getSwapChainBackBuffer(SwapChainHandle swapChainHandle, uint32 backBufferIndex) const;
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
