#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>

namespace XEngine::Core
{
	class Output abstract final
	{
	public:
		static void Initialize();
		static void Destroy();

		static bool IsInFocus();

		static uint32 GetViewCount();
		static uint16x2 GetViewResolution(uint32 viewIndex);
		static void* GetViewWindowHandle(uint32 viewIndex);
	};
}