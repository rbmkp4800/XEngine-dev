#include <Windows.h>
#include <intrin.h>

#include "XLib.System.Threading.Atomics.h"

using namespace XLib;

uint32 Atomics::Core<sizeof(uint32)>::Add(volatile uint32* target, uint32 value) { return InterlockedAdd((volatile LONG*)target, value); }
uint32 Atomics::Core<sizeof(uint32)>::Sub(volatile uint32* target, uint32 value) { return InterlockedAdd((volatile LONG*)target, -sint32(value)); }
uint32 Atomics::Core<sizeof(uint32)>::Exchange(volatile uint32* target, uint32 value) { return InterlockedExchange(target, value); }
uint32 Atomics::Core<sizeof(uint32)>::Increment(volatile uint32* target) { return InterlockedIncrement(target); }
uint32 Atomics::Core<sizeof(uint32)>::Decrement(volatile uint32* target) { return InterlockedDecrement(target); }
bool Atomics::Core<sizeof(uint32)>::CompareExchange(volatile uint32* target, uint32 comparand, uint32 exchange) { return _InterlockedCompareExchange(target, exchange, comparand) == comparand; }
uint32 Atomics::Core<sizeof(uint32)>::And(volatile uint32* target, uint32 value) { return InterlockedAnd((volatile LONG*)target, value); }
uint32 Atomics::Core<sizeof(uint32)>::Or(volatile uint32* target, uint32 value) { return InterlockedOr((volatile LONG*)target, value); }
uint32 Atomics::Core<sizeof(uint32)>::Xor(volatile uint32* target, uint32 value) { return InterlockedXor((volatile LONG*)target, value); }

uint16 Atomics::Core<sizeof(uint16)>::Increment(volatile uint16* target) { return InterlockedIncrement16((volatile SHORT*)target); }
uint16 Atomics::Core<sizeof(uint16)>::Decrement(volatile uint16* target) { return InterlockedDecrement16((volatile SHORT*)target); }
bool Atomics::Core<sizeof(uint16)>::CompareExchange(volatile uint16* target, uint16 comparand, uint16 exchange) { return _InterlockedCompareExchange16((volatile SHORT*)target, exchange, comparand) == comparand; }
uint16 Atomics::Core<sizeof(uint16)>::Or(volatile uint16* target, uint16 value) { return InterlockedOr16((volatile SHORT*)target, value); }

void Atomics::FenceAcquire() { _ReadBarrier(); }
void Atomics::FenceRelease() { _WriteBarrier(); }
void Atomics::FenceFull() { MemoryBarrier(); }