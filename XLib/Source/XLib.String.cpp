#include <stdlib.h>

#include "XLib.String.h"

using namespace XLib;

/*bool Strings::ParseSInt32(const char* string, sint32& value, const char** end)
{
	bool minus = false;
	char c = *string;
	if (c >= '0' && c <= '9')
		goto label_parsingDigits;

	if (c == '-')
		minus = true;
	else if (c != '+')
		return false;

	string++;
	c = *string;

label_parsingDigits:
	sint32 result = c - '0';
	string++;
	c = *string;
	while (c >= '0' && c <= '9')
	{
		result *= 10;
		result += c - '0';

		string++;
		c = *string;
	}

	value = minus ? -result : result;
	if (end)
		*end = string;
	return true;
}

bool Strings::ParseFloat32(const char* string, float32& value, const char** end)
{
	char *_end = nullptr;
	float32 result = strtof(string, &_end);
	if (string == _end)
		return false;

	value = result;
	if (end)
		*end = _end;
	return true;
}*/
