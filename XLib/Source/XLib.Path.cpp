#include "XLib.Path.h"
#include "XLib.Containers.ArrayList.h"

using namespace XLib;

#if 0

bool Path::NormalizeInternal(TextWriterVirtualAdapterBase& resultWriter, const char* path, uintptr lengthLimit)
{
	struct Dir
	{
		uint16 srcPathOffset;
		uint16 length;
	};

	InplaceArrayList<Dir, 256, uint16, false> resultDirs;
	uint16 levelUpCount = 0;

	for (uintptr i = 0; i < lengthLimit; i++)
	{
		const char c = path[i];



		if (!c)
			break;
	}
}

#endif
