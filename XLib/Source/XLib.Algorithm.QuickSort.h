#pragma once

#include "XLib.h"

// TODO: investigate performance

template <typename ElementType>
void QuickSort(ElementType* buffer, uint32 elementsCount)
{
	struct Internal
	{
		static void QuickSort(ElementType* buffer, sint32 lo, sint32 hi)
		{
			sint32 i = lo, j = hi;
			ElementType pivot = buffer[(lo + hi) / 2];

			do
			{
				while (buffer[i] < pivot)
					i++;
				while (pivot < buffer[j])
					j--;
				if (i <= j)
				{
					swap(buffer[i], buffer[j]);
					i++; j--;
				}
			} while (i <= j);

			if (lo < j)
				Internal::QuickSort(buffer, lo, j);
			if (i < hi)
				Internal::QuickSort(buffer, i, hi);
		}
	};

	Internal::QuickSort(buffer, 0, elementsCount - 1);
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