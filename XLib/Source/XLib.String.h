#pragma once

#include "XLib.h"
#include "XLib.AllocatorAdapterBase.h"
#include "XLib.SystemHeapAllocator.h"

namespace XLib
{
	template <typename CharType = char>
	class StringView
	{
	private:
		CharType* str;
		uintptr length;

	public:

	};


	template <typename CharType = char, typename CounterType = uint32, typename AllocatorType = SystemHeapAllocator>
	class String
		: private AllocatorAdapterBase<AllocatorType>
	{
	private:
		CharType* buffer = nullptr;
		CounterType capacity = 0;
		CounterType length = 0;

	public:
		String() = default;
		~String() = default;

		inline void append();

		inline void clear();
		inline void truncate(CounterType newLength);

		inline const CharType* cstr() const { return buffer; }

		inline CounterType getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }
	};


	template <uintptr Capacity = 64, typename CounterType = uint8, typename CharType = char>
	class InplaceString
	{
	private:
		CounterType length = 0;
		CharType storage[Capacity];

	public:
		inline InplaceString();
		~InplaceString() = default;

		inline bool copyFromCStr(const char* string, uintptr lengthLimit = uintptr(-1));

		inline bool append(const char* string, uintptr appendLengthLimit = uintptr(-1));
		//inline bool append(StringView);

		inline void clear();
		inline void truncate(CounterType newLength);
		//inline bool recalculateLength();

		inline const CharType* cstr() const { return &storage; }
		//inline operator const CharType*() const { return &storage; }
		inline CharType* getMutableStorage() { return &storage; }

		inline bool isEmpty() const { return length == 0; }
		inline bool isFull() const { return length + 1 == Capacity; }
		inline CounterType getLength() const { return length; }

		inline CounterType getCapacity() const { return Capacity; }
		static inline CounterType GetCapacity() { return Capacity; }
	};

	using InplaceString32 = InplaceString<31, uint8>;		static_assert(sizeof(InplaceString32) == 32);
	using InplaceString64 = InplaceString<63, uint8>;		static_assert(sizeof(InplaceString64) == 64);
	using InplaceString256 = InplaceString<255, uint8>;		static_assert(sizeof(InplaceString256) == 256);
	using InplaceString1024 = InplaceString<1022, uint16>;	static_assert(sizeof(InplaceString1024) == 1024);
	using InplaceString4096 = InplaceString<4094, uint16>;	static_assert(sizeof(InplaceString4096) == 4096);


	template <typename CharType>
	inline sint32 CompareStrings();

	template <typename CharType>
	inline uintptr ComputeCStrLength(const CharType* cstr);

	/*class Strings abstract final
	{
	private:
		static bool ParseSInt32(const char* string, sint32& value, const char** end);
		static bool ParseFloat32(const char* string, float32& value, const char** end);

	public:
		static inline uintptr Length(const char* string)
		{
			uintptr length = 0;
			while (*string)
			{
				length++;
				string++;
			}
			return length;
		}
		static inline uintptr Length(const wchar* string)
		{
			uintptr length = 0;
			while (*string)
			{
				length++;
				string++;
			}
			return length;
		}
		static inline uintptr Length(const char* string, uintptr limit)
		{
			uintptr length = 0;
			while (*string && length < limit)
			{
				length++;
				string++;
			}
			return length;
		}

		template <typename Type>
		static bool Parse(const char* string, Type& value, const char** end = nullptr) { static_assert("invalid type"); }

		template <> static inline bool Parse<sint32>(const char* string, sint32& value, const char** end) { return ParseSInt32(string, value, end); }
		template <> static inline bool Parse<float32>(const char* string, float32& value, const char** end) { return ParseFloat32(string, value, end); }
	};*/
}