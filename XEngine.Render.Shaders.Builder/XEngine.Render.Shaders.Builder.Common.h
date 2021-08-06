#pragma once

#include <XLib.h>

namespace XEngine::Render::Shaders::Builder
{
	enum class ShaderType : uint8
	{
		None = 0,
		CS,
		VS,
		MS,
		AS,
		PS,
	};
}