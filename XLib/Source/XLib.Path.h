#pragma once

#include "XLib.h"
#include "XLib.String.h"

namespace XLib
{
	class Path abstract final
	{
	public:
		static void GetCurrentPath(VirtualStringRefASCII result);

		static void Normalize(StringViewASCII path, VirtualStringRefASCII result);
		static void Normalize(const char* pathCStr, VirtualStringRefASCII result);
		static void Normalize(VirtualStringRefASCII result);

		static void MakeAbsolute(StringViewASCII path, VirtualStringRefASCII result);
		static void MakeAbsolute(const char* pathCStr, VirtualStringRefASCII result);

		static StringViewASCII GetFileName(const char* pathCStr);
		static StringViewASCII GetFileExtension(const char* pathCStr);
		static StringViewASCII GetRoot(const char* pathCStr);
		static StringViewASCII RemoveFileName(const char* pathCStr);

		static inline bool HasFileName(const char* pathCStr) { return !GetFileName(pathCStr).isEmpty(); }
		static inline bool HasFileExtension(const char* pathCStr) { return !GetFileExtension(pathCStr).isEmpty(); }
		static inline bool HasTrailingDirectorySeparator(StringViewASCII path);

		static bool IsAbsolute(const char* pathCStr);
		static bool IsAbsolute(StringViewASCII path);

		static inline bool IsRelative(const char* pathCStr) { return !Path::IsAbsolute(pathCStr); }
		static inline bool IsRelative(StringViewASCII path) { return !Path::IsAbsolute(path); }

		static inline bool IsDirectorySeparatorChar(char c) { return c == '/' || c == '\\'; }
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	inline bool Path::HasTrailingDirectorySeparator(StringViewASCII path)
	{
		if (path.isEmpty())
			return false;
		return path.endsWith('/') || path.endsWith('\\');
	}
}
