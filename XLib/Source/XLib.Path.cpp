// NOTE: I should not be allowed to program.

// TODO: Remove this shit :clown_face:
#include <filesystem> // YAYKS
#include <Windows.h>

#include "XLib.Path.h"
#include "XLib.Containers.ArrayList.h"

using namespace XLib;

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
	return Path::GetFileName(StringViewASCII::FromCStr(pathCStr));
}

StringViewASCII Path::GetRoot(StringViewASCII path)
{
	// Windows only implementation.
	// Only two options here: "C:" and "//"
	// I do not give a shit (yet) about "//server/share", "//./", "//?/" and friends.

	if (path.getLength() < 2)
		return StringViewASCII();

	if (Char::IsLetter(path[0]) && path[1] == ':')
		return StringViewASCII(path.getData(), 2);

	if (Path::IsDirectorySeparatorChar(path[0]) &&
		Path::IsDirectorySeparatorChar(path[1]))
		return StringViewASCII(path.getData(), 2);

	return StringViewASCII();
}

StringViewASCII Path::GetRoot(const char* pathCStr)
{
	// See `Path::GetRoot(StringViewASCII path)`.

	if (Char::IsLetter(pathCStr[0]) && pathCStr[1] == ':')
		return StringViewASCII(pathCStr, 2);

	if (Path::IsDirectorySeparatorChar(pathCStr[0]) &&
		Path::IsDirectorySeparatorChar(pathCStr[1]))
		return StringViewASCII(pathCStr, 2);

	return StringViewASCII();
}


StringViewASCII Path::GetParent(StringViewASCII path)
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

StringViewASCII Path::GetParent(const char* pathCStr)
{
	return Path::GetParent(StringViewASCII::FromCStr(pathCStr));
}
