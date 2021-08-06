#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"

namespace XLib
{
	struct SystemHeapAllocator abstract final
	{
		static constexpr bool IsStatic = true;

		static void* Allocate(uintptr size);
		static void Release(void* ptr);
		static void* Reallocate(void* ptr, uintptr size);
		static bool ReallocateInplace(void* ptr, uintptr size);
	};
}