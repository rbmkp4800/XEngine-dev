#include <intrin.h>

#include "XLib.h"

uint8 clz32(uint32 value)
{
	unsigned long result = 0;
	return _BitScanReverse(&result, value) ? 31 - uint8(result) : 32;
}