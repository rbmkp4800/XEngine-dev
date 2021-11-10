#pragma once

#include "XLib.h"

namespace XLib
{
	class ByteStreamWriter
	{
	private:
		byte* buffer;
		uintptr bufferSize;
		uintptr currentBufferOffset = 0;

	public:
		inline ByteStreamWriter(void* buffer, uintptr bufferSize) : buffer((byte*)buffer), bufferSize(bufferSize) {}
		~ByteStreamWriter() = default;

		inline void write(const void* data, uintptr size);
		inline void writeAligned(const void* data, uintptr size, uintptr alignment);
		inline void alignUp(uintptr alignment);

		template <typename T>
		inline void write(const T& value) { write(&value, sizeof(value)); }
		template <typename T>
		inline void writeAligned(const T& value, uintptr alignment) { writeAligned(&value, sizeof(value), alignment); }

		inline uintptr getLength() const { return currentBufferOffset; }
		inline void* getData() const { return buffer; }
	};
}
