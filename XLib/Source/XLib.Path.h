#pragma once

#include "XLib.String.h"

namespace XLib
{
	class Path abstract final
	{
	public:
		template <typename ResultWriter> static void Normalize(StringView path, ResultWriter& resultWriter);

		static uintptr NormalizeInplace(char* path, uintptr length);

		static StringView GetFileName(StringView path);
		static StringView GetFileExtension(StringView path);
		static StringView GetDirectoryName(StringView path);

		static inline bool HasFileName(StringView path) { return !GetFileName(path).isEmpty(); }
		static inline bool HasFileExtension(StringView path) { return !GetFileExtension(path).isEmpty(); }

		static bool IsAbsolute(StringView path);
		static inline bool IsRelative(StringView path) { return !Path::IsAbsolute(path); }
	};
}
