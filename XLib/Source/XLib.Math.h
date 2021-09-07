#pragma once

#include "XLib.h"

namespace XLib
{
	class Math abstract final
	{
	public:
		static inline float32 NanF32() { return as<float32>(uint32(0x7fc0'0000)); }
		static inline float64 NanF64() { return as<float64>(uint64(0x7ff8'0000'0000'0000ull)); }

		template <class Type> static constexpr Type Pi = Type(3.1415926535897932385l);
		template <class Type> static constexpr Type HalfPi = Type(3.1415926535897932385l * 0.5l);
		template <class Type> static constexpr Type TwoPi = Type(3.1415926535897932385l * 2.0l);

		static float32 Sqrt(float32 arg);
		static float32 Sin(float32 arg);
		static float32 Cos(float32 arg);
		static float32 Tan(float32 arg);
		static float32 Asin(float32 arg);
		static float32 Acos(float32 arg);
		static float32 Atan(float32 arg);
		static float32 Pow(float32 value, float32 power);

		static constexpr float32 PiF32 = 3.141592654f;
	};
}
