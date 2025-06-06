#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"

// TODO: Introduce `FileOpStatus` and `FileOpResult` (same is done for FileSystem).

namespace XLib
{
	enum class FileAccessMode : uint32
	{
		Undefined = 0,
		Read,
		Write,
		ReadWrite,
	};

	enum class FileOpenMode : uint32
	{
		Undefined = 0,
		OpenExisting,
		Override,
	};

	enum class FileOffsetOrigin : uint32
	{
		Undefined = 0,
		Begin,
		Current,
		End,
	};

	enum class FileHandle : uint64 { Zero = 0, };

	struct FileOpenResult
	{
		FileHandle handle;
		bool status;
		// TODO: `SystemErrorCode`
	};

	class File : public NonCopyable
	{
	public:
		static FileOpenResult Open(const char* name, FileAccessMode accessMode, FileOpenMode openMode);
		static void Close(FileHandle fileHandle);

		static bool Read(FileHandle fileHandle, void* buffer, uintptr bufferSize, uintptr* outReadSize = nullptr);
		static bool Write(FileHandle fileHandle, const void* buffer, uintptr size);
		static bool Flush(FileHandle fileHandle);

		static uint64 GetSize(FileHandle fileHandle);
		static uint64 GetPosition(FileHandle fileHandle);
		static uint64 SetPosition(FileHandle fileHandle, sint64 offset, FileOffsetOrigin origin = FileOffsetOrigin::Begin);

	private:
		FileHandle handle = FileHandle::Zero;

	public:
		File() = default;
		inline ~File() { close(); }

		inline File(File&& that);
		inline void operator = (File&& that);

		bool open(const char* name, FileAccessMode accessMode, FileOpenMode openMode);
		void close();

		inline bool read(void* buffer, uintptr bufferSize, uintptr* outReadSize = nullptr) { return File::Read(handle, buffer, bufferSize, outReadSize); }
		inline bool write(const void* buffer, uintptr size) { return File::Write(handle, buffer, size); }
		inline bool flush() { return File::Flush(handle); }

		inline uint64 getSize() { return File::GetSize(handle); }
		inline uint64 getPosition() { return File::GetPosition(handle);}
		inline uint64 setPosition(sint64 offset, FileOffsetOrigin origin = FileOffsetOrigin::Begin) { return File::SetPosition(handle, offset, origin); }

		inline bool isOpen() { return handle != FileHandle::Zero; }
		inline FileHandle getHandle() { return handle; }
	};

	FileHandle GetStdInFileHandle();
	FileHandle GetStdOutFileHandle();
	FileHandle GetStdErrFileHandle();
}
