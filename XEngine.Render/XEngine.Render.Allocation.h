#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.D3D12.h>

// TODO: All this code should probably be thread safe or assert single-thread usage.

namespace XEngine::Render
{
	class CircularAllocatorWithGPUReleaseQueue : public XLib::NonCopyable
	{
	private:
		static constexpr uint8 ReleaseQueueBufferSizeLog2 = 5;
		static constexpr uint16 ReleaseQueueBufferSize = 1 << ReleaseQueueBufferSizeLog2;
		static constexpr uint16 ReleaseQueueCounterMask = ReleaseQueueBufferSize - 1;
		static_assert(ReleaseQueueBufferSizeLog2 < 16);

	private:
		HAL::DeviceQueueSyncPoint releaseQueueSyncPoints[ReleaseQueueBufferSize];
		uint16 releaseQueueRangeCounters[ReleaseQueueBufferSize];

		HAL::Device* device = nullptr;
		uint8 poolSizeLog2 = 0;

		uint16 allocatedRangeHeadCounter = 0;
		uint16 allocatedRangeTailCounter = 0;
		uint16 releaseQueueHeadCounter = 0;
		uint16 releaseQueueTailCounter = 0;

	private:
		bool tryAllocate(uint16 size, uint16& resultAllocationHeadCounter);
		bool tryAdvanceReleaseQueue();

		inline bool isReleaseQueueEmpty() const;
		inline bool isReleaseQueueFull() const;
		inline bool isPendingReleaseRangeEmpty() const;

	public:
		CircularAllocatorWithGPUReleaseQueue() = default;
		~CircularAllocatorWithGPUReleaseQueue() = default;

		void initialize(HAL::Device& device, uint8 poolSizeLog2);

		uint16 allocate(uint16 size);
		void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint);

		inline HAL::Device* getDevice() { return device; }
	};

	class TransientDescriptorAllocator : public XLib::NonCopyable
	{
	private:
		CircularAllocatorWithGPUReleaseQueue baseAllocator;

		HAL::DescriptorAddress descriptorPoolBaseAddress = 0;

	public:
		TransientDescriptorAllocator() = default;
		~TransientDescriptorAllocator() = default;

		void initialize(HAL::Device& device, HAL::DescriptorAddress descriptorPoolBaseAddress, uint8 descriptorPoolSizeLog2);

		inline HAL::DescriptorAddress allocate(uint16 size) { return baseAllocator.allocate(size); }
		inline void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint) { baseAllocator.enqueueRelease(syncPoint); }
	};

	struct UploadMemoryAllocationInfo
	{
		HAL::BufferPointer gpuPointer;
		void* cpuPointer;
	};

	class TransientUploadMemoryAllocator : public XLib::NonCopyable
	{
	private:
		CircularAllocatorWithGPUReleaseQueue baseAllocator;

		HAL::BufferHandle uploadMemoryPoolBuffer = HAL::BufferHandle::Zero;
		byte* mappedUploadMemoryPoolBuffer = nullptr;

	public:
		static constexpr uint32 AllocationAlignment = HAL::Device::ConstantBufferBindAlignment;

	public:
		TransientUploadMemoryAllocator() = default;
		~TransientUploadMemoryAllocator();

		void initialize(HAL::Device& device, uint8 poolSizeLog2);

		UploadMemoryAllocationInfo allocate(uint32 size);
		void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint) { baseAllocator.enqueueRelease(syncPoint); }
	};
}
