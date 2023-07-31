#include "XLib.Path.h"
#include "XLib.Containers.ArrayList.h"

using namespace XLib;

StringViewASCII Path::GetFileName(const char* pathCStr)
{
	const uintptr pathLength = String::GetCStrLength(pathCStr);

	for (uintptr i = pathLength - 1; i < pathLength; i--)
	{
		if (pathCStr[i] == '\\' || pathCStr[i] == '/')
			return StringViewASCII(pathCStr + i + 1, pathCStr + pathLength);
	}

	return StringViewASCII(pathCStr, pathLength);
}

StringViewASCII Path::RemoveFileName(const char* pathCStr)
{
	const uintptr pathLength = String::GetCStrLength(pathCStr);

	for (uintptr i = pathLength - 1; i < pathLength; i--)
	{
		if (pathCStr[i] == '\\' || pathCStr[i] == '/')
			return StringViewASCII(pathCStr, pathCStr + i + 1);
	}

	return StringViewASCII {};
}

#if 0

bool Path::NormalizeInternal(TextWriterVirtualAdapterBase& resultWriter, const char* path, uintptr lengthLimit)
{
	struct Dir
	{
		uint16 srcPathOffset;
		uint16 length;
	};

	InplaceArrayList<Dir, 256, uint16, false> resultDirs;
	uint16 levelUpCount = 0;

	for (uintptr i = 0; i < lengthLimit; i++)
	{
		const char c = path[i];



		if (!c)
			break;
	}
}

#endif
