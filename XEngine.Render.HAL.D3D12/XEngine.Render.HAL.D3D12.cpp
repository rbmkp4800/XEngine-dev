#include <d3d12.h>
#include <dxgi1_6.h>

#include "D3D12Helpers.h"

#include "XEngine.Render.HAL.D3D12.h"

#define XE_ASSERT(...)
#define XE_ASSERT_UNREACHABLE_CODE
#define XE_MASTER_ASSERT(...)
#define XE_MASTER_ASSERT_UNREACHABLE_CODE

using namespace XLib::Platform;
using namespace XEngine::Render::HAL;

static COMPtr<IDXGIFactory7> dxgiFactory;

static inline D3D12_HEAP_TYPE TranslateBufferTypeToD3D12HeapType(BufferType type)
{
	switch (type)
	{
		case BufferType::Default:	return D3D12_HEAP_TYPE_DEFAULT;
		case BufferType::Upload:	return D3D12_HEAP_TYPE_UPLOAD;
		case BufferType::Readback:	return D3D12_HEAP_TYPE_READBACK;
	}

	XE_MASTER_ASSERT_UNREACHABLE_CODE;
}

static inline DXGI_FORMAT TranslateTextureFormatToDXGIFormat(TextureFormat format)
{
	switch (format)
	{
		case TextureFormat::Undefined:		return DXGI_FORMAT_UNKNOWN;
		case TextureFormat::R8G8B8A8_UNORM:	return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	XE_MASTER_ASSERT_UNREACHABLE_CODE;
}

// GraphicsCommandList /////////////////////////////////////////////////////////////////////////////

void GraphicsCommandList::setRenderTargets(uint8 rtvCount, const RenderTargetViewHandle* rtvs, DepthStencilViewHandle dsv)
{
	XE_ASSERT(device);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRTVDescriptorAddresses[4] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDSVDescriptorAddress = {};

	const bool useRTVs = rtvCount > 0;
	if (useRTVs)
	{
		XE_ASSERT(rtvs);
		XE_ASSERT(rtvCount < countof(d3dRTVDescriptorAddresses));
		for (uint8 i = 0; i < rtvCount; i++)
		{
			XE_ASSERT(rtvs[i] != ZeroRenderTargetViewHandle);
			const uint32 descriptorIndex = uint32(rtvs[i]); // TODO: Proper handle decode
			d3dRTVDescriptorAddresses[i].ptr = device->rtvHeapStartAddress + descriptorIndex * device->rtvDescriptorSize;
		}
	}

	const bool useDSVs = dsv != ZeroDepthStencilViewHandle;
	if (useDSVs)
	{
		const uint32 descriptorIndex = uint32(dsv); // TODO: Proper handle decode
		d3dDSVDescriptorAddress.ptr = device->dsvHeapStartAddress + uint32(dsv) * device->dsvHeapStartAddress;
	}

	d3dCommandList->OMSetRenderTargets(rtvCount,
		useRTVs ? d3dRTVDescriptorAddresses : nullptr, FALSE,
		useDSVs ? &d3dDSVDescriptorAddress : nullptr);
}

// Device //////////////////////////////////////////////////////////////////////////////////////////

void Device::initialize(IDXGIAdapter4* dxgiAdapter)
{
	XE_MASTER_ASSERT(!d3dDevice);

	D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_12_1, d3dDevice.uuid(), d3dDevice.voidInitRef());

	const D3D12_DESCRIPTOR_HEAP_DESC d3dReferenceSRVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, ReferenceSRVHeapSize);
	const D3D12_DESCRIPTOR_HEAP_DESC d3dShaderVisbileSRVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			ShaderVisibleSRVHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
	const D3D12_DESCRIPTOR_HEAP_DESC d3dRTVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RTVHeapSize);
	const D3D12_DESCRIPTOR_HEAP_DESC d3dDSVHeapDesc =
		D3D12Helpers::DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DSVHeapSize);

	d3dDevice->CreateDescriptorHeap(&d3dReferenceSRVHeapDesc, d3dReferenceSRVHeap.uuid(), d3dReferenceSRVHeap.voidInitRef());
	d3dDevice->CreateDescriptorHeap(&d3dShaderVisbileSRVHeapDesc, d3dShaderVisbileSRVHeap.uuid(), d3dShaderVisbileSRVHeap.voidInitRef());
	d3dDevice->CreateDescriptorHeap(&d3dRTVHeapDesc, d3dRTVHeap.uuid(), d3dRTVHeap.voidInitRef());
	d3dDevice->CreateDescriptorHeap(&d3dDSVHeapDesc, d3dDSVHeap.uuid(), d3dDSVHeap.voidInitRef());

	const D3D12_COMMAND_QUEUE_DESC d3dGraphicsQueueDesc = D3D12Helpers::CommandQueueDesc(D3D12_COMMAND_LIST_TYPE_DIRECT);
	d3dDevice->CreateCommandQueue(&d3dGraphicsQueueDesc, d3dGraphicsQueue.uuid(), d3dGraphicsQueue.voidInitRef());

	rtvHeapAllocationMask.clear();
	dsvHeapAllocationMask.clear();

	referenceSRVHeapStartAddress = d3dReferenceSRVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	shaderVisbileSRVHeapStartAddressCPU = d3dShaderVisbileSRVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	shaderVisbileSRVHeapStartAddressGPU = d3dShaderVisbileSRVHeap->GetGPUDescriptorHandleForHeapStart().ptr;
	rtvHeapStartAddress = d3dRTVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	dsvHeapStartAddress = d3dDSVHeap->GetCPUDescriptorHandleForHeapStart().ptr;

	rtvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	dsvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	srvDescriptorSize = uint16(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
}

void Device::createBuffer(uint32 size, BufferType type, Buffer& buffer)
{
	XE_MASTER_ASSERT(!buffer.d3dResource);

	const D3D12_HEAP_TYPE d3dHeapType = TranslateBufferTypeToD3D12HeapType(type);
	const D3D12_HEAP_PROPERTIES d3dHeapProps = D3D12Helpers::HeapProperties(d3dHeapType);
	const D3D12_RESOURCE_DESC d3dResourceDesc = D3D12Helpers::ResourceDescForBuffer(size);

	d3dDevice->CreateCommittedResource(&d3dHeapProps, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&buffer.d3dResource));
}

void Device::createTexture2D(uint16 width, uint16 height, TextureFormat format, uint32 flags, Texture& texture)
{
	XE_MASTER_ASSERT(!texture.d3dResource);

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

ResourceViewHandle Device::createResourceView(const ResourceViewDesc& desc)
{

}

RenderTargetViewHandle Device::createRenderTargetView(Texture& texture)
{
	XE_MASTER_ASSERT(texture.d3dResource);

	const sint32 freeDescriptorIndex = rtvHeapAllocationMask.findFirstZero();
	XE_MASTER_ASSERT(freeDescriptorIndex >= 0);
	rtvHeapAllocationMask.set(freeDescriptorIndex);

	const uint64 descriptorAddress = rtvHeapStartAddress + rtvDescriptorSize * freeDescriptorIndex;
	d3dDevice->CreateRenderTargetView(texture.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorAddress });

	return RenderTargetViewHandle(freeDescriptorIndex); // TODO: Proper handle encode
}

DepthStencilViewHandle Device::createDepthStencilView(Texture& texture)
{
	XE_MASTER_ASSERT(texture.d3dResource);

	const sint32 freeDescriptorIndex = dsvHeapAllocationMask.findFirstZero();
	XE_MASTER_ASSERT(freeDescriptorIndex >= 0);
	dsvHeapAllocationMask.set(freeDescriptorIndex);

	const uint64 descriptorAddress = dsvHeapStartAddress + dsvDescriptorSize * freeDescriptorIndex;
	d3dDevice->CreateDepthStencilView(texture.d3dResource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptorAddress });

	return DepthStencilViewHandle(freeDescriptorIndex); // TODO: Proper handle encode
}

DescriptorAddress Device::allocateDescriptors(uint32 count)
{

}

FenceHandle Device::createFence(uint64 initialValue)
{
	//d3dDevice->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, ...);
}

void Device::createGraphicsCommandList(GraphicsCommandList& commandList)
{
	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandList.d3dCommandAllocator));
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandList.d3dCommandAllocator, nullptr,
		IID_PPV_ARGS(&commandList.d3dCommandList));
	commandList.d3dCommandList->Close();
}

void Device::writeDescriptor(DescriptorAddress address, ResourceViewHandle resourceViewHandle)
{
	// TODO: Proper handles decode
	const uint32 sourceDescriptorIndex = uint32(resourceViewHandle);
	const uint32 destDescriptorIndex = uint32(address);

	const uint64 sourceAddress = referenceSRVHeapStartAddress + sourceDescriptorIndex * srvDescriptorSize;
	const uint64 destAddress = shaderVisbileSRVHeapStartAddressCPU + destDescriptorIndex * srvDescriptorSize;
	d3dDevice->CopyDescriptorsSimple(1,
		D3D12_CPU_DESCRIPTOR_HANDLE{ destAddress },
		D3D12_CPU_DESCRIPTOR_HANDLE{ sourceAddress },
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Device::submitGraphics(GraphicsCommandList& commandList, const FenceSignalDesc* fenceSignals = nullptr, uint32 fenceSignalCount = 0)
{
	commandList.d3dCommandList->Close();

	ID3D12CommandList* d3dCommandListsToExecute[] = { commandList.d3dCommandList };
	d3dGraphicsQueue->ExecuteCommandLists(1, d3dCommandListsToExecute);

	for (uint32 i = 0; i < fenceSignalCount; i++)
	{
		ID3D12Fence* d3dFence = ...; // TODO: proper handle decode
		d3dGraphicsQueue->Signal(d3dFence, fenceSignals[i].value);
	}
}

// Host ////////////////////////////////////////////////////////////////////////////////////////////

void Host::CreateDevice(Device& device)
{

}
