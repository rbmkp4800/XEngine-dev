#include <intrin.h>
#include <memory.h>
#include <string.h>

#include "XLib.h"

using namespace XLib;

uint8 countLeadingZeros32(uint32 value) { return uint8(__lzcnt(value)); }
uint8 countLeadingZeros64(uint32 value) { return uint8(__lzcnt64(value)); }
uint8 countTrailingZeros32(uint32 value) { return uint8(_tzcnt_u32(value)); }
uint8 countTrailingZeros64(uint64 value) { return uint8(_tzcnt_u64(value)); }

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