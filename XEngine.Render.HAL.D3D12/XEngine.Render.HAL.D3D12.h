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

	enum class RenderTargetViewHandle : uint32;
	enum class DepthStencilViewHandle : uint32;

	static constexpr RenderTargetViewHandle ZeroRenderTargetViewHandle = RenderTargetViewHandle(0);
	static constexpr DepthStencilViewHandle ZeroDepthStencilViewHandle = DepthStencilViewHandle(0);

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
			AllowRenderTarget = 0x01,
			AllowDepthStencil = 0x02,
			AllowShaderWrite = 0x04,
		};
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

	class TextureDescriptorArray : public XLib::NonCopyable
	{
		friend Device;
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

	class SwapChain : public XLib::NonCopyable
	{
		friend Device;

	private:
		IDXGISwapChain3* dxgiSwapChain = nullptr;

	public:
		SwapChain() = default;
		~SwapChain();
	};

	class Device : public XLib::NonCopyable
	{
		friend Host;

	private:
		XLib::Platform::COMPtr<ID3D12Device2> d3dDevice;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dSRVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dRTVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dDSVHeap;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dGraphicsQueue;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dAsyncComputeQueue;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dAsyncCopyQueue;

		XLib::BitSet<64> rtvHeapAllocationMask;
		XLib::BitSet<64> dsvHeapAllocationMask;

	private:
		void initialize(IDXGIAdapter4* dxgiAdapter);

	public:
		Device() = default;
		~Device() = default;

		void createBuffer(uint32 size, BufferType type, Buffer& buffer);
		void destroyBuffer(Buffer& buffer);

		void createTexture2D(uint16 width, uint16 height, TextureFormat format, uint32 flags, Texture& texture);
		void destroyTexture(Texture& texture);

		RenderTargetViewHandle createRenderTargetView(Texture& texture);
		void destroyRenderTargetView(RenderTargetViewHandle handle);

		DepthStencilViewHandle createDepthStencilView(Texture& texture);
		void destroyDepthStencilView(DepthStencilViewHandle handle);

		//void createTextureDescriptorArray(uint32 descriptorCount, TextureDescriptorArray& descriptorArray);
		//void destroyTextureDescriptorArray(TextureDescriptorArray& descriptorArray);

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

		void createSwapChain(uint16 width, uint16 height, void* hWnd, SwapChain& swapChain);
		void destroySwapChain(SwapChain& swapChain);

		void writeTextureDescritor(Texture& texture, TextureDescriptorArray& descriptorArray, uint32 arrayOffset);

		void submitGraphics(GraphicsCommandList& commandList);
		void submitAsyncCompute(ComputeCommandList& commandList);
		void submitAsyncCopy(CopyCommandList& commandList);
		void submitFlip(SwapChain& swapChain);

		const char* getName() const;
	};

	class Host abstract final
	{
	public:
		static void CreateDevice(Device& device);
	};
}
