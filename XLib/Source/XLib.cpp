#include <intrin.h>
#include <Windows.h>

#include "XLib.h"

using namespace XLib;

uint8 countLeadingZeros32(uint32 value) { return uint8(__lzcnt(value)); }
uint8 countLeadingZeros64(uint32 value) { return uint8(__lzcnt64(value)); }
uint8 countTrailingZeros32(uint32 value) { return uint8(_tzcnt_u32(value)); }
uint8 countTrailingZeros64(uint64 value) { return uint8(_tzcnt_u64(value)); }

void memorySet(void* memory, byte value, uintptr size) { memset(memory, value, size); }
void memoryCopy(void* destination, const void* source, uintptr size) { memcpy(destination, source, size); }
void memoryMove(void* destination, const void* source, uintptr size) { memmove(destination, source, size); }

Debug::FailureHandler Debug::failureHandler = Debug::DefaultFailureHandler;

void Debug::Output(const char* message)
{
	OutputDebugStringA(message);
}

void Debug::DefaultFailureHandler(const char* message)
{
	if (message)
	{
		if (::IsDebuggerPresent())
			OutputDebugStringA(message);
	}
	::DebugBreak();
}
