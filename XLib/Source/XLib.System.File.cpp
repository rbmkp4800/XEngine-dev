#include <Windows.h>

#include "XLib.System.File.h"

#include "XLib.System.Threading.Atomics.h"

#undef min
#undef max

using namespace XLib;

FileOpenResult File::Open(const char* name, FileAccessMode accessMode, FileOpenMode openMode)
{
	DWORD winDesiredAccess = 0;
	switch (accessMode)
	{
		case FileAccessMode::Read:		winDesiredAccess = GENERIC_READ;					break;
		case FileAccessMode::Write:		winDesiredAccess = GENERIC_WRITE;					break;
		case FileAccessMode::ReadWrite:	winDesiredAccess = GENERIC_READ | GENERIC_WRITE;	break;
		default: XAssertUnreachableCode();
	}

	DWORD winCreationDisposition = 0;
	switch (openMode)
	{
		case FileOpenMode::OpenExisting:	winCreationDisposition = OPEN_EXISTING;	break;
		case FileOpenMode::Override:		winCreationDisposition = CREATE_ALWAYS;	break;
		default: XAssertUnreachableCode();
	}

	HANDLE hFile = CreateFileA(name, winDesiredAccess, 0, nullptr, winCreationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return FileOpenResult { FileHandle::Zero, false };

	XAssert(hFile);
	return FileOpenResult { FileHandle(uintptr(hFile)), true };
}

void File::Close(FileHandle fileHandle)
{
	XAssert(fileHandle != FileHandle::Zero);
	CloseHandle(HANDLE(fileHandle));
}

bool File::Read(FileHandle fileHandle, void* buffer, uintptr bufferSize, uintptr* outReadSize)
{
	XAssert(fileHandle != FileHandle::Zero);

	uintptr totalReadSizeAccum = 0;
	while (totalReadSizeAccum < bufferSize)
	{
		const DWORD iterSizeToRead = DWORD(min<uintptr>(bufferSize - totalReadSizeAccum, 0x8000'0000));
		DWORD iterReadSize = 0;

		if (!ReadFile(HANDLE(fileHandle), buffer, iterSizeToRead, &iterReadSize, nullptr))
			return false;

		totalReadSizeAccum += iterReadSize;

		if (iterReadSize < iterSizeToRead)
			break;
	}

	if (outReadSize)
		*outReadSize = totalReadSizeAccum;
	else if (totalReadSizeAccum < bufferSize)
		return false; // Unexpected EOF

	return true;
}

bool File::Write(FileHandle fileHandle, const void* data, uintptr size)
{
	XAssert(fileHandle != FileHandle::Zero);

	uintptr totalWrittenSizeAccum = 0;
	while (totalWrittenSizeAccum < size)
	{
		const DWORD iterSizeToWrite = DWORD(min<uintptr>(size - totalWrittenSizeAccum, 0x8000'0000));
		DWORD iterWrittenSize = 0;

		if (!WriteFile(HANDLE(fileHandle), data, iterSizeToWrite, &iterWrittenSize, nullptr))
			return false;

		totalWrittenSizeAccum += iterWrittenSize;

		if (iterWrittenSize < iterSizeToWrite)
			return false;
	}

	return true;
}

bool File::Flush(FileHandle fileHandle)
{
	XAssert(fileHandle != FileHandle::Zero);

	if (!FlushFileBuffers(HANDLE(fileHandle)))
		return false;
	return true;
}

uint64 File::GetSize(FileHandle fileHandle)
{
	XAssert(fileHandle != FileHandle::Zero);

	LARGE_INTEGER size = {};
	if (!GetFileSizeEx(HANDLE(fileHandle), &size))
		return uint64(-1);
	return size.QuadPart;
}

uint64 File::GetPosition(FileHandle fileHandle)
{
	XAssert(fileHandle != FileHandle::Zero);

	LARGE_INTEGER distance = {};
	LARGE_INTEGER position = {};
	if (!SetFilePointerEx(HANDLE(fileHandle), distance, &position, FILE_CURRENT))
		return uint64(-1);
	return position.QuadPart;
}

uint64 File::SetPosition(FileHandle fileHandle, sint64 offset, FileOffsetOrigin origin)
{
	XAssert(fileHandle != FileHandle::Zero);

	DWORD winOrigin = 0;
	switch (origin)
	{
		case FileOffsetOrigin::Begin:	winOrigin = FILE_BEGIN;		break;
		case FileOffsetOrigin::Current:	winOrigin = FILE_CURRENT;	break;
		case FileOffsetOrigin::End:		winOrigin = FILE_END;		break;
		default: XAssertUnreachableCode();
	}

	LARGE_INTEGER distance = {};
	LARGE_INTEGER position = {};
	distance.QuadPart = offset;
	if (!SetFilePointerEx(HANDLE(fileHandle), distance, &position, winOrigin))
		return uint64(-1);
	return position.QuadPart;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

File::File(File&& that)
{
	handle = that.handle;
	that.handle = FileHandle::Zero;
}

void File::operator = (File&& that)
{
	close();
	handle = that.handle;
	that.handle = FileHandle::Zero;
}

bool File::open(const char* name, FileAccessMode accessMode, FileOpenMode openMode)
{
	close();

	FileOpenResult fileOpenResult = File::Open(name, accessMode, openMode);
	if (!fileOpenResult.status)
		return false;
	handle = fileOpenResult.handle;
	return true;
}

void File::close()
{
	if (handle != FileHandle::Zero)
	{
		Close(handle);
		handle = FileHandle::Zero;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static HANDLE StdIn = NULL;
static HANDLE StdOut = NULL;
static HANDLE StdErr = NULL;

static volatile uint16 StdInInitGuard = 0;
static volatile uint16 StdOutInitGuard = 0;
static volatile uint16 StdErrInitGuard = 0;

FileHandle XLib::GetStdInFileHandle()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdInInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdInInitGuard, 0, 1))
			{
				StdIn = GetStdHandle(STD_INPUT_HANDLE);
				XAssert(StdIn);

				Atomics::Store<uint16>(StdInInitGuard, 2);
			}
		}
	}

	return FileHandle(uintptr(StdIn));
}

FileHandle XLib::GetStdOutFileHandle()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdOutInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdOutInitGuard, 0, 1))
			{
				StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
				XAssert(StdOut);

				Atomics::Store<uint16>(StdOutInitGuard, 2);
			}
		}
	}

	return FileHandle(uintptr(StdOut));
}

FileHandle XLib::GetStdErrFileHandle()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdErrInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdErrInitGuard, 0, 1))
			{
				StdErr = GetStdHandle(STD_ERROR_HANDLE);
				XAssert(StdErr);

				Atomics::Store<uint16>(StdErrInitGuard, 2);
			}
		}
	}

	return FileHandle(uintptr(StdErr));
}
