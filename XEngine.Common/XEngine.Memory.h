#pragma once

#include <XLib.h>

namespace XEngine::Memory
{
	enum class HeapToken : uint32 { Zero = 0, };

	class Heap
	{

	};

	class Manager
	{
	public:
		static void Initialize();
		static void RegisterHeap(Heap& heap, uint32 sizeMib, const char* nameCStr);
		static void GetCurrentThreadHeap();
		static void SetCurrentThreadHeap();
	};

	class ScopedHeapSetter
	{
	private:

	public:
		inline ScopedHeapSetter(const Heap& heap) {}
		inline ~ScopedHeapSetter() {}
	};
}

#define XEMemScopeSetHeap(heap)
#define XEMemScopeResetHeap
#define XEMemScopeBlockHeaps
