#pragma once

#include "XLib.h"

namespace XLib
{
	class ByteStreamWriter
	{
	private:
		byte* buffer;
		uintptr bufferSize;
		uintptr currentOffset = 0;
		bool overflowed = false;

	public:
		inline ByteStreamWriter(void* buffer, uintptr bufferSize) : buffer((byte*)buffer), bufferSize(bufferSize) {}
		~ByteStreamWriter() = default;

		inline bool alignUp(uintptr alignment);
		inline bool write(const void* data, uintptr size);
		inline bool writeAligned(const void* data, uintptr size, uintptr alignment);
		inline void* advance(uintptr size);
		inline void* advanceAligned(uintptr size, uintptr alignment);

		template <typename T>
		inline void write(const T& value) { write(&value, sizeof(value)); }
		template <typename T>
		inline void writeAligned(const T& value, uintptr alignment) { writeAligned(&value, sizeof(value), alignment); }
		template <typename T>
		inline T* advance() { return (T*)advance(sizeof(T)); }
		template <typename T>
		inline T* advanceAligned(uintptr alignment) { return (T*)advanceAligned(sizeof(T), alignment); }

		inline uintptr getLength() const { return currentOffset; }
		inline void* getData() const { return buffer; }
		inline bool isOverflowed() const { return overflowed; }
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	inline bool ByteStreamWriter::alignUp(uintptr alignment)
	{
		const uintptr alignedOffset = ::alignUp(currentOffset, alignment);
		const bool overflow = alignedOffset > bufferSize;
		const uintptr newOffset = overflow ? bufferSize : alignedOffset;
		memorySet(buffer + currentOffset, 0, newOffset - currentOffset);
		currentOffset = newOffset;
		overflowed |= overflow;
		return !overflow;
	}

	inline bool ByteStreamWriter::write(const void* data, uintptr size)
	{
		XAssert(currentOffset <= bufferSize);
		const bool overflow = currentOffset + size > bufferSize;
		const uintptr bytesToWrite = overflow ? bufferSize - currentOffset : size;
		memoryCopy(buffer + currentOffset, data, bytesToWrite);
		currentOffset += bytesToWrite;
		overflowed |= overflow;
		return !overflow;
	}

	inline bool ByteStreamWriter::writeAligned(const void* data, uintptr size, uintptr alignment)
	{
		if (!alignUp(alignment))
			return false;
		return write(data, size);
	}

	inline void* ByteStreamWriter::advance(uintptr size)
	{
		const bool overflow = currentOffset + size > bufferSize;
		void* result = overflow ? nullptr : buffer + currentOffset;
		currentOffset = overflow ? bufferSize : currentOffset + size;
		overflowed |= overflow;
		return result;
	}

	inline void* ByteStreamWriter::advanceAligned(uintptr size, uintptr alignment)
	{
		if (!alignUp(alignment))
			return nullptr;
		return advance(size);
	}
}
