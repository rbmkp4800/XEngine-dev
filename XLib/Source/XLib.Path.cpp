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

void Path::GetCurrentPath(VirtualStringRefASCII result)
{
	const DWORD requiredBufferSize = GetCurrentDirectoryA(0, nullptr);
	XAssert(requiredBufferSize > 0);
	const uintptr expectedLength = requiredBufferSize - 1;

	if (expectedLength > result.getMaxLength())
	{
		// Explicitly clip output.

		constexpr uintptr internalBufferSize = 4096;
		XAssert(requiredBufferSize <= internalBufferSize);

		char internalBuffer[internalBufferSize];
		const DWORD length = GetCurrentDirectoryA(internalBufferSize, internalBuffer);
		XAssert(length == expectedLength);

		result.resize(result.getMaxLength());
		char* buffer = result.getData();
		memoryCopy(buffer, internalBuffer, result.getMaxLength());
	}
	else
	{
		result.resize(expectedLength);
		char* buffer = result.getData();
		const DWORD length = GetCurrentDirectoryA(requiredBufferSize, buffer);
		XAssert(length == expectedLength);
	}

	char* resultData = result.getData();
	const uintptr resultLength = result.getLength();

	for (uintptr i = 0; i < resultLength; i++)
	{
		if (resultData[i] == '\\')
			resultData[i] = '/';
	}

	if (resultData[resultLength - 1] != '/' && resultLength != result.getMaxLength())
	{
		result.resize(resultLength + 1);
		result.getData()[resultLength] = '/';
	}
}

void Path::Normalize(StringViewASCII path, VirtualStringRefASCII result)
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

void Path::Normalize(VirtualStringRefASCII result)
{
	std::filesystem::path p(std::string_view(result.getData(), result.getLength()));
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

void Path::MakeAbsolute(StringViewASCII path, VirtualStringRefASCII result)
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

void Path::MakeAbsolute(const char* pathCStr, VirtualStringRefASCII result)
{
	MakeAbsolute(StringViewASCII(pathCStr), result);
}
