#include <d3d12.h>

#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.GBuffer.h"

#include "XEngine.Render.Device.h"

using namespace XLib;
using namespace XEngine::Render;

void GBuffer::initialize(Device& device, uint16x2 size)
{
	this->device = &device;
	this->size = size;

	ID3D12Device *d3dDevice = device.d3dDevice;

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R8G8B8A8_UNORM, size.x, size.y,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 1),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&D3D12ClearValue_Color(DXGI_FORMAT_R8G8B8A8_UNORM),
		d3dDiffuseTexture.uuid(), d3dDiffuseTexture.voidInitRef());

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R16G16B16A16_SNORM, size.x, size.y,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 1),
		D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr,
		d3dNormalRoughnessMetalnessTexture.uuid(), d3dNormalRoughnessMetalnessTexture.voidInitRef());

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R32_TYPELESS, size.x, size.y,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, 1),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&D3D12ClearValue_DepthStencil(DXGI_FORMAT_D32_FLOAT, 1.0f),
		d3dDepthTexture.uuid(), d3dDepthTexture.voidInitRef());

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R11G11B10_FLOAT, size.x, size.y,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 1),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&D3D12ClearValue_Color(DXGI_FORMAT_R11G11B10_FLOAT),
		d3dHDRTexture.uuid(), d3dHDRTexture.voidInitRef());

	const uint16 bloomAlignment = 1 << BloomLevelCount;
	const uint16x2 bloomBaseLevelTextureSize =
		uint16x2(alignUp<uint16>(size.x / 4, bloomAlignment), alignUp<uint16>(size.y / 4, bloomAlignment));

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R11G11B10_FLOAT,
			bloomBaseLevelTextureSize.x, bloomBaseLevelTextureSize.y,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, BloomLevelCount),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&D3D12ClearValue_Color(DXGI_FORMAT_R11G11B10_FLOAT),
		d3dBloomTextureA.uuid(), d3dBloomTextureA.voidInitRef());

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R11G11B10_FLOAT,
			bloomBaseLevelTextureSize.x, bloomBaseLevelTextureSize.y,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, BloomLevelCount),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&D3D12ClearValue_Color(DXGI_FORMAT_R11G11B10_FLOAT),
		d3dBloomTextureB.uuid(), d3dBloomTextureB.voidInitRef());

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R16_FLOAT, size.x / 2, size.y / 2,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 1),
		D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr,
		d3dDownscaledX2DepthTexture.uuid(), d3dDownscaledX2DepthTexture.voidInitRef());

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Texture2D(DXGI_FORMAT_R16_FLOAT, size.x / 4, size.y / 4,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 1),
		D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr,
		d3dDownscaledX4DepthTexture.uuid(), d3dDownscaledX4DepthTexture.voidInitRef());

	// Create SRV descriptors ===============================================================//

	srvDescriptorsBaseIndex = device.srvHeap.allocate(SRVDescriptorIndex::Count);

	// G-Buffer descritors range
	d3dDevice->CreateShaderResourceView(d3dDiffuseTexture, nullptr,
		device.srvHeap.getCPUHandle(srvDescriptorsBaseIndex + SRVDescriptorIndex::Diffuse));
	d3dDevice->CreateShaderResourceView(d3dNormalRoughnessMetalnessTexture, nullptr,
		device.srvHeap.getCPUHandle(srvDescriptorsBaseIndex + SRVDescriptorIndex::NormalRoughnessMetalness));
	d3dDevice->CreateShaderResourceView(d3dDepthTexture,
		&D3D12ShaderResourceViewDesc_Texture2D(DXGI_FORMAT_R32_FLOAT),
		device.srvHeap.getCPUHandle(srvDescriptorsBaseIndex + SRVDescriptorIndex::Depth));

	// NOTE: HDR and BloomA texture are used as single range in ToneMapping
	d3dDevice->CreateShaderResourceView(d3dHDRTexture, nullptr,
		device.srvHeap.getCPUHandle(srvDescriptorsBaseIndex + SRVDescriptorIndex::HDR));
	d3dDevice->CreateShaderResourceView(d3dBloomTextureA, nullptr,
		device.srvHeap.getCPUHandle(srvDescriptorsBaseIndex + SRVDescriptorIndex::BloomA));
	d3dDevice->CreateShaderResourceView(d3dBloomTextureB, nullptr,
		device.srvHeap.getCPUHandle(srvDescriptorsBaseIndex + SRVDescriptorIndex::BloomB));

	d3dDevice->CreateShaderResourceView(d3dDownscaledX2DepthTexture, nullptr,
		device.srvHeap.getCPUHandle(srvDescriptorsBaseIndex + SRVDescriptorIndex::DownscaledX2Depth));

	// Create RTV descriptors ===============================================================//

	rtvDescriptorsBaseIndex = device.rtvHeap.allocate(RTVDescriptorIndex::Count);

	d3dDevice->CreateRenderTargetView(d3dDiffuseTexture, nullptr,
		device.rtvHeap.getCPUHandle(rtvDescriptorsBaseIndex + RTVDescriptorIndex::Diffuse));
	d3dDevice->CreateRenderTargetView(d3dNormalRoughnessMetalnessTexture, nullptr,
		device.rtvHeap.getCPUHandle(rtvDescriptorsBaseIndex + RTVDescriptorIndex::NormalRoughnessMetalness));
	d3dDevice->CreateRenderTargetView(d3dHDRTexture, nullptr,
		device.rtvHeap.getCPUHandle(rtvDescriptorsBaseIndex + RTVDescriptorIndex::HDR));

	for (uint16 i = 0; i < BloomLevelCount; i++)
	{
		d3dDevice->CreateRenderTargetView(d3dBloomTextureA, &D3D12RenderTargetViewDesc_Texture2D(i),
			device.rtvHeap.getCPUHandle(rtvDescriptorsBaseIndex + RTVDescriptorIndex::BloomABase + i));
		d3dDevice->CreateRenderTargetView(d3dBloomTextureB, &D3D12RenderTargetViewDesc_Texture2D(i),
			device.rtvHeap.getCPUHandle(rtvDescriptorsBaseIndex + RTVDescriptorIndex::BloomBBase + i));
	}

	d3dDevice->CreateRenderTargetView(d3dDownscaledX2DepthTexture, nullptr,
		device.rtvHeap.getCPUHandle(rtvDescriptorsBaseIndex + RTVDescriptorIndex::DownscaledX2Depth));
	d3dDevice->CreateRenderTargetView(d3dDownscaledX4DepthTexture, nullptr,
		device.rtvHeap.getCPUHandle(rtvDescriptorsBaseIndex + RTVDescriptorIndex::DownscaledX4Depth));

	// Create DSV descriptors ===============================================================//

	dsvDescriptorIndex = device.dsvHeap.allocate(1);
	d3dDevice->CreateDepthStencilView(d3dDepthTexture,
		&D3D12DepthStencilViewDesc_Texture2D(DXGI_FORMAT_D32_FLOAT),
		device.dsvHeap.getCPUHandle(dsvDescriptorIndex));
}

void GBuffer::resize(uint16x2 size)
{
	// TODO: implement resize
}