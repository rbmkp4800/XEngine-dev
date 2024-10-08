#include <intrin.h>
#include <Windows.h>

#include "XLib.h"

using namespace XLib;

uint8 countLeadingZeros32(uint32 value) { return uint8(__lzcnt(value)); }
uint8 countLeadingZeros64(uint64 value) { return uint8(__lzcnt64(value)); }
uint8 countTrailingZeros32(uint32 value) { return uint8(_tzcnt_u32(value)); }
uint8 countTrailingZeros64(uint64 value) { return uint8(_tzcnt_u64(value)); }

void memorySet(void* memory, byte value, uintptr size) { memset(memory, value, size); }
void memoryCopy(void* destination, const void* source, uintptr size) { memcpy(destination, source, size); }
void memoryMove(void* destination, const void* source, uintptr size) { memmove(destination, source, size); }

Debug::FailureHandler Debug::failureHandler = Debug::DefaultFailureHandler;

void Debug::Output(const char* messageCStr)
{
	::OutputDebugStringA(messageCStr);
}

void Debug::DefaultFailureHandler(const char* messageCStr)
{
	if (messageCStr)
	{
		if (::IsDebuggerPresent())
			::OutputDebugStringA(messageCStr);

		{
			HANDLE hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD messageLength = DWORD(::strlen(messageCStr));
			DWORD dummy = 0;
			::WriteFile(hStdOut, messageCStr, messageLength, &dummy, nullptr);
		}
	}
	::DebugBreak();
}
