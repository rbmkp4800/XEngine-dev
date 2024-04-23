#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"

// TODO: Rename `File::isInitialized` to `File::isOpen`.
// TODO: Properly replace `uint32` with `uintptr` everywhere. For now it is hacked just to remove compilation warnings.

namespace XLib
{
	enum class FileAccessMode : uint32
	{
		Read = 0x8000'0000,
		Write = 0x4000'0000,
		ReadWrite = 0xC000'0000,
	};

	enum class FileOpenMode : uint32
	{
		Override = 2,
		OpenExisting = 3,
	};

	enum class FilePosition : uint32
	{
		Begin = 0,
		Current = 1,
		End = 2,
	};

	enum class FileHandle : uintptr { Zero = 0 };

	class File : public NonCopyable
	{
	private:
		FileHandle handle = FileHandle::Zero;

	public:
		File() = default;
		inline ~File() { close(); }

		inline File(File&& that)
		{
			handle = that.handle;
			that.handle = FileHandle::Zero;
		}
		inline void operator = (File&& that)
		{
			close();
			handle = that.handle;
			that.handle = FileHandle::Zero;
		}

		bool open(const char* name, FileAccessMode accessMode, FileOpenMode openMode);
		void close();

		bool read(void* buffer, uintptr size);
		bool read(void* buffer, uintptr bufferSize, uintptr& readSize);
		bool write(const void* buffer, uintptr size);
		void flush();

		template <typename Type>
		inline bool read(Type& value)
		{
			uintptr readSize = 0;
			bool result = read(&value, sizeof(Type), readSize);
			return readSize == sizeof(Type) && result;
		}
		template <typename Type>
		inline bool write(const Type& value)
		{
			return write(&value, sizeof(Type));
		}

		uint64 getSize();
		uint64 getPosition();
		uint64 setPosition(sint64 offset, FilePosition origin = FilePosition::Begin);

		inline bool isOpen() { return handle != FileHandle::Zero; }
		inline FileHandle getHandle() { return FileHandle(handle); }

	public:
		FileHandle Open(const char* name, FileAccessMode accessMode, FileOpenMode openMode);
		bool Read(FileHandle fileHandle, );
		bool Write(FileHandle fileHandle, const void* data, uintptr size);

	public:
		static FileHandle GetStdInHandle();
		static FileHandle GetStdOutHandle();
		static FileHandle GetStdErrHandle();
	};
}
