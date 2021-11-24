#include <Windows.h>

#include "XLib.SystemHeapAllocator.h"

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
	HeapFree(GetProcessHeap(), 0, ptr);
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

bool SystemHeapAllocator::ReallocateInplace(void* ptr, uintptr size)
{
	return HeapReAlloc(GetProcessHeap(), HEAP_REALLOC_IN_PLACE_ONLY, ptr, size) != nullptr;
}
