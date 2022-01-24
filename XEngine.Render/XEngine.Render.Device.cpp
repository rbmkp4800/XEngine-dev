#include <d3d12.h>
#include <dxgi1_6.h>

#include <XEngine.ThreadPool.h>
#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.Device.h"

#include "XEngine.Render.Target.h"

using namespace XLib::Platform;
using namespace XEngine::Render;

COMPtr<IDXGIFactory5> Device::dxgiFactory;
COMPtr<ID3D12Debug> d3dDebug;

void Device::deviceThreadRoutine()
{

}

void Device::DeviceThreadRoutine(void* self)
{
	((Device*)self)->deviceThreadRoutine();
}

Device::Device() :
	bufferHeap(*this),
	textureHeap(*this),
	materialHeap(*this),
	geometryHeap(*this),
	shadersLoader(*this),
	renderer(*this),
	uploader(*this)
{ }

bool Device::initialize()
{
	if (!dxgiFactory.isInitialized())
		CreateDXGIFactory1(dxgiFactory.uuid(), dxgiFactory.voidInitRef());

	if (!d3dDebug.isInitialized())
	{
		D3D12GetDebugInterface(d3dDebug.uuid(), d3dDebug.voidInitRef());
		d3dDebug->EnableDebugLayer();
	}

	for (uint32 adapterIndex = 0;; adapterIndex++)
	{
		COMPtr<IDXGIAdapter1> dxgiAdapter;
		if (dxgiFactory->EnumAdapters1(adapterIndex, dxgiAdapter.initRef()) == DXGI_ERROR_NOT_FOUND)
		{
			dxgiFactory->EnumWarpAdapter(dxgiAdapter.uuid(), dxgiAdapter.voidInitRef());
			if (FAILED(D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_11_0, d3dDevice.uuid(), d3dDevice.voidInitRef())))
				return false;
			break;
		}

		DXGI_ADAPTER_DESC1 desc = {};
		dxgiAdapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (SUCCEEDED(D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_11_0, d3dDevice.uuid(), d3dDevice.voidInitRef())))
			break;
	}


	D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc = {};
	graphicsQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	graphicsQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	d3dDevice->CreateCommandQueue(&graphicsQueueDesc, d3dGraphicsQueue.uuid(), d3dGraphicsQueue.voidInitRef());

	D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
	copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	copyQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	d3dDevice->CreateCommandQueue(&copyQueueDesc, d3dCopyQueue.uuid(), d3dCopyQueue.voidInitRef());

	d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, d3dCopyQueueFence.uuid(), d3dCopyQueueFence.voidInitRef());

	copyQueueFenceValue = 0;

	srvHeap.initalize(d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32, true);
	rtvHeap.initalize(d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32, false);
	dsvHeap.initalize(d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32, false);

	bufferHeap.initialize();
	//textureHeap.initialize();
	//materialHeap.initialize();
	//geometryHeap.initialize();

	//shadersLoader.initialize();
	//psoBuilder.initialize();
	renderer.initialize();
	uploader.initialize();

	//uiResources.initialize(d3dDevice);

	d3dGraphicsQueue->GetTimestampFrequency(&graphicsQueueClockFrequency);

	//ThreadPool::OccupyThread(ThreadPool::ThreadOccupantRoutine(&DeviceThreadRoutine), this, "Render.Device");

	return true;
}

Device::SceneRenderId Device::graphicsQueueSceneRender(
	Scene& scene, const Camera& camera, GBuffer& gBuffer, uint16x2 viewport, Target& target)
{
	renderer.render(scene, camera, gBuffer, target, { 0, 0, viewport.x, viewport.y });

	return 0;
}

void Device::graphicsQueueOutputFlip(Output& output)
{
	output.dxgiSwapChain->Present(1, 0);
}
