#pragma once

#include "XLib.h"
#include "XLib.Debug.h"

namespace XLib
{
	struct CyclicQueueStoragePolicy abstract final
	{
		struct InternalHeap abstract final {};
		struct External abstract final {};

		template <uint32 storageSize>
		struct InternalStatic abstract final {};
	};

	template <typename Type, typename StoragePolicy = CyclicQueueStoragePolicy::InternalHeap>
	class CyclicQueue
	{
		static_assert(true, "XLib.Containers.CyclicQueue: invalid storage policy");
	};

	// TODO: add rvalues and destructors

	template <typename Type>
	class CyclicQueue<Type, CyclicQueueStoragePolicy::External>
	{
	private:
		Type *buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 frontIdx = 0, backIdx = 0;

	public:
		inline void initialize(Type* storage, uint32 capacity)
		{
			buffer = storage;
			bufferSize = capacity;
			frontIdx = 0;
			backIdx = 0;
		}

		inline uint32 getCapacity() { return bufferSize; }
		inline uint32 getSize() { return (bufferSize + backIdx - frontIdx) % bufferSize; }
		inline bool isFull() { return (backIdx + 1) % bufferSize == frontIdx; }
		inline bool isEmpty() { return frontIdx == backIdx; }

		inline void pushBack(const Type& value)
		{
			XASSERT(!isFull(), "queue is full");

			buffer[backIdx] = value;
			backIdx = (backIdx + 1) % bufferSize;
		}
		inline Type popFront()
		{
			XASSERT(!isEmpty(), "queue is empty");

			Type value = buffer[frontIdx];
			frontIdx = (frontIdx + 1) % bufferSize;
			return value;
		}
		inline void dropFront()
		{
			XASSERT(!isEmpty(), "queue is empty");

			buffer[frontIdx].~Type();
			frontIdx = (frontIdx + 1) % bufferSize;
		}

		inline Type& front() { return buffer[frontIdx]; }
		inline Type& back() { return buffer[(backIdx + bufferSize - 1) % bufferSize]; }

		inline Type& operator [] (uint32 index) { return buffer[(frontIdx + index) % bufferSize]; }
	};

	template <typename Type, uint32 bufferSize>
	class CyclicQueue<Type, CyclicQueueStoragePolicy::InternalStatic<bufferSize>>
	{
	private:
		Type buffer[bufferSize];
		uint32 frontIdx, backIdx;

	public:
		inline CyclicQueue() : frontIdx(0), backIdx(0) {}

		inline uint32 size() { return (bufferSize + backIdx - frontIdx) % bufferSize; }
		inline bool isFull() { return (backIdx + 1) % bufferSize == frontIdx; }
		inline bool isEmpty() { return frontIdx == backIdx; }

		inline void pushBack(const Type& value)
		{
			XASSERT(!isFull(), "queue is full");

			buffer[backIdx] = value;
			backIdx = (backIdx + 1) % bufferSize;
		}
		inline Type popFront()
		{
			XASSERT(!isEmpty(), "queue is empty");

			Type value = buffer[frontIdx];
			frontIdx = (frontIdx + 1) % bufferSize;
			return value;
		}
		inline void dropFront()
		{
			XASSERT(!isEmpty(), "queue is empty");

			buffer[frontIdx]->~Type();
			frontIdx = (frontIdx + 1) % bufferSize;
		}

		inline void enqueue(const Type& value) { pushBack(value); }
		inline Type dequeue() { return popFront(); }

		inline Type& operator [] (uint32 index) { return buffer[(frontIdx + index) % bufferSize]; }

		inline Type& front() { return buffer[frontIdx % bufferSize]; }
		inline Type& back() { return buffer[(backIdx - 1) % bufferSize]; }
	};
}