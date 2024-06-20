// NOTE: I should not be allowed to program.

// TODO: Remove this shit :clown_face:
#include <filesystem> // YAYKS
#include <Windows.h>

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

void Path::GetCurrentPath(VirtualStringRefASCII resultPath)
{
	// Including null-terminator.
	const DWORD requiredBufferCapacity = GetCurrentDirectoryA(0, nullptr);
	XAssert(requiredBufferCapacity > 0);
	//const uintptr expectedLength = requiredBufferSize - 1;

	const uint32 resultMaxBufferCapacity = resultPath.getMaxBufferCapacity();
	if (requiredBufferCapacity > resultMaxBufferCapacity)
	{
		// Explicitly clip output.

		char internalBuffer[4096];
		XAssert(requiredBufferCapacity <= countof(internalBuffer));
		const DWORD length = GetCurrentDirectoryA(countof(internalBuffer), internalBuffer);
		XAssert(length + 1 == requiredBufferCapacity);

		resultPath.reserveBufferCapacity(resultMaxBufferCapacity);
		char* buffer = resultPath.getBuffer();

		const uint32 clippedLength = resultMaxBufferCapacity - 1;
		memoryCopy(buffer, internalBuffer, clippedLength);
		resultPath.setLength(clippedLength);
	}
	else
	{
		resultPath.reserveBufferCapacity(requiredBufferCapacity);
		char* buffer = resultPath.getBuffer();

		// Not including null-terminator.
		const DWORD length = GetCurrentDirectoryA(requiredBufferCapacity, buffer);
		XAssert(length + 1 == requiredBufferCapacity);

		resultPath.setLength(length);
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

	const uintptr resultLength = min(np.length(), result.getMaxLength());
	result.resize(resultLength);
	char* resultData = result.getData();
	for (uintptr i = 0; i < resultLength; i++)
	{
		char c = char(np[i]);
		if (c == '\\')
			c = '/';
		resultData[i] = c;
	}
}

void Path::Normalize(VirtualStringRefASCII path)
{
	std::filesystem::path p(std::string_view(path.getBuffer(), path.getLength()));
	p = p.lexically_normal();
	auto np = p.native();
	XAssert(np.length() <= result.getLength());
	result.resize(np.length());
	char* resultData = result.getData();
	for (uintptr i = 0; i < np.length(); i++)
	{
		char c = char(np[i]);
		if (c == '\\')
			c = '/';
		resultData[i] = c;
	}
}

void Path::MakeAbsolute(StringViewASCII path, VirtualStringRefASCII resultPath)
{
	std::filesystem::path p(std::string_view(path.getData(), path.getLength()));
	p = std::filesystem::absolute(p);
	auto np = p.native();

	const uintptr resultLength = min(np.length(), result.getMaxLength());
	result.resize(resultLength);
	char* resultData = result.getData();
	for (uintptr i = 0; i < resultLength; i++)
	{
		char c = char(np[i]);
		if (c == '\\')
			c = '/';
		resultData[i] = c;
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

	if (Path::HasTrailingDirectorySeparator(StringViewASCII(pathData, pathLength)))
		return;

	const uint32 newLength = pathLength + 1;
	const uint32 newBufferCapacity = newLength + 1;
	if (newBufferCapacity > path.getMaxBufferCapacity())
		return;

	path.reserveBufferCapacity(newBufferCapacity);
	path.setLength(newLength);
	pathData = path.getBuffer();
	pathData[pathLength] = '/';
}

