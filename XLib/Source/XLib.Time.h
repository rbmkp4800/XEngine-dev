#pragma once

#include "XLib.h"

namespace XLib
{
	// Time since epoch
	using TimePoint = uint64;

	static constexpr TimePoint InvalidTimePoint = TimePoint(-1);
	// Any valid point in time is less than invalid point
}
