#pragma once

#include "XLib.h"
#include "XLib.Allocation.h"

// TODO: Decide what to do with `String::getCStr()` for empty string.
// TODO: StringView -> StringViewBase, DynamicString -> DynamicStringBase, InplaceString -> InplaceStringBase ?
// TODO: Do something about million template methods compiled for `InplaceString` instances.
// TODO: Profile cost of always supporting zero terminator. Probably we may put it only when calling `getCStr`/`getData` methods.
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

	public:
		static inline constexpr StringView<CharType> FromCStr(const CharType* cstr);
	};


	////////////////////////////////////////////////////////////////////////////////////////////////

	class VirtualStringInterface abstract
	{
	public:
		virtual uint32 getMaxBufferCapacity() const = 0;
		virtual uint32 getBufferCapacity() const = 0;
		virtual void reserveBufferCapacity(uint32 capacity) const = 0;

		virtual uint32 getLength() const = 0;
		virtual void setLength(uint32 length) const = 0;

		virtual char* getBuffer() const = 0;
	};

	template <typename CharType>
	class VirtualStringRef final : public VirtualStringInterface
	{
	private:
		void* stringPtr = nullptr;

	public:
		VirtualStringRef() = default;
		~VirtualStringRef() = default;

		virtual uint32 getMaxBufferCapacity() const override { *(int*)0 = 0; return 0; }
		virtual uint32 getBufferCapacity() const override { *(int*)0 = 0; return 0; }
		virtual void reserveBufferCapacity(uint32 capacity) const override { *(int*)0 = 0; }

		virtual uint32 getLength() const override { *(int*)0 = 0; return 0; }
		virtual void setLength(uint32 length) const override { *(int*)0 = 0; }

		virtual CharType* getBuffer() const override { *(int*)0 = 0; return nullptr; }
	};

	template <typename CharType>
	class VirtualStringFacade : public XLib::NonCopyable
	{
	private:

	public:
		VirtualStringFacade() = default;
		inline ~VirtualStringFacade() { sync(); }

		inline void sync();

		inline bool append(CharType c);
		inline bool append(const StringView<CharType>& string);
		inline bool append(const CharType* cstr) { return append(StringView<CharType>(cstr)); }
		inline bool append(const CharType* data, uintptr length) { return append(StringView<CharType>(data, length)); }

		inline void clear();
	};


	////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename CharType, uint16 Capacity>
	class InplaceString
	{
		static_assert(Capacity > 1); // At least one character (+1 for cstr zero terminator).

	private:
		class VirtualRef;

	private:
		uint16 length = 0;
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

		inline void resize(uint16 newLength, CharType c = CharType(0));
		inline void clear();

		inline CharType* getData() { return buffer; }
		inline uint16 getLength() const { return length; }
		inline const CharType* getCStr() const { return buffer; }

		inline StringView<CharType> getView() const { return StringView<CharType>(buffer, length); }
		inline operator StringView<CharType>() const { return getView(); }

		inline VirtualStringRef<CharType> getVirtualRef();
		inline operator VirtualStringRef<CharType>() { return getVirtualRef(); }

		inline CharType& operator [] (uint16 index) { return buffer[index]; }
		inline CharType operator [] (uint16 index) const { return buffer[index]; }

		inline bool operator == (const StringView<CharType>& that) const;
		inline bool operator == (const CharType* thatCStr) const;

		inline bool startsWith(const StringView<CharType>& prefix) const;
		inline bool startsWith(const CharType* prefixCStr) const;
		inline bool endsWith(const StringView<CharType>& suffix) const;
		inline bool endsWith(const CharType* suffixCStr) const;

		inline bool isEmpty() const { return length == 0; }
		inline bool isFull() const { return length + 1 == Capacity; }

	public:
		static constexpr uint16 GetCapacity() { return Capacity; }
		static constexpr uint16 GetMaxLength() { return Capacity - 1; }
	};


	template <typename CharType, typename AllocatorType = SystemHeapAllocator>
	class DynamicString : private AllocatorAdapterBase<AllocatorType>
	{
	private:
		using AllocatorBase = AllocatorAdapterBase<AllocatorType>;

		class VirtualRef;

		static constexpr uint32 InitialBufferCapacity = 16;

	private:
		CharType* buffer = nullptr;
		uint32 capacity = 0;
		uint32 length = 0;

	private:
		inline void ensureCapacity(uint32 requiredLength);

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

		inline void resize(uint32 newLength, CharType c = CharType(0));
		inline void clear();

		inline void reserve(uint32 newMaxLength) { ensureCapacity(newMaxLength); }
		inline void compact();

		inline CharType* getData() { return buffer; }
		inline uint32 getLength() const { return length; }
		inline const CharType* getCStr() const { buffer[length] = 0; return buffer; }

		inline StringView<CharType> getView() const { return StringView<CharType>(buffer, length); }
		inline operator StringView<CharType>() const { return getView(); }

		inline VirtualStringRef<CharType> getVirtualRef();
		inline operator VirtualStringRef<CharType>() { return getVirtualRef(); }

		inline CharType& operator [] (uint32 index) { return buffer[index]; }
		inline CharType operator [] (uint32 index) const { return buffer[index]; }

		inline bool operator == (const StringView<CharType>& that) const;
		inline bool operator == (const CharType* thatCStr) const;

		inline bool startsWith(const StringView<CharType>& prefix) const;
		inline bool startsWith(const CharType* prefixCStr) const;
		inline bool endsWith(const StringView<CharType>& suffix) const;
		inline bool endsWith(const CharType* suffixCStr) const;

		inline bool isEmpty() const { return length == 0; }
	};


	////////////////////////////////////////////////////////////////////////////////////////////////

	using StringViewASCII = StringView<charASCII>;

	using VirtualStringRefASCII = VirtualStringRef<charASCII>;
	using VirtualStringFacadeASCII = VirtualStringFacade<charASCII>;

	template <uint16 Capacity>
	using InplaceStringASCII = InplaceString<charASCII, Capacity>;

	using InplaceStringASCIIx32		= InplaceString<charASCII, 30>;		static_assert(sizeof(InplaceStringASCIIx32) == 32);
	using InplaceStringASCIIx64		= InplaceString<charASCII, 62>;		static_assert(sizeof(InplaceStringASCIIx64) == 64);
	using InplaceStringASCIIx128	= InplaceString<charASCII, 126>;	static_assert(sizeof(InplaceStringASCIIx128) == 128);
	using InplaceStringASCIIx256	= InplaceString<charASCII, 254>;	static_assert(sizeof(InplaceStringASCIIx256) == 256);
	using InplaceStringASCIIx512	= InplaceString<charASCII, 510>;	static_assert(sizeof(InplaceStringASCIIx512) == 512);
	using InplaceStringASCIIx1024	= InplaceString<charASCII, 1022>;	static_assert(sizeof(InplaceStringASCIIx1024) == 1024);
	using InplaceStringASCIIx2048	= InplaceString<charASCII, 2046>;	static_assert(sizeof(InplaceStringASCIIx2048) == 2048);
	using InplaceStringASCIIx4096	= InplaceString<charASCII, 4094>;	static_assert(sizeof(InplaceStringASCIIx4096) == 4096);

	using DynamicStringASCII = DynamicString<charASCII>;


	////////////////////////////////////////////////////////////////////////////////////////////////

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
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINITION //////////////////////////////////////////////////////////////////////////////////////

// StringView //////////////////////////////////////////////////////////////////////////////////////

template <typename CharType>
inline bool XLib::StringView<CharType>::operator == (const StringView<CharType>& that) const { return String::IsEqual(*this, that); }

template <typename CharType>
inline bool XLib::StringView<CharType>::operator == (const CharType* thatCStr) const { return String::IsEqual(*this, thatCStr); }

template <typename CharType>
inline bool XLib::StringView<CharType>::startsWith(const StringView<CharType>& prefix) const { return String::StartsWith(*this, prefix); }

template <typename CharType>
inline bool XLib::StringView<CharType>::startsWith(const CharType* prefixCStr) const { return String::StartsWith(*this, prefixCStr); }

template <typename CharType>
inline bool XLib::StringView<CharType>::endsWith(const StringView<CharType>& suffix) const { return String::EndsWith(*this, suffix); }

template <typename CharType>
inline bool XLib::StringView<CharType>::endsWith(const CharType* suffixCStr) const { return String::EndsWith(*this, suffixCStr); }

template <typename CharType>
inline XLib::StringView<CharType> XLib::StringView<CharType>::getSubString(uintptr startIndex, uintptr length) const
{
	XAssert(startIndex <= this->length);
	if (length == uintptr(-1))
		length = this->length - startIndex;
	else
		XAssert(startIndex + length <= this->length);

	return StringView<CharType>(data + startIndex, length);
}

template <typename CharType>
inline constexpr XLib::StringView<CharType> XLib::StringView<CharType>::FromCStr(const CharType* cstr)
{
	return StringView<CharType>(cstr, String::GetCStrLength(cstr));
}


// InplaceString ///////////////////////////////////////////////////////////////////////////////////

template <typename CharType, uint16 Capacity>
class XLib::InplaceString<CharType, Capacity>::VirtualRef : public XLib::VirtualStringInterface
{
private:
	InplaceString<CharType, Capacity>* stringPtr;

public:
	VirtualRef(InplaceString<CharType, Capacity>* stringPtr) : stringPtr(stringPtr) {}

	virtual uint32 getMaxBufferCapacity() const override { return Capacity; }
	virtual uint32 getBufferCapacity() const override { return Capacity; }
	virtual void reserveBufferCapacity(uint32 capacity) const override { XAssert(capacity <= Capacity); } // TODO: Revisit this.

	virtual uint32 getLength() const override { return stringPtr->length; }
	virtual void setLength(uint32 length) const { XAssert(length <= uint32(Capacity)); return stringPtr->resize(uint16(length)); } // TODO: Revisit this.

	virtual CharType* getBuffer() const { return stringPtr->buffer; }
};

template <typename CharType, uint16 Capacity>
inline XLib::InplaceString<CharType, Capacity>::InplaceString()
{
	buffer[0] = CharType(0);
}

template <typename CharType, uint16 Capacity>
inline void XLib::InplaceString<CharType, Capacity>::append(CharType c)
{
	if (isFull())
		return;
	buffer[length] = c;
	length++;
	buffer[length] = CharType(0);
}

template <typename CharType, uint16 Capacity>
inline void XLib::InplaceString<CharType, Capacity>::append(const StringView<CharType>& string)
{
	const uintptr fullLength = length + string.getLength();
	const bool overflow = fullLength > uintptr(GetMaxLength());
	const uint16 numCharsToCopy = overflow ? GetMaxLength() - length : uint16(string.getLength());
	memoryCopy(buffer + length, string.getData(), numCharsToCopy * sizeof(CharType));
	length += numCharsToCopy;
	buffer[length] = CharType(0);
}

template <typename CharType, uint16 Capacity>
inline void XLib::InplaceString<CharType, Capacity>::append(const CharType* cstr)
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

template <typename CharType, uint16 Capacity>
inline void XLib::InplaceString<CharType, Capacity>::resize(uint16 newLength, CharType c)
{
	XAssert(newLength <= GetMaxLength());
	CharType* newEnd = buffer + newLength;
	for (CharType* i = buffer + length; i < newEnd; i++)
		*i = c;
	length = newLength;
	buffer[length] = CharType(0);
}

template <typename CharType, uint16 Capacity>
inline void XLib::InplaceString<CharType, Capacity>::clear()
{
	length = 0;
	buffer[0] = CharType(0);
}

template <typename CharType, uint16 Capacity>
inline XLib::VirtualStringRef<CharType> XLib::InplaceString<CharType, Capacity>::getVirtualRef()
{
	VirtualRef virtualRef(this);
	static_assert(sizeof(VirtualRef) == sizeof(XLib::VirtualStringRef<CharType>));
	return (XLib::VirtualStringRef<CharType>&)virtualRef;
}

template <typename CharType, uint16 Capacity>
inline bool XLib::InplaceString<CharType, Capacity>::operator == (const StringView<CharType>& that) const
{
	return String::IsEqual(StringView<CharType>(*this), that);
}

template <typename CharType, uint16 Capacity>
inline bool XLib::InplaceString<CharType, Capacity>::operator == (const CharType* thatCStr) const
{
	return String::IsEqual(StringView<CharType>(*this), thatCStr);
}

template <typename CharType, uint16 Capacity>
inline bool XLib::InplaceString<CharType, Capacity>::startsWith(const CharType* prefixCStr) const
{
	return String::StartsWith(StringView<CharType>(*this), prefixCStr);
}


// DynamicString ///////////////////////////////////////////////////////////////////////////////////

template <typename CharType, typename AllocatorType>
class XLib::DynamicString<CharType, AllocatorType>::VirtualRef : public XLib::VirtualStringInterface
{
private:
	DynamicString<CharType, AllocatorType>* stringPtr;

public:
	VirtualRef(DynamicString<CharType, AllocatorType>* stringPtr) : stringPtr(stringPtr) {}

	virtual uint32 getMaxBufferCapacity() const override { return uint32(-1); }
	virtual uint32 getBufferCapacity() const override { return stringPtr->capacity; }

	// TODO: Revisit this. `reserve` accepts not new buffer capacity but new max length.
	virtual void reserveBufferCapacity(uint32 capacity) const override { stringPtr->reserve(capacity); }

	virtual uint32 getLength() const override { return stringPtr->length; }
	virtual void setLength(uint32 length) const { return stringPtr->resize(length); } // TODO: Revisit this.

	virtual CharType* getBuffer() const { return stringPtr->buffer; }
};

template <typename CharType, typename AllocatorType>
inline void XLib::DynamicString<CharType, AllocatorType>::ensureCapacity(uint32 requiredLength)
{
	const uint32 requiredCapacity = requiredLength + 1;
	if (requiredCapacity <= capacity)
		return;

	// TODO: Maybe align up to 16 or some adaptive pow of 2.
	capacity = max<uint32>(requiredCapacity, capacity * 2, InitialBufferCapacity);
	buffer = (CharType*)AllocatorBase::reallocate(buffer, capacity * sizeof(CharType));
}

template <typename CharType, typename AllocatorType>
inline XLib::DynamicString<CharType, AllocatorType>::~DynamicString()
{
	if (buffer)
		AllocatorBase::release(buffer);
	buffer = nullptr;
	capacity = 0;
	length = 0;
}

template <typename CharType, typename AllocatorType>
inline XLib::DynamicString<CharType, AllocatorType>::DynamicString(DynamicString&& that)
{
	buffer = that.buffer;
	capacity = that.capacity;
	length = that.length;

	that.buffer = nullptr;
	that.capacity = 0;
	that.length = 0;
}

template <typename CharType, typename AllocatorType>
inline auto XLib::DynamicString<CharType, AllocatorType>::operator = (DynamicString&& that) -> DynamicString&
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

template <typename CharType, typename AllocatorType>
inline auto XLib::DynamicString<CharType, AllocatorType>::operator = (const CharType* thatCStr) -> DynamicString&
{
	const uint32 thatLength = XCheckedCastU32(String::GetCStrLength(thatCStr));
	ensureCapacity(thatLength);
	memoryCopy(buffer, thatCStr, thatLength + 1);
	length = thatLength;
	return *this;
}

template <typename CharType, typename AllocatorType>
inline bool XLib::DynamicString<CharType, AllocatorType>::append(CharType c)
{
	ensureCapacity(length + 1);
	buffer[length] = c;
	length++;
	buffer[length] = CharType(0);
	return true;
}

template <typename CharType, typename AllocatorType>
inline bool XLib::DynamicString<CharType, AllocatorType>::append(const StringView<CharType>& string)
{
	const uint32 stringToAppendLength = XCheckedCastU32(string.getLength());
	ensureCapacity(length + stringToAppendLength);
	memoryCopy(buffer + length, string.getData(), stringToAppendLength);
	length += stringToAppendLength;
	buffer[length] = CharType(0);
	return true;
}

template <typename CharType, typename AllocatorType>
inline void XLib::DynamicString<CharType, AllocatorType>::resize(uint32 newLength, CharType c)
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

template <typename CharType, typename AllocatorType>
inline void XLib::DynamicString<CharType, AllocatorType>::clear()
{
	if (buffer)
		buffer[0] = CharType(0);
	length = 0;
}

template <typename CharType, typename AllocatorType>
inline void XLib::DynamicString<CharType, AllocatorType>::compact()
{
	if (capacity > length + 1)
	{
		capacity = length + 1;
		buffer = (CharType*)AllocatorBase::reallocate(buffer, capacity * sizeof(CharType));
	}

	// TODO: Handle case for length == 0
}

template <typename CharType, typename AllocatorType>
inline bool XLib::DynamicString<CharType, AllocatorType>::operator == (const StringView<CharType>& that) const
{
	return String::IsEqual(StringView<CharType>(*this), that);
}

template <typename CharType, typename AllocatorType>
inline bool XLib::DynamicString<CharType, AllocatorType>::operator == (const CharType* thatCStr) const
{
	return String::IsEqual(StringView<CharType>(*this), thatCStr);
}


// String //////////////////////////////////////////////////////////////////////////////////////////

template <typename CharType>
inline constexpr uintptr XLib::String::GetCStrLength(const CharType* cstr)
{
	if (!cstr)
		return 0;
	const CharType* it = cstr;
	while (*it)
		it++;
	return uintptr(it - cstr);
}

template <typename CharType>
inline bool XLib::String::IsEqual(const StringView<CharType>& a, const StringView<CharType>& b)
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
inline bool XLib::String::IsEqual(const StringView<CharType>& a, const CharType* bCStr)
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
inline ordering XLib::String::CompareOrdered(const StringView<CharType>& left, const StringView<CharType>& right)
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
inline bool XLib::String::IsLess(const StringView<CharType>& left, const StringView<CharType>& right)
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
inline bool XLib::String::StartsWith(const StringView<CharType>& string, const StringView<CharType>& prefix)
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
inline bool XLib::String::StartsWith(const StringView<CharType>& string, const CharType* prefixCStr)
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
inline bool XLib::String::StartsWith(const CharType* cstr, const StringView<CharType>& prefix)
{
	for (uintptr i = 0; i < prefix.getLength(); i++)
	{
		if (cstr[i] != prefix[i]) // Also handles terminator.
			return false;
	}
	return true;
}

template <typename CharType>
inline bool XLib::String::StartsWith(const CharType* cstr, const CharType* prefixCStr)
{
	for (uintptr i = 0; prefixCStr[i]; i++)
	{
		if (cstr[i] != prefixCStr[i]) // Also handles terminator.
			return false;
	}
	return true;
}

template <typename CharType>
static inline bool XLib::String::EndsWith(const StringView<CharType>& string, const StringView<CharType>& suffix)
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
static inline bool XLib::String::EndsWith(const StringView<CharType>& string, const CharType* suffixCStr)
{
	return EndsWith(string, StringView<CharType>(suffixCStr, GetCStrLength(suffixCStr)));
}
