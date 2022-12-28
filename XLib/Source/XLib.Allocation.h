#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"

namespace XLib
{
	// FixedBlockDynamicAllocator
	// FixedBlockLinearAllocator
	// FixedBlockPoolAllocator
	//
	// MultiBlockDynamicAllocator
	// MultiBlockLinearAllocator (AKA ArenaAllocator)
	// MultiBlockPoolAllocator
	//
	// VirtualMemoryBlockDynamicAllocator
	// VirtualMemoryBlockLinearAllocator
	// VirtualMemoryBlockPoolAllocator

	class DefaultAllocator abstract final
	{
	public:
		static void* Allocate(uintptr size, uint32 alignment = 0);
		static void Release(void* ptr);
		static bool TryResize(void* ptr, uintptr newSize);
		static void* Reallocate(void* ptr, uintptr newSize, uint32 newAlignment = 0);

		template <typename AllocatorType>
		static void Override();
	};

	class SystemHeapAllocator abstract final
	{
		static void* Allocate(uintptr size, uint32 alignment = 0);
		static void Release(void* ptr);
		static bool TryResize(void* ptr, uintptr newSize);
		static void* Reallocate(void* ptr, uintptr newSize, uint32 newAlignment = 0);
	};

	class FixedBlockLinearAllocator : public XLib::NonCopyable
	{
	private:
		FixedBlockLinearAllocator() = default;
		~FixedBlockLinearAllocator() = default;

		void* allocate(uintptr size, uint32 alignment = 0);
		void reset();
	};

	class MultiBlockLinearAllocator : public XLib::NonCopyable
	{
	private:
		MultiBlockLinearAllocator() = default;
		~MultiBlockLinearAllocator() = default;

		void* allocate(uintptr size, uint32 alignment = 0);
		void reset();
	};
}
