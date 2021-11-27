#include <d3d12.h>

#include <XLib.Debug.h>
#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.Device.Uploader.h"

#include "XEngine.Render.Device.h"
#include "XEngine.Render.MIPMapGenerator.h"

using namespace XLib;
using namespace XEngine::Render::Device_;

static constexpr uint32 uploadBufferSize = 0x40000;

void Uploader::flush()
{
	d3dCommandList->Close();

	ID3D12CommandList *d3dCommandListsToExecute[] = { d3dCommandList };
	device.d3dCopyQueue->ExecuteCommandLists(
		countof(d3dCommandListsToExecute), d3dCommandListsToExecute);

	device.copyQueueFenceValue++;
	device.d3dCopyQueue->Signal(device.d3dCopyQueueFence, device.copyQueueFenceValue);
	if (device.d3dCopyQueueFence->GetCompletedValue() < device.copyQueueFenceValue)
		device.d3dCopyQueueFence->SetEventOnCompletion(device.copyQueueFenceValue, nullptr);

	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);
}

void Uploader::uploadTexture2DMIPLevel(DXGI_FORMAT format,
	ID3D12Resource* d3dDstTexture, uint16 mipLevel, uint16 width, uint16 height,
	uint16 pixelPitch, uint32 sourceRowPitch, const void* sourceData)
{
	uint32 rowByteSize = width * pixelPitch;
	uint32 uploadRowPitch = alignUp<uint32>(rowByteSize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	uint16 rowsFitToBuffer = uint16(uploadBufferSize / uploadRowPitch);
	for (uint16 rowsUploaded = 0; rowsUploaded < height; rowsUploaded += rowsFitToBuffer)
	{
		uint16 rowsToUpload = min<uint16>(rowsFitToBuffer, height - rowsUploaded);

		// TODO: [CRITICAL] handle case, when rowsFitToBuffer == 0 !!!

		for (uint32 i = 0; i < rowsToUpload; i++)
		{
			memoryCopy(mappedUploadBuffer + i * uploadRowPitch,
				to<byte*>(sourceData) + (rowsUploaded + i) * sourceRowPitch, rowByteSize);
		}

		d3dCommandList->CopyTextureRegion(
			&D3D12TextureCopyLocation_Subresource(d3dDstTexture, mipLevel),
			0, rowsUploaded, 0,
			&D3D12TextureCopyLocation_PlacedFootprint(d3dUploadBuffer, 0,
				format, width, rowsToUpload, 1, uploadRowPitch),
			&D3D12Box(0, width, 0, rowsToUpload));

		flush();
	}
}

void Uploader::initialize()
{
	ID3D12Device *d3dDevice = device.d3dDevice;

	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, d3dCommandAllocator.uuid(), d3dCommandAllocator.voidInitRef());
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, d3dCommandAllocator, nullptr, d3dCommandList.uuid(), d3dCommandList.voidInitRef());
	d3dCommandList->Close();

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		d3dUploadBuffer.uuid(), d3dUploadBuffer.voidInitRef());
	d3dUploadBuffer->Map(0, &D3D12Range(), to<void**>(&mappedUploadBuffer));

	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);
}

void Uploader::destroy()
{

}

void Uploader::uploadBuffer(ID3D12Resource* d3dDestBuffer,
	uint32 destOffset, const void* data, uint32 size)
{
	for (uint32 bytesUploaded = 0; bytesUploaded < size; bytesUploaded += uploadBufferSize)
	{
		uint32 currentUploadSize = min(size - bytesUploaded, uploadBufferSize);

		memoryCopy(mappedUploadBuffer, to<byte*>(data) + bytesUploaded, currentUploadSize);

		d3dCommandList->CopyBufferRegion(d3dDestBuffer, destOffset + bytesUploaded,
			d3dUploadBuffer, 0, currentUploadSize);

		flush();
	}
}

void Uploader::uploadTexture2DAndGenerateMIPs(ID3D12Resource* d3dDstTexture,
	const void* sourceData, uint32 sourceRowPitch, void* mipsGenerationBuffer)
{
	D3D12_RESOURCE_DESC desc = d3dDstTexture->GetDesc();
	uint16 pixelPitch = 0;
	switch (desc.Format)
	{
		case DXGI_FORMAT_A8_UNORM:			pixelPitch = sizeof(uint8);		break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:	pixelPitch = sizeof(uint8x4);	break;

		default:
			// TODO: warning
			return;
	}

	uint16x2 mipSize = { uint16(desc.Width), uint16(desc.Height) };

	XASSERT(mipSize.x * pixelPitch <= uploadBufferSize, "texture is too large");

	const void *mipSourceData = sourceData;
	uint32 mipSourceRowPitch = sourceRowPitch;
	uint16 mipLevel = 0;
	for (;;)
	{
		uploadTexture2DMIPLevel(desc.Format, d3dDstTexture, mipLevel,
			mipSize.x, mipSize.y, pixelPitch, mipSourceRowPitch, mipSourceData);

		mipLevel++;
		if (mipLevel >= desc.MipLevels)
			break;
		if (mipSize.x <= 1 && mipSize.y <= 1)
			break;

		switch (desc.Format)
		{
		case DXGI_FORMAT_A8_UNORM:
			MIPMapGenerator::GenerateLevel<uint8>(mipSourceData, mipSize,
				mipSourceRowPitch, mipsGenerationBuffer, mipSize);
			break;

		case DXGI_FORMAT_R8G8B8A8_UNORM:
			MIPMapGenerator::GenerateLevel<uint8x4>(mipSourceData, mipSize,
				mipSourceRowPitch, mipsGenerationBuffer, mipSize);
			break;
		}

		mipSourceData = to<byte*>(mipsGenerationBuffer);
		mipSourceRowPitch = mipSize.x * pixelPitch;
	}
}