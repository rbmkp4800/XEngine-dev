// TODO: Remove this shit :clown_face:
#include <filesystem> // YAYKS
#include <codecvt>

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

bool Path::NormalizeInternal(const char* pathCStr, TextWriterVirtualAdapterBase& resultWriter)
{
	// :))))00)0)0)))
	std::wstring wpn = std::filesystem::absolute(std::filesystem::path(pathCStr)).lexically_normal().native();
	std::string pn;
	for (wchar_t wc : wpn)
	{
		char c = char(wc);
		if (c == '\\')
			c = '/';
		pn.push_back(c); // :))))))))))))))))))
	}

	return resultWriter.append(pn.data(), pn.size());
}

bool Path::NormalizeInternal(StringViewASCII path, TextWriterVirtualAdapterBase& resultWriter)
{
	// :))))00)0)0)))
	std::wstring wpn = std::filesystem::absolute(std::filesystem::path(path.begin(), path.end())).lexically_normal().native();
	std::string pn;
	for (wchar_t wc : wpn)
	{
		char c = char(wc);
		if (c == '\\')
			c = '/';
		pn.push_back(c); // :))))))))))))))))))
	}

	return resultWriter.append(pn.data(), pn.size());
}
