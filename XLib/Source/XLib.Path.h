#pragma once

#include "XLib.String.h"
#include "XLib.Text.h"

namespace XLib
{
	class Path abstract final
	{
	public:
		template <typename ResultWriter>
		static inline bool Normalize(ResultWriter& resultWriter, const char* path, uintptr lengthLimit = uintptr(-1));

		static inline uintptr NormalizeInplace(char* path, uintptr length);

		static StringViewASCII GetFileName(const char* path, uintptr lengthLimit = uintptr(-1));
		static StringViewASCII GetFileExtension(const char* path, uintptr lengthLimit = uintptr(-1));
		static StringViewASCII GetDirectoryName(const char* path, uintptr lengthLimit = uintptr(-1));

		static inline bool HasFileName(const char* path, uintptr lengthLimit = uintptr(-1)) { return !GetFileName(path, lengthLimit).isEmpty(); }
		static inline bool HasFileExtension(const char* path, uintptr lengthLimit = uintptr(-1)) { return !GetFileExtension(path, lengthLimit).isEmpty(); }

		static bool IsAbsolute(const char* path, uintptr lengthLimit = uintptr(-1));
		static inline bool IsRelative(const char* path, uintptr lengthLimit = uintptr(-1)) { return !Path::IsAbsolute(path, lengthLimit); }

		static inline bool IsDirectorySeparatorChar(char c) { return c == '/' || c == '\\'; }

	private:
		static bool NormalizeInternal(TextWriterVirtualAdapterBase& resultWriter, const char* path, uintptr lengthLimit);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	template <typename ResultWriter>
	inline bool Path::Normalize(ResultWriter& resultWriter, const char* path, uintptr lengthLimit)
	{
		TextWriterVirtualAdapter<ResultWriter> resultWriterVirtualAdapter(resultWriter);
		return NormalizeInternal(resultWriterVirtualAdapter, path, lengthLimit);
	}
}
