#pragma once

#include <XLib.h>

namespace XEngine::Render::HAL
{
	static constexpr uint32 MaxRootBindPointCountLog2 = 4;
	static constexpr uint32 MaxRootBindPointCount = 1 << MaxRootBindPointCountLog2;

	enum class TextureFormat : uint8
	{
		Undefined = 0,
		R8G8B8A8_UNORM,
	};
}
