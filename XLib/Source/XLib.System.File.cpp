#include <Windows.h>

#include "XLib.System.File.h"

#include "XLib.Debug.h"

using namespace XLib;

static_assert(DWORD(FileAccessMode::Read) == GENERIC_READ, "FileAccessMode::Read must be equal to GENERIC_READ");
static_assert(DWORD(FileAccessMode::Write) == GENERIC_WRITE, "FileAccessMode::Write must be equal to GENERIC_WRITE");
static_assert(DWORD(FileAccessMode::ReadWrite) == (GENERIC_READ | GENERIC_WRITE), "FileAccessMode::ReadWrite must be equal to GENERIC_READ | GENERIC_WRITE");

static_assert(DWORD(FileOpenMode::Override) == CREATE_ALWAYS, "FileOpenMode::Override must be equal to CREATE_ALWAYS");
static_assert(DWORD(FileOpenMode::OpenExisting) == OPEN_EXISTING, "FileOpenMode::OpenExisting must be equal to OPEN_EXISTING");

static_assert(DWORD(FilePosition::Begin) == FILE_BEGIN, "FilePosition::Begin must be equal to FILE_BEGIN");
static_assert(DWORD(FilePosition::Current) == FILE_CURRENT, "FilePosition::Current must be equal to FILE_CURRENT");
static_assert(DWORD(FilePosition::End) == FILE_END, "FilePosition::End must be equal to FILE_END");

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

bool File::read(void* buffer, uint32 size)
{
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

bool File::read(void* buffer, uint32 bufferSize, uint32& readSize)
{
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

bool File::write(const void* buffer, uint32 size)
{
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