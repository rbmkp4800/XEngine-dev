#pragma once

#include <XLib.h>

namespace XEngine::Render
{
	class Color32 abstract final
	{
	public:
		static constexpr inline uint32 FromRGBA(uint8 r, uint8 g, uint8 b, uint8 a = 0xFF);
		static constexpr inline uint8 GetR(uint32 color32);
		static constexpr inline uint8 GetG(uint32 color32);
		static constexpr inline uint8 GetB(uint32 color32);
		static constexpr inline uint8 GetA(uint32 color32);

		static constexpr uint32 White = 0xFFFFFFFF;
		static constexpr uint32 TransparentBlack = 0;
	};
}
