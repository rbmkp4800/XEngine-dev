#pragma once

#include "XLib.h"
#include "XLib.String.h"

namespace XLib
{
	class Environment abstract final
	{
	public:
		static void GetCurrentPath(VirtualStringRefASCII resultPath);
		static const char* GetCommandLineCStr();
	};
}
