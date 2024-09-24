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

		static void AddTrailingDirectorySeparator(VirtualStringRefASCII path);

		static StringViewASCII GetFileName(StringViewASCII path);
		static StringViewASCII GetFileName(const char* pathCStr);
		static StringViewASCII GetFileExtension(StringViewASCII path);
		static StringViewASCII GetFileExtension(const char* pathCStr);
		static StringViewASCII GetRoot(StringViewASCII path);
		static StringViewASCII GetRoot(const char* pathCStr);
		static StringViewASCII RemoveFileName(StringViewASCII path);
		static StringViewASCII RemoveFileName(const char* pathCStr);

		static inline bool HasRoot(StringViewASCII path) { return !Path::GetRoot(path).isEmpty(); }
		static inline bool HasRoot(const char* pathCStr) { return !Path::GetRoot(pathCStr).isEmpty(); }
		static inline bool HasFileName(StringViewASCII path) { return !Path::GetFileName(path).isEmpty(); }
		static inline bool HasFileName(const char* pathCStr) { return !Path::GetFileName(pathCStr).isEmpty(); }
		static inline bool HasFileExtension(StringViewASCII path) { return !Path::GetFileExtension(path).isEmpty(); }
		static inline bool HasFileExtension(const char* pathCStr) { return !Path::GetFileExtension(pathCStr).isEmpty(); }
		static inline bool HasTrailingDirectorySeparator(StringViewASCII path);
		static inline bool HasTrailingDirectorySeparator(const char* pathCStr);

		static inline bool IsAbsolute(const char* pathCStr) { return Path::HasRoot(pathCStr); }
		static inline bool IsAbsolute(StringViewASCII path) { return Path::HasRoot(path); }
		static inline bool IsRelative(const char* pathCStr) { return !Path::IsAbsolute(pathCStr); }
		static inline bool IsRelative(StringViewASCII path) { return !Path::IsAbsolute(path); }

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
