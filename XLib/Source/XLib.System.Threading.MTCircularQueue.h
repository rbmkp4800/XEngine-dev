#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.System.Threading.Atomics.h"
#include "XLib.System.Threading.Event.h"

// TODO: Swap front and back (wrong notation)

namespace XLib
{
	template <typename Type, uint32 capacityLog2, uint32 spinCount = 1000>
	class MTCircularQueue_MPMC : public NonCopyable
	{
		static_assert(spinCount != 0, "MTCircularQueue spinCount can't be 0");

	private:
		static constexpr uint32 size = 1 << capacityLog2;

	private:
		Type buffer[size];
		Event frontEvent, backEvent;
		Atomic<uint32> frontIdx = 0;
		Atomic<uint32> backIdx = 0;
		volatile uint32 readyFrontIdx = 0;
		volatile uint32 readyBackIdx = 0;
		Atomic<uint16> waitingFrontCount = 0;
		Atomic<uint16> waitingBackCount = 0;

	public:
		MTCircularQueue_MPMC() = default;
		~MTCircularQueue_MPMC() = default;

		inline void initialize();

		inline void enqueue(const Type& value);
		inline Type dequeue();

		inline uint32 getSize() { return frontIdx.value - backIdx.value; }
		inline bool isEmpty() { return frontIdx.value == backIdx.value; }
		inline bool isFull() { return frontIdx.value - backIdx.value >= size; }
	};

	template <typename Type, uint32 capacityLog2, uint32 spinCount = 1000>
	class MTCircularQueue_SPSC : public NonCopyable
	{
		static_assert(spinCount != 0, "MTCircularQueue spinCount can't be 0");

	private:
		static constexpr uint32 size = 1 << capacityLog2;

	private:
		Type buffer[size];
		Event frontEvent;
		Event backEvent;
		volatile uint32 frontIdx;
		volatile uint32 backIdx;
		volatile bool waitingFront;
		volatile bool waitingBack;

	public:
		MTCircularQueue_SPSC() = default;
		~MTCircularQueue_SPSC() = default;

		inline void initialize();

		inline void enqueue(const Type& value);
		inline Type dequeue();

		inline uint32 getSize() { return frontIdx - backIdx; }
		inline bool isEmpty() { return frontIdx == backIdx; }
		inline bool isFull() { return frontIdx - backIdx >= size; }
	};
}

// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	template <typename Type, uint32 capacityLog2, uint32 spinCount>
	inline void MTCircularQueue_MPMC<Type, capacityLog2, spinCount>::initialize()
	{
		frontIdx.value = 0;
		backIdx.value = 0;
		readyFrontIdx = 0;
		readyBackIdx = 0;
		waitingFrontCount.value = 0;
		waitingBackCount.value = 0;

		frontEvent.initialize(false);
		backEvent.initialize(false);
	}

	template <typename Type, uint32 capacityLog2, uint32 spinCount>
	inline void MTCircularQueue_MPMC<Type, capacityLog2, spinCount>::enqueue(const Type& value)
	{
		for (;;)
		{
			const uint32 lastLocalReadyBackIdx = readyBackIdx;
			for (sint32 spin = 0; spin < spinCount; spin++)
			{
				const uint32 localFrontIdx = frontIdx.load();
				const uint32 localReadyBackIdx = readyBackIdx;
				if (localFrontIdx - localReadyBackIdx < size)
				{
					if (frontIdx.compareExchange(localFrontIdx + 1, localFrontIdx))
					{
						buffer[localFrontIdx % size] = value;

						while (readyFrontIdx != localFrontIdx) {}
						readyFrontIdx++;

						if (waitingBackCount.load())
							backEvent.set();

						return;
					}
					spin = -1;
				}
				if (lastLocalReadyBackIdx != localReadyBackIdx)
				{
					spin = -1;
					lastLocalReadyBackIdx = localReadyBackIdx;
				}
			}

			frontEvent.reset();
			waitingFrontCount.increment();
			if (lastLocalReadyBackIdx != readyBackIdx)
			{
				frontEvent.set();
				waitingFrontCount.decrement();
				continue;
			}

			frontEvent.wait();
			waitingFrontCount.decrement();
		}
	}

	template <typename Type, uint32 capacityLog2, uint32 spinCount>
	inline Type MTCircularQueue_MPMC<Type, capacityLog2, spinCount>::dequeue()
	{
		for (;;)
		{
			const uint32 lastLocalReadyFrontIdx = readyFrontIdx;
			for (sint32 spin = 0; spin < spinCount; spin++)
			{
				const uint32 localBackIdx = backIdx.load();
				const uint32 localReadyFrontIdx = readyFrontIdx;
				if (localReadyFrontIdx - localBackIdx > 0)
				{
					if (backIdx.compareExchange(localBackIdx + 1, localBackIdx))
					{
						Type result = buffer[localBackIdx % size];

						while (readyBackIdx != localBackIdx) {}
						readyBackIdx++;

						if (waitingFrontCount.load())
							frontEvent.set();

						return result;
					}
					spin = -1;
				}
				if (lastLocalReadyFrontIdx != localReadyFrontIdx)
				{
					spin = -1;
					lastLocalReadyFrontIdx = localReadyFrontIdx;
				}
			}

			backEvent.reset();
			waitingBackCount.increment();
			if (lastLocalReadyFrontIdx != readyFrontIdx)
			{
				backEvent.set();
				waitingBackCount.decrement();
				continue;
			}

			backEvent.wait();
			waitingBackCount.decrement();
		}
	}
}
