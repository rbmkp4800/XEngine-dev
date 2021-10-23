#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>
#include <XLib.Containers.BitSet.h>

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

	enum class ResourceViewHandle : uint32;
	enum class RenderTargetViewHandle : uint32;
	enum class DepthStencilViewHandle : uint32;

	static constexpr ResourceViewHandle ZeroResourceViewHandle = ResourceViewHandle(0);
	static constexpr RenderTargetViewHandle ZeroRenderTargetViewHandle = RenderTargetViewHandle(0);
	static constexpr DepthStencilViewHandle ZeroDepthStencilViewHandle = DepthStencilViewHandle(0);

	using DescriptorAddress = uint32;

	enum class BufferType : uint8
	{
		Default = 0,
		Upload,
		Readback,
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

	enum class ResourceViewType : uint8
	{
		Undefined = 0,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		ReadOnlyTexture2D,
		ReadWriteTexture2D,
	};

	struct ResourceViewDesc
	{
		ResourceViewType type;
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

	class Buffer : public XLib::NonCopyable
	{
		friend Device;

	private:
		ID3D12Resource* d3dResource = nullptr;

	public:
		Buffer() = default;
		~Buffer();
	};

	class Texture : public XLib::NonCopyable
	{
		friend Device;

	private:
		ID3D12Resource* d3dResource = nullptr;

	public:
		Texture() = default;
		~Texture();
	};

	class SwapChain : public XLib::NonCopyable
	{
		friend Device;

	private:
		IDXGISwapChain3* dxgiSwapChain = nullptr;

	public:
		SwapChain() = default;
		~SwapChain();
	};

	class BindingLayout : public XLib::NonCopyable
	{
		friend Device;

	private:
		ID3D12RootSignature* d3dRootSignature = nullptr;

	public:
		BindingLayout() = default;
		~BindingLayout();
	};

	class GraphicsPipeline : public XLib::NonCopyable
	{
		friend Device;

	private:
		ID3D12PipelineState* d3dPipelineState = nullptr;

	public:
		GraphicsPipeline() = default;
		~GraphicsPipeline();
	};

	class ComputePipeline : public XLib::NonCopyable
	{
		friend Device;

	private:
		ID3D12PipelineState* d3dPipelineState = nullptr;

	public:
		ComputePipeline() = default;
		~ComputePipeline();
	};

	class GraphicsCommandList : public XLib::NonCopyable
	{
		friend Device;

	private:
		Device* device = nullptr;
		ID3D12GraphicsCommandList* d3dCommandList = nullptr;
		ID3D12CommandAllocator* d3dCommandAllocator = nullptr;

	public:
		GraphicsCommandList() = default;
		~GraphicsCommandList();

		void setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv = ZeroDepthStencilViewHandle);
		void setRenderTarget(RenderTargetViewHandle rtv, DepthStencilViewHandle dsv = ZeroDepthStencilViewHandle) { setRenderTargets(1, &rtv, dsv); }
		void setViewport(const Viewport& viewport);
		void setScissor();

		void setGraphicsPipeline(GraphicsPipeline& pipeline);
		void setComputePipeline(ComputePipeline& pipeline);

		void bindConstants(uint32 bindPointNameCRC, const void* data, uint32 size32bitValues);
		void bindConstantBuffer(uint32 bindPointNameCRC, Buffer& buffer, uint32 offset);
		void bindReadOnlyBuffer(uint32 bindPointNameCRC, Buffer& buffer, uint32 offset);
		void bindReadWriteBuffer(uint32 bindPointNameCRC, Buffer& buffer, uint32 offset);
		//void bindDescriptor(uint32 bindPointNameCRC, DescriptorAddress address);
		//void bindDescriptorBundle(uint32 bindPointNameCRC, DescriptorBundleHandle bundle);
		//void bindDescriptorArray(uint32 bindPointNameCRC, DescriptorAddress arrayStartAddress);

		void drawNonIndexed();
		void drawIndexed();
		void drawMesh();

		void dispatch(uint32 groupCountX, uint32 groupCountY = 1, uint32 groupCountZ = 1);

		void copyFromBufferToBuffer();
		void copyFromBufferToTexture();
		void copyFromTextureToTexture();
		void copyFromTextureToBuffer();
	};

	class ComputeCommandList : public XLib::NonCopyable
	{
		friend Device;

	private:
		ID3D12GraphicsCommandList* d3dCommandList = nullptr;
		ID3D12CommandAllocator* d3dCommandAllocator = nullptr;

	public:
		ComputeCommandList() = default;
		~ComputeCommandList();

		void setComputePipeline(ComputePipeline& pipeline);

		void dispatch();
	};

	class CopyCommandList : public XLib::NonCopyable
	{
		friend Device;

	private:
		ID3D12GraphicsCommandList* d3dCommandList = nullptr;
		ID3D12CommandAllocator* d3dCommandAllocator = nullptr;

	public:
		CopyCommandList() = default;
		~CopyCommandList();
	};

	class Device : public XLib::NonCopyable
	{
		friend Host;
		friend GraphicsCommandList;

	private:
		static constexpr uint32 ReferenceSRVHeapSize = 1024;
		static constexpr uint32 ShaderVisibleSRVHeapSize = 4096;
		static constexpr uint32 RTVHeapSize = 64;
		static constexpr uint32 DSVHeapSize = 64;

	private:
		XLib::Platform::COMPtr<ID3D12Device2> d3dDevice;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dReferenceSRVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dShaderVisbileSRVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dRTVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dDSVHeap;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dGraphicsQueue;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dAsyncComputeQueue;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dAsyncCopyQueue;

		XLib::BitSet<RTVHeapSize> rtvHeapAllocationMask;
		XLib::BitSet<DSVHeapSize> dsvHeapAllocationMask;

		uint64 rtvHeapStartAddress = 0;
		uint64 dsvHeapStartAddress = 0;
		uint16 rtvDescriptorSize = 0;
		uint16 dsvDescriptorSize = 0;
		uint16 srvDescriptorSize = 0;

	private:
		void initialize(IDXGIAdapter4* dxgiAdapter);

	public:
		Device() = default;
		~Device() = default;

		void createBuffer(uint32 size, BufferType type, Buffer& buffer);
		void destroyBuffer(Buffer& buffer);

		void createTexture2D(uint16 width, uint16 height, TextureFormat format, uint32 flags, Texture& texture);
		void destroyTexture(Texture& texture);

		void createSwapChain(uint16 width, uint16 height, void* hWnd, SwapChain& swapChain);
		void destroySwapChain(SwapChain& swapChain);

		ResourceViewHandle createResourceView(const ResourceViewDesc& desc);
		void destroyResourceView(ResourceViewHandle handle);

		RenderTargetViewHandle createRenderTargetView(Texture& texture);
		void destroyRenderTargetView(RenderTargetViewHandle handle);

		DepthStencilViewHandle createDepthStencilView(Texture& texture);
		void destroyDepthStencilView(DepthStencilViewHandle handle);

		DescriptorAddress allocateDescriptors(uint32 count = 1);
		void releaseDescriptors(DescriptorAddress address);

		//DescriptorBundleLayoutHandle createDescriptorBundleLayout();
		//DescriptorBundleHandle createDescriptorBundle(DescriptorBundleLayoutHandle layout);

		void createGraphicsPipeline(BindingLayout& bindingLayout, const RasterizerDesc& rasterizerDesc, const BlendDesc& blendDesc, GraphicsPipeline& graphicsPipeline);
		void destroyGraphicsPipeline(GraphicsPipeline& graphicsPipeline);

		void createComputePipeline(BindingLayout& bindingLayout, ComputePipeline& graphicsPipeline);
		void destroyComputePipeline(ComputePipeline& graphicsPipeline);

		void createGraphicsCommandList(GraphicsCommandList& commandList);
		void destroyGraphicsCommandList(GraphicsCommandList& commandList);

		//void createComputeCommandList(ComputeCommandList& commandList);
		//void destroyComputeCommandList(ComputeCommandList& commandList);

		//void createCopyCommandList(CopyCommandList& commandList);
		//void destroyCopyCommandList(CopyCommandList& commandList);

		void writeDescriptor(DescriptorAddress address, ResourceViewHandle resourceViewHandle);
		//void writeBundleDescriptor(DescriptorBundleHandle bundle, uint32 entryNameCRC, ResourceViewHandle resourceViewHandle);

		void submitGraphics(GraphicsCommandList& commandList);
		void submitAsyncCompute(ComputeCommandList& commandList);
		void submitAsyncCopy(CopyCommandList& commandList);
		void submitFlip(SwapChain& swapChain);

		//DescriptorAddress getDescriptorBundleStartAddress(DescriptorBundleHandle bundle) const;

		const char* getName() const;
	};

	class Host abstract final
	{
	public:
		static void CreateDevice(Device& device);
	};
}
