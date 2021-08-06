#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Containers.IntrusiveDoublyLinkedList.h>
#include <XLib.Containers.CyclicQueue.h>
#include <XLib.System.Threading.h>
#include <XLib.System.Threading.Lock.h>
#include <XLib.System.Threading.Event.h>

namespace XEngine::Core { class DiskOperationContext; }
namespace XEngine::Core { class DiskWorker; }

namespace XEngine::Core
{
#if 0

	using DiskPersistentFileHandle = uint32;

	enum class DiskOperationResult : uint8
	{
		Success = 0,
		FileNotFound,
		InvalidFilePosition,
		FileReadError,
	};

	struct DiskReadContinuationParams
	{
		uint64 filePosition;
		uint32 expectedReadSize;
	};

	class DiskWorkerBufferHandle
	{
		friend DiskWorker;

	private:
		uint32 diskBufferBlocksTrackingQueueCyclicIndex = 0;

		inline DiskWorkerBufferHandle(uint32 diskBufferBlocksTrackingQueueCyclicIndex)
			: diskBufferBlocksTrackingQueueCyclicIndex(diskBufferBlocksTrackingQueueCyclicIndex) {}

	public:
		DiskWorkerBufferHandle() = default;
		~DiskWorkerBufferHandle() = default;
	};

	using DiskReadFinishedCallback =
		DiskReadContinuationParams(*)(
			DiskOperationContext& context, DiskOperationResult result,
			DiskWorkerBufferHandle dataBufferHandle, const void* data, uint32 dataSize,
			bool endOfFile);

	using DiskPersistentFileOpenedCallback =
		void(*)(DiskOperationContext& context, DiskOperationResult result, DiskPersistentFileHandle handle);

	class DiskOperationContext : public XLib::NonCopyable
	{
		friend DiskWorker;

	private:
		enum class Type : uint8;

	private:
		XLib::IntrusiveDoublyLinkedListItemHook operationsQueueItemHook;

		union
		{
			DiskReadFinishedCallback readFinishedCallback = nullptr;
			DiskPersistentFileOpenedCallback persistentFileOpenedCallback;
		};

		union
		{
			const char* filename = nullptr;
			DiskPersistentFileHandle persistentFileHandle;
		};

		uint64 filePosition = 0;
		uint32 expectedReadSize = 0;
		Type type = Type(0);

	public:
		DiskOperationContext() = default;
		~DiskOperationContext() = default;
	};


	class DiskWorker : public XLib::NonCopyable
	{
	private:
		using OperationsQueue = XLib::IntrusiveDoublyLinkedList<
			DiskOperationContext, &DiskOperationContext::operationsQueueItemHook>;

		using DiskBufferBlocksTrackingQueue =
			XLib::CyclicQueue<uint32, XLib::CyclicQueueStoragePolicy::External, uint16>;

	private:
		OperationsQueue operationsQueue;
		XLib::Lock operationsQueueLock;

		XLib::Thread dispatchThread;
		XLib::Event operationsWaitEvent;

		XLib::HeapPtr<byte> heapBuffer; // disk buffer + disk buffer blocks tracking queue storage
		uint16 diskBufferSizeLog2 = 0;

		DiskBufferBlocksTrackingQueue diskBufferBlocksTrackingQueue;
		uint32 diskBufferUsedDataBackIndex = 0;

	private:
		//inline void dispatchOperation_openPersistentFile(Operation& operation);
		//inline void dispatchOperation_readPersistenFile(Operation& operation);
		inline void dispatchOperation_readFile(Operation& operation);

		void dispatchThreadMain();
		static uint32 __stdcall DispatchThreadMain(DiskWorker* self);

	public:
		DiskWorker() = default;
		~DiskWorker();

		void startup(uint32 diskBufferSizeLog2);
		void shutdown();

		void openPersistentFile(const char* filename,
			DiskPersistentFileOpenedCallback callback, DiskOperationContext& context);
		void closePersistentFile(DiskPersistentFileHandle handle);

		void readFile(DiskPersistentFileHandle handle, uint64 filePosition, uint32 expectedReadSize,
			DiskReadFinishedCallback callback, DiskOperationContext& context);
		void readFile(const char* filename, uint64 filePosition, uint32 expectedReadSize,
			DiskReadFinishedCallback callback, DiskOperationContext& context);

		void releaseBuffer(DiskWorkerBufferHandle handle);

		void cancelOperation(DiskOperationContext& context);
	};

#endif
}
