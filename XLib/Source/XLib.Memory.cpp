#include <memory.h>
#include <string.h>

#include "XLib.Memory.h"

using namespace XLib;

void Memory::Set(void* memory, byte value, uintptr size)
{
	memset(memory, value, size);
}

void Memory::Copy(void* destination, const void* source, uintptr size)
{
	memcpy(destination, source, size);
}

void Memory::Move(void* destination, const void* source, uintptr size)
{
	memmove(destination, source, size);
}