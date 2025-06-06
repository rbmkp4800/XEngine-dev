#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"

// TODO: Rename file to XLib.Allocation.Heap.h / XLib.HeapAllocation.h. Move linear stuff separately
// TODO: Rename `DefaultAllocator` to `DefaultHeapAllocator`.
// TODO: We sould have separate "Heap allocator" term.

namespace XLib
{
	// TODO: HeapAllocatorAdapterBase
	template <typename AllocatorType, bool AllocatorIsStatic = AllocatorType::IsStatic>
	class AllocatorAdapterBase { };

	template <typename AllocatorType>
	class AllocatorAdapterBase<AllocatorType, true>
	{
	protected:
		AllocatorAdapterBase() = default;
		~AllocatorAdapterBase() = default;

		void* allocate(uintptr size) { return AllocatorType::Allocate(size); }
		void* reallocate(void* ptr, uintptr size) { return AllocatorType::Reallocate(ptr, size); }
		bool tryReallocateInplace(void* ptr, uintptr size) { return AllocatorType::TryReallocateInplace(ptr, size); }
		void release(void* ptr) { AllocatorType::Release(ptr); }
	};

	template <typename AllocatorType>
	class AllocatorAdapterBase<AllocatorType, false>
	{
	private:
		AllocatorType* allocatorInstance = nullptr;

	protected:
		AllocatorAdapterBase() = default;
		~AllocatorAdapterBase() = default;

		void* allocate(uintptr size) { return allocatorInstance->allocate(size); }
		void* reallocate(void* ptr, uintptr size) { return allocatorInstance->reallocate(ptr, size); }
		bool tryReallocateInplace(void* ptr, uintptr size) { return allocatorInstance->tryReallocateInplace(ptr, size); }
		void release(void* ptr) { allocatorInstance->release(ptr); }
	};


	////////////////////////////////////////////////////////////////////////////////////////////////

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

	// TODO: DefaultHeapAllocator
	class DefaultAllocator abstract final
	{
	public:
		static void* Allocate(uintptr size, uint32 alignment = 0);
		static void Release(void* ptr);
		static void* Reallocate(void* ptr, uintptr newSize, uint32 newAlignment = 0);
		static bool TryReallocateInplace(void* ptr, uintptr newSize);

		template <typename AllocatorType>
		static void Override();
	};

	struct SystemHeapAllocator abstract final
	{
		static constexpr bool IsStatic = true;

		static void* Allocate(uintptr size);
		static void Release(void* ptr);
		static void* Reallocate(void* ptr, uintptr size);
		static bool TryReallocateInplace(void* ptr, uintptr size);
	};

	template <typename Type, typename HeapAllocatorType>
	class HeapPtr
	{

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
