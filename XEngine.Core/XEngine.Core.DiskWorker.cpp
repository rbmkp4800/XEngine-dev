#if 0

#include <XLib.System.File.h>

#include "XEngine.Core.DiskWorker.h"

using namespace XLib;
using namespace XEngine::Core;

static constexpr uint32 minReadChunkSize = 256;

enum class DiskOperationContext::Type : uint8
{
	None = 0,
	OpenPersistentFile,
	ReadPersistenFile,
	ReadFile,
};

DiskWorker::~DiskWorker()
{

}

void DiskWorker::startup(uint32 diskBufferSizeLog2)
{
	XASSERT(diskBufferSizeLog2 > 30, "invalid disk buffer size");
	XASSERT(!dispatchThread.isInitialized(), "already running");

	operationsWaitEvent.initialize(false, false);
	dispatchThread.create(&DispatchThreadMain, this);

	this->diskBufferSizeLog2 = diskBufferSizeLog2;

	// Estimation. For 16 Mib disk buffer queue size will be 64.
	const uint16 diskBufferBlocksTrackingQueueCapacityLog2 = max<uint16>(diskBufferSizeLog2 - 18, 32);
	const uint16 diskBufferBlocksTrackingQueueCapacity = (1 << diskBufferBlocksTrackingQueueCapacityLog2);

	const uint32 diskBufferSize = (1 << diskBufferSizeLog2);
	const uint32 diskBufferBlocksTrackingQueueStorageSize =
		diskBufferBlocksTrackingQueueCapacity * sizeof(DiskBufferBlocksTrackingQueue::ElementType);

	const uint32 heapBufferSize = diskBufferSize + diskBufferBlocksTrackingQueueStorageSize;
	heapBuffer.resize(heapBufferSize);

	diskBufferBlocksTrackingQueue.initialize(
		to<uint32*>(heapBuffer + diskBufferSize), diskBufferBlocksTrackingQueueCapacity);

	diskBufferUsedDataBackIndex = 0;
}

void DiskWorker::shutdown()
{

}

DiskOperationHandle DiskWorker::readFileAsync(const char* filename,
	uint64 filePosition, uint32 expectedReadSize, DiskReadHandler handler, void* context)
{
	if (!filename)
		return 0;
	if (!expectedReadSize)
		return 0;
	if (!handler.isInitialized())
		return 0;

	Operation* operation = nullptr;
	{
		ScopedLock lock(operationsAllocatorLock);
		operation = operationsAllocator.allocate();
	}

	// TODO: implement proper handling (wait cycle)
	XASSERT(operation, "operations allocator overflow");

	operation->handler = handler.toRaw();
	operation->handlerContext = context;
	operation->filename = filename;
	operation->filePosition = filePosition;
	operation->expectedReadSize = expectedReadSize;
	operation->type = OperationType::ReadFile;

	{
		ScopedLock lock(operationsQueueLock);
		operationsQueue.pushBack(operation);
	}

	operationsWaitEvent.set();

	return 0;
}

uint32 __stdcall DiskWorker::DispatchThreadMain(DiskWorker* self)
{
	self->dispatchThreadMain();
	return 0;
}

inline void DiskWorker::dispatchOperation_readFile(Operation& operation)
{
	DiskReadHandler handler(operation.handler);

	File file;
	if (!file.open(operation.filename, FileAccessMode::Read, FileOpenMode::OpenExisting))
	{
		handler.call(DiskAccessResult::FileNotFound,
			DiskWorkerBufferReleaseToken(), nullptr, 0, false, operation.handlerContext);
		return;
	}

	const uint64 fileSize = file.getSize();
	const uint32 diskBufferSize = 1 << diskBufferSizeLog2;

	uint32 expectedReadSize = operation.expectedReadSize;
	uint64 nextFilePosition = operation.filePosition;
	uint64 currentFilePosition = 0;

	for (;;)
	{
		if (nextFilePosition != uint64(-1))
		{
			if (nextFilePosition >= fileSize)
			{
				handler.call(DiskAccessResult::InvalidFilePosition,
					DiskWorkerBufferReleaseToken(), nullptr, 0, false, operation.handlerContext);
				return;
			}

			if (currentFilePosition != nextFilePosition)
			{
				currentFilePosition = nextFilePosition;
				file.setPosition(currentFilePosition);
			}
		}

		// Skip all released blocks.
		while (!diskBufferBlocksTrackingQueue.isEmpty() &&
			(diskBufferBlocksTrackingQueue.front() & 0x8000'0000) == 0)
		{
			diskBufferBlocksTrackingQueue.dropFront();
		}

		uint32 currentBufferBlockStartIndex = 0;
		uint32 currentBufferBlockSizeLimit = 0;

		if (diskBufferBlocksTrackingQueue.isEmpty())
		{
			// Use half of the buffer and reset cyclic index.
			currentBufferBlockStartIndex = 0;
			currentBufferBlockSizeLimit = diskBufferSize / 2;
		}
		else
		{
			const uint32 diskBufferUsedDataFrontIndex = diskBufferBlocksTrackingQueue.front() & 0x7FFF'FFFF;

			if (diskBufferUsedDataFrontIndex < diskBufferUsedDataBackIndex)
			{
				// Used data does not cross buffer bounds. We can use space to the end of the buffer.
				currentBufferBlockStartIndex = diskBufferUsedDataBackIndex;
				currentBufferBlockSizeLimit = diskBufferSize - diskBufferUsedDataBackIndex;
			}
			else
			{
				const uint32 freeSpaceSize = diskBufferUsedDataFrontIndex - diskBufferUsedDataBackIndex;

				if (freeSpaceSize < minReadChunkSize)
				{
					// Wait for new blocks to release.

					continue;
				}
				else
				{
					currentBufferBlockStartIndex = diskBufferUsedDataBackIndex;
					currentBufferBlockSizeLimit = freeSpaceSize;
				}
			}
		}

		const uint32 expectedReadSizeFitInBuffer = min(expectedReadSize, currentBufferBlockSizeLimit);
		const uint64 fileBytesLeft = fileSize - currentFilePosition;
		const bool endOfFile = uint64(expectedReadSizeFitInBuffer) >= fileBytesLeft;

		const uint32 readSize = endOfFile ? uint32(fileBytesLeft) : expectedReadSizeFitInBuffer;

		if (!file.read(heapBuffer + currentBufferBlockStartIndex, readSize))
		{
			handler.call(DiskAccessResult::FileReadError, DiskWorkerBufferReleaseToken(),
				nullptr, 0, false, operation.handlerContext);
			return;
		}

		diskBufferBlocksTrackingQueue.pushBack(0x8000'0000 | currentBufferBlockStartIndex);
		diskBufferUsedDataBackIndex = currentBufferBlockStartIndex + readSize;

		if (diskBufferUsedDataBackIndex > diskBufferSize - minReadChunkSize)
			diskBufferUsedDataBackIndex = 0;

		const DiskReadContinuationParams continuationParams =
			handler.call(DiskAccessResult::Success, DiskWorkerBufferReleaseToken(...),
				heapBuffer, readSize, endOfFile, operation.handlerContext);

		if (endOfFile)
			return;
		if (!continuationParams.expectedReadSize)
			return;

		currentFilePosition += readSize;

		nextFilePosition = continuationParams.filePosition;
		expectedReadSize = continuationParams.expectedReadSize;
	}
}

void DiskWorker::dispatchThreadMain()
{
	for (;;)
	{
		operationsWaitEvent.wait();

		for (;;)
		{
			operationsWaitEvent.reset();

			Operation* operation = nullptr;
			{
				ScopedLock lock(operationsQueueLock);
				if (operationsQueue.isEmpty())
					break;
				operation = operationsQueue.popFront();
			}

			switch (operation->type)
			{
			case OperationType::OpenPersistentFile:
				//dispatchOperation_openPersistentFile(*operation);
				break;

			case OperationType::ReadPersistenFile:
				//dispatchOperation_readPersistenFile(*operation);
				break;

			case OperationType::ReadFile:
				dispatchOperation_readFile(*operation);
				break;

			default:
				// TODO: UNREACHABLE_CODE
				break;
			}

			{
				ScopedLock lock(operationsAllocatorLock);
				operationsAllocator.release(operation);
			}
		}
	}
}
#endif