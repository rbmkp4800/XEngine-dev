#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>
#include <XLib.Containers.BitSet.h>

#include <XEngine.Render.HAL.Common.h>

#define XEAssert(cond) XAssert(cond)
#define XEAssertUnreachableCode() XAssertUnreachableCode()
#define XEMasterAssert(cond) XAssert(cond)
#define XEMasterAssertUnreachableCode() XAssertUnreachableCode()

struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12Device2;
struct ID3D12GraphicsCommandList;
struct IDXGIAdapter4;

namespace XEngine::Render::HAL
{
	class Device;
	class Host;

	enum class ResourceHandle : uint32;
	enum class ShaderResourceViewHandle : uint32;
	enum class RenderTargetViewHandle : uint32;
	enum class DepthStencilViewHandle : uint32;
	enum class DescriptorTableLayoutHandle : uint32;
	enum class DescriptorTableHandle : uint32;
	enum class PipelineLayoutHandle : uint32;
	enum class PipelineHandle : uint32;
	enum class FenceHandle : uint32;
	enum class SwapChainHandle : uint32;

	//enum class PipelineBindPointId : uint32;

	using DescriptorAddress = uint32;

	static constexpr ResourceHandle ZeroResourceHandle = ResourceHandle(0);
	static constexpr ShaderResourceViewHandle ZeroShaderResourceViewHandle = ShaderResourceViewHandle(0);
	static constexpr RenderTargetViewHandle ZeroRenderTargetViewHandle = RenderTargetViewHandle(0);
	static constexpr DepthStencilViewHandle ZeroDepthStencilViewHandle = DepthStencilViewHandle(0);
	static constexpr PipelineLayoutHandle ZeroPipelineLayoutHandle = PipelineLayoutHandle(0);

	enum class ResourceType : uint8
	{
		Undefined = 0,
		Buffer,
		Texture,
	};

	enum class BufferMemoryType : uint8
	{
		Undefined = 0,
		Local,
		Upload,
		Readback,
	};

	struct BufferCreationFlags
	{
		bool allowShaderWrite : 1;
	};

	enum class TextureType : uint8
	{
		Undefined = 0,
		Texture1D,
		Texture2D,
		Texture2DArray,
		Texture3D,
	};

	struct TextureCreationFlags
	{
		bool allowRenderTarget : 1;
		bool allowDepthStencil : 1;
		bool allowShaderWrite : 1;
	};

	enum class ShaderResourceViewType : uint8
	{
		Undefined = 0,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		//ReadOnlyTexelBuffer,
		//ReadWriteTexelBuffer,
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

	struct ResourceImmutableState
	{
		bool vertexBuffer : 1;
		bool indexBuffer : 1;
		bool constantBuffer : 1;
		bool pixelShaderRead : 1;
		bool nonPixelShaderRead : 1;
		bool depthRead : 1;
		bool indirectArgument : 1;
		bool copySource : 1;
	};

	enum class ResourceMutableState : uint8
	{
		None = 0,
		RenderTarget,
		DepthWrite,
		ShaderReadWrite,
		CopyDestination,
	};

	class ResourceState
	{
		static_assert(sizeof(ResourceImmutableState) <= sizeof(uint16));
		static_assert(sizeof(ResourceMutableState) <= sizeof(uint16));

	private:
		static constexpr uint16 MutableFlag = 0x8000;

	private:
		union
		{
			uint16 rawState = 0;
			ResourceImmutableState immutableState;
			ResourceMutableState mutableState;
		};

	public:
		ResourceState() = default;

		inline ResourceState(ResourceImmutableState immutableState);
		inline ResourceState(ResourceMutableState mutableState);

		inline bool isMutable() const { return (rawState & MutableFlag) != 0; }
		inline ResourceImmutableState getImmutable() const { XEAssert(!isMutable()); return immutableState; }
		inline ResourceMutableState getMutable() const { XEAssert(isMutable()); return mutableState; }
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

	struct ShaderResourceViewDesc
	{
		ShaderResourceViewType type;
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

	struct ObjectDataView
	{
		const void* data;
		uint32 size;
	};

	struct RasterizerDesc
	{

	};

	struct DepthStencilDesc
	{
		bool enableDepthRead;
		bool enableDepthWrite;
		// depthComparisonFunc;
	};

	struct BlendDesc
	{

	};

	struct Viewport
	{
		float32 top;
		float32 left;
		float32 right;
		float32 bottom;
		float32 depthMin;
		float32 depthMax;
	};

	struct FenceSignalDesc
	{
		FenceHandle fenceHandle;
		uint64 value;
	};

	//namespace Internal
	//{
	//	struct PipelineBindPointsLUTEntry;
	//}

	class CommandList : public XLib::NonCopyable
	{
		friend Device;

	private:
		enum class State : uint8;

	private:
		Device* device = nullptr;
		ID3D12GraphicsCommandList* d3dCommandList = nullptr;
		ID3D12CommandAllocator* d3dCommandAllocator = nullptr;
		CommandListType type = CommandListType::Undefined;

		State state = State(0);
		PipelineType currentPipelineType = PipelineType::Undefined;
		PipelineLayoutHandle currentPipelineLayoutHandle = ZeroPipelineLayoutHandle;
		//Internal::PipelineBindPointsLUTEntry* pipelineBindPointsLUTShortcut = nullptr;

	private:
		//inline Internal::PipelineBindPointsLUTEntry lookupBindPointsLUT(uint32 bindPointNameCRC) const;

	public:
		CommandList() = default;
		~CommandList();

		void begin();

		void clearRenderTarget(RenderTargetViewHandle rtv, const float32* color);
		void clearDepthStencil(DepthStencilViewHandle dsv, bool clearDepth, bool clearStencil, float32 depth, uint8 stencil);

		void setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv = ZeroDepthStencilViewHandle);
		void setViewport(const Viewport& viewport);
		void setScissor();

		inline void setRenderTarget(RenderTargetViewHandle rtv, DepthStencilViewHandle dsv = ZeroDepthStencilViewHandle) { setRenderTargets(1, &rtv, dsv); }

		void setPipelineType(PipelineType pipelineType);
		void setPipelineLayout(PipelineLayoutHandle pipelineLayoutHandle);
		void setPipeline(PipelineHandle pipelineHandle);

		//void bindConstants(PipelineBindPointId bindPointId, const void* data, uint32 size32bitValues, uint32 offset32bitValues = 0);

		void bindConstants(uint32 bindPointNameCRC, const void* data, uint32 size32bitValues, uint32 offset32bitValues = 0);
		void bindBuffer(uint32 bindPointNameCRC, BufferBindType bindType, ResourceHandle bufferHandle, uint32 offset = 0);
		void bindDescriptor(uint32 bindPointNameCRC, DescriptorAddress address);
		void bindDescriptorTable(uint32 bindPointNameCRC, DescriptorTableHandle bundleHandle);
		void bindDescriptorArray(uint32 bindPointNameCRC, DescriptorAddress arrayStartAddress);

		void drawNonIndexed(uint32 vertexCount, uint32 startVertexIndex = 0);
		void drawIndexed();
		void drawMesh();

		void dispatch(uint32 groupCountX, uint32 groupCountY = 1, uint32 groupCountZ = 1);

		void resourceBarrierStateTransition(ResourceHandle resourceHandle,
			ResourceState stateBefore, ResourceState stateAfter,
			const TextureSubresource* textureSubresource = nullptr);
		void resourceBarrierReadWrite(ResourceHandle resourceHandle, const TextureSubresource* textureSubresource = nullptr);

		void copyBuffer(ResourceHandle dstBufferHandle, uint64 dstOffset, ResourceHandle srcBufferHandle, uint64 srcOffset, uint64 size);
		void copyTexture(ResourceHandle dstTextureHandle, TextureSubresource dstSubresource, uint16x3 dstOffset,
			ResourceHandle srcTextureHandle, TextureSubresource srcSubresource, const TextureRegion* srcRegion = nullptr);
		void copyBufferTexture(CopyBufferTextureDirection direction,
			ResourceHandle bufferHandle, uint64 bufferOffset, uint32 bufferRowPitch,
			ResourceHandle textureHandle, TextureSubresource textureSubresource, const TextureRegion* textureRegion = nullptr);
	};

	class Device : public XLib::NonCopyable
	{
		friend CommandList;
		friend Host;

	private:
		static constexpr uint32 MaxResourceCount = 1024;
		static constexpr uint32 MaxShaderResourceViewCount = 1024;
		static constexpr uint32 MaxResourceDescriptorCount = 4096;
		static constexpr uint32 MaxRenderTargetViewCount = 64;
		static constexpr uint32 MaxDepthStencilViewCount = 64;
		static constexpr uint32 MaxPipelineLayoutCount = 64;
		static constexpr uint32 MaxPipelineCount = 1024;
		static constexpr uint32 MaxFenceCount = 64;
		static constexpr uint32 MaxSwapChainCount = 4;
		static constexpr uint32 SwapChainTextureCount = 2;

		struct Resource;
		struct ShaderResourceView;
		struct PipelineLayout;
		struct Pipeline;
		struct Fence;
		struct SwapChain;

	private:
		XLib::Platform::COMPtr<ID3D12Device2> d3dDevice;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dReferenceSRVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dShaderVisbileSRVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dRTVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dDSVHeap;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dGraphicsQueue;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dAsyncComputeQueue;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dAsyncCopyQueue;

		Resource* resourcesTable = nullptr;
		ShaderResourceView* shaderResourceViewsTable = nullptr;
		PipelineLayout* pipelineLayoutsTable = nullptr;
		Pipeline* pipelinesTable = nullptr;
		Fence* fencesTable = nullptr;
		SwapChain* swapChainsTable = nullptr;

		XLib::BitSet<MaxResourceCount> resourcesTableAllocationMask;
		XLib::BitSet<MaxShaderResourceViewCount> shaderResourceViewsTableAllocationMask;
		XLib::BitSet<MaxRenderTargetViewCount> renderTargetViewsTableAllocationMask;
		XLib::BitSet<MaxDepthStencilViewCount> depthStencilViewsTableAllocationMask;
		XLib::BitSet<MaxPipelineLayoutCount> pipelineLayoutsTableAllocationMask;
		XLib::BitSet<MaxPipelineCount> pipelinesTableAllocationMask;
		XLib::BitSet<MaxFenceCount> fencesTableAllocationMask;
		XLib::BitSet<MaxSwapChainCount> swapChainsTableAllocationMask;
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
		inline ResourceHandle composeResourceHandle(uint32 resourceIndex) const;
		inline ShaderResourceViewHandle composeShaderResourceViewHandle(uint32 resourceViewIndex) const;
		inline RenderTargetViewHandle composeRenderTargetViewHandle(uint32 renderTargetIndex) const;
		inline DepthStencilViewHandle composeDepthStencilViewHandle(uint32 depthStencilIndex) const;
		inline FenceHandle composeFenceHandle(uint32 fenceIndex) const;
		inline PipelineLayoutHandle composePipelineLayoutHandle(uint32 pipelineLayoutIndex) const;
		inline PipelineHandle composePipelineHandle(uint32 pipelineIndex) const;
		inline SwapChainHandle composeSwapChainHandle(uint32 swapChainIndex) const;

		inline uint32 resolveResourceHandle(ResourceHandle handle) const;
		inline uint32 resolveShaderResourceViewHandle(ShaderResourceViewHandle handle) const;
		inline uint32 resolveRenderTargetViewHandle(RenderTargetViewHandle handle) const;
		inline uint32 resolveDepthStencilViewHandle(DepthStencilViewHandle handle) const;
		inline uint32 resolvePipelineLayoutHandle(PipelineLayoutHandle handle) const;
		inline uint32 resolvePipelineHandle(PipelineHandle handle) const;
		inline uint32 resolveFenceHandle(FenceHandle handle) const;
		inline uint32 resolveSwapChainHandle(SwapChainHandle handle) const;

		inline DescriptorAddress composeDescriptorAddress(uint32 srvHeapDescriptorIndex) const;
		inline uint32 resolveDescriptorAddress(DescriptorAddress address) const;

		inline Resource& getResourceByHandle(ResourceHandle handle);
		inline ShaderResourceView& getShaderResourceViewByHandle(ShaderResourceViewHandle handle);
		inline PipelineLayout& getPipelineLayoutByHandle(PipelineLayoutHandle handle);
		inline Pipeline& getPipelineByHandle(PipelineHandle handle);
		inline Fence& getFenceByHandle(FenceHandle handle);
		inline const Fence& getFenceByHandle(FenceHandle handle) const;
		inline SwapChain& getSwapChainByHandle(SwapChainHandle handle);
		inline const SwapChain& getSwapChainByHandle(SwapChainHandle handle) const;

	private:
		static uint32 CalculateTextureSubresourceIndex(const Resource& resource, const TextureSubresource& subresource);

	private:
		void initialize(IDXGIAdapter4* dxgiAdapter);

	public:
		Device() = default;
		~Device() = default;

		ResourceHandle createBuffer(uint64 size, BufferMemoryType memoryType, BufferCreationFlags flags);
		void destroyBuffer(ResourceHandle handle);

		ResourceHandle createTexture(TextureType type, uint16x3 size, TextureFormat format, TextureCreationFlags flags = {}, uint8 mipLevelCount = 0);
		void destroyTexture(ResourceHandle handle);

		ShaderResourceViewHandle createShaderResourceView(ResourceHandle resourceHandle, const ShaderResourceViewDesc& viewDesc);
		void destroyShaderResourceView(ShaderResourceViewHandle handle);

		RenderTargetViewHandle createRenderTargetView(ResourceHandle textureHandle);
		void destroyRenderTargetView(RenderTargetViewHandle handle);

		DepthStencilViewHandle createDepthStencilView(ResourceHandle textureHandle);
		void destroyDepthStencilView(DepthStencilViewHandle handle);

		DescriptorAddress allocateDescriptors(uint32 count = 1);
		void releaseDescriptors(DescriptorAddress address);

		DescriptorTableLayoutHandle createDescriptorTableLayout(ObjectDataView objectData);
		void destroyDescriptorTableLayout(DescriptorTableLayoutHandle handle);

		DescriptorTableHandle createDescriptorTable(DescriptorTableLayoutHandle layoutHandle);
		void destroyDescriptorTable(DescriptorTableHandle handle);

		PipelineLayoutHandle createPipelineLayout(ObjectDataView objectData);
		void destroyPipelineLayout(PipelineLayoutHandle handle);

		PipelineHandle createGraphicsPipeline(PipelineLayoutHandle pipelineLayoutHandle,
			ObjectDataView baseObjectData, const ObjectDataView* bytecodeObjectsData, uint8 bytecodeObjectCount,
			const RasterizerDesc& rasterizerDesc, const DepthStencilDesc& depthStencilDesc, const BlendDesc& blendDesc);
		PipelineHandle createComputePipeline(PipelineLayoutHandle pipelineLayoutHandle, ObjectDataView computeShaderObjectData);
		void destroyPipeline(PipelineHandle handle);

		FenceHandle createFence(uint64 initialValue);
		void destroyFence(FenceHandle handle);

		SwapChainHandle createSwapChain(uint16 width, uint16 height, void* hWnd);
		void destroySwapChain(SwapChainHandle handle);

		void createCommandList(CommandList& commandList, CommandListType type);
		void destroyCommandList(CommandList& commandList);

		void writeDescriptor(DescriptorAddress descriptorAddress, ShaderResourceViewHandle srvHandle);
		void writeTableDescriptor(DescriptorTableHandle bundle, uint32 bindPointNameCRC, ShaderResourceViewHandle srvHandle);

		//PipelineBindPointId getPipelineBindPointId(PipelineLayoutHandle pipelineLayoutHandle, uint64 bindPointNameCRC) const;

		void submitGraphics(CommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0);
		void submitAsyncCompute(CommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0);
		void submitAsyncCopy(CommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0);
		void submitFlip(SwapChainHandle swapChainHandle);

		inline void submitGraphics(CommandList& commandList, const FenceSignalDesc& fenceSignal) { submitGraphics(commandList, &fenceSignal, 1); }
		inline void submitAsyncCompute(CommandList& commandList, const FenceSignalDesc& fenceSignal) { submitAsyncCompute(commandList, &fenceSignal, 1); }
		inline void submitAsyncCopy(CommandList& commandList, const FenceSignalDesc& fenceSignal) { submitAsyncCopy(commandList, &fenceSignal, 1); }

		void* getUploadBufferCPUPtr(ResourceHandle uploadBufferHandle);
		DescriptorAddress getDescriptorTableStartAddress(DescriptorTableHandle bundleHandle) const;
		uint64 getFenceValue(FenceHandle fenceHandle) const;
		ResourceHandle getSwapChainTexture(SwapChainHandle swapChainHandle, uint32 textureIndex) const;

		const char* getName() const;

	public:
		static uint16x3 CalculateMipLevelSize(uint16x3 srcSize, uint8 mipLevel);
	};

	class Host abstract final
	{
	public:
		static void CreateDevice(Device& device);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XEngine::Render::HAL
{
	inline ResourceHandle Device::composeResourceHandle(uint32 resourceIndex) const
	{
		XEAssert(resourceIndex < MaxResourceCount);
	}
}
