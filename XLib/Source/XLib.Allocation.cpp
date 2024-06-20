#include <Windows.h>

#include "XLib.Allocation.h"

using namespace XLib;

// TODO: Proper out of memory handling?

void* SystemHeapAllocator::Allocate(uintptr size)
{
	if (!size)
		return nullptr;

	return HeapAlloc(GetProcessHeap(), 0, size);
}

void SystemHeapAllocator::Release(void* ptr)
{
	XAssert(ptr);
	HeapFree(GetProcessHeap(), 0, ptr);
}

bool SystemHeapAllocator::TryResize(void* ptr, uintptr size)
{
	XAssert(ptr);
	XAssert(size);
	return HeapReAlloc(GetProcessHeap(), HEAP_REALLOC_IN_PLACE_ONLY, ptr, size) != nullptr;
}

void* SystemHeapAllocator::Reallocate(void* ptr, uintptr size)
{
	if (!size)
	{
		if (ptr)
			HeapFree(GetProcessHeap(), 0, ptr);
		return nullptr;
	}

	if (ptr)
		return HeapReAlloc(GetProcessHeap(), 0, ptr, size);
	else
		return HeapAlloc(GetProcessHeap(), 0, size);
}
