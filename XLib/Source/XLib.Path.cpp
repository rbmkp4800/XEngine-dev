// NOTE: I should not be allowed to program.

// TODO: Remove this shit :clown_face:
#include <filesystem> // YAYKS
#include <Windows.h>

#include "XLib.Path.h"
#include "XLib.Containers.ArrayList.h"

using namespace XLib;

void Path::GetCurrentPath(VirtualStringRefASCII resultPath)
{
	// Including null-terminator.
	const DWORD resultPathRequiredBufferSize = GetCurrentDirectoryA(0, nullptr);
	const uint32 resultPathSrcLength = resultPathRequiredBufferSize - 1;

	const uint32 resultPathMaxLength = resultPath.getMaxBufferSize();
	if (resultPathMaxLength < resultPathSrcLength)
	{
		// Explicitly clip output.

		char internalBuffer[4096];
		XAssert(resultPathRequiredBufferSize <= countOf(internalBuffer));
		const DWORD lengthCheck = GetCurrentDirectoryA(countOf(internalBuffer), internalBuffer);
		XAssert(lengthCheck == resultPathSrcLength);

		resultPath.growBufferToFitLength(resultPathMaxLength);
		resultPath.setLength(resultPathMaxLength);
		memoryCopy(resultPath.getBuffer(), internalBuffer, resultPathMaxLength);
	}
	else
	{
		resultPath.growBufferToFitLength(resultPathSrcLength + 1); // +1 just in case we need to add trailing directory separator to avoid second reallocation.
		resultPath.setLength(resultPathSrcLength);

		const DWORD lengthCheck = GetCurrentDirectoryA(resultPathRequiredBufferSize, resultPath.getBuffer());
		XAssert(lengthCheck == resultPathSrcLength);
	}

	// Normalize all slashes.
	char* resultData = resultPath.getBuffer();
	const uintptr resultLength = resultPath.getLength();
	for (uintptr i = 0; i < resultLength; i++)
	{
		if (resultData[i] == '\\')
			resultData[i] = '/';
	}

	Path::AddTrailingDirectorySeparator(resultPath);
}

void Path::Normalize(StringViewASCII path, VirtualStringRefASCII resultPath)
{
	std::filesystem::path p(std::string_view(path.getData(), path.getLength()));
	p = p.lexically_normal();
	auto np = p.native();

	// TODO: Revisit
	const uint32 resultPathLength = min<uint32>(XCheckedCastU32(np.length()), resultPath.getMaxLength());
	resultPath.growBufferToFitLength(resultPathLength);
	resultPath.setLength(resultPathLength);
	char* resultPathData = resultPath.getBuffer();
	for (uint32 i = 0; i < resultPathLength; i++)
	{
		char c = char(np[i]);
		if (c == '\\')
			c = '/';
		resultPathData[i] = c;
	}
}

void Path::Normalize(VirtualStringRefASCII path)
{
	std::filesystem::path p(std::string_view(path.getBuffer(), path.getLength()));
	p = p.lexically_normal();
	auto np = p.native();

	// TODO: Revisit
	const uint32 pathLength = min<uint32>(XCheckedCastU32(np.length()), path.getMaxLength());
	path.growBufferToFitLength(pathLength);
	path.setLength(pathLength);
	char* pathData = path.getBuffer();
	for (uint32 i = 0; i < pathLength; i++)
	{
		char c = char(np[i]);
		if (c == '\\')
			c = '/';
		pathData[i] = c;
	}
}

void Path::MakeAbsolute(StringViewASCII path, VirtualStringRefASCII resultPath)
{
	std::filesystem::path p(std::string_view(path.getData(), path.getLength()));
	p = std::filesystem::absolute(p);
	auto np = p.native();

	// TODO: Revisit
	const uint32 resultPathLength = min<uint32>(XCheckedCastU32(np.length()), resultPath.getMaxLength());
	resultPath.growBufferToFitLength(resultPathLength);
	resultPath.setLength(resultPathLength);
	char* resultPathData = resultPath.getBuffer();
	for (uint32 i = 0; i < resultPathLength; i++)
	{
		char c = char(np[i]);
		if (c == '\\')
			c = '/';
		resultPathData[i] = c;
	}
}

void Path::MakeAbsolute(const char* pathCStr, VirtualStringRefASCII resultPath)
{
	MakeAbsolute(StringViewASCII::FromCStr(pathCStr), resultPath);
}

void Path::AddTrailingDirectorySeparator(VirtualStringRefASCII path)
{
	char* pathData = path.getBuffer();
	uint32 pathLength = path.getLength();

	if (pathLength == 0)
		return;
	if (Path::HasTrailingDirectorySeparator(StringViewASCII(pathData, pathLength)))
		return;

	const uint32 newLength = pathLength + 1;
	if (newLength > path.getMaxLength())
		return;

	path.growBufferToFitLength(newLength);
	path.setLength(newLength);
	pathData = path.getBuffer();
	pathData[pathLength] = '/';
}


////////////////////////////////////////////////////////////////////////////////////////////////////

StringViewASCII Path::GetFileName(StringViewASCII path)
{
	const char* pathBegin = path.getData();
	const char* pathEnd = path.getData() + path.getLength();
	for (const char* pathIt = pathEnd - 1; pathBegin <= pathIt; pathIt--)
	{
		if (Path::IsDirectorySeparatorChar(*pathIt))
			return StringViewASCII(pathIt + 1, pathEnd);
	}
	return path;
}

StringViewASCII Path::GetFileName(const char* pathCStr)
{
	return GetFileName(StringViewASCII::FromCStr(pathCStr));
}

StringViewASCII Path::RemoveFileName(StringViewASCII path)
{
	const char* pathBegin = path.getData();
	const char* pathEnd = path.getData() + path.getLength();
	for (const char* pathIt = pathEnd - 1; pathBegin <= pathIt; pathIt--)
	{
		if (Path::IsDirectorySeparatorChar(*pathIt))
			return StringViewASCII(pathBegin, pathIt + 1);
	}
	return StringViewASCII();
}

StringViewASCII Path::RemoveFileName(const char* pathCStr)
{
	return RemoveFileName(StringViewASCII::FromCStr(pathCStr));
}
