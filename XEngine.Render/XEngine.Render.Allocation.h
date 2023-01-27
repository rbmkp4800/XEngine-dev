#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.D3D12.h>

namespace XEngine::Render
{
	class CircularRangeAllocatorWithGPURelease : public XLib::NonCopyable
	{
	private:
		uint32 arenaSize = 0;
		HAL::DeviceQueueSyncPoint* releaseQueueSyncPoints = nullptr;
		uint32* releaseQueueOffsets = nullptr;

	public:
		static constexpr uint32 CalculateInternalStorageRequirement(uint32 releaseQueueLengthLimit);

	public:
		CircularRangeAllocatorWithGPURelease() = default;
		~CircularRangeAllocatorWithGPURelease(); // TODO: Check if all release sync points are reached.

		void initialize(uint32 arenaSize, uint32 releaseQueueLengthLimit, void* internalStorage, uint32 internalStorageSize);
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
	};

	class CircularUploadMemoryAllocator : public XLib::NonCopyable
	{
	private:
		CircularRangeAllocatorWithGPURelease internalAllocator;

		HAL::BufferHandle arenaBuffer = HAL::BufferHandle::Zero;
		uint32 arenaOffset = 0;
		uint32 arenaSize = 0;

	public:
		static constexpr uint32 AllocationAlignment = HAL::Device::ConstantBufferBindAlignment;

	public:
		CircularUploadMemoryAllocator() = default;
		~CircularUploadMemoryAllocator() = default;

		void initialize(HAL::BufferHandle arenaBuffer, uint32 arenaOffset, uint32 arenaSize);

		inline uint32 allocate(uint32 size) { return internalAllocator.allocate(size * AllocationAlignment) * AllocationAlignment; }
		inline void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint) { internalAllocator.enqueueRelease(syncPoint); }
	};
}
