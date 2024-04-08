#pragma once

#include "XLib.String.h"
#include "XLib.Text.h"

// TODO: Make sence of all this mess
// https://en.cppreference.com/w/cpp/filesystem

namespace XLib
{
	class Path abstract final
	{
	public:
		static StringViewASCII GetFileName(const char* pathCStr);
		static StringViewASCII GetFileExtension(const char* pathCStr);
		static StringViewASCII RemoveFileName(const char* pathCStr);

		template <typename ResultWriter> static inline bool Normalize(const char* pathCStr, ResultWriter& resultWriter);
		template <typename ResultWriter> static inline bool Normalize(StringViewASCII path, ResultWriter& resultWriter);

		//template <typename ResultWriter> static inline void GetAbsolute(const char* pathCStr, ResultWriter& resultWriter);
		//template <typename ResultWriter> static inline void GetAbsolute(StringViewASCII path, ResultWriter& resultWriter);

		//static inline uintptr NormalizeInplace(char* path, uintptr length);

		static inline bool HasFileName(const char* pathCStr) { return !GetFileName(pathCStr).isEmpty(); }
		static inline bool HasFileExtension(const char* pathCStr) { return !GetFileExtension(pathCStr).isEmpty(); }
		static inline bool HasTrailingDirectorySeparator(StringViewASCII path);

		// https://referencesource.microsoft.com/#mscorlib/system/io/path.cs,807960f08fca497d
		// > Tests if the given path contains a root. A path is considered rooted
		// > if it starts with a backslash ("\") or a drive letter and a colon (":").
		//static bool IsRooted(const char* pathCStr);
		//static bool IsRooted(StringViewASCII path);

		//static bool IsAbsolute(const char* pathCStr);
		//static bool IsAbsolute(StringViewASCII path);

		//static inline bool IsRelative(const char* pathCStr) { return !Path::IsAbsolute(pathCStr); }
		//static inline bool IsRelative(StringViewASCII path) { return !Path::IsAbsolute(path); }

		static inline bool IsDirectorySeparatorChar(char c) { return c == '/' || c == '\\'; }

	private:
		static bool NormalizeInternal(const char* pathCStr, TextWriterVirtualAdapterBase& resultWriter);
		static bool NormalizeInternal(StringViewASCII path, TextWriterVirtualAdapterBase& resultWriter);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	template <typename ResultWriter>
	inline bool Path::Normalize(const char* pathCStr, ResultWriter& resultWriter)
	{
		TextWriterVirtualAdapter<ResultWriter> resultWriterVirtualAdapter(resultWriter);
		return NormalizeInternal(resultWriterVirtualAdapter, pathCStr);
	}

	template <typename ResultWriter>
	inline bool Path::Normalize(StringViewASCII path, ResultWriter& resultWriter)
	{
		TextWriterVirtualAdapter<ResultWriter> resultWriterVirtualAdapter(resultWriter);
		return NormalizeInternal(path, resultWriterVirtualAdapter);
	}

	inline bool Path::HasTrailingDirectorySeparator(StringViewASCII path)
	{
		if (path.isEmpty())
			return false;
		return path.endsWith('/') || path.endsWith('\\');
	}
}
