#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>

enum D3D12_DESCRIPTOR_HEAP_TYPE;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

struct ID3D12Device;
struct ID3D12DescriptorHeap;

namespace XEngine::Render::Device_
{
	class GPUDescriptorHeap : public XLib::NonCopyable
	{
	private:
		XLib::Platform::COMPtr<ID3D12DescriptorHeap> d3dDescriptorHeap;
		uint16 allocatedDescriptorsCount = 0;
		uint16 descritorSize = 0;

	public:
		GPUDescriptorHeap() = default;
		~GPUDescriptorHeap() = default;

		void initalize(ID3D12Device* d3dDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE type, uint16 size, bool shaderVisible);
		uint16 allocate(uint16 count);

		D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint16 descriptor);
		D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint16 descriptor);
		inline ID3D12DescriptorHeap* getD3D12DescriptorHeap() { return d3dDescriptorHeap; }
	};
}