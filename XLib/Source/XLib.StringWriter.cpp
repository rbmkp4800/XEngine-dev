#include "XLib.StringWriter.h"

using namespace XLib;

inline char quadToHexUpper(uint8 quad) { return '0' + quad + ('A' - '9' - 1) * (quad > 9); }

bool StringWriter::_put(uint32 value)
{
	if (current >= end)
		return false;

	uint32 valueLength = 0;
	if (value)
	{
		uint32 _value = value;
		while (_value)
		{
			_value /= 10;
			valueLength++;
		}
	}
	else
		valueLength = 1;

	if (current + valueLength > end)
	{
		*current = 0;
		current = end;
		return false;
	}

	sint32 position = valueLength - 1;
	while (position >= 0)
	{
		current[position] = '0' + value % 10;
		value /= 10;
		position--;
	}

	current += valueLength;

	return true;
}

bool StringWriter::_put(const char* value)
{
	if (current >= end)
		return false;

	while (*value)
	{
		*current = *value;
		current++;
		value++;
		if (current + 1 == end)
		{
			*current = 0;
			return false;
		}
	}

	return true;
}

bool StringWriter::_putHex(uint32 value, uint8 digitCount, bool leadingZeros)
{
	if (leadingZeros && !digitCount)
		digitCount = 8;

	if (digitCount)
	{
		if (current + digitCount >= end)
		{
			for (; current < end; current++)
				*current = 0;
			return false;
		}

		if (leadingZeros)
		{
			char* limit = current + digitCount;
			uint8 offset = (digitCount - 1) * 4;
			for (; current < limit; current++)
			{
				*current = quadToHexUpper(uint8(value >> offset) & 0b1111);
				offset -= 4;
			}
		}
	}

	return true;
}