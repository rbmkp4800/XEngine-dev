#include <Windows.h>

#include "XLib.System.File.h"

// TODO: Make StdIn, StdOut, StdErr getters thread safe.

using namespace XLib;

static_assert(DWORD(FileAccessMode::Read) == GENERIC_READ, "FileAccessMode::Read must be equal to GENERIC_READ");
static_assert(DWORD(FileAccessMode::Write) == GENERIC_WRITE, "FileAccessMode::Write must be equal to GENERIC_WRITE");
static_assert(DWORD(FileAccessMode::ReadWrite) == (GENERIC_READ | GENERIC_WRITE), "FileAccessMode::ReadWrite must be equal to GENERIC_READ | GENERIC_WRITE");

static_assert(DWORD(FileOpenMode::Override) == CREATE_ALWAYS, "FileOpenMode::Override must be equal to CREATE_ALWAYS");
static_assert(DWORD(FileOpenMode::OpenExisting) == OPEN_EXISTING, "FileOpenMode::OpenExisting must be equal to OPEN_EXISTING");

static_assert(DWORD(FilePosition::Begin) == FILE_BEGIN, "FilePosition::Begin must be equal to FILE_BEGIN");
static_assert(DWORD(FilePosition::Current) == FILE_CURRENT, "FilePosition::Current must be equal to FILE_CURRENT");
static_assert(DWORD(FilePosition::End) == FILE_END, "FilePosition::End must be equal to FILE_END");

namespace
{
	File StdIn;
	File StdOut;
	File StdErr;

	bool StdInIsInitialized = false;
	bool StdOutIsInitialized = false;
	bool StdErrIsInitialized = false;
}

bool File::open(const char* name, FileAccessMode accessMode, FileOpenMode openMode)
{
	handle = CreateFileA(name, DWORD(accessMode), 0, nullptr, DWORD(openMode), FILE_ATTRIBUTE_NORMAL, nullptr);
	return handle != INVALID_HANDLE_VALUE;
}

void File::close()
{
	if (handle)
		CloseHandle(handle);
	handle = nullptr;
}

bool File::read(void* buffer, uintptr _size)
{
	// TODO: Do this properly.
	if (_size > uint32(-1))
		*(int*)nullptr = 0;
	const uint32 size = uint32(_size);

	DWORD readSize = 0;
	BOOL result = ReadFile(handle, buffer, size, &readSize, nullptr);
	if (!result)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	if (readSize != size)
	{
		// Debug::Warning(...);
		return false;
	}
	return true;
}

bool File::read(void* buffer, uintptr _bufferSize, uintptr& readSize)
{
	// TODO: Do this properly.
	if (_bufferSize > uint32(-1))
		*(int*)nullptr = 0;
	const uint32 bufferSize = uint32(_bufferSize);

	DWORD _readSize = 0;
	BOOL result = ReadFile(handle, buffer, bufferSize, &_readSize, nullptr);
	if (!result)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	readSize = _readSize;
	return true;
}

bool File::write(const void* buffer, uintptr _size)
{
	// TODO: Do this properly.
	if (_size > uint32(-1))
		*(int*)nullptr = 0;
	const uint32 size = uint32(_size);

	DWORD writtenSize = 0;
	BOOL result = WriteFile(handle, buffer, size, &writtenSize, nullptr);
	if (!result)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	if (writtenSize != size)
		return false;
	return true;
}

void File::flush()
{
	//if (!FlushFileBuffers(handle))
	//	Debug::LogLastSystemError(SysErrorDbgMsgFmt);
}

uint64 File::getSize()
{
	LARGE_INTEGER size;
	if (!GetFileSizeEx(handle, &size))
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return uint64(-1);
	}
	return size.QuadPart;
}

uint64 File::getPosition()
{
	LARGE_INTEGER distance, postion;
	distance.QuadPart = 0;
	if (!SetFilePointerEx(handle, distance, &postion, FILE_CURRENT))
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return uint64(-1);
	}
	return postion.QuadPart;
}

uint64 File::setPosition(sint64 offset, FilePosition origin)
{
	LARGE_INTEGER distance, postion;
	distance.QuadPart = offset;
	if (!SetFilePointerEx(handle, distance, &postion, DWORD(origin)))
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return uint64(-1);
	}
	return postion.QuadPart;
}

File& File::GetStdIn()
{
	if (!StdInIsInitialized)
	{
		const HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
		if (hStdIn != INVALID_HANDLE_VALUE)
			StdIn.handle = hStdIn;
		StdInIsInitialized = true;
	}
	return StdIn;
}

File& File::GetStdOut()
{
	if (!StdOutIsInitialized)
	{
		const HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hStdOut != INVALID_HANDLE_VALUE)
			StdOut.handle = hStdOut;
		StdOutIsInitialized = true;
	}
	return StdOut;
}

File& File::GetStdErr()
{
	if (!StdErrIsInitialized)
	{
		const HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
		if (hStdErr != INVALID_HANDLE_VALUE)
			StdErr.handle = hStdErr;
		StdErrIsInitialized = true;
	}
	return StdErr;
}
