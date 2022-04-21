#pragma once

#include "XLib.h"
#include "XLib.AllocatorAdapterBase.h"
#include "XLib.SystemHeapAllocator.h"

// TODO: Decide what to do with `String::getCStr()` for empty string.
// TODO: Do we need separate `pushBack()` and `append()`?

namespace XLib
{
	template <typename CharType>
	class StringView
	{
	private:
		const CharType* data = nullptr;
		uintptr length = 0;

	public:
		StringView() = default;
		~StringView() = default;

		inline constexpr StringView(const CharType* data, uintptr length) : data(data), length(length) {}
		inline constexpr StringView(const CharType* begin, const CharType* end) : data(begin), length(end - begin) {}
		inline constexpr StringView(const CharType* cstr);

		inline const CharType& operator [] (uintptr index) const { return data[index]; }
		inline const CharType* getData() const { return data; }
		inline uintptr getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }

		inline const CharType* begin() const { return data; }
		inline const CharType* end() const { return data + length; }
	};


	template <typename CharType, uintptr Capacity, typename CounterType = uint16>
	class InplaceString
	{
		static_assert(Capacity > 1); // At least one character.
		// TODO: Check that capacity is less then counter max value.

	private:
		CounterType length = 0;
		CharType buffer[Capacity];

	public:
		inline InplaceString();
		~InplaceString() = default;

		inline bool copyFrom(StringView<CharType> string);
		inline bool copyFrom(const CharType* cstr);

		inline bool pushBack(CharType c);
		inline bool append(CharType c) { return pushBack(c); }
		inline bool append(StringView<CharType> string);
		inline bool append(const CharType* cstr);

		inline void resize(CounterType newLength, CharType c = CharType(0));
		inline void resizeUnsafe(CounterType newLength);
		inline void clear();
		inline void truncate(CounterType newLength);
		//inline bool recalculateLength();

		inline const CharType* getCStr() const { return buffer; }
		inline StringView<CharType> getView() const { return StringView<CharType>(buffer, length); }
		inline CharType* getData() { return buffer; }
		inline CounterType getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }
		inline bool isFull() const { return length + 1 == Capacity; }

		inline CharType& operator [] (CounterType index) { return buffer[index]; }
		inline CharType operator [] (CounterType index) const { return buffer[index]; }
		inline operator StringView<CharType>() const { return getView(); }

		static constexpr CounterType GetCapacity() { return Capacity; }
		static constexpr CounterType GetMaxLength() { return Capacity - 1; }
	};


	template <typename CharType, typename CounterType = uint32, typename AllocatorType = SystemHeapAllocator>
	class DynamicString : private AllocatorAdapterBase<AllocatorType>
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
		DynamicString() = default;
		~DynamicString();

		inline DynamicString(DynamicString&& that);
		inline void operator = (DynamicString&& that);

		inline bool pushBack(CharType c);
		inline bool append(CharType c) { return pushBack(c); }
		inline bool append(StringView<CharType> string);
		inline bool append(const CharType* cstr);

		inline void resize(CounterType newLength, CharType c = CharType(0));
		inline void resizeUnsafe(CounterType newLength);
		inline void clear();
		inline void compact();
		inline void reserve(CounterType newMaxLength) { ensureCapacity(newMaxLength); }

		inline const CharType* getCStr() const { return buffer; }
		inline StringView<CharType> getView() const { return StringView<CharType>(buffer, length); }
		inline CharType* getData() { return buffer; }
		inline CounterType getLength() const { return length; }
		inline bool isEmpty() const { return length == 0; }

		inline CharType& operator [] (CounterType index) { return buffer[index]; }
		inline CharType operator [] (CounterType index) const { return buffer[index]; }
		inline operator StringView<CharType>() const { return getView(); }
	};


	using StringViewASCII	= StringView<charASCII>;
	//using StringViewUTF8	= StringView<charUTF8>;
	//using StringViewUTF16	= StringView<charUTF16>;
	//using StringViewUTF32	= StringView<charUTF32>;

	template <uintptr Capacity, typename CounterType = uint16>
	using InplaceStringASCII = InplaceString<charASCII, Capacity, CounterType>;

	using DynamicStringASCII = DynamicString<charASCII>;

	using InplaceStringASCIIx32		= InplaceString<charASCII, 31,	 uint8>;	static_assert(sizeof(InplaceStringASCIIx32) == 32);
	using InplaceStringASCIIx64		= InplaceString<charASCII, 63,	 uint8>;	static_assert(sizeof(InplaceStringASCIIx64) == 64);
	using InplaceStringASCIIx128	= InplaceString<charASCII, 127,	 uint8>;	static_assert(sizeof(InplaceStringASCIIx128) == 128);
	using InplaceStringASCIIx256	= InplaceString<charASCII, 255,	 uint8>;	static_assert(sizeof(InplaceStringASCIIx256) == 256);
	using InplaceStringASCIIx512	= InplaceString<charASCII, 510,	 uint16>;	static_assert(sizeof(InplaceStringASCIIx512) == 512);
	using InplaceStringASCIIx1024	= InplaceString<charASCII, 1022, uint16>;	static_assert(sizeof(InplaceStringASCIIx1024) == 1024);
	using InplaceStringASCIIx2048	= InplaceString<charASCII, 2046, uint16>;	static_assert(sizeof(InplaceStringASCIIx2048) == 2048);
	using InplaceStringASCIIx4096	= InplaceString<charASCII, 4094, uint16>;	static_assert(sizeof(InplaceStringASCIIx4096) == 4096);


	class String abstract final
	{
	public:
		template <typename CharType>
		static inline constexpr uintptr GetCStrLength(const CharType* cstr);

		template <typename CharType>
		static inline bool IsEqual(const StringView<CharType>& a, const StringView<CharType>& b);

		template <typename CharType>
		static inline bool IsEqual(const StringView<CharType>& a, const CharType* bCStr);

		template <typename CharType>
		static inline ordering CompareOrdered(const StringView<CharType>& left, const StringView<CharType>& right);
	};

	template <typename TextWriter, typename CharType>
	inline bool TextWriteFmt_HandleArg(TextWriter& writer, const StringView<CharType>& string);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	// StringView //////////////////////////////////////////////////////////////////////////////////

	template <typename CharType>
	inline constexpr StringView<CharType>::StringView(const CharType* cstr)
		: data(cstr), length(String::GetCStrLength(cstr)) {}

	// InplaceString ///////////////////////////////////////////////////////////////////////////////

	template <typename CharType, uintptr Capacity, typename CounterType>
	InplaceString<CharType, Capacity, CounterType>::InplaceString()
	{
		buffer[0] = CharType(0);
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	auto InplaceString<CharType, Capacity, CounterType>::
		copyFrom(StringView<CharType> string) -> bool
	{
		const bool overflow = string.getLength() > GetMaxLength();
		length = overflow ? GetMaxLength() : CounterType(string.getLength());
		memoryCopy(buffer, string.getData(), length * sizeof(CharType));
		buffer[length] = CharType(0);
		return !overflow;
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	auto InplaceString<CharType, Capacity, CounterType>::
		copyFrom(const CharType* cstr) -> bool
	{
		CounterType newLengthAccum = 0;
		bool overflow = false;
		while (cstr[newLengthAccum])
		{
			if (newLengthAccum == GetMaxLength())
			{
				overflow = true;
				break;
			}

			buffer[newLengthAccum] = cstr[newLengthAccum];
			newLengthAccum++;
		}

		length = newLengthAccum;
		buffer[length] = CharType(0);
		return !overflow;
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	auto InplaceString<CharType, Capacity, CounterType>::
		pushBack(CharType c) -> bool
	{
		if (isFull())
			return false;
		buffer[length] = c;
		length++;
		buffer[length] = CharType(0);
		return true;
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	auto InplaceString<CharType, Capacity, CounterType>::
		append(StringView<CharType> string) -> bool
	{
		const CounterType fullLength = length + CounterType(string.getLength());
		const bool overflow = fullLength > GetMaxLength();
		const CounterType numCharsToCopy = overflow ? GetMaxLength() - length : CounterType(string.getLength());
		memoryCopy(buffer + length, string.getData(), numCharsToCopy * sizeof(CharType));
		length = overflow ? GetMaxLength() : fullLength;
		buffer[length] = CharType(0);
		return !overflow;
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	auto InplaceString<CharType, Capacity, CounterType>::
		clear() -> void
	{
		length = 0;
		buffer[0] = CharType(0);
	}


	// DynaicString ////////////////////////////////////////////////////////////////////////////////

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void DynamicString<CharType, CounterType, AllocatorType>::ensureCapacity(CounterType requiredLength)
	{
		const CounterType requiredCapacity = requiredLength + 1;
		if (requiredCapacity <= capacity)
			return;

		// TODO: Maybe align up to 16 or some adaptive pow of 2.
		capacity = max<CounterType>(requiredCapacity, capacity * 2, IntialBufferCapacity);
		buffer = (CharType*)AllocatorBase::reallocate(buffer, capacity * sizeof(CharType));
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline DynamicString<CharType, CounterType, AllocatorType>::~DynamicString()
	{
		if (buffer)
			AllocatorBase::release(buffer);
		buffer = nullptr;
		capacity = 0;
		length = 0;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline bool DynamicString<CharType, CounterType, AllocatorType>::pushBack(CharType c)
	{
		ensureCapacity(length + 1);
		buffer[length] = c;
		length++;
		buffer[length] = CharType(0);
		return true;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline bool DynamicString<CharType, CounterType, AllocatorType>::append(const CharType* cstr)
	{
		// TODO: Assert for `CounterType` overflow.
		const CounterType stringToAppendLength = CounterType(GetCStrLength(cstr));
		ensureCapacity(length + stringToAppendLength);
		memoryCopy(buffer + length, cstr, stringToAppendLength + 1);
		length += stringToAppendLength;
		return true;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void DynamicString<CharType, CounterType, AllocatorType>::resize(CounterType newLength, CharType c)
	{
		if (length < newLength)
		{
			ensureCapacity(newLength);
			for (CharType* i = buffer + length; i != buffer + newLength; i++)
				*i = c;
		}
		if (buffer)
			buffer[newLength] = CharType(0);
		length = newLength;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void DynamicString<CharType, CounterType, AllocatorType>::resizeUnsafe(CounterType newLength)
	{
		ensureCapacity(newLength);
		buffer[newLength] = CharType(0);
		length = newLength;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void DynamicString<CharType, CounterType, AllocatorType>::clear()
	{
		if (buffer)
			buffer[0] = CharType(0);
		length = 0;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void DynamicString<CharType, CounterType, AllocatorType>::compact()
	{
		if (capacity > length + 1)
		{
			capacity = length + 1;
			buffer = (CharType*)AllocatorBase::reallocate(buffer, capacity * sizeof(CharType));
		}

		// TODO: Handle case for length == 0
	}


	// String //////////////////////////////////////////////////////////////////////////////////////

	template <typename CharType>
	inline constexpr uintptr String::GetCStrLength(const CharType* cstr)
	{
		const CharType* it = cstr;
		while (*it)
			it++;
		return uintptr(it - cstr);
	}

	template <typename CharType>
	inline bool String::IsEqual(const StringView<CharType>& a, const CharType* bCStr)
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
	inline ordering String::CompareOrdered(const StringView<CharType>& left, const StringView<CharType>& right)
	{
		const uintptr leftLength = left.getLength();
		const uintptr rightLength = right.getLength();

		for (uintptr i = 0; i < leftLength; i++)
		{
			if (i >= rightLength)
				return ordering::greater;
			if (left[i] > right[i])
				return ordering::greater;
			if (left[i] < right[i])
				return ordering::less;
		}

		return leftLength == rightLength ? ordering::equivalent : ordering::less;
	}
}

template <typename TextWriter, typename CharType>
inline bool XLib::TextWriteFmt_HandleArg(TextWriter& writer, const StringView<CharType>& string)
{
	for (CharType c : string)
	{
		if (!writer.append(c))
			return false;
	}
	// TODO: replace with `writer.writeChars()`.
	return true;
}
