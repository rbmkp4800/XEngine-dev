#include <d3d12.h>
#include <dxgi1_5.h>

#include <XLib.Platform.DXGI.Helpers.h>

#include "XEngine.Render.Target.h"

#include "XEngine.Render.Device.h"

using namespace XLib::Platform;
using namespace XEngine::Render;

void Output::updateRTVs(bool allocateDescriptors)
{
	for (uint32 i = 0; i < bufferCount; i++)
	{
		Target &buffer = buffers[i];

		if (allocateDescriptors)
			buffer.rtvDescriptorIndex = device->rtvHeap.allocate(1);

		dxgiSwapChain->GetBuffer(i, buffer.d3dTexture.uuid(), buffer.d3dTexture.voidInitRef());
		device->d3dDevice->CreateRenderTargetView(buffer.d3dTexture, nullptr,
			device->rtvHeap.getCPUHandle(buffer.rtvDescriptorIndex));
	}
}

void Output::initialize(Device& device, void* hWnd, uint16x2 size)
{
	this->device = &device;

	COMPtr<IDXGISwapChain1> dxgiSwapChain1;
	Device::dxgiFactory->CreateSwapChainForHwnd(device.d3dGraphicsQueue, HWND(hWnd),
		&DXGISwapChainDesc1(size.x, size.y, DXGI_FORMAT_R8G8B8A8_UNORM, bufferCount,
			DXGI_SWAP_EFFECT_FLIP_DISCARD), nullptr, nullptr, dxgiSwapChain1.initRef());
	dxgiSwapChain1->QueryInterface(dxgiSwapChain.uuid(), dxgiSwapChain.voidInitRef());
	updateRTVs(true);
}	

void Output::resize(uint16x2 size)
{
	DXGI_SWAP_CHAIN_DESC desc = {};
	dxgiSwapChain->GetDesc(&desc);
	if (size.x == desc.BufferDesc.Width && size.y == desc.BufferDesc.Height)
		return;

	for (Target& buffer : buffers)
		buffer.d3dTexture.destroy();

	dxgiSwapChain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	updateRTVs(false);
}

Target& Output::getCurrentTarget()
{
	return buffers[dxgiSwapChain->GetCurrentBackBufferIndex()];
}
