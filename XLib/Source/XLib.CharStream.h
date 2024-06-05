#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.System.File.h"

// TODO: Do something about file buffers.

namespace XLib
{
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

		inline void open(const char* data, uintptr length) { begin = data; end = data + length; current = data; }

		inline char peek() const { return isEndOfStream() ? *current : 0; }
		inline char get() { return isEndOfStream() ? *current++ : 0; }
		inline bool isEndOfStream() const { return current != end && *current != 0; }

	};

	class MemoryCharStreamWriter : public NonCopyable
	{
	private:

	public:
		MemoryCharStreamWriter() = default;
		~MemoryCharStreamWriter() = default;

		inline void zeroTerminate();

		inline void write(const char* data, uintptr size);
		inline void put(char c);
	};


	// File streams ////////////////////////////////////////////////////////////////////////////////

	class FileCharStreamReader : public NonCopyable
	{
	private:
		static constexpr uint32 BufferSize = 4096;

	private:
		char buffer[BufferSize];
		FileHandle fileHandle = FileHandle::Zero;
		//byte* buffer = nullptr;
		//uint32 bufferSize = 0;
		//uint32 bufferOffet = 0;

	public:
		FileCharStreamReader() = default;
		inline ~FileCharStreamReader() { close(); }

		void open(FileHandle fileHandle, uint32 bufferSize = 4096);
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
		byte* buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 bufferOffet = 0;

	public:
		FileCharStreamWriter() = default;
		inline ~FileCharStreamWriter() { close(); }

		void open(FileHandle fileHandle, uint32 bufferSize = 4096);
		void close();
		void flush();

		inline bool isOpen() const { return fileHandle != FileHandle::Zero; }

		inline void write(const char* data, uintptr size);
		inline void put(char c);
	};


	// Virtual streams /////////////////////////////////////////////////////////////////////////////

	class VirtualCharStreamReader;
	class VirtualCharStreamWriter;


	// Line-column tracking stream wrappers ////////////////////////////////////////////////////////

	template <typename InnerCharStreamReader>
	class LineColumnTrackingCharStreamReaderWrapper : public NonCopyable
	{
	private:
		InnerCharStreamReader& innerReader;

		uint32 lineNumber = 0;
		uint32 columnNumber = 0;

	public:
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
