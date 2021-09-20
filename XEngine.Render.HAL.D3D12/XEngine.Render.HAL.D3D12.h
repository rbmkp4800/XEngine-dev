#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>

struct ID3D12Device2;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandAllocator;

namespace XEngine::Render::HAL
{
	class Device;

	enum class BufferType : uint8
	{
		Default = 0,
		Upload,
		Readback,
	};

	enum class Format : uint8
	{
		None = 0,
		
	};

	class Buffer : public XLib::NonCopyable
	{
		friend Device;
	};

	class Texture : public XLib::NonCopyable
	{
		friend Device;
	};

	class TextureDescriptorArray : public XLib::NonCopyable
	{
		friend Device;
	};

	class GraphicsPipeline : public XLib::NonCopyable
	{
		friend Device;
	};

	class ComputePipeline : public XLib::NonCopyable
	{
		friend Device;
	};

	class GraphicsCommandList : public XLib::NonCopyable
	{
		friend Device;

	private:
		XLib::Platform::COMPtr<ID3D12GraphicsCommandList> d3dCommandList;
		XLib::Platform::COMPtr<ID3D12CommandAllocator> d3dCommandAllocator;

	public:
		GraphicsCommandList() = default;
		~GraphicsCommandList();

		void bindGraphicsPipeline(GraphicsPipeline& pipeline);
		void bindComputePipeline(ComputePipeline& pipeline);

		void drawNonIndexed();
		void drawIndexed();
		void drawMesh();
		void dispatch();

		void copyBufferRegion();
		void copyTextureRegion();
	};

	class ComputeCommandList : public XLib::NonCopyable
	{
		friend Device;

	private:
		XLib::Platform::COMPtr<ID3D12GraphicsCommandList> d3dCommandList;
		XLib::Platform::COMPtr<ID3D12CommandAllocator> d3dCommandAllocator;

	public:
		ComputeCommandList() = default;
		~ComputeCommandList();

		void bindComputePipeline(ComputePipeline& pipeline);

		void dispatch();
	};

	class CopyCommandList : public XLib::NonCopyable
	{
		friend Device;

	private:
		XLib::Platform::COMPtr<ID3D12GraphicsCommandList> d3dCommandList;
		XLib::Platform::COMPtr<ID3D12CommandAllocator> d3dCommandAllocator;

	public:
		CopyCommandList() = default;
		~CopyCommandList();
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
		~Device();

		void createBuffer(BufferType type, Buffer& buffer);
		void createTexture2D(Texture& texture);
		void createTextureDescriptorArray(uint32 descriptorCount);
		void createGraphicsPipeline(GraphicsPipeline& graphicsPipeline);
		void createComputePipeline(ComputePipeline& graphicsPipeline);
		void createGraphicsCommandList(GraphicsCommandList& commandList);
		void createComputeCommandList(ComputeCommandList& commandList);
		void createCopyCommandList(CopyCommandList& commandList);

		void writeTextureDescritor(Texture& texture, );

		void submitToGraphicsQueue(GraphicsCommandList& commandList);
		void submitToAsyncComputeQueue(ComputeCommandList& commandList);
		void submitToCopyQueue(CopyCommandList& commandList);
	};

	class Host abstract final
	{

	};
}
