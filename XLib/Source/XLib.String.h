#pragma once

#include "XLib.h"
#include "XLib.AllocatorAdapterBase.h"
#include "XLib.SystemHeapAllocator.h"

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
		inline BaseStringView(const CharType* cstr);

		inline const CharType& operator [] (uintptr index) const { return data[index]; }
		inline const CharType* getData() const { return data; }
		inline uintptr getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }
	};

	using StringView = BaseStringView<>;


	template <typename CharType = char, typename CounterType = uint32, typename AllocatorType = SystemHeapAllocator>
	class BaseString : private AllocatorAdapterBase<AllocatorType>
	{
	private:
		CharType* buffer = nullptr;
		CounterType capacity = 0;
		CounterType length = 0;

	private:
		inline void ensureCapacity(CounterType requiredCapacity);

	public:
		BaseString() = default;
		~BaseString();

		inline void append(const char* cstr);
		inline void pushBack(CharType c);

		inline void resize(CounterType newLength);
		inline void clear();
		inline void compact();
		inline void reserve(CounterType newCapacity);

		inline CharType* getMutableData() { return buffer; }

		inline StringView getView() const { return StringView(buffer, length); }
		inline const CharType* getCStr() const { return buffer; }
		inline const CharType* getData() const { return buffer; }
		inline CounterType getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }
	};

	using String = BaseString<>;


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

	using InplaceString32 = InplaceString<31, uint8>;		static_assert(sizeof(InplaceString32) == 32);
	using InplaceString64 = InplaceString<63, uint8>;		static_assert(sizeof(InplaceString64) == 64);
	using InplaceString256 = InplaceString<255, uint8>;		static_assert(sizeof(InplaceString256) == 256);
	using InplaceString1024 = InplaceString<1022, uint16>;	static_assert(sizeof(InplaceString1024) == 1024);
	using InplaceString4096 = InplaceString<4094, uint16>;	static_assert(sizeof(InplaceString4096) == 4096);


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
	BaseString<CharType, CounterType, AllocatorType>::~BaseString()
	{
		AllocatorBase::release(buffer);
		buffer = nullptr;
		capacity = 0;
		length = 0;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	void BaseString<CharType, CounterType, AllocatorType>::pushBack(CharType c)
	{

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
