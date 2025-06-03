#include <filesystem>

#include <Windows.h>

#include "XLib.FileSystem.h"

#include "XLib.Path.h"

using namespace XLib;

FileSystemOpStatus FileSystem::RemoveFile(const char* pathCStr)
{
	if (DeleteFileA(pathCStr))
		return FileSystemOpStatus::Success;

	// TODO: Do `GetLastError()` and stuff.
	return FileSystemOpStatus::Failure;
}

#if 0
FileSystemOpResult<bool> FileSystem::FileExists(const char* pathCStr)
{
	const DWORD winAttributes = GetFileAttributesA(pathCStr);

	if (winAttributes != INVALID_FILE_ATTRIBUTES)
	{
		const DWORD winError = GetLastError();

		if (winError == ERROR_FILE_NOT_FOUND ||
			winError == ERROR_PATH_NOT_FOUND ||
			winError == ERROR_INVALID_NAME)
		{
			return FileSystemOpResult<bool> { false, FileSystemOpStatus::Success };
		}

		// TODO: Fill status properly.
		return FileSystemOpResult<bool> { false, FileSystemOpStatus::Failure };
	}

	if (winAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return FileSystemOpResult<bool> { false, FileSystemOpStatus::Success };

	return FileSystemOpResult<bool> { true, FileSystemOpStatus::Success };
}
#endif

/*FileSystemOpStatus FileSystem::CreateDirSingle(const char* pathCStr)
{
	ERROR_ALREADY_EXISTS

	CreateDirectoryA(path)
}*/

FileSystemOpStatus FileSystem::CreateDirRecursive(StringViewASCII path)
{
	std::filesystem::create_directories(std::string_view(path.getData(), path.getLength()));
	return FileSystemOpStatus::Success;
}

FileSystemOpStatus FileSystem::CreateDirRecursive(const char* pathCStr)
{
	std::filesystem::create_directories(pathCStr);
	return FileSystemOpStatus::Success;

#if 0
	InplaceStringASCIIx2048 path;
	Path::MakeAbsolute(pathCStr, path.getVirtualRef());
	// We expect path to be normalized at this point.

	// Check if directory already exists.
	{
		FileSystemOpResult<FileSystemEntryType> et = FileSystem::GetEntryType(path.getCStr());
		if (et.status != FileSystemOpStatus::Success)
			return et.status;

		if (et.value == FileSystemEntryType::Directory)
			return FileSystemOpStatus::Success;
		if (et.value == FileSystemEntryType::File)
			return FileSystemOpStatus::FileWithThisNameAlreadyExists;

		XAssert(et.value == FileSystemEntryType::None);
	}


	StringViewASCII currentPath = path;

	for (;;)
	{
		currentPath = Path::GetParent(currentPath);
		if (currentPath.isEmpty())
			break;
	}

#endif
}

FileSystemOpResult<TimePoint> FileSystem::GetFileModificationTime(const char* pathCStr)
{
	WIN32_FILE_ATTRIBUTE_DATA winFileAttributeData = {};
	if (!GetFileAttributesExA(pathCStr, GetFileExInfoStandard, &winFileAttributeData))
		return FileSystemOpResult<TimePoint> { .value = InvalidTimePoint, .status = FileSystemOpStatus::Failure };

	const uint64 modificationTime = (uint64(winFileAttributeData.ftLastWriteTime.dwHighDateTime) << 32) | uint64(winFileAttributeData.ftLastWriteTime.dwLowDateTime);
	return FileSystemOpResult<TimePoint> { .value = TimePoint(modificationTime), .status = FileSystemOpStatus::Success };
}
