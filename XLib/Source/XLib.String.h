#pragma once

#include "XLib.h"
#include "XLib.AllocatorAdapterBase.h"
#include "XLib.SystemHeapAllocator.h"

// TODO: Decide what to do with `String::getCStr()` for empty string.

namespace XLib
{
	template <typename CharType = char>
	class BaseStringView
	{
	private:
		const CharType* data = nullptr;
		uintptr length = 0;

	public:
		BaseStringView() = default;
		~BaseStringView() = default;

		inline BaseStringView(const CharType* data, uintptr length) : data(data), length(length) {}
		inline BaseStringView(const CharType* begin, const CharType* end) : data(begin), length(end - begin) {}
		inline BaseStringView(const CharType* cstr);

		inline const CharType& operator [] (uintptr index) const { return data[index]; }
		inline const CharType* getData() const { return data; }
		inline uintptr getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }

		inline const CharType* begin() const { return data; }
		inline const CharType* end() const { return data + length; }
	};

	using StringView = BaseStringView<>;


	template <typename CharType = char, typename CounterType = uint32, typename AllocatorType = SystemHeapAllocator>
	class BaseString : private AllocatorAdapterBase<AllocatorType>
	{
	private:
		using AllocatorBase = AllocatorAdapterBase<AllocatorType>;

		static constexpr CounterType IntialBufferCapacity = 16;

	private:
		CharType* buffer = nullptr;
		CounterType capacity = 0;
		CounterType length = 0;

	private:
		inline void ensureCapacity(CounterType requiredLength);

	public:
		BaseString() = default;
		~BaseString();

		inline BaseString(BaseString&& that);
		inline void operator = (BaseString&& that);

		inline void pushBack(CharType c);
		inline void append(const char* stringToAppend);

		inline void resize(CounterType newLength, CharType c = '\0');
		inline void resizeUnsafe(CounterType newLength);
		inline void clear();
		inline void compact();
		inline void reserve(CounterType newMaxLength) { ensureCapacity(newMaxLength); }

		inline CharType* getMutableData() { return buffer; }

		inline StringView getView() const { return StringView(buffer, length); }
		inline const CharType* getCStr() const { return buffer; }
		inline const CharType* getData() const { return buffer; }
		inline CounterType getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }

		inline CharType& operator [] (CounterType index) { return buffer[index]; }
		inline CharType operator [] (CounterType index) const { return buffer[index]; }
	};


	template <uintptr Capacity, typename CounterType = uint8, typename CharType = char>
	class InplaceString
	{
		static_assert(Capacity > 1); // At least one character.
		// TODO: Check that capacity is less then counter max value.

	private:
		CounterType length = 0;
		CharType storage[Capacity];

	public:
		inline InplaceString();
		~InplaceString() = default;

		inline bool copyFrom(const char* cstr, uintptr lengthLimit = uintptr(-1));
		inline bool copyFrom(StringView string);

		inline void pushBack(CharType c);
		inline bool append(const char* cstr, uintptr appendLengthLimit = uintptr(-1));
		inline bool append(StringView string);

		inline void clear();
		inline void truncate(CounterType newLength);
		//inline bool recalculateLength();

		inline CharType* getMutableData() { return &storage; }

		inline StringView getView() const { return StringView(&storage, length); }
		inline const CharType* getCStr() const { return storage; }
		inline const CharType* getData() const { return storage; }
		inline CounterType getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }
		inline bool isFull() const { return length + 1 == Capacity; }

		static constexpr CounterType GetCapacity() { return Capacity; }
		static constexpr CounterType GetMaxLength() { return Capacity - 1; }
	};

	using String = BaseString<>;

	using InplaceString32 = InplaceString<31, uint8>;		static_assert(sizeof(InplaceString32) == 32);
	using InplaceString64 = InplaceString<63, uint8>;		static_assert(sizeof(InplaceString64) == 64);
	using InplaceString256 = InplaceString<255, uint8>;		static_assert(sizeof(InplaceString256) == 256);
	using InplaceString1024 = InplaceString<1022, uint16>;	static_assert(sizeof(InplaceString1024) == 1024);
	using InplaceString2048 = InplaceString<2046, uint16>;	static_assert(sizeof(InplaceString2048) == 2048);
	using InplaceString4096 = InplaceString<4094, uint16>;	static_assert(sizeof(InplaceString4096) == 4096);


	template <typename CharType>
	inline bool AreStringsEqual(const BaseStringView<CharType>& a, const BaseStringView<CharType>& b);

	template <typename CharType>
	inline bool AreStringsEqual(const BaseStringView<CharType>& a, const CharType* bCStr);

	template <typename CharType>
	inline sint32 CompareStrings();

	template <typename CharType = char>
	inline uintptr GetCStrLength(const CharType* cstr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void BaseString<CharType, CounterType, AllocatorType>::ensureCapacity(CounterType requiredLength)
	{
		const CounterType requiredCapacity = requiredLength + 1;
		if (requiredCapacity <= capacity)
			return;

		// TODO: Maybe align up to 16 or some adaptive pow of 2.
		capacity = max<CounterType>(requiredCapacity, capacity * 2, IntialBufferCapacity);
		buffer = (CharType*)AllocatorBase::reallocate(buffer, capacity * sizeof(CharType));
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline BaseString<CharType, CounterType, AllocatorType>::~BaseString()
	{
		if (buffer)
			AllocatorBase::release(buffer);
		buffer = nullptr;
		capacity = 0;
		length = 0;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void BaseString<CharType, CounterType, AllocatorType>::pushBack(CharType c)
	{
		ensureCapacity(length + 1);
		buffer[length] = c;
		length++;
		buffer[length] = '\0';
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void BaseString<CharType, CounterType, AllocatorType>::append(const char* stringToAppend)
	{
		// TODO: Assert for `CounterType` overflow.
		const CounterType stringToAppendLength = CounterType(GetCStrLength(stringToAppend));
		ensureCapacity(length + stringToAppendLength);
		memoryCopy(buffer + length, stringToAppend, stringToAppendLength + 1);
		length += stringToAppendLength;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void BaseString<CharType, CounterType, AllocatorType>::resize(CounterType newLength, CharType c)
	{
		if (length < newLength)
		{
			ensureCapacity(newLength);
			for (CharType* i = buffer + length; i != buffer + newLength; i++)
				*i = c;
		}
		if (buffer)
			buffer[newLength] = '\0';
		length = newLength;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void BaseString<CharType, CounterType, AllocatorType>::resizeUnsafe(CounterType newLength)
	{
		ensureCapacity(newLength);
		buffer[newLength] = '\0';
		length = newLength;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void BaseString<CharType, CounterType, AllocatorType>::clear()
	{
		if (buffer)
			buffer[0] = '\0';
		length = 0;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void BaseString<CharType, CounterType, AllocatorType>::compact()
	{
		if (capacity > length + 1)
		{
			capacity = length + 1;
			buffer = (CharType*)AllocatorBase::reallocate(buffer, capacity * sizeof(CharType));
		}

		// TODO: Handle case for length == 0
	}


	template <typename CharType>
	inline bool AreStringsEqual(const BaseStringView<CharType>& a, const CharType* bCStr)
	{
		for (uintptr i = 0; i < a.getLength(); i++)
		{
			if (a[i] != bCStr[i])
				return false;
			if (bCStr[i] == 0)
				return false;
		}
		return bCStr[a.getLength()] == 0;
	}

	template <typename CharType>
	inline uintptr GetCStrLength(const CharType* cstr)
	{
		const CharType* it = cstr;
		while (*it)
			it++;
		return uintptr(it - cstr);
	}
}
