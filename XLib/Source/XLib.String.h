#pragma once

#include "XLib.h"
#include "XLib.Allocation.h"

// TODO: Decide what to do with `String::getCStr()` for empty string.
// TODO: StringView -> StringViewBase, DynamicString -> DynamicStringBase, InplaceString -> InplaceStringBase ?
// TODO: Do something about million template methods compiled for `InplaceString` instances.
// TODO: Profile cost of always supporting zero terminator. Probably we may put it only when calling `getCStr`/`getData` methods.
// TODO: Remove CounterType template from InplaceString and just use uint16.
// TODO: Remove CounterType template from DynamicString and just use uint32.
// TODO: String container types should be in XLib.Containers.*** but we also have non-container utils here... Also StringView is certainly not a container. Decide what to do.
//		We may split this file into XLib.String.h and XLib.Containers.String.h

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

		inline constexpr StringView(const CharType* data, uintptr length) : data(length ? data : nullptr), length(length) {}
		inline constexpr StringView(const CharType* begin, const CharType* end) : data(end - begin ? begin : nullptr), length(end - begin) { XAssert(begin <= end); }
		inline constexpr explicit StringView(const CharType* cstr);

		inline const CharType* getData() const { return data; }
		inline uintptr getLength() const { return length; }

		inline const CharType* begin() const { return data; }
		inline const CharType* end() const { return data + length; }

		inline const CharType& operator [] (uintptr index) const { return data[index]; }

		inline bool operator == (const StringView<CharType>& that) const;
		inline bool operator == (const CharType* thatCStr) const;

		inline bool startsWith(const StringView<CharType>& prefix) const;
		inline bool startsWith(const CharType* prefixCStr) const;
		inline bool startsWith(CharType prefix) const;
		inline bool endsWith(const StringView<CharType>& suffix) const;
		inline bool endsWith(const CharType* suffixCStr) const;
		inline bool endsWith(CharType suffix) const { return length > 0 && data[length - 1] == suffix; }

		inline bool isEmpty() const { return length == 0; }

		inline StringView<CharType> getSubString(uintptr startIndex, uintptr length = uintptr(-1)) const;
	};


	template <typename CharType>
	class VirtualStringRef final
	{
	public:
		struct VTable
		{
			using GetDataFunc = CharType* (*)(void* stringPtr);
			using GetLengthFunc = uintptr(*)(void* stringPtr);
			using ResizeFunc = void (*)(void* stringPtr, uintptr length);

			GetDataFunc getData = nullptr;
			GetLengthFunc getLength = nullptr;
			ResizeFunc resize = nullptr;
			uintptr maxLength = 0;
			// Some kind of RTTI?
		};

	private:
		const VTable* vtablePtr = nullptr;
		void* stringPtr = nullptr;

	private:
		VirtualStringRef(const VTable* vtablePtr, void* stringPtr) : vtablePtr(vtablePtr), stringPtr(stringPtr) {}

	public:
		VirtualStringRef() = default;
		~VirtualStringRef() = default;

		inline void resize(uintptr length) { XAssert(length <= vtablePtr->maxLength); vtablePtr->resize(stringPtr, length); }
		inline void clear() { resize(0); }

		inline CharType* getData() { return vtablePtr->getData(stringPtr); }
		inline uintptr getLength() const { return vtablePtr->getLength(stringPtr); }
		inline uintptr getMaxLength() const { return vtablePtr->maxLength; }

	public:
		static inline VirtualStringRef Construct(const VTable* vtablePtr, void* stringPtr) { return VirtualStringRef(vtablePtr, stringPtr); }
	};


	template <typename CharType, uintptr Capacity, typename CounterType = uint16>
	class InplaceString
	{
		static_assert(Capacity > 1); // At least one character (+1 for cstr zero terminator).
		static_assert(Capacity == uintptr(CounterType(Capacity)));

	private:
		struct VirtualStringRefVTable : public VirtualStringRef<CharType>::VTable
		{
			inline VirtualStringRefVTable();
		};
		inline static VirtualStringRefVTable virtualStringRefVTable;

	private:
		CounterType length = 0;
		CharType buffer[Capacity];

	public:
		inline InplaceString();
		inline InplaceString(const StringView<CharType>& string)	{ append(string); }
		inline InplaceString(const CharType* cstr)					{ append(cstr); }
		inline InplaceString(const CharType* data, uintptr length)	{ append(data, length); }

		~InplaceString() = default;

		inline InplaceString& operator = (const StringView<CharType>& that)	{ length = 0; append(that);		return *this; }
		inline InplaceString& operator = (const CharType* thatCStr)			{ length = 0; append(thatCStr);	return *this; }

		inline void append(CharType c);
		inline void append(const StringView<CharType>& string);
		inline void append(const CharType* cstr);
		inline void append(const CharType* data, uintptr length) { return append(StringView<CharType>(data, length)); }

		inline void resize(CounterType newLength, CharType c = CharType(0));
		inline void clear();

		inline CharType* getData() { return buffer; }
		inline CounterType getLength() const { return length; }
		inline const CharType* getCStr() const { return buffer; }

		inline StringView<CharType> getView() const { return StringView<CharType>(buffer, length); }
		inline operator StringView<CharType>() const { return getView(); }

		inline VirtualStringRef<CharType> getVirtualRef() { return VirtualStringRef<CharType>::Construct(&virtualStringRefVTable, this); }
		inline operator VirtualStringRef<CharType>() { return getVirtualRef(); }

		inline CharType& operator [] (CounterType index) { return buffer[index]; }
		inline CharType operator [] (CounterType index) const { return buffer[index]; }

		inline bool operator == (const StringView<CharType>& that) const;
		inline bool operator == (const CharType* thatCStr) const;

		inline bool startsWith(const StringView<CharType>& prefix) const;
		inline bool startsWith(const CharType* prefixCStr) const;
		inline bool endsWith(const StringView<CharType>& suffix) const;
		inline bool endsWith(const CharType* suffixCStr) const;

		inline bool isEmpty() const { return length == 0; }
		inline bool isFull() const { return length + 1 == Capacity; }

	public:
		static constexpr CounterType GetCapacity() { return Capacity; }
		static constexpr CounterType GetMaxLength() { return Capacity - 1; }
	};


	template <typename CharType, typename CounterType = uint32, typename AllocatorType = SystemHeapAllocator>
	class DynamicString : private AllocatorAdapterBase<AllocatorType>
	{
	private:
		using AllocatorBase = AllocatorAdapterBase<AllocatorType>;

		static constexpr CounterType InitialBufferCapacity = 16;

	private:
		struct VirtualStringRefVTable : public VirtualStringRef<CharType>::VTable
		{
			inline VirtualStringRefVTable();
		};
		static VirtualStringRefVTable virtualStringRefVTable;

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
		inline DynamicString(const StringView<CharType>& that);
		inline DynamicString(const CharType* thatCStr);

		inline DynamicString& operator = (DynamicString&& that);
		inline DynamicString& operator = (const StringView<CharType>& that);
		inline DynamicString& operator = (const CharType* thatCStr);

		inline bool append(CharType c);
		inline bool append(const StringView<CharType>& string);
		inline bool append(const CharType* cstr) { return append(StringView<CharType>(cstr)); }
		inline bool append(const CharType* data, uintptr length) { return append(StringView<CharType>(data, length)); }

		inline void resize(CounterType newLength, CharType c = CharType(0));
		inline void clear();

		inline void reserve(CounterType newMaxLength) { ensureCapacity(newMaxLength); }
		inline void compact();

		inline CharType* getData() { return buffer; }
		inline CounterType getLength() const { return length; }
		inline const CharType* getCStr() const { buffer[length] = 0; return buffer; }

		inline StringView<CharType> getView() const { return StringView<CharType>(buffer, length); }
		inline operator StringView<CharType>() const { return getView(); }

		inline VirtualStringRef<CharType> getVirtualRef() { return VirtualStringRef<CharType>::Construct(&virtualStringRefVTable, this); }
		inline operator VirtualStringRef<CharType>() { return getVirtualRef(); }

		inline CharType& operator [] (CounterType index) { return buffer[index]; }
		inline CharType operator [] (CounterType index) const { return buffer[index]; }

		inline bool operator == (const StringView<CharType>& that) const;
		inline bool operator == (const CharType* thatCStr) const;

		inline bool startsWith(const StringView<CharType>& prefix) const;
		inline bool startsWith(const CharType* prefixCStr) const;
		inline bool endsWith(const StringView<CharType>& suffix) const;
		inline bool endsWith(const CharType* suffixCStr) const;

		inline bool isEmpty() const { return length == 0; }
	};


	using StringViewASCII = StringView<charASCII>;
	using VirtualStringRefASCII = VirtualStringRef<charASCII>;

	template <uintptr Capacity, typename CounterType = uint16>
	using InplaceStringASCII = InplaceString<charASCII, Capacity, CounterType>;

	using InplaceStringASCIIx32		= InplaceString<charASCII, 31,	 uint8>;	static_assert(sizeof(InplaceStringASCIIx32) == 32);
	using InplaceStringASCIIx64		= InplaceString<charASCII, 63,	 uint8>;	static_assert(sizeof(InplaceStringASCIIx64) == 64);
	using InplaceStringASCIIx128	= InplaceString<charASCII, 127,	 uint8>;	static_assert(sizeof(InplaceStringASCIIx128) == 128);
	using InplaceStringASCIIx256	= InplaceString<charASCII, 255,	 uint8>;	static_assert(sizeof(InplaceStringASCIIx256) == 256);
	using InplaceStringASCIIx512	= InplaceString<charASCII, 510,	 uint16>;	static_assert(sizeof(InplaceStringASCIIx512) == 512);
	using InplaceStringASCIIx1024	= InplaceString<charASCII, 1022, uint16>;	static_assert(sizeof(InplaceStringASCIIx1024) == 1024);
	using InplaceStringASCIIx2048	= InplaceString<charASCII, 2046, uint16>;	static_assert(sizeof(InplaceStringASCIIx2048) == 2048);
	using InplaceStringASCIIx4096	= InplaceString<charASCII, 4094, uint16>;	static_assert(sizeof(InplaceStringASCIIx4096) == 4096);

	using DynamicStringASCII = DynamicString<charASCII>;


	class String abstract final
	{
	public:
		template <typename CharType>
		static inline constexpr uintptr GetCStrLength(const CharType* cstr);

		template <typename CharType> static inline bool IsEqual(const StringView<CharType>& a, const StringView<CharType>& b);
		template <typename CharType> static inline bool IsEqual(const StringView<CharType>& a, const CharType* bCStr);

		template <typename CharType>
		static inline ordering CompareOrdered(const StringView<CharType>& left, const StringView<CharType>& right);

		template <typename CharType>
		static inline bool IsLess(const StringView<CharType>& left, const StringView<CharType>& right);

		template <typename CharType> static inline bool StartsWith(const StringView<CharType>& string, const StringView<CharType>& prefix);
		template <typename CharType> static inline bool StartsWith(const StringView<CharType>& string, const CharType* prefixCStr);
		template <typename CharType> static inline bool StartsWith(const CharType* cstr, const StringView<CharType>& prefix);
		template <typename CharType> static inline bool StartsWith(const CharType* cstr, const CharType* prefixCStr);

		template <typename CharType> static inline bool EndsWith(const StringView<CharType>& string, const StringView<CharType>& suffix);
		template <typename CharType> static inline bool EndsWith(const StringView<CharType>& string, const CharType* suffixCStr);
		template <typename CharType> static inline bool EndsWith(const CharType* cstr, const StringView<CharType>& suffix);
		template <typename CharType> static inline bool EndsWith(const CharType* cstr, const CharType* suffixCStr);
	};


	template <typename TextWriter, typename CharType>
	inline bool TextWriteFmt_HandleArg(TextWriter& writer, const StringView<CharType>& string);

	// TODO: Revisit these:
	template <typename TextWriter, typename CharType, uintptr Capacity, typename CounterType>
	inline bool TextWriteFmt_HandleArg(TextWriter& writer, const InplaceString<CharType, Capacity, CounterType>& string) { return TextWriteFmt_HandleArg(writer, StringView<CharType>(string)); }

	template <typename TextWriter, typename CharType, typename CounterType, typename AllocatorType>
	inline bool TextWriteFmt_HandleArg(TextWriter& writer, const DynamicString<CharType>& string) { return TextWriteFmt_HandleArg(writer, StringView<CharType>(string)); }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	// StringView //////////////////////////////////////////////////////////////////////////////////

	template <typename CharType>
	inline constexpr StringView<CharType>::StringView(const CharType* cstr)
		: data(cstr), length(String::GetCStrLength(cstr)) {}

	template <typename CharType>
	inline bool StringView<CharType>::operator == (const StringView<CharType>& that) const { return String::IsEqual(*this, that); }

	template <typename CharType>
	inline bool StringView<CharType>::operator == (const CharType* thatCStr) const { return String::IsEqual(*this, thatCStr); }

	template <typename CharType>
	inline bool StringView<CharType>::startsWith(const StringView<CharType>& prefix) const { return String::StartsWith(*this, prefix); }

	template <typename CharType>
	inline bool StringView<CharType>::startsWith(const CharType* prefixCStr) const { return String::StartsWith(*this, prefixCStr); }

	template <typename CharType>
	inline bool StringView<CharType>::endsWith(const StringView<CharType>& suffix) const { return String::EndsWith(*this, suffix); }

	template <typename CharType>
	inline bool StringView<CharType>::endsWith(const CharType* suffixCStr) const { return String::EndsWith(*this, suffixCStr); }

	template <typename CharType>
	inline StringView<CharType> StringView<CharType>::getSubString(uintptr startIndex, uintptr length) const
	{
		XAssert(startIndex <= this->length);
		if (length == uintptr(-1))
			length = this->length - startIndex;
		else
			XAssert(startIndex + length <= this->length);

		return StringView<CharType>(data + startIndex, length);
	}

	// InplaceString ///////////////////////////////////////////////////////////////////////////////

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline InplaceString<CharType, Capacity, CounterType>::VirtualStringRefVTable::VirtualStringRefVTable()
	{
		this->getData = [](void* stringPtr) -> CharType*
			{
				return ((InplaceString<CharType, Capacity, CounterType>*)stringPtr)->getData();
			};

		this->getLength = [](void* stringPtr) -> uintptr
			{
				return ((InplaceString<CharType, Capacity, CounterType>*)stringPtr)->getLength();
			};

		this->resize = [](void* stringPtr, uintptr length) -> void
			{
				XAssert(length == uintptr(CounterType(length)));
				((InplaceString<CharType, Capacity, CounterType>*)stringPtr)->resize(CounterType(length));
			};

		this->maxLength = GetMaxLength();
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline InplaceString<CharType, Capacity, CounterType>::InplaceString()
	{
		buffer[0] = CharType(0);
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline void InplaceString<CharType, Capacity, CounterType>::append(CharType c)
	{
		if (isFull())
			return;
		buffer[length] = c;
		length++;
		buffer[length] = CharType(0);
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline void InplaceString<CharType, Capacity, CounterType>::append(const StringView<CharType>& string)
	{
		const CounterType fullLength = length + CounterType(string.getLength());
		const bool overflow = fullLength > GetMaxLength();
		const CounterType numCharsToCopy = overflow ? GetMaxLength() - length : CounterType(string.getLength());
		memoryCopy(buffer + length, string.getData(), numCharsToCopy * sizeof(CharType));
		length += numCharsToCopy;
		buffer[length] = CharType(0);
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline void InplaceString<CharType, Capacity, CounterType>::append(const CharType* cstr)
	{
		const CharType* cstrIt = cstr;
		while (*cstrIt && length < GetMaxLength())
		{
			buffer[length] = *cstrIt;
			length++;
			cstrIt++;
		}
		buffer[length] = CharType(0);
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline void InplaceString<CharType, Capacity, CounterType>::resize(CounterType newLength, CharType c)
	{
		XAssert(newLength <= GetMaxLength());
		CharType* newEnd = buffer + newLength;
		for (CharType* i = buffer + length; i < newEnd; i++)
			*i = c;
		length = newLength;
		buffer[length] = CharType(0);
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline void InplaceString<CharType, Capacity, CounterType>::clear()
	{
		length = 0;
		buffer[0] = CharType(0);
	}

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline bool InplaceString<CharType, Capacity, CounterType>::
		operator == (const StringView<CharType>& that) const { return String::IsEqual(StringView<CharType>(*this), that); }

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline bool InplaceString<CharType, Capacity, CounterType>::
		operator == (const CharType* thatCStr) const { return String::IsEqual(StringView<CharType>(*this), thatCStr); }

	template <typename CharType, uintptr Capacity, typename CounterType>
	inline bool InplaceString<CharType, Capacity, CounterType>::
		startsWith(const CharType* prefixCStr) const { return String::StartsWith(StringView<CharType>(*this), prefixCStr); }


	// DynaicString ////////////////////////////////////////////////////////////////////////////////

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void DynamicString<CharType, CounterType, AllocatorType>::ensureCapacity(CounterType requiredLength)
	{
		const CounterType requiredCapacity = requiredLength + 1;
		if (requiredCapacity <= capacity)
			return;

		// TODO: Maybe align up to 16 or some adaptive pow of 2.
		capacity = max<CounterType>(requiredCapacity, capacity * 2, InitialBufferCapacity);
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
	inline DynamicString<CharType, CounterType, AllocatorType>::DynamicString(DynamicString&& that)
	{
		buffer = that.buffer;
		capacity = that.capacity;
		length = that.length;

		that.buffer = nullptr;
		that.capacity = 0;
		that.length = 0;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline auto DynamicString<CharType, CounterType, AllocatorType>::operator = (DynamicString&& that) -> DynamicString&
	{
		if (buffer)
			AllocatorBase::release(buffer);

		buffer = that.buffer;
		capacity = that.capacity;
		length = that.length;

		that.buffer = nullptr;
		that.capacity = 0;
		that.length = 0;

		return *this;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline auto DynamicString<CharType, CounterType, AllocatorType>::operator = (const CharType* thatCStr) -> DynamicString&
	{
		// TODO: Assert for `CounterType` overflow.
		const CounterType thatLength = CounterType(String::GetCStrLength(thatCStr));
		ensureCapacity(thatLength);
		memoryCopy(buffer, thatCStr, thatLength + 1);
		length = thatLength;
		return *this;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline bool DynamicString<CharType, CounterType, AllocatorType>::append(CharType c)
	{
		ensureCapacity(length + 1);
		buffer[length] = c;
		length++;
		buffer[length] = CharType(0);
		return true;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline bool DynamicString<CharType, CounterType, AllocatorType>::append(const StringView<CharType>& string)
	{
		// TODO: Assert for `CounterType` overflow.
		const CounterType stringToAppendLength = CounterType(string.getLength());
		ensureCapacity(length + stringToAppendLength);
		memoryCopy(buffer + length, string.getData(), stringToAppendLength);
		length += stringToAppendLength;
		buffer[length] = CharType(0);
		return true;
	}

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline void DynamicString<CharType, CounterType, AllocatorType>::resize(CounterType newLength, CharType c)
	{
		if (length < newLength)
		{
			ensureCapacity(newLength);
			CharType* newEnd = buffer + newLength;
			for (CharType* i = buffer + length; i < newEnd; i++)
				*i = c;
		}
		length = newLength;
		if (buffer)
			buffer[length] = CharType(0);
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

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline bool DynamicString<CharType, CounterType, AllocatorType>::
		operator == (const StringView<CharType>& that) const { return String::IsEqual(StringView<CharType>(*this), that); }

	template <typename CharType, typename CounterType, typename AllocatorType>
	inline bool DynamicString<CharType, CounterType, AllocatorType>::
		operator == (const CharType* thatCStr) const { return String::IsEqual(StringView<CharType>(*this), thatCStr); }


	// String //////////////////////////////////////////////////////////////////////////////////////

	template <typename CharType>
	inline constexpr uintptr String::GetCStrLength(const CharType* cstr)
	{
		if (!cstr)
			return 0;
		const CharType* it = cstr;
		while (*it)
			it++;
		return uintptr(it - cstr);
	}

	template <typename CharType>
	inline bool String::IsEqual(const StringView<CharType>& a, const StringView<CharType>& b)
	{
		if (a.getLength() != b.getLength())
			return false;
		for (uintptr i = 0; i < a.getLength(); i++)
		{
			if (a[i] != b[i])
				return false;
		}
		return true;
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

	template <typename CharType>
	inline bool String::IsLess(const StringView<CharType>& left, const StringView<CharType>& right)
	{
		const uintptr leftLength = left.getLength();
		const uintptr rightLength = right.getLength();

		for (uintptr i = 0; i < leftLength; i++)
		{
			if (i >= rightLength)
				return false;
			if (left[i] != right[i])
				return left[i] < right[i];
		}

		return leftLength < rightLength;
	}

	template <typename CharType>
	inline bool String::StartsWith(const StringView<CharType>& string, const StringView<CharType>& prefix)
	{
		if (prefix.getLength() > string.getLength())
			return false;
		for (uintptr i = 0; i < prefix.getLength(); i++)
		{
			if (string[i] != prefix[i])
				return false;
		}
		return true;
	}

	template <typename CharType>
	inline bool String::StartsWith(const StringView<CharType>& string, const CharType* prefixCStr)
	{
		for (uintptr i = 0; prefixCStr[i]; i++)
		{
			if (i >= string.getLength())
				return false;
			if (string[i] != prefixCStr[i])
				return false;
		}
		return true;
	}

	template <typename CharType>
	inline bool String::StartsWith(const CharType* cstr, const StringView<CharType>& prefix)
	{
		for (uintptr i = 0; i < prefix.getLength(); i++)
		{
			if (cstr[i] != prefix[i]) // Also handles terminator.
				return false;
		}
		return true;
	}

	template <typename CharType>
	inline bool String::StartsWith(const CharType* cstr, const CharType* prefixCStr)
	{
		for (uintptr i = 0; prefixCStr[i]; i++)
		{
			if (cstr[i] != prefixCStr[i]) // Also handles terminator.
				return false;
		}
		return true;
	}

	template <typename CharType>
	static inline bool String::EndsWith(const StringView<CharType>& string, const StringView<CharType>& suffix)
	{
		if (suffix.getLength() > string.getLength())
			return false;

		const CharType* stringRevI = string.getData() + string.getLength() - 1;
		const CharType* suffixRevI = suffix.getData() + suffix.getLength() - 1;
		for (uintptr i = 0; i < suffix.getLength(); i++)
		{
			if (*stringRevI != *suffixRevI)
				return false;
			stringRevI--;
			suffixRevI--;
		}

		return true;
	}

	template <typename CharType>
	static inline bool String::EndsWith(const StringView<CharType>& string, const CharType* suffixCStr)
	{
		return EndsWith(string, StringView<CharType>(suffixCStr, GetCStrLength(suffixCStr)));
	}
}

template <typename TextWriter, typename CharType>
inline bool XLib::TextWriteFmt_HandleArg(TextWriter& writer, const StringView<CharType>& string)
{
	for (CharType c : string)
	{
		writer.append(c);
		//if (!writer.append(c))
		//	return false;
	}
	// TODO: replace with `writer.writeChars()`.
	return true;
}
