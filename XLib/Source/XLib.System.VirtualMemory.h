#pragma once

#include "XLib.h"

struct VirtualMemory abstract final
{
	static void* Allocate(uintptr size);
	static void Release();
	static void* Reserve(uintptr size);
	static void Commit(void* block, uintptr size);
	static void Decomit();
};