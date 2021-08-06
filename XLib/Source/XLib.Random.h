#pragma once

#include "XLib.h"

class Random
{
private:
	static constexpr uint32 multiplier = 1140671485, increment = 12820163, defaultSeed = 0;

	uint32 value;

public:
	inline Random(uint32 seed = defaultSeed) : value(seed) {}

	inline uint16 getU16()
	{
		value = value * multiplier + increment;
		return uint16(value >> 16);
	}
	inline uint32 getU32() { return (uint32(getU16()) << 16) | uint32(getU16()); }
	inline float32 getF32_unorm() { return float32(getU32()) / float32(uint32(-1)); }
	inline float32 getF32(float32 right) { return getF32_unorm() * right; }
	inline float32 getF32(float32 left, float32 right) { return left + getF32_unorm() * (right - left); }
	inline bool getBool() { return getU16() % 2 ? true : false; }

	inline void seed(uint32 seed) { value = seed; }

	static Random Global;
};