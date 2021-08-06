#pragma once

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
}
