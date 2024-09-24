#include <Windows.h>

#include "XLib.System.Environment.h"
#include "XLib.Path.h"

using namespace XLib;

void Environment::GetCurrentPath(VirtualStringRefASCII resultPath)
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

const char* Environment::GetCommandLineCStr()
{
	return ::GetCommandLineA();
}
