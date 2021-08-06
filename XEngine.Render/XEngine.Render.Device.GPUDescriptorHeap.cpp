#include <d3d12.h>

#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.Device.GPUDescriptorHeap.h"

using namespace XEngine::Render;
using namespace XEngine::Render::Device_;

void GPUDescriptorHeap::initalize(ID3D12Device* d3dDevice,
	D3D12_DESCRIPTOR_HEAP_TYPE type, uint16 size, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = type;
	heapDesc.NumDescriptors = size;
	heapDesc.Flags = shaderVisible ?
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE :
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	d3dDevice->CreateDescriptorHeap(&heapDesc, d3dDescriptorHeap.uuid(), d3dDescriptorHeap.voidInitRef());

	allocatedDescriptorsCount = 0;
	descritorSize = d3dDevice->GetDescriptorHandleIncrementSize(type);
}

uint16 GPUDescriptorHeap::allocate(uint16 count)
{
	uint16 result = allocatedDescriptorsCount;
	allocatedDescriptorsCount += count;
	return result;
}

D3D12_CPU_DESCRIPTOR_HANDLE GPUDescriptorHeap::getCPUHandle(uint16 descriptor)
	{ return d3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart() + descriptor * descritorSize; }
D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHeap::getGPUHandle(uint16 descriptor)
	{ return d3dDescriptorHeap->GetGPUDescriptorHandleForHeapStart() + descriptor * descritorSize; }