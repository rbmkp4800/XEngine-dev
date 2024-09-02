#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Gfx.HAL.D3D12.h>

// TODO: All this code should probably be thread safe or assert single-thread usage.
// TODO: We removed `TransientDescriptorAllocator` so there is no need to have abstract `CircularAllocatorWithGPUReleaseQueue`.

// TODO: I do not like 'counter' (headCounter/tailCounter). Sounds more like index.
// TODO: Use `XLib::InplaceCircularQueue` for sync points and hack SOA on top of it.

namespace XEngine::Gfx
{
	class CircularAllocatorWithGPUReleaseQueue : public XLib::NonCopyable
	{
	private:
		static constexpr uint8 ReleaseQueueBufferSizeLog2 = 5;
		static constexpr uint16 ReleaseQueueBufferSize = 1 << ReleaseQueueBufferSizeLog2;
		static constexpr uint16 ReleaseQueueCounterMask = ReleaseQueueBufferSize - 1;
		static_assert(ReleaseQueueBufferSizeLog2 < 16);

	private:
		HAL::DeviceQueueSyncPoint hwReleaseQueueSyncPoints[ReleaseQueueBufferSize];
		uint16 releaseQueueAllocatedRangeCounters[ReleaseQueueBufferSize];

		HAL::Device* hwDevice = nullptr;
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

		void initialize(HAL::Device& hwDevice, uint8 poolSizeLog2);

		uint16 allocate(uint16 size);
		void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint);

		inline HAL::Device* getDevice() { return hwDevice; }
	};

	struct UploadBufferPointer
	{
		HAL::BufferPointer hwDevicePointer;
		void* hostPointer;
	};

	class CircularUploadMemoryAllocator : public XLib::NonCopyable
	{
	public:
		static constexpr uint32 AllocationAlignment = HAL::ConstantBufferBindAlignment;

	private:
		CircularAllocatorWithGPUReleaseQueue baseAllocator;

		HAL::BufferHandle hwUploadMemoryPoolBuffer = {};
		byte* mappedUploadMemoryPoolBuffer = nullptr;

	public:
		CircularUploadMemoryAllocator() = default;
		~CircularUploadMemoryAllocator();

		void initialize(HAL::Device& device, uint8 poolSizeLog2);

		UploadBufferPointer allocate(uint32 size);
		void enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint) { baseAllocator.enqueueRelease(syncPoint); }
	};
}
