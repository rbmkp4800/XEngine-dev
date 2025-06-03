#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.String.h"
#include "XLib.System.File.h"

// TODO: Do we need to ignore '\0' in `CharStreamWriter::put(char c)` ?

namespace XLib
{
	// CharStreamReader:
	//	peek() -> char
	//	get() -> char
	//	read(???)
	//	isEndOfStream() -> bool

	// CharStreamWriter:
	//	put(char c)
	//	write(const char* data, uintptr length)
	//	write(const char* cstr)

	// Char stream can not contain NUL characters.
	// For char stream reader NUL character is considered end-of-stream.
	// After end-of-stream is hit char reader pick/get will return NUL.
	// `CharStreamWriter::put(NUL)` has no effect.
	// `CharStreamWriter::write(data, length)` assumes that there is no NUL characters in input data.

#if 0
	template <typename T>
	concept CharStreamReader = requires(T reader)
	{
		//{ reader.read() } -> IsTypeEqual<void>;
		{ reader.peek() } -> IsTypeEqual<char>;
		{ reader.get() } -> IsTypeEqual<char>;
		{ reader.isEndOfStream() } -> IsTypeEqual<bool>;
	};

	template <typename T>
	concept CharStreamWriter = requires(T writer)
	{
		//{ writer.write() } -> IsTypeEqual<void>;
		{ writer.put() } -> IsTypeEqual<void>;
	};
#endif

	// Memory streams //////////////////////////////////////////////////////////////////////////////

	class MemoryCharStreamReader : public NonCopyable
	{
	private:
		const char* begin = nullptr;
		const char* end = nullptr;
		const char* current = nullptr;

	public:
		MemoryCharStreamReader() = default;
		~MemoryCharStreamReader() = default;

		inline MemoryCharStreamReader(const char* data, uintptr length) : begin(data), end(data + length), current(data) {}
		inline void open(const char* data, uintptr length) { begin = data; end = data + length; current = data; }

		inline const char* getBeginPtr() const { return begin; }
		inline const char* getEndPtr() const { return end; }
		inline const char* getCurrentPtr() const { return current; }

		inline const char* getData() const { return begin; }
		inline uintptr getLength() const { return end - begin; }

		inline uintptr getOffset() const { return current - begin; }

		inline char peek() const { return current < end ? *current : 0; }
		inline char peek(uint32 offset) const;
		inline char get() { return (current < end && *current != 0) ? *current++ : 0; }
		//inline void read(...);
		inline bool isEndOfStream() const { return current == end || *current == 0; }
	};

	class MemoryCharStreamWriter : public NonCopyable
	{
	private:
		char* buffer = nullptr;
		uintptr bufferSize = 0;
		uintptr bufferOffset = 0;

	public:
		MemoryCharStreamWriter() = default;
		~MemoryCharStreamWriter() = default;

		inline MemoryCharStreamWriter(char* buffer, uintptr bufferSize) : buffer(buffer), bufferSize(bufferSize) {}
		inline void open(char* buffer, uintptr bufferSize) { this->buffer = buffer; this->bufferSize = bufferSize; bufferOffset = 0; }

		void nullTerminate();

		inline void put(char c);
		void write(const char* data, uintptr length);
		void write(const char* cstr);
	};


	// File streams ////////////////////////////////////////////////////////////////////////////////

	class FileCharStreamReader : public NonCopyable
	{
	private:
		FileHandle fileHandle = FileHandle::Zero;
		char* buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 bufferOffset = 0;
		bool fileHandleIsInternal = false;
		bool bufferIsInternal = false;

	public:
		FileCharStreamReader() = default;
		inline ~FileCharStreamReader() { close(); }

		bool open(const char* name, uint32 bufferSize = 4096, void* externalBuffer = nullptr);
		void open(FileHandle fileHandle, uint32 bufferSize = 4096, void* externalBuffer = nullptr);
		void close();

		inline bool isOpen() const { return fileHandle != FileHandle::Zero; }

		inline char peek() const;
		inline char peek(uint32 offset) const;
		inline char get();
		inline bool isEndOfStream() const;
	};

	class FileCharStreamWriter : public NonCopyable
	{
	private:
		FileHandle fileHandle = FileHandle::Zero;
		char* buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 bufferOffset = 0;
		bool fileHandleIsInternal = false;
		bool bufferIsInternal = false;

	public:
		FileCharStreamWriter() = default;
		inline ~FileCharStreamWriter() { close(); }

		bool open(const char* name, bool overrideFileContent, uint32 bufferSize = 4096, void* externalBuffer = nullptr);
		void open(FileHandle fileHandle, uint32 bufferSize = 4096, void* externalBuffer = nullptr);
		void close();
		void flush();

		inline bool isOpen() const { return fileHandle != FileHandle::Zero; }

		inline void put(char c);
		void write(const char* data, uintptr length);
		void write(const char* cstr);
	};


	////////////////////////////////////////////////////////////////////////////////////////////////

	class VirtualStringWriter : public NonCopyable
	{
	private:
		static constexpr uint32 BufferExponentialGrowthFactor = 2;

	private:
		VirtualStringRefASCII virtualStringRef;
		char* buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 bufferOffset = 0;
		uint32 maxBufferSize = 0;

	private:
		void growBufferExponentially(uint32 minRequiredBufferSize);

	public:
		VirtualStringWriter() = default;
		inline VirtualStringWriter(VirtualStringRefASCII virtualStringRef) { open(virtualStringRef); }
		inline ~VirtualStringWriter() { flush(); }

		void open(VirtualStringRefASCII virtualStringRef);
		void flush();

		inline void put(char c);
		void write(const char* data, uintptr length);
		void write(const char* cstr);
	};


	// Line-column tracking stream wrappers ////////////////////////////////////////////////////////

	template <typename InnerCharStreamReader>
	class LineColumnTrackingCharStreamReaderWrapper : public NonCopyable
	{
	private:
		InnerCharStreamReader& innerReader;
		uint32 lineNumber = 0;
		uint32 columnNumber = 0;

	private:
		void advanceLineColumnNumbers(char c);

	public:
		inline LineColumnTrackingCharStreamReaderWrapper(InnerCharStreamReader& innerReader) : innerReader(innerReader) {}
		~LineColumnTrackingCharStreamReaderWrapper() = default;

		inline char peek() const { return innerReader.peek(); }
		inline char peek(uint32 offset) const { return innerReader.peek(offset); }
		inline char get() { const char c = innerReader.get(); advanceLineColumnNumbers(c); return c; }
		inline bool isEndOfStream() const { return innerReader.isEndOfStream(); }

		inline uint32 getLineNumber() const { return lineNumber + 1; }
		inline uint32 getColumnNumber() const { return columnNumber + 1; }
	};

	template <typename InnerCharStreamWriter>
	class LineColumnTrackingCharStreamWriterWrapper : public NonCopyable
	{
	private:
		InnerCharStreamWriter& innerWriter;

	public:

	};


	// Std streams /////////////////////////////////////////////////////////////////////////////////

	FileCharStreamReader& GetStdInStream();
	FileCharStreamWriter& GetStdOutStream();
	FileCharStreamWriter& GetStdErrStream();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINITION //////////////////////////////////////////////////////////////////////////////////////

inline char XLib::MemoryCharStreamReader::peek(uint32 offset) const
{
	for (uint32 i = 0; i < offset; i++)
	{
		if (!current[i])
			return 0;
		if (current + i == end)
			return 0;
	}
	return current[offset];
}

inline void XLib::MemoryCharStreamWriter::put(char c)
{
	if (bufferOffset < bufferSize && c)
	{
		buffer[bufferOffset] = c;
		bufferOffset++;
	}
}

inline void XLib::FileCharStreamWriter::put(char c)
{
	buffer[bufferOffset] = c;
	bufferOffset++;
	if (bufferOffset == bufferSize)
		flush();
}

inline void XLib::VirtualStringWriter::put(char c)
{
	const uint32 requiredBufferSize = bufferOffset + 2;
	if (bufferSize < requiredBufferSize)
	{
		XAssert(maxBufferSize > 1); // Effectively checks if object is initialized.
		if (bufferSize == maxBufferSize)
			return;
		growBufferExponentially(requiredBufferSize);
	}

	buffer[bufferOffset] = c;
	bufferOffset++;
}

template <typename InnerCharStreamReader>
inline void XLib::LineColumnTrackingCharStreamReaderWrapper<InnerCharStreamReader>::advanceLineColumnNumbers(char c)
{
	// TODO: Handle end of file properly.

	if (c == '\n')
	{
		lineNumber++;
		columnNumber = 0;
	}
	else if (c != '\r')
	{
		columnNumber++;
	}
}
