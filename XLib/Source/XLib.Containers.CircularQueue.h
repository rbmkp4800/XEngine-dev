#pragma once

#include "XLib.h"

// TODO: Proper destructors and stuff (should not call automatic destructors for entire buffer).

namespace XLib
{
	template <typename ElementType, uint8 CapacityLog2, typename CounterType = uint16>
	class InplaceCircularQueue
	{
	public:
		static constexpr uintptr Capacity = 1 << CapacityLog2;
		static constexpr CounterType CounterToIndexMask = Capacity - 1;

		static_assert(CapacityLog2 <= sizeof(CounterType) * 8);

	private:
		ElementType buffer[Capacity];
		CounterType headCounter = 0;
		CounterType tailCounter = 0;

	public:
		InplaceCircularQueue() = default;
		~InplaceCircularQueue() = default;

		inline void push(const ElementType& value);
		inline ElementType pop();
		inline ElementType& peek();
		inline const ElementType& peek() const;

		inline bool isEmpty() const { return headCounter == tailCounter; }
		inline bool isFull() const { return headCounter + (Capacity - 1) == tailCounter; }

		inline bool isCounterValid(CounterType counter) const;

		inline CounterType getHeadCounter() const { return headCounter; }
		inline CounterType getTailCounter() const { return tailCounter; }
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// INLINE DEFINITIONS //////////////////////////////////////////////////////////////////////////////

template <typename ElementType, uint8 CapacityLog2, typename CounterType>
inline void XLib::InplaceCircularQueue<ElementType, CapacityLog2, CounterType>::push(const ElementType& value)
{
	XAssert(!isFull());
	buffer[tailCounter & CounterToIndexMask] = value;
	tailCounter++;
}

template <typename ElementType, uint8 CapacityLog2, typename CounterType>
inline ElementType XLib::InplaceCircularQueue<ElementType, CapacityLog2, CounterType>::pop()
{
	XAssert(!isEmpty());
	ElementType& value = buffer[headCounter & CounterToIndexMask];
	headCounter++;
	return value;
}

template <typename ElementType, uint8 CapacityLog2, typename CounterType>
inline ElementType& XLib::InplaceCircularQueue<ElementType, CapacityLog2, CounterType>::peek()
{
	XAssert(!isEmpty());
	return buffer[headCounter & CounterToIndexMask];
}
