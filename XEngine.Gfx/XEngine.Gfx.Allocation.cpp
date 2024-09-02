#include "XEngine.Gfx.Allocation.h"

using namespace XEngine::Gfx;

bool CircularAllocatorWithGPUReleaseQueue::tryAllocate(uint16 size, uint16& resultAllocationHeadCounter)
{
	if (allocatedRangeTailCounter < allocatedRangeHeadCounter)
	{
		const uint16 freeSpace = allocatedRangeHeadCounter - allocatedRangeTailCounter;
		if (freeSpace > size)
		{
			resultAllocationHeadCounter = allocatedRangeTailCounter;
			allocatedRangeTailCounter += size;
			return true;
		}
	}
	else
	{
		// If allocated range is empty, then we expect range counters be set to zero by releasing side.
		if (allocatedRangeHeadCounter == allocatedRangeTailCounter)
			XEAssert(allocatedRangeHeadCounter == 0);

		const uint32 poolSize = uint32(1) << poolSizeLog2;
		const uint32 freeSpaceBack = poolSize - uint32(allocatedRangeTailCounter);
		if (freeSpaceBack >= size)
		{
			resultAllocationHeadCounter = allocatedRangeTailCounter;
			allocatedRangeTailCounter += size;
			return true;
		}
		else
		{
			const uint16 freeSpaceFront = allocatedRangeHeadCounter;
			if (freeSpaceFront >= size)
			{
				resultAllocationHeadCounter = 0;
				allocatedRangeTailCounter = size;
				return true;
			}
		}
	}

	return false;
}

bool CircularAllocatorWithGPUReleaseQueue::tryAdvanceReleaseQueue()
{
	bool releaseQueueAdvanced = false;
	for (;;)
	{
		if (releaseQueueHeadCounter == releaseQueueTailCounter)
			break;
		if (!hwDevice->isQueueSyncPointReached(hwReleaseQueueSyncPoints[releaseQueueHeadCounter]))
			break;

		allocatedRangeHeadCounter = releaseQueueAllocatedRangeCounters[releaseQueueHeadCounter];
		releaseQueueHeadCounter++;
		releaseQueueHeadCounter &= ReleaseQueueCounterMask;
		releaseQueueAdvanced = true;
	}

	if (allocatedRangeHeadCounter == allocatedRangeTailCounter) // Allocated range is empty
	{
		allocatedRangeHeadCounter = 0;
		allocatedRangeTailCounter = 0;
	}

	return releaseQueueAdvanced;
}

inline bool CircularAllocatorWithGPUReleaseQueue::isReleaseQueueEmpty() const
{
	return releaseQueueHeadCounter == releaseQueueTailCounter;
}

inline bool CircularAllocatorWithGPUReleaseQueue::isReleaseQueueFull() const
{
	return ((releaseQueueTailCounter + 1) & ReleaseQueueCounterMask) == releaseQueueHeadCounter;
}

inline bool CircularAllocatorWithGPUReleaseQueue::isPendingReleaseRangeEmpty() const
{
	if (isReleaseQueueEmpty())
		return allocatedRangeHeadCounter == allocatedRangeTailCounter;

	const uint16 releaseQueueBackCounter = (releaseQueueTailCounter - 1) & ReleaseQueueCounterMask;
	const uint16 pendingReleaseRangeHeadCounter = releaseQueueAllocatedRangeCounters[releaseQueueBackCounter];
	return pendingReleaseRangeHeadCounter == allocatedRangeTailCounter;
}

void CircularAllocatorWithGPUReleaseQueue::initialize(HAL::Device& hwDevice, uint8 poolSizeLog2)
{
	XEAssert(!this->hwDevice);
	XEAssert(poolSizeLog2 <= 16); // As we use uint16 for counters everywhere

	this->hwDevice = &hwDevice;
	this->poolSizeLog2 = poolSizeLog2;

	allocatedRangeHeadCounter = 0;
	allocatedRangeTailCounter = 0;
	releaseQueueHeadCounter = 0;
	releaseQueueTailCounter = 0;
}

uint16 CircularAllocatorWithGPUReleaseQueue::allocate(uint16 size)
{
	XEAssert(hwDevice);

	uint16 resultAllocationHeadCounter = 0;
	for (;;)
	{
		if (tryAllocate(size, resultAllocationHeadCounter))
			break;
		
		// If release queue is empty, then we trying to allocate more than fits in a pool as a single
		// "release range", without any `enqueueRelease` calls. This is allocator overflow.
		XEMasterAssert(!isReleaseQueueEmpty()); // Allocator overflow.

		while (!tryAdvanceReleaseQueue())
		{
			// We might block here and busy wait for GPU.
			// TODO: Probably emit some warning as this should not happen in general.
		}
	}

	return resultAllocationHeadCounter;
}

void CircularAllocatorWithGPUReleaseQueue::enqueueRelease(HAL::DeviceQueueSyncPoint syncPoint)
{
	XEAssert(hwDevice);

	if (isPendingReleaseRangeEmpty())
		return;

	for (;;)
	{
		tryAdvanceReleaseQueue();

		if (isReleaseQueueFull())
		{
			// Release queue is still full after we tried to advance it.
			// This means that it is too small and something should be done to it.
			XEShitHitTheFan();
		}
		else
		{
			// Push new range to release queue.
			hwReleaseQueueSyncPoints[releaseQueueTailCounter] = syncPoint;
			releaseQueueAllocatedRangeCounters[releaseQueueTailCounter] = allocatedRangeTailCounter;
			releaseQueueTailCounter++;
			releaseQueueTailCounter &= ReleaseQueueCounterMask;
			break;
		}
	}
}

CircularUploadMemoryAllocator::~CircularUploadMemoryAllocator()
{
	if (hwUploadMemoryPoolBuffer != HAL::BufferHandle(0))
	{
		baseAllocator.getDevice()->destroyBuffer(hwUploadMemoryPoolBuffer);

		hwUploadMemoryPoolBuffer = {};
		mappedUploadMemoryPoolBuffer = nullptr;
	}
}

void CircularUploadMemoryAllocator::initialize(HAL::Device& hwDevice, uint8 poolSizeLog2)
{
	// TODO: State asserts

	XEAssert(poolSizeLog2 > HAL::ConstantBufferBindAlignmentLog2);

	baseAllocator.initialize(hwDevice, poolSizeLog2 - HAL::ConstantBufferBindAlignmentLog2);

	const uint32 poolSize = uint32(1) << poolSizeLog2;
	hwUploadMemoryPoolBuffer = hwDevice.createStagingBuffer(poolSize, HAL::StagingBufferAccessMode::DeviceReadHostWrite);
	mappedUploadMemoryPoolBuffer = (byte*)hwDevice.getMappedBufferPtr(hwUploadMemoryPoolBuffer);
}

UploadBufferPointer CircularUploadMemoryAllocator::allocate(uint32 size)
{
	const uint32 baseAllocationSize = divRoundUp(size, AllocationAlignment);
	const uint32 allocationOffset = uint32(baseAllocator.allocate(uint16(baseAllocationSize))) * AllocationAlignment;

	UploadBufferPointer result = {};
	result.hwDevicePointer.buffer = hwUploadMemoryPoolBuffer;
	result.hwDevicePointer.offset = allocationOffset;
	result.hostPointer = mappedUploadMemoryPoolBuffer + allocationOffset;
	return result;
}
