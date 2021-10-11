#include <d3d12.h>
#include <dxgi1_6.h>

#include "D3D12Helpers.h"

#include "XEngine.Render.HAL.D3D12.h"

using namespace XLib::Platform;
using namespace XEngine::Render::HAL;

static COMPtr<IDXGIFactory7> dxgiFactory;

// Device //////////////////////////////////////////////////////////////////////////////////////////

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
		IID_PPV_ARGS(&buffer.d3dResource));
}

void Device::createTexture2D(uint16 width, uint16 height, TextureFormat format, uint32 flags, Texture& texture)
{
	RELEASE_ASSERT(!texture.d3dResource);

	D3D12_RESOURCE_FLAGS d3dResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	if (flags & TextureFlag::AllowRenderTarget)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (flags & TextureFlag::AllowDepthStencil)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (flags & TextureFlag::AllowShaderWrite)
		d3dResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	const DXGI_FORMAT dxgiFormat = TranslateTextureFormatToDXGIFormat(format);
	const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_RESOURCE_DESC d3dResourceDesc =
		D3D12Helpers::ResourceDescForTexture2D(width, height, 0, dxgiFormat, d3dResourceFlags);

	d3dDevice->CreateCommittedResource(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&texture.d3dResource));
}

void Device::createBindingLayout(const void* compiledData, uint32 compiledDataSize, BindingLayout& bindingLayout)
{
	d3dDevice->CreateRootSignature(0, compiledData, compiledDataSize,
		__uuidof(*bindingLayout.d3dRootSignature), (void**)&bindingLayout.d3dRootSignature);
}

void Device::createGraphicsCommandList(GraphicsCommandList& commandList)
{
	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandList.d3dCommandAllocator));
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandList.d3dCommandAllocator, nullptr,
		IID_PPV_ARGS(&commandList.d3dCommandList));
	commandList.d3dCommandList->Close();
}

void Device::createSwapChain(uint16 width, uint16 height, void* hWnd, SwapChain& swapChain)
{
	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc = {};
	dxgiSwapChainDesc.Width = width;
	dxgiSwapChainDesc.Height = height;
	dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.Stereo = FALSE;
	dxgiSwapChainDesc.SampleDesc.Count = 1;
	dxgiSwapChainDesc.SampleDesc.Quality = 1;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.BufferCount = 2;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	dxgiSwapChainDesc.Flags = 0;

	COMPtr<IDXGISwapChain1> dxgiSwapChain1;
	dxgiFactory->CreateSwapChainForHwnd(d3dGraphicsQueue, HWND(hWnd),
		&dxgiSwapChainDesc, nullptr, nullptr, dxgiSwapChain1.initRef());

	dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&swapChain.dxgiSwapChain));
}

// Host ////////////////////////////////////////////////////////////////////////////////////////////

void Host::CreateDevice(Device& device)
{

}
