#pragma once

#include "XLib.h"

namespace XLib
{
	template <uint32 BitCount>
	class BitSet
	{
	private:
		static constexpr uint32 StorageSizeU64s = divRoundUp(BitCount, 64);

	public:
		inline BitSet();
		~BitSet() = default;

		inline void clear();
		inline void set(uint32 index);
		inline void reset(uint32 index);

		inline bool isSet(uintptr index) const;
		inline sint32 findFirstOne() const;
		inline sint32 findFirstZero() const;

		inline sint32 findFirstZeroAndSet() const;


	};
}