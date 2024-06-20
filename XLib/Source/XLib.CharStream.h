#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.System.File.h"

namespace XLib
{
	// CharStreamReader
	//	peek() -> char
	//	get() -> char
	//	read(???)
	//	isEndOfStream() -> bool

	// CharStreamWriter
	//	put(char c)
	//	write(const char* data, uintptr length)

	// Char stream can not contain NUL character
	// For char stream reader NUL character is considered end of stream.
	// `CharStreamWriter::put(NUL)` has no effect.
	// `CharStreamWriter::write(...)` stops at first NUL character.

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

		inline char peek() const { return isEndOfStream() ? *current : 0; }
		inline char get() { return isEndOfStream() ? *current++ : 0; }
		//inline void read(...);
		inline bool isEndOfStream() const { return current != end && *current != 0; }
	};

	class MemoryCharStreamWriter : public NonCopyable
	{
	private:
		char* buffer = nullptr;
		uintptr bufferCapacity = 0;
		uintptr streamLength = 0;

	public:
		MemoryCharStreamWriter() = default;
		~MemoryCharStreamWriter() = default;

		inline MemoryCharStreamWriter(char* buffer, uintptr bufferCapacity) : buffer(buffer), bufferCapacity(bufferCapacity) {}
		inline void open(char* buffer, uintptr bufferCapacity) { this->buffer = buffer; this->bufferCapacity = bufferCapacity; streamLength = 0; }

		inline void nullTerminate();

		inline void put(char c);
		inline void write(const char* data, uintptr length);
	};


	// File streams ////////////////////////////////////////////////////////////////////////////////

	class FileCharStreamReader : public NonCopyable
	{
	private:
		FileHandle fileHandle = FileHandle::Zero;
		char* buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 bufferOffet = 0;
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
		inline char get();
		inline bool isEndOfStream() const;
	};

	class FileCharStreamWriter : public NonCopyable
	{
	private:
		FileHandle fileHandle = FileHandle::Zero;
		char* buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 bufferOffet = 0;
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
		inline void write(const char* data, uintptr length);
	};


	////////////////////////////////////////////////////////////////////////////////////////////////

	class VirtualStringWriter;


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
		LineColumnTrackingCharStreamReaderWrapper(InnerCharStreamReader& innerReader) : innerReader(innerReader) {}

		inline char peek() const { return innerReader.peek(); }
		inline char get() { const char c = innerReader.get(); advanceLocation(c); return c; }
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

inline void XLib::MemoryCharStreamWriter::nullTerminate()
{
	if (streamLength < bufferCapacity)
		buffer[streamLength] = 0;
	else
		buffer[bufferCapacity - 1] = 0;
}

inline void XLib::MemoryCharStreamWriter::put(char c)
{
	if (streamLength < bufferCapacity && c)
	{
		buffer[streamLength] = c;
		streamLength++;
	}
}

inline void XLib::MemoryCharStreamWriter::write(const char* data, uintptr length)
{
	for (uintptr i = 0; i < length; i++)
	{
		if (!data[i])
			break;

		buffer[streamLength] = data[i];
		streamLength++;
	}
}

inline void XLib::FileCharStreamWriter::put(char c)
{
	if (!c)
		return;

	buffer[bufferOffet] = c;
	bufferOffet++;
	if (bufferOffet == bufferSize)
		flush();
}

inline void XLib::FileCharStreamWriter::write(const char* data, uintptr length)
{
	for (uintptr i = 0; i < length; i++)
	{
		if (!data[i])
			break;

		buffer[bufferOffet] = data[i];
		bufferOffet++;
		if (bufferOffet == bufferSize)
			flush();
	}
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
