#include <d3d12.h>

#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.Device.BufferHeap.h"

#include "XEngine.Render.Device.h"

using namespace XEngine::Render;
using namespace XEngine::Render::Device_;

void BufferHeap::initialize()
{
	device.d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Buffer(0x10'0000),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		d3dArenaBuffer.uuid(), d3dArenaBuffer.voidInitRef());

	allocatedByteCount = 0;
}

BufferAddress BufferHeap::allocate(uint32 size)
{
	allocatedByteCount = alignUp(allocatedByteCount, BufferAlignment);

	const BufferAddress result = BufferAddress(allocatedByteCount);
	allocatedByteCount += size;

	return result;
}
