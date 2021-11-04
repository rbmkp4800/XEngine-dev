#pragma once

#include <XLib.h>

namespace XEngine::Render::HAL
{
	static constexpr uint32 MaxPipelineBindPointCountLog2 = 4;
	static constexpr uint32 MaxPipelineBindPointCount = 1 << MaxPipelineBindPointCountLog2;

	enum class BindPointId : uint32;

	enum class TexelFormat : uint8
	{
		Undefined = 0,
		R8G8B8A8_UNORM,
	};
}
