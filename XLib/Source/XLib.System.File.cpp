#include <Windows.h>

#include "XLib.System.File.h"

#undef min
#undef max

// TODO: Make StdIn, StdOut, StdErr getters thread safe.

using namespace XLib;

/*namespace
{
	File StdIn;
	File StdOut;
	File StdErr;

	bool StdInIsInitialized = false;
	bool StdOutIsInitialized = false;
	bool StdErrIsInitialized = false;
}*/

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
		const DWORD iterSizeToRead = DWORD(max<uintptr>(bufferSize - totalReadSizeAccum, 0x8000'0000));
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
		const DWORD iterSizeToWrite = DWORD(max<uintptr>(size - totalWrittenSizeAccum, 0x8000'0000));
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
	if (!fileOpenResult.openResult)
		return false;
	handle = fileOpenResult.fileHandle;
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

FileHandle XLib::GetStdInFileHandle()
{
	static HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdIn == INVALID_HANDLE_VALUE)
		return FileHandle::Zero;
	return FileHandle(uintptr(hStdIn));
}

FileHandle XLib::GetStdOutFileHandle()
{
	static HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdOut == INVALID_HANDLE_VALUE)
		return FileHandle::Zero;
	return FileHandle(uintptr(hStdOut));
}

FileHandle XLib::GetStdErrFileHandle()
{
	static HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
	if (hStdErr == INVALID_HANDLE_VALUE)
		return FileHandle::Zero;
	return FileHandle(uintptr(hStdErr));
}
