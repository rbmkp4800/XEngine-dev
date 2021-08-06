#pragma once

#include "XLib.h"
#include "XLib.Vectors.h"

namespace XLib
{
	constexpr inline uint32 operator "" _rgb(uint64 val) { return ((val & 0xFF) << 16) | (val & 0xFF00) | ((val & 0xFF0000) >> 16) | 0xFF000000; }
	constexpr inline uint32 operator "" _rgba(uint64 val) { return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val & 0xFF000000) >> 24); }
	constexpr inline uint32 rgba(uint32 rgb, uint8 a) { return (rgb & 0xFFFFFF) | (uint32(a) << 24); }

	struct Color
	{
		union
		{
			struct { uint8 r, g, b, a; };
			uint32 rgba;
		};

		constexpr inline Color() : rgba(0) {}
		constexpr inline Color(uint32 _rgba) : rgba(_rgba) {}
		constexpr inline Color(uint8 _r, uint8 _g, uint8 _b, uint8 _a) : r(_r), g(_g), b(_b), a(_a) {}

		inline float32x4 toF32x4() {
			return float32x4(
				float32(r) / 255.0f, float32(g) / 255.0f,
				float32(b) / 255.0f, float32(a) / 255.0f);
		}

		inline operator uint32() const { return rgba; }
	};
}