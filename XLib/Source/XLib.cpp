#include <intrin.h>

#include "XLib.h"

uint8 countLeadingZeros32(uint32 value) { return uint8(__lzcnt(value)); }
uint8 countLeadingZeros64(uint32 value) { return uint8(__lzcnt64(value)); }
uint8 countTrailingZeros32(uint32 value) { return uint8(_tzcnt_u32(value)); }
uint8 countTrailingZeros64(uint64 value) { return uint8(_tzcnt_u64(value)); }
