#include <d3d12.h>

#include <XLib.Vectors.Arithmetics.h>
#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.CustomDrawBatch.h"

#include "XEngine.Render.Device.h"
#include "XEngine.Render.Target.h"
//#include "XEngine.Render.Vertices.h"

using namespace XEngine::Render;

/*static constexpr uint32 defaultVertexBufferSize = 0x10000;

static inline uint8 VertexStrideFromGeometryType(GeometryType type)
{
	switch (type)
	{
		case GeometryType::Color:				return sizeof(VertexUIColor);
		case GeometryType::ColorAlphaTexture:	return sizeof(VertexUIColorTexture);
		default: throw;
	}
}

void CustomDrawBatch::flushCurrentGeometry()
{
	uint32 vertexDataSize = vertexBufferUsedBytes - currentGeometryVertexBufferOffset;
	if (!vertexDataSize)
		return;

	ID3D12PipelineState *d3dPSO = nullptr;
	switch (currentGeometryType)
	{
		case GeometryType::Color:
			d3dPSO = device->uiResources.getColorPSO();
			break;

		case GeometryType::ColorAlphaTexture:
			d3dPSO = device->uiResources.getColorAlphaTexturePSO();
			break;

		default:
			throw;
	}

	d3dCommandList->SetPipelineState(d3dPSO);
	d3dCommandList->IASetVertexBuffers(0, 1,
		&D3D12VertexBufferView(
			d3dVertexBuffer->GetGPUVirtualAddress() + currentGeometryVertexBufferOffset,
			vertexDataSize, currentGeometryVertexStride));
	d3dCommandList->DrawInstanced(vertexDataSize / currentGeometryVertexStride, 1, 0, 0);

	currentGeometryVertexBufferOffset = vertexBufferUsedBytes;
}

void CustomDrawBatch::initialize(Device& device)
{
	this->device = &device;
	vertexBufferSize = defaultVertexBufferSize;

	ID3D12Device *d3dDevice = device.d3dDevice;

	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		d3dCommandAllocator.uuid(), d3dCommandAllocator.voidInitRef());
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3dCommandAllocator, nullptr,
		d3dCommandList.uuid(), d3dCommandList.voidInitRef());
	d3dCommandList->Close();

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		d3dVertexBuffer.uuid(), d3dVertexBuffer.voidInitRef());

	d3dVertexBuffer->Map(0, &D3D12Range(), to<void**>(&mappedVertexBuffer));
}

void CustomDrawBatch::beginDraw(Target& target, rectu16 viewport)
{
	this->target = &target;

	uint16x2 viewportSize = viewport.getSize();
	float32x2 viewportSizeF = float32x2(viewportSize);
	ndcScale = float32x2(2.0f, -2.0f) / viewportSizeF;

	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);

	d3dCommandList->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, viewportSizeF.x, viewportSizeF.y));
	d3dCommandList->RSSetScissorRects(1, &D3D12Rect(0, 0, viewportSize.x, viewportSize.y));
	d3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap *d3dDescriptorHeaps[] = { device->srvHeap.getD3D12DescriptorHeap() };
	d3dCommandList->SetDescriptorHeaps(countof(d3dDescriptorHeaps), d3dDescriptorHeaps);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle =
		device->rtvHeap.getCPUHandle(target.rtvDescriptorIndex);
	d3dCommandList->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

	d3dCommandList->SetGraphicsRootSignature(device->uiResources.getDefaultRS());

	if (!target.stateRenderTarget)
	{
		d3dCommandList->ResourceBarrier(1,
			&D3D12ResourceBarrier_Transition(target.d3dTexture,
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

		target.stateRenderTarget = true;
	}

	vertexBufferUsedBytes = 0;
	currentGeometryVertexBufferOffset = 0;
	currentGeometryType = GeometryType::None;
}

void CustomDrawBatch::endDraw(bool finalizeTarget)
{
	flushCurrentGeometry();

	if (finalizeTarget)
	{
		d3dCommandList->ResourceBarrier(1,
			&D3D12ResourceBarrier_Transition(target->d3dTexture,
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));

		target->stateRenderTarget = false;
	}

	d3dCommandList->Close();

	target = nullptr;
}

void CustomDrawBatch::setTexture(Texture& texture)
{
	d3dCommandList->SetGraphicsRootDescriptorTable(0,
		device->srvHeap.getGPUHandle(texture.srvDescriptorIndex));
}

void* CustomDrawBatch::allocateVertices(GeometryType type, uint32 count)
{
	if (type != currentGeometryType)
	{
		flushCurrentGeometry();
		currentGeometryType = type;
		currentGeometryVertexStride = VertexStrideFromGeometryType(type);
	}

	uint32 size = currentGeometryVertexStride * count;
	if (vertexBufferUsedBytes + size > vertexBufferSize)
		return nullptr;

	void *result = mappedVertexBuffer + vertexBufferUsedBytes;
	vertexBufferUsedBytes += size;
	return result;
}*/
