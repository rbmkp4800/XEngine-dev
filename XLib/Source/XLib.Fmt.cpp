#include "XLib.Fmt.h"

using namespace XLib;

static const char HexCharLUT[] = "0123456789ABCDEF";

template <typename UIntT>
static inline FmtFormatResult FmtFormatDecUXX(UIntT value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	char internalBuffer[24];
	lzWidth = min<uint8>(lzWidth, countOf(internalBuffer));

	uint8 digitCount = 0;
	do
	{
		internalBuffer[digitCount] = value % 10 + '0';
		value /= 10;
		digitCount++;
	} while (value);

	for (; digitCount < lzWidth; digitCount++)
		internalBuffer[digitCount] = '0';

	if (digitCount > bufferSize)
		return FmtFormatResult { FmtFormatStatus::OutputBufferOverflow, 0 };

	for (uint8 i = 0, j = digitCount - 1; i < digitCount; i++, j--)
		buffer[i] = internalBuffer[j];

	return FmtFormatResult { FmtFormatStatus::Success, digitCount };
}

template <typename SIntT, typename UIntT>
static inline FmtFormatResult FmtFormatDecSXX(SIntT value, char* buffer, uint8 bufferSize, uint8 lzWidth, bool showPlus)
{
	uint8 unsignedPartOffset = 0;

	if (value < 0)
	{
		if (bufferSize < 1)
			return FmtFormatResult { FmtFormatStatus::OutputBufferOverflow, 0 };
		buffer[0] = '-';
		unsignedPartOffset = 1;
		value = -value;
	}
	else
	{
		if (showPlus)
		{
			if (bufferSize < 1)
				return FmtFormatResult { FmtFormatStatus::OutputBufferOverflow, 0 };
			buffer[0] = '+';
			unsignedPartOffset = 1;
		}
	}

	FmtFormatResult unsignedFormatResult =
		FmtFormatDecUXX<UIntT>(UIntT(value), buffer + unsignedPartOffset, bufferSize - unsignedPartOffset, lzWidth);

	if (unsignedFormatResult.status == FmtFormatStatus::Success)
		unsignedFormatResult.formattedCharCount += unsignedPartOffset;

	return unsignedFormatResult;
}

template <typename UIntT>
static inline FmtFormatResult FmtFormatHexXX(UIntT value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	char internalBuffer[16];
	lzWidth = min<uint8>(lzWidth, countOf(internalBuffer));

	uint8 digitCount = 0;
	do
	{
		internalBuffer[digitCount] = HexCharLUT[value & 0xF];
		value >>= 4;
		digitCount++;
	} while (value);

	for (; digitCount < lzWidth; digitCount++)
		internalBuffer[digitCount] = '0';

	if (digitCount > bufferSize)
		return FmtFormatResult { FmtFormatStatus::OutputBufferOverflow, 0 };

	for (uint8 i = 0, j = digitCount - 1; i < digitCount; i++, j--)
		buffer[i] = internalBuffer[j];

	return FmtFormatResult { FmtFormatStatus::Success, digitCount };
}


////////////////////////////////////////////////////////////////////////////////////////////////////

FmtFormatResult XLib::FmtFormatDecU8(uint8 value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	return FmtFormatDecUXX<uint8>(value, buffer, bufferSize, lzWidth);
}

FmtFormatResult XLib::FmtFormatDecU16(uint16 value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	return FmtFormatDecUXX<uint16>(value, buffer, bufferSize, lzWidth);
}

FmtFormatResult XLib::FmtFormatDecU32(uint32 value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	return FmtFormatDecUXX<uint32>(value, buffer, bufferSize, lzWidth);
}

FmtFormatResult XLib::FmtFormatDecU64(uint64 value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	return FmtFormatDecUXX<uint64>(value, buffer, bufferSize, lzWidth);
}


FmtFormatResult XLib::FmtFormatDecS8(sint8 value, char* buffer, uint8 bufferSize, uint8 lzWidth, bool showPlus)
{
	return FmtFormatDecSXX<sint8, uint8>(value, buffer, bufferSize, lzWidth, showPlus);
}

FmtFormatResult XLib::FmtFormatDecS16(sint16 value, char* buffer, uint8 bufferSize, uint8 lzWidth, bool showPlus)
{
	return FmtFormatDecSXX<sint16, uint16>(value, buffer, bufferSize, lzWidth, showPlus);
}

FmtFormatResult XLib::FmtFormatDecS32(sint32 value, char* buffer, uint8 bufferSize, uint8 lzWidth, bool showPlus)
{
	return FmtFormatDecSXX<sint32, uint32>(value, buffer, bufferSize, lzWidth, showPlus);
}

FmtFormatResult XLib::FmtFormatDecS64(sint64 value, char* buffer, uint8 bufferSize, uint8 lzWidth, bool showPlus)
{
	return FmtFormatDecSXX<sint64, uint64>(value, buffer, bufferSize, lzWidth, showPlus);
}


FmtFormatResult XLib::FmtFormatHex8(uint8 value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	return FmtFormatHexXX<uint8>(value, buffer, bufferSize, lzWidth);
}

FmtFormatResult XLib::FmtFormatHex16(uint16 value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	return FmtFormatHexXX<uint16>(value, buffer, bufferSize, lzWidth);
}

FmtFormatResult XLib::FmtFormatHex32(uint32 value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	return FmtFormatHexXX<uint32>(value, buffer, bufferSize, lzWidth);
}

FmtFormatResult XLib::FmtFormatHex64(uint64 value, char* buffer, uint8 bufferSize, uint8 lzWidth)
{
	return FmtFormatHexXX<uint64>(value, buffer, bufferSize, lzWidth);
}
