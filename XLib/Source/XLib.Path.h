#pragma once

#include "XLib.h"
#include "XLib.String.h"

namespace XLib
{
	class Path abstract final
	{
	public:
		static void Normalize(StringViewASCII path, VirtualStringRefASCII resultPath);
		static void Normalize(const char* pathCStr, VirtualStringRefASCII resultPath);
		static void Normalize(VirtualStringRefASCII path);

		static void MakeAbsolute(StringViewASCII path, VirtualStringRefASCII resultPath);
		static void MakeAbsolute(const char* pathCStr, VirtualStringRefASCII resultPath);
		static void MakeAbsolute(VirtualStringRefASCII path);

		static inline bool HasTrailingDirectorySeparator(StringViewASCII path);
		static inline bool HasTrailingDirectorySeparator(const char* pathCStr);

		static void AddTrailingDirectorySeparator(VirtualStringRefASCII path);
		static StringViewASCII TrimEndingDirectorySeparator(StringViewASCII path);

		static StringViewASCII GetFileName(StringViewASCII path);
		static StringViewASCII GetFileName(const char* pathCStr);
		static StringViewASCII GetFileExtension(StringViewASCII path);
		static StringViewASCII GetFileExtension(const char* pathCStr);
		static StringViewASCII GetRoot(StringViewASCII path);
		static StringViewASCII GetRoot(const char* pathCStr);
		static StringViewASCII GetParent(StringViewASCII path);
		static StringViewASCII GetParent(const char* pathCStr);

		static inline bool HasRoot(StringViewASCII path) { return !Path::GetRoot(path).isEmpty(); }
		static inline bool HasRoot(const char* pathCStr) { return !Path::GetRoot(pathCStr).isEmpty(); }
		static inline bool HasFileName(StringViewASCII path) { return !Path::GetFileName(path).isEmpty(); }
		static inline bool HasFileName(const char* pathCStr) { return !Path::GetFileName(pathCStr).isEmpty(); }
		static inline bool HasFileExtension(StringViewASCII path) { return !Path::GetFileExtension(path).isEmpty(); }
		static inline bool HasFileExtension(const char* pathCStr) { return !Path::GetFileExtension(pathCStr).isEmpty(); }

		static inline bool IsRelative(const char* pathCStr) { return Path::GetRoot(pathCStr).isEmpty(); }
		static inline bool IsRelative(StringViewASCII path) { return Path::GetRoot(path).isEmpty(); }
		static inline bool IsAbsolute(const char* pathCStr) { return !Path::IsRelative(pathCStr); }
		static inline bool IsAbsolute(StringViewASCII path) { return !Path::IsRelative(path); }

		static inline bool IsDirectorySeparatorChar(char c) { return c == '/' || c == '\\'; }
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

inline bool XLib::Path::HasTrailingDirectorySeparator(StringViewASCII path)
{
	if (path.isEmpty())
		return false;
	return path.endsWith('/') || path.endsWith('\\');
}


/* NOTES *******************************************************************************************

MakeAbsolute() returns normalized path.

Any rooted path is considered absolute. This means that "/foo" on Windows is considered not rooted.
Path "/foo" is considered relative on Windows and absolute on Unix.
Path "X:foo" is considered relative <-> not rooted (I do not give a fuck that it contains root name).
We do not handle (yet?) all the funny cases like "\\server\share\foo", "\\?\X:\foo", "\\?\UNC\server\share\foo" etc.

GetRoot("/XXX") returns "/" (Unix); "" (Windows)
GetRoot("//XXX") returns "/" (Unix); "//" (Windows)
GetRoot("///XXX" returns "/" (Unix); "" (Windows) as only '\\' is considered UNC
GetRoot("X:") returns "X:" (Windows)
GetRoot("X:/XXX") returns "X:/" (Windows)
GetRoot("X:foo/XXX") returns ""

***************************************************************************************************/
