#include "XEngine.Gfx.Uploader.h"

using namespace XEngine::Gfx;

static constexpr uint32 UploadBufferSize = 128 * 1024 * 1024;

Uploader XEngine::Gfx::GUploader;

void Uploader::initialize(HAL::Device& hwDevice)
{
	XEAssert(!this->hwDevice);
	this->hwDevice = &hwDevice;

	hwCommandAllocator = hwDevice.createCommandAllocator();
	hwUploadBuffer = hwDevice.createStagingBuffer(128 * 1024 * 1024, Gfx::HAL::StagingBufferAccessMode::DeviceReadHostWrite);

	mappedUploadBuffer = hwDevice.getMappedBufferPtr(hwUploadBuffer);
}

void Uploader::uploadTexture(HAL::TextureHandle hwDestTexture, HAL::TextureSubresource hwDestSubresource,
	HAL::TextureRegion hwDestRegion, const void* sourceData, uint32 sourceRowPitch)
{
	XEAssert(hwDevice);

	HAL::CommandList hwCommandList;
	hwDevice->openCommandList(hwCommandList, hwCommandAllocator);

	const HAL::TextureDesc hwDestTextureDesc = hwDevice->getTextureDesc(hwDestTexture);

	const uint32 texelSize = HAL::TextureFormatUtils::GetTexelByteSize(hwDestTextureDesc.format);
	const uint32 rowSize = texelSize * hwDestRegion.size.x;
	const uint32 placedRowPitch = alignUp<uint32>(rowSize, HAL::BufferPlacedTextureRowPitchAlignment);
	const uint32 placedTextureSize = placedRowPitch * hwDestRegion.size.y;

	XEAssert(rowSize <= sourceRowPitch);
	XEAssert(placedTextureSize <= UploadBufferSize);

	{
		uintptr srcPtr = uintptr(sourceData);
		uintptr destPtr = uintptr(mappedUploadBuffer);
		for (uint16 rowIndex = 0; rowIndex < hwDestRegion.size.y; rowIndex++)
		{
			memoryCopy((void*)destPtr, (void*)srcPtr, rowSize);
			srcPtr += sourceRowPitch;
			destPtr += placedRowPitch;
		}
	}

	hwCommandList.copyBufferTexture(HAL::CopyBufferTextureDirection::BufferToTexture,
		hwUploadBuffer, 0, placedRowPitch,
		hwDestTexture, hwDestSubresource, &hwDestRegion);

	hwDevice->closeCommandList(hwCommandList);
	hwDevice->submitCommandList(HAL::DeviceQueue::Graphics, hwCommandList);

	const HAL::DeviceQueueSyncPoint hwEOPSyncPoint = hwDevice->getEOPSyncPoint(HAL::DeviceQueue::Graphics);
	while (!hwDevice->isQueueSyncPointReached(hwEOPSyncPoint))
		{ }
}
