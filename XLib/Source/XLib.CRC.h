#pragma once

#include "XLib.h"

// TODO: implement standart algorithms

//	| Algorithm       | Polynomial         | XorIn              | XorOut             | RefIn | RefOut | Check(123456789)   |
//	+-----------------+--------------------+--------------------+--------------------+-------+--------+--------------------+
//	| CRC-32/zlib     | 0x04C11DB7         | 0xFFFFFFFF         | 0xFFFFFFFF         |   +   |   +    | 0xCBF43926         |
//	| CRC-32/POSIX    | 0x04C11DB7         | 0x00000000         | 0xFFFFFFFF         |   -   |   -    | 0x765E7680         |
//	| CRC-64/ECMA182  | 0x42F0E1EBA9EA3693 | 0x0000000000000000 | 0x0000000000000000 |   -   |   -    | 0x6C40DF5F0B497347 |
//	| CRC-64/XZ       | 0x42F0E1EBA9EA3693 | 0xFFFFFFFFFFFFFFFF | 0xFFFFFFFFFFFFFFFF |   +   |   +    | 0x995DC9BBDF1939FA |
//	| CRC-64/ISO      | 0x000000000000001B | 0xFFFFFFFFFFFFFFFF | 0xFFFFFFFFFFFFFFFF |   +   |   +    | 0xB90956C775A41001 |

namespace XLib
{
	class CRC8
	{
	private:
		uint8 value = 0;

	public:
		static uint8 Compute(const void* data, uintptr size, uint8 seed = 0);

		inline void process(const void* data, uintptr size) { value = Compute(data, size, value); }
		inline uint8 getValue() { return value; }
		inline void reset() { value = 0; }

		template <typename Type>
		inline void process(const Type& data) { process(&data, sizeof(data)); }
		template <typename Type>
		static inline uint8 Compute(const Type& data) { return Compute(&data, sizeof(data)); }
	};

	class CRC16
	{
	private:
		uint16 value = 0;

	public:
		static uint16 Compute(const void* data, uintptr size, uint16 seed = 0);

		inline void process(const void* data, uintptr size) { value = Compute(data, size, value); }
		inline uint16 getValue() { return value; }
		inline void reset() { value = 0; }

		template <typename Type>
		inline void process(const Type& data) { process(&data, sizeof(data)); }
		template <typename Type>
		static inline uint16 Compute(const Type& data) { return Compute(&data, sizeof(data)); }
	};

	class CRC32 // CRC-32/zlib
	{
	private:
		uint32 value = 0;

	public:
		static uint32 Compute(const void* data, uintptr size, uint32 seed = 0);

		inline void process(const void* data, uintptr size) { value = Compute(data, size, value); }
		inline uint32 getValue() { return value; }
		inline void reset() { value = 0; }

		template <typename Type>
		inline void process(const Type& data) { process(&data, sizeof(data)); }
		template <typename Type>
		static inline uint32 Compute(const Type& data) { return Compute(&data, sizeof(data)); }
	};

	class CRC64 // CRC-64/XZ
	{
	private:
		uint64 value = 0;

	public:
		static uint64 Compute(const void* data, uintptr size, uint64 seed = 0);

		inline void process(const void* data, uintptr size) { value = Compute(data, size, value); }
		inline uint64 getValue() { return value; }
		inline void reset() { value = 0; }

		template <typename Type>
		inline void process(const Type& data) { process(&data, sizeof(data)); }
		template <typename Type>
		static inline uint64 Compute(const Type& data) { return Compute(&data, sizeof(data)); }
	};
}
