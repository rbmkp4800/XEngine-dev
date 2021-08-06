#pragma once

#include "XLib.h"

template <typename type> inline type nan();
template <> inline float32 nan<float32>() { return as<float32>(0x7fc00000); }
template <> inline float64 nan<float64>() { return as<float64>(0x7ff8'0000'0000'0000ull); }