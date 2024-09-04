#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Gfx.HAL.Shared.h>

// TODO: Validate used vs set vertex buffers when rendering.
// TODO: Validate that viewport and scissor are set when rendering.
// TODO: Validate that scissor rect is not larget than render target.
// TODO: Validate color/depth-stencil RT formats when rendering.
// TODO: Probably we can state that Texture2D is equivalent to Texture2DArray[1].
// TODO: Handle should be 18+10+4 or 17+11+4 instead of 20+8+4.
// TODO: Implement "host visible" memory/resources via `D3D12_HEAP_TYPE_GPU_UPLOAD`.
// TODO: Move bindings from `Device::DescriptorSetLayout` and `Device::PipelineLayout` to device internal heap (TLSF probably).
// TODO: Look into `minImageTransferGranularity`. This is additional constraint on CLs we submit to copy queue.
// TODO: Do something about optimized clear values passed on resource creation. Warning suppressed for now.
// TODO: `TextureLayout::Commom` allows shader write access (D3D12 UAV), but D3D12 spec says that this is only allowed using a "queue-specific" common layout. We should revisit this when implementing multi-queue support (including vulkan specifics).
// TODO: `TextureLayout::Commom` is equivalent to `D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON`, so only gfx queue is supported for now.
// TODO: We may have to introduce version `writeDescriptorSet` that handles multiple writes so we do not have to resolve `DescriptorSet` value multiple times.
// TODO: "Non-owning" handles? (handles that can't be used to destroy object).
// TODO: Introduce safe "owning"/"strong" versions of handles that assert zero value on destruction.
// TODO: In Vulkan to create dtor you need to provide image layout explicitly. In our case for example texture access in `TextureLayout::Common` and `TextureLayout::ShaderReadOnly` will need different dtors.
// TODO: We may try to introduce `HAL::ResourceDesc` (U64, that also encodes resource type), `HAL::Device::createResource`, `HAL::ResourceHandle`. These can be parallel to previous system with separate buffer/texture. This will help to cleanup mess in Gfx.Scheduler.
// TODO: Encode `TextureDesc` into single U64 by hand.
// TODO: Try using "CompositeHeapAllocation" concept.
// TODO: Fast clear value handling. For now we just suppressed `D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE` and `D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE`.


// NOTE: Virtual textures do not support compression (CMASK/DCC) on some architectures.

#define XEAssert(cond) XAssert(cond)
#define XEAssertUnreachableCode XAssertUnreachableCode
#define XEMasterAssert(cond) XAssert(cond)
#define XEMasterAssertUnreachableCode XAssertUnreachableCode
#define XEShitHitTheFan XAssertUnreachableCode

struct ID3D12Device10;
struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList10;

namespace XEngine::Gfx::HAL
{
	constexpr uint32 ConstantBufferSizeLimit = 64 * 1024;
	constexpr uint16 ConstantBufferBindAlignmentLog2 = 8;
	constexpr uint16 ConstantBufferBindAlignment = 1 << ConstantBufferBindAlignmentLog2;
	constexpr uint16 BufferPlacedTextureDataAlignment = 512;
	constexpr uint16 BufferPlacedTextureRowPitchAlignment = 256;
	constexpr uint16 TextureArraySizeLimit = 2048;
	constexpr uint32 OutputBackBufferCount = 2; // TODO: I do not like this name. Replace "back buffer" with something...

	class Device;

	enum class CommandAllocatorHandle		: uint32 {};
	enum class DescriptorAllocatorHandle	: uint32 {};
	enum class DeviceMemoryHandle			: uint32 {};
	enum class BufferHandle					: uint32 {};
	enum class TextureHandle				: uint32 {};
	enum class DescriptorSetLayoutHandle	: uint32 {};
	enum class PipelineLayoutHandle			: uint32 {}; // PipelineBindingLayoutHandle?
	enum class ShaderHandle					: uint32 {};
	enum class GraphicsPipelineHandle		: uint32 {};
	//enum class RaytracingPipelineHandle		: uint32 {};
	enum class OutputHandle					: uint32 {};

	enum class DescriptorSet				: uint64 {}; // TODO: This should be `DescriptorSetRef`
	enum class DeviceQueueSyncPoint			: uint64 {};
	enum class HostSignalToken				: uint64 {};

	enum class DeviceQueue : uint8 // Also used as queue index, so should start from 0.
	{
		Graphics = 0,
		Compute,
		Copy,
		ValueCount,
	};

	static constexpr uint8 DeviceQueueCount = uint8(DeviceQueue::ValueCount);

	enum class ResourceType : uint8
	{
		Undefined = 0,
		Buffer,
		Texture,
	};

	enum class ResourceAlignmentRequirement : uint8
	{
		Undefined = 0,
		_4kib,
		_64kib,
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

	struct ResourceMemoryRequirements
	{
		uint64 size;
		ResourceAlignmentRequirement alignment;
	};

	struct TextureDesc
	{
		// U64 encoding
		// NOTE: We may consider 0 dim as if it has leading 1. This way we can store 16384 in 14 bits instead of 15.
		// width : 14 (max value 16384)
		// height : 14 (max value 16384)
		// depth : 12 (max value 2048)
		// dimension : 4
		// format : 6
		// mipLevelCount : 4
		// -----------------
		// 54 bits total

		uint16x3 size;
		TextureDimension dimension : 2; // TODO: Calculate these bits properly.
		TextureFormat format : 6;
		uint8 mipLevelCount : 4;
		bool enableRenderTargetUsage : 1;

		static inline TextureDesc Create2D(uint16 width, uint16 height, TextureFormat format, uint8 mipLevelCount);
		static inline TextureDesc Create2DRT(uint16 width, uint16 height, TextureFormat format, uint8 mipLevelCount);
	};
	static_assert(sizeof(TextureDesc) == 8);

	enum class ResourceViewType : uint8
	{
		Undefined = 0,
		Buffer,
		Texture,
		TextureCube,
		RaytracingAccelerationStructure,
	};

	struct ResourceView
	{
		union
		{
			uint32 resourceHandle;
			BufferHandle bufferHandle;
			TextureHandle textureHandle;
		};

		union
		{
			struct
			{
				TexelViewFormat format;
				bool writable;
			} buffer;

			struct
			{
				TexelViewFormat format;
				bool writable;
				uint8 baseMipLevel;
				uint8 mipLevelCount;
				// TextureAspects aspects;
			} texture;

			// ...
		};

		ResourceViewType type;
	};

	enum class PipelineType : uint8
	{
		Undefined = 0,
		Graphics,
		Compute,
		//Raytracing,
	};

	enum class CommandListType : uint8
	{
		Undefined = 0,
		Graphics,
		Compute,
		Copy,
	};

	enum class VertexAttributeIndexMode : uint8
	{
		VertexID = 0,
		InstanceID,
	};

	struct VertexAttribute
	{
		const char* name;
		uint16 offset;
		TexelViewFormat format;
		uint8 bufferIndex;
		VertexAttributeIndexMode indexMode;
	};

	enum class RasterizerFillMode : uint8
	{
		Solid = 0,
		Wireframe,
	};

	enum class RasterizerCullMode : uint8
	{
		None = 0,
		Front,
		Back,
	};

	struct RasterizerState
	{
		RasterizerFillMode fillMode;
		RasterizerCullMode cullMode;
		// TODO: ...
	};

	enum class ComparisonFunc : uint8
	{
		Never = 0,
		Always,
		Equal,
		NotEqual,
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
	};

	struct DepthStencilState
	{
		bool depthReadEnable;
		bool depthWriteEnable;
		ComparisonFunc depthComparisonFunc;
		// TODO: Stencil ...
	};

	enum class BlendFactor : uint8
	{
		Zero = 0,
		One,
		SrcColor,
		OneMinusSrcColor,
		DstColor,
		OneMinusDstColor,
		SrcAlpha,
		OneMinusSrcAlpha,
		DstAlpha,
		OneMinusDstAlpha,
		ConstantColor,
		OneMinusConstantColor,
		ConstantAlpha,
		OneMinusConstantAlpha,
	};

	enum class BlendOp : uint8
	{
		Add = 0,
		Substract,
		ReverseSubstract,
		Min,
		Max,
	};

	struct ColorBlendState
	{
		struct RenderTarget
		{
			BlendFactor srcColorBlendFactor : 5;
			BlendFactor dstColorBlendFactor : 5;
			BlendOp colorBlendOp : 3;
			BlendFactor scrAlphaBlendFactor : 5;
			BlendFactor dstAlphaBlendFactor : 5;
			BlendOp alphaBlendOp : 3;

			bool blendEnable : 1;
			bool writeR : 1;
			bool writeG : 1;
			bool writeB : 1;
			bool writeA : 1;
		};

		RenderTarget renderTargets[MaxColorRenderTargetCount];
	};

	struct GraphicsPipelineDesc
	{
		const VertexAttribute* vertexAttributes;
		uint8 vertexAttributeCount;

		ShaderHandle vsHandle;
		ShaderHandle asHandle;
		ShaderHandle msHandle;
		ShaderHandle psHandle;

		TexelViewFormat colorRenderTargetFormats[MaxColorRenderTargetCount];
		DepthStencilFormat depthStencilRenderTargetFormat;

		RasterizerState rasterizerState;
		DepthStencilState depthStencilState;
		ColorBlendState colorBlendState;
	};

	struct ColorRenderTarget
	{
		TextureHandle texture;
		TexelViewFormat format;
		uint8 mipLevel;
		uint16 arrayIndex;

		static inline ColorRenderTarget Create(TextureHandle texture, TexelViewFormat format, uint8 mipLevel = 0, uint16 arrayIndex = 0);
	};

	struct DepthStencilRenderTarget
	{
		TextureHandle texture;
		uint8 mipLevel;
		uint16 arrayIndex;

		static inline DepthStencilRenderTarget Create(TextureHandle texture, uint8 mipLevel = 0, uint16 arrayIndex = 0);
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
		ComputeShader							= 0x0002,
		PrePixelShaders							= 0x0004,
		PixelShader								= 0x0008,
		ColorRenderTarget						= 0x0010,
		DepthStencilRenderTarget				= 0x0020,
		RaytracingAccelerationStructureBuild	= 0x0040,
		RaytracingAccelerationStructureCopy		= 0x0080,
		// Resolve,
		// IndirectArgument,

		AllShaders = ComputeShader | PrePixelShaders | PixelShader,
		AllGraphicsShaders = PrePixelShaders | PixelShader,
		AllGraphics = PrePixelShaders | PixelShader | ColorRenderTarget | DepthStencilRenderTarget,
		All = Copy | ComputeShader | AllGraphics | RaytracingAccelerationStructureBuild | RaytracingAccelerationStructureCopy,
	};

	XDefineEnumFlagOperators(BarrierSync, uint16)

	enum class BarrierAccess : uint16
	{
		None = 0,
		Any										= 0x0001, // This flag can't be combined with others.
		CopySource								= 0x0002,
		CopyDest								= 0x0004,
		VertexOrIndexBuffer						= 0x0008,
		ConstantBuffer							= 0x0010,
		ShaderReadOnly							= 0x0020,
		ShaderReadWrite							= 0x0040,
		ColorRenderTarget						= 0x0080,
		DepthStencilRenderTarget				= 0x0100,
		DepthStencilRenderTargetReadOnly		= 0x0200,
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
		Common, // Copy source&dest, shader read&write (queue specific)
		ShaderReadOnly,
		ShaderReadWrite, // It seems that Vulkan does not have equivalent and VK_IMAGE_LAYOUT_GENERAL is the only option.
		ColorRenderTarget,
		DepthStencilRenderTarget,
		DepthStencilRenderTargetReadOnly,
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
		ID3D12GraphicsCommandList10* d3dCommandList = nullptr;
		uint32 deviceCommandListHandle = 0;
		CommandAllocatorHandle commandAllocatorHandle = {};

		uint16 rtvHeapOffset = 0;
		uint16 dsvHeapOffset = 0;

		CommandListType type = {};
		bool isOpen = false;

		PipelineLayoutHandle currentPipelineLayoutHandle = {};
		PipelineType currentPipelineType = {};
		uint8 setColorRenderTargetCount = 0;
		bool isDepthStencilRenderTargetSet = false;

	private:
		void cleanup();

	public:
		CommandList() = default;
		~CommandList();

		void setPipelineType(PipelineType pipelineType);
		void setPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle);
		void setComputePipeline(ShaderHandle computeShaderHandle);
		void setGraphicsPipeline(GraphicsPipelineHandle graphicsPipelineHandle);
		//void setRaytracingPipeline(RaytracingPipelineHandle raytracingPipelineHandle);

		void setViewport(float32 left, float32 top, float32 right, float32 bottom, float32 minDepth = 0.0f, float32 maxDepth = 1.0f);
		void setScissor(uint32 left, uint32 top, uint32 right, uint32 bottom);

		void bindRenderTargets(uint8 colorRenderTargetCount, const ColorRenderTarget* colorRenderTargets,
			const DepthStencilRenderTarget* depthStencilRenderTarget = nullptr, bool readOnlyDepth = false, bool readOnlyStencil = false);

		inline void bindRenderTargets(const ColorRenderTarget& colorRenderTarget);
		inline void bindRenderTargets(const ColorRenderTarget& colorRenderTarget,
			const DepthStencilRenderTarget& depthStencilRenderTarget, bool readOnlyDepth = false, bool readOnlyStencil = false);

		void bindIndexBuffer(BufferPointer bufferPointer, IndexBufferFormat format, uint32 byteSize);
		void bindVertexBuffer(uint8 bufferIndex, BufferPointer bufferPointer, uint16 stride, uint32 byteSize);

		void bindConstants(uint64 bindingNameXSH, const void* data, uint32 size32bitValues, uint32 offset32bitValues = 0);
		void bindBuffer(uint64 bindingNameXSH, BufferBindType bindType, BufferPointer bufferPointer);
		void bindDescriptorSet(uint64 bindingNameXSH, DescriptorSet descriptorSet);
		//void bindDescriptorArray(uint64 bindingNameXSH, ...);

		inline void bindConstantBuffer(uint64 bindingNameXSH, BufferPointer bufferPointer);

		void clearColorRenderTarget(uint8 colorRenderTargetIndex, const float32* color);
		void clearDepthStencilRenderTarget(bool clearDepth, bool clearStencil, float32 depth, uint8 stencil);

		void draw(uint32 vertexCount, uint32 vertexOffset = 0);
		void drawIndexed(uint32 indexCount, uint32 indexOffset = 0, uint32 vertexOffset = 0);
		void dispatchMesh();
		void dispatch(uint32 groupCountX, uint32 groupCountY = 1, uint32 groupCountZ = 1);

		void globalMemoryBarrier(
			BarrierSync syncBefore, BarrierSync syncAfter,
			BarrierAccess accessBefore, BarrierAccess accessAfter);
		void bufferMemoryBarrier(BufferHandle bufferHandle,
			BarrierSync syncBefore, BarrierSync syncAfter,
			BarrierAccess accessBefore, BarrierAccess accessAfter);
		void textureMemoryBarrier(TextureHandle textureHandle,
			BarrierSync syncBefore, BarrierSync syncAfter,
			BarrierAccess accessBefore, BarrierAccess accessAfter,
			TextureLayout layoutBefore, TextureLayout layoutAfter,
			const TextureSubresourceRange* subresourceRange = nullptr);

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


	struct DeviceSettings
	{
		uint16 maxCommandAllocatorCount;
		uint16 maxDescriptorAllocatorCount;
		uint16 maxCommandListCount;
		uint16 maxMemoryAllocationCount;
		uint16 maxResourceCount;
		uint16 maxDescriptorSetLayoutCount;
		uint16 maxPipelineLayoutCount;
		uint16 maxShaderCount;
		uint16 maxCompositePipelineCount;
		uint16 maxOutputCount;

		uint32 bindlessDescriptorPoolSize;
	};

	static constexpr DeviceSettings DefautlDeviceSettings =
	{
		.maxCommandAllocatorCount = 16,
		.maxDescriptorAllocatorCount = 16,
		.maxCommandListCount = 16,
		.maxMemoryAllocationCount = 1024,
		.maxResourceCount = 1024 * 4,
		.maxDescriptorSetLayoutCount = 256,
		.maxPipelineLayoutCount = 256,
		.maxShaderCount = 1024 * 4,
		.maxCompositePipelineCount = 1024 * 4,
		.maxOutputCount = 8,
		.bindlessDescriptorPoolSize = 1024 * 64,
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
		enum class CompositePipelineType : uint8;

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

			inline EntryType& getEntryByIndex(uint16 entryIndex);
			inline const EntryType& getEntryByIndex(uint16 entryIndex) const;
			inline bool isEntryAllocated(uint16 entryIndex) const;

			inline uint16 getCapacity() const { return capacity; }
			inline uint16 getAllocatedEntryCount() const { return committedEntryCount - freelistLength; }

			static inline uint8 GetHandleGeneration(uint32 handle);
			static inline uint16 GetHandleEntryIndex(uint32 handle);
			static inline uint32 ComposeHandle(uint16 entryIndex, uint8 handleGeneration);
		};

		struct PoolEntryBase;

		struct CommandAllocator;
		struct DescriptorAllocator;
		struct CommandList;
		struct MemoryAllocation;
		struct Resource;
		struct DescriptorSetLayout;
		struct PipelineLayout;
		struct Shader;
		struct CompositePipeline;
		struct Output;

		static constexpr uint32 DescriptorAllocatorChunkSize = 1024;

	private:
		ID3D12Device10* d3dDevice = nullptr;

		Queue queues[DeviceQueueCount] = {};

		ID3D12DescriptorHeap* d3dShaderVisbileSRVHeap = nullptr;
		ID3D12DescriptorHeap* d3dRTVHeap = nullptr;
		ID3D12DescriptorHeap* d3dDSVHeap = nullptr;

		uint64 shaderVisbileSRVHeapCPUPtr = 0;
		uint64 shaderVisbileSRVHeapGPUPtr = 0;
		uint64 rtvHeapPtr = 0;
		uint64 dsvHeapPtr = 0;

		uint16 srvDescriptorSize = 0;
		uint16 rtvDescriptorSize = 0;
		uint16 dsvDescriptorSize = 0;

		Pool<CommandAllocator> commandAllocatorPool;
		Pool<DescriptorAllocator> descriptorAllocatorPool;
		Pool<CommandList> commandListPool;
		Pool<MemoryAllocation> memoryAllocationPool;
		Pool<Resource> resourcePool;
		Pool<DescriptorSetLayout> descriptorSetLayoutPool;
		Pool<PipelineLayout> pipelineLayoutPool;
		Pool<Shader> shaderPool;
		Pool<CompositePipeline> compositePipelinePool;
		Pool<Output> outputPool;

		uint32 bindlessDescriptorPoolSize = 0;

	private:
		static consteval uint8 GetDeviceQueueSyncPointSignalValueBitCount();
		static inline DeviceQueueSyncPoint ComposeDeviceQueueSyncPoint(uint8 queueIndex, uint64 signalValue);
		static inline void DecomposeDeviceQueueSyncPoint(DeviceQueueSyncPoint syncPoint, uint8& outQueueIndex, uint64& outSignalValue);

		static uint32 CalculateTextureSubresourceIndex(const TextureDesc& textureDesc, const TextureSubresource& textureSubresource);

	private:
		inline DescriptorSet composeDescriptorSetReference(DescriptorSetLayoutHandle descriptorSetLayoutHandle,
			uint32 baseDescriptorIndex, uint16 descriptorAllocatorIndex, uint8 descriptorAllocatorResetCounter);
		inline void decomposeDescriptorSetReference(DescriptorSet descriptorSet,
			const DescriptorSetLayout*& outDescriptorSetLayout, uint32& outBaseDescriptorIndex) const;

		void updateCommandAllocatorExecutionStatus(CommandAllocator& commandAllocator);
		void writeDescriptor(uint32 descriptorIndex, const ResourceView& resourceView);

		inline bool isDeviceSignalValueReached(const Queue& queue, uint64 signalValue, uint8 signalValueBitCount) const;

	public:
		Device() = default;
		~Device() = default;

		void initialize(/*const PhysicalDevice& physicalDevice, */const DeviceSettings& settings = DefautlDeviceSettings);

		CommandAllocatorHandle createCommandAllocator();
		void destroyCommandAllocator(CommandAllocatorHandle commandAllocatorHandle);

		DescriptorAllocatorHandle createDescriptorAllocator();
		void destroyDescriptorAllocator(DescriptorAllocatorHandle descriptorAllocatorHandle);

		DeviceMemoryHandle allocateDeviceMemory(uint64 size, bool hostVisible = false);
		void releaseDeviceMemory(DeviceMemoryHandle memoryHandle);

		ResourceMemoryRequirements getTextureMemoryRequirements(const TextureDesc& desc) const;

		BufferHandle createBuffer(uint64 size,
			DeviceMemoryHandle memoryHandle = {}, uint64 memoryOffset = 0);
		TextureHandle createTexture(const TextureDesc& desc, TextureLayout initialLayout,
			DeviceMemoryHandle memoryHandle = {}, uint64 memoryOffset = 0);

		BufferHandle createVirtualBuffer(uint64 size);
		TextureHandle createVirtualTexture(const TextureDesc& textureDesc);

		BufferHandle createStagingBuffer(uint64 size, StagingBufferAccessMode accessMode);

		void destroyBuffer(BufferHandle bufferHandle);
		void destroyTexture(TextureHandle textureHandle);

		DescriptorSetLayoutHandle createDescriptorSetLayout(const void* blobData, uint32 blobSize);
		void destroyDescriptorSetLayout(DescriptorSetLayoutHandle descriptorSetLayoutHandle);

		PipelineLayoutHandle createPipelineLayout(const void* blobData, uint32 blobSize);
		void destroyPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle);

		ShaderHandle createShader(PipelineLayoutHandle pipelineLayoutHandle, const void* blobData, uint32 blobSize);
		void destroyShader(ShaderHandle shaderHandle);

		GraphicsPipelineHandle createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle, const GraphicsPipelineDesc& desc);
		void destroyGraphicsPipeline(GraphicsPipelineHandle graphicsPipelineHandle);

		// RaytracingPipelineHandle createRaytracingPipeline(...);
		// void destroyRaytracingPipeline(RaytracingPipelineHandle raytracingPipelineHandle);

		OutputHandle createWindowOutput(uint16 width, uint16 height, void* platformWindowHandle);
		// OutputHandle createDisplayOutput();
		// OutputHandle createVROutput();
		void destroyOutput(OutputHandle outputHandle);

		DescriptorSet allocateDescriptorSet(DescriptorAllocatorHandle descriptorAllocatorHandle, DescriptorSetLayoutHandle descriptorSetLayoutHandle);

		void writeDescriptorSet(DescriptorSet descriptorSet, uint64 bindingNameXSH, const ResourceView& resourceView);
		void writeBindlessDescriptor(uint32 bindlessDescriptorIndex, const ResourceView& resourceView);

		void openCommandList(HAL::CommandList& commandList, CommandAllocatorHandle commandAllocatorHandle, CommandListType type = CommandListType::Graphics);
		void closeCommandList(HAL::CommandList& commandList);
		void discardCommandList(HAL::CommandList& commandList);

		void resetCommandAllocator(CommandAllocatorHandle commandAllocatorHandle);
		void resetDescriptorAllocator(DescriptorAllocatorHandle descriptorAllocatorHandle);

		void submitCommandList(DeviceQueue deviceQueue, HAL::CommandList& commandList);
		void submitOutputFlip(DeviceQueue deviceQueue, OutputHandle outputHandle);
		//void submitVirtualResourceMappingUpdate(DeviceQueue deviceQueue, );
		void submitSyncPointWait(DeviceQueue deviceQueue, DeviceQueueSyncPoint syncPoint);
		HostSignalToken submitHostSignalWait(DeviceQueue deviceQueue);

		DeviceQueueSyncPoint getEOPSyncPoint(DeviceQueue deviceQueue) const;
		bool isQueueSyncPointReached(DeviceQueueSyncPoint syncPoint) const;
		void emitHostSignal(HostSignalToken signalToken);

		TextureDesc getTextureDesc(TextureHandle textureHandle) const;
		void* getMappedBufferPtr(BufferHandle bufferHandle) const;
		uint16 getDescriptorSetLayoutDescriptorCount(DescriptorSetLayoutHandle descriptorSetLayoutHandle) const;

		TextureHandle getOutputBackBuffer(OutputHandle outputHandle, uint32 backBufferIndex) const;
		TextureHandle getOutputCurrentBackBuffer(OutputHandle outputHandle) const;
		uint32 getOutputCurrentBackBufferIndex(OutputHandle outputHandle) const;

		//void setObjectDebugName(uint32 objectHandle, const char* name);
		//const char* getObjectDebugName() const;

		const char* getName() const;
	};

	// void EnumeratePhysicalDevices(uint32& physicalDeviceCount, PhysicalDevice* physicalDevices);
	// EnumerateDisplays
	// EnumerateVRs

	uint16x3 CalculateMipLevelSize(uint16x3 srcSize, uint8 mipLevel);

	class BarrierAccessUtils abstract final
	{
	private:
		static constexpr BarrierAccess ReadOnlyAccessMask =
			BarrierAccess::CopySource |
			BarrierAccess::VertexOrIndexBuffer |
			BarrierAccess::ConstantBuffer |
			BarrierAccess::ShaderReadOnly |
			BarrierAccess::DepthStencilRenderTargetReadOnly |
			BarrierAccess::RaytracingAccelerationStructureRead;

	public:
		static inline bool IsReadOnly(BarrierAccess access) { return (access & ~ReadOnlyAccessMask) == BarrierAccess(0); }
		static inline bool IsBufferCompatible(BarrierAccess access);
		static inline bool IsTextureCompatible(BarrierAccess access);
		static inline bool IsCompatibleWithTextureLayout(BarrierAccess access, TextureLayout layout);
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// INLINE DEFINITIONS //////////////////////////////////////////////////////////////////////////////

namespace XEngine::Gfx::HAL
{
	inline TextureDesc TextureDesc::Create2D(uint16 width, uint16 height, TextureFormat format, uint8 mipLevelCount)
	{
		TextureDesc desc = {};
		desc.size = uint16x3(width, height, 1);
		desc.dimension = TextureDimension::Texture2D;
		desc.format = format;
		desc.mipLevelCount = mipLevelCount;
		desc.enableRenderTargetUsage = false;
		return desc;
	}

	inline TextureDesc TextureDesc::Create2DRT(uint16 width, uint16 height, TextureFormat format, uint8 mipLevelCount)
	{
		TextureDesc desc = {};
		desc.size = uint16x3(width, height, 1);
		desc.dimension = TextureDimension::Texture2D;
		desc.format = format;
		desc.mipLevelCount = mipLevelCount;
		desc.enableRenderTargetUsage = true;
		return desc;
	}

	inline ColorRenderTarget ColorRenderTarget::Create(TextureHandle texture, TexelViewFormat format, uint8 mipLevel, uint16 arrayIndex)
	{
		ColorRenderTarget rt = {};
		rt.texture = texture;
		rt.format = format;
		rt.mipLevel = mipLevel;
		rt.arrayIndex = arrayIndex;
		return rt;
	}

	inline DepthStencilRenderTarget DepthStencilRenderTarget::Create(TextureHandle texture, uint8 mipLevel, uint16 arrayIndex)
	{
		DepthStencilRenderTarget rt = {};
		rt.texture = texture;
		rt.mipLevel = mipLevel;
		rt.arrayIndex = arrayIndex;
		return rt;
	}

	inline void CommandList::bindRenderTargets(const ColorRenderTarget& colorRenderTarget)
	{
		bindRenderTargets(1, &colorRenderTarget);
	}

	inline void CommandList::bindRenderTargets(const ColorRenderTarget& colorRenderTarget,
		const DepthStencilRenderTarget& depthStencilRenderTarget, bool readOnlyDepth, bool readOnlyStencil)
	{
		bindRenderTargets(1, &colorRenderTarget, &depthStencilRenderTarget, readOnlyDepth, readOnlyStencil);
	}

	inline void CommandList::bindConstantBuffer(uint64 bindingNameXSH, BufferPointer bufferPointer)
	{
		bindBuffer(bindingNameXSH, BufferBindType::Constant, bufferPointer);
	}
}