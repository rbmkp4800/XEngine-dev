#pragma once

#include "XLib.h"

namespace XLib
{
	class BitArray
	{

	};

	template <uint32 BitCount>
	class InplaceBitArray
	{
	private:
		static_assert(BitCount > 0);

		static constexpr uint32 StorageSizeU64s = divRoundUp<uint32>(BitCount, 64);
		static constexpr bool ArrayIsAlignedToStorage = (BitCount & 0x3F) == 0;

	private:
		uint64 buffer[StorageSizeU64s];

	public:
		InplaceBitArray() = default;
		~InplaceBitArray() = default;

		inline void clear() { memorySet(buffer, 0, sizeof(buffer)); }
		inline void set(uint32 index);
		inline void reset(uint32 index);
		inline bool isSet(uintptr index) const;

		inline sint32 findFirstOne() const;
		inline sint32 findFirstZero() const;

		inline sint32 findFirstOneAndReset();
		inline sint32 findFirstZeroAndSet();
	};
}

namespace XLib
{
	template <uint32 BitCount>
	inline void InplaceBitArray<BitCount>::set(uint32 index)
	{
		XAssert(index < BitCount);
		buffer[index >> 6] |= uint64(1) << (index & 0x3F);
	}

	template <uint32 BitCount>
	inline void InplaceBitArray<BitCount>::reset(uint32 index)
	{
		const uint32 indexU64 = index >> 6;
		XAssert(index < BitCount);
		buffer[index >> 6] &= ~(uint64(1) << (index & 0x3F));
	}

	template <uint32 BitCount>
	inline bool InplaceBitArray<BitCount>::isSet(uintptr index) const
	{
		XAssert(index < BitCount);
		return bool( (buffer[index >> 6] >> (index & 0x3F)) & 1 );
	}

	template <uint32 BitCount>
	inline sint32 InplaceBitArray<BitCount>::findFirstOne() const
	{
		for (uint32 i = 0; i < StorageSizeU64s; i++)
		{
			const uint8 ctzResult = countTrailingZeros64(buffer[i]);
			if (ctzResult != 64)
			{
				const uint32 result = i * 64 + ctzResult;
				return (ArrayIsAlignedToStorage || result < BitCount) ? result : -1;
			}
		}
		return -1;
	}

	template <uint32 BitCount>
	inline sint32 InplaceBitArray<BitCount>::findFirstZero() const
	{
		for (uint32 i = 0; i < StorageSizeU64s; i++)
		{
			const uint8 ctzResult = countTrailingZeros64(~buffer[i]);
			if (ctzResult != 64)
			{
				const uint32 result = i * 64 + ctzResult;
				return (ArrayIsAlignedToStorage || result < BitCount) ? result : -1;
			}
		}
		return -1;
	}

	template <uint32 BitCount>
	inline sint32 InplaceBitArray<BitCount>::findFirstOneAndReset()
	{
		const sint32 firstOneIndex = findFirstOne();
		if (firstOneIndex >= 0)
			reset(firstOneIndex);
		return firstOneIndex;
	}

	template <uint32 BitCount>
	inline sint32 InplaceBitArray<BitCount>::findFirstZeroAndSet()
	{
		const sint32 firstZeroIndex = findFirstZero();
		if (firstZeroIndex >= 0)
			set(firstZeroIndex);
		return firstZeroIndex;
	}
}
