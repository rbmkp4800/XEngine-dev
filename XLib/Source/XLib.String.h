#pragma once

#include "XLib.h"
#include "XLib.Allocation.h"

// TODO: Decide what to do with `String::getCStr()` for empty string.
// TODO: StringView -> StringViewBase, DynamicString -> DynamicStringBase, InplaceString -> InplaceStringBase ?
// TODO: Do something about million template methods compiled for `InplaceString` instances.
// TODO: Profile cost of always supporting zero terminator. Probably we may put it only when calling `getCStr`/`getData` methods.
// TODO: String container types should be in XLib.Containers.*** but we also have non-container utils here... Also StringView is certainly not a container. Decide what to do.
//		We may split this file into XLib.Strings.h and XLib.Containers.String.h
// TODO: For DynamicString move capacity and length counters to heap buffer. DynamicString object should contain just a single pointer.

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

	// NOTE: Buffer size includes null terminator (so max length is equal to buffer size minus 1).
	// NOTE: `VirtualStringInterface::setLength()` does not reallocate buffer, so sufficient buffer space should be allocated prior to calling.

	template <typename CharType>
	class VirtualStringInterface abstract
	{
	public:
		virtual uint32 getMaxBufferSize() const = 0;
		virtual uint32 getBufferSize() const = 0;
		virtual void growBuffer(uint32 minRequiredBufferSize) const = 0;
		virtual char* getBuffer() const = 0;

		virtual uint32 getLength() const = 0;
		virtual void setLength(uint32 length) const = 0;

		inline uint32 getMaxLength() const;
		inline void growBufferToFitLength(uint32 minRequiredLength) const;
	};

	template <typename CharType>
	class VirtualStringRef
	{
	private:
		void* vtable;
		void* stringPtr = nullptr;

	public:
		VirtualStringRef() = default;
		~VirtualStringRef() = default;

		inline uint32 getMaxBufferSize() const { return ((VirtualStringInterface<CharType>*)this)->getMaxBufferSize(); }
		inline uint32 getBufferSize() const { return ((VirtualStringInterface<CharType>*)this)->getBufferSize(); }
		inline void growBuffer(uint32 minRequiredBufferSize) const { ((VirtualStringInterface<CharType>*)this)->growBuffer(minRequiredBufferSize); }
		inline CharType* getBuffer() const { return ((VirtualStringInterface<CharType>*)this)->getBuffer(); }

		inline uint32 getLength() const { return ((VirtualStringInterface<CharType>*)this)->getLength(); }
		inline void setLength(uint32 length) const { ((VirtualStringInterface<CharType>*)this)->setLength(length); }

		inline uint32 getMaxLength() const { return ((VirtualStringInterface<CharType>*)this)->getMaxLength(); }
		inline void growBufferToFitLength(uint32 minRequiredLength) const { ((VirtualStringInterface<CharType>*)this)->growBufferToFitLength(minRequiredLength); }
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

	template <typename CharType, uint16 BufferSize>
	class InplaceString
	{
		static_assert(BufferSize > 1); // At least one character (+1 for cstr zero terminator).

	private:
		class VirtualRef;

	private:
		uint16 length = 0;
		CharType buffer[BufferSize];

	public:
		inline InplaceString();
		inline InplaceString(const StringView<CharType>& string)	{ append(string); }
		inline InplaceString(const CharType* cstr)					{ append(cstr); }
		inline InplaceString(const CharType* data, uintptr length)	{ append(data, length); }

		~InplaceString() = default;

		inline InplaceString& operator = (const StringView<CharType>& that)	{ length = 0; append(that);		return *this; }
		inline InplaceString& operator = (const CharType* thatCStr)			{ length = 0; append(thatCStr);	return *this; }

		inline void append(CharType c);
		inline bool append(CharType c, uint32 count);
		inline void append(const StringView<CharType>& string);
		inline void append(const CharType* cstr);
		inline void append(const CharType* data, uintptr length) { return append(StringView<CharType>(data, length)); }

		inline void clear();
		inline void truncate(uint32 newTruncatedLength);

		inline void setLength(uint32 length);

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
		inline bool isFull() const { return length + 1 == BufferSize; }

	public:
		static constexpr uint16 GetBufferSize() { return BufferSize; }
		static constexpr uint16 GetMaxLength() { return BufferSize - 1; }
	};


	template <typename CharType, typename AllocatorType = SystemHeapAllocator>
	class DynamicString : private AllocatorAdapterBase<AllocatorType>
	{
	private:
		using AllocatorBase = AllocatorAdapterBase<AllocatorType>;

		class VirtualRef;

		static constexpr uint32 InitialBufferSize = 16;
		static constexpr uint32 BufferExponentialGrowthFactor = 2;

	private:
		CharType* buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 length = 0;

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
		inline bool append(CharType c, uint32 count);
		inline bool append(const StringView<CharType>& string);
		inline bool append(const CharType* cstr) { return append(StringView<CharType>(cstr)); }
		inline bool append(const CharType* data, uintptr length) { return append(StringView<CharType>(data, length)); }

		inline void clear();
		inline void truncate(uint32 newTruncatedLength);

		void growBuffer(uint32 minRequiredBufferSize);
		void growBufferExponentially(uint32 minRequiredBufferSize);
		void shrinkBuffer();

		inline void growBufferToFitLength(uint32 minRequiredLength) { growBuffer(minRequiredLength + 1); }
		inline void growBufferExponentiallyToFitLength(uint32 minRequiredLength) { growBufferExponentiallyToFitLength(minRequiredLength + 1); }

		inline uint32 getBufferSize() const { return bufferSize; }

		inline void setLength(uint32 length);

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

	template <uint16 BufferSize>
	using InplaceStringASCII = InplaceString<charASCII, BufferSize>;

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


////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename CharType>
inline uint32 XLib::VirtualStringInterface<CharType>::getMaxLength() const
{ 
	const uint32 maxBufferSize = getMaxBufferSize();
	XAssert(maxBufferSize > 0);
	return maxBufferSize - 1;
}

template <typename CharType>
inline void XLib::VirtualStringInterface<CharType>::growBufferToFitLength(uint32 minRequiredLength) const
{
	growBuffer(minRequiredLength + 1);
}


// InplaceString ///////////////////////////////////////////////////////////////////////////////////

template <typename CharType, uint16 BufferSize>
class XLib::InplaceString<CharType, BufferSize>::VirtualRef : public XLib::VirtualStringInterface<CharType>
{
private:
	InplaceString<CharType, BufferSize>* stringPtr;

public:
	VirtualRef(InplaceString<CharType, BufferSize>* stringPtr) : stringPtr(stringPtr) {}

	virtual uint32 getMaxBufferSize() const override { return BufferSize; }
	virtual uint32 getBufferSize() const override { return BufferSize; }
	virtual void growBuffer(uint32 minRequiredBufferSize) const override { XAssert(minRequiredBufferSize <= BufferSize); }
	virtual CharType* getBuffer() const { return stringPtr->buffer; }

	virtual uint32 getLength() const override { return stringPtr->length; }
	virtual void setLength(uint32 length) const { return stringPtr->setLength(length); }
};

template <typename CharType, uint16 BufferSize>
inline XLib::InplaceString<CharType, BufferSize>::InplaceString()
{
	buffer[0] = CharType(0);
}

template <typename CharType, uint16 BufferSize>
inline void XLib::InplaceString<CharType, BufferSize>::append(CharType c)
{
	if (isFull())
		return;
	buffer[length] = c;
	length++;
	buffer[length] = CharType(0);
}

template <typename CharType, uint16 BufferSize>
inline void XLib::InplaceString<CharType, BufferSize>::append(const StringView<CharType>& string)
{
	const uintptr fullLength = length + string.getLength();
	const bool overflow = fullLength > uintptr(GetMaxLength());
	const uint16 numCharsToCopy = overflow ? GetMaxLength() - length : uint16(string.getLength());
	memoryCopy(buffer + length, string.getData(), numCharsToCopy * sizeof(CharType));
	length += numCharsToCopy;
	buffer[length] = CharType(0);
}

template <typename CharType, uint16 BufferSize>
inline void XLib::InplaceString<CharType, BufferSize>::append(const CharType* cstr)
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

template <typename CharType, uint16 BufferSize>
inline void XLib::InplaceString<CharType, BufferSize>::clear()
{
	length = 0;
	buffer[0] = CharType(0);
}

template <typename CharType, uint16 BufferSize>
inline void XLib::InplaceString<CharType, BufferSize>::setLength(uint32 length)
{
	const uint32 requiredBufferSize = length + 1;
	XAssert(requiredBufferSize <= BufferSize);
	this->length = uint16(length);
	buffer[length] = 0;
}

template <typename CharType, uint16 BufferSize>
inline XLib::VirtualStringRef<CharType> XLib::InplaceString<CharType, BufferSize>::getVirtualRef()
{
	VirtualRef virtualRef(this);
	static_assert(sizeof(VirtualRef) == sizeof(XLib::VirtualStringRef<CharType>));
	return (XLib::VirtualStringRef<CharType>&)virtualRef;
}

template <typename CharType, uint16 BufferSize>
inline bool XLib::InplaceString<CharType, BufferSize>::operator == (const StringView<CharType>& that) const
{
	return String::IsEqual(StringView<CharType>(*this), that);
}

template <typename CharType, uint16 BufferSize>
inline bool XLib::InplaceString<CharType, BufferSize>::operator == (const CharType* thatCStr) const
{
	return String::IsEqual(StringView<CharType>(*this), thatCStr);
}

template <typename CharType, uint16 BufferSize>
inline bool XLib::InplaceString<CharType, BufferSize>::startsWith(const CharType* prefixCStr) const
{
	return String::StartsWith(StringView<CharType>(*this), prefixCStr);
}


// DynamicString ///////////////////////////////////////////////////////////////////////////////////

template <typename CharType, typename AllocatorType>
class XLib::DynamicString<CharType, AllocatorType>::VirtualRef : public XLib::VirtualStringInterface<CharType>
{
private:
	DynamicString<CharType, AllocatorType>* stringPtr;

public:
	VirtualRef(DynamicString<CharType, AllocatorType>* stringPtr) : stringPtr(stringPtr) {}

	virtual uint32 getMaxBufferSize() const override { return uint32(-1); }
	virtual uint32 getBufferSize() const override { return stringPtr->bufferSize; }
	virtual void growBuffer(uint32 minRequiredBufferSize) const override { stringPtr->growBuffer(minRequiredBufferSize); }
	virtual CharType* getBuffer() const { return stringPtr->buffer; }

	virtual uint32 getLength() const override { return stringPtr->length; }
	virtual void setLength(uint32 length) const { return stringPtr->setLength(length); }
};

template <typename CharType, typename AllocatorType>
inline XLib::DynamicString<CharType, AllocatorType>::~DynamicString()
{
	if (buffer)
		AllocatorBase::release(buffer);
	buffer = nullptr;
	bufferSize = 0;
	length = 0;
}

template <typename CharType, typename AllocatorType>
inline XLib::DynamicString<CharType, AllocatorType>::DynamicString(DynamicString&& that)
{
	buffer = that.buffer;
	bufferSize = that.bufferSize;
	length = that.length;

	that.buffer = nullptr;
	that.bufferSize = 0;
	that.length = 0;
}

template <typename CharType, typename AllocatorType>
inline auto XLib::DynamicString<CharType, AllocatorType>::operator = (DynamicString&& that) -> DynamicString&
{
	if (buffer)
		AllocatorBase::release(buffer);

	buffer = that.buffer;
	bufferSize = that.bufferSize;
	length = that.length;

	that.buffer = nullptr;
	that.bufferSize = 0;
	that.length = 0;

	return *this;
}

template <typename CharType, typename AllocatorType>
inline auto XLib::DynamicString<CharType, AllocatorType>::operator = (const CharType* thatCStr) -> DynamicString&
{
	const uint32 thatLength = XCheckedCastU32(String::GetCStrLength(thatCStr));

	const uint32 requiredBufferSize = thatLength + 1;
	if (bufferSize < requiredBufferSize)
		growBufferExponentially(requiredBufferSize);

	memoryCopy(buffer, thatCStr, thatLength + 1);
	length = thatLength;
	return *this;
}

template <typename CharType, typename AllocatorType>
inline bool XLib::DynamicString<CharType, AllocatorType>::append(CharType c)
{
	const uint32 requiredBufferSize = length + 2;
	if (bufferSize < requiredBufferSize)
		growBufferExponentially(requiredBufferSize);

	buffer[length] = c;
	length++;
	buffer[length] = CharType(0);
	return true;
}

template <typename CharType, typename AllocatorType>
inline bool XLib::DynamicString<CharType, AllocatorType>::append(const StringView<CharType>& string)
{
	const uint32 stringToAppendLength = XCheckedCastU32(string.getLength());

	const uint32 requiredBufferSize = length + stringToAppendLength + 1;
	if (bufferSize < requiredBufferSize)
		growBufferExponentially(requiredBufferSize);

	memoryCopy(buffer + length, string.getData(), stringToAppendLength);
	length += stringToAppendLength;
	buffer[length] = CharType(0);
	return true;
}

template <typename CharType, typename AllocatorType>
inline void XLib::DynamicString<CharType, AllocatorType>::clear()
{
	if (buffer)
		buffer[0] = CharType(0);
	length = 0;
}

template <typename CharType, typename AllocatorType>
inline void XLib::DynamicString<CharType, AllocatorType>::truncate(uint32 newTruncatedLength)
{
	XAssert(length >= newTruncatedLength);
	length = newTruncatedLength;
	if (buffer)
		buffer[length] = 0;
}

template <typename CharType, typename AllocatorType>
void XLib::DynamicString<CharType, AllocatorType>::growBuffer(uint32 minRequiredBufferSize)
{
	if (minRequiredBufferSize <= bufferSize)
		return;

	// TODO: Maybe align up to 16 or some adaptive pow of 2.
	bufferSize = minRequiredBufferSize;
	buffer = (CharType*)AllocatorBase::reallocate(buffer, bufferSize * sizeof(CharType));
}

template <typename CharType, typename AllocatorType>
void XLib::DynamicString<CharType, AllocatorType>::growBufferExponentially(uint32 minRequiredBufferSize)
{
	if (minRequiredBufferSize <= bufferSize)
		return;

	// TODO: Maybe align up to 16 or some adaptive pow of 2.
	bufferSize = max<uint32>(minRequiredBufferSize, bufferSize * BufferExponentialGrowthFactor, InitialBufferSize);
	buffer = (CharType*)AllocatorBase::reallocate(buffer, bufferSize * sizeof(CharType));
}

template <typename CharType, typename AllocatorType>
void XLib::DynamicString<CharType, AllocatorType>::shrinkBuffer()
{
	// TODO: Revisit this.

	if (!length && buffer)
	{
		AllocatorBase::release(buffer);
		buffer = nullptr;
		bufferSize = 0;
		return;
	}

	if (bufferSize > length + 1)
	{
		bufferSize = length + 1;
		buffer = (CharType*)AllocatorBase::reallocate(buffer, bufferSize * sizeof(CharType));
	}
}

template <typename CharType, typename AllocatorType>
inline void XLib::DynamicString<CharType, AllocatorType>::setLength(uint32 length)
{
	const uint32 requiredBufferSize = length + 1;
	XAssert(requiredBufferSize <= bufferSize);
	this->length = length;
	if (buffer)
		buffer[length] = 0;
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
