#include <d3d12.h>

#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.Device.TextureHeap.h"

#include "XEngine.Render.Device.h"

using namespace XEngine::Render;
using namespace XEngine::Render::Device_;

#if 0

void TextureHeap::initialize()
{

}

TextureHandle TextureHeap::createTexture(uint16 width, uint16 height)
{
	TextureHandle result = TextureHandle(textureCount);
	textureCount++;

	Texture &texture = textures[result];

	device.d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height),
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		texture.d3dTexture.uuid(), texture.d3dTexture.voidInitRef());

	texture.srvDescriptorIndex = device.srvHeap.allocate(1);
	device.d3dDevice->CreateShaderResourceView(texture.d3dTexture,
		nullptr, device.srvHeap.getCPUHandle(texture.srvDescriptorIndex));

	return result;
}

ID3D12Resource* TextureHeap::getTexture(TextureHandle handle)
{
	return textures[handle].d3dTexture;
}

uint16 TextureHeap::getSRVDescriptorIndex(TextureHandle handle) const
{
	return textures[handle].srvDescriptorIndex;
}

#endif