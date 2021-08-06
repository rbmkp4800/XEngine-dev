#pragma once

#include "XLib.h"

namespace XLib
{
	class Memory abstract final
	{
	public:
		static void Set(void* memory, byte value, uintptr size);
		static void Copy(void* destination, const void* source, uintptr size);
		static void Move(void* destination, const void* source, uintptr size);
	};
}