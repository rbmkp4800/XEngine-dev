#pragma once

#include "XLib.h"

// TODO: implement standart algorithms

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

	class CRC32
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
}