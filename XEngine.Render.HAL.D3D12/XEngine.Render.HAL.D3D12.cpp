#include <d3d12.h>

#include "D3D12Helpers.h"

#include "XEngine.Render.HAL.D3D12.h"

using namespace XEngine::Render::HAL;

static inline D3D12_HEAP_TYPE TranslateBuffetTypeToD3D12HeapType(BufferType type)
{
	switch (type)
	{
		case BufferType::Default:	return D3D12_HEAP_TYPE_DEFAULT;
		case BufferType::Upload:	return D3D12_HEAP_TYPE_UPLOAD;
		case BufferType::Readback:	return D3D12_HEAP_TYPE_READBACK;
	}

	UNREACHABLE_CODE();
}

static inline DXGI_FORMAT TranslateTextureFormatToDXGIFormat(TextureFormat format)
{
	switch (format)
	{
		case TextureFormat::Undefined:		return DXGI_FORMAT_UNKNOWN;
		case TextureFormat::R8G8B8A8_UNORM:	return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	UNREACHABLE_CODE();
}

void Device::createBuffer(uint32 size, BufferType type, Buffer& buffer)
{
	RELEASE_ASSERT(!buffer.d3dResource);

	const D3D12_HEAP_TYPE d3dHeapType = TranslateBuffetTypeToD3D12HeapType(type);
	const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapProperties(d3dHeapType);
	const D3D12_RESOURCE_DESC d3dResourceDesc = D3D12Helpers::ResourceDescForBuffer(size);

	d3dDevice->CreateCommittedResource(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		__uuidof(*buffer.d3dResource), (void**)&buffer.d3dResource);
}

void Device::createTexture2D(uint16 width, uint16 height, TextureFormat format, Texture& texture)
{
	RELEASE_ASSERT(!texture.d3dResource);

	const DXGI_FORMAT dxgiFormat = TranslateTextureFormatToDXGIFormat(format);
	const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_RESOURCE_DESC d3dResourceDesc = D3D12Helpers::ResourceDescForTexture2D(width, height, 0, dxgiFormat);

	d3dDevice->CreateCommittedResource(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		__uuidof(*texture.d3dResource), (void**)&texture.d3dResource);
}
