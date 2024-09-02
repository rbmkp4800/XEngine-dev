#include "XLib.CharStream.h"

#include "XLib.Allocation.h"
#include "XLib.System.Threading.Atomics.h"

using namespace XLib;


// Memory streams //////////////////////////////////////////////////////////////////////////////////

void MemoryCharStreamWriter::nullTerminate()
{
	if (bufferOffset < bufferSize)
		buffer[bufferOffset] = 0;
	else
		buffer[bufferSize - 1] = 0;
}

void MemoryCharStreamWriter::write(const char* data, uintptr length)
{
	const uintptr clippedLength = min<uintptr>(length, bufferSize - bufferOffset);
	memoryCopy(buffer + bufferOffset, data, clippedLength);
}

void MemoryCharStreamWriter::write(const char* cstr)
{
	const char *srcIt = cstr;
	char* bufferIt = buffer + bufferOffset;
	char* bufferEnd = buffer + bufferSize;
	while (*srcIt && bufferIt != bufferEnd)
	{
		*bufferIt = *srcIt;
		srcIt++;
		bufferIt++;
	}

	bufferOffset = bufferIt - buffer;
}


// File streams ////////////////////////////////////////////////////////////////////////////////////

bool FileCharStreamReader::open(const char* name, uint32 bufferSize, void* externalBuffer)
{
	XAssert(bufferSize >= 64);

	close();

	const FileOpenResult fileOpenResult = File::Open(name, FileAccessMode::Read, FileOpenMode::OpenExisting);
	if (!fileOpenResult.status)
		return false;

	this->fileHandle = fileOpenResult.handle;
	this->fileHandleIsInternal = true;

	this->bufferIsInternal = externalBuffer == nullptr;
	this->buffer = (char*)(bufferIsInternal ? SystemHeapAllocator::Allocate(bufferSize) : externalBuffer);

	this->bufferSize = bufferSize;
	this->bufferOffset = 0;

	return true;
}

void FileCharStreamReader::open(FileHandle fileHandle, uint32 bufferSize, void* externalBuffer)
{
	XAssert(bufferSize >= 64);

	close();

	this->fileHandle = fileHandle;
	this->fileHandleIsInternal = false;

	this->bufferIsInternal = externalBuffer == nullptr;
	this->buffer = (char*)(bufferIsInternal ? SystemHeapAllocator::Allocate(bufferSize) : externalBuffer);

	this->bufferSize = bufferSize;
	this->bufferOffset = 0;
}

void FileCharStreamReader::close()
{
	if (fileHandleIsInternal && fileHandle != FileHandle::Zero)
		File::Close(fileHandle);

	if (bufferIsInternal && buffer)
		SystemHeapAllocator::Release(buffer);

	memorySet(this, 0, sizeof(*this));
}


bool FileCharStreamWriter::open(const char* name, bool overrideFileContent, uint32 bufferSize, void* externalBuffer)
{
	XAssert(bufferSize >= 64);

	close();

	const FileOpenResult fileOpenResult = File::Open(name, FileAccessMode::Write, overrideFileContent ? FileOpenMode::Override : FileOpenMode::OpenExisting);
	if (!fileOpenResult.status)
		return false;

	this->fileHandle = fileOpenResult.handle;
	this->fileHandleIsInternal = true;

	this->bufferIsInternal = externalBuffer == nullptr;
	this->buffer = (char*)(bufferIsInternal ? SystemHeapAllocator::Allocate(bufferSize) : externalBuffer);

	this->bufferSize = bufferSize;
	this->bufferOffset = 0;

	return true;
}

void FileCharStreamWriter::open(FileHandle fileHandle, uint32 bufferSize, void* externalBuffer)
{
	XAssert(bufferSize >= 64);

	close();

	this->fileHandle = fileHandle;
	this->fileHandleIsInternal = false;

	this->bufferIsInternal = externalBuffer == nullptr;
	this->buffer = (char*)(bufferIsInternal ? SystemHeapAllocator::Allocate(bufferSize) : externalBuffer);

	this->bufferSize = bufferSize;
	this->bufferOffset = 0;
}

void FileCharStreamWriter::close()
{
	flush();

	if (fileHandleIsInternal && fileHandle != FileHandle::Zero)
		File::Close(fileHandle);

	if (bufferIsInternal && buffer)
		SystemHeapAllocator::Release(buffer);

	memorySet(this, 0, sizeof(*this));
}

void FileCharStreamWriter::flush()
{
	if (bufferOffset > 0)
		File::Write(fileHandle, buffer, bufferOffset);
	bufferOffset = 0;
}

void FileCharStreamWriter::write(const char* data, uintptr length)
{
	if (length > bufferSize)
	{
		File::Write(fileHandle, buffer, bufferOffset);
		File::Write(fileHandle, data, length);
		bufferOffset = 0;
	}
	else
	{
		const uint32 bufferFreeSpaceSize = bufferSize - bufferOffset;
		if (length >= bufferFreeSpaceSize)
		{
			const uint32 firstCopySize = bufferFreeSpaceSize;
			const uint32 secondCopySize = uint32(length) - firstCopySize;
			File::Write(fileHandle, buffer, bufferSize);
			bufferOffset = secondCopySize;
			if (secondCopySize > 0)
				memoryCopy(buffer, data + firstCopySize, secondCopySize);
		}
		else
		{
			memoryCopy(buffer + bufferOffset, data, length);
			bufferOffset += uint32(length);
		}
	}

	XAssert(bufferOffset < bufferSize);
}

void FileCharStreamWriter::write(const char* cstr)
{
	const char *srcIt = cstr;
	char* bufferIt = buffer + bufferOffset;
	char* bufferEnd = buffer + bufferSize;

	for (;;)
	{
		while (*srcIt && bufferIt != bufferEnd)
		{
			*bufferIt = *srcIt;
			srcIt++;
			bufferIt++;
		}

		bufferOffset = uint32(bufferIt - buffer);

		if (bufferOffset == bufferSize)
		{
			File::Write(fileHandle, buffer, bufferOffset);
			bufferOffset = 0;
			bufferIt = buffer;
		}

		if (!*srcIt)
			break;
	}

	XAssert(bufferOffset < bufferSize);
}

void VirtualStringWriter::growBufferExponentially(uint32 minRequiredBufferSize)
{
	if (minRequiredBufferSize <= bufferSize)
		return;
	XAssert(minRequiredBufferSize <= maxBufferSize);

	// TODO: Maybe align up to 16 or some adaptive pow of 2.
	// TODO: Revisit this.

	uint32 newBufferSize = minRequiredBufferSize;
	const uint32 expGrownBufferSize = min<uint32>(bufferSize * BufferExponentialGrowthFactor, maxBufferSize);
	if (newBufferSize < expGrownBufferSize)
		newBufferSize = expGrownBufferSize;

	virtualStringRef.growBuffer(newBufferSize);
	buffer = virtualStringRef.getBuffer();
	bufferSize = virtualStringRef.getBufferSize();
	XAssert(bufferSize >= newBufferSize);
}

void VirtualStringWriter::write(const char* data, uintptr length)
{
	const uint32 lengthU32 = XCheckedCastU32(length);

	uint32 clippedLength = lengthU32;
	const uint32 requiredBufferSize = bufferOffset + lengthU32 + 1;
	if (bufferSize < requiredBufferSize)
	{
		const bool overflow = maxBufferSize < requiredBufferSize;
		growBufferExponentially(overflow ? maxBufferSize : requiredBufferSize);

		if (overflow)
		{
			const uint32 maxLength = maxBufferSize - 1;
			clippedLength = maxLength - bufferOffset;
		}
	}

	memoryCopy(buffer + bufferOffset, data, clippedLength);
	bufferOffset += lengthU32;
}

void VirtualStringWriter::write(const char* cstr)
{
	const char *srcIt = cstr;
	char* bufferIt = buffer + bufferOffset;
	char* bufferEnd = buffer + bufferSize - 1;

	while (*srcIt)
	{
		if (bufferIt == bufferEnd)
		{
			bufferOffset = uint32(bufferIt - buffer);

			if (bufferSize == maxBufferSize)
				return;

			const uint32 requiredBufferSize = bufferOffset + 2;
			const bool overflow = maxBufferSize < requiredBufferSize;
			growBufferExponentially(overflow ? maxBufferSize : requiredBufferSize);

			bufferIt = buffer + bufferOffset;
			bufferEnd = buffer + bufferSize - 1;
		}

		*bufferIt = *srcIt;
		srcIt++;
		bufferIt++;
	}

	bufferOffset = uint32(bufferIt - buffer);
}


////////////////////////////////////////////////////////////////////////////////////////////////////

void VirtualStringWriter::open(VirtualStringRefASCII virtualStringRef)
{
	this->virtualStringRef = virtualStringRef;
	buffer = virtualStringRef.getBuffer();
	bufferSize = virtualStringRef.getBufferSize();
	bufferOffset = virtualStringRef.getLength();
	maxBufferSize = virtualStringRef.getMaxBufferSize();
}

void VirtualStringWriter::flush()
{
	virtualStringRef.setLength(bufferOffset);
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
