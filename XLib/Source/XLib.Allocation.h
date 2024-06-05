#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"

namespace XLib
{
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
		bool reallocateInplace(void* ptr, uintptr size) { return AllocatorType::ReallocateInplace(ptr, size); }
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
		bool reallocateInplace(void* ptr, uintptr size) { return allocatorInstance->reallocateInplace(ptr, size); }
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

	struct SystemHeapAllocator abstract final
	{
		static constexpr bool IsStatic = true;

		static void* Allocate(uintptr size);
		static void Release(void* ptr);
		static bool TryResize(void* ptr, uintptr size);
		static void* Reallocate(void* ptr, uintptr size);
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
