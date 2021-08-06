#pragma once

#include "XLib.h"

namespace XLib
{
	template <uint8 digitCount = 0, bool leadingZeros = false>
	struct FmtHex
	{
		uint32 value;
		explicit FmtHex(uint32 _value) : value(_value) {}
	};

	template <uint8 digitCount = 0, uint8 precision = 0>
	struct FmtFP32
	{
		float32 value;
		explicit FmtFP32(float32 _value) : value(_value) {}
	};

	static constexpr char endOfString = '\0';

	class StringWriter
	{
	private:
		char *current, *end;

		inline bool _put(char value)
		{
			if (current < end)
			{
				*current = value;
				current++;
				return true;
			}
			return false;
		}

		bool _put(uint32 value);
		bool _put(const char* value);
		bool _putHex(uint32 value, uint8 digitCount, bool leadingZeros);
		bool _putFP32(float32 value, uint8 digitCount, uint8 precision);

		template <uint8 digitCount, bool leadingZeros>
		inline bool _put(const FmtHex<digitCount, leadingZeros> fmt)
			{ return _putHex(fmt.value, digitCount, leadingZeros); }

		template <uint8 digitCount = 0, uint8 precision = 0>
		inline bool _put(const FmtFP32<digitCount, precision> fmt)
			{ return _putFP32(fmt.value, digitCount, precision); }

	public:
		inline StringWriter(char* buffer, uintptr size)
			: current(buffer), end(buffer + size) {}
		template <uintptr length>
		inline StringWriter(char(&buffer)[length])
			: current(buffer), end(buffer + length) {}

		template <typename ... Types>
		inline bool put(Types ... args)
		{
			bool results[] = { _put(args) ... };
			bool totalResult = true;
			for each (bool result in results)
				totalResult &= result;
			return totalResult;
		}

		inline bool skip(uintptr length)
		{
			current += length;
			if (current > end)
			{
				current = end;
				return false;
			}
			return true;
		}
		inline char* getPosition() { return current; }
		inline void setPosition(char* position) { current = position; }

		inline bool isFull() { return current >= end; }
	};
}