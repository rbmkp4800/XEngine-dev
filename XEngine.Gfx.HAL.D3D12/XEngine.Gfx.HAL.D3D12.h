#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Gfx.HAL.Shared.h>

// TODO: Check that viewport and scissor are set when rendering.
// TODO: Check that scissor rect is not larget than render target.
// TODO: Probably we can state that Texture2D is equivalent to Texture2DArray[1].
// TODO: Handle should be 18+10+4 or 17+11+4 instead of 20+8+4.
// TODO: Setting pipeline type should not clear pipeline layout as one layout could work for graphics and compute.
// TODO: Depth-stencil state, rasterizer state, blend state should be provided in runtime. We assume that these settings have nothing to do with shader bytecode.
// TODO: Implement "host visible" memory/resources via `D3D12_HEAP_TYPE_GPU_UPLOAD`.
// TODO: Move bindings from `Device::DescriptorSetLayout` and `Device::PipelineLayout` to device internal heap (TLSF probably).
// TODO: Look into `minImageTransferGranularity`. This is additional constraint on CLs we submit to copy queue.

// NOTE: `CreateGraphicsPipeline` / `CreateComputePipeline` will be replaced with `CreateShader` / `CompileShader`, when
// D3D12 GPU work graphs (including collection state objects) and VK_EXT_shader_object are ready. At that point we should
// also rename `PipelineLayout` to something like `ShaderInputLayout`.

// NOTE: Virtual textures do not support compression (CMASK/DCC) on some architectures.

#define XEAssert(cond) XAssert(cond)
#define XEAssertUnreachableCode() XAssertUnreachableCode()
#define XEMasterAssert(cond) XAssert(cond)
#define XEMasterAssertUnreachableCode() XAssertUnreachableCode()

struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList7;
struct ID3D12Device10;
struct ID3D12Fence;

namespace XEngine::Gfx::HAL
{
	constexpr uint32 ConstantBufferSizeLimit = 64 * 1024;
	constexpr uint16 ConstantBufferBindAlignmentLog2 = 8;
	constexpr uint16 ConstantBufferBindAlignment = 1 << ConstantBufferBindAlignmentLog2;
	constexpr uint16 TextureArraySizeLimit = 2048;
	constexpr uint32 OutputBackBufferCount = 2;

	class Device;

	enum class CommandAllocatorHandle			: uint32 { Zero = 0 };
	enum class DeviceMemoryAllocationHandle		: uint32 { Zero = 0 };
	enum class BufferHandle						: uint32 { Zero = 0 };
	enum class TextureHandle					: uint32 { Zero = 0 };
	enum class ResourceViewHandle				: uint32 { Zero = 0 }; // DescriptorSourceHandle?? createTextureDescriptorSource?
	enum class ColorRenderTargetHandle			: uint32 { Zero = 0 };
	enum class DepthStencilRenderTargetHandle	: uint32 { Zero = 0 };
	enum class DescriptorSetLayoutHandle		: uint32 { Zero = 0 };
	enum class PipelineLayoutHandle				: uint32 { Zero = 0 };
	enum class PipelineHandle					: uint32 { Zero = 0 };
	enum class OutputHandle						: uint32 { Zero = 0 };

	enum class DescriptorSetReference			: uint64 { Zero = 0 };
	enum class DeviceQueueSyncPoint				: uint64 { Zero = 0 };
	enum class HostSignalToken					: uint64 { Zero = 0 };

	using DescriptorAddress = uint32;

	enum class DeviceQueue : uint8 // Also used as queue index, so should start from 0.
	{
		Graphics = 0,
		Compute,
		Copy,
		ValueCount,
	};

	static constexpr uint8 DeviceQueueCount = uint8(DeviceQueue::ValueCount);

	struct ResourceAllocationInfo
	{
		uint64 size;
		uint64 alignment; // TODO: Remove this. It should be well known constant (inc user provided tiling).
	};

	enum class ResourceType
	{
		Undefined = 0,
		Buffer,
		Texture,
	};

	enum class StagingBufferAccessMode : uint8
	{
		Undefined = 0,
		DeviceReadHostWrite,
		DeviceWriteHostRead,
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
		TextureDimension dimension : 2; // TODO: Calculate these bits properly.
		TextureFormat format : 6;
		uint8 mipLevelCount : 4;
		bool allowRenderTarget : 1;
	};
	static_assert(sizeof(TextureDesc) == 8);

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

	enum class IndexBufferFormat : uint8
	{
		Undefined = 0,
		U16,
		U32,
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
		struct BindingResolveResult;

		static inline BindingResolveResult ResolveBindingByNameXSH(
			Device* device, PipelineLayoutHandle pipleineLayoutHandle, uint64 bindingNameXSH);

	private:
		Device* device = nullptr;
		ID3D12GraphicsCommandList7* d3dCommandList = nullptr;
		uint32 deviceCommandListHandle = 0;
		CommandAllocatorHandle commandAllocatorHandle = CommandAllocatorHandle::Zero;

		CommandListType type = CommandListType::Undefined;
		bool isOpen = false;

		PipelineLayoutHandle currentPipelineLayoutHandle = PipelineLayoutHandle::Zero;
		PipelineType currentPipelineType = PipelineType::Undefined;

	private:
		void cleanup();

	public:
		CommandList() = default;
		~CommandList();

		void clearColorRenderTarget(ColorRenderTargetHandle colorRT, const float32* color);
		void clearDepthStencilRenderTarget(DepthStencilRenderTargetHandle depthStencilRT,
			bool clearDepth, bool clearStencil, float32 depth, uint8 stencil);

		void setRenderTargets(uint8 colorRTCount, const ColorRenderTargetHandle* colorRTs,
			DepthStencilRenderTargetHandle depthStencilRT = DepthStencilRenderTargetHandle::Zero);
		void setRenderTarget(ColorRenderTargetHandle colorRT,
			DepthStencilRenderTargetHandle depthStencilRT = DepthStencilRenderTargetHandle::Zero);
		void setViewport(float32 left, float32 top, float32 right, float32 bottom, float32 minDepth = 0.0f, float32 maxDepth = 1.0f);
		void setScissor(uint32 left, uint32 top, uint32 right, uint32 bottom);

		void setPipelineType(PipelineType pipelineType);
		void setPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle);
		void setPipeline(PipelineHandle pipelineHandle);

		void bindIndexBuffer(BufferPointer bufferPointer, IndexBufferFormat format, uint32 byteSize);
		void bindVertexBuffer(uint8 bufferIndex, BufferPointer bufferPointer, uint16 stride, uint32 byteSize);

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

	struct DeviceSettings
	{
		uint16 maxMemoryAllocationCount;
		uint16 maxCommandAllocatorCount;
		uint16 maxCommandListCount;
		uint16 maxResourceCount;
		uint16 maxResourceViewCount;
		uint16 maxColorRenderTargetCount;
		uint16 maxDepthStencilRenderTargetCount;
		uint16 maxDescriptorSetLayoutCount;
		uint16 maxPipelineLayoutCount;
		uint16 maxPipelineCount;
		uint16 maxOutputCount;

		uint32 descriptorPoolSize;
	};

	/*class PhysicalDevice
	{
	private:

	public:
		PhysicalDevice() = default;
		~PhysicalDevice() = default;
	};*/

	class Device : public XLib::NonCopyable
	{
		friend CommandList;

	private:
		struct Queue
		{
			ID3D12CommandQueue* d3dQueue;
			ID3D12Fence* d3dDeviceSignalFence;
			//ID3D12Fence* d3dHostSignalFence;
			uint64 deviceSignalEmittedValue;
			uint64 deviceSignalReachedValue;
		};

		template <typename EntryType>
		class Pool
		{
		private:
			EntryType* buffer = nullptr;
			uint16 capacity = 0;
			uint16 committedEntryCount = 0; // Number of entries allocated at least once.
			uint16 freelistHeadIdx = 0;
			uint16 freelistLength = 0;

		private:
			static constexpr uint8 GetHandleSignature();

		public:
			Pool() = default;
			~Pool() = default;

			inline void initialize(EntryType* buffer, uint16 capacity);

			inline EntryType& allocate(uint32& outHandle, uint16* outEntryIndex = nullptr);
			inline void release(uint32 handle);

			inline uint16 resolveHandleToEntryIndex(uint32 handle) const;
			inline EntryType& resolveHandle(uint32 handle, uint16* outEntryIndex = nullptr);
			inline const EntryType& resolveHandle(uint32 handle, uint16* outEntryIndex = nullptr) const;

			inline uint16 getCapacity() const { return capacity; }
			inline uint16 getAllocatedEntryCount() const { return committedEntryCount - freelistLength; }

			static inline uint8 GetHandleGeneration(uint32 handle);
			static inline uint16 GetHandleEntryIndex(uint32 handle);
			static inline uint32 ComposeHandle(uint16 entryIndex, uint8 handleGeneration);
		};

		struct PoolEntryBase;

		struct CommandAllocator;
		struct CommandList;
		struct MemoryAllocation;
		struct Resource;
		struct ResourceView;
		struct ColorRenderTarget;
		struct DepthStencilRenderTarget;
		struct DescriptorSetLayout;
		struct PipelineLayout;
		struct Pipeline;
		struct Output;

		struct DecomposedDescriptorSetReference;

	private:
		ID3D12Device10* d3dDevice = nullptr;

		Queue queues[DeviceQueueCount] = {};

		ID3D12DescriptorHeap* d3dHostSRVHeap = nullptr; // Resource view pool
		ID3D12DescriptorHeap* d3dShaderVisbileSRVHeap = nullptr; // Descriptor pool
		ID3D12DescriptorHeap* d3dRTVHeap = nullptr;
		ID3D12DescriptorHeap* d3dDSVHeap = nullptr;

		uint64 hostSRVHeapPtr = 0;
		uint64 shaderVisbileSRVHeapCPUPtr = 0;
		uint64 shaderVisbileSRVHeapGPUPtr = 0;
		uint64 rtvHeapPtr = 0;
		uint64 dsvHeapPtr = 0;

		uint16 srvDescriptorSize = 0;
		uint16 rtvDescriptorSize = 0;
		uint16 dsvDescriptorSize = 0;

		Pool<CommandAllocator> commandAllocatorPool;
		Pool<CommandList> commandListPool;
		Pool<MemoryAllocation> memoryAllocationPool;
		Pool<Resource> resourcePool;
		Pool<ResourceView> resourceViewPool;
		Pool<ColorRenderTarget> colorRenderTargetPool;
		Pool<DepthStencilRenderTarget> depthStencilRenderTargetPool;
		Pool<DescriptorSetLayout> descriptorSetLayoutPool;
		Pool<PipelineLayout> pipelineLayoutPool;
		Pool<Pipeline> pipelinePool;
		Pool<Output> outputPool;

		uint32 descriptorPoolSize = 0;

		uint8 descriptorSetReferenceMagicCounter = 0;

	private:
		static inline DescriptorSetReference ComposeDescriptorSetReference(DescriptorSetLayoutHandle descriptorSetLayoutHandle,
			uint32 baseDescriptorIndex, uint8 descriptorSetMagic);
		static inline void DecomposeDescriptorSetReference(DescriptorSetReference descriptorSetReference,
			DescriptorSetLayoutHandle& outDescriptorSetLayoutHandle, uint32& outBaseDescriptorIndex, uint8& outDescriptorSetMagic);

		static consteval uint8 GetDeviceQueueSyncPointSignalValueBitCount();
		static inline DeviceQueueSyncPoint ComposeDeviceQueueSyncPoint(uint8 queueIndex, uint64 signalValue);
		static inline void DecomposeDeviceQueueSyncPoint(DeviceQueueSyncPoint syncPoint, uint8& outQueueIndex, uint64& outSignalValue);

		static uint32 CalculateTextureSubresourceIndex(const TextureDesc& textureDesc, const TextureSubresource& textureSubresource);

	private:
		inline bool isDeviceSignalValueReached(const Queue& queue, uint64 signalValue, uint8 signalValueBitCount) const;
		void updateCommandAllocatorExecutionStatus(CommandAllocator& commandAllocator);

	public:
		Device() = default;
		~Device() = default;

		void initialize(/*const PhysicalDevice& physicalDevice, */const DeviceSettings& settings);

		CommandAllocatorHandle createCommandAllocator();
		void destroyCommandAllocator(CommandAllocatorHandle commandAllocatorHandle);

		DeviceMemoryAllocationHandle allocateDeviceMemory(uint64 size, bool hostVisible);
		void releaseDeviceMemory(DeviceMemoryAllocationHandle deviceMemoryHandle);

		ResourceAllocationInfo getTextureAllocationInfo(const TextureDesc& textureDesc) const;

		BufferHandle createBuffer(uint64 size, DeviceMemoryAllocationHandle memoryHandle, uint64 memoryOffset);
		TextureHandle createTexture(const TextureDesc& desc, DeviceMemoryAllocationHandle memoryHandle, uint64 memoryOffset);

		BufferHandle createVirtualBuffer(uint64 size);
		TextureHandle createVirtualTexture(const TextureDesc& textureDesc);

		BufferHandle createStagingBuffer(uint64 size, StagingBufferAccessMode accessMode);

		void destroyBuffer(BufferHandle bufferHandle);
		void destroyTexture(TextureHandle textureHandle);

		ResourceViewHandle createBufferView(BufferHandle bufferHandle, TexelViewFormat format, bool writable);
		ResourceViewHandle createTextureView(TextureHandle textureHandle,
			TexelViewFormat format, bool writable, const TextureSubresourceRange& subresourceRange);
		ResourceViewHandle createTextureCubeView(TextureHandle textureHandle, TexelViewFormat format,
			uint8 baseMipLevel = 0, uint8 mipLevelCount = 0, uint16 baseArrayIndex = 0, uint16 arraySize = 0);
		//ResourceViewHandle createRaytracingAccelerationStructureView();

		void destroyResourceView(ResourceViewHandle resourceViewHandle);

		ColorRenderTargetHandle createColorRenderTarget(TextureHandle textureHandle,
			TexelViewFormat format, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		void destroyColorRenderTarget(ColorRenderTargetHandle colorRenderTargetHandle);

		DepthStencilRenderTargetHandle createDepthStencilRenderTarget(TextureHandle textureHandle,
			bool writableDepth, bool writableStencil, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		void destroyDepthStencilView(DepthStencilRenderTargetHandle depthStencilRenderTargetHandle);

		DescriptorSetLayoutHandle createDescriptorSetLayout(BlobDataView blob);
		void destroyDescriptorSetLayout(DescriptorSetLayoutHandle descriptorSetLayoutHandle);

		PipelineLayoutHandle createPipelineLayout(BlobDataView blob);
		void destroyPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle);

		PipelineHandle createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle, const GraphicsPipelineBlobs& blobs);
		PipelineHandle createComputePipeline(PipelineLayoutHandle pipelineLayoutHandle, BlobDataView csBlob);
		void destroyPipeline(PipelineHandle pipelineHandle);

		OutputHandle createWindowOutput(uint16 width, uint16 height, void* platformWindowHandle);
		// OutputHandle createDisplayOutput();
		// OutputHandle createVROutput();
		void destroyOutput(OutputHandle outputHandle);

		DescriptorSetReference createDescriptorSetReference(DescriptorSetLayoutHandle descriptorSetLayoutHandle, DescriptorAddress address);

		void writeDescriptor(DescriptorAddress descriptorAddress, ResourceViewHandle resourceViewHandle);
		void writeDescriptor(DescriptorSetReference descriptorSetReference, uint64 bindingNameXSH, ResourceViewHandle resourceViewHandle);

		void openCommandList(HAL::CommandList& commandList, CommandAllocatorHandle commandAllocatorHandle, CommandListType type = CommandListType::Graphics);
		void closeCommandList(HAL::CommandList& commandList);
		void discardCommandList(HAL::CommandList& commandList);

		void resetCommandAllocator(CommandAllocatorHandle commandAllocatorHandle);

		void submitCommandList(DeviceQueue deviceQueue, HAL::CommandList& commandList);
		void submitOutputFlip(DeviceQueue deviceQueue, OutputHandle outputHandle);
		//void submitVirtualResourceMappingUpdate(DeviceQueue deviceQueue, );
		void submitSyncPointWait(DeviceQueue deviceQueue, DeviceQueueSyncPoint syncPoint);
		HostSignalToken submitHostSignalWait(DeviceQueue deviceQueue);

		DeviceQueueSyncPoint getEndOfQueueSyncPoint(DeviceQueue deviceQueue) const;
		bool isQueueSyncPointReached(DeviceQueueSyncPoint syncPoint) const;
		void emitHostSignal(HostSignalToken signalToken);

		void* getMappedBufferPtr(BufferHandle bufferHandle) const;

		uint16 getDescriptorSetLayoutDescriptorCount(DescriptorSetLayoutHandle descriptorSetLayoutHandle) const;

		TextureHandle getOutputBackBuffer(OutputHandle outputHandle, uint32 backBufferIndex) const;
		uint32 getOutputCurrentBackBufferIndex(OutputHandle outputHandle) const;

		//void setObjectDebugName(uint32 objectHandle, const char* name);
		//const char* getObjectDebugName() const;

		const char* getName() const;
	};

	// void EnumeratePhysicalDevices(uint32& physicalDeviceCount, PhysicalDevice* physicalDevices);
	// EnumerateDisplays
	// EnumerateVRs

	uint16x3 CalculateMipLevelSize(uint16x3 srcSize, uint8 mipLevel);
}
