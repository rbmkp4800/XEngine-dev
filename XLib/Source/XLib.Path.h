#pragma once

#include "XLib.String.h"
#include "XLib.Text.h"

namespace XLib
{
	class Path abstract final
	{
	public:
		template <typename ResultWriter>
		static inline bool Normalize(ResultWriter& resultWriter, const char* pathCStr);

		static inline uintptr NormalizeInplace(char* path, uintptr length);

		static StringViewASCII GetFileName(const char* pathCStr);
		static StringViewASCII GetFileExtension(const char* pathCStr);
		static StringViewASCII RemoveFileName(const char* pathCStr);

		static inline bool HasFileName(const char* pathCStr) { return !GetFileName(pathCStr).isEmpty(); }
		static inline bool HasFileExtension(const char* pathCStr) { return !GetFileExtension(pathCStr).isEmpty(); }

		static bool IsAbsolute(const char* pathCStr);
		static inline bool IsRelative(const char* pathCStr) { return !Path::IsAbsolute(pathCStr); }

		static inline bool IsDirectorySeparatorChar(char c) { return c == '/' || c == '\\'; }

	private:
		static bool NormalizeInternal(TextWriterVirtualAdapterBase& resultWriter, const char* pathCStr);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	template <typename ResultWriter>
	inline bool Path::Normalize(ResultWriter& resultWriter, const char* pathCStr)
	{
		TextWriterVirtualAdapter<ResultWriter> resultWriterVirtualAdapter(resultWriter);
		return NormalizeInternal(resultWriterVirtualAdapter, pathCStr);
	}
}
