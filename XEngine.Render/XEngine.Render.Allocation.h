#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.D3D12.h>

// TODO: Do we really need to manage buffer externally in `CircularUploadMemoryAllocator`?

namespace XEngine::Render
{
	class CircularRangeAllocatorWithGPURelease : public XLib::NonCopyable
	{
	private:
		uint32 poolSize = 0;
		HAL::DeviceQueueSyncPoint* releaseQueueSyncPoints = nullptr;
		uint32* releaseQueueOffsets = nullptr;

	public:
		static constexpr uint32 CalculateInternalStorageRequirement(uint32 releaseQueueLengthLimit);

	public:
		CircularRangeAllocatorWithGPURelease() = default;
		~CircularRangeAllocatorWithGPURelease(); // TODO: Check if all release sync points are reached.

		void initialize(uint32 poolSize, uint32 releaseQueueLengthLimit, void* internalStorage, uint32 internalStorageSize);
		void destroy();

		uint32 allocate(uint32 size);
		void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint);
	};

	class CircularDescriptorAllocator : public XLib::NonCopyable
	{
	private:
		CircularRangeAllocatorWithGPURelease internalAllocator;

	public:
		CircularDescriptorAllocator() = default;
		~CircularDescriptorAllocator() = default;

		inline uint32 allocate(uint32 size) { return internalAllocator.allocate(size); }
		inline void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint) { internalAllocator.enqueueRelease(syncPoint); }
	};

	struct UploadMemoryAllocationInfo
	{
		HAL::BufferHandle buffer;
		uint32 offset;

		void* mappedMemory;
	};

	class CircularUploadMemoryAllocator : public XLib::NonCopyable
	{
	private:
		CircularRangeAllocatorWithGPURelease internalAllocator;

		HAL::BufferHandle uploadMemoryPoolBuffer = HAL::BufferHandle::Zero;
		uint32 uploadMemoryPoolOffset = 0;
		uint32 uploadMemoryPoolSize = 0;

		void* mappedUploadMemoryPoolBuffer = nullptr;

	public:
		static constexpr uint32 AllocationAlignment = HAL::Device::ConstantBufferBindAlignment;

	public:
		CircularUploadMemoryAllocator() = default;
		~CircularUploadMemoryAllocator() = default;

		void initialize(HAL::BufferHandle uploadMemoryPoolBuffer, uint32 uploadMemoryPoolOffset, uint32 uploadMemoryPoolSize);

		UploadMemoryAllocationInfo allocate(uint32 size) { return internalAllocator.allocate(divRoundUp(size, AllocationAlignment)) * AllocationAlignment; }
		inline void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint) { internalAllocator.enqueueRelease(syncPoint); }
	};
}
