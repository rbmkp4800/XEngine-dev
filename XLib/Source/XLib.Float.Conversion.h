#pragma once

#include "XLib.h"

constexpr inline sint16 F32toS16_snorm(float32 value) { return sint16(value * float32((1 << 15) - 1)); }
constexpr inline uint16 F32toU16_unorm(float32 value) { return uint16(value * float32((1 << 16) - 1)); }
constexpr inline uint8 F32toU8_unorm(float32 value) { return uint8(value * float32((1 << 8) - 1)); }