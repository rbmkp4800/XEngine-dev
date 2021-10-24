#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>
#include <XLib.Containers.BitSet.h>

#define XE_ASSERT(...)
#define XE_ASSERT_UNREACHABLE_CODE
#define XE_MASTER_ASSERT(...)
#define XE_MASTER_ASSERT_UNREACHABLE_CODE

struct ID3D12CommandAllocator;
struct ID3D12DescriptorHeap;
struct ID3D12Device2;
struct ID3D12GraphicsCommandList;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12RootSignature;

struct IDXGIAdapter4;
struct IDXGISwapChain3;

namespace XEngine::Render::HAL
{
	class Device;
	class Host;

	enum class ResourceHandle : uint32;
	enum class ResourceViewHandle : uint32;
	enum class RenderTargetViewHandle : uint32;
	enum class DepthStencilViewHandle : uint32;
	enum class DescriptorBundleLayoutHandle : uint32;
	enum class DescriptorBundleHandle : uint32;
	enum class BindingLayoutHandle : uint32;
	enum class PipelineHandle : uint32;
	enum class FenceHandle : uint32;
	enum class SwapChainHandle : uint32;

	using DescriptorAddress = uint32;

	static constexpr ResourceHandle ZeroResourceHandle = ResourceHandle(0);
	static constexpr ResourceViewHandle ZeroResourceViewHandle = ResourceViewHandle(0);
	static constexpr RenderTargetViewHandle ZeroRenderTargetViewHandle = RenderTargetViewHandle(0);
	static constexpr DepthStencilViewHandle ZeroDepthStencilViewHandle = DepthStencilViewHandle(0);

	enum class ResourceType : uint8
	{
		Undefined = 0,
		Buffer,
		Texture,
	};

	enum class ResourceViewType : uint8
	{
		Undefined = 0,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		ReadOnlyTexture1D,
		ReadWriteTexture1D,
		ReadOnlyTexture2D,
		ReadWriteTexture2D,
		ReadOnlyTexture3D,
		ReadWriteTexture3D,
		ReadOnlyTextureCube,
		RaytracingAccelerationStructure,
	};

	enum class BufferMemoryType : uint8
	{
		Undefined = 0,
		Local,
		Upload,
		Readback,
	};

	enum class TextureDimType : uint8
	{
		Undefined = 0,
		Texture1D,
		Texture2D,
		Texture2DArray,
		Texture3D,
	};

	enum class TextureFormat : uint8
	{
		Undefined = 0,
		R8G8B8A8_UNORM,
	};

	struct TextureFlag abstract final
	{
		enum : uint32
		{
			None = 0,
			AllowRenderTarget	= 0x01,
			AllowDepthStencil	= 0x02,
			AllowShaderWrite	= 0x04,
		};
	};

	struct TextureDim
	{
		TextureDimType type;
		uint16 width;
		uint16 height;
		uint16 depth;
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
				uint8 startMipIndex;
				uint8 mipLevelCount;
				uint8 plane;
			} readOnlyTexture2D;

			struct
			{
				uint8 mipIndex;
				uint8 plane;
			} readWriteTexture2D;
		};
	};

	struct DataBuffer
	{
		const void* data;
		uint32 size;
	};

	struct RasterizerDesc
	{

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

	public:
		CommandList() = default;
		~CommandList();

		void begin();

		void setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv = ZeroDepthStencilViewHandle);
		void setViewport(const Viewport& viewport);
		void setScissor();

		inline void setRenderTarget(RenderTargetViewHandle rtv, DepthStencilViewHandle dsv = ZeroDepthStencilViewHandle) { setRenderTargets(1, &rtv, dsv); }

		void setGraphicsPipeline(PipelineHandle pipelineHandle);
		void setComputePipeline(PipelineHandle pipelineHandle);

		void bindConstants(uint32 bindPointNameCRC, const void* data, uint32 size32bitValues);
		void bindConstantBuffer(uint32 bindPointNameCRC, ResourceHandle bufferHandle, uint32 offset);
		void bindReadOnlyBuffer(uint32 bindPointNameCRC, ResourceHandle bufferHandle, uint32 offset);
		void bindReadWriteBuffer(uint32 bindPointNameCRC, ResourceHandle bufferHandle, uint32 offset);
		void bindDescriptor(uint32 bindPointNameCRC, DescriptorAddress address);
		void bindDescriptorBundle(uint32 bindPointNameCRC, DescriptorBundleHandle bundleHandle);
		void bindDescriptorArray(uint32 bindPointNameCRC, DescriptorAddress arrayStartAddress);

		void drawNonIndexed();
		void drawIndexed();
		void drawMesh();

		void dispatch(uint32 groupCountX, uint32 groupCountY = 1, uint32 groupCountZ = 1);

		void resourceStateTransition(ResourceHandle resourceHandle);

		void copyFromBufferToBuffer();
		void copyFromBufferToTexture();
		void copyFromTextureToTexture();
		void copyFromTextureToBuffer();
	};

	class Device : public XLib::NonCopyable
	{
		friend Host;
		friend CommandList;

	private:
		static constexpr uint32 MaxResourceCount = 1024;
		static constexpr uint32 MaxResourceViewCount = 1024;
		static constexpr uint32 MaxResourceDescriptorCount = 4096;
		static constexpr uint32 MaxRenderTargetViewCount = 64;
		static constexpr uint32 MaxDepthStencilViewCount = 64;
		static constexpr uint32 MaxPipelineCount = 1024;
		static constexpr uint32 MaxFenceCount = 64;
		static constexpr uint32 MaxSwapChainCount = 4;
		static constexpr uint32 SwapChainPlaneCount = 2;

		struct Resource;
		struct ResourceView;
		struct BindingLayout;
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
		ResourceView* resourceViewsTable = nullptr;
		Pipeline* pipelinesTable = nullptr;
		Fence* fencesTable = nullptr;
		SwapChain* swapChainsTable = nullptr;

		XLib::BitSet<MaxResourceCount> resourcesAllocationMask;
		XLib::BitSet<MaxResourceViewCount> resourceViewsAllocationMask;
		XLib::BitSet<MaxRenderTargetViewCount> renderTargetViewsAllocationMask;
		XLib::BitSet<MaxDepthStencilViewCount> depthStencilViewsAllocationMask;
		XLib::BitSet<MaxFenceCount> fencesAllocationMask;
		XLib::BitSet<MaxSwapChainCount> swapChainsAllocationMask;
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
		inline ResourceHandle encodeResourceHandle(uint32 resourceIndex) const;
		inline ResourceViewHandle encodeResourceViewHandle(uint32 resourceViewIndex) const;
		inline RenderTargetViewHandle encodeRenderTargetViewHandle(uint32 renderTargetIndex) const;
		inline DepthStencilViewHandle encodeDepthStencilViewHandle(uint32 depthStencilIndex) const;
		inline FenceHandle encodeFenceHandle(uint32 fenceIndex) const;
		inline SwapChainHandle encodeSwapChainHandle(uint32 swapChainIndex) const;

		inline uint32 decodeResourceHandle(ResourceHandle handle);
		inline uint32 decodeResourceViewHandle(ResourceViewHandle handle);
		inline uint32 decodeRenderTargetViewHandle(RenderTargetViewHandle handle);
		inline uint32 decodeDepthStencilViewHandle(DepthStencilViewHandle handle);
		inline uint32 decodePipelineHandle(PipelineHandle handle);
		inline uint32 decodeFenceHandle(FenceHandle handle);
		inline uint32 decodeSwapChainHandle(SwapChainHandle handle);

		inline DescriptorAddress encodeDescriptorAddress(uint32 srvHeapDescriptorIndex);
		inline uint32 decodeDescriptorAddress(DescriptorAddress address);

	private:
		void initialize(IDXGIAdapter4* dxgiAdapter);

	public:
		Device() = default;
		~Device() = default;

		ResourceHandle createBuffer(uint32 size, BufferMemoryType memoryType);
		void destroyBuffer(ResourceHandle handle);

		ResourceHandle createTexture(const TextureDim& dim, TextureFormat format, uint32 flags);
		void destroyTexture(ResourceHandle handle);

		ResourceViewHandle createResourceView(ResourceHandle resourceHandle, const ResourceViewDesc& viewDesc);
		void destroyResourceView(ResourceViewHandle handle);

		RenderTargetViewHandle createRenderTargetView(ResourceHandle textureHandle);
		void destroyRenderTargetView(RenderTargetViewHandle handle);

		DepthStencilViewHandle createDepthStencilView(ResourceHandle textureHandle);
		void destroyDepthStencilView(DepthStencilViewHandle handle);

		DescriptorAddress allocateDescriptors(uint32 count = 1);
		void releaseDescriptors(DescriptorAddress address);

		DescriptorBundleLayoutHandle createDescriptorBundleLayout(DataBuffer bytecodeBlob);
		void destroyDescriptorBundleLayout(DescriptorBundleLayoutHandle handle);

		DescriptorBundleHandle createDescriptorBundle(DescriptorBundleLayoutHandle layoutHandle);
		void destroyDescriptorBundle(DescriptorBundleHandle handle);

		BindingLayoutHandle createBindingLayout(DataBuffer bytecodeBlob);
		void destroyBindingLayout(BindingLayoutHandle handle);

		PipelineHandle createGraphicsPipeline(BindingLayoutHandle bindingLayoutHandle,
			uint32 bytecodeBlobsCount, const DataBuffer* bytecodeBlobs,
			const RasterizerDesc& rasterizerDesc, const BlendDesc& blendDesc);
		PipelineHandle createComputePipeline(BindingLayoutHandle bindingLayoutHandle);
		void destroyPipeline(PipelineHandle handle);

		FenceHandle createFence(uint64 initialValue);
		void destroyFence(FenceHandle handle);

		SwapChainHandle createSwapChain(uint16 width, uint16 height, void* hWnd);
		void destroySwapChain(SwapChainHandle handle);

		void createCommandList(CommandList& commandList, CommandListType type);
		void destroyCommandList(CommandList& commandList);

		void writeDescriptor(DescriptorAddress descriptorAddress, ResourceViewHandle resourceViewHandle);
		void writeBundleDescriptor(DescriptorBundleHandle bundle, uint32 bindPointNameCRC, ResourceViewHandle resourceViewHandle);

		void submitGraphics(CommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0);
		void submitAsyncCompute(CommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0);
		void submitAsyncCopy(CommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0);
		void submitFlip(SwapChainHandle swapChainHandle);

		inline void submitGraphics(CommandList& commandList, const FenceSignalDesc& fenceSignal) { submitGraphics(commandList, &fenceSignal, 1); }
		inline void submitAsyncCompute(CommandList& commandList, const FenceSignalDesc& fenceSignal) { submitAsyncCompute(commandList, &fenceSignal, 1); }
		inline void submitAsyncCopy(CommandList& commandList, const FenceSignalDesc& fenceSignal) { submitAsyncCopy(commandList, &fenceSignal, 1); }

		void* getUploadBufferCPUPtr(ResourceHandle uploadBuffer);
		DescriptorAddress getDescriptorBundleStartAddress(DescriptorBundleHandle bundle) const;
		uint64 getFenceValue(FenceHandle fence) const;
		ResourceHandle getSwapChainPlaneTexture(SwapChainHandle swapChain, uint32 planeIndex) const;

		const char* getName() const;
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
	inline ResourceHandle Device::encodeResourceHandle(uint32 resourceIndex) const
	{
		XE_ASSERT(resourceIndex < MaxResourceCount);
	}
}
