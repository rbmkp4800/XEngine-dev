#include "XLib.CharStream.h"

#include "XLib.Allocation.h"
#include "XLib.System.Threading.Atomics.h"

using namespace XLib;

// File streams ////////////////////////////////////////////////////////////////////////////////////

bool FileCharStreamReader::open(const char* name, uint32 bufferSize, void* externalBuffer)
{
	XAssert(bufferSize >= 64);
	close();

	const FileOpenResult fileOpenResult = File::Open(name, FileAccessMode::Read, FileOpenMode::OpenExisting);
	if (!fileOpenResult.status)
		return false;

	fileHandle = fileOpenResult.handle;
	fileHandleIsInternal = true;

	bufferIsInternal = externalBuffer == nullptr;
	buffer = (char*)(bufferIsInternal ? externalBuffer : SystemHeapAllocator::Allocate(bufferSize));

	this->bufferSize = bufferSize;
	this->bufferOffet = 0;
}

void FileCharStreamReader::open(FileHandle fileHandle, uint32 bufferSize, void* externalBuffer)
{
	close();

	this->fileHandle = fileHandle;
	this->fileHandleIsInternal = false;

	this->bufferIsInternal = externalBuffer == nullptr;
	this->buffer = (char*)(bufferIsInternal ? externalBuffer : SystemHeapAllocator::Allocate(bufferSize));

	this->bufferSize = bufferSize;
	this->bufferOffet = 0;
}

void FileCharStreamReader::close()
{
	if (fileHandleIsInternal && fileHandle != FileHandle::Zero)
	{
		File::Close(fileHandle);
	}

	if (bufferIsInternal && buffer)
	{
		SystemHeapAllocator::Release(buffer);
	}

	memorySet(this, 0, sizeof(*this));
}

void FileCharStreamWriter::open(FileHandle fileHandle, uint32 bufferSize, void* externalBuffer)
{
	close();

	this->fileHandle = fileHandle;
	this->fileHandleIsInternal = false;

	this->bufferIsInternal = externalBuffer == nullptr;
	this->buffer = (char*)(bufferIsInternal ? externalBuffer : SystemHeapAllocator::Allocate(bufferSize));

	this->bufferSize = bufferSize;
	this->bufferOffet = 0;
}

void FileCharStreamWriter::close()
{
	if (fileHandleIsInternal && fileHandle != FileHandle::Zero)
	{
		File::Close(fileHandle);
	}

	if (bufferIsInternal && buffer)
	{
		SystemHeapAllocator::Release(buffer);
	}

	memorySet(this, 0, sizeof(*this));
}

void FileCharStreamWriter::flush()
{
	File::Write(fileHandle, buffer, bufferOffet);
	bufferOffet = 0;
}


// Std streams /////////////////////////////////////////////////////////////////////////////////////

static FileCharStreamReader StdInStream;
static FileCharStreamWriter StdOutStream;
static FileCharStreamWriter StdErrStream;

static byte StdInStreamBuffer[4096];
static byte StdOutStreamBuffer[4096];
static byte StdErrStreamBuffer[4096];

static volatile uint16 StdInStreamInitGuard = 0;
static volatile uint16 StdOutStreamInitGuard = 0;
static volatile uint16 StdErrStreamInitGuard = 0;

FileCharStreamReader& XLib::GetStdInStream()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdInStreamInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdInStreamInitGuard, 0, 1))
			{
				StdInStream.open(GetStdInFileHandle(), sizeof(StdInStreamBuffer), StdInStreamBuffer);
				Atomics::Store<uint16>(StdInStreamInitGuard, 2);
			}
		}
	}

	return StdInStream;
}

FileCharStreamWriter& XLib::GetStdOutStream()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdOutStreamInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdOutStreamInitGuard, 0, 1))
			{
				StdOutStream.open(GetStdOutFileHandle(), sizeof(StdOutStreamBuffer), StdOutStreamBuffer);
				Atomics::Store<uint16>(StdOutStreamInitGuard, 2);
			}
		}
	}

	return StdOutStream;
}

FileCharStreamWriter& XLib::GetStdErrStream()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdErrStreamInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdErrStreamInitGuard, 0, 1))
			{
				StdErrStream.open(GetStdErrFileHandle(), sizeof(StdErrStreamBuffer), StdErrStreamBuffer);
				Atomics::Store<uint16>(StdErrStreamInitGuard, 2);
			}
		}
	}

	return StdErrStream;
}
