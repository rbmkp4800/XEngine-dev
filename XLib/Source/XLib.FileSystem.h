#pragma once

#include "XLib.h"
#include "XLib.String.h"
#include "XLib.Time.h"

namespace XLib
{
	enum class FileSystemOpStatus : uint8
	{
		Success = 0,
		FileNotFound,
		PathNotFound,
		AccessDenied,

		FileWithThisNameAlreadyExists,

		// I'm lazy
		Failure,
	};

	enum class FileSystemEntryType : uint8
	{
		None = 0,
		File,
		Directory,
	};

	template <typename ResultType>
	struct FileSystemOpResult
	{
		ResultType value;
		FileSystemOpStatus status;
	};

	class FileSystem abstract final
	{
	public:
		// What this should return for "file.txt/"?
		// What this should return for "file.txt/blabla"?

		static FileSystemOpResult<FileSystemEntryType> GetEntryType(const char* pathCStr);

		//static FileSystemOpResult<bool> FileExists(const char* pathCStr);
		//static FileSystemOpResult<bool> DirExists(const char* pathCStr);

		//static FileSystemOpResult<bool> IsFile(const char* pathCStr);
		//static FileSystemOpResult<bool> IsDir(const char* pathCStr);

		static FileSystemOpStatus RemoveFile(const char* pathCStr);
		static FileSystemOpStatus RemoveDir(const char* pathCStr);

		static FileSystemOpStatus CreateDirSingle(const char* pathCStr);
		static FileSystemOpStatus CreateDirRecursive(const char* pathCStr);
		static FileSystemOpStatus CreateDirRecursive(StringViewASCII path);

		static TimePoint GetFileModificationTime(const char* pathCStr);
	};
}

// `GetFileLastWriteTime` returns InvalidTimePoint on failure.
