#pragma once

#include "XLib.h"

// This code is broken (ref to pivot breaks quicksort).
#if 0

// TODO: Cleanup this mess... I wrote this code in 2015... Add rvalue support at least...

namespace XLib
{
	namespace Internal
	{
		template <typename ElementType, typename ComparatorType>
		void QuickSort(ElementType* buffer, sint32 lo, sint32 hi, const ComparatorType& comparator)
		{
			sint32 i = lo, j = hi;
			const ElementType& pivot = buffer[(lo + hi) / 2];

			do
			{
				while (comparator(buffer[i], pivot))
					i++;
				while (comparator(pivot, buffer[j]))
					j--;
				if (i <= j)
				{
					swap(buffer[i], buffer[j]);
					i++; j--;
				}
			} while (i <= j);

			if (lo < j)
				Internal::QuickSort(buffer, lo, j, comparator);
			if (i < hi)
				Internal::QuickSort(buffer, i, hi, comparator);
		}
	}

	template <typename ElementType>
	void QuickSort(ElementType* buffer, uint32 elementsCount)
	{
		Internal::QuickSort(buffer, 0, elementsCount - 1, [](const ElementType& left, const ElementType& right) -> bool { return left < right; });
	}

	// comparator (a, b) -> bool = a < b;
	template <typename ElementType, typename ComparatorType>
	void QuickSort(ElementType* buffer, uint32 elementsCount, const ComparatorType& comparator)
	{
		Internal::QuickSort(buffer, 0, elementsCount - 1, comparator);
	}
}

/*

template <typename BufferType>
void QuickSort(BufferType& buffer, uint32 elementsCount)
{
	using ElementType = typename RemoveReference<decltype(buffer[0])>::Type;

	constexpr uint32 maxQSortElementsLog2 = 32;
	struct StackFrame
	{
		sint32 low, high;
		inline void set(sint32 _low, sint32 _high) { low = _low; high = _high; }
	} stack[maxQSortElementsLog2 * 2];
	stack[0].set(0, elementsCount - 1);

	sint32 stackHigh = 1;
	while (stackHigh >= 1)
	{
		stackHigh--;
		StackFrame frame = stack[stackHigh];
		sint32 i = frame.low, j = frame.high;
		ElementType pivot = buffer[(i + j) / 2];
		do
		{
			while (pivot > buffer[i]) i++;
			while (buffer[j] > pivot) j--;
			if (i <= j)
			{
				swap(buffer[i], buffer[j]);
				i++;
				j--;
			}
		} while (i <= j);
		if (i < frame.high)
			stack[stackHigh++].set(i, frame.high);
		if (frame.low < j)
			stack[stackHigh++].set(frame.low, j);
	}
}

*/

#endif