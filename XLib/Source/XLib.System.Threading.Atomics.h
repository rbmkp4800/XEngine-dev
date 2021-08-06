#pragma once

#include "XLib.h"

/*
	+-------------------------------+-----------------------------------+
	|        Reordering type        |   ARMv7   x86     AMD64   IA-64   |
	+-------------------------------+-----------------------------------+
	| Loads reordered after loads   |     +                       +     |
	| Loads reordered after stores  |     +                       +     |
	| Stores reordered after stores |     +                       +     |
	| Stores reordered after loads  |     +      +        +       +     |
	| Atomic reordered with loads   |     +                       +     |
	| Atomic reordered with stores  |     +                       +     |
	+-------------------------------+-----------------------------------+

	+--------------------------------+---------------------------------------------+
	|           Fence type           |             SPARC RMO semantics             |
	+--------------------------------+---------------------------------------------+
	| Acquire                        | LoadStore, LoadLoad                         |
	| Release                        | LoadStore, StoreStore                       |
	| Acquire-release                | LoadStore, LoadLoad, StoreStore             |
	| Full (sequentially consistent) | LoadStore, LoadLoad, StoreStore, StoreLoad  |
	+--------------------------------+---------------------------------------------+
*/

namespace XLib
{
	class Atomics abstract
	{
	private:
		template <uint32 size> struct Core;

		template <> struct Core<sizeof(uint16)> abstract
		{
			using Type = uint16;

			static uint16 Add(volatile uint16* target, uint16 value);
			static uint16 Sub(volatile uint16* target, uint16 value);
			static uint16 Exchange(volatile uint16* target, uint16 value);
			static uint16 Increment(volatile uint16* target);
			static uint16 Decrement(volatile uint16* target);
			static bool CompareExchange(volatile uint16* target, uint16 exchange, uint16 comparand);
			static uint16 And(volatile uint16* target, uint16 value);
			static uint16 Or(volatile uint16* target, uint16 value);
			static uint16 Xor(volatile uint16* target, uint16 value);
		};
		template <> struct Core<sizeof(uint32)> abstract
		{
			using Type = uint32;

			static uint32 Add(volatile uint32* target, uint32 value);
			static uint32 Sub(volatile uint32* target, uint32 value);
			static uint32 Exchange(volatile uint32* target, uint32 value);
			static uint32 Increment(volatile uint32* target);
			static uint32 Decrement(volatile uint32* target);
			static bool CompareExchange(volatile uint32* target, uint32 exchange, uint32 comparand);
			static uint32 And(volatile uint32* target, uint32 value);
			static uint32 Or(volatile uint32* target, uint32 value);
			static uint32 Xor(volatile uint32* target, uint32 value);
		};
		template <> struct Core<sizeof(uint64)> abstract
		{
			using Type = uint64;

			static uint64 Add(volatile uint64* target, uint64 value);
			static uint64 Sub(volatile uint64* target, uint64 value);
			static uint64 Exchange(volatile uint64* target, uint64 value);
			static uint64 Increment(volatile uint64* target);
			static uint64 Decrement(volatile uint64* target);
			static bool CompareExchange(volatile uint64* target, uint64 exchange, uint64 comparand);
			static uint64 And(volatile uint64* target, uint64 value);
			static uint64 Or(volatile uint64* target, uint64 value);
			static uint64 Xor(volatile uint64* target, uint64 value);
		};

	public:
		template <typename Type> static inline Type Add(volatile Type& target, Type value) { return Core<sizeof(Type)>::Add((typename Core<sizeof(Type)>::Type*)&target, Core<sizeof(Type)>::Type(value)); }
		template <typename Type> static inline Type Sub(volatile Type& target, Type value) { return Core<sizeof(Type)>::Sub((typename Core<sizeof(Type)>::Type*)&target, Core<sizeof(Type)>::Type(value)); }
		template <typename Type> static inline Type Exchange(volatile Type& target, Type value) { return Core<sizeof(Type)>::Exchange((typename Core<sizeof(Type)>::Type*)&target, Core<sizeof(Type)>::Type(value)); }
		template <typename Type> static inline Type And(volatile Type& target, Type value) { return Core<sizeof(Type)>::And((typename Core<sizeof(Type)>::Type*)&target, Core<sizeof(Type)>::Type(value)); }
		template <typename Type> static inline Type Or(volatile Type& target, Type value) { return Core<sizeof(Type)>::Or((typename Core<sizeof(Type)>::Type*)&target, Core<sizeof(Type)>::Type(value)); }
		template <typename Type> static inline Type Xor(volatile Type& target, Type value) { return Core<sizeof(Type)>::Xor((typename Core<sizeof(Type)>::Type*)&target, Core<sizeof(Type)>::Type(value)); }
		template <typename Type> static inline bool CompareExchange(volatile Type& target, Type exchange, Type comparand) { return Core<sizeof(Type)>::CompareExchange((typename Core<sizeof(Type)>::Type*)&target, Core<sizeof(Type)>::Type(exchange), Core<sizeof(Type)>::Type(comparand)); }
		template <typename Type> static inline Type Increment(volatile Type& target) { return Core<sizeof(Type)>::Increment((typename Core<sizeof(Type)>::Type*)&target); }
		template <typename Type> static inline Type Decrement(volatile Type& target) { return Core<sizeof(Type)>::Decrement((typename Core<sizeof(Type)>::Type*)&target); }
		template <typename Type> static inline Type Load(volatile Type& target) { return target; }
		template <typename Type> static inline void Store(volatile Type& target, Type value) { target = value; }
		template <typename Type> static inline Type LoadAcquire(volatile Type& target)
		{
			FenceAcquire();
			Type value = Type(Core<sizeof(Type)>::Load((typename Core<sizeof(Type)>::Type*)&target));
			return value;
		}
		template <typename Type> static inline void StoreRelease(volatile Type& target, Type value)
		{
			Core<sizeof(Type)>::Store((typename Core<sizeof(Type)>::Type*)&target, Core<sizeof(Type)>::Type(value));
			FenceRelease();
		}

		static void FenceAcquire();
		static void FenceRelease();
		static void FenceAcquireRelease();
		static void FenceFull();
	};

	template <typename Type>
	struct Atomic
	{
		static_assert(sizeof(Type) == 2 || sizeof(Type) == 4 || sizeof(Type) == 8,
			"XLib.System.Threading.Atomic type must be 2/4/8 bytes width");

		__declspec(align(sizeof(Type))) volatile Type value;

		Atomic() = default;
		inline Atomic(Type _value) : value(_value) {}

		inline Type add(Type a) { return Atomics::Add(value, a); }
		inline Type sub(Type a) { return Atomics::Sub(value, a); }
		inline Type exchange(Type a) { return Atomics::Exchange(value, a); }
		inline Type increment() { return Atomics::Increment(value); }
		inline Type decrement() { return Atomics::Decrement(value); }
		inline bool compareExchange(Type exchange, Type comparand) { return Atomics::CompareExchange(value, exchange, comparand); }
		/*inline Type and() { return Atomics::And(value); }
		inline Type or() { return Atomics::Or(value); }
		inline Type xor() { return Atomics::Xor(value); }*/
		inline Type load() { return Atomics::Load(value); }
		inline void store(Type a) { Atomics::Store(value, a); }
		inline Type loadAcquire()
		{
			Type a = Atomics::Load(value);
			Atomics::FenceAcquire();
			return a;
		}
		inline void storeRelease(Type a)
		{
			Atomics::FenceRelease();
			Atomics::Store(value, a);
		}

		inline void busyWait(Type waitValue) { while (value != waitValue) {} }
	};
}