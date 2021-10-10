#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>

struct ID3D12CommandAllocator;
struct ID3D12DescriptorHeap;
struct ID3D12Device2;
struct ID3D12GraphicsCommandList;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12RootSignature;
struct IDXGISwapChain3;

namespace XEngine::Render::HAL
{
	class Device;

	using BindPointRef = uint32;
	using RenderTargetRef = uint32;
	using DepthStencilRef = uint32;

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

	struct RasterizerDesc
	{

	};

	struct BlendDesc
	{

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

		void setRenderTargets();
		void setViewport();
		void setScissor();

		void setGraphicsBindingLayout(BindingLayout& layout);
		void setComputeBindingLayout(BindingLayout& layout);

		void setGraphicsPipeline(GraphicsPipeline& pipeline);
		void setComputePipeline(ComputePipeline& pipeline);

		void bindGraphicsConstants(BindPointRef bindPoint, const void* data, uint32 size32bitValues);
		void bindGraphicsConstantBuffer(BindPointRef bindPoint, Buffer& buffer, uint32 offset);
		void bindGraphicsReadOnlyBuffer(BindPointRef bindPoint, Buffer& buffer, uint32 offset);
		void bindGraphicsReadWriteBuffer(BindPointRef bindPoint, Buffer& buffer, uint32 offset);

		void drawNonIndexed();
		void drawIndexed();
		void drawMesh();

		void dispatch();

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
	private:
		XLib::Platform::COMPtr<ID3D12Device2> d3dDevice;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dSRVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dRTVHeap;
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dDSVHeap;

	public:
		Device() = default;
		~Device() = default;

		void createBuffer(uint32 size, BufferType type, Buffer& buffer);
		void createTexture2D(uint16 width, uint16 height, TextureFormat format, Texture& texture);
		void createTextureDescriptorArray(uint32 descriptorCount, TextureDescriptorArray& descriptorArray);
		void createBindingLayout(const void* compiledData, uint32 compiledDataSize, BindingLayout& bindingLayout);
		void createGraphicsPipeline(BindingLayout& bindingLayout, const RasterizerDesc& rasterizerDesc, const BlendDesc& blendDesc, GraphicsPipeline& graphicsPipeline);
		void createComputePipeline(BindingLayout& bindingLayout, ComputePipeline& graphicsPipeline);
		void createGraphicsCommandList(GraphicsCommandList& commandList);
		void createComputeCommandList(ComputeCommandList& commandList);
		void createCopyCommandList(CopyCommandList& commandList);
		void createSwapChain(SwapChain& swapChain);

		void destroyBuffer(Buffer& buffer);
		void destroyTexture(Texture& texture);
		void destroyTextureDescriptorArray(TextureDescriptorArray& descriptorArray);
		void destroyGraphicsPipeline(GraphicsPipeline& graphicsPipeline);
		void destroyComputePipeline(ComputePipeline& graphicsPipeline);
		void destroyGraphicsCommandList(GraphicsCommandList& commandList);
		void destroyComputeCommandList(ComputeCommandList& commandList);
		void destroyCopyCommandList(CopyCommandList& commandList);
		void destroySwapChain(SwapChain& swapChain);

		BindPointRef getBindPoint(BindingLayout& layout, uint64 bindPointNameCRC);

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
