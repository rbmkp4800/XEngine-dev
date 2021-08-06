#pragma once

#include "XLib.h"
#include "XLib.Time.h"

namespace XLib
{
	struct FileSystem abstract final
	{
		static bool RemoveFile(const char* path);
		static bool FileExists(const char* path);
		static TimePoint GetFileLastWriteTime(const char* path);
	};
}

// `GetFileLastWriteTime` returns InvalidTimePoint on failure